//++
// UserInterface.cpp -> SBC1802 Emulator Specific User Interface code
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
//   This module implements the user interface specific to the SBC1802 emulator
// process.  The first half of the file are parse tables for the generic command
// line parser classes from CommandParser.cpp, and the second half is the action
// routines needed to implement these commands.  
//
//                                                    Bob Armstrong [16-JUN-2022]
//
// SBC1802 COMMANDS
// ---------------
//   LO*AD filename             - load binary or .HEX file into RAM or ROM
//   SA*VE filename             - save RAM or ROM to a binary or .HEX file
//      /FORMAT=BINARY|INTEL    - set file format
//      /BAS*E=xxxx             - load/save relative to base address (octal)
//      /COU*NT=nnnnn           - number of bytes to save (decimal)
//      /RA*M                   - load/save data from RAM address space
//      /RO*M                   -   "   "     "    "  ROM    "      "
//      /OVER*WRITE             - don't prompt if file already exists (SAVE only!)
//
//   ATT*ACH DI*SK filename     - attach IDE drive to image file
//   DET*ACH DI*SK              - detach IDE drive
//      /UNIT=0|1               - 0 -> master, 1-> slave
//      /CAPACITY=nnnnn         - set image size, IN SECTORS!
//
//   ATT*ACH TA*PE filename     - attach TU58 drive to image file
//      /UNIT=0|1               - tape drive unit, 0 or 1
//      /CAPACITY=nnnnn         - set tape capacity, IN BLOCKS!
//      /READ                   - make tape unit read only
//      /WRITE                  - allow writing to this unit
// 
//   ATT*ACH PRI*NTER filename  - attach parallel port printer to text file
//      /[NO]WID*TH=nn          - set printer width for line wrap
//   DET*ACH PRI*NTER           - detach printer
//
//   DET*ACH TA*PE              - detach TU58 drive
//      /UNIT=0|1               - tape drive unit, 0 or 1
//
//   E*XAMINE xxxx              - display just address xxxx (hex)
//      xxxx-xxxx               - display all addresses in the range
//      xxxx, xxxx, ...         - display multiple addresses or ranges
//      R0..RF,D,DF,...         - display individual CPU register(s)
//      RE*GISTERS              - display all registers
//      /RA*M                   - display data from RAM address space
//      /RO*M                   -    "      "    "  ROM    "      "
//      /I*NSTRUCTION           - disassemble 1802 instructions
//   Registers - R0..RF, D, DF, P, X, I, N, T, IE, Q
//
//   D*EPOSIT xxxx xx           - deposit one byte
//      xxxx xx, xx, ...        - deposit several bytes
//      Rn xxxx                 - deposit in a register
//      /RA*M                   - deposit data in RAM address space ***
//      /RO*M                   -    "      "   " ROM    "      "   ***
//
//   SE*T BRE*AKPOINT xxxx      - set breakpoint at address (hex)
//   CL*EAR BRE*AKPOINT xxxx    - clear   "      "     "       "
//   SE*T BRE*AKPOINT xxxx-xxxx -  set breakpoint on address range
//   CL*EAR BRE*AKPOINT xxx-xxx - clear  "    "   "    "  "    "
//   CL*EAR BRE*AKPOINTS        - clear all breakpoints
//   SH*OW BRE*AKPOINTS         - show breakpoints
//      /RAM                    - set/clear/show breakpoints in RAM space
//      /ROM                    -  "    "     "    "    "    "  ROM   "
//
//   RU*N [xxxx]                - clear CPU and start running at PC=xxxx
//   C*ONTINUE                  - resume execution at current PC// 
//   ST*EP [nnnn]               - single step and trace nnnn instructions
//   RES*ET                     - reset CPU and all devices
//   INP*UT [xx]                - press INPUT/ATTENTION button
//                                (and optionally load xx into the switches)
//
//   SE*ND /TE*XT <file>        - send <file> as raw text
//      /NOCRLF                 - convert line endings to <CR> only
//      /CRLF                   - don't convert line endings
//      /DEL*AY=(line,char)     - set line and character delays, in milliseconds
//   SE*ND /TE*XT /CL*OSE       - abort any send text in progress
//
//   SE*ND /X*MODEM <file>      - send <file> using XMODEM protocol
//      /DEL*AY=delay           - set character delay, in milliseconds
//   SE*ND /X*MODEM /CL*OSE     - abort any XMODEM transfer in progress
//
//   RE*CEIVE/TE*XT <file>      - send emulation output to a raw text file
//      /APP*END                - append to existing file
//      /OVER*WRITE             - overwrite existing file
//   RE*CEIVE/TE*XT/CL*OSE      - stop logging emulation output
//
//   RE*CEIVE/X*MODEM <file>    - receive <file> using XMODEM protocol
//      /DEL*AY=delay           - set character delay, in milliseconds
//   RE*CEIVE/X*MODEM/CL*OSE    - abort any XMODEM transfer in progress
//
//   SH*OW MEM*ORY              - show memory map for all modes
//   CL*EAR MEM*ORY             - clear ALL of memory (RAM and ROM, not NVR!)
//      /RAM                    - clear RAM address space only ***
//      /ROM                    -   "   ROM    "      "     "  ***
//
//   SH*OW DEV*ICE name         - show details for device <name>
//   SH*OW DEV*ICES             - show list of all devices
//   CL*EAR DEV*ICE name        - reset just device <name>
//   CL*EAR DEV*ICES            - reset all I/O devices only
//   SE*T DEV*ICE name          - set device parameters
//      /TX*SPEED=nnnn          - set SLU transmit speed, in CPS
//      /RX*SPEED=nnnn          -  "   "  receive    "    "   "
//      /SPE*ED=nnn             - set printer speed in CPS
//      /SHO*RT=nnnn            - set IDE short delay, in microseconds
//      /LO*NG=nnnn             -  "   "  long    "    "    "    "
//      /SW*ITCHES=xx           - set toggle switches to xx
//      /ENABLE                 - enable TLIO, DISK, TAPE, RTC, PIC, PPI, CTC, or PSG1/2
//      /DISABLE                - disable  "     "     "    "    "    "    "        "
//
//   SH*OW CPU                  - show CPU details
//   CL*EAR CPU                 - reset the CPU only
//   SE*T CPU
//      /BRE*AK=nnn             - set break character to ASCII code nnn
//      /IO=STOP|IGNORE         - stop or ignore illegal I/O references
//      /OPCODE=STOP|IGNORE     -  "    "   "     "   "  opcodes
//      /[NO]EXTENDED           - enable 1804/5/6 extended instructions
//      /CLO*CK=nnnnnnnnn       - set CPU clock frequency (in Hz!)
//
//   SH*OW VER*SION             - show software version
//
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
// #ifdef THREADS
//   SET CHECK*POINT
//         /ENA*BLE             - enable file checkpointing
//         /DISA*BLE            - disable "      "    "
//         /INT*ERVAL=nn        - set checkpointing interval in seconds
//   SHOW CHECK*POINT           - show current checkpoint settings
// #endif
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
// 23-DEC-23  RLA   All second UART and TU58.
// 17-MAY-24  RLA   ShowSense EF numbers are off by 1
// 30-OCT-24  RLA   Add two PSGs
// 31-OCT-24  RLA   Add PPI
//  2-NOV-24  RLA   Add counter/timer
//  6-NOV-24  RLA   TLIO broke SHOW CPU interrupt channels
//                  Add SET CPU/CLOCK=nnnnnnnnnn
//  7-NOV-24  RLA   Change /ENABLE and /DISABLE to ENABLED and DISABLED
//                  Don't show inaccessible devices if TLIO is disabled
//                  Report TLIO as port 1 only (rather than all ports)
//  5-MAR-24  RLA   Add SET DISK and SET TAPE /ENABLE or /DISABLE
//                  Change "SHOW DEVICE TU58" to "SHOW DEVICE TAPE"
// 25-MAR-25  RLA   Add SET DEVICE xxx/ENABLE or /DISABLE for RTC, PIC,
//                  PPI, CTC, and PSG1 & 2
// 28-MAR-25  RLA   Add ATTACH PRINTER, DETACH PRINTER and SET DEVICE PRINTER.
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
#include "SmartConsole.hpp"     // console window file transfer functions
#include "CommandParser.hpp"    // emulator library command line parsing methods
#include "CommandLine.hpp"      // Shell (argc/argv) parser methods
#include "StandardUI.hpp"       // emulator library standard UI commands
#include "SBC1802.hpp"          // global declarations for this project
#include "MemoryTypes.h"        // address_t and word_t data types
#include "EventQueue.hpp"       // I/O device event queue simulation
#include "Memory.hpp"           // emulated memory functions
#include "Device.hpp"           // CDevice I/O device emulation objects
#include "DeviceMap.hpp"        // CDeviceMap memory mapped I/O emulation
#include "COSMACopcodes.hpp"    // one line assembler and disassembler
#include "Interrupt.hpp"        // interrupt simulation logic
#include "CPU.hpp"              // CCPU base class definitions
#include "COSMAC.hpp"           // COSMAC 1802 CPU emulation
#include "IDE.hpp"              // standard IDE disk interface
#include "TU58.hpp"             // TU58 emulator
#include "MemoryMap.hpp"        // SBC1802 memory mapping hardware
#include "TLIO.hpp"             // RCA style two level I/O
#include "POST.hpp"             // SBC1802 7 segment display and DIP switches
#include "Baud.hpp"             // COM8136 baud rate generator
#include "CDP1854.hpp"          // CDP1854 UART definitions (used for SLU0 and 1)
#include "CDP1877.hpp"          // CDP1877 programmable interrupt controller
#include "CDP1879.hpp"          // CDP1879 real time clock
#include "PSG.hpp"              // AY-3-8912 programmable sound generator
#include "TwoPSGs.hpp"          // special hacks for two PSGs in the SBC1802
#include "PPI.hpp"              // generic programmable I/O definitions
#include "Printer.hpp"          // SBC1802 parallel printer port implementation
#include "CDP1851.hpp"          // CDP1851 programmable I/O interface
#include "Timer.hpp"            // generic counter/timer
#include "CDP1878.hpp"          // CDP1878 specific counter/timer
#include "ElfDisk.hpp"          // SBC1802/ELF2K to IDE interface
#include "UserInterface.hpp"    // emulator user interface parse table definitions

// LOAD/SAVE file format keywords ...
const CCmdArgKeyword::KEYWORD CUI::m_keysFileFormat[] = {
  {"BIN*ARY", CUI::FILE_FORMAT_BINARY},
  {"IN*TEL",  CUI::FILE_FORMAT_INTEL},
  {NULL, 0}
};

// STOP or IGNORE options for "SET CPU" ...
const CCmdArgKeyword::KEYWORD CUI::m_keysStopIgnore[] = {
  {"ST*OP", true}, {"IGN*ORE", false}, {NULL, 0}
};

// Argument definitions ...
CCmdArgFileName    CUI::m_argFileName("file name");
CCmdArgFileName    CUI::m_argOptFileName("file name", true);
CCmdArgKeyword     CUI::m_argFileFormat("format", m_keysFileFormat);
CCmdArgNumber      CUI::m_argBaseAddress("starting address", 16, 0, ADDRESS_MAX);
CCmdArgNumber      CUI::m_argByteCount("byte count", 10, 0, ADDRESS_MAX);
CCmdArgNumberRange CUI::m_argAddressRange("address range", 16, 0, ADDRESS_MAX);
CCmdArgName        CUI::m_argRegisterName("register name");
CCmdArgRangeOrName CUI::m_argExamineDeposit("name or range", 16, 0, ADDRESS_MAX);
CCmdArgList        CUI::m_argRangeOrNameList("name or range list", m_argExamineDeposit);
CCmdArgList        CUI::m_argRangeList("address range list", m_argAddressRange);
CCmdArgNumber      CUI::m_argData("data", 16, 0, ADDRESS_MAX);
CCmdArgList        CUI::m_argDataList("data list", m_argData);
CCmdArgNumber      CUI::m_argStepCount("step count", 10, 1, INT16_MAX, true);
CCmdArgNumber      CUI::m_argRunAddress("start address", 16, 0, ADDRESS_MAX, true);
CCmdArgNumberRange CUI::m_argBreakpoint("breakpoint address", 16, 0, ADDRESS_MAX);
CCmdArgNumberRange CUI::m_argOptBreakpoint("breakpoint address", 16, 0, ADDRESS_MAX, true);
CCmdArgNumber      CUI::m_argBreakChar("break character", 10, 1, 31);
CCmdArgKeyword     CUI::m_argStopIO("stop on illegal I/O", m_keysStopIgnore);
CCmdArgKeyword     CUI::m_argStopOpcode("stop on illegal opcode", m_keysStopIgnore);
CCmdArgNumber      CUI::m_argSwitches("switches", 16, 0, 255);
CCmdArgNumber      CUI::m_argOptSwitches("switches", 16, 0, 255, true);
CCmdArgNumber      CUI::m_argTxSpeed("TX speed (cps)", 10, 1, 100000UL);
CCmdArgNumber      CUI::m_argRxSpeed("RX speed (cps)", 10, 1, 100000UL);
CCmdArgNumber      CUI::m_argSpeed("speed (cps)", 10, 1, 100000UL);
CCmdArgNumber      CUI::m_argShortDelay("short delay (us)", 10, 1, 1000000UL);
CCmdArgNumber      CUI::m_argLongDelay("long delay (us)", 10, 1, 1000000UL);
CCmdArgName        CUI::m_argOptDeviceName("device", true);
CCmdArgName        CUI::m_argDeviceName("device", false);
CCmdArgNumber      CUI::m_argUnit("unit", 10, 0, 255);
CCmdArgNumber      CUI::m_argCapacity("capacity", 10, 1, UINT32_MAX);
CCmdArgNumber      CUI::m_argDelay("delay (ms)", 10, 1, 1000000UL);
CCmdArgList        CUI::m_argDelayList("delay list", m_argDelay, true);
CCmdArgNumber      CUI::m_argFrequency("frequency", 10, 1, UINT32_MAX);
CCmdArgNumber      CUI::m_argOptWidth("line width", 10, 1, UINT32_MAX, true);

// Modifier definitions ...
CCmdModifier CUI::m_modFileFormat("FORM*AT", NULL, &m_argFileFormat);
CCmdModifier CUI::m_modInstruction("I*NSTRUCTION");
CCmdModifier CUI::m_modBreakChar("BRE*AK", NULL, &m_argBreakChar);
CCmdModifier CUI::m_modIllegalIO("IO", NULL, &m_argStopIO);
CCmdModifier CUI::m_modIllegalOpcode("OP*CODE", NULL, &m_argStopOpcode);
CCmdModifier CUI::m_modCPUextended("EXT*ENDED", "NOEXT*ENDED");
CCmdModifier CUI::m_modClockFrequency("CLO*CK", NULL, &m_argFrequency);
CCmdModifier CUI::m_modBaseAddress("BAS*E", NULL, &m_argBaseAddress);
CCmdModifier CUI::m_modByteCount("COU*NT", NULL, &m_argByteCount);
CCmdModifier CUI::m_modROM("ROM", "RAM");
CCmdModifier CUI::m_modTxSpeed("TX*SPEED", NULL, &m_argTxSpeed);
CCmdModifier CUI::m_modRxSpeed("RX*SPEED", NULL, &m_argRxSpeed);
CCmdModifier CUI::m_modSpeed("SPE*ED", NULL, &m_argSpeed);
CCmdModifier CUI::m_modShortDelay("SHO*RT", NULL, &m_argShortDelay);
CCmdModifier CUI::m_modLongDelay("LO*NG", NULL, &m_argLongDelay);
CCmdModifier CUI::m_modUnit("UN*IT", NULL, &m_argUnit);
CCmdModifier CUI::m_modCapacity("CAP*ACITY", NULL, &m_argCapacity);
CCmdModifier CUI::m_modReadOnly("RE*AD", "WR*ITE");
CCmdModifier CUI::m_modSwitches("SW*ITCHES", NULL, &m_argSwitches);
CCmdModifier CUI::m_modOverwrite("OVER*WRITE", "NOOVER*WRITE");
CCmdModifier CUI::m_modClose("CL*OSE", NULL);
CCmdModifier CUI::m_modText("TE*XT", NULL);
CCmdModifier CUI::m_modXModem("X*MODEM", NULL);
CCmdModifier CUI::m_modAppend("APP*END", "OVER*WRITE");
CCmdModifier CUI::m_modCRLF("CRLF", "NOCRLF");
CCmdModifier CUI::m_modWidth("WID*TH", "NOWID*TH", &m_argOptWidth);
CCmdModifier CUI::m_modDelayList("DEL*AY", NULL, &m_argDelayList);
CCmdModifier CUI::m_modEnable("ENA*BLED", "DISA*BLED");

// LOAD and SAVE commands ...
CCmdArgument * const CUI::m_argsLoadSave[] = {&m_argFileName, NULL};
CCmdModifier * const CUI::m_modsLoad[] = {&m_modFileFormat, &m_modBaseAddress,
                                          &m_modByteCount, &m_modROM, NULL};
CCmdModifier * const CUI::m_modsSave[] = {&m_modFileFormat, &m_modBaseAddress,
                                          &m_modByteCount, &m_modROM, 
                                          &m_modOverwrite, NULL};
CCmdVerb CUI::m_cmdLoad("LO*AD", &DoLoad, m_argsLoadSave, m_modsLoad);
CCmdVerb CUI::m_cmdSave("SA*VE", &DoSave, m_argsLoadSave, m_modsSave);

// ATTACH and DETACH commands ...
CCmdArgument * const CUI::m_argsAttach[] = {&m_argFileName, NULL};
CCmdModifier * const CUI::m_modsDetach[] = {&m_modUnit, NULL};
CCmdModifier * const CUI::m_modsAttachDisk[] = {&m_modCapacity, &m_modUnit, NULL};
CCmdModifier * const CUI::m_modsAttachTape[] = {&m_modReadOnly, &m_modUnit,
                                                &m_modCapacity, NULL};
CCmdModifier * const CUI::m_modsDetachTape[] = {&m_modUnit, NULL};
CCmdModifier * const CUI::m_modsAttachPrinter[] = {&m_modWidth, NULL};
CCmdVerb CUI::m_cmdAttachDisk("DI*SK", &DoAttachDisk, m_argsAttach, m_modsAttachDisk);
CCmdVerb CUI::m_cmdDetachDisk("DI*SK", &DoDetachDisk, NULL, m_modsDetach);
CCmdVerb CUI::m_cmdAttachTape("TA*PE", &DoAttachTape, m_argsAttach, m_modsAttachTape);
CCmdVerb CUI::m_cmdDetachTape("TA*PE", &DoDetachTape, NULL, m_modsDetachTape);
CCmdVerb CUI::m_cmdAttachPrinter("PRI*NTER", &DoAttachPrinter, m_argsAttach, m_modsAttachPrinter);
CCmdVerb CUI::m_cmdDetachPrinter("PRI*NTER", &DoDetachPrinter, NULL, NULL);
CCmdVerb * const CUI::g_aAttachVerbs[] = {
  &m_cmdAttachDisk, &m_cmdAttachTape, &m_cmdAttachPrinter, NULL
};
CCmdVerb * const CUI::g_aDetachVerbs[] = {
  &m_cmdDetachDisk, &m_cmdDetachTape, &m_cmdDetachPrinter, NULL
};
CCmdVerb CUI::m_cmdAttach("ATT*ACH", NULL, NULL, NULL, g_aAttachVerbs);
CCmdVerb CUI::m_cmdDetach("DET*ACH", NULL, NULL, NULL, g_aDetachVerbs);

// EXAMINE and DEPOSIT verb definitions ...
CCmdArgument * const CUI::m_argsExamine[] = {&m_argRangeOrNameList, NULL};
CCmdArgument * const CUI::m_argsDeposit[] = {&m_argExamineDeposit, &m_argDataList, NULL};
CCmdModifier * const CUI::m_modsExamine[] = {&m_modInstruction, &m_modROM, NULL};
CCmdModifier * const CUI::m_modsDeposit[] = {&m_modROM, NULL};
CCmdVerb CUI::m_cmdDeposit("D*EPOSIT", &DoDeposit, m_argsDeposit, m_modsDeposit);
CCmdVerb CUI::m_cmdExamine("E*XAMINE", &DoExamine, m_argsExamine, m_modsExamine);

// SET, CLEAR and SHOW BREAKPOINT commands ...
CCmdModifier * const CUI::m_modsRAMROM[] = {&m_modROM, NULL};
CCmdArgument * const CUI::m_argsSetBreakpoint[] = {&m_argBreakpoint, NULL};
CCmdArgument * const CUI::m_argsClearBreakpoint[] = {&m_argOptBreakpoint, NULL};
CCmdVerb CUI::m_cmdSetBreakpoint("BRE*AKPOINT", &DoSetBreakpoint, m_argsSetBreakpoint, m_modsRAMROM);
CCmdVerb CUI::m_cmdClearBreakpoint("BRE*AKPOINT", &DoClearBreakpoint, m_argsClearBreakpoint, m_modsRAMROM);
CCmdVerb CUI::m_cmdShowBreakpoint("BRE*AKPOINT", &DoShowBreakpoints, NULL, m_modsRAMROM);

// RUN, CONTINUE, STEP, RESET and INPUT commands ...
CCmdArgument * const CUI::m_argsStep[] = {&m_argStepCount, NULL};
CCmdArgument * const CUI::m_argsRun[] = {&m_argRunAddress, NULL};
CCmdArgument* const CUI::m_argsInput[] = { &m_argOptSwitches, NULL };
CCmdVerb CUI::m_cmdRun("RU*N", &DoRun, m_argsRun, NULL);
CCmdVerb CUI::m_cmdContinue("C*ONTINUE", &DoContinue);
CCmdVerb CUI::m_cmdStep("ST*EP", &DoStep, m_argsStep, NULL);
CCmdVerb CUI::m_cmdReset("RE*SET", &DoReset);
CCmdVerb CUI::m_cmdInput("IN*PUT", &DoInput, m_argsInput, NULL);

// SET, CLEAR and SHOW CPU ...
CCmdModifier * const CUI::m_modsSetCPU[] = {
    &m_modCPUextended, &m_modIllegalIO, &m_modIllegalOpcode,
    &m_modBreakChar, &m_modClockFrequency, NULL
  };
CCmdVerb CUI::m_cmdSetCPU = {"CPU", &DoSetCPU, NULL, m_modsSetCPU};
CCmdVerb CUI::m_cmdClearCPU("CPU", &DoClearCPU);
CCmdVerb CUI::m_cmdShowCPU("CPU", &DoShowCPU);

// CLEAR and SHOW MEMORY ...
CCmdVerb CUI::m_cmdClearMemory("MEM*ORY", &DoClearMemory, NULL, m_modsRAMROM);
CCmdVerb CUI::m_cmdShowMemory("MEM*ORY", &DoShowMemory);

// CLEAR and SHOW DEVICE ...
CCmdArgument * const CUI::m_argsShowDevice[] = {&m_argOptDeviceName, NULL};
CCmdArgument * const CUI::m_argsSetDevice[] = {&m_argDeviceName, NULL};
CCmdModifier * const CUI::m_modsSetDevice[] = {
    &m_modTxSpeed, &m_modRxSpeed, &m_modSpeed, &m_modShortDelay,
    &m_modLongDelay, &m_modSwitches, &m_modEnable,
    &m_modWidth, NULL
  };
CCmdVerb CUI::m_cmdShowDevice("DEV*ICES", &DoShowDevice, m_argsShowDevice, NULL);
CCmdVerb CUI::m_cmdSetDevice("DEV*ICE", &DoSetDevice, m_argsSetDevice, m_modsSetDevice);
CCmdVerb CUI::m_cmdClearDevice("DEV*ICES", &DoClearDevice, m_argsShowDevice, NULL);

// CLEAR verb definition ...
CCmdVerb * const CUI::g_aClearVerbs[] = {
    &m_cmdClearBreakpoint, &m_cmdClearCPU, &m_cmdClearMemory, &m_cmdClearDevice, NULL
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
    &m_cmdShowBreakpoint, &m_cmdShowMemory, &m_cmdShowCPU, &m_cmdShowDevice,
    &m_cmdShowVersion,
    &CStandardUI::m_cmdShowLog, &CStandardUI::m_cmdShowAliases,
#ifdef THREADS
    &CStandardUI::m_cmdShowCheckpoint,
#endif
    NULL
  };
CCmdVerb CUI::m_cmdShow("SH*OW", NULL, NULL, NULL, g_aShowVerbs);

// SEND and RECEIVE commands ...
CCmdArgument * const CUI::m_argsSendFile[] = {&m_argOptFileName, NULL};
CCmdArgument * const CUI::m_argsReceiveFile[] = {&m_argOptFileName, NULL};
CCmdModifier * const CUI::m_modsSendFile[] = {&m_modClose, &m_modText, &m_modXModem, &m_modCRLF, &m_modDelayList, NULL};
CCmdModifier * const CUI::m_modsReceiveFile[] = {&m_modClose, &m_modText, &m_modXModem, &m_modAppend, &m_modDelayList, NULL};
CCmdVerb CUI::m_cmdSendFile = {"SE*ND", &DoSendFile, m_argsSendFile, m_modsSendFile};
CCmdVerb CUI::m_cmdReceiveFile = {"RE*CEIVE", &DoReceiveFile, m_argsReceiveFile, m_modsReceiveFile};

// Master list of all verbs ...
CCmdVerb * const CUI::g_aVerbs[] = {
  &m_cmdLoad, &m_cmdSave, &m_cmdAttach, &m_cmdDetach,
  &m_cmdExamine, &m_cmdDeposit,
  &m_cmdSendFile, &m_cmdReceiveFile, 
  &m_cmdRun, &m_cmdContinue, &m_cmdStep, &m_cmdReset,
  &m_cmdInput, &m_cmdSet, &m_cmdShow, &m_cmdClear,
  &CStandardUI::m_cmdDefine, &CStandardUI::m_cmdUndefine,
  &CStandardUI::m_cmdIndirect, &CStandardUI::m_cmdExit,
  &CStandardUI::m_cmdQuit, &CCmdParser::g_cmdHelp,
  NULL
};

////////////////////////////////////////////////////////////////////////////////
//////////////////////////// LOAD AND SAVE COMMANDS ////////////////////////////
////////////////////////////////////////////////////////////////////////////////

CGenericMemory *CUI::GetMemorySpace()
{
  //++
  //   Figure out which memory space is required - /ROM selects
  // the EPROM space, and /RAM (or no switch at all) selects RAM.
  //--
  if (m_modROM.IsPresent() && !m_modROM.IsNegated()) {
    assert(g_pROM != NULL);  return g_pROM;
  } else {
    assert(g_pRAM != NULL);  return g_pRAM;
  }
}

void CUI::GetImageFileNameAndFormat (bool fCreate, string &sFileName, int &nFormat)
{
  //++
  //   This method will get the memory image file name and format for the LOAD
  // and SAVE commands.  Two file types are supported - Intel hex and straight
  // binary - and the algorithm we use to figure out the type is fairly
  // complicated but really useful.  First, if the /FORMAT=BINARY or /FORMAT=
  // INTEL modifier is used, then that always takes precedence.  If /FORMAT was
  // specified and the file name given doesn't have an extension then we'll
  // supply the an appriate default.
  //
  //   If the /FORMAT modifier isn't specified but the filename does have an
  // explicit extension, either ".hex" or ".bin", then that determines the file
  // format.  And in this case of course no default extension is needed.
  //
  //   And lastly, if there was no /FORMAT and no extension specified (e.g.
  // "LOAD ROM FOO") then we'll try to figure out the type by first looking for
  // a "FOO.HEX" and then a "FOO.BIN".  If one of those exists then we'll go
  // with that one, and if neither exists then it's an error.
  //--
  sFileName = m_argFileName.GetFullPath();
  nFormat = FILE_FORMAT_NONE;

  // Try to figure out the extension and format ...
  if (m_modFileFormat.IsPresent()) {
    // /FORMAT was specified!
    nFormat = (int) m_argFileFormat.GetKeyValue();
    if (nFormat == FILE_FORMAT_BINARY)
      sFileName = CCmdParser::SetDefaultExtension(sFileName, DEFAULT_BINARY_FILE_TYPE);
    else
      sFileName = CCmdParser::SetDefaultExtension(sFileName, DEFAULT_INTEL_FILE_TYPE);
  } else {
    string sDrive, sDir, sName, sExt;
    SplitPath(sFileName, sDrive, sDir, sName, sExt);
    if (sExt.empty() && !fCreate) {
      // No extension given - try searching for .hex or .bin ...
      if (FileExists(MakePath(sDrive, sDir, sName, DEFAULT_BINARY_FILE_TYPE))) {
        sFileName = MakePath(sDrive, sDir, sName, DEFAULT_BINARY_FILE_TYPE);
        nFormat = FILE_FORMAT_BINARY;
      } else if (FileExists(MakePath(sDrive, sDir, sName, DEFAULT_INTEL_FILE_TYPE))) {
        sFileName = MakePath(sDrive, sDir, sName, DEFAULT_INTEL_FILE_TYPE);
        nFormat = FILE_FORMAT_INTEL;
      }
    } else if (sExt == DEFAULT_BINARY_FILE_TYPE) {
      nFormat = FILE_FORMAT_BINARY;
    } else if (sExt == DEFAULT_INTEL_FILE_TYPE) {
      nFormat = FILE_FORMAT_INTEL;
    }
  }

  // If we still don't know the format then assume binary ...
  if (nFormat == FILE_FORMAT_NONE) {
    nFormat = FILE_FORMAT_BINARY;
    CMDERRS("BINARY format assumed for " << sFileName);
  }
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
  if (m_modByteCount.IsPresent())
    cbBytes = m_argByteCount.GetNumber();
  else
    cbBytes = ADDRESS_MAX-wBase+1;
}

bool CUI::DoLoad (CCmdParser &cmd)
{
  //++
  //   The LOAD command loads memory from a disk file in Intel HEX format or 
  // plain binary.  Note that in the SBC1802 all the RAM is battery backed up,
  // and there is no separate NVR chip (only the CDP1879 RTC) so there is no
  // LOAD/NVR command!
  //--
  string sFileName;  int nFormat;  int32_t nBytes = 0;
  GetImageFileNameAndFormat(false, sFileName, nFormat);
  CGenericMemory *pMemory = GetMemorySpace();

  // Get the address range to be loaded ...
  address_t wBase = 0;  size_t cbLimit = 0;
  GetImageBaseAndOffset(wBase, cbLimit);
  if (cbLimit > pMemory->Size()) cbLimit = pMemory->Size();
  switch (nFormat) {
    case FILE_FORMAT_BINARY:
       nBytes = pMemory->LoadBinary(sFileName, wBase, cbLimit);  break;
    case FILE_FORMAT_INTEL:
      nBytes = pMemory->LoadIntel(sFileName, wBase, cbLimit);  break;
  }

  // And we're done!
  if (nBytes < 0) return false;
  CMDOUTF("%d bytes loaded from %s", nBytes, sFileName.c_str());
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
  string sFileName;  int nFormat;  int32_t nBytes = 0;
  GetImageFileNameAndFormat(false, sFileName, nFormat);
  CGenericMemory *pMemory = GetMemorySpace();

  // Save RAM or ROM ...
  address_t wBase = 0;  size_t cbBytes = 0;
  GetImageBaseAndOffset(wBase, cbBytes);
  if (cbBytes > pMemory->Size()) cbBytes = pMemory->Size();
  if (!(m_modOverwrite.IsPresent() && !m_modOverwrite.IsNegated())) {
    if (FileExists(sFileName)) {
      if (!cmd.AreYouSure(sFileName + " already exists")) return false;
    }
  }
  switch (nFormat) {
    case FILE_FORMAT_BINARY:
      nBytes = pMemory->SaveBinary(sFileName, wBase, cbBytes);  break;
    case FILE_FORMAT_INTEL:
      nBytes = pMemory->SaveIntel(sFileName, wBase, cbBytes);  break;
  }

  // All done...
  if (nBytes < 0) return false;
  CMDOUTF("%d bytes saved to %s", nBytes, sFileName.c_str());
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

bool CUI::DoAttachDisk (CCmdParser &cmd)
{
  //++
  // Attach the IDE disk drive to an external image file...
  //--
  assert(g_pIDE != NULL);
  uint8_t nUnit;
  if (!GetUnit(nUnit, CIDE::NDRIVES)) return false;

  if (g_pIDE->IsAttached(nUnit)) {
    CMDERRS("IDE unit " << nUnit << " already attached to " << g_pIDE->GetFileName(nUnit));
    return false;
  }

  // The default image extension is ".dsk" ...
  string sFileName = m_argFileName.GetFullPath();
  if (!FileExists(sFileName)) {
    string sDrive, sDir, sName, sExt;
    SplitPath(sFileName, sDrive, sDir, sName, sExt);
    sFileName = MakePath(sDrive, sDir, sName, ".dsk");
  }

  // Attach the drive to the file, and we're done!
  uint32_t lCapacity = m_modCapacity.IsPresent() ? m_argCapacity.GetNumber() : 0;
  if (!g_pIDE->Attach(nUnit, sFileName, lCapacity)) return false;
  CMDOUTS("IDE unit " << nUnit << " attached to " << sFileName);
  return true;
}

bool CUI::DoDetachDisk (CCmdParser &cmd)
{
  //++
  // Detach and remove the IDE disk drive ...
  //--
  assert(g_pIDE != NULL);
  if (m_modUnit.IsPresent()) {
    uint8_t nUnit;
    if (!GetUnit(nUnit, CIDE::NDRIVES)) return false;
    g_pIDE->Detach(nUnit);
  } else
    g_pIDE->DetachAll();
  return true;
}

bool CUI::DoAttachTape (CCmdParser &cmd)
{
  //++
  //   Attach a TU58 unit to an external image file.  The TU58 supports
  // multiple units.  Each unit can be write locked and you can set the
  // capacity of each unit independently.
  // 
  //   Note that if the /UNIT= modifier is not specified, we default to unit
  // zero.
  //--
  assert(g_pTU58 != NULL);
  uint8_t nUnit;
  if (!GetUnit(nUnit, g_pTU58->GetUnits())) return false;
   
  // If this unit is already attached, then fail ...
  if (g_pTU58->IsAttached(nUnit)) {
    CMDERRS("TU58 unit " << nUnit << " already attached to " << g_pTU58->GetFileName(nUnit));
    return false;
  }

  // The default image extension is ".tu58" ...
  string sFileName = m_argFileName.GetFullPath();
  if (!FileExists(sFileName)) {
    string sDrive, sDir, sName, sExt;
    SplitPath(sFileName, sDrive, sDir, sName, sExt);
    sFileName = MakePath(sDrive, sDir, sName, ".tu58");
  }

  // Attach the drive to the file ...
  bool fReadOnly = m_modReadOnly.IsPresent() && !m_modReadOnly.IsNegated();
  uint32_t lCapacity = m_modCapacity.IsPresent() ? m_argCapacity.GetNumber() : 0;
  if (!g_pTU58->Attach(nUnit, sFileName, fReadOnly, lCapacity)) return false;
  CMDOUTS("TU58 unit " << nUnit << " attached to " << sFileName);
  return true;
}

bool CUI::DoDetachTape (CCmdParser &cmd)
{
  //++
  //   Detach a TU58 unit.  If the /UNIT modifier is specified, then detach
  // only that specific unit.  If no /UNIT is given, then detach ALL UNITS!
  //--
  assert(g_pTU58 != NULL);
  if (m_modUnit.IsPresent()) {
    uint8_t nUnit;
    if (!GetUnit(nUnit, g_pTU58->GetUnits())) return false;
    g_pTU58->Detach(nUnit);
  } else
    g_pTU58->DetachAll();
  return true;
}

bool CUI::DoAttachPrinter (CCmdParser &cmd)
{
  //++
  // Attach the printer emulation to a text file ...
  //--
  assert(g_pPPI != NULL);
  
  // Fail if the printer is already attached ...
  if (g_pPPI->IsAttached()) {
    CMDERRS("Printer unit already attached to " << g_pPPI->GetFileName());
    return false;
  }

  // The default extension here is .TXT!
  string sFileName = m_argFileName.GetFullPath();
  if (!FileExists(sFileName)) {
    string sDrive, sDir, sName, sExt;
    SplitPath(sFileName, sDrive, sDir, sName, sExt);
    sFileName = MakePath(sDrive, sDir, sName, ".txt");
  }

  // And set the options (if any) ...
  if (m_modWidth.IsPresent())
    g_pPPI->SetWidth(m_modWidth.IsNegated() ? 0 : m_argOptWidth.GetNumber());

  // Attach the printer to the file ...
  if (!g_pPPI->OpenFile(sFileName)) return false;
  CMDOUTS("Printer attached to " << sFileName);
  return true;
}

bool CUI::DoDetachPrinter (CCmdParser &cmd)
{
  //++
  // Detach the printer from a file ...
  //--
  assert(g_pPPI != NULL);
  g_pPPI->CloseFile();
  return true;
}

////////////////////////////////////////////////////////////////////////////////
///////////////////////// EXAMINE and DEPOSIT COMMANDS /////////////////////////
////////////////////////////////////////////////////////////////////////////////

void CUI::DumpLine (const CGenericMemory *pMemory, address_t nStart, size_t cbBytes, unsigned nIndent, unsigned nPad)
{
  //++
  //   Dump out one line of memory contents, byte by byte and always in
  // hexadecimal, for the EXAMINE command.  The line can optionally be
  // padded on the left (nIndent > 0) or the right (nPad > 0) so that
  // we can line up rows that don't start on a multiple of 16.
  //--
  char szLine[128];  unsigned i;
  sprintf_s(szLine, sizeof(szLine), "%04X/ ", (uint16_t) nStart);
  for (i = 0; i < nIndent; ++i) strcat_s(szLine, sizeof(szLine), "   ");
  for (i = 0; i < cbBytes; ++i) {
    char sz[16];
    sprintf_s(sz, sizeof(sz), "%02X ", pMemory->UIread(nStart+i));
    strcat_s(szLine, sizeof(szLine), sz);
  }
  for (i = 0; i < nPad; ++i) strcat_s(szLine, sizeof(szLine), "   ");
  strcat_s(szLine, sizeof(szLine), "\t");
  for (i = 0; i < nIndent; ++i) strcat_s(szLine, sizeof(szLine), " ");
  for (i = 0; i < cbBytes; ++i) {
    char sz[2];  uint8_t b = pMemory->UIread(nStart+i) & 0x7F;
    sz[0] = isprint(b) ? b : '.';  sz[1] = 0;
    strcat_s(szLine, sizeof(szLine), sz);
  }
  CMDOUTS(szLine);
}

void CUI::DoExamineRange (const CGenericMemory *pMemory, address_t nStart, address_t nEnd)
{
  //++
  //   THis method handles the EXAMINE command where the argument is a range
  // of memory addresses.  If the range is a single byte then we just print
  // that byte and quit.  If the range is more than one byte but less than 16
  // then it prints a single line with just those bytes.  If the range is
  // larger than 16 bytes then it prints multiple lines, carefully fixed up
  // to align with multiples of 16 and with the first and last lines indented
  // so that all bytes with the same low order 4 address bits line up.
  //--
  if (nStart == nEnd) {
    CMDOUTF("%04X/ %02X", nStart, pMemory->UIread(nStart));
  } else if ((nEnd-nStart) < 16) {
    DumpLine(pMemory, nStart, nEnd-nStart+1);
  } else {
    if ((nStart & 0xF) != 0) {
      address_t nBase = nStart & 0xFFF0;
      address_t nOffset = nStart - nBase;
      DumpLine(pMemory, nStart, 16-nOffset, (unsigned) nOffset);
      nStart += 16-nOffset;
    }
    for (; nStart <= nEnd; nStart += 16) {
      if ((nEnd-nStart) < 16)
        DumpLine(pMemory, nStart, nEnd-nStart+1, 0, (unsigned) (16-(nEnd-nStart+1)));
      else
        DumpLine(pMemory, nStart, 16);
    }
  }
}

size_t CUI::DoExamineInstruction (string &sCode, address_t nStart, const CGenericMemory *pMemory)
{
  //++
  //   This method will disassemble one instruction for the EXAMINE/INSTRUCTION
  // command.  Since instructions are variable length, this can potentially
  // examine 1, 2 or 3 bytes of memory.  The actual number of bytes used is
  // returned.
  //--
  uint8_t bOpcode, b2=0, b3=0, b4=0;

  // Disassemble the opcode and fetch any operands ...
  size_t nCount = Disassemble(pMemory, nStart, sCode);
  bOpcode = pMemory->UIread(nStart);
  if (nCount > 1) b2 = pMemory->UIread(nStart+1);
  if (nCount > 2) b3 = pMemory->UIread(nStart+2);
  if (nCount > 3) b4 = pMemory->UIread(nStart+3);

  // Print it out neatly ...
  if (nCount <= 1)
    sCode = FormatString("%04X/ %02X             ", nStart, bOpcode) + sCode;
  else if (nCount == 2)
    sCode = FormatString("%04X/ %02X %02X          ", nStart, bOpcode, b2) + sCode;
  else if (nCount == 3)
    sCode = FormatString("%04X/ %02X %02X %02X       ", nStart, bOpcode, b2, b3) + sCode;
  else
    sCode = FormatString("%04X/ %02X %02X %02X %02X    ", nStart, bOpcode, b2, b3, b4) + sCode;


  // Return the number of bytes disassembled and we're done...
  return nCount;
}

string CUI::ExamineRegister (int nIndex)
{
  //++
  //   This method will fetch the contents of an internal CPU register and
  // return a formatted string with the register name and value.  This is a
  // tiny bit tricky because registers can have 1, 4, 8 or 16 bits and we try
  // to print the right thing.
  //
  //   Note that nIndex is the index in the CPU register name table.  It is
  // NOT the REGISTER_NUMBER - we'll fetch that ourselves!
  //--
  const CCmdArgKeyword::KEYWORD *paNames = g_pCPU->GetRegisterNames();
  cpureg_t nRegister = (int) paNames[nIndex].m_pValue;
  unsigned nSize = g_pCPU->GetRegisterSize(nRegister) / 4;
  uint16_t nValue = g_pCPU->GetRegister(nRegister);
  return FormatString("%s=%0*X", paNames[nIndex].m_pszName, nSize, nValue);
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
    if (   (i == CCOSMAC::REG_IE)
        || (i == CCOSMAC::REG_XIE)
        || ((sLine.length()+sRegister.length()) > 75)) {
      CMDOUTS(sLine);  sLine.clear();
    }
    sLine += sRegister + (i < 16 ? "  " : " ");
    //if (fBrief && (i == CCOSMAC::REG_B)) break;
    if (!g_pCPU->IsExtended() && (i == CCOSMAC::REG_EF4)) break;
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
        DoExamineAllRegisters(true);
      } else if (!DoExamineOneRegister(sName)) {
        CMDERRS("Unknown register - \"" << sName << "\"");  return false;
      }
    } else {
      address_t nStart = pArg->GetRangeArg().GetStart();
      address_t nEnd = pArg->GetRangeArg().GetEnd();
      CGenericMemory *pMemory = GetMemorySpace();
      if (!pMemory->IsValid(nStart, nEnd)) {
         CMDERRF("range exceeds memory - %04x to %04x", nStart, nEnd);  return false;
      } else if (m_modInstruction.IsPresent()) {
        while (nStart <= nEnd) {
          string sCode;
          nStart += DoExamineInstruction(sCode, nStart, pMemory) & ADDRESS_MASK;
          CMDOUTS(sCode);
        }
      } else
        DoExamineRange(pMemory, nStart, nEnd);
    }
  }
  return true;
}

bool CUI::DoDepositRange (CGenericMemory *pMemory, address_t nStart, address_t nEnd, CCmdArgList &List)
{
  //++
  //   Deposit one or more bytes into main memory starting from nStart and
  // proceeding to successively higher addresses.  If the number of data items
  // would cause nEnd to be exceed, then give an error message and quit.  nEnd
  // is otherwise ignored - i.e. it's not an error to specify too few items!
  //--
  bool fHasEnd = nStart != nEnd;
  for (size_t i = 0;  i < List.Count();  ++i) {
    if (fHasEnd && (nStart > nEnd)) {
      CMDERRS("too many data items to deposit");  return false;
    }
    CCmdArgNumber *pData = dynamic_cast<CCmdArgNumber *> (List[i]);
    if (!pMemory->IsValid(nStart)) {
      CMDERRF("address exceeds memory - %04X", nStart);  return false;
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
  // the register and a new value, in hexadecimal.  Altering main memory needs
  // a memory address and then a list of one or more hex numbers to be stored.
  // If multiple data items are specified then they are stored in successively
  // higher memory addresses starting from the one specified.  It's actually
  // possible to specify a range for the memory address - in that case the
  // ending address is ignored UNLESS the number of data items specified would
  // exceed the range, in which case an error occurs.
  //--
  if (m_argExamineDeposit.IsName()) {
    if (m_argDataList.Count() > 1) {
      CMDERRS("only one datum allowed for DEPOSIT register");
      return false;
    }
    string sRegister = m_argExamineDeposit.GetValue();
    CCmdArgNumber *pData = dynamic_cast<CCmdArgNumber *> (m_argDataList[0]);
    return DoDepositRegister(sRegister, pData->GetNumber());
  } else {
    address_t nStart = m_argExamineDeposit.GetRangeArg().GetStart();
    address_t nEnd = m_argExamineDeposit.GetRangeArg().GetEnd();
    CGenericMemory *pMemory = GetMemorySpace();
    if (!pMemory->IsValid(nStart, nEnd)) {
      CMDERRF("range exceeds memory - %04x to %04x", nStart, nEnd);  return false;
    }
    return DoDepositRange(pMemory, nStart, nEnd, m_argDataList);
  }
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////// SEND and RECEIVE COMMANDS ///////////////////////////
////////////////////////////////////////////////////////////////////////////////

bool CUI::DoCloseSend (CCmdParser& cmd)
{
  //++
  // Handle the SEND/CLOSE command (see SEND for more details) ...
  //--
  assert(g_pConsole != NULL);
  if (m_argOptFileName.IsPresent())
    CMDERRS("File name ignored - " << m_argOptFileName.GetValue());
  if (m_modXModem.IsPresent())
    g_pConsole->XAbort();
  else
    g_pConsole->AbortText();
  return true;
}

bool CUI::DoCloseReceive (CCmdParser& cmd)
{
  //++
  // Handle the RECEIVE/CLOSE command (see RECEIVE for more details) ...
  //--
  assert(g_pConsole != NULL);
  if (m_argOptFileName.IsPresent())
    CMDERRS("File name ignored - " << m_argOptFileName.GetValue());
  if (m_modXModem.IsPresent())
    g_pConsole->XAbort();
  else
    g_pConsole->CloseLog();
  return true;
}

bool CUI::DoSendFile (CCmdParser &cmd)
{
  //++
  //   The SEND command lets you transmit a file to the emulation using
  // either the XMODEM protocol or as a raw text file.
  // 
  //   SEND/TEXT <filename> [/CRLF or /NOCRLF] [/DELAY=(line,char)]
  //
  // Sends a raw text file to the emulation.  The /DELAY modifier specifies
  // the simulated delay, IN MILLISECONDS, between lines and characters.
  // The /NOCRLF modifier specifies that the sequence <CR><LF> or just a bare
  // <LF> (i.e. a classic Unix newline) in the input file will be sent as a
  // <CR> only.  /CRLF sends the input file without modification.
  // 
  //   SEND/TEXT/CLOSE
  //
  // Closes the current text file and stops sending.  The text file is closed
  // automatically when we've reached the end, but this command can be used to
  // abort a transfer early.
  // 
  //   SEND/XMODEM <filename> [/DELAY=delay]
  //
  // Sends a file to the emulation using the XMODEM protocol.  The /DELAY
  // modifier specifies the interval, IN MILLISECONDS, between characters when
  // sending.  Note that the /DELAY settings for both text and XMODEM transfers
  // are "sticky" and will be remembered for subsequent transfers.
  // 
  //   SEND/XMODEM/CLOSE
  //
  // Aborts the XMODEM transfer early.
  //--
  assert(g_pConsole != NULL);

  // Check for the /CLOSE option, and parse the file name if not.
  if (m_modClose.IsPresent()) return DoCloseSend(cmd);
  if (!m_argOptFileName.IsPresent())
    {CMDERRS("File name required");  return false;}
  string sFileName = m_argOptFileName.GetFullPath();

  // Handle the /DELAY modifier, if present ...
  if (m_modDelayList.IsPresent()) {
    if (m_modXModem.IsPresent()) {
      // For XMODEM, /DELAY wants only one parameter ...
      if (m_argDelayList.Count() != 1) {
        CMDERRS("specify /DELAY=nnn in milliseconds");  return false;
      }
      uint64_t qDelay = MSTONS((dynamic_cast<CCmdArgNumber*> (m_argDelayList[0]))->GetNumber());
      g_pConsole->SetXdelay(qDelay);
    } else {
      // For TEXT, /DELAY wants two parameters ...
      if (m_argDelayList.Count() != 2) {
        CMDERRS("specify /DELAY=(line,character) in milliseconds");  return false;
      }
      uint64_t qLineDelay = MSTONS((dynamic_cast<CCmdArgNumber*> (m_argDelayList[0]))->GetNumber());
      uint64_t qCharDelay = MSTONS((dynamic_cast<CCmdArgNumber*> (m_argDelayList[1]))->GetNumber());
      g_pConsole->SetTextDelays(qCharDelay, qLineDelay);
    }
  }

  // And we're finally ready to start the transfer ...
  if (!m_modXModem.IsPresent()) {
    // Handle the /[NO]CRLF modifier ...
    if (m_modCRLF.IsPresent())
      g_pConsole->SetTextNoCRLF(m_modCRLF.IsNegated());
    return g_pConsole->SendText(sFileName);
  } else {
    return g_pConsole->SendFile(sFileName);
  }
}

bool CUI::DoReceiveFile (CCmdParser &cmd)
{
  //++
  //   The RECEIVE command lets you transmit a file to the emulation using
  // either the XMODEM protocol or as a raw text file.
  // 
  //   RECEIVE/TEXT <filename> [/APPEND or /OVERWRITE]
  //
  // Writes the output from the emulation to a raw text file (yes, it's
  // basically capturing a log file).  The /APPEND or /OVERWRITE modifiers 
  // specify whether any existing file of the same name should be overwritten
  // or appended to.
  // 
  //   RECEIVE/TEXT/CLOSE
  //
  // Closes the current text file and stops logging.
  // 
  //   RECEIVE/XMODEM <filename> [/DELAY=delay]
  //
  // Receives a file from the emulation using the XMODEM protocol.  The /DELAY
  // modifier here works exactly as it does for the SEND command.  Note that the
  // XMODEM receive ALWAYS overwrites any existing file.
  // 
  //   RECEIVE/XMODEM/CLOSE
  //
  // Aborts the XMODEM transfer early.
  //--
  assert(g_pConsole != NULL);
  if (m_modClose.IsPresent()) return DoCloseReceive(cmd);
  if (!m_argOptFileName.IsPresent())
    {CMDERRS("File name required");  return false;}
  string sFileName = m_argOptFileName.GetFullPath();
  bool fAppend = m_modAppend.IsPresent() && !m_modAppend.IsNegated();
  if (m_modXModem.IsPresent())
    return g_pConsole->ReceiveFile(sFileName);
  else
    return g_pConsole->OpenLog(sFileName, fAppend);
}


////////////////////////////////////////////////////////////////////////////////
/////////////////// RUN, STEP, CONTINUE and RESET COMMANDS /////////////////////
////////////////////////////////////////////////////////////////////////////////

unsigned CUI::RunSimulation (uint32_t nSteps)
{
  //++
  //   This procedure will run the simulation engine for the specified number
  // of instructions, or indefinitely if nSteps is zero.  The simulation will
  // end either when the step count is reached, or some error (e.g. illegal
  // opcode, illegal I/O, etc) occurs, or the user enters the break character
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
      CMDERRF("illegal I/O at 0x%04X", g_pCPU->GetLastPC());  break;
    case CCPU::STOP_ILLEGAL_OPCODE:
      CMDERRF("illegal instruction at 0x%04X", g_pCPU->GetLastPC());  break;
    case CCPU::STOP_HALT:
      CMDERRF("halt at 0x%04X", g_pCPU->GetLastPC());  break;
    case CCPU::STOP_ENDLESS_LOOP:
      CMDERRF("endless loop at 0x%04X", g_pCPU->GetPC());  break;
    case CCPU::STOP_BREAKPOINT:
      CMDERRF("breakpoint at 0x%04X", g_pCPU->GetPC());  break;
    case CCPU::STOP_BREAK:
      CMDERRF("break at 0x%04X", g_pCPU->GetPC());  break;
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
  unsigned nStop = RunSimulation();
  return    (nStop != CCPU::STOP_ILLEGAL_IO)
         && (nStop != CCPU::STOP_ILLEGAL_OPCODE)
         && (nStop != CCPU::STOP_ENDLESS_LOOP);
}

bool CUI::DoRun (CCmdParser &cmd)
{
  //++
  //   The RUN command is essentially the same as CONTINUE, except that it
  // resets the CPU and all peripherals first.  If an argument is given to the
  // command, e.g. "RUN 8000", then this is taken as a starting address and
  // will be deposited in the PC before we start.
  //--
  DoReset(cmd);
  if (m_argRunAddress.IsPresent()) {
    g_pCPU->SetRegister(CCOSMAC::REG_R0, m_argRunAddress.GetNumber());
  }
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
    CMemoryMap::CHIP_SELECT nSelect = CMemoryMap::ChipSelect(g_pMCR->GetMap(), wPC);
    if ((nSelect != CMemoryMap::CS_RAM) && (nSelect != CMemoryMap::CS_ROM)) {
      CMDERRF("PC address %04X is not in RAM or ROM!", wPC);  return false;
    }
    CGenericMemory *pMemory = (nSelect==CMemoryMap::CS_RAM) ? g_pRAM : g_pROM;
    string sCode;
    DoExamineInstruction(sCode, wPC, pMemory);
    CMDOUTS(CMemoryMap::ChipToString(nSelect) << " " << sCode);
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
  g_pMCR->ClearDevice();
  g_pPIC->ClearDevice();
  g_pRTC->ClearDevice();
  return true;
}

bool CUI::DoInput (CCmdParser &cmd)
{
  //++
  //   The INPUT command simulates pressing the INPUT/ATTENTION button on
  // the SBC1802.  An 8 bit value can optionally be loaded into the DIP
  // switches at the same time, if desired.
  //--
  assert(g_pSwitches != NULL);
  if (m_argOptSwitches.IsPresent())
    g_pSwitches->SetSwitches(m_argOptSwitches.GetNumber());
  g_pSwitches->RequestAttention();
  return true;
}


////////////////////////////////////////////////////////////////////////////////
///////////////////////////// BREAKPOINT COMMANDS //////////////////////////////
////////////////////////////////////////////////////////////////////////////////

bool CUI::DoSetBreakpoint (CCmdParser &cmd)
{
  //++
  //   The "SET BREAKPOINT xxxx" command will (what else??) set a breakpoint
  // at the specified address range.  Note that there's no error message if you
  // set a breakpoint at an address that already has a breakpoint, but only one
  // breakpoint is actually set.
  //--
  CGenericMemory *pMemory = GetMemorySpace();
  address_t nStart = m_argBreakpoint.GetStart();
  address_t nEnd = m_argBreakpoint.GetEnd();
  if (!pMemory->IsValid(nStart, nEnd)) {
    CMDERRF("breakpoint range outside memory - %04x to %04x", nStart, nEnd);  return false;
  } else {
    while (nStart <= nEnd) pMemory->SetBreak(nStart++);
  }
  return true;
}

bool CUI::DoClearBreakpoint (CCmdParser &cmd)
{
  //++
  //   The "CLEAR BREAKPOINT [oooooo]" command will remove the breakpoint(s) at
  // the specified address range or, if no range is specified, it will remove
  // all breakpoints.  Breakpoints may be removed from either RAM or ROM, and
  // RAM is the default if no qualifier is specified.  Note that there's no error
  // message if you ask it to clear a breakpoint that doesn't exist!
  //--
  assert((g_pRAM != NULL)  &&  (g_pROM != NULL));
  if (m_argOptBreakpoint.IsPresent()) {
    CGenericMemory *pMemory = GetMemorySpace();
    address_t nStart = m_argOptBreakpoint.GetStart();
    address_t nEnd = m_argOptBreakpoint.GetEnd();
    if (!pMemory->IsValid(nStart, nEnd)) {
      CMDERRF("breakpoint range outside memory - %04x to %04x", nStart, nEnd);  return false;
    } else {
      while (nStart <= nEnd) pMemory->SetBreak(nStart++, false);
    }
  } else {
    //   Here to remove all breakpoints.  If either /RAM or /ROM was specified,
    // then remove all breakpoints from that memory space only.  If neither was
    // specified, then remove all breakpoints from both!
    if (m_modROM.IsPresent()) {
      if (m_modROM.IsNegated())
        g_pRAM->ClearAllBreaks();
      else
        g_pROM->ClearAllBreaks();
    } else {
      g_pRAM->ClearAllBreaks();
      g_pROM->ClearAllBreaks();
    }
  }
  return true;
}

string CUI::ShowBreakpoints (const CGenericMemory *pMemory)
{
  //++
  //  Show all breakpoints in a given memory space ...
  //--
  string sBreaks;  address_t nLoc = pMemory->Base()-1;
  while (pMemory->FindBreak(nLoc)) {
    sBreaks += sBreaks.empty() ? "Breakpoint(s) at " : ", ";
    //   We found one breakpoint.  See if it's the start of a contiguous range
    // of breakpoints, or if it's all by itself ...
    char sz[16];  
    if (pMemory->IsBreak(nLoc+1)) {
      // There's more than one!
      address_t nEnd = nLoc+1;
      while (pMemory->IsBreak(nEnd)) ++nEnd;
      sprintf_s(sz, sizeof(sz), "%04X-%04X", nLoc, nEnd-1);
      nLoc = nEnd;
    } else {
      // Just this one only ...
      sprintf_s(sz, sizeof(sz), "%04X", nLoc);
    }
    sBreaks += sz;
  }
  return (sBreaks.empty()) ? "none" : sBreaks;
}

bool CUI::DoShowBreakpoints (CCmdParser &cmd)
{
  //++
  // List all current breakpoints ...
  //--
  if (m_modROM.IsPresent() && !m_modROM.IsNegated()) {
    CMDOUTS("ROM: " << ShowBreakpoints(g_pROM));
  } else if (m_modROM.IsPresent() && m_modROM.IsNegated()) {
    CMDOUTS("RAM: " << ShowBreakpoints(g_pRAM));
  } else {
    CMDOUTS("ROM: " << ShowBreakpoints(g_pROM));
    CMDOUTS("RAM: " << ShowBreakpoints(g_pRAM));
  }
  return true;
}


////////////////////////////////////////////////////////////////////////////////
///////////////////////////////// CPU COMMANDS /////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

bool CUI::DoClearCPU (CCmdParser &cmd)
{
  //++
  //   Clear (reset!) the CPU ONLY.  Note that the PIC and the MCR are NOT
  // affected, which may or may not be what you want.  Use the RESET command
  // to clear everything...
  //--
  g_pCPU->ClearCPU();
  return true;
}

bool CUI::DoSetCPU (CCmdParser &cmd)
{
  //++
  //   SET CPU allows you to set the CPU type (e.g. 1802 or 1805), and well as
  // vaious options (e.g. stop on illegal I/O, stop on illegal opcode, etc).
  //--
  assert(g_pCPU != NULL);
  if (m_modIllegalIO.IsPresent())
    g_pCPU->StopOnIllegalIO(m_argStopIO.GetKeyValue() != 0);
  if (m_modIllegalOpcode.IsPresent())
    g_pCPU->StopOnIllegalOpcode(m_argStopOpcode.GetKeyValue() != 0);
  if (m_modBreakChar.IsPresent())
    g_pConsole->SetConsoleBreak(m_argBreakChar.GetNumber());
  if (m_modCPUextended.IsPresent())
    g_pCPU->SetExtended(!m_modCPUextended.IsNegated());
  if (m_modClockFrequency.IsPresent()) {
    // Note that changing the CPU clock frequency affects the CTC timer A too!!
    uint32_t lFrequency = m_argFrequency.GetNumber();
    g_pCPU->SetCrystalFrequency(lFrequency);
    g_pCTC->SetClockA(lFrequency);
  }
  return true;
}

bool CUI::DoShowCPU (CCmdParser &cmd)
{
  //++
  //   The SHOW CPU command displays the CPU name, clock frequency, startup
  // mode and break character.  After that, we also display the internal
  // CPU registers, and the state of the interrupt system too.
  //--
  CMDOUTS("");
  // Show general CPU information ...
  double flCrystal = g_pCPU->GetCrystalFrequency() / 1000000.0;
  double flMajorCycle = CCOSMAC::CLOCKS_PER_CYCLE / flCrystal;
  CMDOUTF("%s %s %3.2fMHz (%3.2fus per microcycle)",
    g_pCPU->GetName(), g_pCPU->GetDescription(), flCrystal, flMajorCycle);
  CMDOUTF("%s instruction set, BREAK is Control-%c",
    g_pCPU->IsExtended() ? "Extended" : "Standard", g_pConsole->GetConsoleBreak() + '@');
  CMDOUTF("%s on illegal opcode, %s on illegal I/O",
    g_pCPU->IsStopOnIllegalOpcode() ? "Stop" : "Continue",
    g_pCPU->IsStopOnIllegalIO() ? "Stop" : "Continue");
  if (g_pCPU->IsExtended())
    CMDOUTF("Counter/timer mode is %s", CCOSMAC::CounterModeToString(g_pCPU->GetCounterMode()));

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

  // Show interrupt status ...
  CMDOUTS(std::endl << "INTERRUPTS");
  CMDOUTS("IRQ  REQ  MASK  VECTOR    DEVICE");
  CMDOUTS("---  ---  ----  --------  --------");
  for (CPriorityInterrupt::IRQLEVEL i = CCDP1877::PICLEVELS; i > 0; --i) {
    CSimpleInterrupt* pInterrupt = g_pPIC->GetLevel(i);
    CDevice *pDevice = g_pCPU->FindDevice(pInterrupt);
    if (pDevice == NULL) pDevice = g_pTLIO->FindDevice(pInterrupt);
    if ((pDevice == NULL) && (pInterrupt == g_pRTC->GetInterrupt())) pDevice = g_pRTC;
    CMDOUTF("%2d   %-3s  %-4s  %02X %02X %02X  %-8s",
      i-1, g_pPIC->IsRequestedAtLevel(i) ? "YES" : "no",
      g_pPIC->IsMasked(i) ? "yes" : "NO",
      CCDP1877::LBR, g_pPIC->GetPage(), g_pPIC->ComputeVector(i),
      (pDevice != NULL) ? pDevice->GetName() : "");
  }

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
  //   The "CLEAR MEMORY/RAM" command clears all of RAM, and "CLEAR MEMORY/ROM"
  // clears all of EPROM.  "CLEAR MEMORY" with no modifier clears both!
  //--
  assert((g_pRAM != NULL) && (g_pROM != NULL));
  if (m_modROM.IsPresent()) {
    if (m_modROM.IsNegated())
      g_pRAM->ClearMemory();
    else
      g_pROM->ClearMemory();
  }
  else {
    g_pRAM->ClearMemory();  g_pROM->ClearMemory();
  }
  return true;
}

bool CUI::DoShowMemory(CCmdParser &cmd)
{
  //++
  //   The SHOW MEMORY command will print a memory map of the SBC1802.  Rather
  // than just hardwire this information (it's fixed after all, and can't
  // really change) we'll attempt to discover it by exercising the ChipSelect()
  // method in the CMemoryMap class.  This is the actual routine that the CPU
  // uses to figure out which memory space should be selected by a given memory
  // address.
  // 
  //   Note that ChipSelect() has no less than five (!) memory mapping modes,
  // BOOT, ROM0, ROM1, ELFOS and MICRODOS.  We'll show a table of the results
  // for ball cases.
  //--
  address_t nStart=0, nCurrent=0, a=0;  bool fLast=false;
  CMemoryMap::CHIP_SELECT nLastBOOT = CMemoryMap::ChipSelect(CMemoryControl::MCR_BOOT, a);  a = 0;
  CMemoryMap::CHIP_SELECT nLastROM0 = CMemoryMap::ChipSelect(CMemoryControl::MCR_ROM0, a);  a = 0;
  CMemoryMap::CHIP_SELECT nLastROM1 = CMemoryMap::ChipSelect(CMemoryControl::MCR_ROM1, a);  a = 0;
  CMemoryMap::CHIP_SELECT nLastELOS = CMemoryMap::ChipSelect(CMemoryControl::MCR_ELOS, a);  a = 0;
  CMemoryMap::CHIP_SELECT nLastMDOS = CMemoryMap::ChipSelect(CMemoryControl::MCR_MDOS, a);
  CMDOUTS("");
  CMDOUTF("ADDRESS       BOOT   ROM0   ROM1   ELFOS  MDOS   SIZE");
  CMDOUTF("-----------   -----  -----  -----  -----  -----  ----------");

  while (!fLast) {
    fLast = (nCurrent == 0xFFFF);  ++nCurrent;
    CMemoryMap::CHIP_SELECT nBOOT, nROM0, nROM1, nELOS, nMDOS;
    a = nCurrent;  nBOOT = CMemoryMap::ChipSelect(CMemoryControl::MCR_BOOT, a);
    a = nCurrent;  nROM0 = CMemoryMap::ChipSelect(CMemoryControl::MCR_ROM0, a);
    a = nCurrent;  nROM1 = CMemoryMap::ChipSelect(CMemoryControl::MCR_ROM1, a);
    a = nCurrent;  nELOS = CMemoryMap::ChipSelect(CMemoryControl::MCR_ELOS, a);
    a = nCurrent;  nMDOS = CMemoryMap::ChipSelect(CMemoryControl::MCR_MDOS, a);

    if (   fLast                || (nBOOT != nLastBOOT)
        || (nROM0 != nLastROM0) || (nROM1 != nLastROM1)
        || (nELOS != nLastELOS) || (nMDOS != nLastMDOS)) {
      string sLine;
      address_t cbSegment = nCurrent - nStart;
      if (cbSegment == 1)
        sLine = FormatString("$%04X       ", nStart);
      else
        sLine = FormatString("$%04X..%04X ", nStart, ADDRESS(nCurrent-1));
      sLine += FormatString("  %-5s  %-5s  %-5s  %-5s  %-5s",
        CMemoryMap::ChipToString(nLastBOOT), CMemoryMap::ChipToString(nLastROM0),
        CMemoryMap::ChipToString(nLastROM1), CMemoryMap::ChipToString(nLastELOS),
        CMemoryMap::ChipToString(nLastMDOS));
      if ((cbSegment & 0x3FF) == 0)
        sLine += FormatString("  %3dK bytes", cbSegment>>10);
      else
        sLine += FormatString("  %4d bytes", cbSegment);

      CMDOUTS(sLine);
      nLastBOOT = nBOOT;  nLastROM0 = nROM0;  nLastROM1 = nROM1;
      nLastELOS = nELOS;  nLastMDOS = nMDOS;  nStart = nCurrent;
    }
  }
  CMDOUTS("");  return true;
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
  //   On the SBC1802 the MCR, PIC and RTC are all "special" because they are
  // memory mapped rather than I/O mapped.  That means CCPU::FindDevice() can't
  // find them, so we have to make a special check just for those devices.
  //--
  CDevice *pDevice = g_pTLIO->FindDevice(sDevice);
  if (pDevice != NULL) return pDevice;
  if (CCmdArgKeyword::Match(sDevice, g_pPIC->GetName())) return g_pPIC;
  if (CCmdArgKeyword::Match(sDevice, g_pRTC->GetName())) return g_pRTC;
  if (CCmdArgKeyword::Match(sDevice, g_pMCR->GetName())) return g_pMCR;
  if (CCmdArgKeyword::Match(sDevice, g_pPSG1->GetName())) return g_pPSG1;
  if (CCmdArgKeyword::Match(sDevice, g_pPSG2->GetName())) return g_pPSG2;
  CMDERRS("No such device as " << sDevice);
  return NULL;
}

bool CUI::ShowTape ()
{
  //++
  // Show details for the TU58 tape interface ...
  //--
  assert(g_pTU58 != NULL);
  ostringstream ofs;
  ofs << std::endl;  g_pTU58->ShowDevice(ofs);  ofs << std::endl;
  CMDOUT(ofs);
  return true;
}

string CUI::ShowDeviceSense (const CDevice *pDevice)
{
  //++
  // Return a string of the EFx inputs used by this device ...
  //--
  bool fFirst = true;  string sResult;
  uint8_t nGroup = g_pTLIO->FindGroup(pDevice);
  if (nGroup == 0) return "";
  for (address_t nEF = CCOSMAC::EF1;  nEF <= CCOSMAC::EF4;  ++nEF) {
    if (g_pTLIO->FindSenseDevice(nGroup, nEF) == pDevice) {
      address_t n = nEF - CCOSMAC::EF1 + 1;
      if (fFirst)
        sResult = FormatString("EF%d", n);
      else
        sResult += FormatString(",%d", n);
      fFirst = false;
    }
  }
  return sResult;
}

void CUI::ShowOneDevice(const CDevice* pDevice, bool fHeading)
{
  //++
  // Show the common device options (description, ports, type) to a string.
  //--
  string sLine;  bool fTLIO = g_pTLIO->IsTLIOenabled();

  sLine = FormatString("%-8s  %-9s  %-30s  ",
    pDevice->GetName(), pDevice->GetType(), pDevice->GetDescription());

  if (pDevice->IsInOut())
    sLine += " INOUT  ";
  else if (pDevice->IsInput())
    sLine += " INPUT  ";
  else if (pDevice->IsOutput())
    sLine += " OUTPUT ";
  else
    sLine += "        ";

  if (fTLIO && (pDevice == g_pTLIO)) {
    sLine += "  ALL  ";
  } else {
    uint8_t nGroup = g_pTLIO->FindGroup(pDevice);
    if ((pDevice == g_pPSG1) || (pDevice == g_pPSG2))
      nGroup = g_pTLIO->FindGroup(g_pTwoPSGs);
    if (fTLIO && (nGroup != 0))
      sLine += FormatString("  $%02X  ", nGroup);
    else
      sLine += "       ";
  }

  if ((pDevice == g_pTLIO) || (pDevice->GetPortCount() <= 1)) {
    if (pDevice->GetBasePort() <= 7)
      sLine += FormatString("      %d      ", pDevice->GetBasePort());
    else
      sLine += FormatString(" $%04X       ", pDevice->GetBasePort());
  } else {
    if (pDevice->GetBasePort() <= 7)
      sLine += FormatString("     %d..%d    ",
        pDevice->GetBasePort(), pDevice->GetBasePort() + pDevice->GetPortCount()-1);
    else
      sLine += FormatString(" $%04X..%04X ",
        pDevice->GetBasePort(), pDevice->GetBasePort() + pDevice->GetPortCount()-1);
  }

  sLine += " " + ShowDeviceSense(pDevice);

  if (fHeading) {
    CMDOUTS("DEVICE    TYPE       DESCRIPTION                      IN/OUT  GROUP      PORT     SENSE    ");
    CMDOUTS("--------  ---------  -------------------------------  ------  -----  -----------  -------  ");
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
  bool fTLIO = g_pTLIO->IsTLIOenabled();
  CMDOUTS("");
  ShowOneDevice(g_pMCR, true);
  ShowOneDevice(g_pRTC);
  ShowOneDevice(g_pPIC);
  ShowOneDevice(g_pSLU0);
  ShowOneDevice(g_pLEDS);
  ShowOneDevice(g_pSwitches);
  ShowOneDevice(g_pIDE);
  ShowOneDevice(g_pBRG);
  if (fTLIO) {
    ShowOneDevice(g_pTLIO);
    ShowOneDevice(g_pSLU1);
    ShowOneDevice(g_pPPI);
    ShowOneDevice(g_pCTC);
    ShowOneDevice(g_pPSG1);
    ShowOneDevice(g_pPSG2);
  }
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

  //   The TU58 doesn't have a CDevice interface, so we have to make
  // a special case for that one...
  string sDevice = m_argOptDeviceName.GetValue();
  if (CCmdArgKeyword::Match(sDevice, "TAPE")) return ShowTape();

  // Otherwise try to match the device name ...
  CDevice *pDevice = FindDevice(sDevice);
  if (pDevice == NULL) return false;

  // And show the detailed device characteristics ...
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
    g_pRTC->ClearDevice();
    g_pMCR->ClearDevice();
    g_pPIC->ClearDevice();
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
  string sDevice = m_argDeviceName.GetValue();

  // The TU58 is a special case because it's not a CDevice derived class!
  if (CCmdArgKeyword::Match(sDevice, "TAPE")) {
    if (m_modEnable.IsPresent()) g_pTU58->Enable(!m_modEnable.IsNegated());
    return true;
  }

  // The printer is also a special case!
  if (CCmdArgKeyword::Match(sDevice, "PRI*NTER")) {
    if (m_modSpeed.IsPresent()) g_pPPI->SetSpeed(m_argSpeed.GetNumber());
    if (m_modWidth.IsPresent())
      g_pPPI->SetWidth(m_modWidth.IsNegated() ? 0 : m_argOptWidth.GetNumber());
    return true;
  }

  // Search for the corresponding CDevice object ...
  CDevice *pDevice = FindDevice(sDevice);
  if (pDevice == NULL) return false;

  // Apply the device specific options ...
  if ((pDevice == g_pTLIO) && m_modEnable.IsPresent()) {
    g_pTLIO->EnableTLIO(!m_modEnable.IsNegated());
  } else if ((pDevice == g_pSwitches) && m_modSwitches.IsPresent()) {
    g_pSwitches->SetSwitches(m_argSwitches.GetNumber());
  } else if (pDevice == g_pSLU0) {
    if (m_modTxSpeed.IsPresent()) g_pSLU0->SetTxSpeed(m_argTxSpeed.GetNumber());
    if (m_modRxSpeed.IsPresent()) g_pSLU0->SetRxSpeed(m_argRxSpeed.GetNumber());
  } else if (pDevice == g_pSLU1) {
    if (m_modTxSpeed.IsPresent()) g_pSLU1->SetTxSpeed(m_argTxSpeed.GetNumber());
    if (m_modRxSpeed.IsPresent()) g_pSLU1->SetRxSpeed(m_argRxSpeed.GetNumber());
  } else if (pDevice == g_pIDE) {
    if (m_modShortDelay.IsPresent()) g_pIDE->SetShortDelay(USTONS(m_argShortDelay.GetNumber()));
    if (m_modLongDelay.IsPresent()) g_pIDE->SetLongDelay(USTONS(m_argLongDelay.GetNumber()));
    if (m_modEnable.IsPresent()) g_pIDE->Enable(!m_modEnable.IsNegated());
  } else if ((pDevice == g_pPIC) && m_modEnable.IsPresent()) {
    g_pPIC->EnablePIC(!m_modEnable.IsNegated());
    g_pMemoryMap->EnablePIC(!m_modEnable.IsNegated());
  } else if ((pDevice == g_pRTC) && m_modEnable.IsPresent()) {
    g_pRTC->EnableRTC(!m_modEnable.IsNegated());
    g_pMemoryMap->EnableRTC(!m_modEnable.IsNegated());
  } else if ((pDevice == g_pPPI) && m_modEnable.IsPresent()) {
    g_pPPI->EnablePPI(!m_modEnable.IsNegated());
  } else if ((pDevice == g_pCTC) && m_modEnable.IsPresent()) {
    g_pCTC->EnableCTC(!m_modEnable.IsNegated());
  } else if (((pDevice == g_pPSG1) || (pDevice == g_pPSG2)) && m_modEnable.IsPresent()) {
    static_cast<CPSG *>(pDevice)->EnablePSG(!m_modEnable.IsNegated());
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
  CMDOUTF("\nSBC1802 Emulator v%d\n", SBCVER);
  return true;
}
