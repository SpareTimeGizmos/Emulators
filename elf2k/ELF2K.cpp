//++
// ELF2K.cpp - ELF2K Emulator main program
//
//   COPYRIGHT (C) 2015-2020 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
//
// LICENSE:
//    This file is part of the ELF2K emulator project.  EMULIB is free
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
//   This file is the main program for the ELF2K Emulator task.  
//
// LIMITATIONS (aka the TO DO list)
//
//                                  Bob Armstrong [23-JUL-19]
//
// REVISION HISTORY:
// 23-JUL-19  RLA   New file.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include "EMULIB.hpp"           // emulator library definitions
#include "VirtualConsole.hpp"   // abstract virtual console window
#include "ConsoleWindow.hpp"    // emulator console window methods
#include "SmartConsole.hpp"     // console file transfer functions
#include "LogFile.hpp"          // emulator library message logging facility
#include "ImageFile.hpp"        // emulator library image file methods
#include "CommandParser.hpp"    // emulator library command line parsing methods
#include "CommandLine.hpp"      // emulator library shell (argc/argv) parser methods
#include "StandardUI.hpp"       // emulator library standard UI commands
#include "ELF2K.hpp"            // global declarations for this project
#include "Interrupt.hpp"        // interrupt simulation logic
#include "EventQueue.hpp"       // I/O device event queue simulation
#include "MemoryTypes.h"        // address_t and word_t data types
#include "Memory.hpp"           // main memory emulation
#include "COSMACopcodes.hpp"    // COSMAC opcode definitions
#include "CPU.hpp"              // CCPU base class definitions
#include "COSMAC.hpp"           // COSMAC 1802 CPU emulation
#include "Device.hpp"           // CDevice I/O device emulation objects
#include "TIL311.hpp"           // TIL311 POST display emulation
#include "Switches.hpp"         // toggle switch register emulation
#include "DiskUARTrtc.hpp"      // ELF2K Disk/UART/RTC card emulation
#include "SoftwareSerial.hpp"   // software serial port
#include "UserInterface.hpp"    // ELF2K user interface parse table definitions


// Global objects ....
//   These objects are used (more or less) everywhere within this program, and
// you'll find "extern ..." declarations for them in ELF2K.hpp.  Note that they
// are declared as pointers rather than the actual objects because we want
// to control the exact order in which they're created and destroyed!
CSmartConsole   *g_pConsole     = NULL; // console file transfer functions
CLog            *g_pLog         = NULL; // message logging object (including console!)
CCmdParser      *g_pParser      = NULL; // command line parser
// These globals point to the objects being emulated ...
CCOSMAC         *g_pCPU         = NULL; // 1802 COSMAC CPU
CEventQueue     *g_pEvents      = NULL; // list of things to do
CGenericMemory  *g_pMemory      = NULL; // memory emulation
CTIL311         *g_pTIL311      = NULL; // ELF2K POST display
CSwitches       *g_pSwitches    = NULL; // ELF2K toggle switches
CUART           *g_pUART        = NULL; // generic UART (e.g. CDP1854)
CDiskUARTrtc    *g_pDiskUARTrtc = NULL; // Disk/UART/RTC card
CSoftwareSerial *g_pSerial      = NULL; // software serial port


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
  // Here's the main program for the ELF2K Emulator .
  //--
#ifdef _WIN32
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
  _CrtSetBreakAlloc(184);
  _crtBreakAlloc = 184;
#endif

  //   The very first thing is to create and initialize the console window
  // object, and after that we create and initialize the log object.  We
  // can't issue any error messages until we done these two things!
  g_pEvents  = DBGNEW CEventQueue();
  g_pConsole = DBGNEW CSmartConsole(g_pEvents);
  g_pLog     = DBGNEW CLog(PROGRAM, g_pConsole);
  g_pLog->SetDefaultConsoleLevel(CLog::WARNING);

  //   Parse the command options.  Note that we want to do this BEFORE we
  // setup the console window, since the command line may tell us to detach
  // and create a new window...
  if (!CStandardUI::ParseOptions(PROGRAM, argc, argv)) goto shutdown;

  //   Set the console window defaults - foreground and background color,
  // scrolling buffer size, title, and icon ...
  g_pConsole->SetTitle("ELF2K Emulator v%d", ELFVER);
  //m_pConsole->SetIcon(IDI_ELF);
  g_pConsole->SetBufferSize(132, 2000);
  g_pConsole->SetWindowSize(132, 40);
  g_pConsole->SetColors(CConsoleWindow::YELLOW, CConsoleWindow::BLACK);

  // We're finally ready to say hello ...
  CMDOUTF("ELF2K Emulator v%d emulator Library v%d", ELFVER, EMUVER);
  CMDOUTS("Built on " << __DATE__ << " " << __TIME__);
//CMDOUTF("Word size = %d; Address size = %d; default radix = %d", WORDSIZE, ADDRSIZE, RADIX);

  // Create the emulated CPU, memory and peripheral devices ...
  g_pMemory = DBGNEW CGenericMemory(MEMSIZE);
  g_pMemory->SetNXM(0, MEMSIZE-1);
  g_pMemory->SetRAM(RAMBASE, RAMBASE+RAMSIZE-1);
//g_pMemory->SetRAM(RAMBASE, 8192-1);
  g_pMemory->SetROM(ROMBASE, ROMBASE+ROMSIZE-1);
  g_pCPU = DBGNEW CCOSMAC(g_pMemory, g_pEvents);
//g_pDisplay = DBGNEW CDisplay(PORT_POST);
//g_pSwitches = DBGNEW CSwitches(PORT_SWITCHES);
//g_pCPU->InstallDevice(g_pDisplay);
//g_pCPU->InstallDevice(g_pSwitches);
#define USE_UART
#ifdef USE_UART
//g_pDiskUARTrtc = DBGNEW CDiskUARTrtc(PORT_DISK_UART_RTC);
//g_pCPU->InstallDevice(g_pDiskUARTrtc);
//g_pDiskUARTrtc->InstallUART(*g_pConsoleWindow);
//g_pDiskUARTrtc->InstallNVR();
//g_pDiskUARTrtc->InstallIDE();
//if (!g_pDiskUARTrtc->GetIDE()->Attach("ELF2K.dsk")) goto shutdown;
//g_pCPU->InstallDevice(g_pDiskUARTrtc);
#else
//g_pSerial = DBGNEW CSoftwareSerial(*g_pConsoleWindow);
//g_pCPU->InstallSerial(g_pSerial);
#endif

  //   Lastly, create the command line parser.  If a startup script was
  // specified on the command line, now is the time to execute it...
  g_pParser = DBGNEW CCmdParser(PROGRAM, CUI::g_aVerbs, &ConfirmExit, g_pConsole);
  if (!CStandardUI::g_sStartupScript.empty())
    g_pParser->OpenScript(CStandardUI::g_sStartupScript);

  //   This thread now becomes the background task, which loops forever
  // executing operator commands.  Well, almost forever - when the operator
  // types "EXIT" or "QUIT", the command parser exits and then we shutdown
  // the ELF2K program.
  g_pParser->CommandLoop();
  LOGS(DEBUG, "command parser exited");

shutdown:
  // Delete all our global objects.  Once again, the order here is important!
  delete g_pParser;         // the command line parser can go away first
  delete g_pCPU;            // the COSMAC CPU
  delete g_pMemory;         // the memory object
  delete g_pConsole;        // lastly (always lastly!) close the console window
  delete g_pLog;            // close the log file
  delete g_pEvents;         // the event queue
#ifdef _DEBUG
#ifdef _WIN32
  system("pause");
   _CrtDumpMemoryLeaks();
#endif
#endif
  return 0;
}
