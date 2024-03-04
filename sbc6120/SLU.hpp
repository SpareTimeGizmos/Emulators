//++
// SLU.h - SBC6120 console terminal interface definitions
//
//   Copyright (C) 1999-2022 by Spare Time Gizmos.  All rights reserved.
//
// LICENSE:
//    This file is part of the SBC6120 emulator project. SBC6120 is free
// software; you may redistribute it and/or modify it under the terms of
// the GNU Affero General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any
// later version.
//
//    SBC6120 is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License
// for more details.  You should have received a copy of the GNU Affero General
// Public License along with SBC6120.  If not, see http://www.gnu.org/licenses/.
//
// REVISION HISTORY:
// dd-mmm-yy    who     description
// 18-Oct-15    RLA     Update for VS2013
// 18-Aug-22    RLA     Move from WinEight to SBC6120
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ std::string class, et al ...
#include "Interrupt.hpp"        // CInterrupt definitions
#include "CPU.hpp"              // CPU definitions
#include "Device.hpp"           // generic device definitions
#include "UART.hpp"             // generic UART base class
using std::string;              // ...
class CConsoleWindow;           // ...


class CSLU : public CUART {
  //++
  // SBC6120 console serial interface emulation ...
  //--

  // Registers and bits ...
public: enum {
  };

  // Constructor and destructor ...
public:
  CSLU (const char *pszName, word_t nIOT, CEventQueue *pEvents, CVirtualConsole *pConsole, CCPU *pCPU=NULL);
  virtual ~CSLU() {};
private:
  // Disallow copy and assignments!
  CSLU(const CSLU&) = delete;
  CSLU& operator= (CSLU const &) = delete;

  // CSLU device methods from CDevice ...
public:
  virtual void ClearDevice() override;
  virtual bool DevIOT (word_t wIR, word_t &wAC, word_t &wPC) override;
  virtual void ShowDevice (ostringstream &ofs) const override;

  // Overridden methods from CUART ...
public:
  virtual UART_TYPE GetType() const override {return UART_KL8E;}
  // Handle transmitter events ...
  virtual void TransmitterDone() override;
  // Load a new character into the receiver buffer
  virtual void UpdateRBR (uint8_t bData) override;

  // CSLU local methods ...
private:
  // TRUE if this device is requesting an interrupt ...
  inline bool IsIRQ() const {return m_fIEN && (m_fKBDflag || m_fTPRflag);}
  // Handle keyboard or printer IOTs ...
  bool KeyboardIOT (word_t wIOT, word_t &wAC,  word_t &wPC);
  bool PrinterIOT  (word_t wIOT, word_t &wAC,  word_t &wPC);

protected:
  // keyboard Member variables ...
  bool     m_fIEN;	// interrupt enable for both keyboard and printer
  bool     m_fKBDflag;	// keyboard (receive) flag
  bool     m_fTPRflag;  // printer (transmit) flag
  uint8_t  m_bKBDbuffer;// keyboard buffer
};
