//++
// DC319.hpp -> DEC DC319 KL11 compatible UART class
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
//   This class implements a DC319 UART connected to the console window.
// This gives the DCT11 a traditional PDP11 compatible console.
//
// REVISION HISTORY:
//  4-JUL-22  RLA   New file.
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


class CDC319 : public CUART {
  //++
  // DEC DC319 DLART emulation ...
  //--

  // DC319 Registers and bits ...
public:
  enum {
    REG_RXCSR     = 0,        // receiver control and status register
    REG_RXBUF     = 2,        // receiver buffer register
    REG_TXCSR     = 4,        // transmitter control and status register
    REG_TXBUF     = 6,        // transmitter buffer register
    REG_COUNT     = 8,        // number of bytes in the UART address space
    // Receiver control and status register (RCSR) bits ...
    RCV_ACT       = 0004000,  // receiver active (start bit received)
    RCV_DONE      = 0000200,  // receiver done (data ready in RBUF)
    RCV_IE        = 0000100,  // receiver interrupt enable
    // Receiver buffer (RBUF) bits ...
    RBUF_ERR      = 0100000,  // OR of overrun, parity and framing errors
    RBUF_OVER     = 0040000,  // receiver overrun error
    RBUF_FERR     = 0020000,  // framing error
    RBUF_BREAK    = 0004000,  // break received
    RBUF_DATA     = 0000377,  // received data
    // Transmitter control and status register (XCSR) bits ...
    XMIT_READY    = 0000200,  // transmitter ready to send another byte
    XMIT_IE       = 0000100,  // transmitter interrupt enable
    XMIT_PBR      = 0000070,  // programmable baud rate
     XMIT_PBR_300  = 0000000, //  ...  300 baud
     XMIT_PBR_600  = 0000010, //  ...  600 baud
     XMIT_PBR_1200 = 0000020, //  ... 1200 baud
     XMIT_PBR_2400 = 0000030, //  ... 2400 baud
     XMIT_PBR_4800 = 0000040, //  ... 4800 baud
     XMIT_PBR_9600 = 0000050, //  ... 9600 baud
     XMIT_PBR_19200= 0000060, //  ...19200 baud
     XMIT_PBR_38400= 0000070, //  ...38400 baud
    XMIT_MAINT    = 0000004,  // maintenance (loopback) mode
    XMIT_PBRE     = 0000002,  // programmable baud rate enable
    XMIT_BREAK    = 0000001,  // transmit break
    // Transmitter buffer (TBUF) bits ...
    TBUF_DATA     = 0000377,  // transmitted data
    // Other magic constants ...
    STANDARD_BAUD_RATES = 8,  // number of standard baud rates ...
    BITS_PER_CHARACTER  = 10, // the DC319 is fixed at 8N1 format!
  };

  // Constructor and destructor ...
public:
  CDC319 (const char *pszName, address_t nBase, CEventQueue *pEvents, CVirtualConsole *pConsole, CCPU *pCPU=NULL);
  virtual ~CDC319() {};
private:
  // Disallow copy and assignments!
  CDC319 (const CDC319&) = delete;
  CDC319& operator= (CDC319 const &) = delete;

  // CDC319 device methods from CDevice ...
public:
  virtual UART_TYPE GetType() const override {return UART_DC319;}
  virtual void ClearDevice() override;
  virtual uint8_t DevRead (uint16_t nRegister) override;
  virtual void DevWrite (uint16_t nRegister, uint8_t bData) override;
  virtual void ShowDevice(ostringstream &ofs) const override;

  // Overridden methods from CUART ...
public:
  // Handle transmitter events ...
  virtual void TransmitterDone() override;
  // Load a new character into the receiver buffer
  virtual void UpdateRBR (uint8_t bData) override;

  // Device interrupt support ...
  //   Note that the DC319 requires TWO independent interrupt assignements; one
  // for transmit and one for receive.  Fortunately, CDevice provides for two,
  // and we use interrupt channel A for transmit and B for receive ...
public:
  virtual void RequestTxInterrupt (bool fInterrupt=true) {RequestInterruptA(fInterrupt);}
  virtual void RequestRxInterrupt (bool fInterrupt=true) {RequestInterruptB(fInterrupt);}

  // Public methods ...
public:
  // Set the DC319 internal baud rate generator ...
  bool SetBaud (uint32_t lTxBaud, uint32_t lRxBaud);
  // Set or clear the programmable baud rate inhibit input ...
  void SetPBRI (bool fPBRI=true);
  // Enable or disable this DC319 chip ...
  void Enable (bool fEnable=true) {m_fEnabled = fEnable;}
  bool IsEnabled() const {return m_fEnabled;}

  // Private methods ...
private:
  // Update the baud rate whenever it's changed ...
  void UpdateBaud (uint32_t lTxBaud, uint32_t lRxBaud);
  void UpdatePBR (uint8_t bNew);
  // Extract the baud rate select bits from the TxCSR ...
  uint8_t GetPBR(uint16_t wCSR) const  {return (wCSR & XMIT_PBR) >> 3;}
  // Read the receiver buffer (it's read only!)
  uint8_t ReadRxBuf ();
  // Read or write the receiver control/status register ...
  uint8_t ReadRxCSR();
  void WriteRxCSR(uint8_t bData);
  // Write the transmitter buffer register ...
  void WriteTxBuf (uint8_t bData);
  // Read or write the transmitter control/status register ...
  uint8_t ReadTxCSR();
  void WriteTxCSR (uint8_t bData);

  // Private member data...
protected:
  // Simulated DC319 registers (ALL are 16 bits!) ...
  uint16_t  m_wRxCSR;   // receiver control and status register
  uint16_t  m_wRxBuf;   // receiver buffer register
  uint16_t  m_wTxCSR;   // transmitter control and status register
  uint16_t  m_wTxBuf;   // transmitter buffer register
  // Other local data ...
  uint32_t  m_lTxBaud;  // "hardware" transmit baud rate
  uint32_t  m_lRxBaud;  //    "   "   receive    "   "
  bool      m_fPBRI;    // programmable baud rate inhibit input
  bool      m_fEnabled; // TRUE if this DC319 is enabled
  // Standard DC319 baud rates ...
  static uint32_t m_alStandardBauds[STANDARD_BAUD_RATES];
};
