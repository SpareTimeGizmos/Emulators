//++
// MS2000.cpp - RCA MS2000 emulator main program
//
//   COPYRIGHT (C) 2024 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
//
// LICENSE:
//    This file is part of the MS2000 emulator project.  MS2000 is free
// software; you may redistribute it and/or modify it under the terms of
// the GNU Affero General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any
// later version.
//
//    MS2000 is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License
// for more details.  You should have received a copy of the GNU Affero General
// Public License along with MS2000.  If not, see http://www.gnu.org/licenses/.
//
// DESCRIPTION:
//   This program is a simple emulator for the RCA MicroDisk Development System
// MS2000.  It's sophisticated enough to run RCA MicroDOS and that's about it.
// The basic MS2000 system requires
// 
//      * the RCA CDP1802 COSMAC CPU
//      * 2K of ROM from $8000 to $87FF for UT71
//      * 62K of RAM everywhere else
//      * RCA standard two level I/O for the COSMAC
//      * a CDP1854 console serial port
//      * the RCA 18S651 disk controller with the uPD765 FDC chip
// 
// and that's all!
//
//                                  Bob Armstrong [5-MAR-24]
//
//
// REVISION HISTORY:
//  5-MAR-24  RLA   Adapted from SBC1802.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include "EMULIB.hpp"           // emulator library definitions
#include "ConsoleWindow.hpp"    // emulator console window methods
#include "LogFile.hpp"          // emulator library message logging facility
#include "ImageFile.hpp"        // emulator library image file methods
#include "CommandParser.hpp"    // emulator library command line parsing methods
#include "CommandLine.hpp"      // emulator library shell (argc/argv) parser methods
#include "StandardUI.hpp"       // emulator library standard UI commands
#include "MS2000.hpp"           // global declarations for this project
#include "Interrupt.hpp"        // interrupt simulation logic
#include "EventQueue.hpp"       // I/O device event queue simulation
#include "MemoryTypes.h"        // address_t and word_t data types
#include "Memory.hpp"           // main memory emulation
#include "COSMACopcodes.hpp"    // COSMAC opcode definitions
#include "CPU.hpp"              // CCPU base class definitions
#include "COSMAC.hpp"           // COSMAC 1802 CPU emulation
#include "Device.hpp"           // CDevice I/O device emulation objects
#include "TLIO.hpp"             // RCA style two level I/O mapping
#include "CDP1854.hpp"          // CDP1854 UARTs 
#include "uPD765.hpp"           // NEC uPD765 FDC definitions
#include "CDP18S651.hpp"        // RCA CDP18S651 floppy disk interface
#include "UserInterface.hpp"    // user interface parse table definitions


// Global objects ....
//   These objects are used (more or less) everywhere within this program, and
// you'll find "extern ..." declarations for them in MS2000.hpp.  Note that they
// are declared as pointers rather than the actual objects because we want
// to control the exact order in which they're created and destroyed!
CConsoleWindow   *g_pConsole    = NULL; // console window object
CLog             *g_pLog        = NULL; // message logging object (including console!)
CCmdParser       *g_pParser     = NULL; // command line parser
// These globals point to the objects being emulated ...
CEventQueue      *g_pEvents     = NULL; // list of upcoming virtual events
CCOSMAC          *g_pCPU        = NULL; // 1802 COSMAC CPU
CSimpleInterrupt *p_pInterrupt  = NULL; // simple wire-OR interrupt system
CGenericMemory   *g_pMemory     = NULL; // generic memory (RAM and ROM) emulation
CTLIO            *g_pTLIO       = NULL; // two level I/O controller
CCDP1854         *g_pSLU        = NULL; // console UART
C18S651          *g_pFDC        = NULL; // CDP18S651 floppy disk controller


static bool ConfirmExit (CCmdParser &cmd)
{
  //++
  //   This routine is called whenever this application has been requested
  // to exit.  It returns true if we really should exit and false if we
  // shouldn't right now.
  //--
  return true;
}

int main (int argc, char *argv[])
{
  //++
  // Here's the main program for the MS2000 Emulator .
  //--
#ifdef _WIN32
  _CrtMemState memstate;
  _CrtMemCheckpoint(&memstate);
  //_CrtSetBreakAlloc(249);
#endif
  
  //   The very first thing is to create and initialize the console window
  // object, and after that we create and initialize the log object.  We
  // can't issue any error messages until we done these two things!
  g_pEvents = DBGNEW CEventQueue();
  g_pConsole = DBGNEW CConsoleWindow();
  g_pLog = DBGNEW CLog(PROGRAM, g_pConsole);
  g_pLog->SetDefaultConsoleLevel(CLog::WARNING);

  //   Parse the command options.  Note that we want to do this BEFORE we
  // setup the console window, since the command line may tell us to detach
  // and create a new window...
  if (!CStandardUI::ParseOptions(PROGRAM, argc, argv)) goto shutdown;

  //   Set the console window defaults - foreground and background color,
  // scrolling buffer size, title, and icon ...
  g_pConsole->SetTitle("MS2000 Emulator v%d", MSVER);
  //m_pConsole->SetIcon(IDI_ELF);
  g_pConsole->SetBufferSize(132, 2000);
  g_pConsole->SetWindowSize(80, 40);
  g_pConsole->SetColors(CConsoleWindow::YELLOW, CConsoleWindow::BLACK);

  // We're finally ready to say hello ...
  CMDOUTF("MS2000 Emulator v%d emulator Library v%d", MSVER, EMUVER);
  CMDOUTS("Built on " << __DATE__ << " " << __TIME__);
 //CMDOUTF("Word size = %d; Address size = %d; default radix = %d", WORDSIZE, ADDRSIZE, RADIX);

  // Create the memory and the CPU, and set the UT71 ROM part to read only.
  g_pMemory = DBGNEW CGenericMemory(MEMSIZE, 0, CMemory::MEM_RAM);
  g_pMemory->SetROM(ROMBASE, ROMBASE+ROMSIZE-1);
  p_pInterrupt = DBGNEW CSimpleInterrupt();
  g_pCPU = DBGNEW CCOSMAC(g_pMemory, g_pEvents, p_pInterrupt);

  //   Create the two level I/O controller and attach it to ALL seven CPU I/O
  // instructions plus all four EF inputs.  The Q output, which isn't really
  // used in this design anyway, isn't affected by the two level I/O.
  g_pTLIO = DBGNEW CTLIO(TLIO_PORT, 1, CCOSMAC::MAXDEVICE);
  g_pCPU->InstallDevice(g_pTLIO);
  g_pCPU->InstallSense(g_pTLIO, CCOSMAC::EF1);
  g_pCPU->InstallSense(g_pTLIO, CCOSMAC::EF2);
  g_pCPU->InstallSense(g_pTLIO, CCOSMAC::EF3);
  g_pCPU->InstallSense(g_pTLIO, CCOSMAC::EF4);

  //   Attach the console UART.  Note that the port and EF assigments used
  // here are the same as the CDP18S605 CPU board ...
  g_pSLU = DBGNEW CCDP1854("SLU", SLU_PORT, g_pEvents, g_pConsole, g_pCPU, SLU_IRQ_EF, SLU_BREAK_EF);
  g_pTLIO->InstallDevice(CDP18S605_GROUP, g_pSLU);
  g_pTLIO->InstallSense(CDP18S605_GROUP, g_pSLU, SLU_IRQ_EF);
  g_pTLIO->InstallSense(CDP18S605_GROUP, g_pSLU, SLU_BREAK_EF);
  g_pSLU->AttachInterrupt(p_pInterrupt);

  //   And lastly the floppy disk controller.  Note that the CDP18S651 board 
  // takes over all I/Os in its group (excepting the TLIO port, of course) so
  // there's no base port or port range to be specified!
  g_pFDC = DBGNEW C18S651(g_pEvents, g_pCPU, FDC_IRQ_EF, FDC_MOTOR_EF);
  g_pTLIO->InstallDevice(CDP18S651_GROUP, g_pFDC);
  g_pTLIO->InstallSense(CDP18S651_GROUP, g_pFDC, FDC_IRQ_EF);
  g_pTLIO->InstallSense(CDP18S651_GROUP, g_pFDC, FDC_MOTOR_EF);
  g_pFDC->AttachInterrupt(p_pInterrupt);

  //   Lastly, create the command line parser.  If a startup script was
  // specified on the command line, now is the time to execute it...
  g_pParser = DBGNEW CCmdParser(PROGRAM, CUI::g_aVerbs, &ConfirmExit, g_pConsole);
  if (!CStandardUI::g_sStartupScript.empty())
    g_pParser->OpenScript(CStandardUI::g_sStartupScript);

  //   This thread now becomes the background task, which loops forever
  // executing operator commands.  Well, almost forever - when the operator
  // types "EXIT" or "QUIT", the command parser exits and then we shutdown
  // the MS2000 program.
  g_pParser->CommandLoop();
  LOGS(DEBUG, "command parser exited");

  // Delete all our global objects.  Once again, the order here is important!
  delete g_pParser;   // the command line parser can go away first


  // Delete everything in the reverse order of their creation.
  delete g_pFDC;      // floppy disk emulator
  delete g_pSLU;      // console serial line unit
  delete g_pTLIO;     // two level I/O controller
  delete g_pCPU;      // the COSMAC CPU
  delete g_pMemory;   // main memory

  // Lastly we can get rid of the log file, console window and event queue.
shutdown:
  delete g_pLog;      // close the log file
  delete g_pConsole;  // lastly (always lastly!) close the console window
  delete g_pEvents;   // the event queue
#ifdef _DEBUG
#ifdef _WIN32
  _CrtMemDumpAllObjectsSince(&memstate);
#endif
#endif
  return 0;
}
