//++
// INS8250.hpp -> C8250 (simple serial terminal) class
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
//   This class implements a 8250 UART connected to the console window.
// This is a typical console terminal for any number of microprocessors.
//
// REVISION HISTORY:
// 12-AUG-19  RLA   New file.
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ std::string class, et al ...
#include "CPU.hpp"              // CPU definitions
#include "Device.hpp"           // generic device definitions
#include "UART.hpp"             // generic UART base class
using std::string;              // ...
class CVirtualConsole;          // ...
class CEventQueue;              // ...


class CINS8250 : public CUART {
  // 8250 Registers and bits ...
public:
  enum {
    REG_RBR       = 0x00,     // receiver buffer register (read only!)
    REG_THR       = 0x00,     // transmitter holding register (write only!)
    REG_DLL       = 0x00,     // divisor latch low (when DLAB=1!)
    REG_DLM       = 0x01,     //    "      "   high   "    "  "
    REG_IER       = 0x01,     // interrupt enable register
    REG_IIR       = 0x02,     // interrupt indentification register (read only!)
    REG_LCR       = 0x03,     // line control register
    REG_MCR       = 0x04,     // modem control register
    REG_LSR       = 0x05,     // line status register
    REG_MSR       = 0x06,     // modem status register
    REG_SCR       = 0x07,     // scratch register (unused on the 8250!)
    REG_COUNT     = 8,        // number of UART registers
    // Interrupt enable register (IER) bits ..
    IER_RDA       = 0x01,     // received data available
    IER_THRE      = 0x02,     // transmitter holding register empty
    IER_LSR       = 0x04,     // line status change
    IER_MSR       = 0x08,     // modem status change
    // Interrupt identification register (IIR) bits ...
    IIR_NOINT     = 0x01,     // zero if an interrupt is pending
    IIR_ID        = 0x06,     // interrupt identification (2 bits)
    IIR_NONE      = 0x00,     // interrupt ID - no interrupt requested
    IIR_RLS       = 0x06,     //    "   "   " - receiver line status
    IIR_RDA       = 0x04,     //    "   "   " - received data available
    IIR_THRE      = 0x02,     //    "   "   " - transmitter holding register
    IIR_MODEM     = 0x00,     //    "   "   " - modem status change
    // Line control register (LCR) bits ...
    LCR_WLS0      = 0x01,     // word length select 0
    LCR_WLS1      = 0x02,     //   "     "      "   1
    LCR_STBS      = 0x04,     // stop bits select
    LCR_PEN       = 0x08,     // parity enable
    LCR_EPE       = 0x10,     // even parity enable
    LCR_STICK     = 0x20,     // "stick" parity
    LCR_BREAK     = 0x40,     // foce break
    LCR_DLAB      = 0x80,     // baud divisor latch enable
    // Modem control register (MCR) bits ...
    MCR_DTR       = 0x01,     // data terminal ready
    MCR_RTS       = 0x02,     // request to send
    MCR_OUT1      = 0x04,     // GPIO #1
    MCR_OUT2      = 0x08,     // GPIO #2
    MCR_LOOP      = 0x10,     // enable loopback
    // Line status register (LSR) bits ...
    LSR_DR        = 0x01,     // receiver data ready
    LSR_OE        = 0x02,     // overrun error
    LSR_PE        = 0x04,     // parity error
    LSR_FE        = 0x08,     // framing error
    LSR_BI        = 0x10,     // break interrupt
    LSR_THRE      = 0x20,     // transmitter holding register empty
    LSR_TEMT      = 0x40,     // transmitter enpty
    // Modem status register (MSR) bits ...
    MSR_DCTS      = 0x01,     // CTS changed
    MSR_DDSR      = 0x02,     // DSR changed
    MSR_TERI      = 0x04,     // RI trailing edge
    MSR_DDCD      = 0x08,     // DCD changed
    MSR_DELTA     = 0x0F,     // mask of ALL "changed" bits
    MSR_CTS       = 0x10,     // current CTS state
    MSR_DSR       = 0x20,     //   "     DSR   "
    MSR_RI        = 0x40,     //   "     RI    "
    MSR_DCD       = 0x80,     //   "     DCD   "
  };

  // Constructor and destructor ...
public:
  CINS8250 (const char *pszName, address_t nBase, CEventQueue *pEvents, CVirtualConsole *pConsole, CCPU *pCPU=NULL);
  virtual ~CINS8250() {};
private:
  // Disallow copy and assignments!
  CINS8250 (const CINS8250&) = delete;
  CINS8250& operator= (CINS8250 const &) = delete;

  // CINS8250 device methods from CDevice ...
public:
  virtual UART_TYPE GetType() const override {return UART_INS8250;}
  virtual void ClearDevice() override;
  virtual uint8_t DevRead(uint16_t nRegister) override;
  virtual void DevWrite (uint16_t nRegister, uint8_t bData) override;
  virtual void ShowDevice (ostringstream &ofs) const override;

  // Private methods ...
  // TRUE if loopback mode is enabled ...
  bool IsLoopback() const {return ISSET(m_bMCR, MCR_LOOP);}
  // TRUE if the divisor latch access bit is set ...
  bool IsDLAB() const {return ISSET(m_bLCR, LCR_DLAB);}
  // Update various registers AND handle the side effects ...
  void UpdateMSR (uint8_t bNew);
  void UpdateMSR (uint8_t bSet, uint8_t bClear) {UpdateMSR((m_bMSR & ~bClear) | bSet);}
  void UpdateMCR (uint8_t bNew);
  void UpdateLSR (uint8_t bNew);
  void UpdateLSR (uint8_t bSet, uint8_t bClear) {UpdateLSR((m_bLSR & ~bClear) | bSet);}
  void UpdateLCR (uint8_t bNew);
  void UpdateIER (uint8_t bNew);
  virtual void UpdateRBR (uint8_t bNew) override;
  // Read or write various registers, again handling the side effects ...
  uint8_t ReadMSR();
  uint8_t ReadLSR();
  uint8_t ReadRBR();
  void WriteTHR (uint8_t bData);
  // Handle transmitter events ...
  virtual void TransmitterDone() override;
  // Return TRUE if the transmitter or receiver is busy ...
  virtual bool IsRXbusy() const override {return ISSET(m_bLSR, LSR_DR);}
  virtual bool IsTXbusy() const override {return ISSET(m_bLSR, LSR_THRE);}

  // Private member data...
protected:
  // Simulated 8250 registers ...
  uint8_t   m_bRBR;             // receiver buffer register (read only!)
  uint8_t   m_bTHR;             // transmitter holding register (write only!)
  uint8_t   m_bIER;             // interrupt enable register
  uint8_t   m_bIIR;             // interrupt indentification register (read only!)
  uint8_t   m_bLCR;             // line control register
  uint8_t   m_bMCR;             // modem control register
  uint8_t   m_bLSR;             // line status register
  uint8_t   m_bMSR;             // modem status register
  uint8_t   m_bSCR;             // scratch register (unused on the 8250!)
  uint16_t  m_wDivisor;         // baud rate divisor
};
