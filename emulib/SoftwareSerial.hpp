//++
// SoftwareSerial.hpp -> "Bit Banged" serial console emulation
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
//   Many microprocessors have a single bit software serial interface of some
// kind or another.  The 1802 has its Q output and EF inputs; the 8085 has SID
// and SOD, and the SC/MP and the 2650 have their SENSE and FLAG.  This class
// implements a simple software serial terminal connected to the console window.
// This is a typical console terminal for any number of microprocessors.
//
//   SHOULD THIS INHERIT CUART?????
// 
// REVISION HISTORY:
// 21-JAN-20  RLA   New file.
//  8-FEB-20  RLA   Update for generic CPU sense/flag support.
// 22-JUN-22  RLA   Add nSense and nFlag parameters to GetSense() and SetFlag()
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ std::string class, et al ...
#include "Device.hpp"           // generic device definitions
using std::string;              // ...
class CVirtualConsole;          // ...
class CEventQueue;              // ...
class CCPU;                     // ...


class CSoftwareSerial : public CDevice {
  // Magic numbers and bits ...
private:
  enum {
    //   These constants are the state names for the internal state machines.
    // Both transmitter and receiver use the same states, although some states
    // (e.g. BREAK) are used only by the receiver.  Note that the numeric
    // values assigned are not arbitrary - the code assumes that it can advance
    // to the next state by incrementing the state number!
    DATA_BITS     =  8,       // number of data bits to transmit
//  DATA_BITS     =  7,       // number of data bits to transmit
    STOP_BITS     =  2,       //   "     " stop  "   "     "
    STATE_BREAK   = -1,                   // break received, waiting for idle again
    STATE_IDLE    =  0,                   // waiting for start bit
    STATE_START   = STATE_IDLE+1,         // 0   ... or transmitting the start bit
    STATE_DATA    = STATE_START+1,        // these states correspond to data bits
    STATE_STOP    = STATE_DATA+DATA_BITS, // and the last two states are stop bits
    // Event queue entries for the software serial device
    EVENT_TXBIT   =  1,       // time to transmit the next bit
    EVENT_RXBIT   =  2,       //   "   " receive   "   "    "
    EVENT_TXPOLL  =  3,       // time to poll the keyboard for data
    // Signalling states (see the discussion at the top of the .cpp file!)
    MARK          =  1,       // EIA positive voltage, idle state
    SPACE         =  0,       // EIA negative voltage, start bit state
  };

  // Constructor and destructor ...
public:
  CSoftwareSerial (CEventQueue *pEvents, CVirtualConsole *pConsole, CCPU *pCPU=NULL);
  virtual ~CSoftwareSerial() {};
private:
  // Disallow copy and assignments!
  CSoftwareSerial (const CSoftwareSerial&) = delete;
  CSoftwareSerial& operator= (CSoftwareSerial const&) = delete;

  // Public properties ...
public:
  // Get/set the invert flag ...
  void SetInvert (bool fTXinvert, bool fRXinvert)
    {m_fTXinvert = fTXinvert;  m_fRXinvert = fRXinvert;}
  bool IsTXinverted() const {return m_fTXinvert;}
  bool IsRXinverted() const {return m_fRXinvert;}
  // Get/set the bit delay and polling interval ...
  uint64_t GetBitDelay() const {return m_llBitTime;}
  uint64_t GetPollDelay() const {return m_llPollingInterval;}
  uint32_t GetBaud() const {return NSTOHZ(m_llBitTime);}
  void SetBitDelay (uint64_t llDelay) {m_llBitTime = llDelay;}
  void SetPollDelay (uint64_t llDelay) {m_llPollingInterval = llDelay;}
  void SetBaud (uint32_t lBaud) {m_llBitTime = HZTONS(lBaud);}

  // Methods from CDevice ...
public:
  virtual void ClearDevice() override;
  virtual void EventCallback (intptr_t lParam) override;
  virtual void SetFlag (address_t nFlag, uint1_t bData) override;
  virtual uint1_t GetSense (address_t nSense, uint1_t bDefault=0) override {return m_bTXbit;}
  virtual void ShowDevice (ostringstream &ofs) const override;

  // Private methods ...
protected:
  // Poll the keyboard for characters to transmit ...
  void PollKeyboard();
  // Transmitter functions ...
  void StartTransmitter (uint8_t bData);
  void TransmitNext();
  // Receiver functions ...
  void StartReceiver();
  void ReceiveNext();
  void ReceiverDone (bool fError);
  // Send a serial output to the CPU (handling signal polarity) ...
  void TransmitBit (uint8_t bData);
  // Return TRUE if the transmitter or receiver is busy ...
  bool IsTXbusy() const {return m_nTXstate != STATE_IDLE;}
  bool IsRXbusy() const {return m_nRXstate != STATE_IDLE;}

  // Private member data...
protected:
  // Other members ...
  int       m_nTXstate;         // current transmitter state
  int       m_nRXstate;         //    "    receiver      "
  uint8_t   m_bTXbuffer;        // character being transmitted
  uint8_t   m_bRXbuffer;        //   "   "     "   received
  uint1_t   m_bRXlast;          // last state of the RX input
  bool      m_fRXinvert;        // TRUE to invert the input from the CPU
  uint1_t   m_bTXbit;           // last bit transmitted to the CPU
  bool      m_fTXinvert;        // TRUE to invert the output to the CPU
  uint64_t  m_llBitTime;        // time (ns) to send one bit
  uint64_t  m_llPollingInterval;// interval for checking the keyboard
  uint64_t  m_llLastBitTime;    // time (ns) last bit was received
  CCPU            *m_pCPU;      // the CPU object that owns this device
  CVirtualConsole *m_pConsole;  // the console window we'll use for I/O
};
