//++
// UserInterface.cpp -> SBCT11 Emulator Specific User Interface code
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
//   This module implements the user interface specific to the SBCT11 emulator
// process.  The first half of the file are parse tables for the generic command
// line parser classes from CommandParser.cpp, and the second half is the action
// routines needed to implement these commands.  
//
//                                                    Bob Armstrong [21-FEB-20]
//
//                                  COMMAND SUMMARY
// ----------------------------------------------------------------------------
//
//   LO*AD filename             - load file into memory
//      /RA*M                   -  ... into RAM
//      /RO*M                   -  ... into ROM
//      /NVR                    -  ... into NVR
//      /FORMAT=BIN*ARY         - raw binary file
//      /FORMAT=IN*TEL          - Intel HEX file format
//      /FORMAT=ABS*OLUTE       - PDP11 absolute loader format
//      /COU*NT=count           - set count of bytes to load for raw binary
//      /BAS*E=address          - set starting address for raw binary
//   Note that for INTEL and ABSOLUTE files the /COUNT and /BASE options are
//      ignored since these file formats contain their own address information.
//
//   SA*VE filename             - save memory to file
//   The modifiers for SAVE are identical to LOAD!
// 
//   ATT*ACH DI*SK filename     - attach IDE drive to image file
//      /UNIT=0|1               - 0 -> master, 1-> slave
//      /CAPACITY=nnnnn         - set image size, IN SECTORS!
//
//   DET*ACH DI*SK              - detach IDE drive
//      /UNIT=0|1               - 0 -> master, 1-> slave
// 
//   ATT*ACH TA*PE filename     - attach TU58 drive to image file
//      /UNIT=0|1               - tape drive unit, 0 or 1
//      /CAPACITY=nnnnn         - set tape capacity, IN BLOCKS!
//      /READ                   - make tape unit read only
//      /WRITE                  - allow writing to this unit
//
//   DET*ACH TA*PE              - detach TU58 drive
//      /UNIT=0|1               - tape drive unit, 0 or 1
// 
//   E*XAMINE oooooo            - display just address oooooo (octal)
//      oooooo-oooooo           - display all addresses in the range
//      oooooo, oooooo, ...     - display multiple addresses or ranges
//      R0..R5, SP, PC, PSW...  - display individual CPU register(s)
//      RE*GISTERS              - display all registers
//      /RA*M                   - display data from RAM address space
//      /RO*M                   -    "      "    "  ROM    "      "
//      /W*ORD                  - display memory in word format
//      /B*YTE                  -    "      "    "  byte    "
//      /I*NSTRUCTION           - disassemble PDP11 instructions
//   Note that it's not possible to examine individual NVR locations, however
//     the SHOW DEVICE NVR command will dump the entire NVR contents!
// 
//   D*EPOSIT oooooo ooooooo    - deposit one word or byte
//      oooooo ooo, ooo, ...    - deposit several words or bytes
//      Rn oooooo               - deposit in a register (R0..R5, SP, PC, or PSW)
//      /W*ORD                  - deposit words in memory
//      /B*YTE                  - deposit bytes in memory
//   Note that it is not possible to deposit into ROM or NVR!
// 
//   RU*N [oooooo]              - clear CPU and start running at PC=xxxx
//   C*ONTINUE                  - resume execution at current PC// 
//   ST*EP [nnnn]               - single step and trace nnnn instructions
//   RES*ET                     - reset CPU and all devices
//   HA*LT                      - "halt" the PDP11 (which causes a trap to ROM!)
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
//   SE*T BRE*AKPOINT oooooo    - set breakpoint at address (octal)
//   CL*EAR BRE*AKPOINT oooooo   - clear   "      "     "       "
//   CL*EAR BRE*AKPOINTS        - clear all breakpoints
//   SH*OW BRE*AKPOINTS         - show breakpoints
//      /RAM                    - set/clear/show breakpoints in RAM space
//      /ROM                    -  "    "     "    "    "    "  ROM   "
// 
//   SH*OW CPU                  - show CPU details
//   CL*EAR CPU                 - reset the CPU only
//   SE*T CPU                   - set CPU options
//      /BRE*AK=nnn             - set break character to ASCII code nnn
//      /MODE=oooooo            - set T11 startup mode register
// 
//   SH*OW TI*ME                - show simulated CPU time
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
// 
//   SH*OW DI*SK                - show IDE disk status and parameters
//   SH*OW TA*PE                -  "   TU58 tape   "    "   "    "
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
//#ifdef THREADS
//   SET CHECK*POINT
//         /ENA*BLE             - enable file checkpointing
//         /DISA*BLE            - disable "      "    "
//         /INT*ERVAL=nn        - set checkpointing interval in seconds
//   SHOW CHECK*POINT           - show current checkpoint settings
//#endif
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
//   "oooooo" is an octal number
//   "[...]" indicates optional arguments
// 
// REVISION HISTORY:
// 21-FEB-20  RLA   New file.
//  5-MAR-20  RLA   Convert to octal (PDP-11s love octal!)
//                  Add /BYTE and /WORD for examine memory
// 21-JUL-22  RLA   Make STEP disassemble from the correct RAM/ROM space
// 28-JUL-22  RLA   FindDevice() doesn't work for SET DEVICE ...
// 17-DEC-23  RLA   Add SEND and RECEIVE commands
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
#include "ConsoleWindow.hpp"    // emulator console window methods
#include "SmartConsole.hpp"     // console file transfer functions
#include "CommandParser.hpp"    // emulator library command line parsing methods
#include "CommandLine.hpp"      // Shell (argc/argv) parser methods
#include "StandardUI.hpp"       // emulator library standard UI commands
#include "ImageFile.hpp"        // emulator library image file methods
#include "sbct11.hpp"           // global declarations for this project
#include "MemoryTypes.h"        // address_t, word_t, etc ...
#include "EventQueue.hpp"       // I/O device event queue simulation
#include "Memory.hpp"           // main memory emulation
#include "DECfile11.hpp"        // PDP-11 absolute loader file routines
#include "Device.hpp"           // CDevice I/O device emulation objects
#include "DeviceMap.hpp"        // CDeviceMap memory mapped I/O emulation
#include "MemoryMap.hpp"        // memory mapping hardware
#include "Interrupt.hpp"        // interrupt simulation logic
#include "CPU.hpp"              // CCPU base class definitions
#include "PIC11.hpp"            // DCT11 interrupt emulation
#include "DCT11.hpp"            // T11 CPU emulation
#include "DCT11opcodes.hpp"     // one line assembler and disassembler
#include "LTC11.hpp"            // line time clock emulation
#include "DC319.hpp"            // DC319 DLART serial line unit emulation
#include "RTC.hpp"              // generic NVR declarations
#include "RTC11.hpp"            // real time clock
#include "DS12887.hpp"          // Dallas DS12887A NVR/RTC declarations
#include "i8255.hpp"            // Intel 8255 programmable peripheral interface
#include "PPI11.hpp"            // Centronics parallel port and POST display
#include "IDE.hpp"              // generic IDE disk drive emulation
#include "IDE11.hpp"            // SBCT11 IDE disk interface
#include "TU58.hpp"             // TU58 emulator
#include "UserInterface.hpp"    // declarations for this module


// LOAD/SAVE file format keywords ...
const CCmdArgKeyword::KEYWORD CUI::m_keysFileFormat[] = {
  {"BIN*ARY",   CUI::FILE_FORMAT_BINARY},
  {"IN*TEL",    CUI::FILE_FORMAT_INTEL},
  {"ABS*OLUTE", CUI::FILE_FORMAT_PAPERTAPE},
  {NULL, 0}
};


// Argument definitions ...
//   These objects define the arguments for all command line parameters as
// well as the arguments for command line modifiers that take a value.  The
// CCmdArgument objects don't distinguish between these two usages. 
//
//   Notice that these are shared by many commands - for example, the same
// g_UnitArg object is shared by every command that takes a unit number as an
// argument.  That's probably not the most elegant way, however it saves a lot
// of object definitions and, since only one command can ever be parsed at any
// one time, it's harmless.
//
//   One last note - none of these can be "const" even though you might want to
// make them so.  That's because the CCmdArgument objects store the results of
// the parse in the object itself.
CCmdArgFileName    CUI::m_argFileName("file name");
CCmdArgFileName    CUI::m_argOptFileName("file name", true);
CCmdArgKeyword     CUI::m_argFileFormat("format", m_keysFileFormat);
CCmdArgNumberRange CUI::m_argAddressRange("address range", 8, 0, ADDRESS_MAX);
CCmdArgName        CUI::m_argRegisterName("register name");
CCmdArgRangeOrName CUI::m_argExamineDeposit("name or range", 8, 0, ADDRESS_MAX);
CCmdArgList        CUI::m_argRangeOrNameList("name or range list", m_argExamineDeposit);
CCmdArgList        CUI::m_argRangeList("address range list", m_argAddressRange);
CCmdArgNumber      CUI::m_argData("data", 8, 0, UINT16_MAX);
CCmdArgList        CUI::m_argDataList("data list", m_argData);
CCmdArgNumber      CUI::m_argStepCount("step count", 10, 1, INT16_MAX, true);
CCmdArgNumber      CUI::m_argRunAddress("run address", 8, 0, ADDRESS_MAX, true);
CCmdArgNumber      CUI::m_argBreakpoint("breakpoint address", 8, 0, ADDRESS_MAX);
CCmdArgNumber      CUI::m_argOptBreakpoint("breakpoint address", 8, 0, ADDRESS_MAX, true);
CCmdArgNumber      CUI::m_argPollDelay("poll delay", 10, 1, 1000000UL);
CCmdArgNumber      CUI::m_argBreakChar("break character", 10, 1, 31);
CCmdArgNumber      CUI::m_argBaseAddress("starting address", 8, 0, ADDRESS_MAX);
CCmdArgNumber      CUI::m_argByteCount("byte count", 10, 0, ADDRESS_MAX);
CCmdArgNumber      CUI::m_argCPUmode("CPU mode", 8, 0, 0177777);
CCmdArgName        CUI::m_argOptDeviceName("device", true);
CCmdArgNumber      CUI::m_argUnit("unit", 10, 0, 255);
CCmdArgNumber      CUI::m_argCapacity("capacity", 10, 1, UINT32_MAX);
CCmdArgNumber      CUI::m_argDelay("delay (ms)", 10, 1, 1000000UL);
CCmdArgList        CUI::m_argDelayList("delay list", m_argDelay, true);

// Modifier definitions ...
//   Like command arguments, modifiers may be shared by several commands...-
CCmdModifier CUI::m_modFileFormat("FORM*AT", NULL, &m_argFileFormat);
CCmdModifier CUI::m_modInstruction("I*NSTRUCTION");
CCmdModifier CUI::m_modPollDelay("POLL", NULL, &m_argPollDelay);
CCmdModifier CUI::m_modBreakChar("BRE*AK", NULL, &m_argBreakChar);
CCmdModifier CUI::m_modCPUmode("MO*DE", NULL, &m_argCPUmode);
CCmdModifier CUI::m_modBaseAddress("BAS*E", NULL, &m_argBaseAddress);
CCmdModifier CUI::m_modByteCount("COU*NT", NULL, &m_argByteCount);
CCmdModifier CUI::m_modWordByte("W*ORD", "B*YTE");
CCmdModifier CUI::m_modROM("RO*M", "RA*M");
CCmdModifier CUI::m_modNVR("NVR");
CCmdModifier CUI::m_modReadOnly("RE*AD", "WR*ITE");
CCmdModifier CUI::m_modUnit("UN*IT", NULL, &m_argUnit);
CCmdModifier CUI::m_modOverwrite("OVER*WRITE", "NOOVER*WRITE");
CCmdModifier CUI::m_modCapacity("CAP*ACITY", NULL, &m_argCapacity);
CCmdModifier CUI::m_modClose("CL*OSE", NULL);
CCmdModifier CUI::m_modText("TE*XT", NULL);
CCmdModifier CUI::m_modXModem("X*MODEM", NULL);
CCmdModifier CUI::m_modAppend("APP*END", "OVER*WRITE");
CCmdModifier CUI::m_modCRLF("CRLF", "NOCRLF");
CCmdModifier CUI::m_modDelayList("DEL*AY", NULL, &m_argDelayList);

// LOAD and SAVE commands ...
CCmdArgument * const CUI::m_argsLoadSave[] = {&m_argFileName, NULL};
CCmdModifier * const CUI::m_modsLoadSave[] = {&m_modFileFormat, &m_modBaseAddress,
                                              &m_modByteCount, &m_modROM, &m_modNVR, NULL};
CCmdVerb CUI::m_cmdLoad("LO*AD", &DoLoad, m_argsLoadSave, m_modsLoadSave);
CCmdVerb CUI::m_cmdSave("SA*VE", &DoSave, m_argsLoadSave, m_modsLoadSave);

// ATTACH and DETACH commands ...
CCmdArgument * const CUI::m_argsAttach[] = {&m_argFileName, NULL};
CCmdModifier * const CUI::m_modsAttachDisk[] = {&m_modCapacity, &m_modUnit, NULL};
CCmdModifier * const CUI::m_modsAttachTape[] = {&m_modReadOnly, &m_modUnit,
                                                &m_modCapacity, NULL};
CCmdModifier * const CUI::m_modsDetachTape[] = {&m_modUnit, NULL};
CCmdVerb CUI::m_cmdAttachDisk("DI*SK", &DoAttachDisk, m_argsAttach, m_modsAttachDisk);
CCmdVerb CUI::m_cmdDetachDisk("DI*SK", &DoDetachDisk, NULL, m_modsDetachTape);
CCmdVerb CUI::m_cmdAttachTape("TA*PE", &DoAttachTape, m_argsAttach, m_modsAttachTape);
CCmdVerb CUI::m_cmdDetachTape("TA*PE", &DoDetachTape, NULL, m_modsDetachTape);
CCmdVerb * const CUI::g_aAttachVerbs[] = {
  &m_cmdAttachDisk, &m_cmdAttachTape, NULL
};
CCmdVerb * const CUI::g_aDetachVerbs[] = {
  &m_cmdDetachDisk, &m_cmdDetachTape, NULL
};
CCmdVerb CUI::m_cmdAttach("ATT*ACH", NULL, NULL, NULL, g_aAttachVerbs);
CCmdVerb CUI::m_cmdDetach("DET*ACH", NULL, NULL, NULL, g_aDetachVerbs);

// EXAMINE and DEPOSIT verb definitions ...
CCmdArgument * const CUI::m_argsExamine[] = {&m_argRangeOrNameList, NULL};
CCmdArgument * const CUI::m_argsDeposit[] = {&m_argExamineDeposit, &m_argDataList, NULL};
CCmdModifier * const CUI::m_modsExamine[] = {&m_modInstruction, &m_modWordByte, &m_modROM, NULL};
CCmdModifier * const CUI::m_modsDeposit[] = {&m_modWordByte, NULL};
CCmdVerb CUI::m_cmdDeposit("D*EPOSIT", &DoDeposit, m_argsDeposit, m_modsDeposit);
CCmdVerb CUI::m_cmdExamine("E*XAMINE", &DoExamine, m_argsExamine, m_modsExamine);

// SET, CLEAR and SHOW BREAKPOINT commands ...
CCmdModifier * const CUI::m_modsRAMROM[] = {&m_modROM, NULL};
CCmdArgument * const CUI::m_argsSetBreakpoint[] = {&m_argBreakpoint, NULL};
CCmdArgument * const CUI::m_argsClearBreakpoint[] = {&m_argOptBreakpoint, NULL};
CCmdVerb CUI::m_cmdSetBreakpoint("BRE*AKPOINT", &DoSetBreakpoint, m_argsSetBreakpoint, m_modsRAMROM);
CCmdVerb CUI::m_cmdClearBreakpoint("BRE*AKPOINT", &DoClearBreakpoint, m_argsClearBreakpoint, m_modsRAMROM);
CCmdVerb CUI::m_cmdShowBreakpoint("BRE*AKPOINT", &DoShowBreakpoints, NULL, m_modsRAMROM);

// RUN, CONTINUE, STEP, RESET and HALT commands ...
CCmdArgument * const CUI::m_argsStep[] = {&m_argStepCount, NULL};
CCmdArgument * const CUI::m_argsRun[] = {&m_argRunAddress, NULL};
CCmdVerb CUI::m_cmdRun("RU*N", &DoRun, m_argsRun, NULL);
CCmdVerb CUI::m_cmdContinue("C*ONTINUE", &DoContinue);
CCmdVerb CUI::m_cmdStep("ST*EP", &DoStep, m_argsStep, NULL);
CCmdVerb CUI::m_cmdReset("RE*SET", &DoReset);
CCmdVerb CUI::m_cmdHalt("HA*LT", &DoHalt);

// SET, CLEAR and SHOW CPU ...
CCmdModifier * const CUI::m_modsSetCPU[] = {&m_modBreakChar, &m_modCPUmode, NULL};
CCmdVerb CUI::m_cmdSetCPU = {"CPU", &DoSetCPU, NULL, m_modsSetCPU};
CCmdVerb CUI::m_cmdClearCPU("CPU", &DoClearCPU);
CCmdVerb CUI::m_cmdShowCPU("CPU", &DoShowCPU);

// CLEAR and SHOW MEMORY ...
CCmdVerb CUI::m_cmdClearMemory("MEM*ORY", &DoClearMemory, NULL, m_modsRAMROM);
CCmdVerb CUI::m_cmdShowMemory("MEM*ORY", &DoShowMemory);

// CLEAR and SHOW DEVICE ...
CCmdArgument * const CUI::m_argsShowDevice[] = {&m_argOptDeviceName, NULL};
CCmdVerb CUI::m_cmdShowDevice("DEV*ICES", &DoShowDevice, m_argsShowDevice, NULL);
CCmdVerb CUI::m_cmdClearDevice("DEV*ICES", &DoClearDevice, m_argsShowDevice, NULL);

// CLEAR verb definition ...
CCmdVerb * const CUI::g_aClearVerbs[] = {
    &m_cmdClearBreakpoint, &m_cmdClearCPU, &m_cmdClearMemory, &m_cmdClearDevice, NULL
  };
CCmdVerb CUI::m_cmdClear("CL*EAR", NULL, NULL, NULL, g_aClearVerbs);

// SET verb definition ...
CCmdVerb * const CUI::g_aSetVerbs[] = {
    &m_cmdSetBreakpoint, &m_cmdSetCPU,
    &CStandardUI::m_cmdSetLog, &CStandardUI::m_cmdSetWindow,
#ifdef THREADS
    &CStandardUI::m_cmdSetCheckpoint,
#endif
    NULL
  };
CCmdVerb CUI::m_cmdSet("SE*T", NULL, NULL, NULL, g_aSetVerbs);

// SHOW verb definition ...
CCmdVerb CUI::m_cmdShowTime("TI*ME", &DoShowTime);
CCmdVerb CUI::m_cmdShowVersion("VER*SION", &DoShowVersion);
CCmdVerb CUI::m_cmdShowTape("TA*PE", &DoShowTape);
CCmdVerb CUI::m_cmdShowDisk("DI*SK", &DoShowDisk);
CCmdVerb * const CUI::g_aShowVerbs[] = {
    &m_cmdShowBreakpoint, &m_cmdShowMemory, &m_cmdShowDevice, &m_cmdShowCPU,
    &m_cmdShowDisk, &m_cmdShowTape, &m_cmdShowTime, &m_cmdShowVersion,
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
  &m_cmdHalt, &m_cmdSet, &m_cmdShow, &m_cmdClear, 
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
  //   Figure out which memory space is required - /ROM selects the EPROM
  // space, and /RAM (or no switch at all) selects RAM.  This is used by a
  // number of commands - LOAD, SAVE, EXAMINE, DEPOSIT, breakpoints, clear,
  // etc...
  // 
  //   Note that the /NVR modifier is also allowed by many of these commands,
  // but that's handled separately.  NVR unfortunately isn't a CGenericMemory
  // object and we can't deal with it as we do the other memory spaces.
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
  // and SAVE commands.  For the PDP11 three file types are supported - Intel
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
  //   And lastly, if there was no /FORMAT and no extension specified (e.g.
  // "LOAD FOO") then we'll try to figure out the type by first looking for
  // a "FOO.HEX", a "FOO.BIN" and then a "FOO.PTP", in that order.  If one of
  // those exists then we'll go with that one, and if none exists then it's
  // an error.
  //--
  sFileName = m_argFileName.GetFullPath();
  nFormat = FILE_FORMAT_NONE;

  // Try to figure out the extension and format ...
  if (m_modFileFormat.IsPresent()) {
    // /FORMAT was specified!
    nFormat = (int) m_argFileFormat.GetKeyValue();
    if (nFormat == FILE_FORMAT_BINARY)
      sFileName = CCmdParser::SetDefaultExtension(sFileName, DEFAULT_BINARY_FILE_TYPE);
    else if (nFormat == FILE_FORMAT_INTEL)
      sFileName = CCmdParser::SetDefaultExtension(sFileName, DEFAULT_INTEL_FILE_TYPE);
    else
      sFileName = CCmdParser::SetDefaultExtension(sFileName, DEFAULT_PAPERTAPE_FILE_TYPE);
  } else {
    string sDrive, sDir, sName, sExt;
    SplitPath(sFileName, sDrive, sDir, sName, sExt);
    if (sExt.empty() && !fCreate) {
      // No extension given - try searching for .hex, .bin or .ptp ...
      if (FileExists(MakePath(sDrive, sDir, sName, DEFAULT_BINARY_FILE_TYPE))) {
        sFileName = MakePath(sDrive, sDir, sName, DEFAULT_BINARY_FILE_TYPE);
        nFormat = FILE_FORMAT_BINARY;
      } else if (FileExists(MakePath(sDrive, sDir, sName, DEFAULT_INTEL_FILE_TYPE))) {
        sFileName = MakePath(sDrive, sDir, sName, DEFAULT_INTEL_FILE_TYPE);
        nFormat = FILE_FORMAT_INTEL;
      } else if (FileExists(MakePath(sDrive, sDir, sName, DEFAULT_PAPERTAPE_FILE_TYPE))) {
        sFileName = MakePath(sDrive, sDir, sName, DEFAULT_PAPERTAPE_FILE_TYPE);
        nFormat = FILE_FORMAT_PAPERTAPE;
      }
    } else if (sExt == DEFAULT_BINARY_FILE_TYPE) {
      nFormat = FILE_FORMAT_BINARY;
    } else if (sExt == DEFAULT_INTEL_FILE_TYPE) {
      nFormat = FILE_FORMAT_INTEL;
    } else if (sExt == DEFAULT_PAPERTAPE_FILE_TYPE) {
      nFormat = FILE_FORMAT_PAPERTAPE;
    }
  }

  // If we still don't know the format then assume binary ...
  if (nFormat == FILE_FORMAT_NONE) {
    nFormat = FILE_FORMAT_BINARY;
    CMDERRS("BINARY format assumed for " << sFileName);
  }
}

void CUI::GetImageBaseAndOffset (address_t&wBase, size_t &cbBytes)
{
  //++
  //   This method will try to figure out the starting address (aka the base)
  // and the size (in bytes) of the memory region to be loaded or saved.  When
  // we're saving it's pretty straight forward and these two parameters
  // determine exactly what gets written.  When we're loading it's a little
  // more complicated, however.  Binary files don't contain any address 
  // information, so the base determines where in memory the file will be 
  // loaded.  Intel hex files do contain an address, and the base is added
  // to the address specified by the file.  This is handy for things like the
  // ELF2K EPROM, where the hex file is origined at 0x0000, but the code is
  // intended to run at 0x8000.  
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
    cbBytes = ADDRESS_MAX-wBase;
}

bool CUI::DoLoadNVR(CCmdParser& cmd)
{
  //++
  //   This routine gets called for the "LOAD/NVR ..." command.  The only file
  // format allowed is binary, and the other /RAM, /ROM, /BASE, /COUNT, etc
  // modifers aren't allowed.
  //--
  assert(g_pRTC != NULL);
  if (m_modROM.IsPresent() || m_modBaseAddress.IsPresent() || m_modByteCount.IsPresent() ||
     (m_modFileFormat.IsPresent() && (m_argFileFormat.GetKeyValue() != FILE_FORMAT_BINARY))) {
    CMDERRS("Conflicting qualifiers for LOAD /NVR");  return false;
  }
  string sFileName = m_argFileName.GetFullPath();
  sFileName = CCmdParser::SetDefaultExtension(sFileName, DEFAULT_BINARY_FILE_TYPE);
  size_t nBytes = g_pRTC->Get12887()->LoadNVR(sFileName);
  CMDOUTF("%d bytes loaded from %s", nBytes, sFileName.c_str());
  return true;
}

bool CUI::DoLoad (CCmdParser &cmd)
{
  //++
  //   The LOAD command loads memory from a disk file in Intel HEX format, 
  // plain binary or DEC PDP11 absolute loader format.
  //--
  string sFileName;  int nFormat;  int32_t nBytes = 0;
  if (m_modNVR.IsPresent()) return DoLoadNVR(cmd);
  GetImageFileNameAndFormat(false, sFileName, nFormat);
  CGenericMemory *pMemory = GetMemorySpace();

  // Get the address range to be loaded ...
  address_t wBase = 0;  size_t cbLimit = 0;
  GetImageBaseAndOffset(wBase, cbLimit);
  switch (nFormat) {
    case FILE_FORMAT_BINARY:
       nBytes = pMemory->LoadBinary(sFileName, wBase, cbLimit);  break;
    case FILE_FORMAT_INTEL:
      nBytes = pMemory->LoadIntel(sFileName, wBase, cbLimit);  break;
    case FILE_FORMAT_PAPERTAPE:
      nBytes = CDECfile11::LoadPaperTape(pMemory, sFileName);  break;
  }

  // And we're done!
  if (nBytes < 0) return false;
  CMDOUTF("%d bytes loaded from %s", nBytes, sFileName.c_str());
  return true;
}

bool CUI::DoSaveNVR (CCmdParser& cmd)
{
  //++
  //   This routine gets called for the "SAVE/NVR ..." command.  The only file
  // format allowed is binary, and the other /RAM, /ROM, /BASE, /COUNT, etc
  // modifers aren't allowed.
  //--
  assert(g_pRTC != NULL);
  if (m_modROM.IsPresent() || m_modBaseAddress.IsPresent() || m_modByteCount.IsPresent() ||
     (m_modFileFormat.IsPresent() && (m_argFileFormat.GetKeyValue() != FILE_FORMAT_BINARY))) {
    CMDERRS("Conflicting qualifiers for SAVE /NVR");  return false;
  }
  string sFileName = m_argFileName.GetFullPath();
  sFileName = CCmdParser::SetDefaultExtension(sFileName, DEFAULT_BINARY_FILE_TYPE);
  size_t nBytes = g_pRTC->Get12887()->SaveNVR(sFileName);
  CMDOUTF("%d bytes saved to %s", nBytes, sFileName.c_str());
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
  if (m_modNVR.IsPresent()) return DoSaveNVR(cmd);
  GetImageFileNameAndFormat(false, sFileName, nFormat);
  CGenericMemory* pMemory = GetMemorySpace();

  // Save RAM or ROM ...
  address_t wBase = 0;  size_t cbBytes = 0;
  GetImageBaseAndOffset(wBase, cbBytes);
  if (FileExists(sFileName)) {
    if (!cmd.AreYouSure(sFileName + " already exists")) return false;
  }
  switch (nFormat) {
    case FILE_FORMAT_BINARY:
      nBytes = pMemory->SaveBinary(sFileName, wBase, cbBytes);  break;
    case FILE_FORMAT_INTEL:
      nBytes = pMemory->SaveIntel(sFileName, wBase, cbBytes);  break;
    case FILE_FORMAT_PAPERTAPE:
      nBytes = CDECfile11::SavePaperTape(pMemory, sFileName, wBase, cbBytes);  break;
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


////////////////////////////////////////////////////////////////////////////////
///////////////////////// EXAMINE and DEPOSIT COMMANDS /////////////////////////
////////////////////////////////////////////////////////////////////////////////

//++
// Special PDP11 helper macros for 16 bit arithmetic ....
//--
static inline uint16_t ADD16 (uint16_t  v, uint16_t i=1) {return MASK16(v+i);}
static inline uint16_t SUB16 (uint16_t  v, uint16_t d=1) {return MASK16(v-d);}
static inline uint16_t INC16 (uint16_t &v, uint16_t i=1) {return v = MASK16(v+i);}
static inline uint16_t DEC16 (uint16_t &v, uint16_t d=1) {return v = MASK16(v-d);}

//++
// Special PDP11 helper macros for dealing with words ...
//   Remember that the T11 does NOT have an odd address trap, and a word access
// to memory simply ignores the LSB.  PDP11s are little endian machines, so the
// low order byte is always at the even address and the high byte is at the odd
// address.
//--
static inline uint8_t  MEMRDB (const CGenericMemory *p, address_t a) {return p->UIread(a);}
static inline uint16_t MEMRDW (const CGenericMemory *p, address_t a)
  {uint8_t l = MEMRDB(p, (a & 0177776));  uint8_t h = MEMRDB(p, (a | 1));  return MKWORD(h, l);}
static inline void MEMWRB (CGenericMemory *p, address_t a, uint8_t b) {p->UIwrite(a, b);}
static inline void MEMWRW (CGenericMemory *p, address_t a, uint16_t w)
  {MEMWRB(p, (a & 0177776), LOBYTE(w));  MEMWRB(p, (a | 1), HIBYTE(w));}


void CUI::DumpLine (address_t nStart, size_t cbBytes, unsigned nIndent, unsigned nPad)
{
  //++
  //   Dump out one line of memory contents, byte by byte and always in octal,
  // for the EXAMINE command.  The line can optionally be padded on the left
  // (nIndent > 0) or the right (nPad > 0) so that we can line up rows that
  // don't start on a multiple of 16.
  //--
  char szLine[128];  unsigned i;
  CGenericMemory *pMemory = GetMemorySpace();

  // In word mode, round off the address and count to multiples of 2 ...
  bool fByte = m_modWordByte.IsPresent() && m_modWordByte.IsNegated();
  address_t nByteStart = nStart;
  if (!fByte) {
    if (ISSET(nStart, 1))  --nStart;
    if (ISSET(cbBytes, 1)) ++cbBytes;
  }

  // Print the address, and indent if necessary ...
  sprintf_s(szLine, sizeof(szLine), "%06o/ ", nStart);
  if (fByte) {
    for (i = 0; i < nIndent; ++i) strcat_s(szLine, sizeof(szLine), "    ");
  } else {
    for (i = 0; i < nIndent/2; ++i) strcat_s(szLine, sizeof(szLine), "       ");
  }

  // Now dump the data in octal, either by bytes or words ...
  for (i = 0; i < cbBytes;) {
    char sz[16];
    if (fByte) {
      sprintf_s(sz, sizeof(sz), "%03o ", MEMRDB(pMemory, nStart+i));  i += 1;
    } else {
      sprintf_s(sz, sizeof(sz), "%06o ", MEMRDW(pMemory, nStart+i));  i += 2;
    }
    strcat_s(szLine, sizeof(szLine), sz);
  }

  // If the last line wasn't full, then pad it out ...
  if (fByte) {
    for (i = 0; i < nPad; ++i) strcat_s(szLine, sizeof(szLine), "    ");
  } else {
    for (i = 0; i < nPad/2; ++i) strcat_s(szLine, sizeof(szLine), "       ");
  }

  // Now dump the same data, but in ASCII ...
  if (cbBytes > 1) {
    strcat_s(szLine, sizeof(szLine), "\t");
    for (i = 0; i < nIndent; ++i) strcat_s(szLine, sizeof(szLine), " ");
    for (i = 0; i < cbBytes; ++i) {
      char sz[2];  uint8_t b = MEMRDB(pMemory, nByteStart+i) & 0x7F;
      sz[0] = isprint(b) ? b : '.';  sz[1] = 0;
      strcat_s(szLine, sizeof(szLine), sz);
    }
  }
  CMDOUTS(szLine);
}

void CUI::DoExamineRange (address_t nStart, address_t nEnd)
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
  /*if (nStart == nEnd) {
    CMDOUTF("%04X/ %02X", nStart, g_pCPU->GetMemory().UIread(nStart));
  } else*/ if ((nEnd-nStart) < 16) {
    DumpLine(nStart, nEnd-nStart+1);
  } else {
    if ((nStart & 0xF) != 0) {
      address_t nBase = nStart & 0xFFF0;
      address_t nOffset = nStart - nBase;
      DumpLine(nStart, 16-nOffset, (unsigned) nOffset);
      nStart += 16-nOffset;
    }
    for (; nStart <= nEnd; nStart += 16) {
      if ((nEnd-nStart) < 16)
        DumpLine(nStart, nEnd-nStart+1, 0, (unsigned) (16-(nEnd-nStart+1)));
      else
        DumpLine(nStart, 16);
    }
  }
}

uint16_t CUI::DoExamineInstruction (string &sCode, uint16_t wLoc, CGenericMemory *pMemory)
{
  //++
  //   This method will disassemble one instruction for the EXAMINE/INSTRUCTION
  // command.  Since instructions are variable length, this can potentially
  // examine 1, 2 or 3 words of memory.  The actual number of bytes (twice the
  // number of words) used is returned.
  //
  //   Note that we always disassemble PDP-11 instructions in octal and word
  // mode, just as it would appear in the assembler listing.  Hex and bytes
  // might be cute, but they were never used here.
  //--
  uint16_t wOpcode, w2, w3;

  // Disassemble the opcode and fetch any operands ...
  uint16_t wCount = Disassemble(pMemory, wLoc, sCode);
  wOpcode = MEMRDW(pMemory, wLoc);
  if (wCount > 2) w2 = MEMRDW(pMemory, ADD16(wLoc, 2));
  if (wCount > 4) w3 = MEMRDW(pMemory, ADD16(wLoc, 4));

  // Print it out neatly ...
  if (wCount <= 2)
    sCode = FormatString("%06o/ %06o\t\t\t%s", wLoc, wOpcode, sCode.c_str());
  else if (wCount <= 4)
    sCode = FormatString("%06o/ %06o %06o\t\t%s", wLoc, wOpcode, w2, sCode.c_str());
  else
    sCode = FormatString("%06o/ %06o %06o %06o\t%s", wLoc, wOpcode, w2, w3, sCode.c_str());

  // Return the number of bytes disassembled and we're done...
  return wCount;
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
  if (nRegister == CDCT11::REG_PSW) {
    string sPSW = FormatString("PSW/%03o ", g_pCPU->GetRegister(nRegister));
    return sPSW + g_pCPU->GetPSW();
  } else {
    unsigned nSize = (g_pCPU->GetRegisterSize(nRegister)+2) / 3;
    uint16_t nValue = g_pCPU->GetRegister(nRegister);
    return FormatString("%s/%0*o", paNames[nIndex].m_pszName, nSize, nValue);
  }
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

void CUI::DoExamineAllRegisters()
{
  //++
  //   Print the contents of ALL internal CPU registers (formatted as neatly
  // as we can without knowing too much about them!)...
  //--
  string sLine;
  const CCmdArgKeyword::KEYWORD *paNames = g_pCPU->GetRegisterNames();
  for (int i = 0; paNames[i].m_pszName != NULL; ++i) {
    string sRegister = ExamineRegister(i);
    if ((sLine.length()+sRegister.length()) > 60) {
      CMDOUTS(sLine);  sLine.clear();
    }
    sLine += sRegister + (i < 16 ? "  " : " ");
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
  for (size_t i = 0; i < m_argRangeOrNameList.Count(); ++i) {
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
      string sCode;
      if (m_modInstruction.IsPresent()) {
        while (nStart <= nEnd) {
          nStart += DoExamineInstruction(sCode, nStart, GetMemorySpace());
          CMDOUTS(sCode);
        }
      } else
        DoExamineRange(nStart, nEnd);
    }
  }
  return true;
}

bool CUI::DoDepositRange (address_t nStart, address_t nEnd, CCmdArgList &List)
{
  //++
  //   Deposit one or more bytes into main memory starting from nStart and
  // proceeding to successively higher addresses.  If the number of data items
  // would cause nEnd to be exceeded, then give an error message and quit.  nEnd
  // is otherwise ignored - i.e. it's not an error to specify too few items!
  //--
  CGenericMemory *pMemory = GetMemorySpace();
  bool fByte = m_modWordByte.IsPresent() && m_modWordByte.IsNegated();
  bool fHasEnd = nStart != nEnd;
  if (!fByte && ISSET(nStart, 1)) --nStart;
  for (size_t i = 0;  i < List.Count();  ++i) {
    if (fHasEnd && (nStart > nEnd)) {
      CMDERRS("too many data items to deposit");  return false;
    }
    CCmdArgNumber *pData = dynamic_cast<CCmdArgNumber *> (List[i]);
    if (fByte) {
      MEMWRB(pMemory, nStart, pData->GetNumber());  ++nStart;
    } else {
      MEMWRW(pMemory, nStart, pData->GetNumber());  nStart += 2;
    }
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
    return DoDepositRange(nStart, nEnd, m_argDataList);
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
  assert(g_pCPU != NULL);

  // Figure out the magic character used to break emulation.
  if (nSteps == 0) {
    CMDOUTF("[Simulation started.  Type CONTROL+%c to break.]", (g_pConsole->GetConsoleBreak()+'@'));
  }

  // Now run the simulation ...
  CCPU::STOP_CODE nStop = g_pCPU->Run(nSteps);
  if (nSteps == 0) CMDOUTS("");

  // Decode the reason we stopped ...
  switch (nStop) {
    case CCPU::STOP_HALT:
      CMDERRF("halt at 0%06o", g_pCPU->GetLastPC());  break;
    case CCPU::STOP_ENDLESS_LOOP:
      CMDERRF("endless loop at 0%06o", g_pCPU->GetPC());  break;
    case CCPU::STOP_BREAKPOINT:
      CMDERRF("breakpoint at 0%06o", g_pCPU->GetPC());  break;
    case CCPU::STOP_BREAK:
      CMDERRF("break at 0%06o", g_pCPU->GetPC());  break;
    default:  break;
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
    g_pCPU->SetPC(m_argRunAddress.GetNumber());
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
  assert(g_pCPU != NULL);
  unsigned nCount = 1;
  if (m_argStepCount.IsPresent()) nCount = m_argStepCount.GetNumber();
  assert(nCount > 0);
  while (nCount-- > 0) {
    address_t wPC = g_pCPU->GetPC();
    CMemoryMap::CHIP_SELECT nSelect = CMemoryMap::ChipSelect(wPC, g_pMCR->IsRAM(), false);
    if ((nSelect != CMemoryMap::CS_RAM) && (nSelect != CMemoryMap::CS_ROM)) {
      CMDERRF("PC address %06o is not in RAM or ROM!", wPC);  return false;
    }
    CGenericMemory *pMemory = (nSelect == CMemoryMap::CS_RAM) ? g_pRAM : g_pROM;
    string sCode;
    DoExamineInstruction(sCode, g_pCPU->GetPC(), pMemory);
    CMDOUTS(CMemoryMap::GetChipName(nSelect) << " " << sCode);
    unsigned  nStop = RunSimulation(1);
    if (nStop != CCPU::STOP_FINISHED) return false;
    DoExamineAllRegisters();
  }
  return true;
}

bool CUI::DoReset (CCmdParser &cmd)
{
  //++
  // Reset everything (CPU and all devices) ...
  //--
  assert((g_pCPU != NULL) && (g_pIOpage != NULL));
  g_pCPU->MasterClear();
  g_pIOpage->ClearAll();
  return true;
}

bool CUI::DoHalt (CCmdParser &cmd)
{
  //++
  //   The HALT command is equivalent to toggling the RUN/HALT switch on the
  // SBCT11.  It tells the DCT11 to HALT, which doesn't really halt but instead
  // traps to the firmware ...
  //--
  assert(g_pCPU != NULL);
  g_pCPU->HaltRequest();
  return true;
}


////////////////////////////////////////////////////////////////////////////////
///////////////////////////// BREAKPOINT COMMANDS //////////////////////////////
////////////////////////////////////////////////////////////////////////////////

bool CUI::DoSetBreakpoint (CCmdParser &cmd)
{
  //++
  //   The "SET BREAKPOINT" command will (what else??) set a breakpoint at the
  // specified address.  Breakpoints may be set in either RAM or ROM address
  // space, as selected by the /RAM or /ROM qualifiers.  RAM is the default if
  // neither is specified.  Note that there's no error message if you set
  // a breakpoint at an address that already has a breakpoint, and only one
  // breakpoint is actually set.
  //--
  assert((g_pRAM != NULL)  &&  (g_pROM != NULL));
  CGenericMemory *pMemory = GetMemorySpace();
  address_t nBreak = m_argBreakpoint.GetNumber();
  pMemory->SetBreak(nBreak);
  return true;
}

bool CUI::DoClearBreakpoint (CCmdParser &cmd)
{
  //++
  //   The "CLEAR BREAKPOINT [oooooo]" command will remove the breakpoint at
  // the specified address or, if no address is specified, it will remove all
  // breakpoints.  Breakpoints may be removed from either RAM or ROM, and RAM
  // is the default if no qualifier is specified.  Note that there's no error
  // message if you ask it to clear a breakpoint that doesn't exist!
  //--
  assert((g_pRAM != NULL)  &&  (g_pROM != NULL));
  if (m_argOptBreakpoint.IsPresent()) {
    CGenericMemory *pMemory = GetMemorySpace();
    pMemory->SetBreak(m_argOptBreakpoint.GetNumber(), false);
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
  // List breakpoints in the specified memory space ...
  //--
  string sBreaks;  address_t nLoc = pMemory->Base()-1;
  while (pMemory->FindBreak(nLoc)) {
    sBreaks += sBreaks.empty() ? "Breakpoint(s) at " : ", ";
    char sz[16];  sprintf_s(sz, sizeof(sz), "%06o", nLoc);
    sBreaks += sz;
  }
  return (sBreaks.empty()) ? "none" : sBreaks;
}

bool CUI::DoShowBreakpoints (CCmdParser &cmd)
{
  //++
  //   List all current breakpoints, in RAM, ROM or both ...
  //--
  assert((g_pRAM != NULL)  &&  (g_pROM != NULL));
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
  // Clear (reset!) the CPU but NOT any peripherals ...
  //--
  g_pCPU->MasterClear();
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
  CMDOUTF("%s %s %3.2fMHz MODE=%06o BREAK=^%c",
    g_pCPU->GetName(), g_pCPU->GetDescription(),
    flCrystal, g_pCPU->GetMode(), (g_pConsole->GetConsoleBreak()+'@'));
  double flMicrocycle = (g_pCPU->IsLMC() ? 4.0 : 3.0) * 1000.0 / flCrystal;
  
  // Show simulated CPU time ...
  uint64_t llTime = NSTOMS(g_pCPU->ElapsedTime());
  uint32_t lMilliseconds = llTime % 1000ULL;  llTime /= 1000ULL;
  uint32_t lSeconds      = llTime % 60ULL;    llTime /= 60ULL;
  uint32_t lMinutes      = llTime % 60ULL;    llTime /= 60ULL;
  uint32_t lHours        = llTime % 24ULL;    llTime /= 24ULL;
  uint32_t lDays         = (uint32_t) llTime;
  CMDOUTF("CPU time %ldd %02ld:%02ld:%02ld.%03ld (%3.2fns per microcycle)\n",
    lDays, lHours, lMinutes, lSeconds, lMilliseconds, flMicrocycle);

  // Show CPU registers ...
  CMDOUTS("REGISTERS");
  DoExamineAllRegisters();

  // Show interrupt status ...
  CMDOUTS(std::endl << "INTERRUPTS");
  for (CPIC11::IRQ_t i = CPIC11::IRQLEVELS; i > 0; --i) {
    CSimpleInterrupt *pInterrupt = (*g_pPIC)[i];
    if (!pInterrupt->IsAttached()) continue;
    CDevice *pDevice = g_pIOpage->Find(pInterrupt);
    CMDOUTF("CP%-2d BR%d vector %03o device %-5s is %s",
      i, g_pPIC->GetPriority(i)>>5, g_pPIC->GetVector(i),
      (pDevice != NULL) ? pDevice->GetName() : "",
      g_pPIC->GetLevel(i)->IsRequested() ? "REQUESTED" : "not requested");
  }

  // That's about all we know!
  CMDOUTS("");
  return true;
}

bool CUI::DoSetCPU (CCmdParser &cmd)
{
  //++
  //   SET CPU allows you to set the T11 mode register (which controls the
  // startup and halt/restart address!) and the break character ...
  //--
  assert(g_pCPU != NULL);
  if (m_modBreakChar.IsPresent())
    g_pConsole->SetConsoleBreak(m_argBreakChar.GetNumber());
  if (m_modCPUmode.IsPresent())
    g_pCPU->SetMode(m_argCPUmode.GetNumber());
  return true;
}


////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// MEMORY COMMANDS ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

bool CUI::DoClearMemory(CCmdParser &cmd)
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
  //   The SHOW MEMORY command will print a memory map of the SBCT11.  Rather
  // than just hardwire this information (it's fixed after all, and can't
  // really change) we'll attempt to discover it by exercising the ChipSelect()
  // method in the CMemoryMap class.  This is the actual routine that the CPU
  // uses to figure out which memory space should be selected by a given memory
  // address.
  // 
  //   Note that ChipSelect() has two modes, RAM mode and ROM mode, as selected
  // by the RAM bit in the MCR.  We'll show the results for both cases.
  //--
  address_t nStart=0, nCurrent=0;  bool fLast=false;
  CMemoryMap::CHIP_SELECT nLastRAM, nLastROM;
  nLastRAM = CMemoryMap::ChipSelect(nStart, true, true);
  nLastROM = CMemoryMap::ChipSelect(nStart, false, true);
  CMDOUTF("\n   ADDRESS      RAM MODE  ROM MODE");
  CMDOUTF("--------------  --------  --------");
  while (!fLast) {
    fLast = (nCurrent == 0177776);  nCurrent += 2;
    CMemoryMap::CHIP_SELECT nRAM = CMemoryMap::ChipSelect(nCurrent, true, true);
    CMemoryMap::CHIP_SELECT nROM = CMemoryMap::ChipSelect(nCurrent, false, true);
    if ((nRAM != nLastRAM) || (nROM != nLastROM) || fLast) {
      CMDOUTF("%06o..%06o  %-8s  %-8s", nStart, ADDRESS(nCurrent-1),
        CMemoryMap::GetChipName(nLastRAM), CMemoryMap::GetChipName(nLastROM));
      nStart = nCurrent;  nLastRAM = nRAM;  nLastROM = nROM;
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
  //--
  CDevice *pDevice = g_pIOpage->Find(sDevice);
  if (pDevice != NULL) return pDevice;
  CMDERRS("No such device as " << sDevice);
  return NULL;
}

void CUI::ShowOneDevice (bool fHeading, const CDevice *pDevice)
{
  //++
  //   Show the short description (name, type, description, address and vector)
  // for a single device.  If fHeading is true, then print a heading first;
  // otherwise don't bother.
  //--
  assert(pDevice != NULL);
  if (fHeading) {
    CMDOUTF("\nNAME   TYPE     DESCRIPTION                     ADDRESS         VECTOR");
    CMDOUTF("-----  -------  ------------------------------  --------------  -------");
  }

  // Figure out the device address(es) and vector(s) ...
//address_t nBase = pDevice->GetBasePort();
//address_t cbSize = pDevice->GetPortCount();
//address_t nTop = nBase + cbSize - 1;
  CSimpleInterrupt* pInterruptA = pDevice->GetInterruptA();
  CSimpleInterrupt* pInterruptB = pDevice->GetInterruptB();
  CPIC11::IRQ_t nIRQA = g_pPIC->FindInterrupt(pInterruptA);
  CPIC11::IRQ_t nIRQB = g_pPIC->FindInterrupt(pInterruptB);
  address_t nVectorA = g_pPIC->GetVector(nIRQA);
  address_t nVectorB = g_pPIC->GetVector(nIRQB);

  // Format it all into a nice string ...
  string sDevice = FormatString("%-5s  %-7s  %-30s  %06o..%06o ",
    pDevice->GetName(), pDevice->GetType(), pDevice->GetDescription(),
    pDevice->GetBasePort(), pDevice->GetBasePort() + pDevice->GetPortCount()-1);
  if (nVectorA != 0) sDevice += FormatString(" %03o", nVectorA);
  if (nVectorB != 0) sDevice += FormatString(" %03o", nVectorB);

  // Print it and we're done!
  CMDOUTS(sDevice);
}

bool CUI::ShowAllDevices()
{
  //++
  //   Show a table with a short description of all the I/O devices in the
  // SBCT11.  We could just use an iterator to go thru the CDeviceMap set,
  // but that gives the devices in random order.  We'd prefer to have them
  // sorted by address, but there's nothing in CDeviceMap to do that.
  // 
  //   So we cheat a bit.  We just start with an address at the start of the
  // SBCT11 IOPAGE and scan upward until we find a device mapped to that
  // location.  We print that device, skip over the rest of the addresses
  // assigned to that device, and keep scanning.  Yes, it's a kludge, but it
  // works!
  //--
  bool fHeading = true;
  address_t nAddress = IOPAGE;
  while (nAddress < 0177776) {
    const CDevice *pDevice = g_pIOpage->Find(nAddress);
    if (pDevice != NULL) {
      ShowOneDevice(fHeading, pDevice);
      fHeading = false;  nAddress += pDevice->GetPortCount();
    } else
      nAddress += 2;
  }
  CMDOUTS("");  return true;
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
  ShowOneDevice(true, pDevice);
  ostringstream ofs;
  pDevice->ShowDevice(ofs);
  CMDOUTS("");  CMDOUT(ofs);  CMDOUTS("");
  return true;
}

bool CUI::DoClearDevice (CCmdParser &cmd)
{
  //++
  //   Clear (reset!) one or all devices but NOT the CPU.  This does the PDP11
  // equivalent of a BCLR, or the RESET instruction ...
  //--
  if (!m_argOptDeviceName.IsPresent()) {
    g_pIOpage->ClearAll();
  } else {
    CDevice *pDevice = FindDevice(m_argOptDeviceName.GetValue());
    if (pDevice == NULL) return false;
    pDevice->ClearDevice();
  }
  return true;
}

bool CUI::DoShowDisk (CCmdParser &cmd)
{
  //++
  // Show details for the IDE disk interface ...
  //--
  assert(g_pIDE != NULL);
  ostringstream ofs;
  ofs << std::endl;  g_pIDE->ShowDevice(ofs);  ofs << std::endl;
  CMDOUT(ofs);
  return true;
}

bool CUI::DoShowTape (CCmdParser &cmd)
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


////////////////////////////////////////////////////////////////////////////////
/////////////////////////// MISCELLANEOUS COMMANDS /////////////////////////////
////////////////////////////////////////////////////////////////////////////////

bool CUI::DoShowVersion (CCmdParser &cmd)
{
  //++
  // Show just the version number ...
  //--
  CMDOUTF("\nSBCT11 Emulator v%d\n", T11VER);
  return true;
}

bool CUI::DoShowTime (CCmdParser& cmd)
{
  //++
  // Show the elapsed simulation time ...
  //--
  assert(g_pCPU != NULL);
  uint64_t llTime = NSTOMS(g_pCPU->ElapsedTime());
  uint32_t lMilliseconds = llTime % 1000ULL;  llTime /= 1000ULL;
  uint32_t lSeconds      = llTime % 60ULL;    llTime /= 60ULL;
  uint32_t lMinutes      = llTime % 60ULL;    llTime /= 60ULL;
  uint32_t lHours        = llTime % 24ULL;    llTime /= 24ULL;
  uint32_t lDays         = (uint32_t) llTime;
  CMDOUTF("\nElapsed simulation time = %ld %02ld:%02ld:%02ld.%03ld\n",
    lDays, lHours, lMinutes, lSeconds, lMilliseconds);
  return true;
}

