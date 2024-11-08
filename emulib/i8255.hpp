//++
// i8255.hpp -> C8255 Intel programmable peripheral interface class
//
// DESCRIPTION:
//   This class implements a generic emulation of the Intel 8255 PPI.  It's
// not all that useful by itself, but is intended to be used as a base class
// for system specific emulations.
//
// REVISION HISTORY:
//  7-JUL-22  RLA   New file.
// 10-JUL-22  RLA   Make StrobedOutputX() and InputReadyX() do nothing!
//  7-JUL-23  RLA   Change to use CPPI generic PPI base class
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ std::string class, et al ...
#include "MemoryTypes.h"        // address_t and word_t data types
#include "Device.hpp"           // generic device definitions
#include "PPI.hpp"              // generic parallel peripheral interface
using std::string;              // ...
class CEventQueue;              // ...


class C8255 : public CPPI {
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
    // and they're zero when the buffer is full.  Here we simply call them OBE
    // (output buffer empty) instead, which inverts the meaning but the actual
    // bits are the same.
    PC_OBEA       = 0x80,     // one when output buffer A is empty
    PC_IEOA       = 0x40,     // port A interrupt enable FOR BIDIRECTIONAL OUTPUT!
    PC_IBFA       = 0x20,     // one when input buffer A is full
    PC_IEIA       = 0x10,     // port A interrupt enable FOR BIDIRECTIONAL INPUT!
    PC_IRQA       = 0x08,     // any interrupt request for port A
    PC_IENB       = 0x04,     // port B interrupt enable
    PC_IBFB       = 0x02,     // one when input buffer B is full
    PC_OBEB       = PC_IBFB,  // one when output buffer B is empty
    PC_IRQB       = 0x01,     // interrupt request for port B
    // Port C bits that are used when port A is in mode 1 ...
    //   Note that these are different for INPUT vs OUTPUT modes!
    PC_A_MODE_1_INPUT  = PC_IBFA | PC_IEIA | PC_IRQA,
    PC_A_MODE_1_OUTPUT = PC_OBEA | PC_IEOA | PC_IRQA,
    // Port C bits that are used when port A is in mode 2 ...
    PC_A_MODE_2        = PC_A_MODE_1_INPUT | PC_A_MODE_1_OUTPUT,
    // Port C bits that are used when port B is in mode 1 ...
    PC_B_MODE_1        = PC_IENB | PC_IBFB | PC_IRQB,
  };

  // Constructor and destructor ...
public:
  C8255 (const char *pszName, address_t nPort, CEventQueue *pEvents=NULL, address_t cPorts=REG_COUNT);
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
  virtual void ClearDevice();
  virtual uint8_t DevRead (address_t nPort);
  virtual void DevWrite (address_t nPort, uint8_t bData);
  virtual void ShowDevice (ostringstream &ofs) const;

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

  // Local methods ...
private:
  // Handle reads or writes for ports A, B or C...
  virtual uint8_t ReadC() override;
  virtual void WriteC (uint8_t bData) override;
  // Figure the status bit mask for port C depending on the current mode ...
  uint8_t GetStatusMask() const;
  // Handle the port C bit set/reset command ...
  void BitSetReset (uint8_t bControl);
  // Handle mode changes ...
  void NewMode (uint8_t bNewMode);
  void NewModeA();
  void NewModeB();
  void NewModeC();
  // Update the current interrupt request status ...
  virtual void UpdateInterrupts() override;
  // Update the flags (IEN, IBE, OBF, etc) when port C changes ...
  void UpdateFlags (bool fSet, uint8_t bMask);

  // Private member data...
private:
  bool      m_fIEIA, m_fIEOA; // port A interrupt enables for input and output
  uint8_t   m_bStatus;        // current status for modes 1 and 2
  uint8_t   m_bMode;          // current mode control byte
};
