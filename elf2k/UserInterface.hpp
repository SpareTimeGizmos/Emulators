//++
// UserInterface.hpp -> UserInterface for ELF2K Emulator
//
//   COPYRIGHT (C) 2015-2020 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
//
// LICENSE:
//    This file is part of the ELF2K emulator project.  ELF2K is free
// software; you may redistribute it and/or modify it under the terms of
// the GNU Affero General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any
// later version.
//
//    ELF2K is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License
// for more details.  You should have received a copy of the GNU Affero General
// Public License along with ELF2K.  If not, see http://www.gnu.org/licenses/.
//
// DESCRIPTION:
//   This class implements the ELF2K specific user interface (as opposed to the
// classes in CommandParser.hpp, which implement a generic command line parser).
// The attentive reader can't help but notice that everything here is static -
// that's because there is only one UI object, ever.  The critical reader might
// ask "Well then, why bother with a class definition at all?"  Good point, but
// if nothing else it keeps things together and localizes the UI implementation
// to this class.
//
//   Think of it as a poor man's namespace ...
//
// REVISION HISTORY:
// 23-JUL-19  RLA   New file.
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ string functions
#include "MemoryTypes.h"        // address_t and word_t data types
using std::string;              // ...

class CUI {
  //++
  //--

  // Command scanner constants ...
public:
  enum {
    // Address space selections ...
    ADDRESS_SPACE_RAM   = 0,    // address the RAM
    ADDRESS_SPACE_ROM   = 1,    // address the EPROM
    ADDRESS_SPACE_NVR   = 2,    // address the non-volatile RAM
    // LOAD/SAVE file formats ...
    FILE_FORMAT_NONE    = 0,    // no format specified
    FILE_FORMAT_INTEL   = 1,    // INTEL .HEX file format
    FILE_FORMAT_BINARY  = 2,    // raw binary dump
    // Software serial inversion types ...
    INVERT_NONE         = 0,    // don't invert anything
    INVERT_TX           = 1,    // invert serial transmit data
    INVERT_RX           = 2,    // invert serial receive data
    INVERT_BOTH         = 3     // invert both transmit and receive
  };

  //   The constructor and destructor for this class are conspicuously private.
  // That's because we never need to actually create an instance of this class
  // - it has no local data and the parser tables and command action routines
  // are all static.
private:
  CUI() {};
  ~CUI() {};
  CUI(const CUI &);
  void operator= (const CUI &);

  //   Believe it or not, the only public hook into this whole thing (other
  // than the constructor, IS this table of verbs that we pass to the 
  // command parser ...
public:
  static CCmdVerb * const g_aVerbs[];

  // Keyword tables ...
private:
  static const CCmdArgKeyword::KEYWORD m_keysFileFormat[], m_keysStopIgnore[];
  static const CCmdArgKeyword::KEYWORD m_keysEFs[], m_keysInvert[];

  // Argument tables ...
private:
  static CCmdArgFileName    m_argFileName, m_argOptFileName;
  static CCmdArgKeyword     m_argFileFormat, m_argStopOpcode, m_argStopIO;
  static CCmdArgNumber      m_argData, m_argRunAddress, m_argStepCount;
  static CCmdArgNumber      m_argBreakpoint, m_argOptBreakpoint;
  static CCmdArgNumber      m_argBaseAddress, m_argByteCount;
  static CCmdArgNumber      m_argSwitches, m_argBaudRate, m_argDelay;
  static CCmdArgNumber      m_argPollDelay, m_argBreakChar, m_argPortNumber;
  static CCmdArgName        m_argRegisterName;
  static CCmdArgNumberRange m_argAddressRange;
  static CCmdArgRangeOrName m_argExamineDeposit;
  static CCmdArgList        m_argRangeList, m_argDataList;
  static CCmdArgList        m_argRangeOrNameList, m_argDelayList;
  static CCmdArgKeyword     m_argEF, m_argInvert;

  // Modifier definitions ...
private:
  static CCmdModifier m_modFileFormat;
  static CCmdModifier m_modInstruction;
  static CCmdModifier m_modBaudRate;
  static CCmdModifier m_modInvertData;
  static CCmdModifier m_modDelay;
  static CCmdModifier m_modDelayList;
  static CCmdModifier m_modPollDelay;
  static CCmdModifier m_modBreakChar;
  static CCmdModifier m_modPortNumber;
  static CCmdModifier m_modIllegalOpcode;
  static CCmdModifier m_modIllegalIO;
  static CCmdModifier m_modRAM;
  static CCmdModifier m_modROM;
  static CCmdModifier m_modNVR;
  static CCmdModifier m_modBaseAddress;
  static CCmdModifier m_modByteCount;
  static CCmdModifier m_modEFdefault;
  static CCmdModifier m_modCPUextended;
  static CCmdModifier m_modOverwrite;
  static CCmdModifier m_modClose;
  static CCmdModifier m_modText;
  static CCmdModifier m_modXModem;
  static CCmdModifier m_modAppend;
  static CCmdModifier m_modCRLF;

  // Verb definitions ...
private:
  // LOAD and SAVE commands ...
  static CCmdArgument * const m_argsLoadSave[];
  static CCmdModifier * const m_modsLoadSave[];
  static CCmdVerb m_cmdLoad, m_cmdSave;

  // ATTACH and DETACH commands ...
  static CCmdArgument * const m_argsAttachIDE[];
  static CCmdArgument * const m_argsAttachSerial[];
  static CCmdModifier * const m_modsAttach[];
  static CCmdVerb m_cmdAttachIDE, m_cmdAttachINS8250, m_cmdAttachDS12887;
  static CCmdVerb m_cmdDetachIDE, m_cmdDetachINS8250, m_cmdDetachDS12887;
  static CCmdVerb m_cmdDetachCombo;
  static CCmdVerb m_cmdDetachSerial, m_cmdAttachSerial;
  static CCmdVerb m_cmdAttachTIL311, m_cmdDetachTIL311;
  static CCmdVerb m_cmdAttachSwitches, m_cmdDetachSwitches;
#ifdef INCLUDE_CDP1854
  static CCmdVerb m_cmdAttachCDP1854, m_cmdDetachCDP1854;
#endif
  static CCmdVerb * const g_aAttachVerbs[];
  static CCmdVerb * const g_aDetachVerbs[];
  static CCmdVerb m_cmdAttach, m_cmdDetach;

  // EXAMINE and DEPOSIT commands ...
  static CCmdArgument * const m_argsDeposit[];
  static CCmdArgument * const m_argsExamine[];
  static CCmdModifier * const m_modsExamine[];
  static CCmdVerb m_cmdExamine, m_cmdDeposit;

  // SET, SHOW and CLEAR BREAKPOINT commands ...
  static CCmdArgument * const m_argsSetBreakpoint[];
  static CCmdArgument * const m_argsClearBreakpoint[];
  static CCmdVerb m_cmdSetBreakpoint, m_cmdShowBreakpoint, m_cmdClearBreakpoint;

  // CLEAR, RUN, CONTINUE, STEP and RESET commands ....
  static CCmdVerb * const g_aClearVerbs[];
  static CCmdArgument * const m_argsStep[];
  static CCmdArgument * const m_argsRun[];
  static CCmdVerb m_cmdClearRAM, m_cmdClearMemory, m_cmdClearNVR;
  static CCmdVerb m_cmdClearCPU, m_cmdRun, m_cmdContinue, m_cmdStep;
  static CCmdVerb m_cmdClear, m_cmdReset;

  // SEND and RECEIVE commands ...
  static CCmdArgument * const m_argsSendFile[];
  static CCmdArgument * const m_argsReceiveFile[];
  static CCmdModifier * const m_modsSendFile[];
  static CCmdModifier * const m_modsReceiveFile[];
  static CCmdVerb m_cmdSendFile, m_cmdReceiveFile;

  // SET and SHOW verb definitions ...
  static CCmdArgument * const m_argsSetSwitches[];
  static CCmdArgument * const m_argsSetMemory[];
  static CCmdModifier * const m_modsSetSerial[];
  static CCmdModifier * const m_modsSetUART[];
  static CCmdModifier * const m_modsSetIDE[];
  static CCmdModifier * const m_modsSetCPU[];
  static CCmdModifier * const m_modsSetMemory[];
  static CCmdVerb * const g_aSetVerbs[];
  static CCmdVerb * const g_aShowVerbs[];
  static CCmdVerb m_cmdSet, m_cmdShow;
  static CCmdVerb m_cmdShowAll, m_cmdShowVersion;
  static CCmdVerb m_cmdShowConfiguration, m_cmdShowMemory;
  static CCmdVerb m_cmdSetCPU, m_cmdSetSwitches, m_cmdSetSerial;
  static CCmdVerb m_cmdSetUART, m_cmdSetIDE, m_cmdSetMemory;

  // Other "helper" routines ...
public:
  static bool ConfirmExit();

  // Verb action routines ....
private:
  static bool DoLoad(CCmdParser &cmd), DoSave(CCmdParser &cmd);
  static bool DoShowVersion(CCmdParser &cmd), DoShowAll(CCmdParser &cmd);
  static bool DoDeposit(CCmdParser &cmd), DoExamine(CCmdParser &cmd);
  static bool DoClearCPU(CCmdParser &cmd), DoRun(CCmdParser &cmd);
  static bool DoContinue(CCmdParser &cmd), DoStep(CCmdParser &cmd);
  static bool DoSetBreakpoint(CCmdParser &cmd), DoClearBreakpoint(CCmdParser &cmd);
  static bool DoShowBreakpoints(CCmdParser &cmd), DoShowMemory(CCmdParser &cmd);
  static bool DoAttachIDE(CCmdParser &cmd), DoAttachDS12887(CCmdParser &cmd), DoAttachINS8250(CCmdParser &cmd);
  static bool DoDetachIDE(CCmdParser &cmd), DoDetachDS12887(CCmdParser &cmd), DoDetachINS8250(CCmdParser &cmd);
  static bool DoAttachSerial(CCmdParser &cmd), DoDetachSerial(CCmdParser &cmd);
  static bool DoAttachTIL311(CCmdParser &cmd), DoDetachTIL311(CCmdParser &cmd), DoDetachCombo(CCmdParser &cmd);
#ifdef INCLUDE_CDP1854
  static bool DoAttachCDP1854(CCmdParser &cmd), DoDetachCDP1854(CCmdParser &cmd);
#endif
  static bool DoAttachSwitches(CCmdParser &cmd), DoDetachSwitches(CCmdParser &cmd);
  static bool DoShowConfiguration(CCmdParser &cmd), DoSetMemory(CCmdParser &cmd);
  static bool DoClearRAM(CCmdParser &cmd), DoClearMemory(CCmdParser &cmd), DoClearNVR(CCmdParser &cmd);
  static bool DoSetCPU(CCmdParser &cmd), DoSetSwitches(CCmdParser &cmd), DoSetSerial(CCmdParser &cmd);
  static bool DoSetUART(CCmdParser &cmd), DoSetIDE(CCmdParser &cmd);
  static bool DoReset(CCmdParser &cmd);
  static bool DoSendFile(CCmdParser &cmd), DoReceiveFile(CCmdParser &cmd);

  // Other "helper" routines ...
  static bool IsINS8250installed(), IsDS12887installed(), IsIDEinstalled();
  static bool IsSerialInstalled(), IsTIL311Installed(), IsSwitchesInstalled();
#ifdef INCLUDE_CDP1854
  static bool IsCDP1854Installed();
#endif
  static void DumpLine (address_t nStart, size_t cbBytes, unsigned nIndent=0, unsigned nPad=0);
  static void DoExamineRange (address_t nStart, address_t nEnd);
  static size_t DoExamineInstruction (address_t nStart);
  static string ExamineRegister (int nIndex);
  static bool DoExamineOneRegister (string sName);
  static void DoExamineAllRegisters();
  static bool DoExamineDevice(string sName);
  static bool DoDepositRange (address_t nStart, address_t nEnd, CCmdArgList &List);
  static bool DoDepositRegister (string sName, uint16_t nValue);
  static void GetImageFileNameAndFormat (bool fCreate, string &sFileName, int &nFormat);
  static void GetImageBaseAndOffset (address_t&wBase, size_t &cbBytes);
  static unsigned RunSimulation(uint32_t nSteps=0);
  static void AttachDiskUARTrtc(), DetachDiskUARTrtc();
  static string ShowOneDevice (const class CDevice *pDevice);
  static bool DoCloseSend(CCmdParser &cmd), DoCloseReceive(CCmdParser &cmd);
};
