//++
// UserInterface.cpp -> SBC6120 Emulator Specific User Interface code
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
//   This module implements the user interface specific to the SBC6120 emulator
// process.  The first half of the file are parse tables for the generic command
// line parser classes from CommandParser.cpp, and the second half is the action
// routines needed to implement these commands.  
//
//                                 Bob Armstrong [16-JUN-2022]
//
// SBC6120 COMMANDS
// ---------------
//   E*XAMINE ooooo             - display just address oooooo (octal)
//         ooooo-oooo           - display all addresses in the range
//         ooooo, ooooo, ...    - display multiple addresses or ranges
//         AC,MQ,L,IF,DF, ...   - display individual CPU register(s)
//         RE*GISTERS           - display all registers
//         /M*AIN               - display data from main memory (default)
//         /P*ANEL              -    "      "    "  control panel memory
//         /R*OM                -    "      "    "  EPROM
//         /I*NSTRUCTION        - disassemble 6120 instructions
//         /ASC*II              - also dump in PDP-8 packed ASCII text
//         /SIX*BIT             - also dump in PDP-8 SIXBIT text
//   Registers - AC,MQ,L,IF,DF,??
// 
//   D*EPOSIT ooooo oooo        - deposit one word
//         ooooo oooo, oooo,... - deposit several words
//         rr xxxx              - deposit in a register
//         /M*AIN               - deposit data in main memory (default)
//         /P*ANEL              -    "      "  "  control panel memory
//         /R*OM                -    "      "  "  EPROM
//
//   LO*AD filename             - load binary or .HEX file into RAM or ROM
//   SA*VE filename             - save RAM or ROM to a binary or .HEX file
//         /FORMAT=xxxx         - set file format
//         /BAS*E=ooooo         - load/save relative to base address (octal)
//         /COU*NT=nnnnn        - number of words to save (decimal)
//         /M*AIN               - load/save main memory (default)
//         /P*ANEL              -   "    "  control panel memory
//         /E*PROM              -   "    "  EPROM
//         /OVER*WRITE          - don't prompt if file already exists (SAVE only!)
//   Formats - BINARY, INTEL, ABSOLUTE
//
//   SH*OW CPU                  - show CPU details
//   CL*EAR CPU                 - reset the CPU only
//   SE*T CPU
//         /BRE*AK=nnn          - set break character to ASCII code nnn
//         /IO=STOP|IGNORE      - stop or ignore illegal IOTs
//         /OPCODE=STOP|IGNORE  -  "    "   "     "   "  opcodes
//         /STARTUP=MAIN|PANEL  - start execution in main or panel memory
//         /[NO]HALT            - HLT opcode traps to panel memory
// 
//   RU*N [ooooo]               - reset all and start running at IF,PC=ooooo
//         /SW*ITCHES=oooo      - set switch register to oooo before starting
//   C*ONTINUE                  - resume execution at current PC/
//   ST*EP [nnnn]               - single step and trace nnnn instructions
//   RES*ET                     - reset CPU and all devices
//
//   SE*T BRE*AKPOINT ooooo     - set breakpoint at address (octal)
//   CL*EAR BRE*AKPOINT 00000   - clear   "      "     "       "
//   CL*EAR BRE*AKPOINTS        - clear all breakpoints
//   SH*OW BRE*AKPOINTS         - show breakpoints
//         /M*AIN               - set/clear breakpoint in main memory (default)
//         /P*ANEL              -  "    "     "    "    " control panel memory
//         /E*PROM              -  "    "     "    "    " EPROM
//
//   CL*EAR MEM*ORY             - clear ALL of memory (MAIN, PANEL and EPROM!)
//         /M*AIN               - clear MAIN memory only
//         /P*ANEL              -   "   PANEL "  "    "
//         /E*PROM              -   "   EPROM "  "    "
//   SH*OW MEM*ORY              - show memory map for all modes
//
//   ATT*ACH IDE|RAM filename   - attach IDE drive or RAM disk to image file
//   DET*ACH IDE|RAM            - detach IDE drive or RAM disk
//        /UNIT=m               - select unit number for IDE or RAM disk
//        /CAPACITY=nnnnn       - set image size, IN SECTORS!
//
//   SH*OW DEV*ICE name         - show details for device <name>
//   SH*OW DEV*ICES             - show list of all devices
//   CL*EAR DEV*ICE name        - reset just device <name>
//   CL*EAR DEV*ICES            - reset all I/O devices only
//   SE*T DEV*ICE name          - set device parameters
//         /TX*SPEED=nnnn       - set SLU transmit speed, in CPS
//         /RX*SPEED=nnnn       -  "   "  receive    "    "   "
//         /SHO*RT=nnnn         - set IDE short delay, in microseconds
//         /LO*NG=nnnn          -  "   "  long    "    "    "    "
// 
//   SH*OW VER*SION             - show software version
// 
// STANDARD UI COMMANDS
// --------------------
//   SET WINDOW
//         /TIT*LE="string"     - set window title string
//         /FORE*GROUND=color   - set window foreground color
//         /BACK*GROUND=color   -  "     "   background   "
//         /X=nnn               - set window X position in pixels
//         /Y=nnn               -  "     "   Y     "     "   "
//         /W*IDTH=nn           - set window width in character columns
//         /H*EIGHT=nn          -  "     "   height "     "       "
//   Colors - BLACK, ORANGE, GRAY, BLUE, GREEN, CYAN, RED, MAGENTA, YELLOW, WHITE
//         BARK_BLUE, DARK_GREEN, DARK_CYAN, DARK_RED, DARK_MAGENTA, LIGHT_GRAY
// 
//   SET LOG*GING
//         /FI*LE=filename      - enable logging to a file
//         /NOFI*LE             - disable   "    "  "  "
//         /APP*END             - append to existing log file
//         /OVER*WRITE          - overwrite    "      "   "
//         /LEV*EL=level        - set logging level (with /FILE or /CONSOLE)
//         /CON*SOLE            - enable logging to console terminal
//   SHOW LOG*GING              - show current log settings
//   Levels - ERR*ORS, WARN*INGS, DEB*UG, TRA*CE
// 
//   SET CHECK*POINT
//         /ENA*BLE             - enable file checkpointing
//         /DISA*BLE            - disable "      "    "
//         /INT*ERVAL=nn        - set checkpointing interval in seconds
//   SHOW CHECK*POINT           - show current checkpoint settings
// 
//   DEF*INE name "substitution"- define alias commands
//   UNDEF*INE name             - undefule "      "
//   SHOW ALIAS name            - show definition for "name"
//   SHOW ALIAS*ES              - show all aliases
// 
//   DO filename                - execute commands from a file
// 
//   HELP name                  - show arguments and modifiers for one verb
//   HELP                       - show a list of all verbs
// 
//   EXIT                       - terminate the program
//   QUIT                       - ditto
// 
// 
// STANDARD COMMAND LINE OPTIONS
// -----------------------------
//   -d                 - set console message level to DEBUG
//   -l filename        - start logging to a file
//   -x                 - run as a detached process
//   filename           - take commands from a script file
// 
// 
// NOTES
// -----
//   UPPERCASE names are keywords.  lowercase names are arguments.
//   A "*" indicates the minimum unique abbreviation.
//   "nnnn" is a decimal number
//   "xxxx" is a hexadecimal number
//   "[...]" indicates optional arguments
// 
// REVISION HISTORY:
// 16-JUN-22  RLA   Adapted from ELF2K.
// 28-JUL-22  RLA   FindDevice() doesn't work for SET DEVICE
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include <string>               // C++ std::string class, et al ...
#include <iostream>             // C++ style output for LOGS() ...
#include <sstream>              // C++ std::stringstream, et al ...
#include "EMULIB.hpp"           // emulator library definitions
#include "LogFile.hpp"          // emulator library message logging facility
#include "ImageFile.hpp"        // emulator library image file methods
#include "ConsoleWindow.hpp"    // emulator console window methods
#include "CommandParser.hpp"    // emulator library command line parsing methods
#include "CommandLine.hpp"      // Shell (argc/argv) parser methods
#include "StandardUI.hpp"       // emulator library standard UI commands
#include "SBC6120.hpp"          // global declarations for this project
#include "MemoryTypes.h"        // address_t and word_t data types
#include "EventQueue.hpp"       // I/O device event queue simulation
#include "Memory.hpp"           // emulated memory functions
#include "DECfile8.hpp"         // DEC PDP8 absolute loader image file support
#include "Device.hpp"           // CDevice I/O device emulation objects
#include "DeviceMap.hpp"        // CDeviceMap memory mapped I/O emulation
#include "HD6120opcodes.hpp"    // one line assembler and disassembler
#include "Interrupt.hpp"        // interrupt simulation logic
#include "CPU.hpp"              // CCPU base class definitions
#include "HD6120.hpp"           // HD6120 specific emulation
#include "SLU.hpp"              // KL8E console serial line
#include "POST.hpp"             // SBC6120 three LED POST code display
#include "MiscellaneousIOTs.hpp"// miscellaneous SBC6120 641x and 643x IOTs 
#include "RAMdisk.hpp"          // SBC6120 RAM disk emulation
#include "MemoryMap.hpp"        // SBC6120 memory mapping hardware
#include "IDE.hpp"              // generic IDE disk definitions
#include "IDEdisk.hpp"          // SBC6120 specific IDE disk interface
#include "UserInterface.hpp"    // emulator user interface parse table definitions


// LOAD/SAVE file format keywords ...
const CCmdArgKeyword::KEYWORD CUI::m_keysFileFormat[] = {
  {"BIN*ARY",   CUI::FILE_FORMAT_BINARY},
  {"IN*TEL",    CUI::FILE_FORMAT_INTEL},
  {"ABS*OLUTE", CUI::FILE_FORMAT_PAPER_TAPE},
  {NULL, 0}
};

// STOP or IGNORE and MAIN or PANEL options for "SET CPU" ...
const CCmdArgKeyword::KEYWORD CUI::m_keysStopIgnore[] = {
  {"ST*OP",   true},
  {"IGN*ORE", false},
  {NULL, 0}
};
const CCmdArgKeyword::KEYWORD CUI::m_keysMainPanel[] = {
  {"MA*IN",  C6120::STARTUP_MAIN},
  {"PA*NEL", C6120::STARTUP_PANEL},
  {NULL,     0}
};


// Argument definitions ...
CCmdArgFileName    CUI::m_argFileName("file name");
CCmdArgFileName    CUI::m_argOptFileName("file name", true);
CCmdArgKeyword     CUI::m_argFileFormat("format", m_keysFileFormat);
CCmdArgNumber      CUI::m_argBaseAddress("starting address", 8, 0, ADDRESS_MAX);
CCmdArgNumber      CUI::m_argWordCount("word count", 10, 0, ADDRESS_MAX);
CCmdArgRangeOrName CUI::m_argExamineDeposit("name or range", 8, 0, ADDRESS_MAX);
CCmdArgList        CUI::m_argRangeOrNameList("name or range list", m_argExamineDeposit);
CCmdArgNumber      CUI::m_argData("data", 8, 0, WORD_MAX);
CCmdArgList        CUI::m_argDataList("data list", m_argData);
CCmdArgNumber      CUI::m_argStepCount("step count", 10, 1, INT16_MAX, true);
CCmdArgNumber      CUI::m_argRunAddress("start address", 8, 0, ADDRESS_MAX, true);
CCmdArgNumber      CUI::m_argBreakpoint("breakpoint address", 8, 0, ADDRESS_MAX);
CCmdArgNumber      CUI::m_argOptBreakpoint("breakpoint address", 8, 0, ADDRESS_MAX, true);
CCmdArgNumber      CUI::m_argBreakChar("break character", 10, 1, 31);
CCmdArgKeyword     CUI::m_argStopIO("stop on illegal IOT", m_keysStopIgnore);
CCmdArgKeyword     CUI::m_argStopOpcode("stop on illegal opcode", m_keysStopIgnore);
CCmdArgKeyword     CUI::m_argStartupMode("startup mode", m_keysMainPanel);
CCmdArgNumber      CUI::m_argSwitches("switches", 8, 0, WORD_MAX);
CCmdArgNumber      CUI::m_argTxSpeed("TX speed (cps)", 10, 1, 100000UL);
CCmdArgNumber      CUI::m_argRxSpeed("RX speed (cps)", 10, 1, 100000UL);
CCmdArgNumber      CUI::m_argShortDelay("short delay (us)", 10, 1, 1000000UL);
CCmdArgNumber      CUI::m_argLongDelay("long delay (us)", 10, 1, 1000000UL);
CCmdArgName        CUI::m_argOptDeviceName("device", true);
CCmdArgName        CUI::m_argDeviceName("device", false);
CCmdArgNumber      CUI::m_argUnit("unit", 10, 0, 255);
CCmdArgNumber      CUI::m_argCapacity("capacity", 10, 1, UINT32_MAX);

// Modifier definitions ...
CCmdModifier CUI::m_modFileFormat("FORM*AT", NULL, &m_argFileFormat);
CCmdModifier CUI::m_modInstruction("I*NSTRUCTION");
CCmdModifier CUI::m_modBreakChar("BRE*AK", NULL, &m_argBreakChar);
CCmdModifier CUI::m_modIllegalIO("IO", NULL, &m_argStopIO);
CCmdModifier CUI::m_modIllegalOpcode("OP*CODE", NULL, &m_argStopOpcode);
CCmdModifier CUI::m_modStartupMode("ST*ARTUP", NULL, &m_argStartupMode);
CCmdModifier CUI::m_modHaltOpcode("HA*LT", "NOHA*LT");
CCmdModifier CUI::m_modBaseAddress("BAS*E", NULL, &m_argBaseAddress);
CCmdModifier CUI::m_modWordCount("COU*NT", NULL, &m_argWordCount);
CCmdModifier CUI::m_modEPROM("E*PROM");
CCmdModifier CUI::m_modPanel("P*ANEL", "M*AIN");
CCmdModifier CUI::m_modASCII("A*SCII");
CCmdModifier CUI::m_modSIXBIT("S*IXBIT");
CCmdModifier CUI::m_modTxSpeed("TX*SPEED", NULL, &m_argTxSpeed);
CCmdModifier CUI::m_modRxSpeed("RX*SPEED", NULL, &m_argRxSpeed);
CCmdModifier CUI::m_modShortDelay("SHO*RT", NULL, &m_argShortDelay);
CCmdModifier CUI::m_modLongDelay("LO*NG", NULL, &m_argLongDelay);
CCmdModifier CUI::m_modUnit("UN*IT", NULL, &m_argUnit);
CCmdModifier CUI::m_modCapacity("CAP*ACITY", NULL, &m_argCapacity);
CCmdModifier CUI::m_modSwitches("SW*ITCHES", NULL, &m_argSwitches);
CCmdModifier CUI::m_modOverwrite("OVER*WRITE", "NOOVER*WRITE");


// EXAMINE and DEPOSIT verb definitions ...
CCmdArgument * const CUI::m_argsExamine[] = {&m_argRangeOrNameList, NULL};
CCmdArgument * const CUI::m_argsDeposit[] = {&m_argExamineDeposit, &m_argDataList, NULL};
CCmdModifier * const CUI::m_modsExamine[] = {&m_modInstruction, &m_modEPROM, &m_modPanel,
                                             &m_modASCII, &m_modSIXBIT, NULL};
CCmdModifier * const CUI::m_modsDeposit[] = {&m_modEPROM, &m_modPanel, NULL};
CCmdVerb CUI::m_cmdDeposit("D*EPOSIT", &DoDeposit, m_argsDeposit, m_modsDeposit);
CCmdVerb CUI::m_cmdExamine("E*XAMINE", &DoExamine, m_argsExamine, m_modsExamine);

// LOAD and SAVE commands ...
CCmdArgument * const CUI::m_argsLoadSave[] = {&m_argFileName, &m_argOptFileName, NULL};
CCmdModifier * const CUI::m_modsLoad[] = {&m_modFileFormat, &m_modBaseAddress, &m_modWordCount,
                                          &m_modEPROM, &m_modPanel, NULL};
CCmdModifier * const CUI::m_modsSave[] = {&m_modFileFormat, &m_modBaseAddress, &m_modWordCount,
                                          &m_modEPROM, &m_modPanel, &m_modOverwrite, NULL};
CCmdVerb CUI::m_cmdLoad("LO*AD", &DoLoad, m_argsLoadSave, m_modsLoad);
CCmdVerb CUI::m_cmdSave("SA*VE", &DoSave, m_argsLoadSave, m_modsSave);

// ATTACH and DETACH commands ...
CCmdArgument * const CUI::m_argsAttach[] = {&m_argFileName, NULL};
CCmdModifier * const CUI::m_modsDetach[] = {&m_modUnit, NULL};
CCmdModifier * const CUI::m_modsAttachDisk[] = {&m_modCapacity, &m_modUnit, NULL};
CCmdVerb CUI::m_cmdAttachIDE("IDE", &DoAttachIDE, m_argsAttach, m_modsAttachDisk);
CCmdVerb CUI::m_cmdDetachIDE("IDE", &DoDetachIDE, NULL, m_modsDetach);
CCmdVerb CUI::m_cmdAttachRAM("RAM", &DoAttachRAM, m_argsAttach, m_modsAttachDisk);
CCmdVerb CUI::m_cmdDetachRAM("RAM", &DoDetachRAM, NULL, m_modsDetach);
CCmdVerb * const CUI::g_aAttachVerbs[] = {
  &m_cmdAttachIDE, &m_cmdAttachRAM, NULL
};
CCmdVerb * const CUI::g_aDetachVerbs[] = {
  &m_cmdDetachIDE, &m_cmdDetachRAM, NULL
};
CCmdVerb CUI::m_cmdAttach("ATT*ACH", NULL, NULL, NULL, g_aAttachVerbs);
CCmdVerb CUI::m_cmdDetach("DET*ACH", NULL, NULL, NULL, g_aDetachVerbs);

// SET, CLEAR and SHOW BREAKPOINT commands ...
CCmdModifier * const CUI::m_modsBreakpoint[] = {&m_modEPROM, &m_modPanel, NULL};
CCmdArgument * const CUI::m_argsSetBreakpoint[] = {&m_argBreakpoint, NULL};
CCmdArgument * const CUI::m_argsClearBreakpoint[] = {&m_argOptBreakpoint, NULL};
CCmdVerb CUI::m_cmdSetBreakpoint("BRE*AKPOINT", &DoSetBreakpoint, m_argsSetBreakpoint, m_modsBreakpoint);
CCmdVerb CUI::m_cmdClearBreakpoint("BRE*AKPOINT", &DoClearBreakpoint, m_argsClearBreakpoint, m_modsBreakpoint);
CCmdVerb CUI::m_cmdShowBreakpoint("BRE*AKPOINT", &DoShowBreakpoints, NULL, m_modsBreakpoint);

// RUN, CONTINUE, STEP, and RESET commands ...
CCmdArgument * const CUI::m_argsStep[] = {&m_argStepCount, NULL};
CCmdArgument * const CUI::m_argsRun[] = {&m_argRunAddress, NULL};
CCmdModifier * const CUI::m_modsRun[] = {&m_modSwitches, NULL};
CCmdVerb CUI::m_cmdRun("RU*N", &DoRun, m_argsRun, m_modsRun);
CCmdVerb CUI::m_cmdContinue("C*ONTINUE", &DoContinue);
CCmdVerb CUI::m_cmdStep("ST*EP", &DoStep, m_argsStep, NULL);
CCmdVerb CUI::m_cmdReset("RE*SET", &DoReset);

// SET, CLEAR and SHOW CPU ...
CCmdModifier * const CUI::m_modsSetCPU[] = {
    &m_modHaltOpcode, &m_modIllegalIO, &m_modIllegalOpcode,
    &m_modBreakChar, &m_modStartupMode, NULL
};
CCmdVerb CUI::m_cmdSetCPU = {"CPU", &DoSetCPU, NULL, m_modsSetCPU};
CCmdVerb CUI::m_cmdClearCPU("CPU", &DoClearCPU);
CCmdVerb CUI::m_cmdShowCPU("CPU", &DoShowCPU);

// CLEAR and SHOW MEMORY ...
CCmdModifier * const CUI::m_modsClearMemory[] = {&m_modEPROM, &m_modPanel, NULL};
CCmdVerb CUI::m_cmdClearMemory("MEM*ORY", &DoClearMemory, NULL, m_modsClearMemory);
CCmdVerb CUI::m_cmdShowMemory("MEM*ORY", &DoShowMemory);

// CLEAR and SHOW DEVICE ...
CCmdArgument * const CUI::m_argsShowDevice[] = {&m_argOptDeviceName, NULL};
CCmdArgument * const CUI::m_argsSetDevice[] = {&m_argDeviceName, NULL};
CCmdModifier * const CUI::m_modsSetDevice[] = {
    &m_modTxSpeed, &m_modRxSpeed, &m_modShortDelay, &m_modLongDelay, NULL
};
CCmdVerb CUI::m_cmdShowDevice("DEV*ICES", &DoShowDevice, m_argsShowDevice, NULL);
CCmdVerb CUI::m_cmdSetDevice("DEV*ICE", &DoSetDevice, m_argsSetDevice, m_modsSetDevice);
CCmdVerb CUI::m_cmdClearDevice("DEV*ICES", &DoClearDevice, m_argsShowDevice, NULL);

// CLEAR verb definition ...
CCmdVerb * const CUI::g_aClearVerbs[] = {
    &m_cmdClearBreakpoint, &m_cmdClearCPU,
    &m_cmdClearMemory,
    &m_cmdClearDevice,
  NULL
};
CCmdVerb CUI::m_cmdClear("CL*EAR", NULL, NULL, NULL, g_aClearVerbs);

// SET verb definition ...
CCmdVerb * const CUI::g_aSetVerbs[] = {
  &m_cmdSetBreakpoint, &m_cmdSetCPU, &m_cmdSetDevice,
  &CStandardUI::m_cmdSetLog, &CStandardUI::m_cmdSetWindow,
#ifdef THREADS
  &CStandardUI::m_cmdSetCheckpoint,
#endif
  NULL
};
CCmdVerb CUI::m_cmdSet("SE*T", NULL, NULL, NULL, g_aSetVerbs);

// SHOW verb definition ...
CCmdVerb CUI::m_cmdShowVersion("VER*SION", &DoShowVersion);
CCmdVerb * const CUI::g_aShowVerbs[] = {
  &m_cmdShowBreakpoint, &m_cmdShowCPU, &m_cmdShowDevice,
  &m_cmdShowMemory, &m_cmdShowVersion,
    &CStandardUI::m_cmdShowLog, &CStandardUI::m_cmdShowAliases,
#ifdef THREADS
    &CStandardUI::m_cmdShowCheckpoint,
#endif
    NULL
};
CCmdVerb CUI::m_cmdShow("SH*OW", NULL, NULL, NULL, g_aShowVerbs);

// Master list of all verbs ...
CCmdVerb * const CUI::g_aVerbs[] = {
  &m_cmdExamine, &m_cmdDeposit,
  &m_cmdLoad, &m_cmdSave, 
  &m_cmdAttach, &m_cmdDetach,
  &m_cmdRun, &m_cmdContinue, &m_cmdStep, &m_cmdReset,
  &m_cmdSet, &m_cmdShow, &m_cmdClear,
  &CStandardUI::m_cmdDefine, &CStandardUI::m_cmdUndefine,
  &CStandardUI::m_cmdIndirect, &CStandardUI::m_cmdExit,
  &CStandardUI::m_cmdQuit, &CCmdParser::g_cmdHelp,
  NULL
};



////////////////////////////////////////////////////////////////////////////////
///////////////////////// EXAMINE and DEPOSIT COMMANDS /////////////////////////
////////////////////////////////////////////////////////////////////////////////

CGenericMemory *CUI::GetMemorySpace()
{
  //++
  //   Figure out which memory space is required - /ROM selects
  // the EPROM space; /PANEL selects control panel memory, and
  // /MAIN (or no switch at all) selects main memory ...
  //--
  assert((g_pMainMemory != NULL) && (g_pPanelMemory != NULL) && (g_pEPROM != NULL));
  if (m_modEPROM.IsPresent() && m_modPanel.IsPresent()) {
    CMDERRS("conflicting address space switches");  return g_pMainMemory;
  }
  if (m_modEPROM.IsPresent() && !m_modEPROM.IsNegated())
    return g_pEPROM;
  else if (m_modPanel.IsPresent() && !m_modPanel.IsNegated())
    return g_pPanelMemory;
  else
    return g_pMainMemory;
}

string CUI::DumpASCII (word_t w1, word_t w2)
{
  //++
  //   Dump two PDP-8 words as three ASCII characters packed using the usual
  // OS/8 scheme.  Only printing ASCII characters are displayed - non-printing
  // characters show as a ".".
  //--
  char c1 = w1 & 0177;
  char c2 = w2 & 0177;
  char c3 = (((w1 & 07400) >> 4) | ((w2 & 07400) >> 8)) & 0177;
  if (!isprint(c1)) c1 = '.';
  if (!isprint(c2)) c2 = '.';
  if (!isprint(c3)) c3 = '.';
  return FormatString("%c%c%c", c1, c2, c3);
}

string CUI::DumpSIXBIT (word_t w)
{
  //++
  //   Dump one word as two characters in the brain damaged OS/8 SIXBIT (which
  // is not the same as DECsystem-10 SIXBIT!).  No need to worry about non-
  // printing characters here; in SIXBIT everything is printable.
  // 
  //   BTW, just in case you aren't familiar with such things, the DEC-10 does
  // SIXBIT by simply subtracting 40 (octal) from each character.  That's easy
  // to reverse, but for whatever reason the PDP-8 does SIXBIT by ANDing each
  // character with 77 (octal).  That's a little harder to reverse, because 
  // values 40-77 (octal) don't change, but values 00..37 need to have 100
  // (octal) added to them.
  //--
  char c1 = ((w >> 6) & 077);
  char c2 = ( w       & 077);
  if (c1 < 040) c1 += 0100;
  if (c2 < 040) c2 += 0100;
  return FormatString("%c%c", c1, c2);
}

void CUI::DumpLine (const CGenericMemory *pMemory, address_t nStart, size_t cwWords,
                    unsigned nIndent, unsigned nPad, bool fShowASCII, bool fShowSIXBIT)
{
  //++
  //   Dump out one line of memory contents, word by word and always in
  // octal, packed ASCII and SIXBIT, for the EXAMINE command.  The line 
  // can optionally be padded on the left (nIndent > 0) or the right
  // (nPad > 0) so that we can line up rows that don't start on a 
  // multiple of 8.
  //--
  string sLine;  unsigned i;
  sLine = FormatString("%05o/ ", (uint16_t) nStart);
  for (i = 0; i < nIndent; ++i) sLine += "     ";
  for (i = 0; i < cwWords; ++i) {
    sLine += FormatString("%04o ", pMemory->UIread(nStart+i));
  }
  for (i = 0; i < nPad; ++i) sLine += "     ";
  if (fShowASCII) {
    sLine += "\t";
    for (i = 0; i < nIndent / 2; ++i) sLine += "   ";
    for (i = 0; i < cwWords; i += 2) {
      address_t wBase = (nStart + i) & (~1);
      word_t w1 = pMemory->UIread(wBase);
      word_t w2 = pMemory->UIread(ADDRESS(wBase + 1));
      sLine += DumpASCII(w1, w2);
    }
    for (i = 0; i < nPad / 2; ++i) sLine += "   ";
  }
  if (fShowSIXBIT) {
    sLine += "\t";
    for (i = 0; i < nIndent; ++i) sLine += "  ";
    for (i = 0; i < cwWords; i++) {
      word_t w = pMemory->UIread(ADDRESS(nStart+i));
      sLine += DumpSIXBIT(w);
    }
    for (i = 0; i < nPad; ++i) sLine += "  ";
  }
  CMDOUTS(sLine);
}

void CUI::DoExamineRange (const CGenericMemory *pMemory, address_t nStart, address_t nEnd)
{
  //++
  //   This method handles the EXAMINE command where the argument is a range
  // of memory addresses.  If the range is a single word then we just print
  // that byte and quit.  If the range is more than one word but less than 8
  // then it prints a single line with just those words.  If the range is
  // larger than 8 words then it prints multiple lines, carefully fixed up
  // to align with multiples of 16 and with the first and last lines indented
  // so that all bytes with the same low order 3 address bits line up.
  //--
  bool fASCII  = m_modASCII.IsPresent()  && !m_modASCII.IsNegated();
  bool fSIXBIT = m_modSIXBIT.IsPresent() && !m_modSIXBIT.IsNegated();
  if (nStart == nEnd) {
    CMDOUTF("%05o/ %04o", nStart, pMemory->UIread(nStart));
  } else if ((nEnd-nStart+1) < 8) {
    DumpLine(pMemory, nStart, nEnd-nStart+1);
  } else {
    if ((nStart & 07) != 0) {
      address_t nBase = nStart & 077770;
      address_t nOffset = nStart - nBase;
      DumpLine(pMemory, nStart, 8-nOffset, (unsigned) nOffset, 0, fASCII, fSIXBIT);
      nStart += 8-nOffset;
    }
    for (; nStart <= nEnd; nStart += 8) {
      if ((nEnd-nStart) < 8)
        DumpLine(pMemory, nStart, nEnd-nStart+1, 0, (unsigned) (8-(nEnd-nStart+1)), fASCII, fSIXBIT);
      else
        DumpLine(pMemory, nStart, 8, 0, 0, fASCII, fSIXBIT);
    }
  }
}

string CUI::DoExamineInstruction (address_t nStart, const CGenericMemory *pMemory)
{
  //++
  //   This method will disassemble one instruction for the EXAMINE/INSTRUCTION
  // command.  In this case, PDP-8 instructions are always exactly one word
  // long and we don't have to worry about the length.
  //--
  word_t wOpcode;  string sCode;
  wOpcode = pMemory->UIread(nStart);
  return FormatString("%05o/ %04o\t", nStart, wOpcode) + Disassemble(nStart, wOpcode);
}

string CUI::ExamineRegister (int nIndex)
{
  //++
  //   This method will fetch the contents of an internal CPU register and
  // return a formatted string with the register name and value.  This is a
  // tiny bit tricky because registers can have 1, 3, or 12 bits and we try
  // to print the right thing.
  //--
  const CCmdArgKeyword::KEYWORD *paNames = g_pCPU->GetRegisterNames();
  cpureg_t nRegister = (int) paNames[nIndex].m_pValue;
  unsigned nSize = g_pCPU->GetRegisterSize(nRegister) / 3;
  uint16_t nValue = g_pCPU->GetRegister(nRegister);
  return FormatString("%s=%0*o", paNames[nIndex].m_pszName, nSize, nValue);
}

bool CUI::DoExamineOneRegister (string sName)
{
  //++
  //   Examine the contents of a single internal CPU register, given its name
  // from the command line.  If the name isn't a register name, then silently
  // return false (it might be a device name or something else!)...
  //--
  const CCmdArgKeyword::KEYWORD *paNames = g_pCPU->GetRegisterNames();
  int nIndex = CCmdArgKeyword::Search(sName, paNames);
  if (nIndex < 0) return false;
  CMDOUTS(ExamineRegister(nIndex));
  return true;
}

void CUI::DoExamineAllRegisters (bool fBrief)
{
  //++
  //   Print the contents of ALL internal CPU registers (formatted as neatly
  // as we can without knowing too much about them!)...
  //--
  string sLine;
  const CCmdArgKeyword::KEYWORD *paNames = g_pCPU->GetRegisterNames();
  for (int i = 0; paNames[i].m_pszName != NULL; ++i) {
    string sRegister = ExamineRegister(i);
    if ((sLine.length()+sRegister.length()) > 55) {
      CMDOUTS(sLine);  sLine.clear();
    }
    sLine += sRegister + "  ";
    if (fBrief && (i == C6120::REG_PS)) break;
  }
  if (!sLine.empty()) CMDOUTS(sLine);
}

bool CUI::DoExamine (CCmdParser &cmd)
{
  //++
  //   This is the general case for the EXAMINE command.  It can examine a
  // single memory address or register, a range of addresses, all internal
  // CPU registers, or any combination of those.
  //--
  for (size_t i = 0;  i < m_argRangeOrNameList.Count();  ++i) {
    CCmdArgRangeOrName *pArg = dynamic_cast<CCmdArgRangeOrName *> (m_argRangeOrNameList[i]);
    assert(pArg != NULL);
    if (pArg->IsName()) {
      string sName = pArg->GetNameArg().GetValue();
      if (CCmdArgKeyword::Match(sName, "REG*ISTERS")) {
        DoExamineAllRegisters();
      } else if (!DoExamineOneRegister(sName)) {
        CMDERRS("Unknown register - \"" << sName << "\"");  return false;
      }
    } else {
      address_t nStart = pArg->GetRangeArg().GetStart();
      address_t nEnd = pArg->GetRangeArg().GetEnd();
      CGenericMemory *pMemory = GetMemorySpace();
      if (!pMemory->IsValid(nStart, nEnd)) {
         CMDERRF("range exceeds memory - %05o to %05o", nStart, nEnd);  return false;
      } else if (m_modInstruction.IsPresent()) {
        while (nStart <= nEnd) {
          CMDOUTS(DoExamineInstruction(nStart, pMemory));
          nStart = ADDRESS(nStart+1);
        }
      } else
        DoExamineRange(pMemory, nStart, nEnd);
    }
  }
  m_argRangeOrNameList.ClearList();
  return true;
}

bool CUI::DoDepositRange (CGenericMemory *pMemory, address_t nStart, address_t nEnd, CCmdArgList &List)
{
  //++
  //   Deposit one or more words into main memory starting from nStart and
  // proceeding to successively higher addresses.  If the number of data items
  // would cause nEnd to be exceeded, then give an error message and quit.
  // nEnd is otherwise ignored - i.e. it's not an error to specify too few
  // items!
  //--
  bool fHasEnd = nStart != nEnd;
  for (size_t i = 0;  i < List.Count();  ++i) {
    if (fHasEnd && (nStart > nEnd)) {
      CMDERRS("too many data items to deposit");  return false;
    }
    CCmdArgNumber *pData = dynamic_cast<CCmdArgNumber *> (List[i]);
    if (!pMemory->IsValid(nStart)) {
      CMDERRF("address exceeds memory - %05o", nStart);  return false;
    } else
      pMemory->UIwrite(nStart, pData->GetNumber());
    ++nStart;
  }
  return true;
}

bool CUI::DoDepositRegister (string sName, uint16_t nValue)
{
  //++
  // Deposit a new value into a CPU internal register ...
  //--
  const CCmdArgKeyword::KEYWORD *paNames = g_pCPU->GetRegisterNames();
  int nIndex = CCmdArgKeyword::Search(sName, paNames);
  if (nIndex < 0) {
    CMDERRS("Unknown register - \"" << sName << "\"");  return false;
  }
  cpureg_t nRegister = (int) paNames[nIndex].m_pValue;
  g_pCPU->SetRegister(nRegister, nValue);
  return true;
}

bool CUI::DoDeposit (CCmdParser &cmd)
{
  //++
  //   The DEPOSIT command can be used to alter main memory OR any internal
  // CPU register.  The register form takes only two arguments - the name of
  // the register and a new value, in octal.  Altering main memory needs a
  // memory address and then a list of one or more octal numbers to be stored.
  // If multiple data items are specified then they are stored in successively
  // higher memory addresses starting from the one specified.  It's actually
  // possible to specify a range for the memory address - in that case the
  // ending address is ignored UNLESS the number of data items specified would
  // exceed the range, in which case an error occurs.
  //--
  if (m_argExamineDeposit.IsName()) {
    if (m_argDataList.Count() > 1) {
      CMDERRS("only one value allowed for DEPOSIT register");
      return false;
    }
    string sRegister = m_argExamineDeposit.GetValue();
    CCmdArgNumber *pData = dynamic_cast<CCmdArgNumber *> (m_argDataList[0]);
    if (!DoDepositRegister(sRegister, pData->GetNumber())) return false;
  } else {
    address_t nStart = m_argExamineDeposit.GetRangeArg().GetStart();
    address_t nEnd = m_argExamineDeposit.GetRangeArg().GetEnd();
    CGenericMemory *pMemory = GetMemorySpace();
    if (!pMemory->IsValid(nStart, nEnd)) {
      CMDERRF("range exceeds memory - %05o to %05o", nStart, nEnd);  return false;
    }
    if (!DoDepositRange(pMemory, nStart, nEnd, m_argDataList)) return false;
  }
  m_argDataList.ClearList();
  return true;
}



////////////////////////////////////////////////////////////////////////////////
//////////////////////////// LOAD AND SAVE COMMANDS ////////////////////////////
////////////////////////////////////////////////////////////////////////////////

bool CUI::GetImageFileNameAndFormat (bool fCreate, string &sFileName1, string &sFileName2, int &nFormat)
{
  //++
  //   This method will get the memory image file name and format for the LOAD
  // and SAVE commands.  For the PDP8 three file types are supported - Intel
  // hex (which we use to program EPROMs!), straight binary and DEC absolute
  // loader paper tape images.  The algorithm we use to figure out the type is
  // fairly complicated but really useful.  First, if the /FORMAT modifier is
  // used, then that always takes precedence, and when /FORMAT is specified
  // and the file name given doesn't have an extension then we'll supply an
  // appriate default.
  //
  //   If the /FORMAT modifier isn't specified but the filename does have an
  // explicit extension, either ".hex", ".bin" or ".ptp", then that determines
  // the file format.  And of course no default extension is needed this time.
  //
  //   If there was no /FORMAT and no extension specified (e.g. "LOAD FOO")
  // then we'll try to figure out the type by first looking for a "FOO.HEX",
  // a "FOO.BIN" and then a "FOO.PTP", in that order.  If one of those exists
  // then we'll go with that one, and if none exists then it's an error.
  // 
  //   Lastly, Intel HEX format is a problem because those files hold only 8
  // bit bytes and we have a 12 bit machine here!  The SBC6120 uses two .HEX 
  // files to program two EPROMs, a HIGH (containing the high order 6 bits of
  // every word) and a LOW (containing, you guessed it, the low 6 bits).  If,
  // and only if, the Intel format is specified then TWO file names are
  // required.  In all other cases, sFileName2 is ignored.
  //--
  string sDrive, sDir, sName, sExt;
  sFileName1 = m_argFileName.GetFullPath();
  sFileName2.clear();  nFormat = FILE_FORMAT_NONE;

  // Try to figure out the extension and format ...
  if (m_modFileFormat.IsPresent()) {
    // /FORMAT was specified!
    nFormat = (int) m_argFileFormat.GetKeyValue();
    if (nFormat == FILE_FORMAT_BINARY)
      sFileName1 = CCmdParser::SetDefaultExtension(sFileName1, DEFAULT_BINARY_FILE_TYPE);
    else if (nFormat == FILE_FORMAT_INTEL)
      sFileName1 = CCmdParser::SetDefaultExtension(sFileName1, DEFAULT_INTEL_FILE_TYPE);
    else
      sFileName1 = CCmdParser::SetDefaultExtension(sFileName1, DEFAULT_PAPERTAPE_FILE_TYPE);
  } else {
    SplitPath(sFileName1, sDrive, sDir, sName, sExt);
    if (sExt.empty() && !fCreate) {
      // No extension given - try searching for .hex, .bin or .ptp ...
      if (FileExists(MakePath(sDrive, sDir, sName, DEFAULT_BINARY_FILE_TYPE))) {
        sFileName1 = MakePath(sDrive, sDir, sName, DEFAULT_BINARY_FILE_TYPE);
        nFormat = FILE_FORMAT_BINARY;
      } else if (FileExists(MakePath(sDrive, sDir, sName, DEFAULT_INTEL_FILE_TYPE))) {
        sFileName1 = MakePath(sDrive, sDir, sName, DEFAULT_INTEL_FILE_TYPE);
        nFormat = FILE_FORMAT_INTEL;
      } else if (FileExists(MakePath(sDrive, sDir, sName, DEFAULT_PAPERTAPE_FILE_TYPE))) {
        sFileName1 = MakePath(sDrive, sDir, sName, DEFAULT_PAPERTAPE_FILE_TYPE);
        nFormat = FILE_FORMAT_PAPER_TAPE;
      }
    } else if (sExt == DEFAULT_BINARY_FILE_TYPE) {
      nFormat = FILE_FORMAT_BINARY;
    } else if (sExt == DEFAULT_INTEL_FILE_TYPE) {
      nFormat = FILE_FORMAT_INTEL;
    } else if (sExt == DEFAULT_PAPERTAPE_FILE_TYPE) {
      nFormat = FILE_FORMAT_PAPER_TAPE;
    }
  }

  // If we still don't know the format then give up ...
  if (nFormat == FILE_FORMAT_NONE) {
    CMDERRS("Unable to determine the format for " << sFileName1);
    return false;
  }

  //   If this is an Intel format file, then look for the second file name and
  // apply the same default extension to it.  If this NOT an Intel format file,
  // then only one file name is allowed.
  if (nFormat == FILE_FORMAT_INTEL) {
    if (!m_argOptFileName.IsPresent()) {
      CMDERRS("two file names required");  return false;
    }
    sFileName2 = m_argOptFileName.GetFullPath();
    SplitPath(sFileName2, sDrive, sDir, sName, sExt);
    if (sExt.empty() && !fCreate) {
      // No extension given - use .HEX ...
      sFileName2 = MakePath(sDrive, sDir, sName, DEFAULT_INTEL_FILE_TYPE);
    }
  } else if (m_argOptFileName.IsPresent()) {
    CMDERRS("only one file name allowed");  return false;
  }
  return true;
}

void CUI::GetImageBaseAndOffset (address_t &wBase, size_t &cbBytes)
{
  //++
  //   This method will try to figure out the starting address (aka the base)
  // and the size (in bytes) of the memory region to be loaded or saved.  When
  // we're saving it's pretty straight forward and these two parameters
  // determine exactly what gets written.  When we're loading it's a little
  // more complicated, however.  Binary files don't contain any address 
  // information, so the base determines where in memory the file will be 
  // loaded.  Intel hex files do contain an address, and the base is added
  // to the address specified by the file.
  //--

  // Figure out the base address ...
  if (m_modBaseAddress.IsPresent())
    wBase = m_argBaseAddress.GetNumber();
  else
    wBase = 0;

  // And figure out the size ...
  if (m_modWordCount.IsPresent())
    cbBytes = m_argWordCount.GetNumber();
  else
    cbBytes = ADDRESS_MAX-wBase+1;
}

bool CUI::DoLoad (CCmdParser &cmd)
{
  //++
  //   The LOAD command loads memory from a disk file in Intel HEX format or 
  // plain binary.  Note that in the SBC6120 all the RAM is battery backed up,
  // and there is no separate NVR chip (only the CDP1879 RTC) so there is no
  // LOAD/NVR command!
  //--
  string sFileName1, sFileName2;  int nFormat;  int32_t nBytes = 0;
  if (!GetImageFileNameAndFormat(false, sFileName1, sFileName2, nFormat)) return false;
  CGenericMemory *pMemory = GetMemorySpace();

  // Get the address range to be loaded ...
  address_t wBase = 0;  size_t cbLimit = 0;
  GetImageBaseAndOffset(wBase, cbLimit);
  if (cbLimit > pMemory->Size()) cbLimit = pMemory->Size();
  switch (nFormat) {
    case FILE_FORMAT_BINARY:
       nBytes = pMemory->LoadBinary(sFileName1, wBase, cbLimit);  break;
    case FILE_FORMAT_INTEL:
      nBytes = CDECfile8::Load2Intel(pMemory, sFileName1, sFileName2, wBase, cbLimit);  break;
    case FILE_FORMAT_PAPER_TAPE:
      nBytes = CDECfile8::LoadPaperTape(pMemory, sFileName1);  break;
  }

  // And we're done!
  if (nBytes < 0) return false;
  uint16_t nSegments = HIWORD(nBytes);
  if (nSegments > 0) 
    CMDOUTF("%d words in %d segments loaded from %s", LOWORD(nBytes), nSegments, sFileName1.c_str());
  else
    CMDOUTF("%d words loaded from %s", nBytes, sFileName1.c_str());
  return true;
}

bool CUI::DoSave (CCmdParser &cmd)
{
  //++
  //   SAVE is basically the same as LOAD (dataflow direction not withstanding,
  // of course!) except that this time we check to see if the output file
  // already exists.  If it does, then we ask "Are you sure?" before over
  // writing it...
  //--
  string sFileName1, sFileName2;  int nFormat;  int32_t nBytes = 0;
  if (!GetImageFileNameAndFormat(false, sFileName1, sFileName2, nFormat)) return false;
  CGenericMemory *pMemory = GetMemorySpace();

  // Save RAM or ROM ...
  address_t wBase = 0;  size_t cbBytes = 0;
  GetImageBaseAndOffset(wBase, cbBytes);
  if (cbBytes > pMemory->Size()) cbBytes = pMemory->Size();
  if (!(m_modOverwrite.IsPresent() && !m_modOverwrite.IsNegated())) {
    if (FileExists(sFileName1)) {
      if (!cmd.AreYouSure(sFileName1 + " already exists")) return false;
    }
    if (!sFileName2.empty() && FileExists(sFileName2)) {
      if (!cmd.AreYouSure(sFileName2 + " already exists")) return false;
    }
  }
  switch (nFormat) {
    case FILE_FORMAT_BINARY:
      nBytes = pMemory->SaveBinary(sFileName1, wBase, cbBytes);  break;
    case FILE_FORMAT_INTEL:
      nBytes = CDECfile8::Save2Intel(pMemory, sFileName1, sFileName2, wBase, cbBytes);  break;
    case FILE_FORMAT_PAPER_TAPE:
      nBytes = CDECfile8::SavePaperTape(pMemory, sFileName1, wBase, cbBytes);  break;
  }

  // All done...
  if (nBytes < 0) return false;
  CMDOUTF("%d bytes saved to %s", nBytes, sFileName1.c_str());
  return true;
}



////////////////////////////////////////////////////////////////////////////////
////////////////////////// ATTACH and DETACH COMMANDS //////////////////////////
////////////////////////////////////////////////////////////////////////////////

bool CUI::GetUnit (uint8_t &nUnit, uint8_t nMaxUnit)
{
  //++
  // Return the /UNIT modifier, or zero if none ...
  //--
  nUnit = 0;
  if (!m_modUnit.IsPresent()) return true;
  nUnit = m_argUnit.GetNumber();
  if ((nMaxUnit > 0)  &&  (nUnit >= nMaxUnit)) {
    CMDERRF("invalid unit (%d maximum)",nMaxUnit);
    return false;
  }
  return true;
}

bool CUI::DoAttachIDE (CCmdParser &cmd)
{
  //++
  // Attach the IDE disk drive to an external image file...
  //--
  assert(g_pIDEdisk != NULL);
  uint8_t nUnit;
  if (!GetUnit(nUnit, CIDEdisk::NDRIVES)) return false;

  if (g_pIDEdisk->IsAttached(nUnit)) {
    CMDERRS("IDE already attached to " << g_pIDEdisk->GetFileName(nUnit));
    return false;
  }

  // The default image extension is ".dsk" ...
  string sFileName = m_argFileName.GetFullPath();
  if (!FileExists(sFileName)) {
    string sDrive, sDir, sName, sExt;
    SplitPath(sFileName, sDrive, sDir, sName, sExt);
    sFileName = MakePath(sDrive, sDir, sName, ".ide");
  }

  // Attach the drive to the file, and we're done!
  uint32_t lCapacity = m_modCapacity.IsPresent() ? m_argCapacity.GetNumber() : 0;
  if (!g_pIDEdisk->Attach(nUnit, sFileName, lCapacity)) return false;
  CMDOUTS("IDE attached to " << sFileName);
  return true;
}

bool CUI::DoAttachRAM (CCmdParser &cmd)
{
  //++
  // Attach the RAM disk drive to an external image file...
  //--
  assert(g_pRAMdisk != NULL);
  uint8_t nUnit;
  if (!GetUnit(nUnit, CRAMdisk::NDRIVES)) return false;

  if (g_pRAMdisk->IsAttached(nUnit)) {
    CMDERRS("RAM disk unit " << nUnit << " already attached to " << g_pRAMdisk->GetFileName(nUnit));
    return false;
  }

  // The default image extension is ".ram" ...
  string sFileName = m_argFileName.GetFullPath();
  if (!FileExists(sFileName)) {
    string sDrive, sDir, sName, sExt;
    SplitPath(sFileName, sDrive, sDir, sName, sExt);
    sFileName = MakePath(sDrive, sDir, sName, ".vmd");
  }

  // Attach the drive to the file, and we're done!
  //   Note that the argument for the /CAPACITY modifier is in kilobytes, and
  // should be either 128 or 512.  The CRAMdisk::Attach() method wants the
  // capacity to be specified in sectors, or banks, which are 4K bytes each...
  uint32_t lCapacity = m_modCapacity.IsPresent() ? m_argCapacity.GetNumber() : 0;
  lCapacity = (lCapacity * 1024) / CRAMdisk::BANK_SIZE;
  if (!g_pRAMdisk->Attach(nUnit, sFileName, lCapacity)) return false;
  CMDOUTF("RAM disk unit %d loaded from %s capacity %dK", nUnit,
   g_pRAMdisk->GetFileName(nUnit).c_str(), g_pRAMdisk->GetCapacity(nUnit) * (CRAMdisk::BANK_SIZE/1024));
  return true;
}

bool CUI::DoDetachIDE (CCmdParser &cmd)
{
  //++
  // Detach and remove the IDE disk drive ...
  //--
  assert(g_pIDEdisk != NULL);
  if (m_modUnit.IsPresent()) {
    uint8_t nUnit;
    if (!GetUnit(nUnit, CIDEdisk::NDRIVES)) return false;
    g_pIDEdisk->Detach(nUnit);
  } else
    g_pIDEdisk->DetachAll();
  return true;
}

bool CUI::DoDetachRAM (CCmdParser &cmd)
{
  //++
  // Detach and remove the RAM disk unit ...
  //--
  assert(g_pRAMdisk != NULL);
  if (m_modUnit.IsPresent()) {
    uint8_t nUnit;
    if (!GetUnit(nUnit, CRAMdisk::NDRIVES)) return false;
    g_pRAMdisk->Detach(nUnit);
  } else
    g_pRAMdisk->DetachAll();
  return true;
}



////////////////////////////////////////////////////////////////////////////////
/////////////////// RUN, STEP, CONTINUE and RESET COMMANDS /////////////////////
////////////////////////////////////////////////////////////////////////////////

CCPU::STOP_CODE CUI::RunSimulation (uint32_t nSteps)
{
  //++
  //   This procedure will run the simulation engine for the specified number
  // of instructions, or indefinitely if nSteps is zero.  The simulation will
  // end either when the step count is reached, or some error (e.g. illegal
  // opcode, illegal IOT, etc) occurs, or the user enters the break character
  // on the console.  When that happens we print an appropriate message and
  // then return control.  
  //--

  // Figure out the magic character used to break emulation.
  if (nSteps == 0) {
    CMDOUTF("[Simulation started.  Type CONTROL+%c to break.]", (g_pConsole->GetConsoleBreak()+'@'));
  }

  // Now run the simulation ...
  CCPU::STOP_CODE nStop = g_pCPU->Run(nSteps);
  if (nSteps == 0) CMDOUTS("");

  // Decode the reason we stopped ...
  switch (nStop) {
    case CCPU::STOP_ILLEGAL_IO:
      CMDERRF("illegal IOT at %05o", g_pCPU->GetLastPC());  break;
    case CCPU::STOP_ILLEGAL_OPCODE:
      CMDERRF("illegal opcode at %05o", g_pCPU->GetLastPC());  break;
    case CCPU::STOP_HALT:
      CMDERRF("halt at %05o", g_pCPU->GetLastPC());  break;
    case CCPU::STOP_ENDLESS_LOOP:
      CMDERRF("endless loop at %05o", g_pCPU->GetPC());  break;
    case CCPU::STOP_BREAKPOINT:
      CMDERRF("breakpoint at %05o", g_pCPU->GetPC());  break;
    case CCPU::STOP_BREAK:
      CMDERRF("break at %05o", g_pCPU->GetPC());  break;
    case CCPU::STOP_FINISHED:
    case CCPU::STOP_NONE:    break;
  }

  // And we're done!
  return nStop;
}

bool CUI::DoContinue (CCmdParser &cmd)
{
  //++
  //   This will continue running the simulation where ever we last left off.
  // The simulation will continue until it is interrupted by any one of a
  // number of conditions - illegal instruction, illegal opcode, breakpoint,
  // user break, halt, endless loop, etc.  Note that some of these conditions
  // are considered "errors" and will abort a command procedure, and some are
  // not errors and will not abort a script.
  //--
  CCPU::STOP_CODE nStop = RunSimulation();
  return    (nStop == CCPU::STOP_ILLEGAL_IO)
         || (nStop == CCPU::STOP_ILLEGAL_OPCODE)
         || (nStop == CCPU::STOP_ENDLESS_LOOP)
         || (nStop == CCPU::STOP_HALT);
}

bool CUI::DoRun (CCmdParser &cmd)
{
  //++
  //   The RUN command is essentially the same as CONTINUE, except that it
  // resets the CPU and all peripherals first.  If an argument is given to the
  // command, e.g. "RUN 200", then this is taken as a starting address and
  // will be deposited in the PC before we start.  The /SWITCHES=oooo modifier
  // may also be used to place an initial value in the switch register (very
  // handy for running DEC diagnostics!).
  //--
  DoReset(cmd);
  if (m_argRunAddress.IsPresent())
    g_pCPU->SetPC(m_argRunAddress.GetNumber());
  if (m_argSwitches.IsPresent())
    g_pCPU->SetRegister(C6120::REG_SR, m_argSwitches.GetNumber());
  return DoContinue(cmd);
}

bool CUI::DoStep (CCmdParser &cmd)
{
  //++
  //   The STEP command single steps thru one or more instructions.  It prints
  // out the disassembly of each instruction just before it is executed, and
  // then dumps the register contents just after the instruction is executed.
  //--
  unsigned nCount = 1;
  if (m_argStepCount.IsPresent()) nCount = m_argStepCount.GetNumber();
  assert(nCount > 0);
  while (nCount-- > 0) {
    address_t wPC = g_pCPU->GetPC();
    word_t    wOP = g_pCPU->GetCurrentInstruction();
    string sCode = Disassemble(wPC, wOP);
    CMDOUTF("%05o/ %04o\t%s", wPC, wOP, sCode.c_str());
    unsigned  nStop = RunSimulation(1);
    if (nStop != CCPU::STOP_FINISHED) return false;
    DoExamineAllRegisters(true);
  }
  return true;
}

bool CUI::DoReset (CCmdParser &cmd)
{
  //++
  //   Reset the CPU and all I/O devices!  Note that the PIC, MCR and RTC are
  // memory mapped devices and don't get cleared by CCPU::MasterClear()!
  //--
  assert(g_pCPU != NULL);
  g_pCPU->MasterClear();
  g_pMemoryMap->MasterClear();
  return true;
}



////////////////////////////////////////////////////////////////////////////////
///////////////////////////// BREAKPOINT COMMANDS //////////////////////////////
////////////////////////////////////////////////////////////////////////////////

bool CUI::DoSetBreakpoint (CCmdParser &cmd)
{
  //++
  //   The "SET BREAKPOINT ooooo" command will (what else??) set a breakpoint
  // at the specified address.  Note that there's no error message if you set
  // a breakpoint at an address that already has a breakpoint, and only one
  // breakpoint is actually set.
  //--
  address_t nBreak = m_argBreakpoint.GetNumber();
  CGenericMemory *pMemory = GetMemorySpace();
  if (!pMemory->IsValid(nBreak, nBreak)) {
    CMDERRF("breakpoint outside memory - %05o", nBreak);  return false;
  } else
    pMemory->SetBreak(nBreak);
  return true;
}

bool CUI::DoClearBreakpoint (CCmdParser &cmd)
{
  //++
  //   The "CLEAR BREAKPOINT [ooooo]" command will remove the breakpoint at
  // the specified address or, if no address is specified, it will remove all
  // breakpoints.  Breakpoints may be removed from either MAIN memory, PANEL
  // memory, or EPROM.  MAIN is the default if no qualifier is specified.
  // Note that there's no error message if you ask it to clear a breakpoint
  // that doesn't exist!
  //--
  if (m_argOptBreakpoint.IsPresent()) {
    CGenericMemory *pMemory = GetMemorySpace();
    pMemory->SetBreak(m_argOptBreakpoint.GetNumber(), false);
  } else {
    //   Here to remove all breakpoints.  If any of /MAIN, /PANEL or /EPROM was
    // specified then remove all breakpoints from that memory space only.  If
    // no modifier was specified, then remove all breakpoints from everywhere!
    if (!m_modEPROM.IsPresent() && !m_modPanel.IsPresent()) {
      g_pEPROM->ClearAllBreaks();
      g_pPanelMemory->ClearAllBreaks();
      g_pMainMemory->ClearAllBreaks();
    } else if (m_modEPROM.IsPresent() && !m_modEPROM.IsNegated())
      g_pEPROM->ClearAllBreaks();
    else if (m_modPanel.IsPresent() && !m_modPanel.IsNegated())
      g_pPanelMemory->ClearAllBreaks();
    else
      g_pMainMemory->ClearAllBreaks();
  }
  return true;
}

string CUI::ShowBreakpoints (const CGenericMemory *pMemory)
{
  //++
  // List all breakpoints in a particular memory space ...
  //--
  string sBreaks;  address_t nLoc = pMemory->Base()-1;
  while (pMemory->FindBreak(nLoc)) {
    sBreaks += sBreaks.empty() ? "Breakpoint(s) at " : ", ";
    char sz[16];  sprintf_s(sz, sizeof(sz), "%05o", nLoc);
    sBreaks += sz;
  }
  return (sBreaks.empty()) ? "none" : sBreaks;
}

bool CUI::DoShowBreakpoints (CCmdParser &cmd)
{
  //++
  // List all current breakpoints ...
  //--
  if (!m_modEPROM.IsPresent() && !m_modPanel.IsPresent()) {
    CMDOUTS("EPROM: " << ShowBreakpoints(g_pEPROM));
    CMDOUTS("PANEL: " << ShowBreakpoints(g_pPanelMemory));
    CMDOUTS("MAIN:  " << ShowBreakpoints(g_pMainMemory));
  } else if (m_modEPROM.IsPresent() && !m_modEPROM.IsNegated()) {
    CMDOUTS("EPROM: " << ShowBreakpoints(g_pEPROM));
  } else if (m_modPanel.IsPresent() && !m_modPanel.IsNegated()) {
    CMDOUTS("PANEL: " << ShowBreakpoints(g_pPanelMemory));
  } else {
    CMDOUTS("MAIN: " << ShowBreakpoints(g_pMainMemory));
  }
  return true;
}



////////////////////////////////////////////////////////////////////////////////
///////////////////////////////// CPU COMMANDS /////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

bool CUI::DoClearCPU (CCmdParser &cmd)
{
  //++
  //   Clear (reset!) the CPU ONLY.  Note that any peripheral devices are NOT
  // affected, which may or may not be what you want.  Use the RESET command
  // to clear everything...
  //--
  g_pCPU->ClearCPU();
  return true;
}

bool CUI::DoSetCPU (CCmdParser &cmd)
{
  //++
  //   SET CPU allows you to set the CPU startup mode, whether the HLT opcode
  // really halts or traps to panel memory, whether to stop or igonore illegal
  // opcodes and IOTs, etc ...
  //--
  assert(g_pCPU != NULL);
  if (m_modIllegalIO.IsPresent())
    g_pCPU->StopOnIllegalIO(m_argStopIO.GetKeyValue() != 0);
  if (m_modIllegalOpcode.IsPresent())
    g_pCPU->StopOnIllegalOpcode(m_argStopOpcode.GetKeyValue() != 0);
  if (m_modBreakChar.IsPresent())
    g_pConsole->SetConsoleBreak(m_argBreakChar.GetNumber());
  if (m_modHaltOpcode.IsPresent())
    g_pCPU->SetStopOnHalt(!m_modHaltOpcode.IsNegated());
  if (m_modStartupMode.IsPresent())
    g_pCPU->SetStartupMode((C6120::STARTUP_MODE) m_argStartupMode.GetKeyValue());
  return true;
}

bool CUI::DoShowCPU (CCmdParser &cmd)
{
  //++
  //   The SHOW CPU command displays the CPU name, clock frequency, startup
  // mode, break character and other options.  After that, we also display the
  // internal CPU registers, and the state of the interrupt system too.
  //--
  CMDOUTS("");
  // Show general CPU information ...
  double flCrystal = g_pCPU->GetCrystalFrequency() / 1000000.0;
  CMDOUTF("%s %s %3.2fMHz, BREAK is Control-%c, STARTUP is %s",
    g_pCPU->GetName(), g_pCPU->GetDescription(), flCrystal, g_pConsole->GetConsoleBreak()+'@',
    g_pCPU->GetStartupMode() == C6120::STARTUP_MAIN ? "MAIN" : "PANEL");
  CMDOUTF("%s on illegal opcode, %s on illegal IOT, %s on HLT opcode",
    g_pCPU->IsStopOnIllegalOpcode() ? "STOP" : "CONTINUE",
    g_pCPU->IsStopOnIllegalIO() ? "STOP" : "CONTINUE",
    g_pCPU->IsStopOnHalt() ? "STOP" : "TRAP");

  // Show simulated CPU time ...
  uint64_t llTime = NSTOMS(g_pCPU->ElapsedTime());
  uint32_t lMilliseconds = llTime % 1000ULL;  llTime /= 1000ULL;
  uint32_t lSeconds      = llTime % 60ULL;    llTime /= 60ULL;
  uint32_t lMinutes      = llTime % 60ULL;    llTime /= 60ULL;
  uint32_t lHours        = llTime % 24ULL;    llTime /= 24ULL;
  uint32_t lDays         = (uint32_t) llTime;
  CMDOUTF("Simulated CPU time %ldd %02ld:%02ld:%02ld.%03ld\n",
    lDays, lHours, lMinutes, lSeconds, lMilliseconds);

  // Show CPU registers ...
  CMDOUTS("REGISTERS");
  DoExamineAllRegisters(false);
  word_t wPS = g_pCPU->GetRegister(C6120::REG_PS);
  word_t wFF = g_pCPU->GetRegister(C6120::REG_FLAGS);
  if (wPS != 0) CMDOUTS("Status: " << g_pCPU->DecodeStatus(wPS));
  if (wFF != 0) CMDOUTS("Flags:  " << g_pCPU->DecodeFlags(wFF));

  // Show interrupt status ...
  CMDOUTS(std::endl << "INTERRUPTS");
  CMDOUTF("Panel: %s", g_pCPU->IsCPREQ() ? "REQUESTED" : "not requested");
  CMDOUTF("Main : %s, %s", 
    ISSET(wPS, C6120::PS_IEFF) ? "ENABLED" : "not enabled",
    g_pCPU->IsIRQ() ? "REQUESTED" : "not requested");

  // That's about all we know!
  CMDOUTS("");
  return true;
}



////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// MEMORY COMMANDS ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

bool CUI::DoClearMemory (CCmdParser &cmd)
{
  //++
  //   The "CLEAR MEMORY" command clears memory, and the /EPROM, /MAIN and
  // /PANEL qualifiers may be used to specify a particular memory space.  If
  // no qualifier given, then all three memory spaces are cleared.
  //--
  if (!m_modEPROM.IsPresent() && !m_modPanel.IsPresent()) {
    g_pEPROM->ClearMemory();
    g_pPanelMemory->ClearMemory();
    g_pMainMemory->ClearMemory();
  }
  else if (m_modEPROM.IsPresent() && !m_modEPROM.IsNegated())
    g_pEPROM->ClearMemory();
  else if (m_modPanel.IsPresent() && !m_modPanel.IsNegated())
    g_pPanelMemory->ClearMemory();
  else
    g_pMainMemory->ClearMemory();
  return true;
}

bool CUI::DoShowMemory(CCmdParser &cmd)
{
  //++
  //   The SHOW MEMORY command will print a memory map of the SBC6120.  It's
  // not terribly useful since the information is well known in advance, but
  // it does have the advantage of reporting the status of all the RAM disk
  // chips ...
  //--
  assert((g_pMainMemory != NULL) && (g_pPanelMemory != NULL));
  assert((g_pEPROM != NULL) && (g_pRAMdisk != NULL));
  CMDOUTS("");
  CMDOUTF("Main  memory: %dKW", g_pMainMemory->Size()/1024);
  CMDOUTF("Panel memory: %dKW", g_pPanelMemory->Size()/1024);
  CMDOUTF("EPROM memory: %dKW", g_pEPROM->Size()/1024);
  ostringstream ofs;
  g_pRAMdisk->ShowStatus(ofs);
  CMDOUT(ofs);
  CMDOUTS("");
  return true;
}



////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// DEVICE COMMANDS ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

CDevice *CUI::FindDevice (const string sDevice)
{
  //++
  //   Search for the device named by m_argOptDeviceName and return a pointer
  // to its CDevice object.  If no match is found, print an error message and
  // return NULL ...
  // 
  //   On the SBC6120 the MCR, PIC and RTC are all "special" because they are
  // memory mapped rather than I/O mapped.  That means CCPU::FindDevice() can't
  // find them, so we have to make a special check just for those devices.
  //--
  CDevice *pDevice = g_pCPU->FindDevice(sDevice);
  if (pDevice != NULL) return pDevice;
  //if (CCmdArgKeyword::Match(sDevice, g_pPIC->GetName())) return g_pPIC;
  CMDERRS("No such device as " << sDevice);
  return NULL;
}

void CUI::ShowOneDevice (const CDevice *pDevice, bool fHeading)
{
  //++
  // Show the common device options (description, ports, type) to a string.
  //--
  string sLine;
  
  sLine = FormatString("%-8s  %-7s  %-25s  ",
    pDevice->GetName(), pDevice->GetType(), pDevice->GetDescription());

  if (pDevice->GetPortCount() <= 1) {
    sLine += FormatString(" 6%02ox       ", pDevice->GetBasePort());
  } else if (pDevice->GetPortCount() == 2) {
    sLine += FormatString(" 6%02ox, 6%02ox     ", pDevice->GetBasePort(), pDevice->GetBasePort()+1);
  } else {
    sLine += FormatString(" 6%02o0..6%02o7 ",
        pDevice->GetBasePort(), pDevice->GetBasePort() + pDevice->GetPortCount()-1);
  }

  int16_t nSense = g_pCPU->FindSense(pDevice);
  sLine += FormatString(" %-3s", (nSense < 0) ? "" : g_pCPU->GetSenseName(nSense));
  //sLine += FormatString(" %-3s", (pDevice != g_pSLU0) ? "" : g_pCPU->GetSenseName(EF_SLU0_IRQ));

  if (fHeading) {
    CMDOUTS("DEVICE    TYPE     DESCRIPTION                 IOT");
    CMDOUTS("--------  -------  --------------------------  -----------");
  }
  CMDOUTS(sLine);
}

bool CUI::ShowAllDevices()
{
  //++
  //   Show a table of all devices in the system.  Unfortunately, there's
  // no simple way to enumerate all devices, since some are memory mapped,
  // some are I/O mapped, some are input only, some are output only, or
  // both.  This is pretty kludgey, but it gets the job done...
  //--
  CMDOUTS("");
  ShowOneDevice(g_pSLU, true);
  ShowOneDevice(g_pMemoryMap);
  ShowOneDevice(g_pIOT641x);
  ShowOneDevice(g_pIOT643x);
  ShowOneDevice(g_pPOST);
  ShowOneDevice(g_pIDEdisk);
  CMDOUTS("");
  return true;
}

bool CUI::DoShowDevice (CCmdParser &cmd)
{
  //++
  //    This method is called for the "SHOW DEVICE name" command.  It attempts
  // to lookup the specified device and the print the details, including all
  // internal device state and registers, for that device.  Note that there
  // are no abbreviations for the device name - the command argument must 
  // match a device instance name exactly.
  //
  //    If no name is given, then it prints a brief summary of all IO devices.
  //--
  if (!m_argOptDeviceName.IsPresent()) return ShowAllDevices();
  CDevice *pDevice = FindDevice(m_argOptDeviceName.GetValue());
  if (pDevice == NULL) return false;
  CMDOUTS("");
  ShowOneDevice(pDevice, true);
  ostringstream ofs;
  pDevice->ShowDevice(ofs);
  CMDOUTS("");  CMDOUT(ofs);  CMDOUTS("");
  return true;
}

bool CUI::DoClearDevice (CCmdParser &cmd)
{
  //++
  // Clear (reset!) one or all I/O devices but NOT the CPU.
  //--
  if (!m_argOptDeviceName.IsPresent()) {
    g_pCPU->ClearAllDevices();
  } else {
    CDevice *pDevice = FindDevice(m_argOptDeviceName.GetValue());
    if (pDevice == NULL) return false;
    pDevice->ClearDevice();
  }
  return true;
}

bool CUI::DoSetDevice (CCmdParser &cmd)
{
  //++
  //   The SET DEVICE ... command can set various device parameters - delays
  // for the IDE drive, transmit and receive characters per second for the
  // serial ports, the value in the DIP switches, etc.  This code is not
  // very smart in that it silently ignores any options which don't apply
  // to the selected device.
  //--
  CDevice *pDevice = FindDevice(m_argDeviceName.GetValue());
  if (pDevice == NULL) return false;

  if (pDevice == g_pSLU) {
    if (m_modTxSpeed.IsPresent()) g_pSLU->SetTxSpeed(m_argTxSpeed.GetNumber());
    if (m_modRxSpeed.IsPresent()) g_pSLU->SetRxSpeed(m_argRxSpeed.GetNumber());
  } else if (pDevice == g_pIDEdisk) {
    if (m_modShortDelay.IsPresent()) g_pIDEdisk->SetShortDelay(USTONS(m_argShortDelay.GetNumber()));
    if (m_modLongDelay.IsPresent()) g_pIDEdisk->SetLongDelay(USTONS(m_argLongDelay.GetNumber()));
  }

  return true;
}


////////////////////////////////////////////////////////////////////////////////
/////////////////////////// MISCELLANEOUS COMMANDS /////////////////////////////
////////////////////////////////////////////////////////////////////////////////

bool CUI::DoShowVersion (CCmdParser &cmd)
{
  //++
  // Show just the version number ...
  //--
  CMDOUTF("\nSBC6120 Emulator v%d\n", SBCVER);
  return true;
}
