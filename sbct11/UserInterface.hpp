//++
// UserInterface.hpp -> UserInterface for SBCT11 Emulator
//
// LICENSE:
//    This file is part of the emulator library project.  SBCT11 is free
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
//   This class implements the SBCT11 specific user interface (as opposed to the
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
#include "MemoryTypes.h"        // address_t and word_t types
#include "Memory.hpp"           // we need CMemory defined
using std::string;              // ...

class CUI {
  //++
  //--

  // Command scanner constants ...
public:
  enum {
    // LOAD/SAVE file formats ...
    FILE_FORMAT_NONE,         // no format specified
    FILE_FORMAT_INTEL,        // INTEL .HEX file format
    FILE_FORMAT_BINARY,       // raw binary dump
    FILE_FORMAT_PAPERTAPE,    // DEC absolute loader format
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
  static const CCmdArgKeyword::KEYWORD m_keysFileFormat[];

  // Argument tables ...
private:
  static CCmdArgFileName    m_argFileName, m_argOptFileName;
  static CCmdArgKeyword     m_argFileFormat;
  static CCmdArgNumber      m_argData, m_argRunAddress, m_argStepCount;
  static CCmdArgNumber      m_argBreakpoint, m_argOptBreakpoint;
  static CCmdArgNumber      m_argBaseAddress, m_argByteCount, m_argDelay;
  static CCmdArgNumber      m_argPollDelay, m_argBreakChar, m_argCPUmode;
  static CCmdArgName        m_argRegisterName, m_argOptDeviceName;
  static CCmdArgNumberRange m_argAddressRange;
  static CCmdArgRangeOrName m_argExamineDeposit;
  static CCmdArgList        m_argRangeList, m_argRangeOrNameList;
  static CCmdArgList        m_argDataList, m_argDelayList;
  static CCmdArgNumber      m_argUnit, m_argCapacity;

  // Modifier definitions ...
private:
  static CCmdModifier m_modFileFormat;
  static CCmdModifier m_modInstruction;
  static CCmdModifier m_modPollDelay;
  static CCmdModifier m_modBreakChar;
  static CCmdModifier m_modCPUmode;
  static CCmdModifier m_modBaseAddress;
  static CCmdModifier m_modByteCount;
  static CCmdModifier m_modWordByte;
  static CCmdModifier m_modROM;
  static CCmdModifier m_modNVR;
  static CCmdModifier m_modReadOnly;
  static CCmdModifier m_modCapacity;
  static CCmdModifier m_modUnit;
  static CCmdModifier m_modOverwrite;
  static CCmdModifier m_modClose;
  static CCmdModifier m_modText;
  static CCmdModifier m_modXModem;
  static CCmdModifier m_modAppend;
  static CCmdModifier m_modCRLF;
  static CCmdModifier m_modDelayList;

  // Verb definitions ...
private:
  // LOAD and SAVE commands ...
  static CCmdArgument * const m_argsLoadSave[];
  static CCmdModifier * const m_modsLoadSave[];
  static CCmdVerb m_cmdLoad, m_cmdSave;

  // ATTACH and DETACH commands ...
  static CCmdArgument * const m_argsAttach[];
  static CCmdModifier * const m_modsAttachDisk[];
  static CCmdModifier * const m_modsAttachTape[];
  static CCmdModifier * const m_modsDetachTape[];
  static CCmdVerb m_cmdAttachDisk, m_cmdDetachDisk;
  static CCmdVerb m_cmdAttachTape, m_cmdDetachTape;
  static CCmdVerb * const g_aAttachVerbs[];
  static CCmdVerb * const g_aDetachVerbs[];
  static CCmdVerb m_cmdAttach, m_cmdDetach;

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

  // RESET, RUN, CONTINUE, STEP and HALT commands ....
  static CCmdArgument * const m_argsStep[];
  static CCmdArgument * const m_argsRun[];
  static CCmdVerb m_cmdRun, m_cmdContinue, m_cmdStep;
  static CCmdVerb m_cmdReset, m_cmdHalt;

  // SET, CLEAR and SHOW DEVICE, CPU amd MEMORY ...
  static CCmdModifier * const m_modsSetCPU[];
  static CCmdArgument * const m_argsShowDevice[];
  static CCmdVerb m_cmdClearMemory, m_cmdShowMemory;
  static CCmdVerb m_cmdClearDevice, m_cmdShowDevice;
  static CCmdVerb m_cmdClearCPU, m_cmdSetCPU, m_cmdShowCPU;

  // CLEAR, SET and SHOW verb definitions ...
  static CCmdVerb * const g_aClearVerbs[];
  static CCmdVerb m_cmdClear;
  static CCmdVerb * const g_aSetVerbs[];
  static CCmdVerb * const g_aShowVerbs[];
  static CCmdVerb m_cmdSet, m_cmdShow;
  static CCmdVerb m_cmdShowTime, m_cmdShowVersion;
  static CCmdVerb m_cmdShowDisk, m_cmdShowTape;

  // SEND and RECEIVE commands ...
  static CCmdArgument* const m_argsSendFile[];
  static CCmdArgument* const m_argsReceiveFile[];
  static CCmdModifier* const m_modsSendFile[];
  static CCmdModifier* const m_modsReceiveFile[];
  static CCmdVerb m_cmdSendFile, m_cmdReceiveFile;

  // Other "helper" routines ...
public:
  static bool ConfirmExit();

  // Verb action routines ....
private:
  static bool DoLoad(CCmdParser &cmd), DoSave(CCmdParser &cmd);
  static bool DoLoadNVR(CCmdParser &cmd), DoSaveNVR(CCmdParser &cmd);
  static bool DoDeposit(CCmdParser &cmd), DoExamine(CCmdParser &cmd);
  static bool DoAttachDisk(CCmdParser &cmd), DoDetachDisk(CCmdParser &cmd);
  static bool DoAttachTape(CCmdParser &cmd), DoDetachTape(CCmdParser &cmd);
  static bool DoRun(CCmdParser &cmd), DoContinue(CCmdParser &cmd);
  static bool DoStep(CCmdParser &cmd), DoReset(CCmdParser &cmd);
  static bool DoHalt(CCmdParser &cmd);
  static bool DoSetBreakpoint(CCmdParser &cmd), DoClearBreakpoint(CCmdParser &cmd);
  static bool DoShowBreakpoints(CCmdParser& cmd);
  static bool DoShowMemory(CCmdParser &cmd), DoClearMemory(CCmdParser &cmd);
  static bool DoShowDevice (CCmdParser &cmd), DoClearDevice(CCmdParser &cmd);
  static bool DoSetCPU(CCmdParser &cmd), DoClearCPU(CCmdParser &cmd);
  static bool DoShowCPU(CCmdParser &cmd);
  static bool DoShowTime(CCmdParser &cmd), DoShowVersion(CCmdParser &cmd);
  static bool DoShowDisk(CCmdParser &cmd), DoShowTape(CCmdParser &cmd);
  static bool DoSendFile(CCmdParser &cmd), DoReceiveFile(CCmdParser &cmd);

  // Other "helper" routines ...
  static class CGenericMemory *GetMemorySpace();
  static void DumpLine (address_t nStart, size_t cbBytes, unsigned nIndent=0, unsigned nPad=0);
  static void DoExamineRange (address_t nStart, address_t nEnd);
  static uint16_t DoExamineInstruction (string &sCode, uint16_t wLoc, class CGenericMemory *pMemory);
  static string ExamineRegister (int nIndex);
  static bool DoExamineOneRegister (string sName);
  static void DoExamineAllRegisters();
  static bool DoDepositRange (address_t nStart, address_t nEnd, CCmdArgList &List);
  static bool DoDepositRegister (string sName, uint16_t nValue);
  static void GetImageFileNameAndFormat (bool fCreate, string &sFileName, int &nFormat);
  static void GetImageBaseAndOffset (address_t&wBase, size_t &cbBytes);
  static bool GetUnit (uint8_t &nUnit, uint8_t nMaxUnit);
  static unsigned RunSimulation (uint32_t nSteps=0);
  static CDevice *FindDevice (const string sDevice);
  static bool ShowAllDevices();
  static void ShowOneDevice (bool fHeading, const class CDevice *pDevice);
  static string ShowBreakpoints (const CGenericMemory *pMemory);
  static bool DoCloseSend(CCmdParser &cmd), DoCloseReceive(CCmdParser &cmd);
};
