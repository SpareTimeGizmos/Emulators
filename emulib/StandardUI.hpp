//++
// StandardUI.hpp -> Standard User Interface Commands
//
//   COPYRIGHT (C) 2015-2020 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
//
// LICENSE:
//    This file is part of the emulator library project.  EMULIB is free
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
//   This module contains standard user interface commands that are shared
// by all emulator implementations.
//
// Bob Armstrong <bob@jfcl.com>   [12-JUN-2015]
//
// REVISION HISTORY:
// 12-JUN-15  RLA   New file.
// 17-JUN-15  RLA   Add command alias support.
// 22-OCT-15  RLA   Add SET WINDOW command.
// 29-OCT-15  RLA   Add SET CHECKPOINT command.
// 29-OCT-15  RLA   Add the SET/SHOW CHECKPOINT commands.
// 26-AUG-22  RLA   Clean up Linux/WIN32 conditionals.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#pragma once
#include <string>               // C++ std::string class, et al ...
using std::string;              // ...

class CStandardUI {
  //++
  //--

  // This class can never be instanciated, so all constructors are hidden ...
private:
  CStandardUI() {};
  virtual ~CStandardUI() {};
  CStandardUI (const CStandardUI &) = delete;
  void operator= (const CStandardUI &) = delete;

  // Keyword tables ...
public:
  static const CCmdArgKeyword::KEYWORD m_keysVerbosity[];
  static const CCmdArgKeyword::KEYWORD m_keysColor[];

  // Argument tables ...
public:
  static CCmdArgName m_argAlias, m_argOptAlias;
  static CCmdArgKeyword m_argVerbosity, m_argForeground, m_argBackground;
  static CCmdArgFileName m_argFileName, m_argOptFileName;
  static CCmdArgString m_argSubstitution, m_argTitle;
  static CCmdArgNumber m_argRows, m_argColumns, m_argInterval;
#if defined(_WIN32)
  static CCmdArgNumber m_argX, m_argY;
#endif

  // Modifier definitions ...
public:
  static CCmdModifier m_modVerbosity, m_modNoFile, m_modConsole, m_modAppend;
  static CCmdModifier m_modRows, m_modColumns, m_modTitle;
#if defined(_WIN32)
  static CCmdModifier m_modX, m_modY;
#endif
  static CCmdModifier m_modForeground, m_modBackground, m_modEnable;
  static CCmdModifier m_modInterval;

  // Verb definitions ...
public:
  //   SET and SHOW LOG, SET and SHOW CHECKPOINT, SET WINDOW and
  // SHOW ALIASES verb definitions ...
  static CCmdModifier * const m_modsSetLog[];
  static CCmdModifier * const m_modsSetWindow[];
  static CCmdModifier * const m_modsSetCheckpoint[];
  static CCmdArgument * const m_argsShowAliases[];
  static CCmdVerb m_cmdSetLog, m_cmdSetWindow, m_cmdSetCheckpoint;
  static CCmdVerb m_cmdShowLog, m_cmdShowAliases, m_cmdShowCheckpoint;

  // DEFINE and UNDEFINE verb definitions ...
public:
  static CCmdArgument * const m_argsDefine[];
  static CCmdArgument * const m_argsUndefine[];
  static CCmdVerb m_cmdDefine, m_cmdUndefine;

  // DO verb definition ...
public:
  static CCmdArgument * const m_argsIndirect[];
  static CCmdVerb m_cmdIndirect;

  // EXIT verb definition ...
public:
  static CCmdVerb m_cmdExit, m_cmdQuit;

  // Verb action routines ....
public:
  static bool DoSetLog(CCmdParser &cmd), DoSetWindow(CCmdParser &cmd);
  static bool DoIndirect(CCmdParser &cmd), DoExit(CCmdParser &cmd);
  static bool DoDefine(CCmdParser &cmd), DoUndefine(CCmdParser &cmd);
  static bool DoSetCheckpoint(CCmdParser &cmd), DoShowAliases(CCmdParser &cmd);
  static bool DoShowOneAlias(CCmdParser &cmd, string sAlias);
  static bool DoShowLog(CCmdParser &cmd), DoShowCheckpoint(CCmdParser &cmd);
  static bool DoShowAllAliases(CCmdParser &cmd);

  // Other "helper" routines ...
public:
  // Parse the initial program arguments from the shell ...
  static bool ParseOptions (const char *pszProgram, int argc, const char * const argv[]);
  // Create an independent copy of this process ...
  static bool DetachProcess (string sCommand);
  // "Abbreviate" a string for printing ...
  static string Abbreviate (string str, uint32_t max);
  // Show a table of color names for SET WINDOW ...
  static void DoHelpColors();

  // Other global data ...
public:
  static CCommandLine g_oShellCommand;   // original argc/argv shell command
  static string       g_sStartupScript;  // startup script file (if any)
};
