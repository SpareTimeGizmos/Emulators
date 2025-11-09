//++
// UserInterface.hpp -> UserInterface for SC/MP Emulator
//
//   This class implements the SC/MP specific user interface (as opposed to the
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
    // LOAD/SAVE file formats ...
    FILE_FORMAT_NONE    = 0,    // no format specified
    FILE_FORMAT_INTEL   = 1,    // INTEL .HEX file format
    FILE_FORMAT_BINARY  = 2     // raw binary dump
  };

  //   The constructor and destructor for this class are conspicuously private.
  // That's because we never need to actually create an instance of this class
  // - it has no local data and the parser tables and command action routines
  // are all static.
private:
  CUI() {};
  ~CUI() {};
  CUI(const CUI &);
  void operator= (const CUI &) = delete;

  //   Believe it or not, the only public hook into this whole thing (other
  // than the constructor, IS this table of verbs that we pass to the 
  // command parser ...
public:
  static CCmdVerb * const g_aVerbs[];

  // Keyword tables ...
private:
  static const CCmdArgKeyword::KEYWORD m_keysFileFormat[], m_keysStopIgnore[];
  static const CCmdArgKeyword::KEYWORD m_keysSenseInputs[], m_keysFlagOutputs[];
  static const CCmdArgKeyword::KEYWORD m_keysTXRXboth[];

  // Argument tables ...
private:
  static CCmdArgFileName    m_argFileName, m_argOptFileName;
  static CCmdArgKeyword     m_argFileFormat, m_argStopOpcode, m_argStopIO;
  static CCmdArgKeyword     m_argSenseInput, m_argFlagOutput;
  static CCmdArgKeyword     m_argOptTXRXboth;
  static CCmdArgNumber      m_argData, m_argRunAddress, m_argStepCount;
  static CCmdArgNumber      m_argBreakpoint, m_argOptBreakpoint;
  static CCmdArgNumber      m_argBaseAddress, m_argByteCount;
  static CCmdArgNumber      m_argBaudRate, m_argPollDelay, m_argBreakChar;
  static CCmdArgName        m_argRegisterName;
  static CCmdArgNumberRange m_argAddressRange;
  static CCmdArgRangeOrName m_argExamineDeposit;
  static CCmdArgList        m_argRangeList, m_argRangeOrNameList;
  static CCmdArgList        m_argDataList, m_argDelayList;
  static CCmdArgNumber      m_argFrequency;

  // Modifier definitions ...
private:
  static CCmdModifier m_modFileFormat;
  static CCmdModifier m_modInstruction;
  static CCmdModifier m_modBaudRate;
  static CCmdModifier m_modInvertData;
  static CCmdModifier m_modPollDelay;
  static CCmdModifier m_modBreakChar;
  static CCmdModifier m_modIllegalOpcode;
  static CCmdModifier m_modRAM;
  static CCmdModifier m_modROM;
  static CCmdModifier m_modBaseAddress;
  static CCmdModifier m_modByteCount;
  static CCmdModifier m_modOverwrite;
  static CCmdModifier m_modClockFrequency;

  // Verb definitions ...
private:
  // LOAD and SAVE commands ...
  static CCmdArgument * const m_argsLoadSave[];
  static CCmdModifier * const m_modsLoadSave[];
  static CCmdVerb m_cmdLoad, m_cmdSave;

  // ATTACH and DETACH commands ...
  static CCmdArgument * const m_argsAttachSerial[];
  static CCmdModifier * const m_modsAttach[];
  static CCmdVerb m_cmdDetachSerial, m_cmdAttachSerial;
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

  // CLEAR, RUN, CONTINUE and STEP commands ....
  static CCmdModifier * const m_modsClearMemory[];
  static CCmdVerb * const g_aClearVerbs[];
  static CCmdArgument * const m_argsStep[];
  static CCmdArgument * const m_argsRun[];
  static CCmdVerb m_cmdClearMemory, m_cmdClearCPU;
  static CCmdVerb m_cmdRun, m_cmdContinue, m_cmdStep;
  static CCmdVerb m_cmdClear, m_cmdReset;

  // SET and SHOW verb definitions ...
  static CCmdArgument * const m_argsSetMemory[];
  static CCmdModifier * const m_modsSetSerial[];
  static CCmdModifier * const m_modsSetCPU[];
  static CCmdModifier * const m_modsSetMemory[];
  static CCmdVerb * const g_aSetVerbs[];
  static CCmdVerb * const g_aShowVerbs[];
  static CCmdVerb m_cmdSet, m_cmdShow, m_cmdShowCPU;
  static CCmdVerb m_cmdShowAll, m_cmdShowVersion;
  static CCmdVerb m_cmdShowConfiguration, m_cmdShowMemory;
  static CCmdVerb m_cmdShowSerial;
  static CCmdVerb m_cmdSetCPU, m_cmdSetSerial, m_cmdSetMemory;

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
  static bool DoAttachSerial(CCmdParser &cmd), DoDetachSerial(CCmdParser &cmd);
  static bool DoShowConfiguration(CCmdParser &cmd), DoSetMemory(CCmdParser &cmd);
  static bool DoClearMemory(CCmdParser &cmd), DoReset(CCmdParser &cmd);
  static bool DoSetCPU(CCmdParser &cmd), DoSetSerial(CCmdParser &cmd);
  static bool DoShowCPU(CCmdParser &cmd), DoShowSerial(CCmdParser &cmd);

  // Other "helper" routines ...
  static bool IsSerialInstalled();
  static void DumpLine(address_t nStart, size_t cbBytes, unsigned nIndent=0, unsigned nPad=0);
  static void DoExamineRange (address_t nStart, address_t nEnd);
  static size_t DoExamineInstruction (address_t nStart);
  static string ExamineRegister (int nIndex);
  static bool DoExamineOneRegister (string sName);
  static void DoExamineAllRegisters();
  static bool DoDepositRange (address_t nStart, address_t nEnd, CCmdArgList &List);
  static bool DoDepositRegister (string sName, uint16_t nValue);
  static void GetImageFileNameAndFormat (bool fCreate, string &sFileName, int &nFormat);
  static void GetImageBaseAndOffset (address_t&wBase, size_t &cbBytes);
  static unsigned RunSimulation (uint32_t nSteps=0);
};
