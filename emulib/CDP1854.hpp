//++
// CDP1854.hpp -> CCDP1854 (simple serial terminal) class
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
//   This class implements a CDP1854 UART connected to the console window.
// This is a typical console terminal for RCA COSMAC systems.
//
// REVISION HISTORY:
//  4-FEB-20  RLA   New file.
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ std::string class, et al ...
#include "MemoryTypes.h"        // address_t and word_t data types
#include "EventQueue.hpp"       // CEventQueue declarations
#include "Device.hpp"           // generic device definitions
#include "UART.hpp"             // generic UART base class
using std::string;              // ...
class CVirtualConsole;          // ...
class CCPU;                     // ...


class CCDP1854 : public CUART {
  // CDP1854 Registers and bits ...
public:
  enum {
    REG_RBR       = 0x00,     // receiver buffer register (read only!)
    REG_THR       = 0x00,     // transmitter holding register (write only!)
    REG_STS       = 0x01,     // status register (read only!)
    REG_CTL       = 0x01,     // control register (write only!)
    REG_COUNT     = 2,        // number of UART registers
    // Status register (STS) ...
    STS_THRE      = 0x80,     // transmitter holding register empty
    STS_TSRE      = 0x40,     //   "    "    shift     "   "    "
    STS_PSI       = 0x20,     // peripheral status input (unused)
    STS_ES        = 0x10,     // external status (unused)
    STS_FE        = 0x08,     // framing error
    STS_PE        = 0x04,     // parity error
    STS_OE        = 0x02,     // receiver overrun error
    STS_DA        = 0x01,     // receiver data available
    // Control register (CTL) bits ...
    CTL_TR        = 0x80,     // transmit request
    CTL_BREAK     = 0x40,     // force break
    CTL_IE        = 0x20,     // interrupt enable
    CTL_WLS2      = 0x10,     // word length select, bit 2
    CTL_WLS1      = 0x08,     //  "      "      "    bit 1
    CTL_SBS       = 0x04,     // stop bit select
    CTL_EPE       = 0x02,     // even parity enable
    CTL_PI        = 0x01,     // parity inhibit
  };

  // Constructor and destructor ...
public:
  CCDP1854 (const char *pszName, address_t nBase, CEventQueue *pEvents, CVirtualConsole *pConsole, CCPU *pCPU=NULL, address_t nSenseIRQ=UINT16_MAX, address_t nSenseBRK=UINT16_MAX);
  virtual ~CCDP1854() {};
private:
  // Disallow copy and assignments!
  CCDP1854 (const CCDP1854 &) = delete;
  CCDP1854& operator= (CCDP1854 const&) = delete;

  // CCDP1854 device methods from CDevice ...
public:
  virtual UART_TYPE GetType() const override {return UART_CDP1854;}
  virtual void ClearDevice() override;
  virtual word_t DevRead (address_t nRegister) override;
  virtual void DevWrite (address_t nRegister, word_t bData) override;
  virtual uint1_t GetSense (address_t nSense, uint1_t bDefault=0) override;
  virtual void ShowDevice (ostringstream &ofs) const override;

  // Private methods ...
  // Manage the receiver buffer register and handle its side effects ...
  void UpdateRBR (uint8_t bChar) override;
  uint8_t ReadRBR();
  // Update the status register and handle side effects (like interrupts!) ...
  void UpdateStatus (uint8_t bNew);
  void UpdateStatus (uint8_t bSet, uint8_t bClear) {UpdateStatus((m_bSTS & ~bClear) | bSet);}
  uint8_t ReadSTS();
  //
  void WriteTHR (uint8_t bChar);
  void WriteCTL (uint8_t bData);
  // Handle transmitter and receiver events ...
  void TransmitterDone() override;

  // Private member data...
protected:
  // Simulated CDP1854 registers ...
  uint8_t   m_bRBR;             // receiver buffer register (read only!)
  uint8_t   m_bTHR;             // transmitter holding register (write only!)
  uint8_t   m_bSTS;             // status register (read only!)
  uint8_t   m_bCTL;             // control register (write only!)
  bool      m_fIRQ;             // TRUE if we are requesting an interrupt
  bool      m_fTHRE_IRQ;        // THRE interrupt request flip flop
  address_t m_nSenseIRQ;        // GetSense() address for testing IRQ
  address_t m_nSenseBRK;        // GetSense() address for testing break request
  // This table is used to translate a name to a REGISTER ...
//static const CCmdArgKeyword::KEYWORD g_keysRegisters[];
};
