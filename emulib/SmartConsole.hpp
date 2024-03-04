//++
// SmartConsole.hpp -> CSmartConsole - Console w/file transfer definitions
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
// object (usually a UART emulation of some kind), and adds file transfer
// functions to the console window, including both XMODEM and raw text.
//
// REVISION HISTORY:
// 13-NOV-23  RLA   New file.
//--
#pragma once
#include <string>               // C++ std::string class, et al ...
#include "ConsoleWindow.hpp"    // out base class
#include "MemoryTypes.h"        // address_t and word_t data types
#include "EventQueue.hpp"       // CEventQueue declarations
#include "CPU.hpp"              // needed for HZTONS(), etc...
using std::string;              // ...


class CSmartConsole : public CConsoleWindow, public CEventHandler {
  //++
  // Console with file transfer definitions
  //--

public:
  // Generic constants ...
  enum {
    // Magic XMODEM characters ...
    SOH             = 0x01,         // start of block
    EOT             = 0x04,         // end of transmission (last block)
    ACK             = 0x06,         // block received OK
    LF              = 0x0A,         // line feed
    CR              = 0x0D,         // carriage return
    NAK             = 0x15,         // block NOT received OK
    SUB             = 0x1A,         // used as a padding character for the last block
    XPAD            = SUB,          // standard character for padding last buffer
    // Other  protocol constants ...
    XBLKLEN         = 128,          // XMODEM block size (data bytes only!)
    IOBUFSIZ        = 512,          // internal I/O buffer size   
    // Event queue event type parameters ...
    EVENT_TXREADY   = 1,            // time to transmit next byte
    // Default delay parameters ...
    SEND_CHAR_DELAY = CPSTONS(500), // SendText() sends 500 characters/second
    SEND_LINE_DELAY =  MSTONS(25),  // and delays for 25ms at end of line
    XMODEM_DELAY    = CPSTONS(50),  // and XMODEM sends only 50 cps!
  };

  // XMODEM transfer states ...
public:
  enum _XSTATES {
    // XMODEM state machine states ...
    XIDLE,                      // no XMODEM transfer in progress
                                // Receiver states ...
    SEND_NAK_START,             //  send a NAK and wait to receive data
    WAIT_BLOCK,                 //  waiting for SOH or EOT
    WAIT_BLKNO_1,               //  waiting for first byte of block number
    WAIT_BLKNO_2,               //   "    "  second  "   "   "     "
    WAIT_DATA,                  //  waiting for 128 byte data block
    WAIT_CKSUM,                 //  waiting for checksum byte
    SEND_ACK,                   //  send an ACK and wait for another block
    SEND_ACK_FINISH,            //    "   "  "   "  then finish the transfer
                                // Transmitter states ...
    WAIT_NAK_START,             //  wait fo a NAK and then start sending data
    WAIT_ACK_NAK,               //  waiting for ACK/NAK
    SEND_BLOCK,                 //  send either SOH or EOT
    SEND_BLKNO_1,               //  send first byte of block number
    SEND_BLKNO_2,               //    "  second  "  "    "     "
    SEND_DATA,                  //  send 128 byte data block
    SEND_CKSUM,                 //  send checksum
    WAIT_ACK_FINISH,            //  wait for ACK and finish the transfer
  };
  typedef enum _XSTATES XSTATE;

  // Constructor and destructor ...
public:
  CSmartConsole (CEventQueue *pEvents, const char *pszTitle=NULL);
  virtual ~CSmartConsole();
private:
  // Disallow copy and assignment operations ...
  CSmartConsole (const CSmartConsole &) = delete;
  CSmartConsole& operator= (const CSmartConsole &) = delete;

  // CSmartConsole methods inherited from CVirtualConsole ...
public:
  // Send or receive raw data to or from the serial port or console window ...
  virtual int32_t RawRead (uint8_t *pabBuffer, size_t cbBuffer, uint32_t lTimeout=0) override;
  virtual void RawWrite (const char *pabBuffer, size_t cbBuffer) override;

  // CSmartConsole methods inherited from CEventHandler ...
public:
  virtual void EventCallback (intptr_t lParam) override;
  virtual const char *EventName() const override {return "SmartConsole";}

  // Console log file functions ...
public:
  bool OpenLog (const string &sFileName, bool fAppend=true);
  void CloseLog (bool fFlush=true);
  bool IsLoggingOutput() const {return m_pLogFile != NULL;}
  string GetLogFileName() const 
    {return IsLoggingOutput() ? m_sLogName : string();}

  // Send raw ASCII files ...
public:
  bool SendText (const string &sFileName);
  void AbortText();
  bool IsSendingText() const {return m_pTextFile != NULL;}
  string GetTextFileName() const 
    {return IsSendingText() ? m_sTextName : string();}
  void SetTextDelays (uint64_t qCharDelay, uint64_t qLineDelay)
    {m_qSendCharDelay = qCharDelay;  m_qSendLineDelay = qLineDelay;}
  void GetTextDelays (uint64_t &qCharDelay, uint64_t &qLineDelay) const
    {qCharDelay = m_qSendCharDelay;  qLineDelay = m_qSendLineDelay;}
  void SetTextNoCRLF (bool fNoCRLF) {m_fNoCRLF = fNoCRLF;}
  bool GetTextNoCRLF() const {return m_fNoCRLF;}

  // Send or receive files via XMODEM protocol ...
public:
  static const char *StateToString (XSTATE state);
  bool ReceiveFile (const string &sFileName);
  bool SendFile (const string &sFileName);
  bool IsXactive() const {return m_Xstate != XIDLE;}
  void XAbort();
  string GetXfileName() const 
    {return IsXactive() ? m_sXname : string();}
  void SetXdelay (uint64_t qXdelay) {m_qXdelay = qXdelay;}
  void GetXdelay (uint64_t &qXdelay) const {qXdelay = m_qXdelay;}

  // SendText local routines ...
private:
  void RawWrite(uint8_t ch);
  void FillTextBuffer();
  bool GetTextByte (uint8_t &ch);
  bool NextTextByte (uint8_t &ch);
  void ScheduleTX (uint64_t qDelay);

  // Log file local routines ...
private:
  void FlushLogBuffer();
  void WriteLog (uint8_t ch);
  void WriteLog (const char *pabBuffer, size_t cbBuffer);

  // XMODEM local routines ...
private:
  bool XSendByte (uint8_t &ch);
  void XReceiveByte (uint8_t ch);
  void XStart (XSTATE state);
  void XFinish();
  bool XReadBuffer();
  bool XWriteBuffer (bool fLast=false);
  void XNextState (XSTATE state);
  void XScheduleTX (XSTATE state, uint64_t qDelay=0);

  // Log file locals ...
protected:
  string    m_sLogName;             // name of the current log file
  FILE     *m_pLogFile;             // handle of the log file
  uint8_t   m_abLogBuffer[IOBUFSIZ];// buffer for logging text
  size_t    m_cbLogBuffer;          // number of bytes in logging buffer
  size_t    m_cbLogTotal;           // total bytes logged

  // Send text file locals ...
protected:
  string   m_sTextName;             // name of the current send text file
  FILE    *m_pTextFile;             // handle of the indirect file
  uint8_t  m_abTextBuffer[IOBUFSIZ];// buffer for sending text
  size_t   m_cbTextBuffer;          // number of bytes in text buffer
  size_t   m_cbTextNext;            // pointer to next text character to send
  size_t   m_cbTextTotal;           // total text bytes sent
  bool     m_fNoCRLF;               // change <CRLF> or <LF> into <CR> only
  bool     m_fCRlast;               // last character seen was a carriage return
  uint64_t m_qSendCharDelay;        // delay between characters when sending
  uint64_t m_qSendLineDelay;        // delay between lines/blocks when sending

  // XMODEM file transfer locals ...
protected:
  string    m_sXname;               // current XMODEM file name
  FILE     *m_pXfile;               // current XMODEM file handle
  XSTATE    m_Xstate;               // current XMODEM transfer state
  uint8_t   m_abXbuffer[XBLKLEN];   // buffer for XMODEM data
  size_t    m_cbXbuffer;            // count of bytes in the XMODEM buffer
  size_t    m_cbXnext;              // pointer to next byte in XMODEM buffer
  size_t    m_cbXtotal;             // total number of bytes transferred
  uint8_t   m_bXcurrentBlock;       // current XMODEM block number
  uint8_t   m_bXchecksum;           // current checksum
  uint64_t  m_qXdelay;              // delay between characters for XMODEM

  // Other locals ...
protected:
  CEventQueue *m_pEvents;           // used for scheduling timeouts
  bool         m_fTXready;          // true when it's time to send the next byte

  // Default file extensions (static constants defined in the .cpp file) ...
protected:
  static const char *m_pszDefaultLogType;     // default extension for log files
  static const char *m_pszDefaultTextType;    //    "      "   "    "  text files
  static const char *m_pszDefaultBinaryType;  //    "      "   "    "  XMODEM files
};

//   This little inserter function allows us to write an XMODEM state to a
// stream as the actual name of the state rather than just a number.  It's only
// used for debug messages, but it's still handy!
inline ostream& operator << (ostream &os, CSmartConsole::XSTATE state)
  {os << CSmartConsole::StateToString(state);  return os;}
