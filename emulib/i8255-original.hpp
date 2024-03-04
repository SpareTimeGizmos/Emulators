//++
// i8255.hpp -> C8255 Intel programmable peripheral interface class
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
//   This class implements a generic emulation of the Intel 8255 PPI.  It's
// not all that useful by itself, but is intended to be used as a base class
// for system specific emulations.
//
// THIS NEEDS TO BE REWRITTEN TO USE THE GENERIC 8255/PPI IMPLEMENTATION!!!!
//
// REVISION HISTORY:
//  7-JUL-22  RLA   New file.
// 10-JUL-22  RLA   Make StrobedOutputX() and InputReadyX() do nothing!
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ std::string class, et al ...
#include "MemoryTypes.h"        // address_t and word_t data types
#include "EventQueue.hpp"       // CEventQueue declarations
#include "Interrupt.hpp"        // CInterrupt definitions
#include "Device.hpp"           // generic device definitions
using std::string;              // ...


// Specific PPI types ...
enum _PPI_TYPES {
  PPI_UNKNOWN   =    0,     // undefined
  PPI_I8255     = 8255,     // ubiquitous Intel 8255 or 8256 
  PPI_CDP1851   = 1851,     // RCA CDP1851 programmable I/O interface
};
typedef enum _PPI_TYPES PPI_TYPE;


class C8255 : public CDevice {
  //++
  // Intel 8255 "programmable peripheral interface" emulation ...
  //--
 
  // 8255 registers and bits ...
public:
  enum {
    // 8255 register offsets relative to the base address
    PORTA         = 0,        // Port A (read/write)
    PORTB         = 1,        // Port B (read/write)
    PORTC         = 2,        // Port C (read/write)
    CONTROL       = 3,        // control byte (write only)
    REG_COUNT     = 4,        // total number of byte wide registers
    // Control register bits ...
    CTL_MODE_SET  = 0x80,     // must be 1 to enable mode setting
    CTL_A_MODE_0  = 0x00,     // port A mode 0 (simple I/O)
    CTL_A_MODE_1  = 0x20,     // port A mode 1 (unidirectional strobed I/O)
    CTL_A_MODE_2  = 0x40,     // port A mode 2 (bidirectional I/O)
    CTL_A_MODE    = 0x60,     // mask for port A mode bits
    CTL_A_INPUT   = 0x10,     // port A is an input
    CTL_CU_INPUT  = 0x08,     // port C upper nibble are inputs
    CTL_B_MODE_0  = 0x00,     // port B mode 0 (simple I/O)
    CTL_B_MODE_1  = 0x04,     // port B mode 1 (unidirectional strobed I/O)
    CTL_B_MODE    = 0x04,     // mask for port B mode bits
    CTL_B_INPUT   = 0x02,     // port B are inputs
    CTL_CL_INPUT  = 0x01,     // port C lower nibble are inputs
    CTL_RESET     = 0x9B,     // control register setting after a RESET
    // Bit set/reset functions ...
    BSR_SELECT    = 0x0E,     // select the bit to alter (0..7)
    BSR_SET       = 0x01,     // set selected bit (otherwise clear it)
    // Port C alternate bit assignments for modes 1 and 2 ...
    //   Note that these bits are what the software would see when reading
    // from port C (the Intel datasheet calls this the "status" register).
    // These are slightly different from the pin definitions - in particular,
    // the strobe (STB) and acknowledge (ACK) pins don't appear here and are
    // replaced with interrupt enable bits.
    // 
    //   Also note that port A is capable of both input and output at the same
    // time, so it has separate bits for input buffer full/output buffer empty.
    // Likewise, port A has two interrupt enable bits, one for input and one
    // for output.
    // 
    //   Lastly, in the datasheet the OBF (output buffer full) bits are inverted
    // - they're zero when the buffer is full.  Here we simply call them OBE
    // (output buffer empty) instead, which inverts the meaning but the actual
    // bits are the same.
    PC_OBEA       = 0x80,     // one when output buffer A is empty
    PC_IEAO       = 0x40,     // port A interrupt enable FOR OUTPUT!
    PC_IBFA       = 0x20,     // one when input buffer A is full
    PC_IEAI       = 0x10,     // port A interrupt enable FOR INPUT!
    PC_IRQA       = 0x08,     // any interrupt request for port A
    PC_IENB       = 0x04,     // port B interrupt enable
    PC_IBFB       = 0x02,     // one when input buffer B is full
    PC_OBEB       = PC_IBFB,  // one when output buffer B is empty
    PC_IRQB       = 0x01,     // interrupt request for port B
    // Port C bits that are used when port A is in mode 1 ...
    //   Note that these are different for INPUT vs OUTPUT modes!
    PC_A_MODE_1_INPUT  = PC_IBFA | PC_IEAI | PC_IRQA,
    PC_A_MODE_1_OUTPUT = PC_OBEA | PC_IEAO | PC_IRQA,
    // Port C bits that are used when port A is in mode 2 ...
    PC_A_MODE_2        = PC_A_MODE_1_INPUT | PC_A_MODE_1_OUTPUT,
    // Port C bits that are used when port B is in mode 1 ...
    PC_B_MODE_1        = PC_IENB | PC_IBFB | PC_IRQB,
  };

  // Constructor and destructor ...
public:
  C8255 (const char *pszName, address_t nPort, address_t cPorts=REG_COUNT, CEventQueue *pEvents=NULL);
  virtual ~C8255() {};
private:
  // Disallow copy and assignments!
  C8255(const C8255&) = delete;
  C8255& operator= (C8255 const &) = delete;

  // Public properties ...
public:
  // Return the specific PPI subtype ...
  virtual PPI_TYPE GetType() const {return PPI_I8255;}

  // Device methods inherited from CDevice ...
public:
  virtual void ClearDevice() override;
  virtual uint8_t DevRead (address_t nPort) override;
  virtual void DevWrite (address_t nPort, uint8_t bData) override;
  virtual void ShowDevice (ostringstream &ofs) const override;

  // Test the 8255 mode register for various conditions ...
public:
  // Return true if port A has the specified mode ...
  inline bool IsSimpleA()  const {return (m_bMode & CTL_A_MODE) == CTL_A_MODE_0;}
  // Note that this returns true for EITHER mode 1 or 2!
  inline bool IsStrobedA() const {return (m_bMode & CTL_A_MODE) != CTL_A_MODE_0;}
  inline bool IsBiDirA()   const {return (m_bMode & CTL_A_MODE) == CTL_A_MODE_2;}
  // Same, but for port B (and there are only two modes here) ...
  inline bool IsSimpleB()  const {return (m_bMode & CTL_B_MODE) == CTL_B_MODE_0;}
  inline bool IsStrobedB() const {return (m_bMode & CTL_B_MODE) == CTL_B_MODE_1;}
  // Return true if port A or B is an input or output ...
  inline bool IsInputA()  const {return ISSET(m_bMode, CTL_A_INPUT);}
  inline bool IsInputB()  const {return ISSET(m_bMode, CTL_B_INPUT);}
  inline bool IsOutputA() const {return !IsInputA();}
  inline bool IsOutputB() const {return !IsInputB();}
  // Same thing for port C, but we have two halves to contend with here ...
  inline bool IsInputCU() const {return ISSET(m_bMode, CTL_CU_INPUT);}
  inline bool IsInputCL() const {return ISSET(m_bMode, CTL_CL_INPUT);}

  // Simple, non-strobed, I/O emulation ...
public:
  //   These methods are called whenever a new byte is written to ports A, B
  // or C in simple, mode 0, output mode.
  virtual void OutputA (uint8_t bNew) {};
  virtual void OutputB (uint8_t bNew) {};
  virtual void OutputC (uint8_t bNew) {};
  //   And these methods are called whenever the simulation tries to read a
  // byte in simple, mode 0, input mode.  Whatever they return is what will be
  // passed to the simulated software.
  virtual uint8_t InputA() {return 0xFF;}
  virtual uint8_t InputB() {return 0xFF;}
  virtual uint8_t InputC() {return 0xFF;}

  // Strobed input/output emulation ...
public:
  // These are the methods we call which a derived class should override ...
  virtual void StrobedOutputA (uint8_t bData) {};
  virtual void StrobedOutputB (uint8_t bData) {};
  virtual void InputReadyA() {};
  virtual void InputReadyB() {};
  // These are the methods we implement which a derived class should call ...
  void StrobedInputA (uint8_t bData);
  void StrobedInputB (uint8_t bData);
  void OutputDoneA();
  void OutputDoneB();

  // Interrupt emulation ...
  //   Note that the 8255 requires TWO independent interrupt assignements; one
  // for port A and the other for port B.  Fortunately the CDevice base class
  // provides for exactly two interrupt channels, and they're conveniently
  // already named Interrupt A and Interrupt B!

  // Local methods ...
private:
  // Handle reads or writes for ports A, B or C...
  uint8_t ReadA();
  uint8_t ReadB();
  uint8_t ReadC();
  void WriteA (uint8_t bData);
  void WriteB (uint8_t bData);
  void WriteC (uint8_t bData);
  // Figure the status bit mask for port C depending on the current mode ...
  uint8_t GetStatusMask() const;
  // Handle the port C bit set/reset command ...
  void BitSetReset (uint8_t bControl);
  // Handle mode changes ...
  void NewMode (uint8_t bNewMode);
  // Update the current interrupt request status ...
  void UpdateInterrupts();

  // Private member data...
private:
  //   Ports A and B both potentially have latches for both input and output.
  // Port A is fully bidirectional and can be strobed either way.  Port B is
  // unidirectional, but it can still do strobed transfers either direction.
  uint8_t   m_bInputA, m_bOutputA;  // current port A contents
  uint8_t   m_bInputB, m_bOutputB;  //  "   "    "  B   "  "
  uint8_t   m_bInputC, m_bOutputC;  //  "   "    "  C   "  "
  uint8_t   m_bStatus;              // current status for modes 1 and 2
  uint8_t   m_bMode;                // current mode control byte
};
