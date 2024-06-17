//++
// LogFile.cpp -> CLog (emulator library log file) methods
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
//   The CLog class defines a generic logging facility for the emulator library.
// Messages may be logged to the console, to a file, or both depending on the
// message severity.  Messages logged to the log file are automatically time
// stamped.  Log files may be opened and closed, and the message level for
// both console and log file may be changed dynamically.
//
//    It's important to remember that the emulator programs are multi-threaded.
// Logging "control" methods like OpenLog() or CloseLog(), SetFileLevel(), etc,
// can only ever be called from the background UI thread and generally don't
// need to worry about this.  HOWEVER, the Print(), SendLog() and SendConsole()
// methods can be called by any thread, and that includes the possibility that
// they may be called by more than one thread at the same time.  Care must be
// taken to make sure they're re-entrant and thread safe.
//
//   Lastly, note that it is intended that there be only one CLog instance per
// application, and it follows a somewhat modified Singleton design pattern.
// It's modified because the constructor has parameters and we want it to be
// explicitly called, but only once.  Subsequent calls to the constructor will
// generate assertion failures, and a pointer to the original CLog instance
// can be retrieved at any time by calling CLog::GetLog().
//
// Bob Armstrong <bob@jfcl.com>   [20-MAY-2015]
//
// REVISION HISTORY:
// 20-May-15  RLA   New file.
// 11-JUN-15  RLA   Add CConsoleWindow support.
// 28-OCT-15  RLA   Open log file in shared read mode.
//  2-JUN-17  RLA   Linux port.
// 26-AUG-22  RLA   Clean up Linux/WIN32 conditionals.
//  6-FEB-24  RLA   Create single threaded version.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <stdarg.h>             // va_start(), va_end(), et al ...
#include <assert.h>             // assert() (what else??)
#include <string.h>             // strcpy(), strerror(), etc ...
#include <cstring>              // needed for memset()
#include <time.h>               // _localtime64_s(), localtime_r(), ...
#include <sys/time.h>           // AW Defines gettimeofday()
#include <sys/timeb.h>          // struct __timeb, ftime(), etc ...
#include "EMULIB.hpp"           // emulator library definitions
#include "SafeCRT.h"		// replacements for Microsoft "safe" CRT functions
#ifdef THREADS
#include "Thread.hpp"           // CThread portable thread library
#include "CheckpointFiles.hpp"  // emulator library file checkpoint thread
#include "MessageQueue.hpp"     // log file queueing functions
#endif
#include "ConsoleWindow.hpp"    // console window methods
#include "CommandParser.hpp"    // emulator library command line parsing methods
#include "LogFile.hpp"          // declarations for this module


// Initialize the pointer to the one and only CLog instance ...
CLog *CLog::m_pLog = NULL;


CLog::CLog (const char *pszProgram, CConsoleWindow *pConsole)
  : m_sProgram(pszProgram), m_pConsole(pConsole)
#ifdef THREADS
  , m_setQueued(), m_mapConsoleLevel(), m_mapFileLevel()
#endif
{
  //++
  //   The log file constructor just initializes all the members.  The
  // initial console logging level is set to WARNING and the log file is
  // initially closed.
  //
  //   Note that the pszProgram parameter is the name of the application that
  // uses EMULIB - ELF, PDP, etc. It's only used as a prefix on error messages.
  //--

  // This had better be the first and only instance of this object!
  assert(m_pLog == NULL);
  m_pLog = this;

  // Initialize all the members ...
  m_pLogFile = NULL;  m_sLogName.clear();  m_lvlFile = NOLOG;
#ifdef THREADS
  m_mapConsoleLevel.clear();  m_mapFileLevel.clear();  m_setQueued.clear();
  m_pQueue = DBGNEW CMessageQueue();
#endif
#if defined(_DEBUG)
  m_lvlConsole = DEBUG;
#else
  m_lvlConsole = WARNING;
#endif
}

CLog::~CLog()
{
  //++
  // Destroying the log closes the log file ...
  //--

#ifdef THREADS
  // Stop the message logging thread (if any) ...
  StopLoggingThread();  delete m_pQueue;  m_pQueue = NULL;
#endif

  // Close any log file ...
  if (IsLogFileOpen()) CloseLog();

  //   Reset the pointer to this Singleton object. In theory this would allow
  // another CLog instance to be created, but that's not likely to be useful.
  assert(m_pLog == this);
  m_pLog = NULL;
}

/*static*/ string CLog::LevelToString (SEVERITY nLevel)
{
  //++
  //   Return a simple string corresponding to nLevel.   This is used to
  // put the message level into the log file...
  //--
  switch (nLevel) {
    case CMDOUT:  return string("CMDOUT");
    case CMDERR:  return string("CMDERR");
    case TRACE:   return string("TRACE");
    case DEBUG:   return string("DEBUG");
    case WARNING: return string("WARN");
    case ERROR:   return string("ERROR");
    case ABORT:   return string("ABORT");
    default:      return string("UNKNOWN");
  }
}

#ifdef THREADS
void CLog::SetThreadQueued (bool fQueued, THREAD_ID idThread)
{
  //++
  //   If fQueued is TRUE, then add the thread ID to the set of queued threads.
  // If fQueued is FALSE, then remove the thread ID.  Either way, no errors
  // are ever returned...
  //--
  if (idThread == 0) idThread = CThread::GetCurrentThreadID();
  if (fQueued) {
    m_setQueued.insert(idThread);
  } else {
    QUEUE_SET::iterator it = m_setQueued.find(idThread);
    if (it != m_setQueued.end()) m_setQueued.erase(it);
  }
}

bool CLog::IsThreadQueued (THREAD_ID idThread) const
{
  //++
  // Return TRUE if the thread is in the set of queued threads ...
  //--
  if (idThread == 0) idThread = CThread::GetCurrentThreadID();
  return m_setQueued.count(idThread) > 0;
}

void CLog::SetThreadConsoleLevel(SEVERITY nLevel, THREAD_ID idThread)
{
  //++
  //   Set the console log levels for a specific thread (or the current
  // thread, if idThread is zero).  This adds a mapping between the thread
  // id and the specified level to the m_mapConsoleLevel unordered_map
  // (assuming one doesn't already exist, of course - if it does already
  // exist then we just change the existing level).
  //--
  if (idThread == 0) idThread = CThread::GetCurrentThreadID();
  m_mapConsoleLevel[idThread] = nLevel;
}

void CLog::SetThreadFileLevel(SEVERITY nLevel, THREAD_ID idThread)
{
  //++
  // Ditto, but for the file log level instead ...
  //--
  if (idThread == 0) idThread = CThread::GetCurrentThreadID();
  m_mapFileLevel[idThread] = nLevel;
}

CLog::SEVERITY CLog::GetThreadConsoleLevel (THREAD_ID idThread) const
{
  //++
  //   This method will retiurn the console logging level established for the
  // specified thread (or the current thread, if idThread is zero).  If no
  // thread specific mapping has been established, then NOLOG is returned ...
  //--
  if (idThread == 0) idThread = CThread::GetCurrentThreadID();
  THREAD_LEVEL::const_iterator it = m_mapConsoleLevel.find(idThread);
  return (it != m_mapConsoleLevel.end()) ? it->second : NOLOG;
}

CLog::SEVERITY CLog::GetThreadFileLevel (THREAD_ID idThread) const
{
  //++
  // Ditto, but for the file log level instead ...
  //--
  if (idThread == 0) idThread = CThread::GetCurrentThreadID();
  THREAD_LEVEL::const_iterator it = m_mapFileLevel.find(idThread);
  return (it != m_mapFileLevel.end()) ? it->second : NOLOG;
}

void CLog::RemoveThreadLevels (THREAD_ID idThread)
{
  //++
  //   Remove any thread specific logging levels for the specified thread (or
  // the current thread, if idThread is zero).  Note that this removes both
  // console and file thread specific logging levels.
  //--
  if (idThread == 0) idThread = CThread::GetCurrentThreadID();
  THREAD_LEVEL::iterator it = m_mapConsoleLevel.find(idThread);
  if (it != m_mapConsoleLevel.end()) m_mapConsoleLevel.erase(it);
  it = m_mapFileLevel.find(idThread);
  if (it != m_mapFileLevel.end()) m_mapFileLevel.erase(it);
}
#endif

CLog::SEVERITY CLog::GetConsoleLevel() const
{
  //++
  //   This returns the current console message level.  This is either the 
  // thread specific level for this thread, if one is defined, or the default
  // console level if no thread specific one exists.
  //--
#ifdef THREADS
  SEVERITY lvl = GetThreadConsoleLevel();
  return (lvl != NOLOG) ? lvl : GetDefaultConsoleLevel();
#else
  return GetDefaultConsoleLevel();
#endif
}

CLog::SEVERITY CLog::GetFileLevel() const
{
  //++
  // Ditto, but for the current file message level ...
  //--
#ifdef THREADS
  SEVERITY lvl = GetThreadFileLevel();
  return (lvl != NOLOG) ? lvl : GetDefaultFileLevel();
#else
  return GetDefaultFileLevel();
#endif
}

/*static*/ void CLog::GetTimeStamp (TIMESTAMP *ptb)
{
  //++
  //   Return the time stamp (an _timeb structure) for right now!  Yes, this
  // is a trivial function but it's here to help hide the actual implementation
  // of the TIMESTAMP type.
  //--
#if defined(_WIN32)
  _ftime64_s(ptb);
#elif defined(__linux__) 
  ftime(ptb);
#elif defined(__APPLE__) || defined(__unix__)
  gettimeofday(ptb, NULL);
#endif
}

/*static*/ string CLog::TimeStampToString (const TIMESTAMP *ptb)
{
  //++
  //   This method will convert the specified timestamp into the local time of
  // day as a string in the format "HH:MM:SS.ddd". Notice that the milliseconds
  // are included because many messages get logged in short intervals, however
  // the date is not.  That'll probably change at some point.  
  //--
#if defined(_WIN32)
  struct tm tmNow;  
  char szNow[16];
  _localtime64_s(&tmNow, &(ptb->tv_sec));
  sprintf_s(szNow, sizeof(szNow), "%02d:%02d:%02d.%03ld",
    tmNow.tm_hour, tmNow.tm_min, tmNow.tm_sec, (long)(ptb->tv_usec / 1000));
#elif defined(__linux__) || defined(__APPLE__) || defined(__unix__)
  struct tm tmNow;  
  char szNow[16];
  localtime_r(&(ptb->tv_sec), &tmNow);
  snprintf(szNow, sizeof(szNow), "%02d:%02d:%02d.%03ld",
    tmNow.tm_hour, tmNow.tm_min, tmNow.tm_sec, (long)(ptb->tv_usec / 1000));
#endif
  return string(szNow);
}

/*static*/ string CLog::TimeDifferenceToString (const TIMESTAMP *ptb1, const TIMESTAMP *ptb2)
{
  //++
  //   This method will compute the time interval between ptb2 and ptb1 and
  // convert the result to a nicely formatted ASCII string something like
  // "123d 15:23:47".  Note that ptb2 MUST BE LATER THAN ptb1 or this will
  // fail (we don't handle negative times!).  Also note that the result is
  // only printed with one second resolution, even though the TIMESTAMP
  // structures actually have millisecond data available.
  //--
  assert((ptb1 != NULL) && (ptb2 != NULL));

  // NYI TBA TODO - I think this might work on Linux too, but it hasn't been tested!
#if defined(_WIN32)
  int32_t nInterval = (int32_t) _difftime64(ptb2->time, ptb1->time);
#elif defined(__linux__) || defined(__APPLE__) || defined(__unix__)
    int32_t nInterval = (int32_t) difftime(ptb2->tv_sec, ptb1->tv_sec);
#endif
  if (nInterval < 0) nInterval = -nInterval;

  // Decompose the difference into seconds, minutes, hours and days ...
  uint32_t lSeconds = nInterval % 60;  nInterval /= 60;
  uint32_t lMinutes = nInterval % 60;  nInterval /= 60;
  uint32_t lHours   = nInterval % 24;  nInterval /= 24;
  uint32_t lDays    = nInterval;

  // Print it and we're done ...
  char szInterval[64];
  sprintf_s(szInterval, sizeof(szInterval), "%3dd %02d:%02d:%02d", lDays, lHours, lMinutes, lSeconds);
  return string(szInterval);
}

/*static*/ string CLog::GetTimeStamp()
{
  //++
  // Return a time stamp string for right now ...
  //--
  TIMESTAMP tbNow; 
  GetTimeStamp(&tbNow);
  return TimeStampToString(&tbNow);
}

string CLog::GetDefaultLogFileName()
{
  //++
  //   This method returns a default name for the log file, something like
  // "yyyymmdd.log".  It's used when the operator doesn't specify an explicit
  // log file name...
  //--
#if defined(_WIN32)
  __time64_t qNow;  struct tm tmNow;  char szFN[64];
  _time64(&qNow);  _localtime64_s(&tmNow, &qNow);
#elif defined(__linux__) || defined(__APPLE__) || defined(__unix__)
  time_t tNow;  struct tm tmNow;  char szFN[64];
  time(&tNow);  localtime_r(&tNow, &tmNow);
#endif
  sprintf_s(szFN, sizeof(szFN), "%s_%04d%02d%02d.log",
    m_sProgram.c_str(), tmNow.tm_year+1900, tmNow.tm_mon+1, tmNow.tm_mday);
  return string(szFN);
}


#ifdef THREADS
bool CLog::IsLoggingThreadRunning() const
{
  //++
  //   Return TRUE if the logging thread is currently running.  The only reason
  // this isn't an inline function is because of the circular dependencies
  // between MessageQueue.hpp and LogFile.hpp ...
  //--
  return (m_pQueue != NULL)  &&  m_pQueue->IsLoggingThreadRunning();
}

bool CLog::StartLoggingThread()
{
  //++
  // Start the message logging thread ...
  //--
  assert(m_pQueue != NULL);
  return m_pQueue->BeginLoggingThread(); 
}

void CLog::StopLoggingThread()
{
  //++
  // Stop the message logging thread ...
  //--
  if (m_pQueue != NULL) m_pQueue->EndLoggingThread(); 
}
#endif

bool CLog::OpenLog (const string &sFileName, SEVERITY nLevel, bool fAppend)
{
  //++
  //   This method opens a new log file and sets the default message level for
  // it. If the file name passed is null, then a default file name will be used
  // instead.  Normally new text is appended to any existsing file, however
  // if fAppend is false then any existing log will be overwritten.  In either
  // case a new, empty, file will be created if one does not exist.
  //--
  if (IsLogFileOpen()) CloseLog();
  m_sLogName = sFileName.empty() ? GetDefaultLogFileName() : sFileName;
  m_sLogName = CCmdParser::SetDefaultExtension(m_sLogName, ".log");
  const char *pszMode = fAppend ? "a+t" : "w+t";
//m_pLogFile = _fsopen(m_sLogName.c_str(), pszMode, _SH_DENYWR);
  int err = fopen_s(&m_pLogFile, m_sLogName.c_str(), pszMode);
  if (err != 0) {
    CMDERRS("error (" << errno << ") opening log " << m_sLogName);
    m_sLogName.clear();  return false;
  }
  SetDefaultFileLevel(nLevel);
  LOGS(DEBUG, "log " << m_sLogName << " opened");
#ifdef THREADS
  if (CCheckpointFiles::IsEnabled())
    CCheckpointFiles::GetCheckpoint()->AddFile(m_pLogFile);
#endif
  return true;
}

void CLog::CloseLog()
{
  //++
  //   Close the currently open log file (if any).  Console logging is not
  // affected by this operation.
  //--
  if (!IsLogFileOpen()) return;
  LOGS(DEBUG, "log " << m_sLogName << " closed");
#ifdef THREADS
  if (CCheckpointFiles::IsEnabled())
    CCheckpointFiles::GetCheckpoint()->RemoveFile(m_pLogFile);
#endif
  fclose(m_pLogFile);
  m_pLogFile = NULL;  m_sLogName.clear();  SetDefaultFileLevel(NOLOG);
}

void CLog::Print (SEVERITY nLevel, ostringstream &osText)
{
  //++
  //   This method does the work for the LOGS() macro - it sends output from
  // an ostringstream to the console and/or log file.
  //--
#ifdef THREADS
  if (IsLoggingThreadRunning() && IsThreadQueued()) {
    m_pQueue->AddEntry(nLevel, osText.str().c_str(), IsLoggedToConsole(nLevel), IsLoggedToFile(nLevel));
  } else {
#endif
    if (IsLoggedToFile(nLevel)) SendLog(nLevel, osText.str().c_str());
    if (IsLoggedToConsole(nLevel)) SendConsole(nLevel, osText.str().c_str());
#ifdef THREADS
  }
#endif
}

void CLog::Print (SEVERITY nLevel, const char *pszFormat, ...)
{
  //++
  //   And this method does the work for the LOGF() macro - it sends printf()
  // formatted output to the console and/or log file.  This takes a tiny bit
  // more work than the I/O streams version ...
  //--
  char szBuffer[MAXMSG];  va_list args;
  memset(szBuffer, 0, sizeof(szBuffer));
  va_start(args, pszFormat);
  vsprintf_s (szBuffer, sizeof(szBuffer), pszFormat, args);
  va_end(args);
#ifdef THREADS
  if (IsLoggingThreadRunning() && IsThreadQueued()) {
    m_pQueue->AddEntry(nLevel, szBuffer, IsLoggedToConsole(nLevel), IsLoggedToFile(nLevel));
  } else {
#endif
    if (IsLoggedToFile(nLevel)) SendLog(nLevel, szBuffer);
    if (IsLoggedToConsole(nLevel)) SendConsole(nLevel, szBuffer);
#ifdef THREADS
  }
#endif
}

void CLog::LogSingleLine (const TIMESTAMP *ptb, const string &sPrefix, const char *pszText)
{
  //++
  //   This private method writes a text string, which must be guaranteed to
  // be a single line, to the log file.  A date/time stamp and the message
  // severity is also printed at the start of the line (which is why the
  // message text shouldn't contain newlines!)
  //--
  if (!IsLogFileOpen()) return;
  fprintf(m_pLogFile, "%s %s\t%s\n",
    TimeStampToString(ptb).c_str(),  sPrefix.c_str(), pszText);
}

void CLog::SendLog (SEVERITY nLevel, const char *pszText, const TIMESTAMP *ptb)
{
  //++
  //   This method sends text to the log file, where the text may contain
  // newline characters.  That's messy, because we have to split the text up
  // into individual lines for logging.
  //
  //   Note that the time stamp is optional and, if not specified, defaults to
  // "right now" ...
  //--
  const char *pszEnd;  TIMESTAMP tmNow;
  if (ptb == NULL) {
    GetTimeStamp(&tmNow);  ptb = &tmNow;
  }
  while ((pszEnd = strchr(pszText, '\n')) != NULL) {
    string sLine(pszText, pszEnd-pszText);
    LogSingleLine(ptb, nLevel, sLine.c_str());
    pszText = pszEnd+1;
  }
  LogSingleLine(ptb, nLevel, pszText);
}

void CLog::SendConsole (SEVERITY nLevel, const char *pszText)
{
  //++
  //   This method sends a message to the console.  The exact formatting and
  // the destination (stderr vs stdout) depend on the severity of the
  // message.  CMDOUT, for example, goes to stdout and does not print any
  // trailing newline (the caller is presumed to handle all formatting in
  // that case).
  //--
  char szBuffer[MAXMSG];
  switch (nLevel) {
    case CMDOUT:  sprintf_s(szBuffer, sizeof(szBuffer), "%s\n", pszText);                           break;
    case TRACE:   sprintf_s(szBuffer, sizeof(szBuffer), "-- %s\n", pszText);                        break;
    case DEBUG:   sprintf_s(szBuffer, sizeof(szBuffer), "[%s]\n", pszText);                         break;
    default:      sprintf_s(szBuffer, sizeof(szBuffer), "%s: %s\n", m_sProgram.c_str(), pszText); break;
  }
  if (m_pConsole != NULL)
    m_pConsole->Write(szBuffer);
  else
    fputs(szBuffer, stderr);
}

void CLog::LogOperator (const string &strPrompt, const char *pszCommand)
{
  //++
  //   This method logs input typed by the operator.  Operator input is NEVER
  // logged on the console (he just typed it, after all!) and appears in the
  // log file if the message level is set to WARNINGS or less.
  //--
  TIMESTAMP tmNow;  GetTimeStamp(&tmNow);
  if (GetDefaultFileLevel() <= WARNING) {
    string str(strPrompt + "> " + pszCommand);
    LogSingleLine(&tmNow, "OPERATOR", str.c_str());
  }
}

void CLog::LogScript (const string &sScript, const char *pszCommand)
{
  //++
  //   And this method will log input received from a script file.  Script
  // files are logged to the log file if the message level is WARNING or less
  // (the same condition as for logging operator commands).  HOWEVER, script
  // files are also logged to the console IF the console level is set to DEBUG.
  //--
  if (GetDefaultFileLevel() <= WARNING) {
    TIMESTAMP tmNow;  GetTimeStamp(&tmNow);
    string str(sScript + ": " + pszCommand);
    LogSingleLine(&tmNow, "SCRIPT", str.c_str());
  }
  if (GetDefaultConsoleLevel() <= DEBUG) {
    if (m_pConsole != NULL)
      m_pConsole->Print("%s: %s\n", sScript.c_str(), pszCommand);
    else
      fprintf(stderr, "%s: %s\n", sScript.c_str(), pszCommand);
  }
}
