//++
// UserInterface.hpp -> UserInterface for MS2000 Emulator
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
//   This class implements the MS2000 specific user interface (as opposed to the
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
//  5-MAR-24  RLA   Adapted from SBC1802.
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
    // LOAD/SAVE file formats ...
    FILE_FORMAT_NONE    = 0,    // no format specified
    FILE_FORMAT_INTEL   = 1,    // INTEL .HEX file format
    FILE_FORMAT_BINARY  = 2,    // raw binary dump
  };

  //   The constructor and destructor for this class are conspicuously private.
  // That's because we never need to actually create an instance of this class
  // - it has no local data and the parser tables and command action routines
  // are all static.
private:
  CUI() {};
  virtual ~CUI() {};
  CUI(const CUI &) = delete;
  void operator= (const CUI &) = delete;

  //   Believe it or not, the only public hook into this whole thing (other
  // than the constructor, IS this table of verbs that we pass to the 
  // command parser ...
public:
  static CCmdVerb * const g_aVerbs[];

  // Keyword tables ...
private:
  static const CCmdArgKeyword::KEYWORD m_keysFileFormat[], m_keysStopIgnore[];

  // Argument tables ...
private:
  static CCmdArgFileName    m_argFileName, m_argOptFileName;
  static CCmdArgKeyword     m_argFileFormat, m_argStopOpcode, m_argStopIO;
  static CCmdArgNumber      m_argRunAddress, m_argStepCount; 
  static CCmdArgNumberRange m_argBreakpoint, m_argOptBreakpoint;
  static CCmdArgNumber      m_argBaseAddress, m_argByteCount;
  static CCmdArgNumber      m_argData, m_argBreakChar;
  static CCmdArgName        m_argRegisterName;
  static CCmdArgName        m_argDeviceName, m_argOptDeviceName;
  static CCmdArgNumberRange m_argAddressRange;
  static CCmdArgRangeOrName m_argExamineDeposit;
  static CCmdArgList        m_argDataList, m_argRangeList, m_argRangeOrNameList;
  static CCmdArgNumber      m_argUnit;
  static CCmdArgNumber      m_argTxSpeed, m_argRxSpeed;
  static CCmdArgNumber      m_argStepDelay, m_argRotationalDelay;
  static CCmdArgNumber      m_argTransferDelay, m_argLoadDelay, m_argUnloadDelay;

  // Modifier definitions ...
private:
  static CCmdModifier m_modFileFormat;
  static CCmdModifier m_modInstruction;
  static CCmdModifier m_modBaseAddress;
  static CCmdModifier m_modByteCount;
  static CCmdModifier m_modBreakChar;
  static CCmdModifier m_modIllegalOpcode;
  static CCmdModifier m_modIllegalIO;
  static CCmdModifier m_modCPUextended;
  static CCmdModifier m_modUnit;
  static CCmdModifier m_modReadOnly;
  static CCmdModifier m_modTxSpeed;
  static CCmdModifier m_modRxSpeed;
  static CCmdModifier m_modOverwrite;
  static CCmdModifier m_modEnable;
  static CCmdModifier m_modROM;
  static CCmdModifier m_modWriteLock;
  static CCmdModifier m_modStepDelay;
  static CCmdModifier m_modRotationalDelay;
  static CCmdModifier m_modTransferDelay;
  static CCmdModifier m_modLoadDelay;
  static CCmdModifier m_modUnloadDelay;

  // Verb definitions ...
private:
  // LOAD and SAVE commands ...
  static CCmdArgument * const m_argsLoadSave[];
  static CCmdModifier * const m_modsLoad[];
  static CCmdModifier * const m_modsSave[];
  static CCmdVerb m_cmdLoad, m_cmdSave;

  // ATTACH and DETACH commands ...
  static CCmdArgument * const m_argsAttachDiskette[];
  static CCmdModifier * const m_modsAttachDiskette[];
  static CCmdModifier * const m_modsDetachDiskette[];
  static CCmdVerb m_cmdAttachDiskette, m_cmdDetachDiskette;

  // EXAMINE and DEPOSIT commands ...
  static CCmdArgument * const m_argsDeposit[];
  static CCmdArgument * const m_argsExamine[];
  static CCmdModifier * const m_modsExamine[];
  static CCmdModifier * const m_modsDeposit[];
  static CCmdVerb m_cmdExamine, m_cmdDeposit;

  // SET, SHOW and CLEAR BREAKPOINT commands ...
  static CCmdArgument * const m_argsSetBreakpoint[];
  static CCmdArgument * const m_argsClearBreakpoint[];
  static CCmdModifier * const m_modsRAMROM[];
  static CCmdVerb m_cmdSetBreakpoint, m_cmdShowBreakpoint, m_cmdClearBreakpoint;

  // RESET, RUN, CONTINUE, STEP and INPUT commands ....
  static CCmdArgument * const m_argsStep[];
  static CCmdArgument * const m_argsRun[];
  static CCmdArgument * const m_argsInput[];
  static CCmdVerb m_cmdRun, m_cmdContinue, m_cmdStep, m_cmdReset,m_cmdInput;

  // SET, CLEAR and SHOW CPU and MEMORY ...
  static CCmdModifier * const m_modsRAMROM[];
  static CCmdModifier * const m_modsSetCPU[];
  static CCmdVerb m_cmdShowMemory, m_cmdClearMemory, m_cmdClearCPU;
  static CCmdVerb m_cmdSetCPU, m_cmdShowCPU;

  // SET, CLEAR and SHOW DEVICE ...
  static CCmdArgument * const m_argsShowDevice[];
  static CCmdArgument * const m_argsSetDevice[];
  static CCmdModifier * const m_modsSetDevice[];
  static CCmdVerb m_cmdClearDevice, m_cmdShowDevice, m_cmdSetDevice;

  // CLEAR, SET and SHOW verb definitions ...
  static CCmdVerb m_cmdShowVersion;
  static CCmdVerb * const g_aSetVerbs[];
  static CCmdVerb * const g_aShowVerbs[];
  static CCmdVerb * const g_aClearVerbs[];
  static CCmdVerb m_cmdClear, m_cmdSet, m_cmdShow;


  // Other "helper" routines ...
public:
  static bool ConfirmExit();

  // Verb action routines ....
private:
  static bool DoLoad(CCmdParser &cmd), DoSave(CCmdParser &cmd);
  static bool DoDeposit(CCmdParser &cmd), DoExamine(CCmdParser &cmd);
  static bool DoAttachDiskette(CCmdParser &cmd), DoDetachDiskette(CCmdParser &cmd);
  static bool DoRun(CCmdParser &cmd), DoContinue(CCmdParser &cmd);
  static bool DoStep(CCmdParser &cmd), DoReset(CCmdParser &cmd);
  static bool DoSetBreakpoint(CCmdParser &cmd), DoClearBreakpoint(CCmdParser &cmd);
  static bool DoShowBreakpoints(CCmdParser &cmd);
  static bool DoShowMemory(CCmdParser &cmd), DoClearMemory(CCmdParser &cmd);
  static bool DoShowDevice (CCmdParser &cmd), DoClearDevice(CCmdParser &cmd);
  static bool DoSetDevice (CCmdParser &cmd);
  static bool DoSetCPU(CCmdParser &cmd), DoClearCPU(CCmdParser &cmd);
  static bool DoShowCPU(CCmdParser &cmd);
  static bool DoShowVersion(CCmdParser &cmd);

  // Other "helper" routines ...
  static void DumpLine (address_t nStart, size_t cbBytes, unsigned nIndent=0, unsigned nPad=0);
  static void DoExamineRange (address_t nStart, address_t nEnd);
  static size_t DoExamineInstruction (string &sCode, address_t nStart);
  static string ExamineRegister (int nIndex);
  static bool DoExamineOneRegister (string sName);
  static void DoExamineAllRegisters (bool fBrief=false);
  static bool DoDepositRange (address_t nStart, address_t nEnd, CCmdArgList &List);
  static bool DoDepositRegister (string sName, uint16_t nValue);
  static void GetImageFileNameAndFormat (bool fCreate, string &sFileName, int &nFormat);
  static void GetImageBaseAndOffset (address_t &wBase, size_t &cbBytes);
  static bool GetUnit (uint8_t &nUnit, uint8_t nMaxUnit);
  static unsigned RunSimulation (uint32_t nSteps=0);
  static CDevice *FindDevice (const string sDevice);
  static bool ShowAllDevices();
  static void ShowOneDevice (const class CDevice *pDevice, bool fHeading=false);
  static string ShowBreakpoints();
};
