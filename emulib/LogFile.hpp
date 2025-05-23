//++
// LogFile.hpp -> Emulator log file driver class
//
//   COPYRIGHT (C) 2015-2024 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
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
//   The CLog class defines a fairly simple logging object for the emulator
// library.  Messages may be logged to the console, to a file, or both depending
// on the message severity.  Messages logged to the log file are automatically
// time stamped.
//
//    This is a "modified" Singleton class - only one instance per application
// should ever be created, however the constructor must be explicitly called
// once to create that instance.  Subsequent calls to the constructor will
// cause assertion failures, and a pointer to the original instance may be
// retrieved at any time by calling GetLog().
//
// Bob Armstrong <bob@jfcl.com>   [20-MAY-2015]
//
// REVISION HISTORY:
// 20-MAY-15  RLA   New file.
//  1-JUN-17  RLA   Linux port.
// 25-Jan-18  RLA   Add TimeDifferenceToString
// 31-Jan-20  RLA   Add CMDOUT and CMDERR (ostringstream style)
// 26-AUG-22  RLA   Clean up Linux/WIN32 conditionals.
//  6-FEB-24  RLA   Create single threaded version.
//--
#pragma once
#include <sys/timeb.h>          // struct __timeb, ftime(), etc ...
#include <string>               // C++ std::string class, et al ...
#include <iostream>             // C++ style output for LOGS() ...
#include <sstream>              // C++ std::stringstream, et al ...
#ifdef THREADS
#include <unordered_set>        // C++ std::unordered_set (a simple list) template
#include <unordered_map>        // C++ std::unordered_map (aka hash table) template
using std::unordered_map;       // ...
using std::unordered_set;       // ...
#include "Thread.hpp"           // we need this for THREAD_ID, et al ...
class CMessageQueue;            //  ... and this ...
#endif
class CConsoleWindow;           // we need forward pointers for this class
using std::string;              // ...
using std::ostream;             // ...
using std::ostringstream;       // ...
#undef ERROR


// Clog class definition ...
class CLog {
  //++
  //--

  // Logging levels and constants ...
public:
  enum SEVERITY {
    //   These are the severity level of all log messages.  Needless to say,
    // the order of these is important since the test for logging is whether
    // the message's level is .GE. the current logging level.
    //
    //   Note that CMDOUT and CMDERR are special cases - these are used
    // exclusively for output and errors generated in response to something
    // the operator just typed.  This text is handled specially.
    CMDOUT   = -2,        // output generated by operator commands
    CMDERR   = -1,        // errors generated by operator commands
    TRACE    = 0,         // I/O trace messages
    DEBUG    = TRACE+1,   // debugging messages
    WARNING  = DEBUG+1,   // warning messages
    ERROR    = WARNING+1, // severe error messages
    ABORT    = ERROR+1,   // severe error that aborts this program
    NOLOG    = 99999,     // imaginary level that disables all logging
  };
  // Other constants ...
  enum {
    MAXMSG   = 3072       // The longest possible line in the log file
  };

#ifdef THREADS
  //   The THREAD_LEVEL type implements a mapping from a THREAD_ID to a log
  // message level, and that allows us to implement different logging levels
  // for each channel/MASSBUS/whatever thread.
  //
  //   Oh, and yes, we could have implemented something like this using thread
  // local storage.  That's harder in this case, though, because we have to
  // resort to Windows specific TLS calls to determine whether a log level 
  // has been set for the current thread or not...
  typedef unordered_map<THREAD_ID, SEVERITY> THREAD_LEVEL;

  //   And the QUEUE_SET type implements a set of THREAD_IDs that have their
  // messages queued for later, asynchronous, reporting.  If a given thread
  // ID is present in this set, then it's queued.  If the thread ID is not in
  // the set, then messages are reported synchronously (that is, right now!).
  typedef unordered_set<THREAD_ID> QUEUE_SET;
#endif

  // This type is used to record a timestamp everywhere in the log ...
#if defined(_WIN32)
  typedef struct __timeb64 TIMESTAMP;
#elif defined(__linux__)
  typedef struct timeb TIMESTAMP;
#elif defined(__APPLE__)
  typedef struct timeval TIMESTAMP;
#elif defined(__unix__)
  typedef struct timeb TIMESTAMP;
#endif

  // Constructors and destructor ...
public:
  CLog (const char *pszProgram, CConsoleWindow *pConsole=NULL);
  virtual ~CLog();
private:
  // Disallow copy and assignment operations with CLog objects...
  CLog(const CLog &log) = delete;
  CLog& operator= (const CLog &log) = delete;

  // Public CLog properties ...
public:
  // Return a pointer to the one and only CLog object ...
  static CLog *GetLog() {assert(m_pLog != NULL);  return m_pLog;}
  // Return true if a log file is open ...
  bool IsLogFileOpen() const {return m_pLogFile != NULL;}
  // Return the current log file name ...
  string GetLogFileName() const
    {return IsLogFileOpen() ? m_sLogName : string();}
  // Set the default console and file logging levels ...
  void SetDefaultConsoleLevel (SEVERITY nLevel) {m_lvlConsole = nLevel;}
  void SetDefaultFileLevel (SEVERITY nLevel) {m_lvlFile = nLevel;}
  SEVERITY GetDefaultConsoleLevel() const {return m_lvlConsole;}
  SEVERITY GetDefaultFileLevel() const {return m_lvlFile;}
  // Get the log levels for this thread (or the default, if none) ...
  SEVERITY GetConsoleLevel() const;
  SEVERITY GetFileLevel() const;
#ifdef THREADS
  // Set the console and file log levels for a specific thread ...
  void SetThreadConsoleLevel (SEVERITY nLevel, THREAD_ID idThread=0);
  void SetThreadFileLevel(SEVERITY nLevel, THREAD_ID idThread=0);
  SEVERITY GetThreadConsoleLevel(THREAD_ID idThread=0) const;
  SEVERITY GetThreadFileLevel(THREAD_ID idThread=0) const;
  // Remove all log level mappings for the specified thread ...
  void RemoveThreadLevels(THREAD_ID idThread=0);
  // Control whether this thread's messages are queued ...
  void SetThreadQueued(bool fQueued=true, THREAD_ID idThread=0);
  bool IsThreadQueued(THREAD_ID idThread=0) const;
#endif
  // Convert a log level to a string for messages ...
  static string LevelToString(SEVERITY nLevel);

  // Test a message level against the current logging level ...
  static bool IsLogged (SEVERITY msglvl, SEVERITY loglvl)
    {return ((msglvl <= CMDERR) || (msglvl >= loglvl));}
  // Return true if a message of nLevel should be sent to the console ...
  bool IsLoggedToConsole (SEVERITY nLevel) const
    {return IsLogged(nLevel, GetConsoleLevel());}
  // Return true if a message of nLevel should be sent to the log file ...
  bool IsLoggedToFile (SEVERITY nLevel) const
    {return IsLogFileOpen() && IsLogged(nLevel, GetFileLevel());}
  // Return true if a message of nLevel should be logged at all ...
  bool IsLogged (SEVERITY nLevel) const
    {return IsLoggedToConsole(nLevel)  ||  IsLoggedToFile(nLevel);}
  // Return a time stamp for the current time ...
  static string TimeStampToString (const TIMESTAMP *ptb);
  static string TimeDifferenceToString (const TIMESTAMP *ptb1, const TIMESTAMP *ptb2);
  static void GetTimeStamp (TIMESTAMP *ptb);
  static string GetTimeStamp();
  // Return a "made up" name for the log file...
  string GetDefaultLogFileName();

  // Public CLog methods ...
public:
  // Open and close the log file ...
  bool OpenLog (const string &sFileName = string(), SEVERITY nLevel=DEBUG, bool fAppend=true);
  void CloseLog();
  // Do all the work of logging a message ...
  void Print (SEVERITY nLevel, ostringstream &osText);
  void Print (SEVERITY nLevel, const char *pszFormat, ...);
  // Send output directly to the console or log file ...
  void SendLog (SEVERITY nLevel, const char *pszText, const TIMESTAMP *ptb=NULL);
  void SendLog (SEVERITY nLevel, const string &sText, const TIMESTAMP *ptb=NULL)
    {SendLog(nLevel, sText.c_str(), ptb);}
  void SendConsole (SEVERITY nLevel, const char *pszText);
  void SendConsole (SEVERITY nLevel, const string &sText)
    {SendConsole(nLevel, sText.c_str());}
  // Log input from the operator or from a script ...
  void LogOperator (const string &strPrompt, const char *pszCommand);
  void LogScript (const string &sScript, const char *pszCommand);
#ifdef THREADS
  // Start and stop the message logging thread ...
  bool IsLoggingThreadRunning() const;
  bool StartLoggingThread();
  void StopLoggingThread();
#endif

  // Private CLog methods ...
private:
  void LogSingleLine (const TIMESTAMP *ptb, const string &sPrefix, const char *pszText);
  void LogSingleLine (const TIMESTAMP *ptb, SEVERITY nLevel, const char *pszText)
    {LogSingleLine(ptb, LevelToString(nLevel), pszText);}

  // Local members ...
private:
  const string    m_sProgram;     // this program's name (for messages)
  SEVERITY        m_lvlConsole;   // default console message level
  SEVERITY        m_lvlFile;      // default log file message level 
  string          m_sLogName;     // name of the current log file
  FILE           *m_pLogFile;     // handle of the log file
  CConsoleWindow *m_pConsole;     // pointer to console window object
#ifdef THREADS
  CMessageQueue  *m_pQueue;       // pointer to message queue object
  QUEUE_SET       m_setQueued;    // set of threads which are queued
  THREAD_LEVEL m_mapConsoleLevel; // per-thread console logging levels
  THREAD_LEVEL m_mapFileLevel;    // per-thread file logging levels
#endif

  // Static data ...
private:
  static CLog    *m_pLog;         // a pointer to the one and only CLog instance
};


//   This macro determines whether a message with a given message level will
// appear in any log (console or file).  It's used by the LOGx macros, and it
// can be used directly in the code (e.g. in DumpData()) as an efficiency
// optimization...
#define ISLOGGED(lvl) (CLog::GetLog()->IsLogged(CLog::lvl))

//   These macros send output to the log and ultimately should be used for
// ALL output.  That means printf()/fprintf() and/or cout/cerr should never
// be used outside of the LogFile module.  Note that there are two versions
// of these macros - LOGS, which does C++ stream style output, and LOGF, which
// takes the old fashioned printf() style argument list.  Either can be used
// and both do the same thing in the end.
#define LOGS(lvl,args)       \
  {if (ISLOGGED(lvl)) {ostringstream os;  os << args;  CLog::GetLog()->Print(CLog::lvl, os);}}
//   Note that the "##" in front of __VA_ARGS__ is a gcc hack to remove the
// trailing comma when the variable argument list is empty. Amazingly, Visual
// C++ seems to understand it too (or at least it doesn't complain!!).
#define LOGF(lvl, fmt, ...)  \
  {if (ISLOGGED(lvl)) CLog::GetLog()->Print(CLog::lvl, fmt, ##__VA_ARGS__);}

//   Note that CMDOUTx() and CMDERRx() are special cases - there's no need (or
// desire) to check the log level in those cases ...
#define CMDOUTS(args)     {ostringstream os;  os << args;  CLog::GetLog()->Print(CLog::CMDOUT, os);}
#define CMDOUTF(fmt, ...) CLog::GetLog()->Print(CLog::CMDOUT, fmt, ##__VA_ARGS__)
#define CMDOUT(os)        CLog::GetLog()->Print(CLog::CMDOUT, os)
#define CMDERRS(args)     {ostringstream os;  os << args;  CLog::GetLog()->Print(CLog::CMDERR, os);}
#define CMDERRF(fmt, ...) CLog::GetLog()->Print(CLog::CMDERR, fmt, ##__VA_ARGS__)
#define CMDERR(os)        CLog::GetLog()->Print(CLog::CMDERR, os)

//   stdint.h defines the uint8_t type as an unsigned char, so if you try to
// write a uint8_t variable to cout, C++ will decide that it's a character
// and won't use the decimal inserter!  Bummer...  We'll fix that by teaching
// C++ how to output a uint8_t type.
inline ostream& operator << (ostream &os, uint8_t n)
  {os << static_cast<int>(n);  return os;}
