//++
// SmartConsol.cpp -> CSmartWindow Console w/file transfer definitions
//
//   COPYRIGHT (C) 2015-2023 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
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
//   The CSmartConsole object sits between any CVirtualConsole object (such as
// a WindowsConsole or LinuxConsole) and any other thing that needs a console
// object (usually a UART emulation of some kind).  This class adds file transfer
// functions to the console window, including -
// 
//   * Logging raw console output to a text file ...
//   * Sending a text file to the UART emulation as input ...
//   * Receving a file from the UART using the XMODEM protocol
//   * Sending a file to the UART using the XMODEM protocol
// 
// NOTES:
//   * It is possible to send a text file while a console log file is opened.  The
// text file sent will appear in the log file ONLY IF the simulated system echos
// the input text.
// 
//   * It is possible to either send or receive an XMODEM file while a console log
// is opened. The XMODEM protocol, including any file data transmitted or received,
// will NOT appear in the log!
// 
//   * It is NOT POSSIBLE to both send and receive an XMODEM file at the same time.
// If one transfer is active, then any attempt to start the other will fail.
// 
//   * ASCII text files for SEND/TEXT and RECEIVE/TEXT are opened in binary mode.
// That's because we want to send or log ASCII characters exactly as they appear
// at the UART, without any messing about by the C RTL.  The only processing done
// is to turn CRLF into a bare CR when sending.
// 
//   * The XMODEM protocol doesn't implement any timeouts.  If the emulation does
// not respond in a timely fashion, it's up to the user to interrupt emulation
// and fix it!
// 
//   * The XMODEM protocol also doesn't implement error recover.  If the emulation
// NAKs us for any reason, we just print an error message and abort.  It _is_ an
// error free communication channel, after all!
//
// Bob Armstrong <bob@jfcl.com>   [11-NOV-2023]
//
// REVISION HISTORY:
// 11-NOV-23  RLA   New file.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdio.h>              // fputs(), fprintf(), all the usual stuff ...
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include <stdarg.h>             // va_start(), va_end(), et al ...
#include <string.h>             // strcpy(), memset(), strerror(), etc ...
#include <sys/types.h>          // size_t, ...
#include "EMULIB.hpp"           // emulator library definitions
#include "CommandParser.hpp"    // SetDefaultExtension(), ...
#include "VirtualConsole.hpp"   // CVirtualConsole declarations
#include "LogFile.hpp"          // message logging facility
#include "SmartConsole.hpp"     // declarations for this module

// Default file extensions  ...
const char *CSmartConsole::m_pszDefaultLogType     = ".log";  // default extension for log files
const char *CSmartConsole::m_pszDefaultTextType    = ".txt";  //    "      "   "    "  text files
const char *CSmartConsole::m_pszDefaultBinaryType  = ".bin";  //    "      "   "    "  XMODEM files


CSmartConsole::CSmartConsole (CEventQueue *pEvents, const char *pszTitle)
  : CConsoleWindow(pszTitle)
{
  //++
  //   The constructor just initializes all the local members.  There's
  // a lot of them!
  //--
  assert(pEvents != NULL);
  m_pEvents = pEvents;

  // Log file variables ...
  m_sLogName.clear();  m_pLogFile = NULL;
  m_cbLogBuffer = m_cbLogTotal = 0;
  memset(m_abLogBuffer, 0, sizeof(m_abLogBuffer));

  // Text file variables ...
  m_sTextName.clear();  m_pTextFile = NULL;
  m_cbTextBuffer = m_cbTextNext = m_cbTextTotal = 0;
  m_fNoCRLF = true;  m_fCRlast = false;
  memset(m_abTextBuffer, 0, sizeof(m_abTextBuffer));
  m_qSendCharDelay = SEND_CHAR_DELAY;  m_qSendLineDelay = SEND_LINE_DELAY;

  // XMODEM binary file variables ...
  m_sXname.clear();  m_pXfile = NULL;  m_Xstate = XIDLE;
  m_cbXbuffer = m_cbXnext = m_cbXtotal = m_bXchecksum = m_bXcurrentBlock = 0;
  memset(m_abXbuffer, XPAD, sizeof(m_abXbuffer));
  m_qXdelay = XMODEM_DELAY;

  // Others ...
  m_fTXready = true;
}

CSmartConsole::~CSmartConsole()
{
  //++
  // The destructor will abort any transfer in progress ...
  //--
  if (IsLoggingOutput()) CloseLog();
  if (IsSendingText()) AbortText();
  if (IsXactive()) XAbort();
}

int32_t CSmartConsole::RawRead (uint8_t *pabBuffer, size_t cbBuffer, uint32_t lTimeout)
{
  //++
  //   This routine is called by the emulation whenever it wants to read a
  // character from the console terminal.  It first checks to see whether we're
  // doing an XMODEM transfer or sending a raw text file and, if neither of
  // those is happening, it passes the call along to the CConsoleWindow.
  //--
  assert(cbBuffer > 0);
  if (IsSendingText())
    return NextTextByte(*pabBuffer) ? 1 : 0;
  //   If we're doing an XMODEM transfer BUT we're currently waiting for the
  // transfer to start or a block to be acknowledged, then go ahead and let
  // the user type.  In particular, this allows the user to first issue a
  // RECEIVE command in the emulator, resume emulation, and then issue a SAVE
  // command to the emulated software.
  else if (IsXactive() && XSendByte(*pabBuffer))
    return 1;
  //   No XMODEM transfer - check for a raw text file instead.  Note that we
  // never allow both XMODEM and raw text to be active at the same time!
  // Otherwise let the console window handle it...
  else
    return CConsoleWindow::RawRead(pabBuffer, cbBuffer, lTimeout);
}

void CSmartConsole::RawWrite (uint8_t ch)
{
  //++
  //   Send just one character to the console, and to the log file too if
  // that's active now.
  // 
  //   IMPORTANT - Note that this routine is used internally to this module
  // only.  It's not a standard CVirtualConsole method that we override, and
  // it's never called directly by any emulation routines!  
  // 
  //   For that reason, it also DOESN'T HANDLE THE XMODEM PROTOCOL!
  //--
  if (IsLoggingOutput()) WriteLog(ch);
  CConsoleWindow::RawWrite((const char *) &ch, 1);
}

void CSmartConsole::RawWrite (const char *pabBuffer, size_t cbBuffer)
{
  //++
  //   This routine is called by the emulation whenever it wants to send a
  // character to the console terminal.  We first check to see whether we're
  // doing an XMODEM transfer and handle that separately.  Otherwise the byte
  // gets passed off to CConsoleWindow and, if we're capturing the text to a
  // log file, it also gets written to the log.
  //--

  //   If an XMODEM transfer is NOT active, then just send the entire bufer
  // out to the console and the log file (if active) expeditiously!
  if (!IsXactive()) {
    if (IsLoggingOutput()) WriteLog(pabBuffer, cbBuffer);
    CConsoleWindow::RawWrite(pabBuffer, cbBuffer);
    return;
  }

  //   When XMODEM is active, regardless of the transfer direction, and we're
  // in the middle of a data block then always hand off all bytes to the XMODEM
  // engine.  However when we're between blocks, waiting for either an ACK or
  // EOT, if we receive anything other than ACK or EOT then pass that along to
  // the console window.  That way if the emulated sofware decides to print a
  // message or something, the user will see it.
  for (size_t cb = 0; cb < cbBuffer; ++cb) {
    uint8_t ch = pabBuffer[cb];
    if (        ((m_Xstate==WAIT_BLOCK) && !((ch==SOH) || (ch==EOT)))
        || (    ((m_Xstate==WAIT_ACK_NAK) || (m_Xstate==WAIT_ACK_FINISH) || (m_Xstate==WAIT_NAK_START))
            && !((ch==ACK) || (ch==NAK))) )
       RawWrite(ch);
    else
       XReceiveByte(ch);
  }
}

void CSmartConsole::ScheduleTX (uint64_t qDelay)
{
  //++
  // Schedule a "transmit ready" event after qDelay nanoseconds...
  //--
  assert(m_pEvents != NULL);
  assert(qDelay > 0);
  m_fTXready = false;
  m_pEvents->Schedule(this, EVENT_TXREADY, qDelay);
}

void CSmartConsole::EventCallback (intptr_t lParam)
{
  //++
  //   The "transmit ready" event (which is the only event type we have!)
  // just sets the m_fTXready flag.  The RawRead() routine uses that to know
  // when it's time to send the next byte.
  //--
  assert(lParam == EVENT_TXREADY);
  m_fTXready = true;
}


////////////////////////////////////////////////////////////////////////////////
/////////////////////////////   LOG FILE METHODS   /////////////////////////////
////////////////////////////////////////////////////////////////////////////////

bool CSmartConsole::OpenLog (const string &sFileName, bool fAppend)
{
  //++
  //   This method will open an ASCII log file.  Future console output will be
  // written to this file until it's closed.  If the fAppend parameter is true
  // then we'll append the current text to the end of any existing file;
  // otherwise an existing file will be overwritten.  It returns true if all is
  // well, and false if the file can't be created for some reason.  
  //--
  if (IsLoggingOutput()) CloseLog();
  m_sLogName = CCmdParser::SetDefaultExtension(sFileName, m_pszDefaultLogType);
  int err = fopen_s(&m_pLogFile, m_sLogName.c_str(), (fAppend ? "wba" : "wb"));
  if (err != 0) {
    CMDERRS("unable (" << err << ") to open file " << m_sLogName);
    return false;
  }
  LOGS(WARNING, "capturing console output to file " << m_sLogName);
  memset(m_abLogBuffer, 0, sizeof(m_abLogBuffer));
  m_cbLogTotal = m_cbLogBuffer = 0;
  return true;
}

void CSmartConsole::FlushLogBuffer()
{
  //++
  //    Flush the log file buffer to the file and reset the buffer to the
  // empty status again.  If any error occurs while writing the log file, the
  // file is closed and future logging stops.
  //--
  assert(m_pLogFile != NULL);
  if (m_cbLogBuffer == 0) return;
  size_t m_cbWritten = fwrite(m_abLogBuffer, 1, m_cbLogBuffer, m_pLogFile);
  if (m_cbWritten != m_cbLogBuffer) {
    int nError = ferror(m_pLogFile);
    LOGS(ERROR, "error (" << nError << ") writing file " << m_sLogName);
    CloseLog(false);
  }
  m_cbLogTotal += m_cbWritten;  m_cbLogBuffer = 0;
}

void CSmartConsole::WriteLog (uint8_t ch)
{
  //++
  //   This method will add another byte to the log file buffer and, if the
  // buffer is full, flush it out to the file.  If any error occurs the log
  // file is closed and future logging will stop.
  //--
  if (IsLoggingOutput()) {
    m_abLogBuffer[m_cbLogBuffer++] = ch;
    if (m_cbLogBuffer >= sizeof(m_abLogBuffer)) FlushLogBuffer();
  }
}

void CSmartConsole::WriteLog (const char *pabBuffer, size_t cbBuffer)
{
  //++
  // Same as above, but this time send an entire buffer of text to the log ...
  //--
  if (!IsLoggingOutput()) return;
  for (size_t cb = 0; cb < cbBuffer; ++cb)  WriteLog(pabBuffer[cb]);
}

void CSmartConsole::CloseLog (bool fFlush)
{
  //++
  //   Close the current log file and, if fFlush is true, flush any text in
  // the current buffer first.
  //--
  if (!IsLoggingOutput()) return;
  if (fFlush && (m_cbLogBuffer > 0)) FlushLogBuffer();
  LOGS(WARNING, "Wrote " << m_cbLogTotal << " bytes to " << m_sLogName);
  fclose(m_pLogFile);  m_pLogFile = NULL;
}


////////////////////////////////////////////////////////////////////////////////
/////////////////////////////   TEXT FILE METHODS   ////////////////////////////
////////////////////////////////////////////////////////////////////////////////

bool CSmartConsole::SendText (const string &sFileName)
{
  //++
  //   This routine will open an ASCII text file and start sending it to the
  // emulation.  If we're currently in the process of sending another file,
  // either another text file or via XMODEM, then that transfer will be
  // aborted!  It returns true if all is well, and false if the file can't be
  // opened for some reason.  
  //--

  // Cancel any current transfers, either text or XMODEM ...
  if (IsSendingText()) AbortText();
  if (IsXactive()) XAbort();

  // Open the file (note that the default extension is ".txt") ...
  m_sTextName = CCmdParser::SetDefaultExtension(sFileName, m_pszDefaultTextType);
  int err = fopen_s(&m_pTextFile, m_sTextName.c_str(), "rb");
  if (err != 0) {
    CMDERRS("unable (" << err << ") to open file " << m_sTextName);
    return false;
  }

  // Initialize all the send text variables and we're ready to go!
  LOGS(WARNING, "sending text file " << m_sTextName);
  m_cbTextBuffer = m_cbTextNext = m_cbTextTotal = 0;
  m_fTXready = true;  m_fCRlast = false;
  memset(m_abTextBuffer, 0, sizeof(m_abTextBuffer));
  return true;
}

void CSmartConsole::FillTextBuffer()
{
  //++
  //   This method reads the next buffer of ASCII text from the text file.  The
  // result is stored in m_abTextBuffer and the associated buffer pointers are
  // reset accordingly.  If (when!) we reach the end of file then it will close
  // the file.  The caller can (and should!) check for this condition by calling
  // the IsSendingText() method...
  //--
  assert(m_pTextFile != NULL);
  m_cbTextNext = 0;
  m_cbTextBuffer = fread(&m_abTextBuffer, 1, sizeof(m_abTextBuffer), m_pTextFile);
  if (m_cbTextBuffer == 0) {
    int nError = ferror(m_pTextFile);
    if (nError != 0)
      LOGS(ERROR, "error (" << nError << ") reading file " << m_sTextName);
    AbortText();
  }
}

bool CSmartConsole::GetTextByte (uint8_t &ch)
{
  //++
  //   This method will get and return the next byte from the send text buffer.
  // It returns true if all is well, and false if end of file (or any other
  // problem) occurs.  Note that it doesn't do any manipulation of the bytes
  // read (e.g. futzing with CRLF, LF, etc) - all that is handled by the
  // NextTextByte() routine.
  //--
  if (!IsSendingText() || !m_fTXready) return false;
  if (m_cbTextNext >= m_cbTextBuffer) FillTextBuffer();
  if (!IsSendingText()) return false;
  ch = m_abTextBuffer[m_cbTextNext++]; ++m_cbTextTotal;
  return true;
}

bool CSmartConsole::NextTextByte (uint8_t &ch)
{
  //++
  //   This method returns the next character from the text file, AND it
  // handles both the transmit pacing delays AND the CRLF/LF nonsense.
  // 
  //   There are two separate delays used for sending characters to the
  // emulation - m_qSendCharDelay is the delay (in simulated nanoseconds)
  // between individual characters, and m_qSendLineDelay is the delay between
  // lines.  We use the latter any time we detect either a CR or LF, or both,
  // in the text file.
  // 
  //   Also, since most emulated software wants to see a carriage return for
  // end of line, we turn both a bare line feed (aka newline, the standard Un*x
  // style end of line) and the CR LF pair, into a single CR instead.  This
  // behavior is controlled by the m_fNoCRLF setting and, if this is false,
  // then we don't mess with the input text at all.
  //
  //   If there's no text available, either because we've hit the end of file OR
  // because it's not time yet to send another byte, then we return false.
  //--
  if (!m_fTXready) return false;
  if (!GetTextByte(ch)) return false;

  //   If the last character was CR and this is LF, then just throw away the
  // line feed and get the next byte.
  while (m_fNoCRLF && m_fCRlast && (ch == LF)) {
     if (!GetTextByte(ch)) return false;
  }
  m_fCRlast = m_fTXready = false;

  //   Now schedule an event for the appropriate time; either the long delay
  // (if this is an end of line) otherwise the short delay ...
  if ((ch == CR) || (ch == LF)) {
    ScheduleTX(m_qSendLineDelay);
    if (ch == CR) m_fCRlast = true;
    if (m_fNoCRLF && (ch == LF)) ch = CR;
  } else {
    ScheduleTX(m_qSendCharDelay);
  }
  return true;
}

void CSmartConsole::AbortText()
{
  //++
  // Close the current text file that we're sending ...
  //--
  if (!IsSendingText()) return;
  LOGS(WARNING, "Read " << m_cbTextTotal << " bytes from " << m_sTextName);
  fclose(m_pTextFile);  m_pTextFile = NULL;
}


////////////////////////////////////////////////////////////////////////////////
//////////////////////////////   XMODEM METHODS   //////////////////////////////
////////////////////////////////////////////////////////////////////////////////

const char *CSmartConsole::StateToString (XSTATE state)
{
  //++
  //   Return the name associated with a particular XMODEM state.  Yes,
  // you could be clever and do this with a std::map and some macros, but
  // there's no need to make this complicated!
  //--
  switch (state) {
    case XIDLE:             return "XIDLE";
    
    // Receiver states ...
    case WAIT_BLOCK:        return "WAIT_BLOCK";
    case WAIT_BLKNO_1:      return "WAIT_BLKNO_1";
    case WAIT_BLKNO_2:      return "WAIT_BLKNO_2";
    case WAIT_DATA:         return "WAIT_DATA";
    case WAIT_CKSUM:        return "WAIT_CKSUM";
    case SEND_NAK_START:    return "SEND_NAK_START";
    case SEND_ACK:          return "SEND_ACK";
    case SEND_ACK_FINISH:   return "SEND_ACK_FINISH";

    // Transmitter states ...
    case WAIT_NAK_START:    return "WAIT_NAK_START";
    case WAIT_ACK_NAK:      return "WAIT_ACK_NAK";
    case SEND_BLOCK:        return "SEND_BLOCK";
    case SEND_BLKNO_1:      return "SEND_BLKNO_1";
    case SEND_BLKNO_2:      return "SEND_BLKNO_2";
    case SEND_DATA:         return "SEND_DATA";
    case SEND_CKSUM:        return "SEND_CKSUM";
    case WAIT_ACK_FINISH:   return "WAIT_ACK_FINISH";
  }
  return "UNKNOWN";
}

void CSmartConsole::XNextState (XSTATE state)
{
  //++
  //   Set the next state for the XMODEM protocol engine.  This is totally
  // trivial, and the only reason this function exists is to print a debugging
  // message...
  //--
  if (state == m_Xstate) return;
  LOGS(DEBUG, "XMODEM old state " << m_Xstate << " -> new state " << state);
  m_Xstate = state;
}

void CSmartConsole::XScheduleTX (XSTATE state, uint64_t qDelay)
{
  //++
  //  This routine simply does an XNextState() followed by a ScheduleTX().
  // It exists just to save a little typing!
  //--
  XNextState(state);
  ScheduleTX((qDelay == 0) ? m_qXdelay : qDelay);
}

void CSmartConsole::XStart (XSTATE state)
{
  //++
  //   This is common code for both SendFile() and ReceiveFile().  It sets up
  // all the XMODEM related variables for a file transfer, either for sending
  // or receiving, and the state parameter gives the initial state for the
  // XMODEM state machine.
  //--
  m_cbXbuffer = XBLKLEN;  m_cbXnext = m_cbXtotal = 0;
  m_bXcurrentBlock = m_bXchecksum = 0;
  memset(m_abXbuffer, XPAD, sizeof(m_abXbuffer));
  m_fTXready = true;  XNextState(state);
}

bool CSmartConsole::ReceiveFile (const string &sFileName)
{
  //++
  //   Create a binary file and set things up for receiving it from the
  // emulation via XMODEM transfer.  Note that any existing XMODEM transfer
  // or raw text transfer will be aborted!  A log file is not affected,
  // however the XMODEM transfer won't appear in the log.
  //--
  if (IsSendingText()) AbortText();
  if (IsXactive()) XAbort();

  // Create the output file ...
  m_sXname = CCmdParser::SetDefaultExtension(sFileName, m_pszDefaultBinaryType);
  int err = fopen_s(&m_pXfile, m_sXname.c_str(), "wb");
  if (err != 0) {
    CMDERRS("unable (" << err << ") to create file " << m_sXname);
    return false;
  }

  //   Note that the other end is expecting us to send a NAK to start off the
  // XMODEM transfer, so that's our initial state.
  //LOGS(WARNING, "receiving file " << m_sXname);
  XStart(SEND_NAK_START);  m_bXcurrentBlock = 1;
  return true;
}

bool CSmartConsole::SendFile (const string &sFileName)
{
  //++
  //   Open a binary file and send it to the emulation via XMODEM transfer.
  // Note that any existing XMODEM transfer or raw text transfer will be
  // aborted!  A log file is not affected, however the XMODEM transfer won't
  // appear in the log.
  //--
  if (IsSendingText()) AbortText();
  if (IsXactive()) XAbort();

  // Open the input file ...
  m_sXname = CCmdParser::SetDefaultExtension(sFileName, m_pszDefaultBinaryType);
  int err = fopen_s(&m_pXfile, m_sXname.c_str(), "rb");
  if (err != 0) {
    CMDERRS("unable (" << err << ") to read file " << m_sXname);
    return false;
  }

  //   Wait for the other end to send us a NAK, and that'll signal our end
  // to start sending the file...
  //LOGS(WARNING, "sending file " << m_sXname);
  XStart(WAIT_NAK_START);  return true;
}

void CSmartConsole::XFinish()
{
  //++
  // Finish off any XMODEM transfer (in either direction) ...
  //--
  if (m_pXfile == NULL) return;
  LOGS(WARNING, "transferred " << m_cbXtotal << " bytes for " << m_sXname);
  fclose(m_pXfile);  m_pXfile = NULL;
}

void CSmartConsole::XAbort()
{
  //++
  //   This routine is used by the user interface to abort any current XMODEM
  // transfer in progress.  It basically does XFinish() and then sets the next
  // state to IDLE.
  //--
  if (!IsXactive()) return;
  XFinish();  XNextState(XIDLE);
}

bool CSmartConsole::XReadBuffer()
{
  //++
  //   Read the next buffer from the XMODEM binary file and set up all the
  // buffer pointers appropriately.  If there's an error, or if we hit the
  // end of the file, then close the file and return false.
  //--
  assert(m_pXfile != NULL);
  memset(m_abXbuffer, XPAD, sizeof(m_abXbuffer));
  m_cbXbuffer = m_cbXnext = m_bXchecksum = 0;
  m_cbXbuffer = fread(&m_abXbuffer, 1, XBLKLEN, m_pXfile);
  if (m_cbXbuffer == 0) {
    int nError = ferror(m_pXfile);
    if (nError != 0)
      LOGS(ERROR, "error (" << nError << ") reading file " << m_sXname);
    XFinish();  return false;
  }
  m_cbXtotal += m_cbXbuffer;  ++m_bXcurrentBlock;
  return true;
}

bool CSmartConsole::XWriteBuffer (bool fLast)
{
  //++
  //   Write the current XMODEM buffer to the binary file, clear the buffer,
  // and reset all the buffer pointers.  If there's an error, or if we hit
  // the end of the file, then close the file and return false.
  //
  //   XMODEM data blocks are always exactly 128 bytes, never more and never
  // less.  That's a problem for the last data block IF the length of the file
  // being sent isn't a multiple of 128 bytes!  The XMODEM "standard" is to
  // pad the last block with SUB (ASCII 0x1A) characters, because that's what
  // CP/M uses as an end of file character.
  // 
  //   If the fLast parameter is TRUE, then this routine will go thru the
  // current buffer, starting at the end, and remove all padding characters
  // that it finds.  The m_cbXbuffer count will be adjusted accordingly.
  // 
  //   If fLast is TRUE, then we'll also close the file.
  //--
  assert(m_pXfile != NULL);

  // Trim the buffer if this is the last one ...
  if (fLast) {
    while ((m_cbXnext>0) && (m_abXbuffer[m_cbXnext-1]==XPAD)) --m_cbXnext;
  }

  // Write the last block to the file ...
  m_cbXtotal += m_cbXnext;
  if (m_cbXnext > 0) {
    size_t m_cbWritten = fwrite(m_abXbuffer, 1, m_cbXnext, m_pXfile);
    if (m_cbWritten != m_cbXnext) {
      int nError = ferror(m_pXfile);
      LOGS(ERROR, "error (" << nError << ") writing file " << m_sXname);
      XFinish();
    }
  }

  // And close the file if this is the last block ...
  m_cbXnext = m_bXchecksum = 0;  ++m_bXcurrentBlock;
  if (fLast) XFinish();
  return true;
}

void CSmartConsole::XReceiveByte (uint8_t ch)
{
  //++
  //   This is the XMODEM state machine for characters received from the
  // emulation.  Note that, regardless of the overall data transfer direction,
  // there are always some bytes going back and forth in both direction.
  // Because of that you'll find states for both receiving AND transmitting
  // files here.
  //--
  LOGF(DEBUG, "XMODEM state %s received 0x%02X", StateToString(m_Xstate), ch);

  //   One important note about this giant switch statement - all the good
  // (i.e. non-error) outcomes should execute an immediate return.  All the
  // error conditions should print an error message and then fall thru to 
  // the end of the switch, where the transfer will be aborted!
  switch (m_Xstate) {
    case WAIT_BLOCK:
      if (ch == SOH) {
        //   Another block is coming - write out the last block and look for
        // the next one.  Note that we also end up here before the very first
        // data block, in which case the buffer will be empty and we don't
        // want to write it!
        if (m_cbXnext > 0) {
          if (!XWriteBuffer()) {XNextState(XIDLE);  return;}
        } else if (m_bXcurrentBlock == 1) {
          LOGS(WARNING, "receiving file " << m_sXname);
        }
        XNextState(WAIT_BLKNO_1);  return;
      } else if (ch == EOT) {
        // End of file - the other end expects us to send an ACK here ...
        if (!XWriteBuffer(true)) {XNextState(XIDLE);  return;}
        XScheduleTX(SEND_ACK_FINISH);  return;
      } else {
        LOGF(ERROR, "XMODEM received 0x%02X when expecting SOH or EOT", ch);
      }
      break;

    case WAIT_BLKNO_1:
      //   This block number should match the one we're expecting, and if it
      // doesn't then print an error message abort the transfer.  In theory I
      // suppose we could send a NAK, but there's no way to tell the other end
      // to back up to the block we want, so we just give up instead.
      if (ch == m_bXcurrentBlock) {
        XNextState(WAIT_BLKNO_2);  return;
      } else {
        LOGS(ERROR, "XMODEM received block number " << ch << " when expecting " << m_bXcurrentBlock);
      }
      break;

    case WAIT_BLKNO_2:
      //   This should be the "inverse" block number - it's sent this way as a
      // simple check to make sure the first block number was correct ...
      if (ch == (255-m_bXcurrentBlock)) {
        XNextState(WAIT_DATA);  return;
      } else {
        LOGS(ERROR, "XMODEM received inverse block number " << ch << " when expecting " << (255-m_bXcurrentBlock));
      }
      break;

    case WAIT_DATA:
      //   Stuff this byte into the buffer.  XMODEM blocks are always fixed
      // length, and we stay in this state until we've received exactly 128
      // data bytes ...
      assert(m_cbXnext < XBLKLEN);
      m_abXbuffer[m_cbXnext++] = ch;  m_bXchecksum += ch;
      //   If we've received a complete block, then look for the checksum next.
      // Otherwise just stay in the WAIT_DATA state.
      XNextState((m_cbXnext < XBLKLEN) ? WAIT_DATA : WAIT_CKSUM);
      return;

    case WAIT_CKSUM:
      //   This is the checksum byte - verify it against the data in the
      // buffer, and abort the transfer if they don't match.  Once again, in
      // a proper world we should send a NAK here and wait for the other guy
      // to retransmit this block, but we don't.
      //
      //   Note that even though the data block is complete at this point, we
      // don't write it out to the file just yet.  That's because we have to
      // wait to see whether the next character is an SOH or an EOT before we
      // know whether this is the last block in the file.
      if (m_bXchecksum == ch) {
        XScheduleTX(SEND_ACK);  return;
      } else {
        LOGF(ERROR, "XMODEM received checksum 0x%02X but expected 0x%02X", ch, m_bXchecksum);
      }
      break;

    case WAIT_ACK_NAK:
      //   This state is used after we've finished transmitting a complete data
      // block and we're waiting for either an ACK or a NAK.  If we get the ACK
      // then we keep sending; anything else is an error.
      if (ch == ACK) {XScheduleTX(SEND_BLOCK);  return;}
      if (ch == NAK) {
        LOGS(ERROR, "XMODEM received a NAK for our data block");
      }
      break;

    case WAIT_NAK_START:
      //   This state is used when we first start an XMODEM download.  The only
      // difference between this one and the previous WAIT_ACK_NAK is that in
      // this instance NAK is NOT an error!
      if (ch == NAK) {
        LOGS(WARNING, "sending file " << m_sXname);
        XScheduleTX(SEND_BLOCK);  return;
      }
      //LOGF(ERROR, "XMODEM received 0x%02X when expecting NAK to start", ch);
      break;

    case WAIT_ACK_FINISH:
      //   This is the final state of sending a file to the emulation.  We just
      // wait for the other end to ACK us and we're done.
      if (ch == ACK) {XNextState(XIDLE);  return;}
      break;

    default:
      // Should never happen, but ...
      break;
  }

  // Here for all error conditions.  Abort the transfer!
  LOGF(ERROR, "XMODEM protocol error, state %s, data 0x%02X", StateToString(m_Xstate), ch);
  XFinish();  XNextState(XIDLE);
}

bool CSmartConsole::XSendByte (uint8_t &ch)
{
  //++
  //   This is the XMODEM state machine for characters we want to send to the
  // emulation.  Note that, like XReceiveByte(), some of these states apply to
  // both transmitting and receiving files.
  // 
  //   Also note that the m_fTXready flag, along with the XScheduleTX() and
  // EventCallback() methods, is used to pace the tranmitted data.  Even though
  // it's just an emulation, it's still possible for us to send data faster
  // than the emulation can process it, and bytes will be lost unless we slow
  // things down.
  // 
  //   This routine returns true and a byte in ch if it has something to send,
  // and false otherwise.
  //--
  XSTATE OldState = m_Xstate;
  if (!m_fTXready) return false;

  //   Within this switch statement, all the cases which have nothing to send
  // should just immediately return false.  Cases which have a byte to send to
  // the emulation should fall thru to the end of the switch, so that a debug
  // message can be printed.
  switch (m_Xstate) {
    case SEND_NAK_START:
      //   This state starts up the receiver.  The other end expects us to send
      // a NAK to get things going, and after that it'll send a data block.
      ch = NAK;  XNextState(WAIT_BLOCK);  break;

    case SEND_ACK:
      //   This state is used when we finish receiving a data block.  It sends
      // an ACK to the other end, which will then send the next block.  Note
      // that we never NAK a block, even if it's bad.
      ch = ACK;  XNextState(WAIT_BLOCK);  break;

    case SEND_ACK_FINISH:
      //   And this state is used by the receiver after we receive an EOT.  The
      // other end is expecting us to ACK the EOT, and then we're all done.
      ch = ACK;  XNextState(XIDLE);  break;

    case SEND_BLOCK:
      //   This is the first state of transmitting a data block.  We need to
      // read the next data block from the disk file and then send an SOH to
      // the other end, UNLESS we find the end of the file in which case we
      // send an EOT to complete the transfer.
      if (XReadBuffer()) {
        // Send the next data block ...
        ch = SOH;  XScheduleTX(SEND_BLKNO_1);  break;
      } else {
        // End of file - send EOT and wait to receive an ACK back ...
        ch = EOT;  XNextState(WAIT_ACK_FINISH);  break;
      }

    case SEND_BLKNO_1:
      // Send the current block number ...
      ch = m_bXcurrentBlock;  XScheduleTX(SEND_BLKNO_2);  break;
    case SEND_BLKNO_2:
      // Send the complement of the current block number ...
      ch = 255-m_bXcurrentBlock;  XScheduleTX(SEND_DATA);  break;

    case SEND_DATA:
      //   Send all 128 data bytes and accumulate the checksum as we go along.
      // Note that XMODEM always MUST send exactly 128 bytes, even if the last
      // data block wasn't completely full.  In that event we add padding bytes
      // to finish it off.
      assert(m_cbXnext < XBLKLEN);
      ch =  (m_cbXnext < m_cbXbuffer) ? m_abXbuffer[m_cbXnext] : XPAD;
      ++m_cbXnext;  m_bXchecksum += ch;
      //   If we've send a complete block, then send the checksum next.
      // Otherwise just stay in the SEND_DATA state.
      XScheduleTX((m_cbXnext < XBLKLEN) ? SEND_DATA : SEND_CKSUM);
      break;

    case SEND_CKSUM:
      //   The last step when transmitting is to send the checksum byte.  After
      // that, we wait for the other end to send us an ACK (or NAK!).
      ch = m_bXchecksum;  XNextState(WAIT_ACK_NAK);  break;

    default:
      //   All the other states, which should be the receiver states where
      // we're waiting for the other end to send us something, do nothing.
      return false;
  }

  // If we sent a byte to the emulation, log it for debugging ...
  LOGF(DEBUG, "XMODEM state %s sending 0x%02X", StateToString(OldState), ch);
  return true;
}
