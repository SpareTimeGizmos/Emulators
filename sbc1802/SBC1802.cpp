//++
// SBC1802.cpp - SBC1802 Emulator main program
//
//   COPYRIGHT (C) 2015-2024 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
//
// LICENSE:
//    This file is part of the SBC1802 emulator project.  SBC1802 is free
// software; you may redistribute it and/or modify it under the terms of
// the GNU Affero General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any
// later version.
//
//    SBC1802 is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License
// for more details.  You should have received a copy of the GNU Affero General
// Public License along with SBC1802.  If not, see http://www.gnu.org/licenses/.
//
// DESCRIPTION:
//   This file is the main program for the SBC1802 Emulator task.  
//
//                                  Bob Armstrong [23-JUL-19]
//
// LIMITATIONS (aka the TO DO list)
//   Need to add an IRQ for the RTC and SLU0
//
// REVISION HISTORY:
// 16-JUN-22  RLA   Adapted from ELF2K.
// 26-AUG-22  RLA   Clean up Linux/WIN32 conditionals.
// 30-AUG-22  RLA   Make sure objects are deleted in the reverse order of creation!
// 21-DEC-23  RLA   Add two level I/O.
// 23-DEC-23  RLA   All second UART and TU58.
// 27-FEB-24  RLA   Add AY-3-8912.
// 31-MAY-24  RLA   When closing, delete the console first, then the log file.
//                  Delete all the initial console window size and color settings.
// 31-OCT-24  RLA   Add CDP1854 PPI.
//                  Add second AY-3-8912 PSG and CTwoPSGs class.
//  2-NOV-24  RLA   Add CDP1878 counter/timer
//  6-NOV-24  RLA   Make CPU clock frequency programmable
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
#include "SBC1802.hpp"          // global declarations for this project
#include "Interrupt.hpp"        // interrupt simulation logic
#include "EventQueue.hpp"       // I/O device event queue simulation
#include "MemoryTypes.h"        // address_t and word_t data types
#include "Memory.hpp"           // main memory emulation
#include "COSMACopcodes.hpp"    // COSMAC opcode definitions
#include "CPU.hpp"              // CCPU base class definitions
#include "COSMAC.hpp"           // COSMAC 1802 CPU emulation
#include "Device.hpp"           // CDevice I/O device emulation objects
#include "TLIO.hpp"             // RCA style two level I/O mapping
#include "MemoryMap.hpp"        // SBC1802 memory mapping hardware definitions
#include "POST.hpp"             // SBC1802 7 segment LED and DIP switches
#include "Baud.hpp"             // COM8136 baud rate generator
#include "CDP1854.hpp"          // CDP1854 UARTs for SLU0 and SLU1
#include "CDP1877.hpp"          // CDP1877 programmable interrupt controller
#include "CDP1879.hpp"          // CDP1879 real time clock
#include "IDE.hpp"              // standard IDE disk interface
#include "ElfDisk.hpp"          // SBC1802/ELF2K to IDE interface
#include "TU58.hpp"             // TU58 drive emulator
#include "PSG.hpp"              // AY-3-8912 programmable sound generator
#include "TwoPSGs.hpp"          // SBC1802 implementation of two PSGs
#include "PPI.hpp"              // generic programmable I/O definitions
#include "CDP1851.hpp"          // CDP1851 programmable I/O interface
#include "Timer.hpp"            // generic counter/timer emulation
#include "CDP1878.hpp"          // CDP1878 counter/timer
#include "UserInterface.hpp"    // SBC1802 user interface parse table definitions


// Global objects ....
//   These objects are used (more or less) everywhere within this program, and
// you'll find "extern ..." declarations for them in SBC1802.hpp.  Note that they
// are declared as pointers rather than the actual objects because we want
// to control the exact order in which they're created and destroyed!
CSmartConsole   *g_pConsole     = NULL; // console window object
CLog            *g_pLog         = NULL; // message logging object (including console!)
CCmdParser      *g_pParser      = NULL; // command line parser
// These globals point to the objects being emulated ...
CEventQueue     *g_pEvents      = NULL; // list of upcoming virtual events
CCOSMAC         *g_pCPU         = NULL; // 1802 COSMAC CPU
CGenericMemory  *g_pRAM         = NULL; // 64K of RAM emulation
CGenericMemory  *g_pROM         = NULL; // and 32K of EPROM emulation
CMemoryControl  *g_pMCR         = NULL; // memory control register
CMemoryMap      *g_pMemoryMap   = NULL; // memory mapping hardware
CTLIO           *g_pTLIO        = NULL; // two level I/O controller
CLEDS           *g_pLEDS        = NULL; // SBC1802 7 segment display
CSwitches       *g_pSwitches    = NULL; // SBC1802 DIP switches
CBaud           *g_pBRG         = NULL; // baud rate generator for SLU0 & 1
CCDP1854        *g_pSLU0        = NULL; // console UART
CElfDisk        *g_pIDE         = NULL; // standard disk interface
CCDP1879        *g_pRTC         = NULL; // CDP1879 real time clock
CCDP1877        *g_pPIC         = NULL; // CDP1877 programmable interrupt controller
// Extension board devices ...
CCDP1854        *g_pSLU1        = NULL; // secondary UART (for TU58)
CTU58           *g_pTU58        = NULL; // TU58 drive emulator
CPSG            *g_pPSG1        = NULL; // AY-3-8912 programmable sound generator #1
CPSG            *g_pPSG2        = NULL; // AY-3-8912 programmable sound generator #2
CTwoPSGs        *g_pTwoPSGs     = NULL; // SBC1802 implementation of two PSGs
CCDP1851        *g_pPPI         = NULL; // CDP1851 programmable I/O interface
CCDP1878        *g_pCTC         = NULL; // CDP1878 counter/timer


static bool ConfirmExit (CCmdParser &cmd)
{
  //++
  //   This routine is called whenever this application has been requested
  // to exit.  It returns true if we really should exit and false if we
  // shouldn't right now.
  //--
  return true;
}

static void CreateBaseBoard()
{
  //++
  // Create all the base board peripherals ...
  //--

  //   Create the memory (both RAM and ROM), interrupt controller, real time
  // clock and memory control register.  The latter three peripherals are all
  // memory mapped devices.  After that, we can create the CPU and attach the
  // memory and memory mapped devices.
  g_pRAM = DBGNEW CGenericMemory(RAMSIZE, RAMBASE, CMemory::MEM_RAM);
  g_pROM = DBGNEW CGenericMemory(ROMSIZE, ROMBASE, CMemory::MEM_ROM);
  g_pPIC = DBGNEW CCDP1877(PICBASE);
  g_pRTC = DBGNEW CCDP1879(RTCBASE, g_pEvents);
  g_pRTC->AttachInterrupt(g_pPIC->GetLevel(IRQ_RTC));
  g_pMCR = DBGNEW CMemoryControl(MCRBASE, g_pPIC);
  g_pMemoryMap = DBGNEW CMemoryMap(g_pRAM, g_pROM, g_pMCR, g_pRTC, g_pPIC);
  g_pCPU = DBGNEW CCOSMAC(g_pMemoryMap, g_pEvents, g_pPIC);
  g_pCPU->SetCrystalFrequency(CPUCLK);

  //   Create the two level I/O controller and attach it to ALL seven CPU I/O
  // instructions plus all four EF inputs.  The Q output, which isn't really
  // used in this design anyway, isn't affected by the two level I/O.
  g_pTLIO = DBGNEW CTLIO(TLIO_PORT, 1, CCOSMAC::MAXDEVICE);
  g_pCPU->InstallDevice(g_pTLIO);
  g_pCPU->InstallSense(g_pTLIO, CCOSMAC::EF1);
  g_pCPU->InstallSense(g_pTLIO, CCOSMAC::EF2);
  g_pCPU->InstallSense(g_pTLIO, CCOSMAC::EF3);
  g_pCPU->InstallSense(g_pTLIO, CCOSMAC::EF4);

  // The RTC IRQ is attached to group 1 EF2.
  g_pTLIO->InstallSense(BASE_GROUP, g_pRTC, RTC_IRQ_EF);

  //   Attach the LEDs and switches to group 1 port 4.  Note that the switches
  // are also associated with the INPUT button, which is attached to EF4 and
  // also interrupt request level 0.
  g_pLEDS = DBGNEW CLEDS(LEDS_PORT);
  g_pSwitches = DBGNEW CSwitches(SWITCHES_PORT);
  g_pSwitches->AttachInterrupt(g_pPIC->GetLevel(IRQ_INPUT));
  g_pTLIO->InstallDevice(BASE_GROUP, g_pLEDS);
  g_pTLIO->InstallDevice(BASE_GROUP, g_pSwitches);
  g_pTLIO->InstallSense(BASE_GROUP, g_pSwitches, INPUT_EF);

  // The baud rate generator is attached to group 1 port 7...
  g_pBRG = DBGNEW CBaud(BAUD_PORT);
  g_pTLIO->InstallDevice(BASE_GROUP, g_pBRG);

  //   The primary UART is attached to group 1, ports 2-3, and also to
  // EF3 (IRQ) plus EF1 (break)...
  g_pSLU0 = DBGNEW CCDP1854("SLU0", SLU0_PORT, g_pEvents, g_pConsole, g_pCPU, SLU0_IRQ_EF, SLU0_BREAK_EF);
  g_pSLU0->AttachInterrupt(g_pPIC->GetLevel(IRQ_SLU0));
  g_pTLIO->InstallDevice(BASE_GROUP, g_pSLU0);
  g_pTLIO->InstallSense(BASE_GROUP, g_pSLU0, SLU0_IRQ_EF);
  g_pTLIO->InstallSense(BASE_GROUP, g_pSLU0, SLU0_BREAK_EF);

  // And the IDE disk is attached to group 1, ports 5-6...
  g_pIDE = DBGNEW CElfDisk(IDE_PORT, g_pEvents);
  g_pIDE->AttachInterrupt(g_pPIC->GetLevel(IRQ_DISK));
  g_pTLIO->InstallDevice(BASE_GROUP, g_pIDE);
}

static void CreateExtensionBoard()
{
  //++
  // Create all the expansion board peripherals ...
  //--

  // SLU1 and the TU58 drive ...
  g_pTU58 = DBGNEW CTU58();
  g_pSLU1 = DBGNEW CCDP1854("SLU1", SLU1_PORT, g_pEvents, g_pTU58, NULL, SLU1_IRQ_EF, SLU1_SID_EF);
  g_pSLU1->AttachInterrupt(g_pPIC->GetLevel(IRQ_SLU1));
  g_pTLIO->InstallDevice(SLU1_GROUP, g_pSLU1);
  g_pTLIO->InstallSense(SLU1_GROUP, g_pSLU1, SLU1_IRQ_EF);
  g_pTLIO->InstallSense(SLU1_GROUP, g_pSLU1, SLU1_SID_EF);

  // CDP1851 programmable I/O interface ...
  g_pPPI = DBGNEW CCDP1851("PPI", PPI_PORT, g_pEvents, PPI_ARDY_EF, PPI_BRDY_EF, PPI_IRQ_EF, PPI_IRQ_EF);
  g_pPPI->AttachInterruptA(g_pPIC->GetLevel(IRQ_PPI));
  g_pPPI->AttachInterruptB(g_pPIC->GetLevel(IRQ_PPI));
  g_pTLIO->InstallDevice(PPI_GROUP, g_pPPI);
  g_pTLIO->InstallSense(PPI_GROUP, g_pPPI, PPI_ARDY_EF);
  g_pTLIO->InstallSense(PPI_GROUP, g_pPPI, PPI_BRDY_EF);
  g_pTLIO->InstallSense(PPI_GROUP, g_pPPI, PPI_IRQ_EF);

  // Programmable sound generators ...
  g_pPSG1 = DBGNEW CPSG("PSG1", PSG1_PORT, g_pEvents);
  g_pPSG2 = DBGNEW CPSG("PSG2", PSG2_PORT, g_pEvents);
  g_pTwoPSGs = DBGNEW CTwoPSGs(g_pPSG1, g_pPSG2, g_pEvents);
  g_pTLIO->InstallDevice(PSG_GROUP, g_pTwoPSGs);

  // CDP1878 counter/timer ...
  g_pCTC = DBGNEW CCDP1878("CTC", g_pEvents, TIMER_IRQ_EF);
  g_pCTC->SetClockA(g_pCPU->GetCrystalFrequency());
  g_pCTC->SetClockB(BAUDCLK/4UL);

  g_pCTC->AttachInterrupt(g_pPIC->GetLevel(IRQ_TIMER));
  g_pTLIO->InstallDevice(TIMER_GROUP, g_pCTC);
  g_pTLIO->InstallSense(TIMER_GROUP, g_pCTC, TIMER_IRQ_EF);
}

int main (int argc, char *argv[])
{
  //++
  // Here's the main program for the SBC1802 Emulator .
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
  g_pConsole = DBGNEW CSmartConsole(g_pEvents);
  g_pLog = DBGNEW CLog(PROGRAM, g_pConsole);
  g_pLog->SetDefaultConsoleLevel(CLog::WARNING);

  //   Parse the command options.  Note that we want to do this BEFORE we
  // setup the console window, since the command line may tell us to detach
  // and create a new window...
  if (!CStandardUI::ParseOptions(PROGRAM, argc, argv)) goto shutdown;

  //   Set the console window defaults - foreground and background color,
  // scrolling buffer size, title, and icon ...
  //g_pConsole->SetTitle("SBC1802 Emulator v%d", SBCVER);
  //m_pConsole->SetIcon(IDI_ELF);
  //g_pConsole->SetBufferSize(132, 2000);
  //g_pConsole->SetWindowSize(80, 40);
  //g_pConsole->SetColors(CConsoleWindow::YELLOW, CConsoleWindow::BLACK);

  // We're finally ready to say hello ...
  CMDOUTF("SBC1802 Emulator v%d emulator Library v%d", SBCVER, EMUVER);
  CMDOUTS("Built on " << __DATE__ << " " << __TIME__);
 //CMDOUTF("Word size = %d; Address size = %d; default radix = %d", WORDSIZE, ADDRSIZE, RADIX);

  // Create the base board peripherals ...
  CreateBaseBoard();

  // Create the extension board peripherals ...
  CreateExtensionBoard();

  //   Lastly, create the command line parser.  If a startup script was
  // specified on the command line, now is the time to execute it...
  g_pParser = DBGNEW CCmdParser(PROGRAM, CUI::g_aVerbs, &ConfirmExit, g_pConsole);
  if (!CStandardUI::g_sStartupScript.empty())
    g_pParser->OpenScript(CStandardUI::g_sStartupScript);

  //   This thread now becomes the background task, which loops forever
  // executing operator commands.  Well, almost forever - when the operator
  // types "EXIT" or "QUIT", the command parser exits and then we shutdown
  // the SBC1802 program.
  g_pParser->CommandLoop();
  LOGS(DEBUG, "command parser exited");

  // Delete all our global objects.  Once again, the order here is important!
  delete g_pParser;   // the command line parser can go away first

  // First delete the extension board peripherals ...
  delete g_pCTC;      // CDP1878 counter/timer
  delete g_pTwoPSGs;  // two PSG implementation
  delete g_pPSG2;     // programmable sound generator #1
  delete g_pPSG1;     // programmable sound generator #1
  delete g_pPPI;      // CDP1851 programmable I/O interface
  delete g_pSLU1;     // secondary serial line unit (for TU58)
  delete g_pTU58;     // TU58 tape emulator

  // Delete the base board peripherals, in the reverse order of their creation.
  delete g_pIDE;      // Elf disk emulator
  delete g_pSLU0;     // console serial line unit
  delete g_pBRG;      // baud rate generator
  delete g_pSwitches; // DIP configuration switches (DELETED by CCPU!)
  delete g_pLEDS;     // 7 segment POST display (DELETED by CCPU!)
  delete g_pTLIO;     // two level I/O controller
  delete g_pCPU;      // the COSMAC CPU
  delete g_pMemoryMap;// the memory mapping hardware
  delete g_pMCR;      // memory control register
  delete g_pRTC;      // the real time clock
  delete g_pPIC;      // interrupt controller
  delete g_pROM;      // the EPROM emulation
  delete g_pRAM;      // the SRAM emulation

  // Lastly we can get rid of the log file, console window and event queue.
shutdown:
  //   Note that the SmartConsole uses the log file to debug messages, so
  // it's critical to delete the console first, then the log file!
  delete g_pConsole;  // close the console window
  delete g_pLog;      // close the log file
  delete g_pEvents;   // the event queue
#ifdef _DEBUG
#ifdef _WIN32
  _CrtMemDumpAllObjectsSince(&memstate);
#endif
#endif
  return 0;
}
