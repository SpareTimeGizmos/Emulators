//++
// UserInterface.cpp -> MS2000 Emulator Specific User Interface code
//
//   COPYRIGHT (C) 2015-2024 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
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
//   This module implements the user interface specific to the MS2000 emulator
// process.  The first half of the file are parse tables for the generic command
// line parser classes from CommandParser.cpp, and the second half is the action
// routines needed to implement these commands.  
//
//                                                    Bob Armstrong [16-JUN-2022]
//
// MS2000 COMMANDS
// ---------------
//   LO*AD filename             - load binary or .HEX file into memory
//   SA*VE filename             - save memory to a binary or .HEX file
//      /FORMAT=BINARY|INTEL    - set file format
//      /BAS*E=xxxx             - load/save relative to base address (octal)
//      /COU*NT=nnnnn           - number of bytes to save (decimal)
//      /OVER*WRITE             - don't prompt if file already exists (SAVE only!)
//
//   ATT*ACH filename           - attach FDC drive to image file
//   DET*ACH                    - detach FDC drive
//      /UNIT=0|1               - floppy diskette drive unit number
//
//   E*XAMINE xxxx              - display just address xxxx (hex)
//      xxxx-xxxx               - display all addresses in the range
//      xxxx, xxxx, ...         - display multiple addresses or ranges
//      R0..RF,D,DF,...         - display individual CPU register(s)
//      RE*GISTERS              - display all registers
//      /I*NSTRUCTION           - disassemble 1802 instructions
//   Registers - R0..RF, D, DF, P, X, I, N, T, IE, Q
//
//   D*EPOSIT xxxx xx           - deposit one byte
//      xxxx xx, xx, ...        - deposit several bytes
//      Rn xxxx                 - deposit in a register
//
//   SE*T BRE*AKPOINT xxxx      - set breakpoint at address (hex)
//   CL*EAR BRE*AKPOINT xxxx    - clear   "      "     "       "
//   SE*T BRE*AKPOINT xxxx-xxxx -  set breakpoint on address range
//   CL*EAR BRE*AKPOINT xxx-xxx - clear  "    "   "    "  "    "
//   CL*EAR BRE*AKPOINTS        - clear all breakpoints
//   SH*OW BRE*AKPOINTS         - show breakpoints
//
//   RU*N [xxxx]                - clear CPU and start running at PC=xxxx
//   C*ONTINUE                  - resume execution at current PC
//   ST*EP [nnnn]               - single step and trace nnnn instructions
//   RES*ET                     - reset CPU and all devices
// 
//   SH*OW MEM*ORY              - show memory map
//   CL*EAR MEM*ORY             - clear ALL of memory
//      /RAM                    - clear memory except for UT71
//      /ROM                    - clear ONLY UT71
//
//   SH*OW DEV*ICE name         - show details for device <name>
//   SH*OW DEV*ICES             - show list of all devices
//   CL*EAR DEV*ICE name        - reset just device <name>
//   CL*EAR DEV*ICES            - reset all I/O devices only
//   SE*T DEV*ICE name          - set device parameters
//      /TX*SPEED=nnnn          - set SLU transmit speed, in CPS
//      /RX*SPEED=nnnn          -  "   "  receive    "    "   "
//      /STEP=nnnn              - set FDC head step delay (ms)
//      /ROTATIONAL=nnnn        - set FDC rotational delay (ms)
//      /TRANSFER=nnnn          - set FDC data transfer delay (us)
//      /LOAD=nnnn              - set FDC head load time (ms)
//      /UNLOAD=nnnn            - set FDC head unload time (ms)
//      /ENABLE                 - enable TLIO
//      /DISABLE                - disable TLIO
//
//   SH*OW CPU                  - show CPU details
//   CL*EAR CPU                 - reset the CPU only
//   SE*T CPU
//      /BRE*AK=nnn             - set break character to ASCII code nnn
//      /IO=STOP|IGNORE         - stop or ignore illegal I/O references
//      /OPCODE=STOP|IGNORE     -  "    "   "     "   "  opcodes
//      /[NO]EXTENDED           - enable 1804/5/6 extended instructions
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
//  5-MAR-24  RLA   Adapted from SBC1802.
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
#include "MS2000.hpp"          // global declarations for this project
#include "MemoryTypes.h"        // address_t and word_t data types
#include "EventQueue.hpp"       // I/O device event queue simulation
#include "Memory.hpp"           // emulated memory functions
#include "Device.hpp"           // CDevice I/O device emulation objects
#include "DeviceMap.hpp"        // CDeviceMap memory mapped I/O emulation
#include "COSMACopcodes.hpp"    // one line assembler and disassembler
#include "Interrupt.hpp"        // interrupt simulation logic
#include "CPU.hpp"              // CCPU base class definitions
#include "COSMAC.hpp"           // COSMAC 1802 CPU emulation
#include "TLIO.hpp"             // RCA style two level I/O
#include "CDP1854.hpp"          // CDP1854 UART definitions
#include "uPD765.hpp"           // NEC uPD765 FDC definitions
#include "CDP18S651.hpp"        // RCA CDP18S651 floppy disk interface
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
CCmdArgNumber      CUI::m_argTxSpeed("TX speed (cps)", 10, 1, 100000UL);
CCmdArgNumber      CUI::m_argRxSpeed("RX speed (cps)", 10, 1, 100000UL);
CCmdArgName        CUI::m_argOptDeviceName("device", true);
CCmdArgName        CUI::m_argDeviceName("device", false);
CCmdArgNumber      CUI::m_argUnit("unit", 10, 0, 255);
CCmdArgNumber      CUI::m_argStepDelay("head step delay (ms)", 10, 1, 1000000UL);
CCmdArgNumber      CUI::m_argRotationalDelay("rotational delay (ms)", 10, 1, 1000000UL);
CCmdArgNumber      CUI::m_argTransferDelay("data transfer delay (us)", 10, 1, 1000000UL);
CCmdArgNumber      CUI::m_argLoadDelay("head load delay (ms)", 10, 1, 1000000UL);
CCmdArgNumber      CUI::m_argUnloadDelay("head unload delay (ms)", 10, 1, 1000000UL);

// Modifier definitions ...
CCmdModifier CUI::m_modFileFormat("FORM*AT", NULL, &m_argFileFormat);
CCmdModifier CUI::m_modInstruction("I*NSTRUCTION");
CCmdModifier CUI::m_modBreakChar("BRE*AK", NULL, &m_argBreakChar);
CCmdModifier CUI::m_modIllegalIO("IO", NULL, &m_argStopIO);
CCmdModifier CUI::m_modIllegalOpcode("OP*CODE", NULL, &m_argStopOpcode);
CCmdModifier CUI::m_modCPUextended("EXT*ENDED", "NOEXT*ENDED");
CCmdModifier CUI::m_modBaseAddress("BAS*E", NULL, &m_argBaseAddress);
CCmdModifier CUI::m_modByteCount("COU*NT", NULL, &m_argByteCount);
CCmdModifier CUI::m_modTxSpeed("TX*SPEED", NULL, &m_argTxSpeed);
CCmdModifier CUI::m_modRxSpeed("RX*SPEED", NULL, &m_argRxSpeed);
CCmdModifier CUI::m_modUnit("UN*IT", NULL, &m_argUnit);
CCmdModifier CUI::m_modReadOnly("RE*AD", "WR*ITE");
CCmdModifier CUI::m_modOverwrite("OVER*WRITE", "NOOVER*WRITE");
CCmdModifier CUI::m_modEnable("ENA*BLE", "DISA*BLE");
CCmdModifier CUI::m_modROM("ROM", "RAM");
CCmdModifier CUI::m_modWriteLock("WR*ITE", "NOWR*ITE");
CCmdModifier CUI::m_modStepDelay("STEP", NULL, &m_argStepDelay);
CCmdModifier CUI::m_modRotationalDelay("ROT*ATE", NULL, &m_argRotationalDelay);
CCmdModifier CUI::m_modTransferDelay("TRAN*SFER", NULL, &m_argTransferDelay);
CCmdModifier CUI::m_modLoadDelay("LOAD", NULL, &m_argLoadDelay);
CCmdModifier CUI::m_modUnloadDelay("UNLOAD", NULL, &m_argUnloadDelay);

// LOAD and SAVE commands ...
CCmdArgument * const CUI::m_argsLoadSave[] = {&m_argFileName, NULL};
CCmdModifier * const CUI::m_modsLoad[] = {&m_modFileFormat, &m_modBaseAddress,
                                          &m_modByteCount, NULL};
CCmdModifier * const CUI::m_modsSave[] = {&m_modFileFormat, &m_modBaseAddress,
                                          &m_modByteCount,  &m_modOverwrite, NULL};
CCmdVerb CUI::m_cmdLoad("LO*AD", &DoLoad, m_argsLoadSave, m_modsLoad);
CCmdVerb CUI::m_cmdSave("SA*VE", &DoSave, m_argsLoadSave, m_modsSave);

// ATTACH and DETACH commands ...
CCmdArgument * const CUI::m_argsAttachDiskette[] = {&m_argFileName, NULL};
CCmdModifier * const CUI::m_modsDetachDiskette[] = {&m_modUnit, NULL};
CCmdModifier * const CUI::m_modsAttachDiskette[] = {&m_modUnit, &m_modWriteLock, NULL};
CCmdVerb CUI::m_cmdAttachDiskette("ATT*ACH", &DoAttachDiskette, m_argsAttachDiskette, m_modsAttachDiskette);
CCmdVerb CUI::m_cmdDetachDiskette("DET*ACH", &DoDetachDiskette, NULL, m_modsDetachDiskette);

// EXAMINE and DEPOSIT verb definitions ...
CCmdArgument * const CUI::m_argsExamine[] = {&m_argRangeOrNameList, NULL};
CCmdArgument * const CUI::m_argsDeposit[] = {&m_argExamineDeposit, &m_argDataList, NULL};
CCmdModifier * const CUI::m_modsExamine[] = {&m_modInstruction, NULL};
CCmdVerb CUI::m_cmdDeposit("D*EPOSIT", &DoDeposit, m_argsDeposit, NULL);
CCmdVerb CUI::m_cmdExamine("E*XAMINE", &DoExamine, m_argsExamine, m_modsExamine);

// SET, CLEAR and SHOW BREAKPOINT commands ...
CCmdArgument * const CUI::m_argsSetBreakpoint[] = {&m_argBreakpoint, NULL};
CCmdArgument * const CUI::m_argsClearBreakpoint[] = {&m_argOptBreakpoint, NULL};
CCmdVerb CUI::m_cmdSetBreakpoint("BRE*AKPOINT", &DoSetBreakpoint, m_argsSetBreakpoint, NULL);
CCmdVerb CUI::m_cmdClearBreakpoint("BRE*AKPOINT", &DoClearBreakpoint, m_argsClearBreakpoint, NULL);
CCmdVerb CUI::m_cmdShowBreakpoint("BRE*AKPOINT", &DoShowBreakpoints, NULL, NULL);

// RUN, CONTINUE, STEP, and RESET commands ...
CCmdArgument * const CUI::m_argsStep[] = {&m_argStepCount, NULL};
CCmdArgument * const CUI::m_argsRun[] = {&m_argRunAddress, NULL};
CCmdVerb CUI::m_cmdRun("RU*N", &DoRun, m_argsRun, NULL);
CCmdVerb CUI::m_cmdContinue("C*ONTINUE", &DoContinue);
CCmdVerb CUI::m_cmdStep("ST*EP", &DoStep, m_argsStep, NULL);
CCmdVerb CUI::m_cmdReset("RE*SET", &DoReset);

// SET, CLEAR and SHOW CPU ...
CCmdModifier * const CUI::m_modsSetCPU[] = {
    &m_modCPUextended, &m_modIllegalIO, &m_modIllegalOpcode, &m_modBreakChar, NULL
  };
CCmdVerb CUI::m_cmdSetCPU = {"CPU", &DoSetCPU, NULL, m_modsSetCPU};
CCmdVerb CUI::m_cmdClearCPU("CPU", &DoClearCPU);
CCmdVerb CUI::m_cmdShowCPU("CPU", &DoShowCPU);

// CLEAR and SHOW MEMORY ...
CCmdModifier * const CUI::m_modsRAMROM[] = {&m_modROM, NULL};
CCmdVerb CUI::m_cmdClearMemory("MEM*ORY", &DoClearMemory, NULL, m_modsRAMROM);
CCmdVerb CUI::m_cmdShowMemory("MEM*ORY", &DoShowMemory);

// CLEAR and SHOW DEVICE ...
CCmdArgument * const CUI::m_argsShowDevice[] = {&m_argOptDeviceName, NULL};
CCmdArgument * const CUI::m_argsSetDevice[] = {&m_argDeviceName, NULL};
CCmdModifier * const CUI::m_modsSetDevice[] = {
    &m_modTxSpeed, &m_modRxSpeed,
    &m_modStepDelay, &m_modRotationalDelay,
    &m_modTransferDelay, &m_modLoadDelay, &m_modUnloadDelay,
    &m_modEnable, NULL
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
    NULL
  };
CCmdVerb CUI::m_cmdSet("SE*T", NULL, NULL, NULL, g_aSetVerbs);

// SHOW verb definition ...
CCmdVerb CUI::m_cmdShowVersion("VER*SION", &DoShowVersion);
CCmdVerb * const CUI::g_aShowVerbs[] = {
    &m_cmdShowBreakpoint, &m_cmdShowMemory, &m_cmdShowCPU, &m_cmdShowDevice,
    &m_cmdShowVersion,
    &CStandardUI::m_cmdShowLog, &CStandardUI::m_cmdShowAliases,
    NULL
  };
CCmdVerb CUI::m_cmdShow("SH*OW", NULL, NULL, NULL, g_aShowVerbs);

// Master list of all verbs ...
CCmdVerb * const CUI::g_aVerbs[] = {
  &m_cmdLoad, &m_cmdSave, &m_cmdAttachDiskette, &m_cmdDetachDiskette,
  &m_cmdExamine, &m_cmdDeposit,
  &m_cmdRun, &m_cmdContinue, &m_cmdStep, &m_cmdReset,
  &m_cmdSet, &m_cmdShow, &m_cmdClear,
  &CStandardUI::m_cmdDefine, &CStandardUI::m_cmdUndefine,
  &CStandardUI::m_cmdIndirect, &CStandardUI::m_cmdExit,
  &CStandardUI::m_cmdQuit, &CCmdParser::g_cmdHelp,
  NULL
};

////////////////////////////////////////////////////////////////////////////////
//////////////////////////// LOAD AND SAVE COMMANDS ////////////////////////////
////////////////////////////////////////////////////////////////////////////////

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
  // plain binary.  Note that part of the memory space, from ROMBASE to
  // ROMBASE+ROMSIZE-1 is actually EPROM, but we don't distinguish that here.
  //--
  string sFileName;  int nFormat;  int32_t nBytes = 0;
  GetImageFileNameAndFormat(false, sFileName, nFormat);

  // Get the address range to be loaded ...
  address_t wBase = 0;  size_t cbLimit = 0;
  GetImageBaseAndOffset(wBase, cbLimit);
  if (cbLimit > g_pMemory->Size()) cbLimit = g_pMemory->Size();
  switch (nFormat) {
    case FILE_FORMAT_BINARY:
       nBytes = g_pMemory->LoadBinary(sFileName, wBase, cbLimit);  break;
    case FILE_FORMAT_INTEL:
      nBytes = g_pMemory->LoadIntel(sFileName, wBase, cbLimit);  break;
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

  // Save RAM or ROM ...
  address_t wBase = 0;  size_t cbBytes = 0;
  GetImageBaseAndOffset(wBase, cbBytes);
  if (cbBytes > g_pMemory->Size()) cbBytes = g_pMemory->Size();
  if (!(m_modOverwrite.IsPresent() && !m_modOverwrite.IsNegated())) {
    if (FileExists(sFileName)) {
      if (!cmd.AreYouSure(sFileName + " already exists")) return false;
    }
  }
  switch (nFormat) {
    case FILE_FORMAT_BINARY:
      nBytes = g_pMemory->SaveBinary(sFileName, wBase, cbBytes);  break;
    case FILE_FORMAT_INTEL:
      nBytes = g_pMemory->SaveIntel(sFileName, wBase, cbBytes);  break;
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

bool CUI::DoAttachDiskette (CCmdParser &cmd)
{
  //++
  // Attach the floppy diskette drive to an external image file...
  //--
  assert(g_pFDC != NULL);
  uint8_t nUnit;
  if (!GetUnit(nUnit, CUPD765::MAXUNIT)) return false;

  if (g_pFDC->IsAttached(nUnit)) {
    CMDERRS("Floppy disk unit " << nUnit << " already attached to " << g_pFDC->GetFileName(nUnit));
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
  bool fWriteLock = m_modWriteLock.IsPresent() && !m_modWriteLock.IsNegated();
  if (!g_pFDC->Attach(nUnit, sFileName, fWriteLock)) return false;
  CMDOUTS("Floppy disk unit " << nUnit << " attached to " << sFileName);
  if (g_pFDC->IsWriteLocked(nUnit) && !fWriteLock)
    CMDOUTS("Floppy disk unit " << nUnit << " is WRITE PROTECTED");
  return true;
}

bool CUI::DoDetachDiskette (CCmdParser &cmd)
{
  //++
  // Detach and remove the floppy diskette drive ...
  //--
  assert(g_pFDC != NULL);
  if (m_modUnit.IsPresent()) {
    uint8_t nUnit;
    if (!GetUnit(nUnit, CUPD765::MAXUNIT)) return false;
    g_pFDC->Detach(nUnit);
  } else
    g_pFDC->DetachAll();
  return true;
}


////////////////////////////////////////////////////////////////////////////////
///////////////////////// EXAMINE and DEPOSIT COMMANDS /////////////////////////
////////////////////////////////////////////////////////////////////////////////

void CUI::DumpLine (address_t nStart, size_t cbBytes, unsigned nIndent, unsigned nPad)
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
    sprintf_s(sz, sizeof(sz), "%02X ", g_pMemory->UIread(nStart+i));
    strcat_s(szLine, sizeof(szLine), sz);
  }
  for (i = 0; i < nPad; ++i) strcat_s(szLine, sizeof(szLine), "   ");
  strcat_s(szLine, sizeof(szLine), "\t");
  for (i = 0; i < nIndent; ++i) strcat_s(szLine, sizeof(szLine), " ");
  for (i = 0; i < cbBytes; ++i) {
    char sz[2];  uint8_t b = g_pMemory->UIread(nStart+i) & 0x7F;
    sz[0] = isprint(b) ? b : '.';  sz[1] = 0;
    strcat_s(szLine, sizeof(szLine), sz);
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
  if (nStart == nEnd) {
    CMDOUTF("%04X/ %02X", nStart, g_pMemory->UIread(nStart));
  } else if ((nEnd-nStart) < 16) {
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

size_t CUI::DoExamineInstruction (string &sCode, address_t nStart)
{
  //++
  //   This method will disassemble one instruction for the EXAMINE/INSTRUCTION
  // command.  Since instructions are variable length, this can potentially
  // examine 1, 2 or 3 bytes of memory.  The actual number of bytes used is
  // returned.
  //--
  uint8_t bOpcode, b2=0, b3=0, b4=0;

  // Disassemble the opcode and fetch any operands ...
  size_t nCount = Disassemble(g_pMemory, nStart, sCode);
  bOpcode = g_pMemory->UIread(nStart);
  if (nCount > 1) b2 = g_pMemory->UIread(nStart+1);
  if (nCount > 2) b3 = g_pMemory->UIread(nStart+2);
  if (nCount > 3) b4 = g_pMemory->UIread(nStart+3);

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
      if (!g_pMemory->IsValid(nStart, nEnd)) {
         CMDERRF("range exceeds memory - %04x to %04x", nStart, nEnd);  return false;
      } else if (m_modInstruction.IsPresent()) {
        while (nStart <= nEnd) {
          string sCode;
          nStart += DoExamineInstruction(sCode, nStart) & ADDRESS_MASK;
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
  // would cause nEnd to be exceed, then give an error message and quit.  nEnd
  // is otherwise ignored - i.e. it's not an error to specify too few items!
  //--
  bool fHasEnd = nStart != nEnd;
  for (size_t i = 0;  i < List.Count();  ++i) {
    if (fHasEnd && (nStart > nEnd)) {
      CMDERRS("too many data items to deposit");  return false;
    }
    CCmdArgNumber *pData = dynamic_cast<CCmdArgNumber *> (List[i]);
    if (!g_pMemory->IsValid(nStart)) {
      CMDERRF("address exceeds memory - %04X", nStart);  return false;
    } else
      g_pMemory->UIwrite(nStart, pData->GetNumber());
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
    if (!g_pMemory->IsValid(nStart, nEnd)) {
      CMDERRF("range exceeds memory - %04x to %04x", nStart, nEnd);  return false;
    }
    return DoDepositRange(nStart, nEnd, m_argDataList);
  }
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
    string sCode;
    DoExamineInstruction(sCode, wPC);
    CMDOUTS(sCode);
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
  address_t nStart = m_argBreakpoint.GetStart();
  address_t nEnd = m_argBreakpoint.GetEnd();
  if (!g_pMemory->IsValid(nStart, nEnd)) {
    CMDERRF("breakpoint range outside memory - %04x to %04x", nStart, nEnd);  return false;
  } else {
    while (nStart <= nEnd) g_pMemory->SetBreak(nStart++);
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
  assert(g_pMemory != NULL);
  if (m_argOptBreakpoint.IsPresent()) {
    address_t nStart = m_argOptBreakpoint.GetStart();
    address_t nEnd = m_argOptBreakpoint.GetEnd();
    if (!g_pMemory->IsValid(nStart, nEnd)) {
      CMDERRF("breakpoint range outside memory - %04x to %04x", nStart, nEnd);  return false;
    } else {
      while (nStart <= nEnd) g_pMemory->SetBreak(nStart++, false);
    }
  } else {
    // Here to remove all breakpoints.
    g_pMemory->ClearAllBreaks();
  }
  return true;
}

string CUI::ShowBreakpoints()
{
  //++
  //  Show all breakpoints in a given memory space ...
  //--
  string sBreaks;  address_t nLoc = g_pMemory->Base()-1;
  while (g_pMemory->FindBreak(nLoc)) {
    sBreaks += sBreaks.empty() ? "Breakpoint(s) at " : ", ";
    //   We found one breakpoint.  See if it's the start of a contiguous range
    // of breakpoints, or if it's all by itself ...
    char sz[16];  
    if (g_pMemory->IsBreak(nLoc+1)) {
      // There's more than one!
      address_t nEnd = nLoc+1;
      while (g_pMemory->IsBreak(nEnd)) ++nEnd;
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
  CMDOUTS(ShowBreakpoints());
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
  // Clear some or all of memory.  
  // 
  //    CLEAR MEMORY/RAM  -> clear all memory EXCEPT UT71
  //    CLEAR MEMORY/ROM  -> clear UT71 space
  //    CLEAR MEMORY      -> clear everything
  //--
  assert(g_pMemory);
  if (m_modROM.IsPresent() && !m_modROM.IsNegated()) g_pMemory->ClearROM();
  if (m_modROM.IsPresent() &&  m_modROM.IsNegated()) g_pMemory->ClearRAM();
  if (!m_modROM.IsPresent()) g_pMemory->ClearMemory();
  return true;
}

bool CUI::DoShowMemory(CCmdParser &cmd)
{
  //++
  // Figure out (it's not too hard!) and print the memory map ...
  //--
  size_t nFirst = 0;
  CMDOUTF("\nSTART  END    SIZE  TYPE");
  CMDOUTF("-----  -----  ----  ----");
  while (nFirst < g_pMemory->Size()) {
    size_t nSize = g_pMemory->CountFlags(ADDRESS(nFirst));
    if (g_pMemory->IsRAM(ADDRESS(nFirst)))
      CMDOUTF("$%04X  $%04X  %3dK  RAM", nFirst, nFirst+nSize-1, nSize>>10);
    else if (g_pMemory->IsROM(ADDRESS(nFirst)))
      CMDOUTF("$%04X  $%04X  %3dK  ROM", nFirst, nFirst+nSize-1, nSize>>10);
    nFirst += nSize;
  }
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
  //--
  CDevice *pDevice = g_pTLIO->FindDevice(sDevice);
  if (pDevice != NULL) return pDevice;
  CMDERRS("No such device as " << sDevice);
  return NULL;
}

void CUI::ShowOneDevice (const CDevice *pDevice, bool fHeading)
{
  //++
  // Show the common device options (description, ports, type) to a string.
  //--
  string sLine;
  
  sLine = FormatString("%-6s  %-7s  %-25s  ",
    pDevice->GetName(), pDevice->GetType(), pDevice->GetDescription());

  if (pDevice->IsInOut())
    sLine += " INOUT  ";
  else if (pDevice->IsInput())
    sLine += " INPUT  ";
  else if (pDevice->IsOutput())
    sLine += " OUTPUT ";
  else
    sLine += "        ";

  uint8_t nGroup = g_pTLIO->FindGroup(pDevice);
  if (nGroup != 0)
    sLine += FormatString("  $%02X  ", nGroup);
  else
    sLine += "       ";

  if (pDevice->GetPortCount() <= 1) {
    if (pDevice->GetBasePort() <= 7)
      sLine += FormatString(" %d       ", pDevice->GetBasePort());
    else
      sLine += FormatString(" $%04X       ", pDevice->GetBasePort());
  } else {
    if (pDevice->GetBasePort() <= 7)
      sLine += FormatString(" %d..%d    ",
        pDevice->GetBasePort(), pDevice->GetBasePort() + pDevice->GetPortCount()-1);
    else
      sLine += FormatString(" $%04X..%04X ",
        pDevice->GetBasePort(), pDevice->GetBasePort() + pDevice->GetPortCount()-1);
  }

  if (fHeading) {
    CMDOUTS("DEVICE  TYPE     DESCRIPTION                 IN/OUT  GROUP  PORT");
    CMDOUTS("------  -------  --------------------------  ------  -----  ----");
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
  ShowOneDevice(g_pTLIO, true);
  ShowOneDevice(g_pSLU);
  ShowOneDevice(g_pFDC);
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

  // Otherwise try to match the device name ...
  string sDevice = m_argOptDeviceName.GetValue();
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
  // serial ports, floppy diskette delays, etc.  This code is not very smart
  // in that it silently ignores any options which don't apply to the selected
  // device.
  //--
  CDevice *pDevice = FindDevice(m_argDeviceName.GetValue());
  if (pDevice == NULL) return false;

  if ((pDevice == g_pTLIO) && m_modEnable.IsPresent()) {
    g_pTLIO->EnableTLIO(!m_modEnable.IsNegated());
  } else if (pDevice == g_pSLU) {
    if (m_modTxSpeed.IsPresent()) g_pSLU->SetTxSpeed(m_argTxSpeed.GetNumber());
    if (m_modRxSpeed.IsPresent()) g_pSLU->SetRxSpeed(m_argRxSpeed.GetNumber());
  } else if (pDevice == g_pFDC) {
    if (m_modStepDelay.IsPresent())       g_pFDC->SetStepDelay(MSTONS(m_argStepDelay.GetNumber()));
    if (m_modRotationalDelay.IsPresent()) g_pFDC->SetRotationalDelay(MSTONS(m_argRotationalDelay.GetNumber()));
    if (m_modTransferDelay.IsPresent())   g_pFDC->SetTransferDelay(USTONS(m_argTransferDelay.GetNumber()));
    if (m_modLoadDelay.IsPresent())       g_pFDC->SetLoadDelay(MSTONS(m_argLoadDelay.GetNumber()));
    if (m_modUnloadDelay.IsPresent())     g_pFDC->SetUnloadDelay(MSTONS(m_argUnloadDelay.GetNumber()));
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
  CMDOUTF("\nMS2000 Emulator v%d\n", MSVER);
  return true;
}
