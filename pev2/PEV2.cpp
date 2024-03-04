//++
// PEV2.cpp - PEV2 Emulator main program
//
//   COPYRIGHT (C) 2015-2020 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
//
// LICENSE:
//    This file is part of the PicoElf emulator project.  EMULIB is free
// software; you may redistribute it and/or modify it under the terms of
// the GNU Affero General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any
// later version.
//
//    EMULIB is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License
// for more details.  You should have received a copy of the GNU Affero General
// Public License along with EMULIB.  If not, see http://www.gnu.org/licenses/.
//
// DESCRIPTION:
//   This file is the main program for the PEV2 Emulator task.  
//
//                                  Bob Armstrong [28-JUL-22]
//
// REVISION HISTORY:
// 28-JUL-22  RLA   Adapted from ELF2K.
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
#include "PEV2.hpp"             // global declarations for this project
#include "Interrupt.hpp"        // interrupt simulation logic
#include "EventQueue.hpp"       // I/O device event queue simulation
#include "MemoryTypes.h"        // address_t and word_t data types
#include "Memory.hpp"           // main memory emulation
#include "COSMACopcodes.hpp"    // COSMAC opcode definitions
#include "CPU.hpp"              // CCPU base class definitions
#include "COSMAC.hpp"           // COSMAC 1802 CPU emulation
#include "Device.hpp"           // CDevice I/O device emulation objects
#include "TIL311.hpp"           // standard hexadecimal POST display
#include "IDE.hpp"              // standard IDE disk interface
#include "ElfDisk.hpp"          // "standard" Elf to IDE disk interface
#include "SoftwareSerial.hpp"   // software serial port
#include "UART.hpp"             // generic UART emulation
#include "INS8250.hpp"          // INS8250 specific emulation
#include "RTC.hpp"              // generic RTC emulation
#include "DS12887.hpp"          // DS12887 NVR/RTC emulation
#include "UARTrtc.hpp"          // UART/RTC expansion card
#include "UserInterface.hpp"    // PEV2 user interface parse table definitions


// Global objects ....
//   These objects are used (more or less) everywhere within this program, and
// you'll find "extern ..." declarations for them in PEV2.hpp.  Note that they
// are declared as pointers rather than the actual objects because we want
// to control the exact order in which they're created and destroyed!
CConsoleWindow  *g_pConsole     = NULL; // console window object
CLog            *g_pLog         = NULL; // message logging object (including console!)
CCmdParser      *g_pParser      = NULL; // command line parser
// These globals point to the objects being emulated ...
CEventQueue     *g_pEvents      = NULL; // list of upcoming virtual events
CCOSMAC         *g_pCPU         = NULL; // 1802 COSMAC CPU
CGenericMemory  *g_pMemory      = NULL; // 64K memory (RAM and EPROM)
CTIL311         *g_pTIL311      = NULL; // POST display
CElfDisk        *g_pIDE         = NULL; // IDE disk emulation
#ifdef EF_SERIAL
CSoftwareSerial *g_pSerial      = NULL; // software serial port
#else
CINS8250        *g_pUART        = NULL; // INS8250 console UART
C12887          *g_pRTC         = NULL; // DS12887 RTC/NVR emulation
CUARTrtc        *g_pCombo       = NULL; // UART/RTC expansion card
#endif


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
  // Here's the main program for the PEV2 Emulator .
  //--

  //   The very first thing is to create and initialize the console window
  // object, and after that we create and initialize the log object.  We
  // can't issue any error messages until we done these two things!
  g_pConsole = DBGNEW CConsoleWindow();
  g_pLog = DBGNEW CLog(PROGRAM, g_pConsole);

  //   Parse the command options.  Note that we want to do this BEFORE we
  // setup the console window, since the command line may tell us to detach
  // and create a new window...
  if (!CStandardUI::ParseOptions(PROGRAM, argc, argv)) goto shutdown;

  //   Set the console window defaults - foreground and background color,
  // scrolling buffer size, title, and icon ...
  g_pConsole->SetTitle("PEV2 Emulator v%d", PEV2VER);
  //m_pConsole->SetIcon(IDI_ELF);
  g_pConsole->SetBufferSize(132, 2000);
  g_pConsole->SetWindowSize(132, 40);
  g_pConsole->SetColors(CConsoleWindow::GREEN, CConsoleWindow::BLACK);
  g_pLog->SetDefaultConsoleLevel(CLog::WARNING);

  // We're finally ready to say hello ...
  CMDOUTF("PEV2 Emulator v%d emulator Library v%d", PEV2VER, EMUVER);
  CMDOUTS("Built on " << __DATE__ << " " << __TIME__);

  // Create the emulated CPU, memory and peripheral devices ...
  g_pEvents = DBGNEW CEventQueue();
  g_pMemory = DBGNEW CGenericMemory(MEMSIZE);
  g_pMemory->SetNXM(0, MEMSIZE-1);
  g_pMemory->SetRAM(RAMBASE, RAMBASE+RAMSIZE-1);
  g_pMemory->SetROM(ROMBASE, ROMBASE+ROMSIZE-1);
  g_pCPU = DBGNEW CCOSMAC(g_pMemory, g_pEvents);
  g_pTIL311 = DBGNEW CTIL311(PORT_POST);
  g_pCPU->InstallDevice(g_pTIL311);
  g_pIDE = DBGNEW CElfDisk(PORT_IDE, g_pEvents);
  g_pCPU->InstallDevice(g_pIDE);
#ifdef EF_SERIAL
  g_pSerial = DBGNEW CSoftwareSerial(g_pEvents, g_pConsole, g_pCPU);
  g_pCPU->InstallSense(g_pSerial, EF_SERIAL);
  g_pCPU->InstallFlag(g_pSerial, CCOSMAC::Q);
#else
  g_pUART = DBGNEW CINS8250("SLU", 0, g_pEvents, g_pConsole, g_pCPU);
  g_pRTC = DBGNEW C12887("RTC", 0, g_pEvents, true);
  g_pCombo = DBGNEW CUARTrtc(PORT_COMBO, g_pUART, g_pRTC);
  g_pCPU->InstallDevice(g_pCombo);
#endif

  //   Lastly, create the command line parser.  If a startup script was
  // specified on the command line, now is the time to execute it...
  g_pParser = DBGNEW CCmdParser(PROGRAM, CUI::g_aVerbs, &ConfirmExit, g_pConsole);
  if (!CStandardUI::g_sStartupScript.empty())
    g_pParser->OpenScript(CStandardUI::g_sStartupScript);

  //   This thread now becomes the background task, which loops forever
  // executing operator commands.  Well, almost forever - when the operator
  // types "EXIT" or "QUIT", the command parser exits and then we shutdown
  // the PEV2 program.
  g_pParser->CommandLoop();
  LOGS(DEBUG, "command parser exited");

  // Delete all our global objects.  Once again, the order here is important!
  delete g_pParser;   // the command line parser can go away first
#ifdef EF_SERIAL
  delete g_pSerial;   // software serial port
#else
  delete g_pCombo;    // UART/RTC expansion card
  delete g_pRTC;      // DS12887 real time clock
  delete g_pUART;     // INS8250 UART for console
#endif
  delete g_pTIL311;   // POST display
  delete g_pIDE;      // IDE disk
  delete g_pCPU;      // the COSMAC CPU
  delete g_pMemory;   // the memory
  delete g_pEvents;   // the event queue
shutdown:
  delete g_pLog;      // close the log file
  delete g_pConsole;  // lastly (always lastly!) close the console window
#ifdef _DEBUG
#ifdef _WIN32
  system("pause");
  _CrtDumpMemoryLeaks();
#endif
#endif
  return 0;
}
