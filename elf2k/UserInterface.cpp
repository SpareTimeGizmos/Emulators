//++
// UserInterface.cpp -> ELF2K Emulator Specific User Interface code
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
//   This module implements the user interface specific to the ELF2K emulator
// process.  The first half of the file are parse tables for the generic command
// line parser classes from CommandParser.cpp, and the second half is the action
// routines needed to implement these commands.  
//
//                                 Bob Armstrong [23-JUL-19]
//
// COMMAND SUMMARY
//    
// REVISION HISTORY:
// 23-JUL-19  RLA   New file.
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
#include "SafeCRT.h"		// sprintf_s(), etc ...
#include "LogFile.hpp"          // emulator library message logging facility
#include "ImageFile.hpp"        // emulator library image file methods
#include "CommandParser.hpp"    // emulator library command line parsing methods
#include "CommandLine.hpp"      // Shell (argc/argv) parser methods
#include "VirtualConsole.hpp"   // needed for CVirtualConsole
#include "ConsoleWindow.hpp"    // standard console window emulation
#include "SmartConsole.hpp"     // console window file transfer functions
#include "ELF2K.hpp"            // global declarations for this project
#include "StandardUI.hpp"       // emulator library standard UI commands
#include "UserInterface.hpp"    // emulator user interface parse table definitions
#include "MemoryTypes.h"        // address_t and word_t data types
#include "Memory.hpp"           // emulated memory functions
#include "COSMACopcodes.hpp"    // one line assembler and disassembler
#include "CPU.hpp"              // CCPU base class definitions
#include "COSMAC.hpp"           // COSMAC 1802 CPU emulation
#include "Device.hpp"           // CDevice I/O device emulation objects
#include "TIL311.hpp"           // TIL311 POST display emulation
#include "Switches.hpp"         // toggle switch register emulation
#include "SoftwareSerial.hpp"   // bit banged serial port emulation
#include "INS8250.hpp"          // UART emulation
//#include "CDP1854.hpp"          // and another UART emulation
#include "DS12887.hpp"          // NVR/RTC emulation
#include "IDE.hpp"              // IDE emulation
#include "DiskUARTrtc.hpp"      // ELF2K Disk/UART/RTC card emulation


// UI class object constructor cheat sheet!
//   CCmdArgument    (<name>, [<optional>])
//   CCmdArgName     (<name>, [<optional>])
//   CCmdArgNumber   (<name>, <radix>, <min>, <max>, [<optional>])
//   CCmdArgKeyword  (<name>, <keywords>, [<optional>])
//   CCmdArgFileName (<name>, [<optional>])
//   CCmdModifier    (<name>, [<"NO" name>], [<arg>], [<optional>])
//   CCmdVerb        (<name>, <action>, [<args>], [<mods>], [<subverbs>])


// LOAD/SAVE file format keywords ...
const CCmdArgKeyword::KEYWORD CUI::m_keysFileFormat[] = {
  {"BIN*ARY", CUI::FILE_FORMAT_BINARY},
  {"IN*TEL",  CUI::FILE_FORMAT_INTEL},
  {NULL, 0}
};

// EFx names for "ATTACH SERIAL" ...
const CCmdArgKeyword::KEYWORD CUI::m_keysEFs[] = {
  {"EF1", 0},  {"EF2", 1},
  {"EF3", 2},  {"EF4", 3},
  {NULL, 0}
};

// Arguments for "SET SERIAL/INVERT=..."
const CCmdArgKeyword::KEYWORD CUI::m_keysInvert[] = {
  {"NONE", INVERT_NONE}, {"TX",   INVERT_TX},
  {"RX",   INVERT_RX},   {"BOTH", INVERT_BOTH},
  {NULL, 0}
};

// STOP or IGNORE options for "SET CPU" ...
const CCmdArgKeyword::KEYWORD CUI::m_keysStopIgnore[] = {
  {"ST*OP", true}, {"IGN*ORE", false}, {NULL, 0}
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
CCmdArgNumberRange CUI::m_argAddressRange("address range", 16, 0, MEMSIZE-1);
CCmdArgName        CUI::m_argRegisterName("register name");
CCmdArgRangeOrName CUI::m_argExamineDeposit("name or range", 16, 0, MEMSIZE-1);
CCmdArgList        CUI::m_argRangeOrNameList("name or range list", m_argExamineDeposit);
CCmdArgList        CUI::m_argRangeList("address range list", m_argAddressRange);
CCmdArgNumber      CUI::m_argData("data", 16, 0, UINT16_MAX);
CCmdArgList        CUI::m_argDataList("data list", m_argData);
CCmdArgNumber      CUI::m_argStepCount("step count", 10, 1, INT16_MAX, true);
CCmdArgNumber      CUI::m_argRunAddress("run address", 16, 0, MEMSIZE-1, true);
CCmdArgNumber      CUI::m_argBreakpoint("breakpoint address", 16, 0, MEMSIZE-1);
CCmdArgNumber      CUI::m_argOptBreakpoint("breakpoint address", 16, 0, MEMSIZE-1, true);
CCmdArgKeyword     CUI::m_argEF("EF input", m_keysEFs);
CCmdArgKeyword     CUI::m_argInvert("TX, RX or BOTH", m_keysInvert, true);
CCmdArgNumber      CUI::m_argSwitches("switches", 16, 0, 255);
CCmdArgNumber      CUI::m_argBaudRate("bits per second", 10, 110, 2400UL);
CCmdArgNumber      CUI::m_argDelay("delay (us)", 10, 1, 1000000UL);
CCmdArgNumber      CUI::m_argPollDelay("poll delay", 10, 1, 1000000UL);
CCmdArgList        CUI::m_argDelayList("delay list", m_argDelay, true);
CCmdArgNumber      CUI::m_argBreakChar("break character", 10, 1, 31);
CCmdArgNumber      CUI::m_argPortNumber("port number", 10, 1, 7);
CCmdArgKeyword     CUI::m_argStopIO("stop on illegal I/O", m_keysStopIgnore);
CCmdArgKeyword     CUI::m_argStopOpcode("stop on illegal opcode", m_keysStopIgnore);
CCmdArgNumber      CUI::m_argBaseAddress("starting address", 16, 0, MEMSIZE-1);
CCmdArgNumber      CUI::m_argByteCount("byte count", 10, 0, MEMSIZE-1);

// Modifier definitions ...
//   Like command arguments, modifiers may be shared by several commands...-
CCmdModifier CUI::m_modFileFormat("FORM*AT", NULL, &m_argFileFormat);
CCmdModifier CUI::m_modInstruction("I*NSTRUCTION");
CCmdModifier CUI::m_modBaudRate("BAUD", NULL, &m_argBaudRate);
CCmdModifier CUI::m_modInvertData("INV*ERT", "NOINV*ERT", &m_argInvert);
CCmdModifier CUI::m_modDelay("DEL*AY", NULL, &m_argDelay);
CCmdModifier CUI::m_modDelayList("DEL*AY", NULL, &m_argDelayList);
CCmdModifier CUI::m_modPollDelay("POLL", NULL, &m_argPollDelay);
CCmdModifier CUI::m_modBreakChar("BRE*AK", NULL, &m_argBreakChar);
CCmdModifier CUI::m_modPortNumber("POR*T", NULL, &m_argPortNumber);
CCmdModifier CUI::m_modIllegalIO("IO", NULL, &m_argStopIO);
CCmdModifier CUI::m_modIllegalOpcode("OP*CODE", NULL, &m_argStopOpcode);
CCmdModifier CUI::m_modRAM("RAM", "NORAM");
CCmdModifier CUI::m_modROM("ROM", "NOROM");
CCmdModifier CUI::m_modNVR("NVR", "NONVR");
CCmdModifier CUI::m_modBaseAddress("BAS*E", NULL, &m_argBaseAddress);
CCmdModifier CUI::m_modByteCount("COU*NT", NULL, &m_argByteCount);
CCmdModifier CUI::m_modEFdefault("EFDEF*AULT", NULL, &m_argDataList);
CCmdModifier CUI::m_modCPUextended("EXT*ENDED", "NOEXT*ENDED");
CCmdModifier CUI::m_modOverwrite("OVER*WRITE", "NOOVER*WRITE");
CCmdModifier CUI::m_modClose("CL*OSE", NULL);
CCmdModifier CUI::m_modText("TE*XT", NULL);
CCmdModifier CUI::m_modXModem("X*MODEM", NULL);
CCmdModifier CUI::m_modAppend("APP*END", "OVER*WRITE");
CCmdModifier CUI::m_modCRLF("CRLF", "NOCRLF");

// LOAD and SAVE verb definitions ...
CCmdArgument * const CUI::m_argsLoadSave[] = {&m_argFileName, NULL};
CCmdModifier * const CUI::m_modsLoadSave[] = {&m_modFileFormat, &m_modBaseAddress, 
                                              &m_modByteCount, &m_modRAM, &m_modROM, 
                                              &m_modNVR, &m_modOverwrite, NULL};
CCmdVerb CUI::m_cmdLoad("LO*AD", &DoLoad, m_argsLoadSave, m_modsLoadSave);
CCmdVerb CUI::m_cmdSave("SA*VE", &DoSave, m_argsLoadSave, m_modsLoadSave);

// ATTACH and DETACH commands ...
CCmdArgument * const CUI::m_argsAttachIDE[] = {&m_argFileName, NULL};
CCmdArgument * const CUI::m_argsAttachSerial[] = {&m_argEF, NULL};
CCmdModifier * const CUI::m_modsAttach[] = {&m_modPortNumber, NULL};
CCmdVerb CUI::m_cmdAttachIDE("IDE", &DoAttachIDE, m_argsAttachIDE, m_modsAttach);
CCmdVerb CUI::m_cmdDetachIDE("IDE", &DoDetachIDE);
CCmdVerb CUI::m_cmdAttachINS8250("INS8250", &DoAttachINS8250, NULL, m_modsAttach);
CCmdVerb CUI::m_cmdDetachINS8250("INS8250", &DoDetachINS8250);
CCmdVerb CUI::m_cmdAttachDS12887("DS12887", &DoAttachDS12887, NULL, m_modsAttach);
CCmdVerb CUI::m_cmdDetachDS12887("DS12887", &DoDetachDS12887);
CCmdVerb CUI::m_cmdDetachCombo("COMBO", &DoDetachCombo);
//CCmdVerb CUI::m_cmdAttachCDP1854("CDP1854", &DoAttachCDP1854, NULL, m_modsAttach);
//CCmdVerb CUI::m_cmdDetachCDP1854("CDP1854", &DoDetachCDP1854);
CCmdVerb CUI::m_cmdAttachSerial("SER*IAL", &DoAttachSerial, m_argsAttachSerial, m_modsAttach);
CCmdVerb CUI::m_cmdDetachSerial("SER*IAL", &DoDetachSerial);
CCmdVerb CUI::m_cmdAttachTIL311("TIL311", &DoAttachTIL311, NULL, m_modsAttach);
CCmdVerb CUI::m_cmdDetachTIL311("TIL311", &DoDetachTIL311);
CCmdVerb CUI::m_cmdAttachSwitches("SWIT*CHES", &DoAttachSwitches, NULL, m_modsAttach);
CCmdVerb CUI::m_cmdDetachSwitches("SWIT*CHES", &DoDetachSwitches);
CCmdVerb * const CUI::g_aAttachVerbs[] = {
  &m_cmdAttachINS8250, &m_cmdAttachDS12887, &m_cmdAttachIDE,
  /*&m_cmdAttachCDP1854,*/ &m_cmdAttachSerial, &m_cmdAttachTIL311, &m_cmdAttachSwitches,
  NULL
};
CCmdVerb * const CUI::g_aDetachVerbs[] = {
  &m_cmdDetachINS8250, &m_cmdDetachDS12887, &m_cmdDetachIDE, &m_cmdDetachCombo,
  /*&m_cmdDetachCDP1854,*/ &m_cmdDetachSerial, &m_cmdDetachTIL311, &m_cmdDetachSwitches,
  NULL
};
CCmdVerb CUI::m_cmdAttach("ATT*ACH", NULL, NULL, NULL, g_aAttachVerbs);
CCmdVerb CUI::m_cmdDetach("DET*ACH", NULL, NULL, NULL, g_aDetachVerbs);

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

// RUN, CONTINUE, STEP and RESET commands ...
CCmdArgument * const CUI::m_argsStep[] = {&m_argStepCount, NULL};
CCmdArgument * const CUI::m_argsRun[] = {&m_argRunAddress, NULL};
CCmdVerb CUI::m_cmdRun("RU*N", &DoRun, m_argsRun, NULL);
CCmdVerb CUI::m_cmdContinue("C*ONTINUE", &DoContinue, NULL, NULL);
CCmdVerb CUI::m_cmdStep("ST*EP", &DoStep, m_argsStep, NULL);
CCmdVerb CUI::m_cmdReset("RESET", &DoReset);

// CLEAR command ...
CCmdVerb CUI::m_cmdClearMemory("MEM*ORY", &DoClearMemory);
CCmdVerb CUI::m_cmdClearRAM("RAM", &DoClearRAM);
CCmdVerb CUI::m_cmdClearNVR("NVR", &DoClearNVR);
CCmdVerb CUI::m_cmdClearCPU("CPU", *DoClearCPU);
CCmdVerb * const CUI::g_aClearVerbs[] = {&m_cmdClearBreakpoint, &m_cmdClearCPU, &m_cmdClearRAM, &m_cmdClearMemory, &m_cmdClearNVR, NULL};
CCmdVerb CUI::m_cmdClear("CL*EAR", NULL, NULL, NULL, g_aClearVerbs);

// SEND and RECEIVE commands ...
CCmdArgument * const CUI::m_argsSendFile[] = {&m_argOptFileName, NULL};
CCmdArgument * const CUI::m_argsReceiveFile[] = {&m_argOptFileName, NULL};
CCmdModifier * const CUI::m_modsSendFile[] = {&m_modClose, &m_modText, &m_modXModem, &m_modCRLF, &m_modDelayList, NULL};
CCmdModifier * const CUI::m_modsReceiveFile[] = {&m_modClose, &m_modText, &m_modXModem, &m_modAppend, &m_modDelayList, NULL};
CCmdVerb CUI::m_cmdSendFile = {"SE*ND", &DoSendFile, m_argsSendFile, m_modsSendFile};
CCmdVerb CUI::m_cmdReceiveFile = {"RE*CEIVE", &DoReceiveFile, m_argsReceiveFile, m_modsReceiveFile};

// SET verb definition ...
CCmdArgument * const CUI::m_argsSetMemory[] = {&m_argRangeList, NULL};
CCmdModifier * const CUI::m_modsSetMemory[] = {&m_modRAM, &m_modROM, NULL};
CCmdArgument * const CUI::m_argsSetSwitches[] = {&m_argSwitches, NULL};
CCmdModifier * const CUI::m_modsSetSerial[] = {&m_modBaudRate, &m_modInvertData, &m_modPollDelay, NULL};
CCmdModifier * const CUI::m_modsSetUART[] = {&m_modDelay, &m_modPollDelay, NULL};
CCmdModifier * const CUI::m_modsSetIDE[] = {&m_modDelayList, NULL};
CCmdModifier * const CUI::m_modsSetCPU[] = {&m_modIllegalIO, &m_modIllegalOpcode, 
                                            &m_modBreakChar, &m_modEFdefault, 
                                            &m_modCPUextended, NULL};
CCmdVerb CUI::m_cmdSetCPU = {"CPU", &DoSetCPU, NULL, m_modsSetCPU};
CCmdVerb CUI::m_cmdSetMemory = {"MEM*ORY", &DoSetMemory, m_argsSetMemory, m_modsSetMemory};
CCmdVerb CUI::m_cmdSetSwitches = {"SWIT*CHES", &DoSetSwitches, m_argsSetSwitches, NULL};
CCmdVerb CUI::m_cmdSetSerial = {"SER*IAL", &DoSetSerial, NULL, m_modsSetSerial};
CCmdVerb CUI::m_cmdSetUART = {"UART", &DoSetUART, NULL, m_modsSetUART};
CCmdVerb CUI::m_cmdSetIDE = {"IDE", &DoSetIDE, NULL, m_modsSetIDE};
CCmdVerb * const CUI::g_aSetVerbs[] = {
  &m_cmdSetBreakpoint, &m_cmdSetCPU, &m_cmdSetMemory, &m_cmdSetSwitches,
  &m_cmdSetUART, &m_cmdSetIDE, &m_cmdSetSerial,
  &CStandardUI::m_cmdSetLog, &CStandardUI::m_cmdSetWindow,
  NULL
};
CCmdVerb CUI::m_cmdSet("SE*T", NULL, NULL, NULL, g_aSetVerbs);

// SHOW verb definition ...
CCmdVerb CUI::m_cmdShowConfiguration("CONF*IGURATION", &DoShowConfiguration);
CCmdVerb CUI::m_cmdShowMemory("MEM*ORY", &DoShowMemory);
CCmdVerb CUI::m_cmdShowVersion("VER*SION", &DoShowVersion);
CCmdVerb CUI::m_cmdShowAll("ALL", &DoShowAll);
CCmdVerb * const CUI::g_aShowVerbs[] = {
  &m_cmdShowBreakpoint, &m_cmdShowMemory, &m_cmdShowConfiguration,
  &CStandardUI::m_cmdShowLog, &m_cmdShowVersion,
  &CStandardUI::m_cmdShowAliases, &m_cmdShowAll,
  NULL
};
CCmdVerb CUI::m_cmdShow("SH*OW", NULL, NULL, NULL, g_aShowVerbs);

// Master list of all verbs ...
CCmdVerb * const CUI::g_aVerbs[] = {
  &m_cmdLoad, &m_cmdSave, &m_cmdAttach, &m_cmdDetach,
  &m_cmdExamine, &m_cmdDeposit, &m_cmdReset,
  &m_cmdSendFile, &m_cmdReceiveFile, &m_cmdSet, &m_cmdShow,
  &m_cmdClear, &m_cmdRun, &m_cmdContinue, &m_cmdStep,
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

  //   The file format is always binary for the NVR.  For RAM or ROM, if we still
  // don't know the format then assume binary ...
  if (m_modNVR.IsPresent()) {
    if ((nFormat != FILE_FORMAT_NONE) && (nFormat != FILE_FORMAT_BINARY))
      CMDERRS("NVR images must be binary format");
  } else {
    if (nFormat == FILE_FORMAT_NONE) {
      nFormat = FILE_FORMAT_BINARY;
      CMDERRS("BINARY format assumed for " << sFileName);
    }
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
  // to the address specified by the file.  This is handy for things like the
  // ELF2K EPROM, where the hex file is origined at 0x0000, but the code is
  // intended to run at 0x8000.  
  //
  //   There are several ways to specify the base and size.  First, the
  // user can be explicit about it with the /BASE= and /COUNT= modifiers.
  // Or he can use one of the /RAM or /ROM modifiers, which specify one of
  // the predefined RAM or EPROM areas on the ELF2K.  Or lastly the /NVR
  // modifier specifies that the non-volatile RAM is to be accessed instead.
  // This is entirely separate from the main memory.
  //
  //    Notice that /RAM, /ROM, /NVR and /BASE=.../COUNT=... are all mutually
  // exclusive.  Strictly speaking only one is legal, however we don't bother
  // to check for this error.  /BASE and /COUNT take precedence over any of
  // the others, and any other conflicts are ignored.
  //--

  // Figure out the base address ...
  if (m_modBaseAddress.IsPresent())
    wBase = m_argBaseAddress.GetNumber();
  else if (m_modRAM.IsPresent())
    wBase = RAMBASE;
  else if (m_modROM.IsPresent())
    wBase = ROMBASE;
  else if (m_modNVR.IsPresent())
    wBase = 0;
  else
    wBase = 0;

  // And figure out the size ...
  if (m_modByteCount.IsPresent())
    cbBytes = m_argByteCount.GetNumber();
  else if (m_modRAM.IsPresent())
    cbBytes = RAMSIZE;
  else if (m_modROM.IsPresent())
    cbBytes = ROMSIZE;
  else if (m_modNVR.IsPresent())
    cbBytes = C12887::NVRSIZE;
  else
    cbBytes = MEMSIZE-wBase;
}

bool CUI::DoLoad (CCmdParser &cmd)
{
  //++
  //   The LOAD command loads either RAM or ROM from a disk file in either
  // Intel HEX format or plain binary.  It's pretty simple minded and loading
  // ALL of RAM or all of ROM are the only options.  There's no way to specify
  // any base addresses, offsets, limits, or anything other than the file name.
  //--
  string sFileName;  int nFormat;  int32_t nBytes = 0;
  GetImageFileNameAndFormat(false, sFileName, nFormat);

  if (m_modNVR.IsPresent()) {
    // Load the NVR (note that the file format is ignored!) ...
    if (!IsDS12887installed()) {
      CMDERRS("ATTACH DS12887 first");  return false;
    } else
      nBytes = g_pDiskUARTrtc->GetNVR()->LoadNVR(sFileName);
  } else {
    // Load RAM or ROM ...
    address_t wBase = 0;  size_t cbLimit = 0;
    GetImageBaseAndOffset(wBase, cbLimit);
    switch (nFormat) {
      case FILE_FORMAT_BINARY:
        nBytes = g_pMemory->LoadBinary(sFileName, wBase, cbLimit);  break;
      case FILE_FORMAT_INTEL:
        nBytes = g_pMemory->LoadIntel(sFileName, wBase, cbLimit);  break;
    }
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

  if (m_modNVR.IsPresent()) {
    // Save the NVR (the file format is ignored!) ...
    if (!IsDS12887installed()) {
      CMDERRS("ATTACH DS12887 first");  return false;
    } else
      nBytes = g_pDiskUARTrtc->GetNVR()->SaveNVR(sFileName);
  } else {
    // Save RAM or ROM ...
    address_t wBase = 0;  size_t cbBytes = 0;
    GetImageBaseAndOffset(wBase, cbBytes);
    if (m_modOverwrite.IsPresent() && !m_modOverwrite.IsNegated()) {
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
  }

  // All done...
  if (nBytes < 0) return false;
  CMDOUTF("%d bytes saved to %s", nBytes, sFileName.c_str());
  return true;
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////// SEND and RECEIVE COMMANDS ///////////////////////////
////////////////////////////////////////////////////////////////////////////////

bool CUI::DoCloseSend (CCmdParser &cmd)
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

bool CUI::DoCloseReceive (CCmdParser &cmd)
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
      g_pConsole->SetXdelay(qDelay);  g_pConsole->GetXdelay(qDelay);
      CMDOUTF("XMODEM speed is %ld CPS", NSTOCPS(qDelay));
    } else {
      // For TEXT, /DELAY wants two parameters ...
      if (m_argDelayList.Count() != 2) {
        CMDERRS("specify /DELAY=(line,character) in milliseconds");  return false;
      }
      uint64_t qLineDelay = MSTONS((dynamic_cast<CCmdArgNumber*> (m_argDelayList[0]))->GetNumber());
      uint64_t qCharDelay = MSTONS((dynamic_cast<CCmdArgNumber*> (m_argDelayList[1]))->GetNumber());
      g_pConsole->SetTextDelays(qCharDelay, qLineDelay);
      g_pConsole->GetTextDelays(qCharDelay, qLineDelay);
      CMDOUTF("Text speed is %ld CPS, end of line delay %ld ms", NSTOCPS(qCharDelay), NSTOMS(qLineDelay));
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
////////////////////////// ATTACH and DETACH COMMANDS //////////////////////////
////////////////////////////////////////////////////////////////////////////////

#ifdef UNUSED
bool CUI::IsCDP1854Installed()
{
  //++
  // Return TRUE if the CDP1854 UART is installed ...
  //--
  return g_pUART != NULL;
}
#endif

bool CUI::IsINS8250installed()
{
  //++
  // Return TRUE if the INS8250 UART is installed ...
  //--
  return (g_pDiskUARTrtc != NULL)  &&  g_pDiskUARTrtc->IsUARTinstalled();
}

bool CUI::IsDS12887installed()
{
  //++
  // Return TRUE if the NVR/RTC is installed ...
  //--
  return (g_pDiskUARTrtc != NULL)  &&  g_pDiskUARTrtc->IsNVRinstalled();
}

bool CUI::IsIDEinstalled()
{
  //++
  // Return TRUE if the IDE disk is installed ...
  //--
  return (g_pDiskUARTrtc != NULL)  &&  g_pDiskUARTrtc->IsIDEinstalled();
}

bool CUI::IsSerialInstalled()
{
  //++
  // Return TRUE if the software serial is installed ...
  //--
  return (g_pSerial != NULL) /*&& g_pCPU->IsSerialInstalled()*/;
}

bool CUI::IsTIL311Installed()
{
  //++
  // Return TRUE if the TIL311 displays are isntalled ...
  //--
  return (g_pTIL311 != NULL);
}

bool CUI::IsSwitchesInstalled()
{
  //++
  // Return true if the toggle switch register is installed ...
  //--
  return (g_pSwitches != NULL);
}

void CUI::AttachDiskUARTrtc()
{
  //++
  //   The ELF2K Disk/UART/RTC card is a "master" device that includes the
  // UART, IDE disk and NVR/RTC.  This routine will check that this master
  // card is installed and, if it isn't, will install it.  This is a necessary
  // prerequesite for installing the IDE, UART or NVR options...
  //--
  if (g_pDiskUARTrtc != NULL) return;
  uint8_t nPort = PORT_DISK_UART_RTC;
  if (m_modPortNumber.IsPresent()) nPort = m_argPortNumber.GetNumber();
  g_pDiskUARTrtc = DBGNEW CDiskUARTrtc(nPort, g_pEvents);
  g_pCPU->InstallDevice(g_pDiskUARTrtc);
}

void CUI::DetachDiskUARTrtc()
{
  //++
  //   If the ELF2K master Disk/UART/RTC card is installed, AND if all three
  // of the IDE, UART and NVR are no longer attached, then this routine will
  // detach the master card too.
  //--
  if (g_pDiskUARTrtc == NULL) return;
  if (g_pDiskUARTrtc->IsIDEinstalled()) return;
  if (g_pDiskUARTrtc->IsNVRinstalled()) return;
  if (g_pDiskUARTrtc->IsUARTinstalled()) return;
  g_pCPU->RemoveDevice(g_pDiskUARTrtc);
  g_pDiskUARTrtc = NULL;
}

bool CUI::DoAttachIDE (CCmdParser &cmd)
{
  //++
  //   Install the IDE drive and attach it to an external image file, after
  // first installing the Disk/UART/RTC card if necessary ...
  //--
#ifdef UNUSED
  if (IsCDP1854Installed()) {
    CMDERRS("CDP1854 already installed");  return false;
  }
#endif
  if (IsIDEinstalled()) {
    CMDERRS("IDE already attached to " << g_pDiskUARTrtc->GetIDE()->GetFileName(0));
    return false;
  }

  // The default image extension is ".dsk" ...
  string sFileName = m_argFileName.GetFullPath();
  if (!FileExists(sFileName)) {
    string sDrive, sDir, sName, sExt;
    SplitPath(sFileName, sDrive, sDir, sName, sExt);
    sFileName = MakePath(sDrive, sDir, sName, ".dsk");
  }

  // Attach the card and the drive, and we're done!
  AttachDiskUARTrtc();
  return g_pDiskUARTrtc->InstallIDE(sFileName);
}

bool CUI::DoDetachIDE (CCmdParser &cmd)
{
  //++
  // Detach and remove the IDE drive ...
  //--
  if (!IsIDEinstalled()) {
    CMDERRS("IDE not attached");  return false;
  }
  g_pDiskUARTrtc->RemoveIDE();
  DetachDiskUARTrtc();
  return true;
}

bool CUI::DoAttachDS12887 (CCmdParser &cmd)
{
  //++
  //   Install the NVR/RTC device, after first installing the Disk/UART/RTC
  // card if necessary.  Note that the NVR is initialized to all zeros - you
  // can load it from a file with the "LOAD NVR" command if desired ...
  //--
#ifdef UNUSED
  if (IsCDP1854Installed()) {
    CMDERRS("CDP1854 already installed");  return false;
  }
#endif
  if (IsDS12887installed()) {
    CMDERRS("DS12887 already installed");  return false;
  }
  AttachDiskUARTrtc();
  g_pDiskUARTrtc->InstallNVR();
  return true;
}

bool CUI::DoDetachDS12887 (CCmdParser &cmd)
{
  //++
  //   Remove the NVR/RTC device. Note that the current contents of the NVR
  // are lost when you do this!  If you want to save it, use the "SAVE NVR"
  // command first...
  //--
  if (!IsDS12887installed()) {
    CMDERRS("DS12887 not installed");  return false;
  }
  g_pDiskUARTrtc->RemoveNVR();
  DetachDiskUARTrtc();
  return true;
}

bool CUI::DoClearNVR (CCmdParser &cmd)
{
  //++
  //   Clear the non-volatile RAM (all bytes EXCEPT the time data in the first
  // 14 bytes!) ...
  //--
  if (!IsDS12887installed()) {
    CMDERRS("DS12887 not installed");  return false;
  }
  g_pDiskUARTrtc->GetNVR()->ClearNVR();
  return true;
}

bool CUI::DoAttachINS8250 (CCmdParser &cmd)
{
  //++
  // Install the emulated UART device and connect it to the console ...
  //
  //   Note that you can't have both the UART and the Software Serial installed
  // at the same time.  It's one console option or the other.
  //--
#ifdef UNUSED
  if (IsCDP1854Installed()) {
    CMDERRS("CDP1854 already installed");  return false;
  }
#endif
  if (IsINS8250installed()) {
    CMDERRS("INS8250 already installed");  return false;
  }
  if (IsSerialInstalled()) {
    CMDERRS("software serial already installed");  return false;
  }
  AttachDiskUARTrtc();
  g_pDiskUARTrtc->InstallUART(g_pConsole, g_pCPU);
  return true;
}

bool CUI::DoDetachINS8250(CCmdParser &cmd)
{
  //++
  // Remove the simulated UART ...
  //--
  if (!IsINS8250installed()) {
    CMDERRS("INS8250 not installed");  return false;
  }
  g_pDiskUARTrtc->RemoveUART();
  DetachDiskUARTrtc();
  return true;
}

bool CUI::DoDetachCombo (CCmdParser &cmd)
{
  //++
  //   Force the Disk/UART/RTC card to be removed, detaching all subdevices
  // first if necessary.
  //--
  if (g_pDiskUARTrtc == NULL) {
    CMDERRS("Disk/UART/RTC card not attached");  return false;
  }
  if (g_pDiskUARTrtc->IsIDEinstalled())  g_pDiskUARTrtc->RemoveIDE();
  if (g_pDiskUARTrtc->IsNVRinstalled())  g_pDiskUARTrtc->RemoveNVR();
  if (g_pDiskUARTrtc->IsUARTinstalled()) g_pDiskUARTrtc->RemoveUART();
  g_pCPU->RemoveDevice(g_pDiskUARTrtc);
  g_pDiskUARTrtc = NULL;
  return true;
}

bool CUI::DoAttachSerial (CCmdParser &cmd)
{
  //++
  // Install the software serial (bit banged!) terminal emulation.
  //
  //   Note that you can only have one serial console - either the UART or
  // the software serial - but not both.
  //--
  uint16_t nSense = (uint16_t) m_argEF.GetKeyValue();
  if (IsSerialInstalled() || g_pCPU->IsSenseInstalled(nSense)) {
    CMDERRS("software serial already installed");  return false;
  }
  if (IsINS8250installed()) {
    CMDERRS("UART already installed");  return false;
  }
  g_pSerial = DBGNEW CSoftwareSerial(g_pEvents, g_pConsole, g_pCPU);
  g_pCPU->InstallSense(g_pSerial, nSense);
  g_pCPU->InstallFlag(g_pSerial);
  return true;
}

bool CUI::DoDetachSerial (CCmdParser &cmd)
{
  //++
  // Remove the software serial emulation ...
  //--
  if (!IsSerialInstalled()) {
    CMDERRS("software serial not installed");  return false;
  }
  g_pCPU->RemoveDevice(g_pSerial);  g_pSerial = NULL;
  return true;
}

bool CUI::DoAttachTIL311 (CCmdParser &cmd)
{
  //++
  // Install the TIL311 POST display ...
  //--
  if (IsTIL311Installed()) {
    CMDERRS("TIL311 display already installed");  return false;
  }
  uint8_t nPort = PORT_POST;
  if (m_modPortNumber.IsPresent()) nPort = m_argPortNumber.GetNumber();
  g_pTIL311 = DBGNEW CTIL311(nPort);
  g_pCPU->InstallDevice(g_pTIL311);
  return true;
}

bool CUI::DoDetachTIL311 (CCmdParser &cmd)
{
  //++
  // Remove the POST display ...
  //--
  if (!IsTIL311Installed()) {
    CMDERRS("TIL311 display not installed");  return false;
  }
  g_pCPU->RemoveDevice(g_pTIL311);  g_pTIL311 = NULL;
  return true;
}

bool CUI::DoAttachSwitches (CCmdParser &cmd)
{
  //++
  // Install the toggle switch register ...
  //--
  if (IsSwitchesInstalled()) {
    CMDERRS("switch register already installed");  return false;
  }
  uint8_t nPort = PORT_SWITCHES;
  if (m_modPortNumber.IsPresent()) nPort = m_argPortNumber.GetNumber();
  g_pSwitches = DBGNEW CSwitches(nPort);
  g_pCPU->InstallDevice(g_pSwitches);
  return true;
}

bool CUI::DoDetachSwitches (CCmdParser &cmd)
{
  //++
  // Remove the switch register ...
  //--
  if (!IsSwitchesInstalled()) {
    CMDERRS("switch register not installed");  return false;
  }
  g_pCPU->RemoveDevice(g_pSwitches);  g_pSwitches = NULL;
  return true;
}

#ifdef UNUSED
bool CUI::DoAttachCDP1854 (CCmdParser &cmd)
{
  //++
  // Install the emulated CDP1854 UART device and connect it to the console ...
  //
  //   Note that the CDP1854 can only be installed if a) the Disk/UART/RTC
  // combo board isn't installed, and b) the Software Serial isn't installed
  // at the same time.  It's one console option or the other.
  //--
  if (IsCDP1854Installed()) {
    CMDERRS("CDP1854 already installed");  return false;
  }
  if (g_pDiskUARTrtc != NULL) {
    CMDERRS("Disk/UART/RTC board already installed");  return false;
  }
  if (IsSerialInstalled()) {
    CMDERRS("software serial already installed");  return false;
  }
  uint8_t nPort = PORT_CDP1854;
  if (m_modPortNumber.IsPresent()) nPort = m_argPortNumber.GetNumber();
  g_pUART = DBGNEW CCDP1854("SLU", nPort, g_pEvents, g_pSmartConsole, g_pCPU);
  g_pCPU->InstallDevice(g_pUART);
  return true;
}

bool CUI::DoDetachCDP1854 (CCmdParser &cmd)
{
  //++
  // Remove the CDP1854 UART ...
  //--
  if (!IsCDP1854Installed()) {
    CMDERRS("CDP1854 not installed");  return false;
  }
  g_pCPU->RemoveDevice(g_pUART);  g_pUART = NULL;
  return true;
}
#endif


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

size_t CUI::DoExamineInstruction (address_t nStart)
{
  //++
  //   This method will disassemble one instruction for the EXAMINE/INSTRUCTION
  // command.  Since instructions are variable length, this can potentially
  // examine 1, 2 or 3 bytes of memory.  The actual number of bytes used is
  // returned.
  //--
  string sCode;  uint8_t bOpcode=0, b2=0, b3=0;

  // Disassemble the opcode and fetch any operands ...
  size_t nCount = Disassemble(g_pMemory, nStart, sCode);
  bOpcode = g_pMemory->UIread(nStart);
  if (nCount > 1) b2 = g_pMemory->UIread(nStart+1);
  if (nCount > 2) b3 = g_pMemory->UIread(nStart+2);

  // Print it out neatly ...
  if (nCount <= 1)
    CMDOUTF("%04X/ %02X      \t%s", nStart, bOpcode, sCode.c_str());
  else if (nCount == 2)
    CMDOUTF("%04X/ %02X %02X   \t%s", nStart, bOpcode, b2, sCode.c_str());
  else
    CMDOUTF("%04X/ %02X %02X %02X\t%s", nStart, bOpcode, b2, b3, sCode.c_str());

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

void CUI::DoExamineAllRegisters()
{
  //++
  //   Print the contents of ALL internal CPU registers (formatted as neatly
  // as we can without knowing too much about them!)...
  //--
  string sLine;
  const CCmdArgKeyword::KEYWORD *paNames = g_pCPU->GetRegisterNames();
  for (int i = 0; paNames[i].m_pszName != NULL; ++i) {
    if (!g_pCPU->IsExtended() && (i >= CCOSMAC::REG_XIE)) break;
    string sRegister = ExamineRegister(i);
    if (((sLine.length()+sRegister.length()) > 75) || (i == CCOSMAC::REG_XIE)) {
      CMDOUTS(sLine);  sLine.clear();
    }
    sLine += sRegister + (i < 16 ? "  " : " ");
  }
  if (!sLine.empty()) CMDOUTS(sLine);
}

bool CUI::DoExamineDevice (string sName)
{
  //++
  //   Check to see if sName is a device name and, if it is, print the status
  // and registers of that device.  If sName is unknown then return false and
  // do nothing (don't print an error message!).
  //--
  const CDevice *pDevice = g_pCPU->FindDevice(sName);
  if (pDevice == NULL) {
    if (g_pDiskUARTrtc != NULL)  pDevice = g_pDiskUARTrtc->FindDevice(sName);
    if (pDevice == NULL) return false;
  }
  ostringstream ofs;
  pDevice->ShowDevice(ofs);
  CMDOUT(ofs);
  return true;
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
      } else if (!DoExamineDevice(sName) && !DoExamineOneRegister(sName)) {
        CMDERRS("Unknown register - \"" << sName << "\"");  return false;
      }
    } else {
      address_t nStart = pArg->GetRangeArg().GetStart();
      address_t nEnd = pArg->GetRangeArg().GetEnd();
      if (m_modInstruction.IsPresent()) {
        while (nStart <= nEnd)
          nStart += ADDRESS(DoExamineInstruction(nStart));
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
    DoExamineInstruction(g_pCPU->GetPC());
    unsigned  nStop = RunSimulation(1);
    if (nStop != CCPU::STOP_FINISHED) return false;
    DoExamineAllRegisters();
  }
  return true;
}

bool CUI::DoReset (CCmdParser &cmd)
{
  //++
  // Reset the CPU and all I/O devices!
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
  // at the specified address.  Note that there's no error message if you set
  // a breakpoint at an address that already has a breakpoint, and only one
  // breakpoint is actually set.
  //--
  address_t nBreak = m_argBreakpoint.GetNumber();
  g_pMemory->SetBreak(nBreak);
  return true;
}

bool CUI::DoClearBreakpoint (CCmdParser &cmd)
{
  //++
  //   The CLEAR BREAKPOINT [nnnn] will remove the breakpoint at the specified
  // address or, if no address is specified, it will remove all breakpoints.
  // Note that there's no error message if you ask it to clear a breakpoint
  // that doesn't exist!
  //--
  if (m_argOptBreakpoint.IsPresent()) {
    g_pMemory->SetBreak(m_argOptBreakpoint.GetNumber(), false);
  } else
    g_pMemory->ClearAllBreaks();
  return true;
}

bool CUI::DoShowBreakpoints (CCmdParser &cmd)
{
  //++
  // List all current breakpoints ...
  //--
  string sBreakpoints;  address_t nBreak = g_pMemory->Base()-1;
  while (g_pMemory->FindBreak(nBreak)) {
    sBreakpoints += sBreakpoints.empty() ? "Breakpoint(s) at " : ", ";
    char sz[16];  sprintf_s(sz, sizeof(sz), "%04X", nBreak);
    sBreakpoints += sz;
  }
  if (sBreakpoints.empty()) {
    CMDOUTS("No breakpoints set.");
  } else
    CMDOUTS(sBreakpoints);
  return true;
}


////////////////////////////////////////////////////////////////////////////////
///////////////////////////////// CPU COMMANDS /////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

bool CUI::DoClearCPU (CCmdParser &cmd)
{
  //++
  // Clear (reset!) the CPU and all peripherals ...
  //--
  g_pCPU->MasterClear();
  return true;
}

bool CUI::DoSetCPU (CCmdParser &cmd)
{
  //++
  //   SET CPU allows you to set the CPU type (e.g. 1802 or 1805), and well as
  // vaious options (e.g. stop on illegal I/O, stop on illegal opcode, etc).
  //--
  if (m_modIllegalIO.IsPresent())
    g_pCPU->StopOnIllegalIO(m_argStopIO.GetKeyValue() != 0);
  if (m_modIllegalOpcode.IsPresent())
    g_pCPU->StopOnIllegalOpcode(m_argStopOpcode.GetKeyValue() != 0);
  if (m_modBreakChar.IsPresent())
    g_pConsole->SetConsoleBreak(m_argBreakChar.GetNumber());
  if (m_modCPUextended.IsPresent())
    g_pCPU->SetExtended(!m_modCPUextended.IsNegated());
  if (m_modEFdefault.IsPresent()) {
    for (uint16_t i = 0;  i < m_argDataList.Count();  ++i) {
      CCmdArgNumber *pData = dynamic_cast<CCmdArgNumber *> (m_argDataList[i]);
      if (i < CCOSMAC::MAXSENSE)
        g_pCPU->SetDefaultEF(i, pData->GetNumber());
    }
  }
  return true;
}


////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// MEMORY COMMANDS ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

bool CUI::DoClearRAM (CCmdParser &cmd)
{
  //++
  //   Clear all writable locations in memory, but leave the read only ones
  // (e.g. ROM/EPROM!) alone ...
  //--
  g_pMemory->ClearRAM();
  return true;
}

bool CUI::DoClearMemory (CCmdParser &cmd)
{
  //++
  // Clear ALL of memory, RAM and ROM/EPROM alike ...
  //--
  g_pMemory->ClearMemory();
  return true;
}

bool CUI::DoShowMemory (CCmdParser &cmd)
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
      CMDOUTF("%04X   %04X   %3dK  RAM", nFirst, nFirst+nSize-1, nSize>>10);
    else if (g_pMemory->IsROM(ADDRESS(nFirst)))
      CMDOUTF("%04X   %04X   %3dK  ROM", nFirst, nFirst+nSize-1, nSize>>10);
    nFirst += nSize;
  }
  CMDOUTS("");
  return true;
}

bool CUI::DoSetMemory (CCmdParser &cmd)
{
  //++
  //   The SET MEMORY command allows you to define segments of the memory space
  // as RAM, ROM, or non-existent.  The /RAM, /ROM or /NORAM/NOROM modifiers
  // specify the type of memory to be defined, and the argument list specifies
  // a list of address ranges to be set.
  //
  //   Note that the address map only affects the operation of the simulated
  // CPU.  The UI commands (LOAD, SAVE, EXAMINE, DEPOSIT, etc) can always
  // access all of memory regardless.
  //--
  uint8_t bMemFlags = 0;

  // Figure out what we're setting memory to ...
  if (m_modRAM.IsPresent() && m_modRAM.IsNegated() && m_modROM.IsPresent() && m_modROM.IsNegated()) {
    // /NORAM and /NOROM - no memory at all!
  } else if (m_modRAM.IsPresent() && !m_modRAM.IsNegated() && !m_modROM.IsPresent()) {
    // /RAM -> read/write memory ...
    bMemFlags = CMemory::MEM_READ | CMemory::MEM_WRITE;
  } else if (m_modROM.IsPresent() && !m_modROM.IsNegated() && !m_modRAM.IsPresent()) {
    bMemFlags = CMemory::MEM_READ;
  } else {
    CMDERRS("use /RAM, /ROM or /NORAM/NOROM only!");  return false;
  }

  // Now go thru all the address ranges and set each one ...
  for (size_t i = 0; i < m_argRangeList.Count(); ++i) {
    CCmdArgNumberRange *pArg = dynamic_cast<CCmdArgNumberRange *> (m_argRangeList[i]);
    assert(pArg != NULL);
    address_t nStart = pArg->GetStart();
    address_t nEnd = pArg->GetEnd();
    g_pMemory->SetFlags(nStart, nEnd, bMemFlags, CMemory::MEM_READ|CMemory::MEM_WRITE);
  }
  return true;
}


////////////////////////////////////////////////////////////////////////////////
/////////////////////////////// DEVICE COMMANDS ////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

string CUI::ShowOneDevice (const CDevice *pDevice)
{
  //++
  // Convert the common device options (description, ports, type) to a string.
  //--
  char szName[100], szPort[100], szType[100];
  if (pDevice == NULL) return "";
  sprintf_s(szName, sizeof(szName), "%-8s  %-25s  ", pDevice->GetName(), pDevice->GetDescription());

  if (pDevice->GetBasePort() == 0)
    sprintf_s(szPort, sizeof(szPort), "        ");
  else if (pDevice->GetPortCount() <= 1)
    sprintf_s(szPort, sizeof(szPort), "%5d   ", pDevice->GetBasePort());
  else
    sprintf_s(szPort, sizeof(szPort), " %d..%d   ",
      pDevice->GetBasePort(), pDevice->GetBasePort() + pDevice->GetPortCount()-1);

  sprintf_s(szType, sizeof(szType), "%-6s",
              pDevice->IsInOut() ? "INOUT" : pDevice->IsInput() ? "INPUT" : "OUTPUT");
  return string(szName) + string(szPort) + string(szType);
}

bool CUI::DoShowConfiguration (CCmdParser &cmd)
{
  //++
  // Show the configuration of the CPU and all installed devices ...
  //--
  double flClock = g_pCPU->GetCrystalFrequency() / 1000000.0;
  CMDOUTF("\nDEVICE    DESCRIPTION                 PORT   TYPE    CHARACTERISTICS");
  CMDOUTF("--------  -------------------------  ------  ------  -------------------------------------");
  CMDOUTF("%-8s  %-27s        CPU     %3.1fMHz, BREAK=^%c", g_pCPU->GetName(), g_pCPU->GetDescription(),
          flClock, (g_pConsole->GetConsoleBreak()+'@'));
  if (IsTIL311Installed())   CMDOUTF("%s  POST=0x%02X", ShowOneDevice(g_pTIL311).c_str(), g_pTIL311->GetPOST());
  if (IsSwitchesInstalled()) CMDOUTF("%s  SWITCHES=0x%02X", ShowOneDevice(g_pSwitches).c_str(), g_pSwitches->GetSwitches());
  if (IsSerialInstalled()) {
    string sInvert("");
    if (g_pSerial->IsTXinverted())
      sInvert = g_pSerial->IsRXinverted() ? "BOTH" : "TX";
    else
      sInvert = g_pSerial->IsRXinverted() ? "RX" : "NONE";
    CMDOUTF("%s  %s/%s, INVERT=%s, BAUD=%ld, POLL=%ldus", ShowOneDevice(g_pSerial).c_str(),
            g_pCPU->GetSenseName(g_pCPU->FindSense(g_pSerial)),
            g_pCPU->GetFlagName(g_pCPU->FindFlag(g_pSerial)),
            sInvert.c_str(),
            g_pSerial->GetBaud(), NSTOUS(g_pSerial->GetPollDelay()));
  }
#ifdef UNUSED
  if (IsCDP1854Installed()) {
    CMDOUTF("%s  DELAY=%ldus, POLL=%ldus", ShowOneDevice(g_pUART).c_str(),
            NSTOUS(g_pUART->GetCharacterDelay()), NSTOUS(g_pUART->GetPollDelay()));
  }
#endif

  if (g_pDiskUARTrtc != NULL) {
    CMDOUTF("%s  ", ShowOneDevice(g_pDiskUARTrtc).c_str());
    if (IsINS8250installed()) {
      const CUART *pUART = g_pDiskUARTrtc->GetUART();
      CMDOUTF("%s  DELAY=%lldus, POLL=%lldus", ShowOneDevice(pUART).c_str(),
              NSTOUS(pUART->GetCharacterDelay()), NSTOUS(pUART->GetPollDelay()));
    }
    if (g_pDiskUARTrtc->IsNVRinstalled()) {
      const C12887 *pNVR  = g_pDiskUARTrtc->GetNVR();
      CMDOUTF("%s  REGA=0x%02X, REGB=0x%02X, REGC=0x%02X", ShowOneDevice(pNVR).c_str(), pNVR->GetRegA(), pNVR->GetRegB(), pNVR->GetRegC());
    }
    if (g_pDiskUARTrtc->IsIDEinstalled()) {
      const CIDE *pIDE  = g_pDiskUARTrtc->GetIDE();
      CMDOUTF("%s  %s", ShowOneDevice(pIDE).c_str(), CStandardUI::Abbreviate(pIDE->GetFileName(0), 35).c_str());
    }
  }
  CMDOUTS("");
  return true;
}

bool CUI::DoSetSwitches (CCmdParser &cmd)
{
  //++
  //   Set the state of the toggle switches, if installed ...  Note that the
  // switch value is an argument to the command, e.g. "SET SWITCHES 0xFF".
  // The switches do not support any timing parameters or options!
  //--
  if (IsSwitchesInstalled()) {
    g_pSwitches->SetSwitches(m_argSwitches.GetNumber());
    return true;
  } else {
    CMDERRS("switches not installed");  return false;
  }
}

bool CUI::DoSetUART (CCmdParser &cmd)
{
  //++
  //   The UART has two timing parameters which may be changed - the transmit
  // delay, which determines how long it takes to send a character from the
  // CPU to the terminal, and the polling interval, which determines how often
  // we poll the keyboard for input from the terminal to the CPU.  Note that
  // ALL DELAYS ARE SPECIFIED IN MICROSECONDS!
  //--
  if (!IsINS8250installed()) {
    CMDERRS("UART not installed");  return false;
  }
  CINS8250 *pUART = g_pDiskUARTrtc->GetUART();
  if (m_modDelay.IsPresent())
    pUART->SetCharacterDelay(USTONS(m_argDelay.GetNumber()));
  if (m_modPollDelay.IsPresent())
    pUART->SetPollDelay(USTONS(m_argPollDelay.GetNumber()));
  return true;
}

bool CUI::DoSetSerial (CCmdParser &cmd)
{
  //++
  //   The software (aka "bit banged") serial port supports the same POLL 
  // parameter that the UART does (see DoSetUART(), above), however instead
  // of a DELAY parameter the software serial supports a BAUD rate parameter.
  // The BAUD parameter sets the number of simulated bits per second; this is
  // more convenient than setting the delay directly.
  //
  //   Software serial also supports an additional /INVERT option.  This option
  // gets a bit messy since either transmit, receive, or both may be inverted.
  // Data should be inverted if the hardware uses an inverting RS232 driver,
  // such as the MAX232 or MC1488 and/or MC1489. Data is not inverted for a
  // direct RS232 interface.
  //--
  if (!IsSerialInstalled()) {
    CMDERRS("serial emulation not installed");  return false;
  }
  if (m_modBaudRate.IsPresent())
    g_pSerial->SetBaud(m_argBaudRate.GetNumber());
  if (m_modPollDelay.IsPresent())
    g_pSerial->SetPollDelay(USTONS(m_argPollDelay.GetNumber()));

  // See if /INVERT was present and, if it was, see if there was an argument...
  if (m_modInvertData.IsPresent()) {
    bool fInvert = !m_modInvertData.IsNegated();
    if (!m_argInvert.IsPresent()) {
      //   If /INVERT is used with no argument, then set both RX and TX to
      // inverted.  Likewise, if /NOINVERT is used with no argument, set
      // both RX and TX to not-inverted.
      g_pSerial->SetInvert(fInvert, fInvert);
    } else {
      if (m_argInvert.GetKeyValue() == INVERT_NONE)
        //   /INVERT=NONE sets both TX and RX to true.  Likewise, /NOINVERT=NONE
        // sets both to inverted.  That latter one is kind of silly, but what
        // else can we do??
        g_pSerial->SetInvert(!fInvert, !fInvert);
      else if (m_argInvert.GetKeyValue() == INVERT_TX)
        //   /INVERT=TX and /NOINVERT=TX change just the transmit state and
        // leave the receive state unchanged ...
        g_pSerial->SetInvert(fInvert, g_pSerial->IsRXinverted());
      else if (m_argInvert.GetKeyValue() == INVERT_RX)
        // Ditto for /INVERT=RX ....
        g_pSerial->SetInvert(g_pSerial->IsTXinverted(), fInvert);
      else
        // And finally, /INVERT=BOTH changes both values ...
        g_pSerial->SetInvert(fInvert, fInvert);
    }
  }
  return true;
}

bool CUI::DoSetIDE (CCmdParser &cmd)
{
  //++
  //   The IDE emulation supports two delay times - a "long" delay which is
  // for all operations that read or write data, and a "short" delay which
  // is used for all other commands.  The "SET IDE/DELAY=(long,short)" command
  // can be used to change these two values.  The shorter form of the same
  // command, "SET IDE/DELAY=delay" will set both delays to the same value.
  // NOTE THAT ALL DELAYS ARE SPECIFIED IN MICROSECONDS!
  //--
  if (!IsIDEinstalled()) {
    CMDERRS("IDE not installed");  return false;
  }
  if (!m_modDelayList.IsPresent() || (m_argDelayList.Count() > 2)) {
    CMDERRS("specify /DELAY=(long,short)");  return false;
  }
  uint64_t llLongDelay = (dynamic_cast<CCmdArgNumber *> (m_argDelayList[0]))->GetNumber();
  uint64_t llShortDelay = llLongDelay;
  if (m_argDelayList.Count() > 1) 
    llShortDelay = (dynamic_cast<CCmdArgNumber *> (m_argDelayList[1]))->GetNumber();
  if (llShortDelay > llLongDelay) {
    CMDERRS("long delay must be .GE. short delay");  return false;
  }
  g_pDiskUARTrtc->GetIDE()->SetDelays(USTONS(llLongDelay), USTONS(llShortDelay));
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
  CMDOUTF("\nELF2K Emulator v%d\n", ELFVER);
  return true;
}

bool CUI::DoShowAll (CCmdParser &cmd)
{
  //++
  // Show everything!
  //--
  DoShowVersion(cmd);
  CStandardUI::DoShowLog(cmd);
//CStandardUI::DoShowAllAliases(cmd);
  DoShowConfiguration(cmd);
  return true;
}
