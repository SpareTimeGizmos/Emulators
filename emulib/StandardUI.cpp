//++
// StandardUI.cpp -> Standard User Interface Commands
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
// by all emulator implementations.  That includes these commands -
//
//      SET LOG ...
//      SHOW LOG ...
//      SET CHECKPOINT ...
//      SHOW CHECKPOINT ...
//      DEFINE ...
//      UNDEFINE ...
//      SHOW ALIAS ...
//      SET WINDOW ...
//      DO ...
//      EXIT ...
//
//   Notice that this class only contains the parser tables and code for these
// commands - it's still up to each application to add the appropriate entries
// to their command tables.
//
//   Lastly, the HELP command is also common to all applications but you won't
// find it here.  That particular command is built into CCmdParser class.
//
// Bob Armstrong <bob@jfcl.com>   [12-JUN-2015]
//
// REVISION HISTORY:
// 12-JUN-15  RLA   New file.
// 17-JUN-15  RLA   Add command alias support.
// 22-OCT-15  RLA   Add SET WINDOW command.
// 24-OCT-15  RLA   Add DetachProcess() and -x option.
// 29-OCT-15  RLA   Add the SET/SHOW CHECKPOINT commands.
//  2-JUN-17  RLA   Linux port.
// 26-AUG-22  RLA   Clean up Linux/WIN32 conditionals.
//  9-FEB-24  RLA   Add THREADS conditional.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include <string.h>             // strcpy(), strerror(), etc ...
#include <cstring>              // needed for memset()
#if defined(_WIN32)
#include <windows.h>            // WIN32 API for GetModuleFileName() ...
#include <process.h>            // needed for CreateProcess(), et al ...
#endif
#include "EMULIB.hpp"           // emulator library definitions
#include "SafeCRT.h"		// replacements for Microsoft "safe" CRT functions
#include "LogFile.hpp"          // emulator library message logging facility
#ifdef THREADS
#include "CheckpointFiles.hpp"  // emulator library file checkpoint facility
#endif
#include "CommandLine.hpp"      // CCommandLine (argc/argv) parser
#include "CommandParser.hpp"    // emulator library command line parsing methods
#include "ConsoleWindow.hpp"    // WIN32 console window functions
#include "StandardUI.hpp"       // declarations for this module


// The original shell command that invoked this program  ...
CCommandLine CStandardUI::g_oShellCommand("dlx", 0, 1, false, "-/");
// And the name of the startup script from the shell command ...
std::string  CStandardUI::g_sStartupScript;


// UI class object constructor cheat sheet!
//   CCmdArgument    (<name>, [<optional>])
//   CCmdArgName     (<name>, [<optional>])
//   CCmdArgNumber   (<name>, <radix>, <min>, <max>, [<optional>])
//   CCmdArgKeyword  (<name>, <keywords>, [<optional>])
//   CCmdArgFileName (<name>, [<optional>])
//   CCmdModifier    (<name>, [<"NO" name>], [<arg>], [<optional>])
//   CCmdVerb        (<name>, <action>, [<args>], [<mods>], [<subverbs>])

// Message level keywords ...
const CCmdArgKeyword::KEYWORD CStandardUI::m_keysVerbosity[] = {
    {"ERR*ORS",   CLog::ERROR},
    {"WARN*INGS", CLog::WARNING},
    {"DEB*UG",    CLog::DEBUG},
    {"TRA*CE",    CLog::TRACE},
    {NULL, 0}
};

// Color keywords ...
const CCmdArgKeyword::KEYWORD CStandardUI::m_keysColor[] = {
  {"BLACK",        CConsoleWindow::BLACK},
  {"DARK_BLUE",    CConsoleWindow::DARK_BLUE},
  {"DARK_GREEN",   CConsoleWindow::DARK_GREEN},
  {"DARK_CYAN",    CConsoleWindow::DARK_CYAN},
  {"DARK_RED",     CConsoleWindow::DARK_RED},
  {"DARK_MAGENTA", CConsoleWindow::DARK_MAGENTA},
  {"ORANGE",       CConsoleWindow::ORANGE},
  {"LIGHT_GRAY",   CConsoleWindow::LIGHT_GRAY},
  {"GRAY",         CConsoleWindow::GRAY},
  {"BLUE",         CConsoleWindow::BLUE},
  {"GREEN",        CConsoleWindow::GREEN},
  {"CYAN",         CConsoleWindow::CYAN},
  {"RED",          CConsoleWindow::RED},
  {"MAGENTA",      CConsoleWindow::MAGENTA},
  {"YELLOW",       CConsoleWindow::YELLOW},
  {"WHITE",        CConsoleWindow::WHITE},
  {NULL, 0}
};

// Argument definitions ...
CCmdArgFileName   CStandardUI::m_argFileName("file name");
CCmdArgFileName   CStandardUI::m_argOptFileName("file name", true);
CCmdArgKeyword    CStandardUI::m_argVerbosity("message level", m_keysVerbosity);
CCmdArgName       CStandardUI::m_argAlias("alias");
CCmdArgName       CStandardUI::m_argOptAlias("alias",true);
CCmdArgString     CStandardUI::m_argSubstitution("substitution");
CCmdArgKeyword    CStandardUI::m_argForeground("color name", m_keysColor);
CCmdArgKeyword    CStandardUI::m_argBackground("color name", m_keysColor);
#if defined(_WIN32)
CCmdArgNumber     CStandardUI::m_argX("screen X position", 10, 0);
CCmdArgNumber     CStandardUI::m_argY("screen Y position", 10, 0);
#endif
CCmdArgNumber     CStandardUI::m_argColumns("character columns", 10, 20, 250);
CCmdArgNumber     CStandardUI::m_argRows("character rows", 10, 5, 100);
CCmdArgString     CStandardUI::m_argTitle("window title");
CCmdArgNumber     CStandardUI::m_argInterval("interval (seconds)", 10, 1, 10000);

// Modifier definitions ...
CCmdModifier      CStandardUI::m_modVerbosity("LEV*EL", NULL, &m_argVerbosity);
CCmdModifier      CStandardUI::m_modNoFile("NOFI*LE", "FI*LE", &m_argOptFileName);
CCmdModifier      CStandardUI::m_modAppend("APP*END", "OVER*WRITE");
CCmdModifier      CStandardUI::m_modConsole("CON*SOLE");
CCmdModifier      CStandardUI::m_modTitle("TIT*LE", NULL, &m_argTitle);
CCmdModifier      CStandardUI::m_modForeground("FORE*GROUND", NULL, &m_argForeground);
CCmdModifier      CStandardUI::m_modBackground("BACK*GROUND", NULL, &m_argBackground);
#if defined(_WIN32)
CCmdModifier      CStandardUI::m_modX("X", NULL, &m_argX);
CCmdModifier      CStandardUI::m_modY("Y", NULL, &m_argY);
#endif
CCmdModifier      CStandardUI::m_modRows("H*EIGHT", NULL, &m_argRows);
CCmdModifier      CStandardUI::m_modColumns("W*IDTH", NULL, &m_argColumns);
CCmdModifier      CStandardUI::m_modEnable("ENA*BLE", "DISA*BLE");
CCmdModifier      CStandardUI::m_modInterval("INT*ERVAL", NULL, &m_argInterval);

// SET LOGGING and SHOW LOGGING verb definitions ...
CCmdModifier * const CStandardUI::m_modsSetLog[] = {&m_modNoFile, &m_modConsole, &m_modVerbosity, &m_modAppend, NULL};
CCmdVerb CStandardUI::m_cmdSetLog("LOG*GING", &DoSetLog, NULL, m_modsSetLog);
CCmdVerb CStandardUI::m_cmdShowLog("LOG*GING", &DoShowLog);

// SET WINDOW verb definitions ...
CCmdModifier * const CStandardUI::m_modsSetWindow[] = {&m_modTitle, &m_modForeground, &m_modBackground, 
#if defined(_WIN32)
                                                       &m_modX, &m_modY,
#endif
                                                       &m_modColumns, &m_modRows, NULL};
CCmdVerb CStandardUI::m_cmdSetWindow("WIN*DOW", &DoSetWindow, NULL, m_modsSetWindow);

// Set CHECKPOINT and SHOW CHECKPOINT verb definitions ...
#ifdef THREADS
CCmdModifier * const CStandardUI::m_modsSetCheckpoint[] = {&m_modEnable, &m_modInterval, NULL};
CCmdVerb CStandardUI::m_cmdSetCheckpoint("CHECK*POINT", &DoSetCheckpoint, NULL, m_modsSetCheckpoint);
CCmdVerb CStandardUI::m_cmdShowCheckpoint("CHECK*POINT", &DoShowCheckpoint, NULL, NULL);
#endif

// SHOW ALIASES verb definition ...
CCmdArgument * const CStandardUI::m_argsShowAliases[] = {&m_argOptAlias, NULL};
CCmdVerb CStandardUI::m_cmdShowAliases("ALIAS*ES", &DoShowAliases, m_argsShowAliases, NULL);

// DEFINE and UNDEFINE verb definitions ...
CCmdArgument * const CStandardUI::m_argsDefine[] = {&m_argAlias, &m_argSubstitution, NULL};
CCmdArgument * const CStandardUI::m_argsUndefine[] = {&m_argAlias, NULL};
CCmdVerb CStandardUI::m_cmdDefine("DEF*INE", &DoDefine, m_argsDefine, NULL);
CCmdVerb CStandardUI::m_cmdUndefine("UNDEF*INE", &DoUndefine, m_argsUndefine, NULL);

// DO verb definition ...
CCmdArgument * const CStandardUI::m_argsIndirect[] = {&m_argFileName, NULL};
CCmdVerb CStandardUI::m_cmdIndirect("DO", &DoIndirect, m_argsIndirect, NULL);

// EXIT verb definition ...
CCmdVerb CStandardUI::m_cmdExit("EXIT", &DoExit, NULL, NULL);
CCmdVerb CStandardUI::m_cmdQuit("QUIT", &DoExit, NULL, NULL);


bool CStandardUI::DetachProcess (string sCommand)
{
  //++
  //   This method will create a brand new process, completely independent of
  // this one, but running the same executable file and with the command line
  // arguments given by sCommand.  It's used to implement the "fork" command
  // line option.
  //--

#if defined(_WIN32)
  //   Convert the program name and command line as UNICODE strings ...
  // One thing about CreateProcess which the documentation doesn't make clear
  // is that if you pass BOTH a program name AND a command line, then the
  // latter needs to repeat the program name as the first item.  If you
  // don't, e.g. if you pass "C:\BIN\FOO.EXE" as the program name and "A B C"
  // as the command line, then "A" becomes argv[0] for the created process!
  // In other words, to the created process it looks like the first argument
  // is actually the program name!  The only way out of this that I've found
  // is to duplicate the program name and pass "C:\BIN\FOO.EXE A B C" as the
  // command line.  It's odd, but that's how it seems to work ...
  wchar_t wsz[_MAX_PATH], wszProgram[_MAX_PATH], wszCommand[_MAX_PATH];
  memset(wszProgram, 0, sizeof(wszProgram));
  memset(wszCommand, 0, sizeof(wszCommand));
  memset(wsz, 0, sizeof(wsz));
  if (GetModuleFileName(NULL, wszProgram, _MAX_PATH) == 0) return false;
  // Quote the program name, just in case it has spaces!!!
  wcscpy_s(wszCommand, _MAX_PATH, L"\"");
  wcscat_s(wszCommand, _MAX_PATH, wszProgram);
  wcscat_s(wszCommand, _MAX_PATH, L"\" ");
  MultiByteToWideChar(CP_UTF8, 0, sCommand.c_str(), -1, wsz, _MAX_PATH);
  wcscat_s(wszCommand, _MAX_PATH, wsz);

  // Initilize the STARTUP and PROCESS info structures for CreateProcess() ...
  STARTUPINFO si;  PROCESS_INFORMATION pi;
  memset(&si, 0, sizeof(si));  si.cb = sizeof(si);
  memset(&pi, 0, sizeof(pi)); 
  //   Note - a lot of the STARTUPINFO structure deals with the window title,
  // size, position, etc.  A smart thing would be to initialize all this data
  // by copying the current console characteristics.  I'll leave that for
  // later.

  // And away we go!
  if (!CreateProcess(
    wszProgram, wszCommand,             // executable and command line
    NULL, NULL, FALSE,                  // process and thread handle not inheritable
    DETACHED_PROCESS|CREATE_NO_WINDOW,  // creation flags
    NULL, NULL,                         // use parent's environment and CWD
    &si, &pi                            // STARTUPINFO and PROCESS_INFORMATION structures
    )) return false;

  //   Attempt to wait for the created process to start up before closing the
  // handles and exiting.  It's not completely clear that this is needed, but
  // just to be safe ...
  WaitForInputIdle(pi.hProcess, 10000);

  //   Close the process handles assigned.  We'll never need them again, and
  // if we don't close them we'll leak Windows resources until we exit. Closing
  // these handles has no effect on the child process!
  CloseHandle(pi.hProcess);  CloseHandle(pi.hThread);
  return true;
#elif defined(__linux__) || defined(__APPLE__) || defined(__unix__)
  // TBA TODO NYI!!!
  assert(false);
  return false;
#endif
}


bool CStandardUI::ParseOptions (const char *pszProgram, int argc, const char * const argv[])
{
  //++
  //   This method will parse the arguments from the original shell command
  // that invoked this program.  Originally the emulator programs all had only
  // a few options and they were standard between all implementations, so it
  // worked to have this be a shared function. 
  //
  //   Now we have the CCommandLine class which makes parsing arbitrary options
  // and argument lists fairly easy and some programs now handle their shell
  // command lines directly.  This version is retained, although reimplemented
  // using CCommandLine, for those programs that still need it.
  //
  //   Needless to say, this is in no way a part of the CCmdParser UI, but it
  // is another UI function and for lack of a better place it's here.  We return
  // true if all is kosher and false if any invalid options are found. Generally
  // the application should exit if false is returned.
  //
  // Currently, the command line options recognized are -
  //
  //  -d          - set the console message level to DEBUG
  //  -l          - open a log file using the default name
  //  -x          - run as an independent process
  //  <file name> - use <file name> as a startup script
  //
  //   Note that the CLog and CConsoleWindow objects must be created before
  // we get here!
  //--
  if (!g_oShellCommand.Parse(pszProgram, argc, argv)) {
    // Here for any kind of syntax error ...
    fprintf(stderr, "\nusage:\t%s [-x] [-d] [-l] [<script name>]\n\n", pszProgram);
    fprintf(stderr, "\t-x\t\t- fork an independent instance of this application\n");
    fprintf(stderr, "\t-d\t\t- set the console message level to DEBUG\n");
    fprintf(stderr, "\t-l\t\t- open a log file using the default name\n");
    fprintf(stderr, "\t<script name>\t- use a startup script\n");
    return false;
  }

  //  If we want to fork, then do that before anything else!
  if (g_oShellCommand.IsOptionPresent('x')) {
    //   Kludge here - we have to remove the -x option from the command line
    // we pass to the new process, because otherwise we'll loop forever!
    g_oShellCommand.RemoveOption('x');
    if (!DetachProcess(g_oShellCommand.BuildCommand()))
      fprintf(stderr, "%s: failed to create process\n", pszProgram);
    return false;
  }

  // Otherwise apply the options and we're done ...
  if (g_oShellCommand.IsOptionPresent('d')) CLog::GetLog()->SetDefaultConsoleLevel(CLog::DEBUG);
  if (g_oShellCommand.IsOptionPresent('l')) CLog::GetLog()->OpenLog();
  g_sStartupScript = FullPath(g_oShellCommand.GetArgument(0).c_str());
  return true;
}

#ifdef UNUSED
// The original version, prior to CCommandLine ...
bool CStandardUI::ParseOptions (const char *pszProgram, int argc, char *argv[])
{
  //++
  //   This method will parse the arguments from the original shell command
  // that invoked this program.  Fortunately we only accept a few options and
  // they are pretty much standard between all implementations, so it works to
  // have this be a shared function.  Needless to say, this is in no way a
  // part of the CCmdParser UI, but it is another UI function and for lack of
  // a bettery place it's here.
  //
  //   We return true if all is kosher and false if any invalid options are
  // found.  Generally the application should exit if false is returned.
  // Currently, the command line options recognized are -
  //
  //  -d          - set the console message level to DEBUG
  //  -l          - open a log file using the default name
  //  -x          - run as an independent process
  //  <file name> - use <file name> as a startup script
  //
  //   Note that the CLog and CConsoleWindow objects must be created before
  // we get here!
  //--
  bool fDebug = false, fLog = false, fFork = false;

  // This is simple, but it works...
  //   BTW, note that this implementation is a) independent of the order of
  // the options, and b) doesn't care if a particular option is repeated.
  // Both properties are useful in some instances!
  for (int i = 1; i < argc; ++i) {
         if (strcmp(argv[i], "-d") == 0) fDebug = true;
    else if (strcmp(argv[i], "-l") == 0) fLog   = true;
    else if (strcmp(argv[i], "-x") == 0) fFork  = true;
    else if (argv[i][0] == '-')          goto usage;
    else {
      // ... must be a file name ...
      if (g_pszStartupScript != NULL) goto usage;
      g_pszStartupScript = argv[i];
    }
  }

  //   If we want to fork, then reconstruct a canonical command line (less
  // the -x option, of course!) and go fork a copy of ourselves.  In that
  // case this procedure returns false and this instance exits immediately.
  if (fFork) {
    string sCommand;
    if (fDebug) sCommand += " -d";
    if (fLog)   sCommand += " -l";
    if (g_pszStartupScript != NULL) sCommand += " " + string(g_pszStartupScript);
    if (!DetachProcess(sCommand))
      fprintf(stderr, "%s: failed to create process\n", pszProgram);
    return false;
  }

  // Otherwise apply the options and return true ...
  if (fDebug) CLog::GetLog()->SetConsoleLevel(CLog::DEBUG);
  if (fLog) CLog::GetLog()->OpenLog();
  return true;

usage:
  // Here for any kind of syntax error ...
  fprintf(stderr, "\nusage:\t%s [-x] [-d] [-l] [<script name>]\n\n", pszProgram);
  fprintf(stderr, "\t-x\t\t- fork an independent instance of this application\n");
  fprintf(stderr, "\t-d\t\t- set the console message level to DEBUG\n");
  fprintf(stderr, "\t-l\t\t- open a log file using the default name\n");
  fprintf(stderr, "\t<script name>\t- use a startup script\n");
  return false;
}
#endif


string CStandardUI::Abbreviate (string str, uint32_t nMax)
{
  //++
  //   This routine will "abbreviate" a string so that it is at most nMax
  // characters long.  If the original string length is less than or equal to
  // nMax then it is returned unchanged, but otherwise characters are chopped
  // out of the middle and replaced with "..." to make the result exactly
  // nMax characters long.
  //
  ///   Note that nMax must be at least 5 - enough for one character at each
  // end of the result plus three for the "...".
  //--
  assert(nMax > 4);
  size_t nLength = str.length();
  if (nLength <= nMax) return str;
  size_t nChop = (nLength - nMax) + 3;
  str.replace((nMax/2-2), nChop, "...");
  return str;
}


bool CStandardUI::DoSetWindow (CCmdParser &cmd)
{
  //++
  //   The "SET WINDOW" command sets several characteristics relating to the
  // emulator console window, including title string, foreground and background
  // colors, window size, and window position on the screen.
  //
  // Format:
  //    SET WINDOW /TITLE=string /FOREGROUND=color /BACKGROUND=color
  //               /COLUMS=n /ROWS=n /X=n /Y=n
  //--
  CConsoleWindow *pConsole = CConsoleWindow::GetConsole();

  // Set the title if necessary ...
  if (m_argTitle.IsPresent())
    pConsole->SetTitle(m_argTitle.GetValue().c_str());

  //   Set the window colors if specified.  Note that if only one of the
  // foreground and background options are specified, the other one remains
  // unchanged.  Also note that it's an error to make both colors the same!
  if (m_argForeground.IsPresent() || m_argBackground.IsPresent()) {
    uint8_t bForeground, bBackground;
    pConsole->GetColors(bForeground, bBackground);
    if (m_argForeground.IsPresent()) bForeground = MASK8(m_argForeground.GetKeyValue());
    if (m_argBackground.IsPresent()) bBackground = MASK8(m_argBackground.GetKeyValue());
    if (bBackground == bForeground)
      CMDERRF("the foreground and background colors cannot be the same");
    else
      pConsole->SetColors(bForeground, bBackground);
  }

  //   And lastly handle the window size and position.  Both characteristics
  // are set by one method in CConsoleWindow, so we must handle them together.
  // Note that on Linux there is no option to set the window position - only
  // the size, in rows and columns, can be changed.
  if (m_argRows.IsPresent() || m_argColumns.IsPresent()
#if defined(_WIN32)
    || m_argX.IsPresent() || m_argY.IsPresent()
#endif
    ) {
    int32_t nX=0, nY=0;  uint16_t nRows=0, nColumns=0;
#if defined(_WIN32)
    pConsole->GetWindowPosition(nX, nY);
    if (m_argX.IsPresent()) nX = m_argX.GetNumber();
    if (m_argY.IsPresent()) nY = m_argY.GetNumber();
#endif
    pConsole->GetWindowSize(nColumns, nRows);
    if (m_argColumns.IsPresent()) nColumns = m_argColumns.GetNumber();
    if (m_argRows.IsPresent()   ) nRows    = m_argRows.GetNumber();
    pConsole->SetWindowSize(nColumns, nRows, nX, nY);
  }

  // All done!
  return true;
}


void CStandardUI::DoHelpColors()
{
  //++
  // Print a table of all known color names for SET WINDOW ...
  //--
  CMDOUTF("\n  Color");
  CMDOUTF("  --------");
  for (uint32_t i = 1;  m_keysColor[i].m_pszName != NULL;  ++i) {
    CMDOUTF("  %s", m_keysColor[i].m_pszName);
  }
  CMDOUTS("");
}


bool CStandardUI::DoSetLog (CCmdParser &cmd)
{
  //++
  //   The "SET LOGGING" command allows the console message level to be
  // changed, a log file to be opened or closed, and the log file message
  // level to be set.
  //
  // Format:
  //    SET LOGGING /NOFILE /FILE[=xyz] /CONSOLE /LEVEL=xyz
  //
  //   There are several modifiers for this command and, just between us, the
  // semantics are a bit screwy.  Of all the possible combinations, the ones
  // that are useful are -
  //
  //  SET LOG/CONSOLE/LEVEL=lvl - set the console message level.  The current
  //        log file, if any, is not affected.
  //
  //  SET LOG/FILE=file/LEVEL=lvl - open the new log file specified by "file"
  //        (if a log file is already opened, it is closed) and set the log
  //        message level.  The /OVERWRITE or /APPEND modifiers may also be
  //        used with this command.
  //
  //  SET LOG/FILE=file - same as above, but the message level is unchanged.
  //
  //  SET LOG/FILE/LEVEL=lvl - if a log file is already opened, then change
  //        its message level but keep using the current log file.  If no log
  //        file is opened, generate a unique file name using the current date
  //        and time and start logging to that.  The /OVERWRITE and /APPEND
  //        modifiers have no effect here, since the new log file is guaranteed
  //        a unique file name.
  //
  //  SET LOG/FILE - always start a new log file, using a unique name.  The
  //        current message level is unchanged.
  //
  //  SET LOG/NOFILE - close the current log file, if any.
  //--
  CLog::SEVERITY nLevel = static_cast<CLog::SEVERITY> (m_argVerbosity.GetKeyValue());
  CLog *pLog = CLog::GetLog();

  //   If /CONSOLE and /LEVEL are both specified, then set the console message
  // level.  Note that this can be combined with the /FILE or /NOFILE option
  // (although in that case the console and file will both be set to the same
  // message level).  This also means that commands like "SET LOG/CONSOLE"
  // (with no /LEVEL) are silently ignored.
  if (m_modConsole.IsPresent() && m_modVerbosity.IsPresent()) {
    pLog->SetDefaultConsoleLevel(nLevel);
    LOGS(DEBUG, "console message level set to " << CLog::LevelToString(pLog->GetDefaultConsoleLevel()));
  }

  //   /NOFILE closes the current log file, and /FILE opens a new one.  /FILE
  // takes an optional file name argument - if omitted, the CLog class will
  // make up a temporary name for us.  Note that the sense of /FILE and /NOFILE
  // are intentionally switched in the m_modNoFile definition, so /FILE is
  // the "negated" option!
  if (m_modNoFile.IsPresent()) {
    if (m_modNoFile.IsNegated()) {
      //   Special case here - if /FILE is specified without any argument AND
      // a log file is already opened, then just leave it alone.  This allows
      // the log file level to be changed w/o opening a new file!
      if (m_argOptFileName.IsPresent() || !pLog->IsLogFileOpen()) {
        bool fOverwrite = m_modAppend.IsPresent() && m_modAppend.IsNegated();
        pLog->OpenLog(m_argOptFileName.GetFullPath(), CLog::DEBUG, !fOverwrite);
      }
    } else
      pLog->CloseLog();
    if (m_modVerbosity.IsPresent()) {
      pLog->SetDefaultFileLevel(nLevel);
      LOGS(DEBUG, "log file message level set to " << CLog::LevelToString(pLog->GetDefaultFileLevel()));
    }
  }
  return true;
}

#ifdef THREADS
bool CStandardUI::DoSetCheckpoint (CCmdParser &cmd)
{
  //++
  //   The "SET CHECKPOINT" command allows the flie checkpoint thread to be
  // started, stopped, or the checkpoint interval changed ...
  //
  // Format:
  //    SET CHECKPOINT /ENABLE /INTERVAL=nnn
  //    SET CHECKPOINT /DISABLE
  //--
  if (!CCheckpointFiles::IsEnabled()) {
    CMDERRS("file checkpointing not enabled");  return false;
  }
  CCheckpointFiles *pFlusher = CCheckpointFiles::GetCheckpoint();
  if (m_modEnable.IsPresent() && m_modEnable.IsNegated()) {
    if (m_modInterval.IsPresent()) {
      CMDERRS("/INTERVAL ignored with /DISABLE");
    }
    pFlusher->Stop();
  } else {
    if (m_modInterval.IsPresent())
      pFlusher->SetInterval(m_argInterval.GetNumber());
    pFlusher->Start();
  }
  return true;
}
#endif


bool CStandardUI::DoShowLog (CCmdParser &cmd)
{
  //++
  // Show the console and log file message levels ...
  //--
  const CLog *pLog = CLog::GetLog();
//CMDOUTS("");
  CMDOUTS("Default console message level set to " << CLog::LevelToString(pLog->GetDefaultConsoleLevel()));
  if (pLog->IsLogFileOpen()) {
    CMDOUTS("Default log file message level set to " << CLog::LevelToString(pLog->GetDefaultFileLevel()));
    CMDOUTS("Logging to file " << pLog->GetLogFileName());
  } else {
    CMDOUTS("No log file opened");
  }
  CMDOUTS("");
  return true;
}


bool CStandardUI::DoShowOneAlias (CCmdParser &cmd, string sAlias)
{
  //++
  // Show the definition for just one, specific, alias ...
  //--
  const CCmdAliases *pAliases = cmd.GetAliases();
  if (!pAliases->IsDefined(sAlias)) {
    CMDERRS("alias " << sAlias << " is not defined");  return false;
  }
  CMDOUTS(sAlias << " is defined as \"" << pAliases->GetDefinition(sAlias) << "\"");
  return true;
}


bool CStandardUI::DoShowAllAliases (CCmdParser &cmd)
{
  //++
  //--
  const CCmdAliases *pAliases = cmd.GetAliases();
  if (pAliases->Count() == 0) {
    CMDOUTS("No command aliases defined\n");  return true;
  }

  CMDOUTF("\nAlias            Definition");
  CMDOUTF("---------------  --------------------------------------------");
  for (CCmdAliases::const_iterator it = pAliases->begin();  it != pAliases->end();  ++it) {
    CMDOUTF("%-15.15s  \"%s\"", (*it).first.c_str(), Abbreviate((*it).second, 64).c_str());
  }
  CMDOUTF("\n%d command aliases defined\n", pAliases->Count());
  return true;
}


bool CStandardUI::DoShowAliases (CCmdParser &cmd)
{
  //++
  // Show the console and log file message levels ...
  //--
  if (m_argOptAlias.IsPresent())
    return DoShowOneAlias(cmd, m_argOptAlias.GetValue());
  else
    return DoShowAllAliases(cmd);
}

#ifdef THREADS
bool CStandardUI::DoShowCheckpoint (CCmdParser &cmd)
{
  //++
  // Show the current checkpoint thread settings ...
  //--
  if (!CCheckpointFiles::IsEnabled()) {
    CMDERRS("file checkpointing not enabled");  return false;
  }
  const CCheckpointFiles *pFlusher = CCheckpointFiles::GetCheckpoint();
//CMDOUTS("");
  if (pFlusher->IsRunning())
    CMDOUTF("File checkpoint thread running at %d second intervals", pFlusher->GetInterval());
  else
    CMDOUTF("File checkpoint thread not enabled");
  CMDOUTS("");
  return true;
}
#endif


bool CStandardUI::DoDefine (CCmdParser &cmd)
{
  //++
  //   The DEFINE command add an entry to the table of command aliases, for
  // example
  //
  //    DEFINE R0 "REWIND TAPE/UNIT=0"
  //
  // would define the command "R0" as an alias for the longer string "REWIND
  // TAPE/UNIT=0".  Notice that the substitution string must be enclosed in
  // quotes if it contains any spaces or "/" characters (and which it almost
  // always will!).
  //
  //    Note that it's not possible to redefine any built in command, including
  // anything that would conflict with an alias for a built in command.  Thus,
  // if "CRE*ATE" is a built in command, then "DEFINE CRE ...", "DEFINE CREA
  // ...", etc would all be invalid.  However, "DEFINE CR ..." would be allowed,
  // since "CR" is not a valid abbreviation for "CREATE".  This allows you to do
  // tricks like "DEFINE EX EXIT" to shorten the length of an already existing
  // abbreviation, if you really want to.
  //
  //   Lastly, note that aliases cannot themselves be abbreviated, so you cannot
  // define an abbreviation with a "*" in the name.
  //--
  return cmd.DefineAlias(m_argAlias.GetValue(), m_argSubstitution.GetValue());
}


bool CStandardUI::DoUndefine (CCmdParser &cmd)
{
  //++
  // Delete an alias (assuming it has previously been defined) ...
  //--
  return cmd.UndefineAlias(m_argAlias.GetValue());
}


bool CStandardUI::DoIndirect (CCmdParser &cmd)
{
  //++
  //   The DO command executes an indirect command file. The command file is
  // simply a text file containing a list of commands, and these are executed
  // as if they had been typed in by the operator. Presently indirect command
  // files do not take parameters and may not be nested.  An error in any
  // command will abort the indirect command file at that point.
  //
  // Format:
  //    DO <command-file>
  //
  // This command has no qualifiers.
  //--
  return cmd.OpenScript(m_argFileName.GetFullPath());
}


bool CStandardUI::DoExit (CCmdParser &cmd)
{
  //++
  //   The EXIT command terminates the CDC server and returns to the shell.
  // All attached image files are detached, all online drives are made offline,
  // and finally all drives are marked as disconnected. From the point of view
  // of the host, it appears that all drives on the channel have been suddenly
  // turned off.
  //
  // Format:
  //    EXIT
  //
  // This command has no qualifiers.
  //--

  // EXIT or QUIT in an indirect file just terminates that file ...
  if (cmd.InScript()) {
    cmd.CloseScript();  return true;
  }

  // Otherwise, exit if it's OK ...
  if (cmd.ConfirmExit()) cmd.SetExitRequest();
  return true;
}
