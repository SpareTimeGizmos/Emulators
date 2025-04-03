//++
// CDP1851.hpp -> CCDP1851 RCA programmable I/O interface class
//
// DESCRIPTION:
//   This class implements a generic emulation of the RCA CDP1851 PPI.  It's
// not all that useful by itself, but is intended to be used as a base class
// for system specific emulations.
//
// REVISION HISTORY:
// 31-OCT-24  RLA   New file.
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ std::string class, et al ...
#include "MemoryTypes.h"        // address_t and word_t data types
//#include "EventQueue.hpp"       // CEventQueue declarations
//#include "Interrupt.hpp"        // CInterrupt definitions
#include "Device.hpp"           // generic device definitions
#include "PPI.hpp"              // generic parallel I/O interface
using std::string;              // ...
class CEventQueue;              // ...


class CCDP1851 : public CPPI {
  //++
  // RCA CDP1851 "programmable I/O interface" emulation ...
  //--
 
  // CDP1851 registers and bits ...
public:
  enum {
    // CDP1851 register offsets relative to the base address ...
    //   Note that register 0 is not used - writes to it are ignored, and reads
    // will leave the bus tri-stated (which returns all 1s)...
    CONTROL       = 1,        // control byte (write only)
    STATUS        = 1,        // status (read only)
    PORTA         = 2,        // Port A (read/write)
    PORTB         = 3,        // Port B (read/write)
    REG_COUNT     = 3,        // total number of byte wide registers
    // Status register bits ...
    STS_BINT      = 0x01,     // B interrupt status
    STS_AINT      = 0x02,     // A   "    "     "
    STS_INTBSTB   = 0x04,     // A interrrupt was caused by B strobe
    STS_INTASTB   = 0x08,     // "   "    "    "    "    "  A    "
    STS_ARDY      = 0x10,     // A ready input
    STS_ASTB      = 0x20,     // A strobe input
    STS_BRDY      = 0x40,     // B ready input
    STS_BSTB      = 0x80,     // B strobe input
    // Control register bits - Mode set function (table I) ...
    CTL_MODE_MASK   = 0xC0,   // mask for MODE SET command
    CTL_MODE_SET_A  = 0x08,   // set port A mode
    CTL_MODE_SET_B  = 0x10,   //  "   "   B   "
    CTL_MODE_INPUT  = 0x00,   // all port bits are inputs
    CTL_MODE_OUTPUT = 0x40,   //  "    "   "    "  outputs
    CTL_MODE_BITPR  = 0xC0,   // individual port bits bit programmable
    CTL_MODE_BIDIR  = 0x80,   // bidirectional
    // Control register bits - STROBE/READY control (table II) ...
    CTL_IOC_PORT_B  = 0x02,   // 0->select READY/STROBE A, 1->select B
    CTL_IOC_UPD_RDY = 0x04,   // update READY per bit 6
    CTL_IOC_UPD_STB = 0x08,   // update STROBE per bit 7
    CTL_IOC_RDY_DATA= 0x10,   // READY data (when used as output)
    CTL_IOC_STB_DATA= 0x20,   // STROBE data (when used as output)
    CTL_IOC_RDY_OUT = 0x40,   // READY  is used as an output
    CTL_IOC_STB_OUT = 0x80,   // STROBE  "   "   "  "   "
    // Control register bits - Logical conditions and mask (table III) ...
    CTL_INT_FNMASK  = 0x60,   // mask for interrupt function code
    CTL_INT_NAND    = 0x00,   // NAND of all masked bits
    CTL_INT_OR      = 0x20,   // OR   "   "     "    "
    CTL_INT_NOR     = 0x40,   // NOR  "   "     "    "
    CTL_INT_AND     = 0x60,   // AND  "   "     "    "
    CTL_INT_NEWMASK = 0x10,   // new bit mask follows
    CTL_INT_PORTB   = 0x08,   // select port B (port A otherwise)
    // Control register bits - Interrupt control (table IV) ...
  };
  // Control register state machine ...
  enum _CTL_REG_STATE {
    CTL_REG_IDLE,             // control register idle
    CTL_REG_BITP_MASK_NEXT,   // bit programmable direction mask next
    CTL_REG_INTMASK_NEXT,     // interrupt bit mask next
  };
  typedef enum _CTL_REG_STATE CTL_REG_STATE;

  // Constructor and destructor ...
public:
  CCDP1851 (const char *pszName, address_t nPort, CEventQueue *pEvents=NULL,
    address_t nReadySenseA=UINT16_MAX, address_t nReadySenseB=UINT16_MAX,
    address_t nIntSenseA=UINT16_MAX, address_t nIntSenseB=UINT16_MAX);
  virtual ~CCDP1851() {};
private:
  // Disallow copy and assignments!
  CCDP1851(const CCDP1851&) = delete;
  CCDP1851& operator= (CCDP1851 const &) = delete;

  // Public properties ...
public:
  // Return the specific PPI subtype ...
  virtual PPI_TYPE GetType() const override {return PPI_CDP1851;}

  // Device methods inherited from CDevice ...
public:
  virtual void ClearDevice();
  virtual uint8_t DevRead (address_t nPort) override;
  virtual void DevWrite (address_t nPort, uint8_t bData) override;
  virtual void ShowDevice(ostringstream& ofs) const override;
  virtual uint1_t GetSense (address_t nSense, uint1_t bDefault=0) override;

  // Unique public methods for CCDP1851 ...
public:
  // Return TRUE when port A or B is in bit programmable mode ...
  bool IsBitModeA() const {return GetModeA() == BIT_PROGRAMMABLE;}
  bool IsBitModeB() const {return GetModeB() == BIT_PROGRAMMABLE;}
  //   Asynchronously update any input pins for port A or B, IF that port is
  // set for bit programmable mode.  As an important side effect, cause an
  // interrupt if that particular bit is programmed as an interrupt source.
  virtual void UpdateInputA (uint8_t bData);
  virtual void UpdateInputB (uint8_t bData);
  //   These methods are called when bit programmable mode is used and either
  // READY or STROBE is configured as an output.  This lets any derived class
  // know that the state of these pins has changed, and it can override these
  // methods to take whatever action is necessary.
  virtual void OutputReadyA  (uint1_t bNew) {};
  virtual void OutputReadyB  (uint1_t bNew) {};
  virtual void OutputStrobeA (uint1_t bNew) {};
  virtual void OutputStrobeB (uint1_t bNew) {};
  //   And these methods are called when bit programmable mode is used and
  // either READY or STROBE is configured as an input.  Any derived class can
  // override these and return whatever value is appropriate.  Note that they
  // probably should be called InputReadyA() and InputReadyB(), but we can't
  // because the CPPI class uses those names for something else!
  virtual void UpdateReadyA  (uint1_t bNew)
    {if (IsBitModeA() && (m_fRdyDirA == 0)) m_bReadyA  = bNew;  UpdateReadyStrobe();}
  virtual void UpdateReadyB  (uint1_t bNew)
    {if (IsBitModeB() && (m_fRdyDirB == 0)) m_bReadyB  = bNew;  UpdateReadyStrobe();}
  virtual void UpdateStrobeA (uint1_t bNew)
    {if (IsBitModeA() && (m_fStbDirA == 0)) m_bStrobeA = bNew;  UpdateReadyStrobe();}
  virtual void UpdateStrobeB (uint1_t bNew)
    {if (IsBitModeB() && (m_fStbDirB == 0)) m_bStrobeB = bNew;  UpdateReadyStrobe();}

  // Local methods ...
private:
  // Compute the READY bits for ports A or B ...
  bool IsReadyA() const;
  bool IsReadyB() const;
  // Read the status register ...
  uint8_t ReadStatus();
  // Write the control register ...
  void WriteControl (uint8_t bData);
  // Handle the mode sete control port writes ...
  void ModeSet (uint8_t bData);
  void SetBitProgrammable (uint8_t bPortAB, uint8_t bMask);
  void SetStrobeReady (uint8_t bControl);
  // Handle the interrupt control and enable control port writes ...
  void InterruptControl (uint8_t bData);
  void InterruptEnable (uint8_t bData);
  // Compute the bit masked interrupt ...
  bool InterruptMask (uint8_t bData, uint8_t bMask, uint8_t bIntFn);
  // Update the current interrupt request status ...
  virtual void UpdateInterrupts() override;
  // Update the READY A/B and STROBE A/B status bits ..
  void UpdateReadyStrobe();
  // COnvert the control state to a string for debugging ...
  static const char *ControlToString (CTL_REG_STATE nState);

  // Private member data...
private:
  CTL_REG_STATE m_ControlState;     // current "state" of the control register
  uint8_t m_bLastControl;           // last byte written to control register
  uint8_t m_bPortAB;                // port A/B select bits from control word
  uint8_t m_bIntMaskA, m_bIntMaskB; // bit programmable interrupt mask
  uint8_t m_bIntFnA, m_bIntFnB;     // interrupt function (AND, OR, etc) port A
  address_t m_nReadySenseA;         // sense flag (EF) for A READY
  address_t m_nReadySenseB;         //   "     "   "    "  B READY
  address_t m_nIntSenseA;           // sense flag (EF) for A interrupt request
  address_t m_nIntSenseB;           //   "     "   "    "  B  "    "    "   "
  uint8_t   m_bStatus;              // current status byte
  //   These are used only in bit programmable mode when READY A/B and STROBE A/B
  // are configured as general purpose I/Os.  Otherwise READY A/B and STROBE A/B
  // have their usual handshaking assignments instead.
  uint1_t m_fRdyDirA, m_fRdyDirB;   // 1 if READY  A or B is an output
  uint1_t m_fStbDirA, m_fStbDirB;   // "  " STROBE "  " "  "  "   "
  uint1_t m_bReadyA,  m_bReadyB;    // last state of READY  A or B inputs or outputs
  uint1_t m_bStrobeA, m_bStrobeB;   //  "     "   "  STROBE "  " "   "    "    "
};
