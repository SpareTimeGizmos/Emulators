//++
// SBC6120.cpp - SBC6120 Emulator main program
//
//   Copyright (C) 1999-2022 by Spare Time Gizmos.  All rights reserved.
//
// LICENSE:
//    This file is part of the SBC6120 emulator project. SBC6120 is free
// software; you may redistribute it and/or modify it under the terms of
// the GNU Affero General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any
// later version.
//
//    SBC6120 is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License
// for more details.  You should have received a copy of the GNU Affero General
// Public License along with SBC6120.  If not, see http://www.gnu.org/licenses/.
//
// DESCRIPTION:
//   This file is the main program for the SBC6120 Emulator task.  
//
// LIMITATIONS (aka the TO DO list)
//
//                                  Bob Armstrong [30-JUL-22]
//
// REVISION HISTORY:
// 30-JUL-22  RLA   Adapted from ELF2K.
// 30-AUG-22  RLA   Be sure objects are delete in reverse order of creation!
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
#include "SBC6120.hpp"          // global declarations for this project
#include "Interrupt.hpp"        // interrupt simulation logic
#include "EventQueue.hpp"       // I/O device event queue simulation
#include "MemoryTypes.h"        // address_t and word_t data types
#include "Memory.hpp"           // main memory emulation
#include "HD6120opcodes.hpp"    // PDP-8 opcode definitions
#include "CPU.hpp"              // CCPU base class definitions
#include "HD6120.hpp"           // HD6120 CPU specific emulation
#include "SLU.hpp"              // KL8E console serial line
#include "RAMdisk.hpp"          // SBC6120 RAM disk emulation
#include "MemoryMap.hpp"        // SBC6120 memory mapping hardware
#include "POST.hpp"             // SBC6120 three LED POST code display
#include "MiscellaneousIOTs.hpp"// miscellaneous SBC6120 641x and 643x IOTs 
#include "IDE.hpp"              // generic IDE disk definitions
#include "IDEdisk.hpp"          // SBC6120 specific IDE disk interface
#include "UserInterface.hpp"    // SBC6120 user interface parse table definitions


// Global objects ....
//   These objects are used (more or less) everywhere within this program, and
// you'll find "extern ..." declarations for them in SBC6120.hpp.  Note that they
// are declared as pointers rather than the actual objects because we want
// to control the exact order in which they're created and destroyed!
CConsoleWindow   *g_pConsole        = NULL; // console window object
CLog             *g_pLog            = NULL; // message logging object (including console!)
CCmdParser       *g_pParser         = NULL; // command line parser
// These globals point to the objects being emulated ...
CEventQueue      *g_pEvents         = NULL; // list of upcoming virtual events
CSimpleInterrupt *g_pPanelInterrupt = NULL; // control panel interrupt sources
CSimpleInterrupt *g_pMainInterrupt  = NULL; // "wire-or" interrupt system
C6120            *g_pCPU            = NULL; // 6120 PDP-8 CPU
CGenericMemory   *g_pMainMemory     = NULL; // main PDP-8 memory
CGenericMemory   *g_pPanelMemory    = NULL; // HD6120 control panel memory
CGenericMemory   *g_pEPROM          = NULL; // SBC6120 EPROM space
CRAMdisk         *g_pRAMdisk        = NULL; // SBC6120 RAM disk memory
CMemoryMap       *g_pMemoryMap      = NULL; // SBC6120 memory map hardware
CPOST            *g_pPOST           = NULL; // three LED POST code display
CSLU             *g_pSLU            = NULL; // console serial line
CIOT641x         *g_pIOT641x        = NULL; // miscellaneous 641x IOTs
CIOT643x         *g_pIOT643x        = NULL; // FP6120 front panel IOTs
CIDEdisk         *g_pIDEdisk        = NULL; // IDE disk interface


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
  // Here's the main program for the SBC6120 Emulator .
  //--
  //_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
//_crtBreakAlloc = 152;
//_CrtSetBreakAlloc(152);

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
  g_pConsole->SetTitle("SBC6120 Emulator v%d", SBCVER);
  //m_pConsole->SetIcon(IDI_ELF);
  g_pConsole->SetBufferSize(132, 2000);
  g_pConsole->SetWindowSize(132, 40);
  g_pConsole->SetColors(CConsoleWindow::YELLOW, CConsoleWindow::BLACK);
  g_pLog->SetDefaultConsoleLevel(CLog::WARNING);

  // We're finally ready to say hello ...
  CMDOUTF("SBC6120 Emulator v%d emulator Library v%d", SBCVER, EMUVER);
  CMDOUTS("Built on " << __DATE__ << " " << __TIME__);
 //CMDOUTF("Word size = %d; Address size = %d; default radix = %d", WORDSIZE, ADDRSIZE, RADIX);

  // Create the emulated CPU, memory and peripheral devices ...
  g_pEvents         = DBGNEW CEventQueue();
  g_pPanelInterrupt = DBGNEW CSimpleInterrupt(CSimpleInterrupt::EDGE_TRIGGERED);
  g_pMainInterrupt  = DBGNEW CSimpleInterrupt(CSimpleInterrupt::LEVEL_TRIGGERED);;
  g_pMainMemory     = DBGNEW CGenericMemory(MAIN_MEMORY_SIZE,  0, CMemory::MEM_RAM);
  g_pPanelMemory    = DBGNEW CGenericMemory(PANEL_MEMORY_SIZE, 0, CMemory::MEM_RAM);
  g_pEPROM          = DBGNEW CGenericMemory(EPROM_MEMORY_SIZE, 0, CMemory::MEM_ROM);
  g_pRAMdisk        = DBGNEW CRAMdisk();
  g_pCPU            = DBGNEW C6120(g_pMainMemory, g_pPanelMemory, g_pEvents, g_pMainInterrupt, g_pPanelInterrupt);
  g_pMemoryMap      = DBGNEW CMemoryMap(MMAP_DEVICE_CODE, g_pCPU, g_pMainMemory, g_pPanelMemory, g_pEPROM, g_pRAMdisk);
  g_pCPU->InstallDevice(g_pMemoryMap);
//g_pMemoryMap->MasterClear();
  g_pPOST = DBGNEW CPOST(POST_DEVICE_CODE);
  g_pCPU->InstallDevice(g_pPOST);
  g_pSLU = DBGNEW CSLU("SLU", SLU_DEVICE_CODE, g_pEvents, g_pConsole, g_pCPU);
  g_pSLU->AttachInterrupt(g_pMainInterrupt);
  g_pCPU->InstallDevice(g_pSLU);
  g_pIDEdisk = DBGNEW CIDEdisk(IDE_DEVICE_CODE, g_pEvents);
  g_pCPU->InstallDevice(g_pIDEdisk);
  g_pIDEdisk->AttachInterrupt(g_pMainInterrupt);
  g_pIOT641x = DBGNEW CIOT641x(MISC_DEVICE_CODE, g_pSLU, g_pRAMdisk, g_pIDEdisk);
  g_pCPU->InstallDevice(g_pIOT641x);
  g_pIOT643x = DBGNEW CIOT643x(PANEL_DEVICE_CODE);
  g_pCPU->InstallDevice(g_pIOT643x);

  //   Lastly, create the command line parser.  If a startup script was
  // specified on the command line, now is the time to execute it...
  g_pParser = DBGNEW CCmdParser(PROGRAM, CUI::g_aVerbs, &ConfirmExit, g_pConsole);
  if (!CStandardUI::g_sStartupScript.empty())
    g_pParser->OpenScript(CStandardUI::g_sStartupScript);

  //   This thread now becomes the background task, which loops forever
  // executing operator commands.  Well, almost forever - when the operator
  // types "EXIT" or "QUIT", the command parser exits and then we shutdown
  // the SBC6120 program.
  g_pParser->CommandLoop();
  LOGS(DEBUG, "command parser exited");

  // Delete all our global objects.  Once again, the order here is important!
  delete g_pParser;           // the command line parser can go away first
  delete g_pIOT643x;          // FP6120 front panel IOTs
  delete g_pIOT641x;          // miscellaneous SBC6120 IOTs
  delete g_pIDEdisk;          // IDE disk interface
  delete g_pSLU;             // console serial line
  delete g_pPOST;             // LED POST code display
  delete g_pMemoryMap;        // memory mapping hardware
  delete g_pCPU;              // the PDP-8 CPU
  delete g_pRAMdisk;          // RAM disk memory
  delete g_pEPROM;            // EPROM memory space
  delete g_pPanelMemory;      // control panel memory
  delete g_pMainMemory;       // main PDP-8 memory
  delete g_pMainInterrupt;    // conventional interrupt sources
  delete g_pPanelInterrupt;   // control panel interrupt sources
  delete g_pEvents;           // the event queue
shutdown:
  delete g_pLog;              // close the log file
  delete g_pConsole;          // lastly (always lastly!) close the console window
#ifdef _DEBUG
#ifdef _WIN32
  system("pause");
  _CrtDumpMemoryLeaks();
#endif
#endif
  return 0;
}
