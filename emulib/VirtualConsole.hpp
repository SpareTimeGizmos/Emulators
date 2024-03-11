//++
// VirtualConsole.hpp -> CVirtualConsole definitions
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
//   CVirtualConsole is an abstract interface to a "console window" and it's
// normally used by the UART emulation to talk to the operator.  HOWEVER, it
// can also be a real serial port (in the event you want your virtual UART
// to talk to a real UART) or a serial device emulation (like the DEC TU58,
// for example).
//
// Bob Armstrong <bob@jfcl.com>   [17-NOV-2023]
//
// REVISION HISTORY:
// 11-JUN-15  RLA   New file.
// 26-AUG-22  RLA   Clean up Linux/WIN32 conditionals.
// 17-NOV-23  RLA   Separate into our own file
//                  Add console break handling.  Add IsConsoleBreak() ...
//                  Change existing RS232 break functions to "...SerialBreak(...)"
// 10-MAR-24  RLA   Add ReceiveSerialBreak() function.
//--
#pragma once
#include <string>               // C++ std::string class, et al ...
using std::string;              // ...


class CVirtualConsole {
  //++
  // Abstract console, window or serial port, interface class ...
  //--

  // Constants ...
public:
  enum {
    CH_CONSOLE_BREAK = 0x05,  // default console break character (^E)
#if defined(__linux__)
    CH_SERIAL_BREAK = 0x02,  // default serial break character (^B)
#elif defined(_WIN32)
    CH_SERIAL_BREAK = 0x00,  // Windows uses the PAUSE/BREAK key!
#endif
  };

  // Constructor and destructor ...
public:
  CVirtualConsole (char chConsoleBreak=CH_CONSOLE_BREAK, char chSerialBreak=CH_SERIAL_BREAK) {
    SetConsoleBreak(chConsoleBreak);  SetSerialBreak(chSerialBreak);
  }
  virtual ~CVirtualConsole() {};
private:
  // Disallow copy and assignments!
  CVirtualConsole (const CVirtualConsole&) = delete;
  CVirtualConsole& operator= (CVirtualConsole const&) = delete;

  // Only these methods are required!
public:
  // Send or receive raw data to or from the serial port or console window ...
  virtual int32_t RawRead (uint8_t *pabBuffer, size_t cbBuffer, uint32_t lTimeout=0) = 0;
  virtual void RawWrite (const char *pabBuffer, size_t cbBuffer) = 0;

  //   FYI - the word "break" is used to mean two different things here.  A
  // "serial break" refers to the RS232 long space condition.  This is used by
  // the TU58 emulation, and by a real physical UART (should we ever connect
  // one).  It's basically meaningless for a Windows or Linux console terminal.
  // 
  //   A "Console Break" refers to a Control-E (or whatever special character
  // you may choose) typed by the operator on a Windows or Linux console.  This
  // interrupts the emulation and returns control to the command parser.
 
  // Console Break functions ...
  // Return TRUE if a console break was detected ...
  virtual bool IsConsoleBreak (uint32_t lTimeout=0) {return false;}
  // Change (or return) the console break character ...
  inline virtual void SetConsoleBreak (uint8_t ch) {assert(ch < ' ');  m_chConsoleBreak = ch;}
  inline virtual uint8_t GetConsoleBreak() const {return m_chConsoleBreak;}

  // Serial Break functions ...
  //   Return TRUE if a BREAK (RS232 long space) is currently detected.  Note
  // that the IsSendingSerialBreak() and SendSerialBreak() refer to UART ->
  // console (or TU58, as the case may be) data direction.  This is the UART
  // transmiting a break to the console/TU58.  In this case the duration of
  // the break condition is determined by the firmware driving the UART.
  virtual bool IsSendingSerialBreak() {return false;}
  //   This routine forces the transmitted data to a RS232 space condition thus
  // transmitting a BREAK.  The break condition continues until this routine is
  // called again with false for the parameter.  
  virtual void SendSerialBreak (bool fBreak) {};
  //   And this function is called when the console/TU58 wants to send a break
  // conditon to the UART.  It's the reverse data flow direction from above.
  // The TU58 emulation never uses this, but some emulated systems, notably
  // the RCA MS2000 use a break sent from the console to interrupt program
  // execution.  With the Windows and Linux consoles, this is simulated by
  // pressing some magic key (NOT the same one as the Console Break!) and
  // that triggers a call to this routine.
  virtual bool IsReceivingSerialBreak(uint32_t lTimeout=0) {return false;}
  // Change (or return) the serial break character ...
  inline virtual void SetSerialBreak (uint8_t ch) {assert(ch < ' ');  m_chSerialBreak = ch;}
  inline virtual uint8_t GetSerialBreak() const {return m_chSerialBreak;}

  // Private member data...
protected:
  uint8_t m_chConsoleBreak;   // console break emulation character
  uint8_t m_chSerialBreak;    // character to trigger ReceiveSerialBreak()
};
