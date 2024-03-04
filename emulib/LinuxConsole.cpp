//++
// LinuxConsole.cpp -> CConsoleWindow implementation for Linux/ANSI
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
// NOTE:
//   This file implements the CConsoleWindow class for the Linux platform using
// ANSI escape sequences and the ncurses library.  The Microsoft Windows version
// is implemented by the WindowsConsole.cpp file.  Both share the same 
// ConsoleWindow.hpp header and class definitions.
//
// DESCRIPTION:
//   This module implements (or tries to implement) WIN32 specific console
// things like set the window title, change the window size and/or reposition
// it on the screen and, ugliest of all, trap control-C and Windows Shutdown
// and LogOff events.  Hence we now have this class - one of the first things
// the main program does is to create a CConsoleWindow object and from then on
// it takes over management of the console, including all console I/O. 
//
//   Unfortunately with the Linux port this became a bit trickier, because the
// Linux assumes and ordinary text console and can even be run remotely using
// telnet or ssh.  The Linux version therefore uses ANSI escape sequences and
// the ncurses library to the same effect.  We can still do a lot of things
// this way, like set the window size, colors and title, but some of things we
// can do on WIN32 are just not possible this way.
//
//   It's probably worth reading the comments at the beginning of the
// WindowsConsole.cpp file because many of the things said there will also
// apply here.  Both files share exactly the same header file and class
// definitions in the file ConsoleWindow.hpp.  This implementation is also
// a modified Singleton object that can be instanciated only once, and so
// on.
//
// Bob Armstrong <bob@jfcl.com>   [5-JUN-2017]
//
// REVISION HISTORY:
//  5-JUN-17  RLA   Split from WindowsConsole.cpp
// 25-DEC-23  RLA   Add keyboard buffer and make IsConsoleBreak() read ahead
//                  DON'T call RawWrite() from Write() ...
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#ifdef __linux__                // THIS ENTIRE FILE IS ONLY FOR LINUX!
#include <stdio.h>              // fputs(), fprintf(), all the usual stuff ...
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include <stdarg.h>             // va_start(), va_end(), et al ...
#include <string.h>             // strcpy(), memset(), strerror(), etc ...
#include <termios.h>            // struct termios (what else?!)
#include <unistd.h>             // read(), write(), exit(), etc ...
#include <sys/types.h>          // size_t, ...
#include <sys/time.h>           // struct timeval, ...
#include "EMULIB.hpp"           // emulator library definitions
#include "VirtualConsole.hpp"   // CVirtualConsole abstract class
#include "ConsoleWindow.hpp"    // declarations for this module
#include "LogFile.hpp"          // message logging facility


// Initialize the pointer to the one and only CConsoleWindow instance ...
CConsoleWindow *CConsoleWindow::m_pConsole = NULL;


CConsoleWindow::CConsoleWindow (const char *pszTitle)
{
  //++
  //   Note that the CConsoleWindow is a singleton object - only one instance
  // ever exists and we keep a pointer to that one in m_pConsole.
  //--
  assert(m_pConsole == NULL);
  m_pConsole = this;
  m_fForceExit = m_fConsoleBreak = false;
  m_KeyBuffer.Clear();

  // Get the current console settings and save them away ...
  m_fRawMode = false;
  m_pRawAttr = DBGNEW struct termios;
  m_pCookedAttr = DBGNEW struct termios;
  memset(m_pCookedAttr, 0, sizeof(struct termios));
  tcgetattr(STDIN_FILENO, m_pCookedAttr);

  // Now setup a copy of the settings, but in raw mode ...
  memcpy(m_pRawAttr, m_pCookedAttr, sizeof(struct termios));
  cfmakeraw(m_pRawAttr);
  //   These two settings effectively disable blocking I/O for the console.
  // A call to read() will read whatever characters are currently buffered
  // and then return immediately.
  m_pRawAttr->c_cc[VMIN] = 0;
  m_pRawAttr->c_cc[VTIME] = 0;

  if (pszTitle != NULL) SetTitle(pszTitle);
}


CConsoleWindow::~CConsoleWindow()
{
  //++
  //   The destructor closes the handles for the console input and output
  // buffers, and restores the original console mode, just in case we've
  // changed it to something funny...
  //--
  //   Reset the pointer to this Singleton object. In theory this would allow
  // another CConsoleWindow instance to be created, but that's not likely to
  // be useful.
  assert(m_pConsole == this);
  CookedMode();
  delete m_pRawAttr;  m_pRawAttr = NULL;
  delete m_pCookedAttr;  m_pCookedAttr = NULL;
  m_pConsole = NULL;
}

void CConsoleWindow::RawMode ()
{
  //++
  //   Select the "raw" mode for terminal input - no intraline editing (DEL, ^U,
  // etc) and no echo.  Note that this does NOT allow ^C to be input; for that,
  // we have to trap SIGINTs.
  //--
  assert(m_pRawAttr != NULL);
  if (m_fRawMode) return;
  tcsetattr(STDIN_FILENO, TCSANOW, m_pRawAttr);
  m_fRawMode = true;
}

void CConsoleWindow::CookedMode ()
{
  //++
  //   Select "cooked" mode for terminal input - echo and intraline editing
  // is back.  Note that this really just restores the terminal settings
  // in effect when this program was started, so what you get now will be
  // what you had then!
  //--
  assert(m_pCookedAttr != NULL);
  if (!m_fRawMode) return;
  tcsetattr(STDIN_FILENO, TCSANOW, m_pCookedAttr);
  m_fRawMode = false;
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////


void CConsoleWindow::Write (const char *pszText)
{
  //++
  //   Write a string to the console window.  This is pretty easy and for now
  // the Linux version just does an fputs() to stdout.  Ideally we'd like send
  // error messages to stderr and regular messages to stdout, but the Windows
  // version doesn't distinguish those two and that information isn't available
  // here.  Maybe some day we'll add it, but not today...
  //--
  assert(pszText != NULL);
  CookedMode();
  fputs(pszText, stdout);
}


void CConsoleWindow::WriteLine (const char *pszLine)
{
  //++
  // Write a string followed by a newline ...
  //--
  if (pszLine != NULL) Write(pszLine);
  Write("\n");
}


void CConsoleWindow::Print (const char *pszFormat, ...)
{
  //++
  // Send printf() style formatted output to the console.
  //--
  assert(pszFormat != NULL);
  CookedMode();
  va_list args;
  va_start(args, pszFormat);
  vfprintf(stdout, pszFormat, args);
  va_end(args);
}


bool CConsoleWindow::ReadLine (const char *pszPrompt, char *pszBuffer, size_t cbBuffer)
{
  //++
  //   This routine will read one line of input from the console window.  Once
  // again, the Linux version is pretty simple for now and just reads from
  // stdin.  
  //
  //   Also note that Windows will return the carriage return and line feed
  // characters ("\r\n") at the end of the buffer.  We strip those out and
  // the string returned to the caller has no special line terminator.
  //--
  assert((pszBuffer != NULL) && (cbBuffer > 0));
  if (m_fForceExit) return false;
  if (pszPrompt != NULL) fputs(pszPrompt, stdout);
  fflush(stdout);  CookedMode();
  if (fgets(pszBuffer, cbBuffer, stdin) == NULL) return false;
  char *psz = strrchr(pszBuffer, '\n');
  if (psz != NULL) *psz = '\0';
  return true;
}

void CConsoleWindow::RawWrite (const char *pabBuffer, size_t cbBuffer)
{
  //++
  //   Write a buffer of one or more characters to the console window.  Should
  // be easy, but a lot of the emulated machines and software like to send
  // funky characters to what they think is an ASR33.  This includes NULLs,
  // RUBOUTs, ASCII characters with the 8th bit set, and all kinds of junk.
  // We need to filter those out, but that involves going thru the caller's
  // buffer one character at a time.
  //
  //   Note that this is used only during emulation.  The command scanner
  // uses the Write() and Print() functions, above.
  //--
  RawMode();

  //   Run thru the caller's buffer first, and if there's nothing bad in there
  // then we can just print it and quit ...
  bool fBad = false;  ssize_t cbWrite;
  for (size_t i = 0; i < cbBuffer; ++i) {
    if (   ISSET(pabBuffer[i], 0x80)
        || (pabBuffer[i] == 0)     ) {fBad = true;  break;}
  }
  
  if (!fBad) {
   // Only good characters here, so take the easy (and fast) option ...
   cbWrite =  write(STDOUT_FILENO, pabBuffer, cbBuffer);
   assert(cbWrite == (ssize_t) cbBuffer);
  } else {
    // Print the buffer one character at a time, fixing each one as we go ...
    for (size_t i = 0;  i < cbBuffer;  ++i) {
      char bChar = pabBuffer[i] & 0x7F;
      if (bChar == 0) continue;
      cbWrite = write(STDOUT_FILENO, &bChar, 1);
      assert(cbWrite == 1);
    }
  }
}

int32_t CConsoleWindow::ReadKey (uint8_t &bData, uint32_t lTimeout)
{
  //++
  //   This routine will read a single keystroke from the console terminal,
  // potentially with a time out.  It will return, without blocking forever,
  // if the timeout period expires and the lTimeout parameter specifies the
  // timeout interval in milliseconds.  The return value is +1 if a key is
  // read, zero if the time out period expired, or -1 if some other error
  // (e.g. window closed, EOF, etc) occurred.  If lTimeout is zero then we
  // check for an existing keystroke in the buffer and return either +1 or
  // zero, but never wait.
  //--
  RawMode();
  struct timeval tmo;
  tmo.tv_sec  =  lTimeout / 1000UL;
  tmo.tv_usec = (lTimeout % 1000UL) * 1000UL;

  fd_set rdfs;
  FD_ZERO(&rdfs);
  FD_SET(STDIN_FILENO, &rdfs);;
  select(STDIN_FILENO+1, &rdfs, NULL, NULL, &tmo);

  if (FD_ISSET(STDIN_FILENO, &rdfs)) {
    ssize_t cbRead = read(STDIN_FILENO, &bData, 1);
    assert(cbRead == 1);
    // Check for console break character ...
    if ((m_chBreak != 0) && (bData == m_chBreak)) {
      m_fConsoleBreak = true;  return 0;
    } else
      return (bData != 0) ? 1 : 0;
  } else
    return 0;
}

bool CConsoleWindow::IsConsoleBreak (uint32_t lTimeout)
{
  //++
  //   Return TRUE if the console break flag is set (i.e. a Control-E or
  // whatever has been seen on input) and then clear the flag.
  // 
  //   Notice that this routine will call ReadKey() repeatedly until there are
  // no more keystrokes waiting.  That's necessary so that we can detect a
  // console break even if the emulated program stops reading keyboard input.
  // Characters we read this way are not discarded, though, and instead we stuff
  // them into the keyboard buffer, m_KeyBuffer.  The RawRead() routine will
  // return characters from this buffer next time it's called.
  //--

  // First poll for any keyboard input ...
  uint8_t bData;
  while (ReadKey(bData, lTimeout) > 0) m_KeyBuffer.Put(bData);

  // Now return the state of the console break flag, and also clear it ...
  bool fBreak = m_fConsoleBreak;
  m_fConsoleBreak = false;
  return fBreak;
}

int32_t CConsoleWindow::RawRead (uint8_t *pabBuffer, size_t cbBuffer, uint32_t lTimeout)
{
  //++
  //   This method will read one or more characters from the keyboard in "raw" 
  // mode, as returned by ReadKey().  The raw characters are stuffed into the
  // buffer until either the buffer becomes full, the ReadKey() times out, or
  // an error occurs.  It returns the number of characters actually read, or
  // zero if the time out expired before any data was entered, or -1 if some
  // error (window closed, EOF, etc) occurred.
  //--
  int32_t cbRead = 0;  uint8_t bData;

  // First try to pull characters from the keyboard typeahead buffer ...
  while (((size_t) cbRead < cbBuffer) && m_KeyBuffer.Get(bData)) {
    *pabBuffer++ = bData;  ++cbRead;
  }

  //   Then if the type ahead buffer is empty and we still want more input,
  // try reading from the actual keyboard ...
  while ((size_t) cbRead < cbBuffer) {
    int32_t nRet = ReadKey(bData, lTimeout);
    if (nRet < 0) return -1;
    if (nRet == 0) return cbRead;
    *pabBuffer++ = bData;  ++cbRead;
  }

  // All done - return the number of characters actually read ...
  return cbRead;

#ifdef UNUSED
  //   Despite what I said above, there's no need to actually use ReadKey()
  // here.  Because VMIN and VTIME are both set to zero in the raw termios
  // structure, read() will never block.  If no characters are available
  // when we call it now, it'll just return zero ...
  RawMode();
  return read(STDIN_FILENO, pabBuffer, cbBuffer);
#endif
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////


void CConsoleWindow::SetTitle (const char *pszTitle, ...)
{
  //++
  //   This method sets the title string of the console window.  It's pretty
  // simple, but notice that it allows for extra printf() style arguments.
  // This makes it easy to include the version number, build date, disk status
  // or whatever else, in the title bar.
  //--
  assert((pszTitle != NULL) && (strlen(pszTitle) > 0));
}


string CConsoleWindow::GetTitle() const
{
  //++
  // Return the current title string for the console window ...
  //--
  return string("");
}


void CConsoleWindow::SetColors (uint8_t bForeground, uint8_t bBackground)
{
  //++
  //   This routine will set the foreground (text) and background (window)
  // colors for the console.  The console palette is very limited and 
  // essentially emulates the CGA display with a range of only 16 colors.
  // Both bForeground and bBackground are four bit values where the value
  // 1 is blue, 2 is green, 4 is red and 8 is the intensify bit.  
  //--
}


bool CConsoleWindow::GetColors (uint8_t &bForeground, uint8_t &bBackground)
{
  //++
  //   Return the current console window colors (see the SetColors() method
  // for more details on the color set).
  //--
  bForeground = WHITE;  bBackground = BLACK;
  return false;
}


bool CConsoleWindow::SetWindowSize (uint16_t nColumns, uint16_t nRows, int32_t nX, int32_t nY)
{
  //++
  //--
  return false;
}


bool CConsoleWindow::GetWindowSize (uint16_t &nColumns, uint16_t &nRows)
{
  //++
  // Return the current console window size, in rows and columns ...
  //--
  nColumns = 80;  nRows = 24;
  return false;
}

bool CConsoleWindow::GetBufferSize (uint16_t &nColumns, uint16_t &nRows)
{
  //++
  // Return the size of the scrolling buffer, in rows and columns ...
  //--
  nColumns = 80;  nRows = 24;
  return false;
}

bool CConsoleWindow::SetBufferSize (uint16_t nColumns, uint16_t nRows)
{
  //++
  // Set the window's scrolling buffer, in rows and columns ...
  //--
  return false;
}

#endif      // end of #ifdef __linux__ from the very top of this file!
