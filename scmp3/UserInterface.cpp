//++
// UserInterface.cpp -> SC/MP-III Emulator Specific User Interface code
//
// DESCRIPTION:
//   This module implements the user interface specific to the SCMP emulator
// process.  The first half of the file are parse tables for the generic command
// line parser classes from CommandParser.cpp, and the second half is the action
// routines needed to implement these commands.  
//
//                                                      Bob Armstrong [5-NOV-25]
//
// SCMP3 COMMANDS
// ---------------
//   E*XAMINE xxxx              - display just address xxxx (hex)
//      xxxx-xxxx               - display all addresses in the range
//      xxxx, xxxx, ...         - display multiple addresses or ranges
//      reg                     - display individual CPU register(s)
//      RE*GISTERS              - display all registers
//      /I*NSTRUCTION           - disassemble INS807x instructions
//   Registers - PC, SP, P2, P3, A, E, T, S
//
//   D*EPOSIT xxxx xx           - deposit one byte
//      xxxx xx, xx, ...        - deposit several bytes
//      reg xxxx                - deposit in a register
//
//   LO*AD filename             - load binary or .HEX file into RAM or ROM
//   SA*VE filename             - save RAM or ROM to a binary or .HEX file
//      /FORMAT=BINARY|INTEL    - set file format
//      /BAS*E=xxxx             - load/save relative to base address (octal)
//      /COU*NT=nnnnn           - number of bytes to save (decimal)
//      /RA*M                   - load/save data from RAM address space
//      /RO*M                   -   "   "     "    "  ROM    "      "
//      /OVER*WRITE             - don't prompt if file already exists (SAVE only!)
//
//   SE*T BRE*AKPOINT xxxx      - set breakpoint at address xxxx
//   CL*EAR BRE*AKPOINT xxxx    - clear   "      "     "     "
//   CL*EAR BRE*AKPOINTS        - clear all breakpoints
//   SH*OW BRE*AKPOINTS         - show breakpoints
//
//   RU*N [xxxx]                - clear CPU and start running at PC=xxxx
//   C*ONTINUE                  - resume execution at current PC
//   ST*EP [nnnn]               - single step and trace nnnn instructions
//   RES*ET                     - reset CPU and all devices
//
//   SH*OW CPU                  - show CPU details
//   CL*EAR CPU                 - reset the CPU only
//   SE*T CPU
//      /BRE*AK=nnn             - set break character to ASCII code nnn
//      /OPCODE=STOP|IGNORE     -  "    "   "     "   "  opcodes
//      /CLO*CK=nnnnnnnnn       - set CPU clock frequency (in Hz!)
//
//   SH*OW MEM*ORY              - show memory configuration
//   CL*EAR MEM*ORY             - clear (zero) ALL memory (RAM and ROM!)
//      /RAM                    - clear RAM only
//      /ROM                    - clear ROM only
//   SET MEM*ORY xxxx-yyyy      - set memory characteristics for xxxx to yyyy
//      /RAM                    - memory is read/write
//      /ROM                    - memory is read only
//      /NORAM/NOROM            - memory doesn't exist
//      /FAST                   - memory is internal to the INX807x
//      /SLOW                   - memory is external
//
//   ATT*ACH SER*IAL sense flag - attach software (bit banged) serial port
//   SHOW SER*IAL               - show software serial characteristics
//   SET SER*IAL                - set software serial port characteristics
//      /BAUD=nnnn              - simulated baud rate
//      /[NO]INVERT=TX|RX|BOTH  - specify polarity of serial signals
//      /POLL=nnnn              - polling delay in microseconds
//   DET*ACH SER*IAL            - disconnect software serial emulation
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
// NOTES
// -----
//   UPPERCASE names are keywords.  lowercase names are arguments.
//   A "*" indicates the minimum unique abbreviation.
//   "nnnn" is a decimal number
//   "xxxx" is a hexadecimal number
//   "[...]" indicates optional arguments
//    
// REVISION HISTORY:
// 23-JUL-19  RLA   New file.
//  5-NOV-25  RLA   Revised for SC/MP-III (INS807x)
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
#include "ConsoleWindow.hpp"    // needed for CVirtualConsole 
#include "SCMP3.hpp"             // global declarations for this project
#include "StandardUI.hpp"       // emulator library standard UI commands
#include "UserInterface.hpp"    // emulator user interface parse table definitions
#include "MemoryTypes.h"        // address_t and word_t data types
#include "Memory.hpp"           // emulated memory functions
#include "INS8070opcodes.hpp"   // ditto for the INS8070
#include "CPU.hpp"              // CCPU base class definitions
#include "INS8070.hpp"          // SC/MP-III CPU emulation
#include "Device.hpp"           // CDevice I/O device emulation objects
#include "SoftwareSerial.hpp"   // bit banged serial port emulation


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

// STOP or IGNORE options for "SET CPU" ...
const CCmdArgKeyword::KEYWORD CUI::m_keysStopIgnore[] = {
  {"ST*OP", true}, {"IGN*ORE", false}, {NULL, 0}
};

// Sense input and flag output names for the SC/MP ...
const CCmdArgKeyword::KEYWORD CUI::m_keysSenseInputs[] = {
  {"SENSEA", CSCMP3::SENSEA},  {"SENSEB", CSCMP3::SENSEB},
  {NULL, 0}
};
const CCmdArgKeyword::KEYWORD CUI::m_keysFlagOutputs[] = {
  {"FLAG1", CSCMP3::FLAG1},  {"FLAG2", CSCMP3::FLAG2},
  {"FLAG3", CSCMP3::FLAG3},  {NULL, 0}
};

// "RX", "TX", "BOTH" or "NONE" keywords for /INVERT= ...
const CCmdArgKeyword::KEYWORD CUI::m_keysTXRXboth[] = {
  {"RX", 1}, {"TX", 2}, {"BOTH", 3}, {"NONE", 0}, {NULL, 0}
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
CCmdArgNumber      CUI::m_argBaudRate("bits per second", 10, 110, 4800UL);
CCmdArgNumber      CUI::m_argPollDelay("poll delay", 10, 1, 1000000UL);
CCmdArgNumber      CUI::m_argBreakChar("break character", 10, 1, 31);
CCmdArgKeyword     CUI::m_argStopIO("stop on illegal I/O", m_keysStopIgnore);
CCmdArgKeyword     CUI::m_argStopOpcode("stop on illegal opcode", m_keysStopIgnore);
CCmdArgNumber      CUI::m_argBaseAddress("starting address", 16, 0, MEMSIZE-1);
CCmdArgNumber      CUI::m_argByteCount("byte count", 10, 0, MEMSIZE-1);
CCmdArgKeyword     CUI::m_argSenseInput("sense input", m_keysSenseInputs);
CCmdArgKeyword     CUI::m_argFlagOutput("flag output", m_keysFlagOutputs);
CCmdArgKeyword     CUI::m_argOptTXRXboth("TX, RX or BOTH", m_keysTXRXboth, true);
CCmdArgNumber      CUI::m_argFrequency("frequency", 10, 1, UINT32_MAX);

// Modifier definitions ...
//   Like command arguments, modifiers may be shared by several commands...-
CCmdModifier CUI::m_modFileFormat("FORM*AT", NULL, &m_argFileFormat);
CCmdModifier CUI::m_modInstruction("I*NSTRUCTION");
CCmdModifier CUI::m_modBaudRate("BAUD", NULL, &m_argBaudRate);
CCmdModifier CUI::m_modInvertData("INV*ERT", "NOINV*ERT", &m_argOptTXRXboth);
CCmdModifier CUI::m_modPollDelay("POLL", NULL, &m_argPollDelay);
CCmdModifier CUI::m_modBreakChar("BRE*AK", NULL, &m_argBreakChar);
CCmdModifier CUI::m_modIllegalOpcode("OP*CODE", NULL, &m_argStopOpcode);
CCmdModifier CUI::m_modRAM("RAM", "NORAM");
CCmdModifier CUI::m_modROM("ROM", "NOROM");
CCmdModifier CUI::m_modBaseAddress("BAS*E", NULL, &m_argBaseAddress);
CCmdModifier CUI::m_modByteCount("COU*NT", NULL, &m_argByteCount);
CCmdModifier CUI::m_modOverwrite("OVER*WRITE", "NOOVER*WRITE");
CCmdModifier CUI::m_modClockFrequency("CLO*CK", NULL, &m_argFrequency);
CCmdModifier CUI::m_modFastSlow("FA*ST", "SL*OW");

// LOAD and SAVE verb definitions ...
CCmdArgument * const CUI::m_argsLoadSave[] = {&m_argFileName, NULL};
CCmdModifier * const CUI::m_modsLoadSave[] = {&m_modFileFormat, &m_modBaseAddress, &m_modByteCount,
                                              &m_modOverwrite, &m_modRAM, &m_modROM, NULL};
CCmdVerb CUI::m_cmdLoad("LO*AD", &DoLoad, m_argsLoadSave, m_modsLoadSave);
CCmdVerb CUI::m_cmdSave("SA*VE", &DoSave, m_argsLoadSave, m_modsLoadSave);

// ATTACH and DETACH commands ...
CCmdArgument * const CUI::m_argsAttachSerial[] = {&m_argSenseInput, &m_argFlagOutput, NULL};
CCmdModifier * const CUI::m_modsAttach[] = {NULL};
CCmdVerb CUI::m_cmdAttachSerial("SER*IAL", &DoAttachSerial, m_argsAttachSerial, m_modsAttach);
CCmdVerb CUI::m_cmdDetachSerial("SER*IAL", &DoDetachSerial);
CCmdVerb * const CUI::g_aAttachVerbs[] = {&m_cmdAttachSerial, NULL};
CCmdVerb * const CUI::g_aDetachVerbs[] = {&m_cmdDetachSerial, NULL};
CCmdVerb CUI::m_cmdAttach("ATT*ACH", NULL, NULL, NULL, g_aAttachVerbs);
CCmdVerb CUI::m_cmdDetach("DET*ACH", NULL, NULL, NULL, g_aDetachVerbs);

// EXAMINE and DEPOSIT verb definitions ...
CCmdArgument * const CUI::m_argsExamine[] = {&m_argRangeOrNameList, NULL};
CCmdArgument * const CUI::m_argsDeposit[] = {&m_argExamineDeposit, &m_argDataList, NULL};
CCmdModifier * const CUI::m_modsExamine[] = {&m_modInstruction, NULL};
CCmdVerb CUI::m_cmdDeposit("D*EPOSIT", &DoDeposit, m_argsDeposit, NULL);
CCmdVerb CUI::m_cmdExamine("E*XAMINE", &DoExamine, m_argsExamine, m_modsExamine);

// RUN, CONTINUE, STEP and RESET commands ...
CCmdArgument * const CUI::m_argsStep[] = {&m_argStepCount, NULL};
CCmdArgument * const CUI::m_argsRun[] = {&m_argRunAddress, NULL};
CCmdVerb CUI::m_cmdRun("RU*N", &DoRun, m_argsRun, NULL);
CCmdVerb CUI::m_cmdContinue("C*ONTINUE", &DoContinue, NULL, NULL);
CCmdVerb CUI::m_cmdStep("ST*EP", &DoStep, m_argsStep, NULL);
CCmdVerb CUI::m_cmdReset("RE*SET", &DoReset);

// CLEAR command ...
CCmdArgument * const CUI::m_argsClearBreakpoint[] = {&m_argOptBreakpoint, NULL};
CCmdVerb CUI::m_cmdClearBreakpoint("BRE*AKPOINT", &DoClearBreakpoint, m_argsClearBreakpoint, NULL);
CCmdModifier * const CUI::m_modsClearMemory[] = {&m_modROM, &m_modRAM, NULL};
CCmdVerb CUI::m_cmdClearMemory("MEM*ORY", &DoClearMemory, NULL, m_modsClearMemory);
CCmdVerb CUI::m_cmdClearCPU("CPU", *DoClearCPU);
CCmdVerb * const CUI::g_aClearVerbs[] = {&m_cmdClearBreakpoint, &m_cmdClearCPU, &m_cmdClearMemory, NULL};
CCmdVerb CUI::m_cmdClear("CL*EAR", NULL, NULL, NULL, g_aClearVerbs);

// SET verb definition ...
CCmdArgument * const CUI::m_argsSetBreakpoint[] = {&m_argBreakpoint, NULL};
CCmdVerb CUI::m_cmdSetBreakpoint("BRE*AKPOINT", &DoSetBreakpoint, m_argsSetBreakpoint, NULL);
CCmdArgument * const CUI::m_argsSetMemory[] = {&m_argRangeList, NULL};
CCmdModifier * const CUI::m_modsSetMemory[] = {&m_modRAM, &m_modROM, &m_modFastSlow, NULL};
CCmdModifier * const CUI::m_modsSetSerial[] = {&m_modBaudRate, &m_modInvertData, &m_modPollDelay, NULL};
CCmdModifier * const CUI::m_modsSetCPU[] = {&m_modIllegalOpcode, &m_modBreakChar, &m_modClockFrequency, NULL};
CCmdVerb CUI::m_cmdSetCPU = {"CPU", &DoSetCPU, NULL, m_modsSetCPU};
CCmdVerb CUI::m_cmdSetMemory = {"MEM*ORY", &DoSetMemory, m_argsSetMemory, m_modsSetMemory};
CCmdVerb CUI::m_cmdSetSerial = {"SER*IAL", &DoSetSerial, NULL, m_modsSetSerial};
CCmdVerb * const CUI::g_aSetVerbs[] = {
  &m_cmdSetBreakpoint, &m_cmdSetCPU, &m_cmdSetMemory, &m_cmdSetSerial,
  &CStandardUI::m_cmdSetLog, &CStandardUI::m_cmdSetWindow,
  NULL
};
CCmdVerb CUI::m_cmdSet("SE*T", NULL, NULL, NULL, g_aSetVerbs);

// SHOW verb definition ...
CCmdVerb CUI::m_cmdShowBreakpoint("BRE*AKPOINT", &DoShowBreakpoints, NULL, NULL);
CCmdVerb CUI::m_cmdShowConfiguration("CONF*IGURATION", &DoShowConfiguration);
CCmdVerb CUI::m_cmdShowMemory("MEM*ORY", &DoShowMemory);
CCmdVerb CUI::m_cmdShowCPU("CPU", &DoShowCPU);
CCmdVerb CUI::m_cmdShowSerial("SER*IAL", &DoShowSerial);
CCmdVerb CUI::m_cmdShowVersion("VER*SION", &DoShowVersion);
CCmdVerb CUI::m_cmdShowAll("ALL", &DoShowAll);
CCmdVerb * const CUI::g_aShowVerbs[] = {
  &m_cmdShowBreakpoint, &m_cmdShowMemory, &m_cmdShowCPU,
  &m_cmdShowConfiguration, &m_cmdShowSerial,
  &CStandardUI::m_cmdShowLog, &m_cmdShowVersion,
  &CStandardUI::m_cmdShowAliases, &m_cmdShowAll,
  NULL
};
CCmdVerb CUI::m_cmdShow("SH*OW", NULL, NULL, NULL, g_aShowVerbs);

// Master list of all verbs ...
CCmdVerb * const CUI::g_aVerbs[] = {
  &m_cmdLoad, &m_cmdSave, &m_cmdAttach, &m_cmdDetach,
  &m_cmdExamine, &m_cmdDeposit,
  &m_cmdSet, &m_cmdShow, &m_cmdReset,
  &m_cmdClear, &m_cmdRun, &m_cmdContinue, &m_cmdStep,
//&CStandardUI::m_cmdDefine, &CStandardUI::m_cmdUndefine,
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
  // to the address specified by the file.  This is handy for things like the
  // SCMP EPROM, where the hex file is origined at 0x0000, but the code is
  // intended to run at 0x8000.  
  //
  //   There are several ways to specify the base and size.  First, the
  // user can be explicit about it with the /BASE= and /COUNT= modifiers.
  // Or he can use one of the /RAM or /ROM modifiers, which specify one of
  // the predefined RAM or EPROM areas on the SCMP.  Or lastly the /NVR
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
  else
    wBase = 0;

  // And figure out the size ...
  if (m_modByteCount.IsPresent())
    cbBytes = m_argByteCount.GetNumber();
  else if (m_modRAM.IsPresent())
    cbBytes = RAMSIZE;
  else if (m_modROM.IsPresent())
    cbBytes = ROMSIZE;
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

  // Load RAM or ROM ...
  address_t wBase = 0;  size_t cbLimit = 0;
  GetImageBaseAndOffset(wBase, cbLimit);
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
  size_t nCount = Disassemble3(g_pMemory, nStart, sCode);
  bOpcode = g_pMemory->UIread(nStart);
  address_t wNext = ADDRESS(nStart+1);
  if (nCount > 1) b2 = g_pMemory->UIread(wNext);
  wNext = ADDRESS(wNext+1);
  if (nCount > 2) b3 = g_pMemory->UIread(wNext);

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
    string sRegister = ExamineRegister(i);
    if ((sLine.length()+sRegister.length()) > 75) {
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
    g_pCPU->SetRegister(CSCMP3::REG_PC, ADDRESS(m_argRunAddress.GetNumber()-1));
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
  string sBreakpoints;  address_t nAddr = g_pMemory->Base()-1;
  while (g_pMemory->FindBreak(nAddr)) {
    sBreakpoints += sBreakpoints.empty() ? "Breakpoint(s) at " : ", ";
    char sz[16];  sprintf_s(sz, sizeof(sz), "%04X", nAddr);
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
  // Clear (reset!) the CPU only!
  //--
  g_pCPU->ClearCPU();
  return true;
}

bool CUI::DoSetCPU (CCmdParser &cmd)
{
  //++
  //   SET CPU allows you to set the CPU type (e.g. 1802 or 1805), and well as
  // vaious options (e.g. stop on illegal opcode, set clock frequency, etc).
  //--
  if (m_modIllegalOpcode.IsPresent())
    g_pCPU->StopOnIllegalOpcode(m_argStopOpcode.GetKeyValue() != 0);
  if (m_modBreakChar.IsPresent())
    g_pConsole->SetConsoleBreak(m_argBreakChar.GetNumber());
  if (m_modClockFrequency.IsPresent())
    g_pCPU->SetCrystalFrequency(m_argFrequency.GetNumber());
  return true;
}

bool CUI::DoShowCPU (CCmdParser &cmd)
{
  //++
  //   The SHOW CPU command displays the CPU name, clock frequency, internal
  // CPU registers, and the state of the interrupt system too.
  //--
  CMDOUTS(std::endl << "CPU");
  // Show general CPU information ...
  double flCrystal = g_pCPU->GetCrystalFrequency() / 1000000.0;
  double flMicroCycle = CSCMP3::CLOCKS_PER_MICROCYCLE / flCrystal;
  CMDOUTF("%s %s %3.2fMHz (%3.2fus per microcycle)",
    g_pCPU->GetName(), g_pCPU->GetDescription(), flCrystal, flMicroCycle);
  CMDOUTF("%s on illegal opcode, console break character ^%c",
    g_pCPU->IsStopOnIllegalOpcode() ? "Stop" : "Continue",
    (g_pConsole->GetConsoleBreak() + '@'));

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
  DoExamineAllRegisters();

  // Decode the status bits ...
  CMDOUTS(std::endl << "STATUS");
  uint8_t bStatus = LOBYTE(g_pCPU->GetRegister(CSCMP3::REG_S));
  string sStatus;
  if (ISSET(bStatus, CSCMP3::SR_CYL)) sStatus += "Carry/Link  ";
  if (ISSET(bStatus, CSCMP3::SR_OV )) sStatus += "Overflow  ";
  if (ISSET(bStatus, CSCMP3::SR_SB )) sStatus += "Sense B  ";
  if (ISSET(bStatus, CSCMP3::SR_SA )) sStatus += "Sense A  ";
  if (ISSET(bStatus, CSCMP3::SR_F3 )) sStatus += "Flag 3  ";
  if (ISSET(bStatus, CSCMP3::SR_F2 )) sStatus += "Flag 2  ";
  if (ISSET(bStatus, CSCMP3::SR_F1 )) sStatus += "Flag 1  ";
  if (ISSET(bStatus, CSCMP3::SR_IE )) sStatus += "Interrupt Enable ";
  if (bStatus == 0) sStatus = "NONE";
  CMDOUTS(sStatus);

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
  //   Clear memory, either RAM, ROM or both.  If only the /RAM modifier is
  // present then only RAM is cleared.  Likewise if only /ROM is present then
  // only ROM is cleared.  If neither is present, OR BOTH, then all of memory
  // (RAM and ROM) is cleared.
  //--

  // /RAM or /NOROM both clear RAM ...
  if (   (m_modRAM.IsPresent() && !m_modRAM.IsNegated())
      || (m_modROM.IsPresent() &&  m_modROM.IsNegated()))
    g_pMemory->ClearRAM();

  // /ROM or /NORAM both clear ROM ...
  if (   (m_modROM.IsPresent() && !m_modROM.IsNegated())
      || (m_modRAM.IsPresent() &&  m_modRAM.IsNegated()))
    g_pMemory->ClearROM();

  // And lastly if no modifiers are present, then clear everything!
  if (!m_modRAM.IsPresent() && !m_modROM.IsPresent())
    g_pMemory->ClearMemory();
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
  if (m_modRAM.IsPresent() || m_modROM.IsPresent()) {
    uint8_t bMemFlags = 0;

    // Figure out what we're setting memory to ...
    if (m_modRAM.IsPresent() && m_modRAM.IsNegated() && m_modROM.IsPresent() && m_modROM.IsNegated()) {
      // /NORAM and /NOROM - no memory at all!
    } else if (m_modRAM.IsPresent() && !m_modRAM.IsNegated() && !m_modROM.IsPresent()) {
      // /RAM -> read/write memory ...
      bMemFlags = CMemory::MEM_READ | CMemory::MEM_WRITE;
    } else if (m_modROM.IsPresent() && !m_modROM.IsNegated() && !m_modRAM.IsPresent()) {
      bMemFlags = CMemory::MEM_READ;
    }

    // Now go thru all the address ranges and set each one ...
    for (size_t i = 0; i < m_argRangeList.Count(); ++i) {
      CCmdArgNumberRange *pArg = dynamic_cast<CCmdArgNumberRange *> (m_argRangeList[i]);
      assert(pArg != NULL);
      address_t nStart = pArg->GetStart();
      address_t nEnd = pArg->GetEnd();
      g_pMemory->SetFlags(nStart, nEnd, bMemFlags, CMemory::MEM_READ | CMemory::MEM_WRITE);
    }
  }

  // Now handle /FAST and /SLOW too...
  if (m_modFastSlow.IsPresent()) {
    for (size_t i = 0; i < m_argRangeList.Count(); ++i) {
      CCmdArgNumberRange *pArg = dynamic_cast<CCmdArgNumberRange *> (m_argRangeList[i]);
      assert(pArg != NULL);
      address_t nStart = pArg->GetStart();
      address_t nEnd = pArg->GetEnd();
      if (m_modFastSlow.IsNegated())
        g_pMemory->SetFlags(nStart, nEnd, CMemory::MEM_SLOW, 0);
      else
        g_pMemory->SetFlags(nStart, nEnd, 0, CMemory::MEM_SLOW);
    }
  }
  return true;
}

bool CUI::DoShowMemory (CCmdParser &cmd)
{
  //++
  // Figure out (it's not too hard!) and print the memory map ...
  //--
  size_t nFirst = 0;
  CMDOUTF("\nMEMORY MAP");
  CMDOUTF("START  END    SIZE  TYPE");
  CMDOUTF("-----  -----  ----  -----------");
  while (nFirst < g_pMemory->Size()) {
    size_t nSize = g_pMemory->CountFlags(ADDRESS(nFirst));
    string sSpeed = g_pMemory->IsSlow(ADDRESS(nFirst)) ? "SLOW" : "FAST";
    if (g_pMemory->IsRAM(ADDRESS(nFirst)))
      CMDOUTF("%04X   %04X   %3dK  RAM (%s)", nFirst, nFirst+nSize-1, nSize>>10, sSpeed.c_str());
    else if (g_pMemory->IsROM(ADDRESS(nFirst)))
      CMDOUTF("%04X   %04X   %3dK  ROM (%s)", nFirst, nFirst+nSize-1, nSize>>10, sSpeed.c_str());
    //else
    //  CMDOUTF("%04X   %04X   %3dK     ", nFirst, nFirst + nSize - 1, nSize >> 10);
    nFirst += nSize;
  }
  CMDOUTS("");
  return true;
}


////////////////////////////////////////////////////////////////////////////////
///////////////////////////// SERIAL PORT COMMANDS /////////////////////////////
////////////////////////////////////////////////////////////////////////////////

bool CUI::IsSerialInstalled()
{
  //++
  // Return TRUE if the software serial is installed ...
  //--
  return (g_pSerial != NULL) /*&& g_pCPU->IsSerialInstalled()*/;
}

bool CUI::DoAttachSerial (CCmdParser &cmd)
{
  //++
  // Install the software serial (bit banged!) terminal emulation.
  //--
  uint16_t nSense = 0; // (uint16_t) m_argEF.GetKeyValue();
  if (IsSerialInstalled() || g_pCPU->IsSenseInstalled(nSense)) {
    CMDERRS("software serial already installed");  return false;
  }
  g_pSerial = DBGNEW CSoftwareSerial(g_pEvents, g_pConsole, g_pCPU);
  // TEMPORARY TODO NYI TBA!!!
  g_pCPU->InstallSense(g_pSerial, (uint16_t) m_argSenseInput.GetKeyValue());
  g_pCPU->InstallFlag(g_pSerial, (uint16_t) m_argFlagOutput.GetKeyValue());
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

bool CUI::DoSetSerial (CCmdParser &cmd)
{
  //++
  //   The software (aka "bit banged") serial port supports the same POLL 
  // parameter that the UART does (see DoSetUART(), above), however instead
  // of a DELAY parameter the software serial supports a BAUD rate parameter.
  // The BAUD parameter sets the number of simulated bits per second; this is
  // more convenient than setting the delay directly.
  //
  //   Software serial also supports an additional /INVERT or /NOINVERT option,
  // which determines the sense of the serial data.  /INVERT should be used if
  // the hardware uses an inverting RS232 driver, such as the MAX232 or MC1488
  // /MC1489. /NOINVERT is used for a direct, non-inverting, RS232 interface.
  //--
  if (!IsSerialInstalled()) {
    CMDERRS("serial emulation not installed");  return false;
  }
  if (m_modBaudRate.IsPresent())
    g_pSerial->SetBaud(m_argBaudRate.GetNumber());
  if (m_modPollDelay.IsPresent())
    g_pSerial->SetPollDelay(USTONS(m_argPollDelay.GetNumber()));
  if (m_modInvertData.IsPresent()) {
    //   This is a bit of a kludge, but remember that it's possible to invert
    // either the TX data, RX data, both or neither.  The value for the keyword
    // argument to /INVERT is arranged so that bit 0 corresponds to RX and bit
    // 1 to TX.  If no argument is specified for /INVERT, then "BOTH" is
    // assumed.  If the NO prefix is used, then the choice is inverted.
    int bInvert = m_argOptTXRXboth.IsPresent() ? (int) m_argOptTXRXboth.GetKeyValue() : 3;
    if (m_modInvertData.IsNegated()) bInvert ^= 3;
    g_pSerial->SetInvert(ISSET(bInvert, 2), ISSET(bInvert, 1));
  }
  return true;
}

bool CUI::DoShowSerial (CCmdParser &cmd)
{
  //++
  //--
  ostringstream ofs;
  CMDOUTS(std::endl << "SERIAL PORT EMULATION");
  g_pSerial->ShowDevice(ofs);
  CMDOUT(ofs);
  return true;
}


////////////////////////////////////////////////////////////////////////////////
/////////////////////////// MISCELLANEOUS COMMANDS /////////////////////////////
////////////////////////////////////////////////////////////////////////////////

bool CUI::DoShowConfiguration (CCmdParser &cmd)
{
  //++
  // Show the configuration of the CPU and all installed devices ...
  //--
  double flCrystal = g_pCPU->GetCrystalFrequency() / 1000000.0;
  CMDOUTF("\nDEVICE    DESCRIPTION                PORT          TYPE    CHARACTERISTICS");
  CMDOUTF("--------  -------------------------  ------------  ------  -------------------------------------");
  CMDOUTF("%-8s  %-25s                CPU     %3.1fMHz, BREAK=^%c", g_pCPU->GetName(), g_pCPU->GetDescription(),
          flCrystal, (g_pConsole->GetConsoleBreak()+'@'));
  if (IsSerialInstalled())   CMDOUTF("%-8s  %-25s  %6s/%-6s INOUT   INVERT=%s, BAUD=%ld, POLL=%ldus", 
                                     g_pSerial->GetName(), g_pSerial->GetDescription(),
                                     g_pCPU->GetSenseName(g_pCPU->FindSense(g_pSerial)),
                                     g_pCPU->GetFlagName(g_pCPU->FindFlag(g_pSerial)),
                                     g_pSerial->IsRXinverted() ? (g_pSerial->IsTXinverted() ? "BOTH" : "RX") : (g_pSerial->IsTXinverted() ? "TX" : "NONE"),
                                     g_pSerial->GetBaud(), NSTOUS(g_pSerial->GetPollDelay()));

  CMDOUTS("");
  return true;
}

bool CUI::DoShowVersion (CCmdParser &cmd)
{
  //++
  // Show just the version number ...
  //--
  CMDOUTF("\nSC/MP Emulator v%d\n", SCMPVER);
  return true;
}

bool CUI::DoShowAll(CCmdParser& cmd)
{
  //++
  // Show everything!
  //--
  DoShowVersion(cmd);
  DoShowConfiguration(cmd);
  DoShowCPU(cmd);
  DoShowMemory(cmd);
  DoShowSerial(cmd);
  CStandardUI::DoShowLog(cmd);
  CStandardUI::DoShowAllAliases(cmd);
  return true;
}
