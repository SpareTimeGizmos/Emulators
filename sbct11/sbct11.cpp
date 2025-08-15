//++
// SBCT11.cpp - Spare Time Gizmos SBCT11 Emulator main program
//
//   COPYRIGHT (C) 2015-2024 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
// 
// LICENSE:
//    This file is part of the SBCT11 emulator project.  SBCT11 is free
// software; you may redistribute it and/or modify it under the terms of
// the GNU Affero General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any
// later version.
//
//    SBCT11 is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License
// for more details.  You should have received a copy of the GNU Affero General
// Public License along with SBCT11.  If not, see http://www.gnu.org/licenses/.
//
// DESCRIPTION:
//
// LIMITATIONS (aka the TO DO list)
//
//                                  Bob Armstrong [4-MAR-20]
//
// REVISION HISTORY:
//  4-MAR-20  RLA   New file.
// 30-AUG-22  RLA   Delete objects in the reverse order of creation!
// 15-AUG-25  RLA   Change RTC to the "new PCB" version.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include "EMULIB.hpp"           // emulator library definitions
#include "ConsoleWindow.hpp"    // emulator console window methods
#include "SmartConsole.hpp"     // console file transfer functions
#include "LogFile.hpp"          // emulator library message logging facility
#include "ImageFile.hpp"        // emulator library image file methods
#include "CommandParser.hpp"    // emulator library command line parsing methods
#include "CommandLine.hpp"      // emulator library shell (argc/argv) parser methods
#include "StandardUI.hpp"       // emulator library standard UI commands
#include "sbct11.hpp"           // global declarations for this project
#include "EventQueue.hpp"       // I/O device event queue simulation
#include "MemoryTypes.h"        // address_t, word_t, etc ...
#include "Memory.hpp"           // main memory emulation
#include "Device.hpp"           // CDevice I/O device emulation objects
#include "DeviceMap.hpp"        // CDeviceMap memory mapped I/O emulation
#include "MemoryMap.hpp"        // memory mapping hardware
#include "Interrupt.hpp"        // interrupt simulation logic
#include "CPU.hpp"              // CCPU base class definitions
#include "PIC11.hpp"            // DCT11 interrupt emulation
#include "DCT11.hpp"            // DCT11 CPU emulation
#include "LTC11.hpp"            // line time clock emulation
#include "DC319.hpp"            // DC319 DLART serial line unit emulation
#include "RTC.hpp"              // generic NVR declarations
#include "DS12887.hpp"          // Dallas DS12887A NVR/RTC declarations
#include "RTC11.hpp"            // real time clock
#include "i8255.hpp"            // Intel 8255 programmable peripheral interface
#include "PPI11.hpp"            // Centronics parallel port and POST display
#include "IDE.hpp"              // generic IDE disk drive emulation
#include "IDE11.hpp"            // IDE disk attachment
#include "TU58.hpp"             // TU58 drive emulator
#include "UserInterface.hpp"    // User interface parse table definitions


// Global objects ....
//   These objects are used (more or less) everywhere within this program, and
// you'll find "extern ..." declarations for them in SBCT11.hpp.  Note that they
// are declared as pointers rather than the actual objects because we want
// to control the exact order in which they're created and destroyed!
CSmartConsole   *g_pConsole     = NULL; // console window object
CLog            *g_pLog         = NULL; // message logging object (including console!)
CCmdParser      *g_pParser      = NULL; // command line parser
// These globals point to the objects being emulated ...
CEventQueue     *g_pEvents      = NULL; // list of events to be done
CPIC11          *g_pPIC         = NULL; // DCT11 interrupt controller
CDCT11          *g_pCPU         = NULL; // DCT11 CPU
CMemoryMap      *g_pMMap        = NULL; // SBCT11 memory mapping hardware
CGenericMemory  *g_pRAM         = NULL; // 64K (bytes) of SRAM
CGenericMemory  *g_pROM         = NULL; // 64K (bytes) of EPROM
CDeviceMap      *g_pIOpage      = NULL; // PDP11 memory mapped I/O page
CMemoryControl  *g_pMCR         = NULL; // MEMC/NXMCS registers
CLTC11          *g_pLTC         = NULL; // line time clock
CDC319          *g_pSLU0        = NULL; // console serial line
CRTC11          *g_pRTC         = NULL; // real time clock
CPPI11          *g_pPPI         = NULL; // Centronics printer and POST display
CIDE11          *g_pIDE         = NULL; // IDE disk attachment
CDC319          *g_pSLU1        = NULL; // TU58 serial port
CTU58           *g_pTU58        = NULL; // TU58 drive emulator

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
  // Here's the main program for the SBCT11 Emulator .
  //--
#ifdef _WIN32
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
  _CrtSetBreakAlloc(184);
  _crtBreakAlloc = 184;
#endif

  //   The very first thing is to create and initialize the console window
  // object, and after that we create and initialize the log object.  We
  // can't issue any error messages until we done these two things!
  g_pEvents = DBGNEW CEventQueue();
  g_pConsole = DBGNEW CSmartConsole(g_pEvents);
  g_pLog = DBGNEW CLog(PROGRAM, g_pConsole);
  g_pLog->SetDefaultConsoleLevel(CLog::WARNING);

  //   Parse the command options.  Note that we want to do this BEFORE we
  // setup the console window, since the command line may tell us to detach
  // and create a new window...
  if (!CStandardUI::ParseOptions(PROGRAM, argc, argv)) goto shutdown;

  //   Set the console window defaults - foreground and background color,
  // scrolling buffer size, title, and icon ...
  g_pConsole->SetTitle("SBCT11 Emulator v%d", T11VER);
  g_pConsole->SetBufferSize(132, 2000);
  g_pConsole->SetWindowSize(132, 40);
  g_pConsole->SetColors(CConsoleWindow::GREEN, CConsoleWindow::BLACK);
  g_pLog->SetDefaultConsoleLevel(CLog::WARNING);

  // We're finally ready to say hello ...
  CMDOUTF("SBCT11 Emulator v%d emulator Library v%d", T11VER, EMUVER);
  CMDOUTS("Built on " << __DATE__ << " " << __TIME__);

  // Create the emulated CPU, memory and peripheral devices ...
  g_pPIC = DBGNEW CPIC11();
  g_pRAM = DBGNEW CGenericMemory(RAMSIZE);
  g_pROM = DBGNEW CGenericMemory(ROMSIZE);
  g_pRAM->SetRAM();  g_pROM->SetROM();
  g_pIOpage = DBGNEW CDeviceMap();
  g_pMCR = DBGNEW CMemoryControl(MEMCSR);
  g_pIOpage->Install(g_pMCR);
  g_pMMap = DBGNEW CMemoryMap(g_pRAM, g_pROM, g_pIOpage, g_pMCR);
  g_pCPU = DBGNEW CDCT11(CDCT11::MODE_172000, g_pMMap, g_pEvents, g_pPIC);
  g_pMMap->SetCPU(g_pCPU);
  g_pLTC = DBGNEW CLTC11(LTCCSR, g_pEvents);
  g_pIOpage->Install(g_pLTC);
  g_pLTC->AttachInterrupt((*g_pPIC)[LTC_IRQ]);
  g_pSLU0 = DBGNEW CDC319("SLU0", SLU0_BASE, g_pEvents, g_pConsole, g_pCPU);
  g_pIOpage->Install(g_pSLU0);
  g_pSLU0->AttachInterrupt((*g_pPIC)[SLU0_XMT_IRQ], (*g_pPIC)[SLU0_RCV_IRQ]);
  g_pRTC = DBGNEW CRTC11(RTC_BASE, g_pEvents);
  g_pIOpage->Install(g_pRTC);
  g_pPPI = DBGNEW CPPI11("PPI", PPI_BASE, g_pEvents);
  g_pIOpage->Install(g_pPPI);
  (*g_pPIC)[PPI_IRQ]->SetMode(CSimpleInterrupt::LEVEL_TRIGGERED);
  g_pPPI->AttachInterrupt((*g_pPIC)[PPI_IRQ], NULL);
  g_pIDE = DBGNEW CIDE11(IDE_BASE, g_pEvents);
  g_pIOpage->Install(g_pIDE);
  (*g_pPIC)[IDE_IRQ]->SetMode(CSimpleInterrupt::LEVEL_TRIGGERED);
  g_pIDE->AttachInterrupt((*g_pPIC)[IDE_IRQ]);
  // TBA TU58!!
  g_pTU58 = DBGNEW CTU58();
  g_pSLU1 = DBGNEW CDC319("SLU1", SLU1_BASE, g_pEvents, g_pTU58);
  g_pIOpage->Install(g_pSLU1);
  g_pSLU1->AttachInterrupt((*g_pPIC)[SLU1_XMT_IRQ], (*g_pPIC)[SLU1_RCV_IRQ]);
  //   RT11 seems a bit sensitive to the speed of the TU58 serial port.  If
  // too fast then it will hang during the boot process, but interestingly
  // it will also hang if it's too slow.  Empirically these numbers work! 
  g_pSLU1->SetCharacterDelay(HZTONS(20000));
  g_pSLU1->SetPollDelay     (HZTONS( 2000));

  //   Lastly, create the command line parser.  If a startup script was
  // specified on the command line, now is the time to execute it...
  g_pParser = DBGNEW CCmdParser(PROGRAM, CUI::g_aVerbs, &ConfirmExit, g_pConsole);
  if (!CStandardUI::g_sStartupScript.empty())
    g_pParser->OpenScript(CStandardUI::g_sStartupScript);

  //   This thread now becomes the background task, which loops forever
  // executing operator commands.  Well, almost forever - when the operator
  // types "EXIT" or "QUIT", the command parser exits and then we shutdown
  // the SBCT11 program.
  g_pParser->CommandLoop();
  LOGS(DEBUG, "command parser exited");

shutdown:
  // Delete all our global objects.  Once again, the order here is important!
  delete g_pSLU1;         // TU58 serial port
  delete g_pTU58;         // TU58 emulator
  delete g_pIDE;          // IDE disk attachment
  delete g_pPPI;          // Centronics printer and POST
  delete g_pRTC;          // real time clock
  delete g_pSLU0;         // console serial line
  delete g_pLTC;          // line time clock
  delete g_pCPU;          // the CPU
  delete g_pMMap;         // memory mapping hardware
  delete g_pMCR;          // MEMC/NXMCS registers
  delete g_pIOpage;       // PDP11 I/O page
  delete g_pROM;          // (EP)ROM
  delete g_pRAM;          // RAM
  delete g_pPIC;          // interrupt controller
  delete g_pParser;       // the command line parser can go away first
  delete g_pLog;          // close the log file
  delete g_pConsole;      // lastly (always lastly!) close the console window
  delete g_pEvents;       // the event queue
#ifdef _DEBUG
#ifdef _WIN32
  system("pause");
  _CrtDumpMemoryLeaks();
#endif
#endif
  return 0;
}
