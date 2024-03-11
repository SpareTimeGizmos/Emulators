//++
// UART.hpp -> CUART generic UART to console terminal emulator class
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
//   This class implements a generic UART emulator that's logically connected
// to the console terminal.  This class is not intended to be used by itself,
// but as a base class for a specific UART emulation (e.g. INS8250, CDP1854,
// S2651, etc).
//
// REVISION HISTORY:
//  6-FEB-20  RLA   New file (adapted from the old implementation)
// 17-JUN-23  RLA   Add Signetics 2651
// 10-MAR-24  RLA   Add received break support
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ std::string class, et al ...
#include "MemoryTypes.h"        // address_t and word_t data types
#include "EventQueue.hpp"       // CEventQueue declarations
#include "Device.hpp"           // generic device definitions
using std::string;              // ...
class CVirtualConsole;          // ...
class CCPU;                     // ...


class CUART : public CDevice {

public:
  enum {
    // Magic UART contants ...
    DEFAULT_SPEED = 2000,     // 2000 cps (a little more than 19,200 baud!)
  };
  enum _UART_TYPES {
    //   These are returned by GetUARTtype() for code that needs to identify
    // the exact type of a CUART derived object.  It's a cheapo RTTI ...
    UART_UNKNOWN  =    0,     // undefined
    UART_SOFTWARE =    1,     // software serial (bit banged)
    UART_INS8250  = 8250,     // National INS8250 UART
    UART_I8251    = 8251,     // Intel 8251 USART
    UART_S2651    = 2651,     // Signetics 2651 PCI
    UART_CDP1854  = 1854,     // RCA CDP1854 UART
    UART_DC319    =  319,     // DEC DC319 UART
    UART_KL8E     = 8650,     // DEC KL8E (M8650)
    UART_IM6402   = 6402,     // dumb, generic 6402 UART
  };
  typedef enum _UART_TYPES UART_TYPE;
private:
  enum {
    // Event queue parameters ...
    EVENT_TXDONE  = 1,        // Event queue type for tranmitter done
    EVENT_RXREADY = 2,        //   "     "    "    "  receiver ready
    EVENT_BRKDONE = 3,        // break condition terminated
  };

  // Constructor and destructor ...
public:
  CUART (const char *pszName, const char *pszType, const char *pszDescription,
         address_t nPort, address_t cPorts,
         CEventQueue *pEvents, CVirtualConsole *pConsole, CCPU *pCPU=NULL);
  virtual ~CUART() {};
private:
  // Disallow copy and assignments!
  CUART (const CUART&) = delete;
  CUART& operator= (CUART const &) = delete;

  // Public properties ...
public:
  // Return the specific UART subtype ...
  virtual UART_TYPE GetType() const {return UART_UNKNOWN;}
  // Return the console window associated with this UART ...
  CVirtualConsole *GetConsole() const {return m_pConsole;}
  // Get/set the bit delay and polling interval ...
  uint64_t GetCharacterDelay() const {return m_llCharacterTime;}
  uint64_t GetPollDelay() const {return m_llPollingInterval;}
  void SetCharacterDelay (uint64_t llDelay) {m_llCharacterTime = llDelay;}
  void SetPollDelay (uint64_t llDelay) {m_llPollingInterval = llDelay;}
  void SetRxSpeed (uint32_t nCPS)
    {assert(nCPS > 0);  SetCharacterDelay(HZTONS(nCPS));}
  void SetTxSpeed (uint32_t nCPS)
    {assert(nCPS > 0);  SetPollDelay(HZTONS(nCPS));}
  // Get/set the time a received BREAK is asserted ...
  void SetBreakDelay(uint64_t llDelay) {m_llBreakTime = llDelay;}
  uint64_t GetBreakDelay() const {return m_llBreakTime;}

  // CUART device methods inherited from CDevice ...
public:
  virtual void ClearDevice() override;
  virtual void EventCallback (intptr_t lParam) override;
  virtual void ShowDevice (ostringstream &ofs) const override;

  // These methods need to be provided by the specific UART implementation ...
  virtual void UpdateRBR (uint8_t bData) {};
  virtual bool IsRXbusy() const {return false;}
  virtual void TransmitterDone() {};
  virtual bool IsTXbusy() const {return false;}
//virtual void SetFramingError() {};

  // And these are our methods the UART implementation can call...
  void StartTransmitter (uint8_t bData, bool fLoopback=false);
  void ReceiverReady();
  bool IsReceivingBreak() const {return m_fReceivingBreak;}
  void ReceivingBreakDone();

  // Private member data...
protected:
  uint64_t  m_llCharacterTime;  // time (ns) to send one character
  uint64_t  m_llPollingInterval;// time (ns) to send one character
  uint64_t  m_llBreakTime;      // duration of a received break
  bool      m_fReceivingBreak;  // TRUE if we are currently receiving a break
  CCPU     *m_pCPU;             // the CPU that owns this UART
  CVirtualConsole *m_pConsole;  // the console window we'll use for I/O
};
