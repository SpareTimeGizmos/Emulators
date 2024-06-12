//++
// CDP1877.hpp -> COSMAC Programmable Interrupt Controller definitions
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
//   This class emulates the RCA CDP1877 priority interrupt controller.
//
// REVISION HISTORY:
// 18-JUN-22  RLA   New file.
// 12-JUN-24  RLA   Suppress Apple C++ warning for AcknowledgeRequest ...
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ std::string class, et al ...
#include "MemoryTypes.h"        // address_t and word_t data types
#include "Interrupt.hpp"        // generic priority interrupt controller
#include "CPU.hpp"              // CPU definitions
#include "Device.hpp"           // generic device definitions
using std::string;              // ...


class CCDP1877 : public CPriorityInterrupt, public CDevice, public CInterrupt
{
  //++
  //--

  // CDP1877 registers and bits ...
public:
  enum {
    // Reserved RAM locations ...
    PICSIZE           = 16,   // number of PIC registers
    PICLEVELS         = 8,    // number of interrupt levels supported
    //   Constants for the CDP1877 interrupt levels.  Note that these are defined
    // to be compatible with the CInterrupt class, which numbers levels starting
    // from one and not zero!
    IRQ0 = 1,  IRQ1 = 2,  IRQ2 = 3,  IRQ3 = 4,
    IRQ4 = 5,  IRQ5 = 6,  IRQ6 = 7,  IRQ7 = 8,
    // CDP1877 PIC register offsets from PICBASE ...
    PICSTATUS         = 0,    // status register  (R/O)
    PICMASK           = 0,    // interrupt mask   (W/O)
    PICPOLLING        = 4,    // polling register (R/O)
    PICCONTROL        = 4,    // control register (W/O)
    PICVECTOR         = 8,    // interrupt vector (R/O)
    PICPAGE           = 8,    // ISR page address (W/O)
    // Interrupt mask bits ...
    //   These correspond to the way the SBC1802 interrupts are wired to the CDP1877.
    // Note that these same bits work in the MASK register, the POLLING register, and
    // the STATUS register.
    MASK_SLU1         = 0x80, // CDP1854 UART #1 (expansion)
    MASK_PPI          = 0x40, // CDP1851 PPI(expansion)
    MASK_TIMR         = 0x20, // CDP1878 TIMER(expansion)
    MASK_SLU0         = 0x10, // CDP1854 UART #1 (console)
    MASK_DISK         = 0x08, // IDE / ATA disk interface
    MASK_IRQ2         = 0x04, // spare interrupt
    MASK_RTC          = 0x02, // CDP1879 RTC
    MASK_INPUT        = 0x01, // input/attention button
    // CDP1877 control register bits ...
    CTL_VADN          = 0xF0, // upper nibble of the low vector address byte
    CTL_NRPI          = 0x08, // reset pending interrupts WHEN ZERO!
    CTL_NRMR          = 0x04, // reset all interrupt mask bits WHEN ZERO!
    CTL_VS2B          = 0,    // vector spacing 2 bytes
    CTL_VS4B          = 1,    //    "      "    4   "
    CTL_VS8B          = 2,    //    "      "    8   "
    CTL_VS16          = 3,    //    "      "   16   "
    // Other random stuff ...
    LBR               = 0xC0, // opcode for a long branch instruction
  };

  // Constructor and destructor ...
public:
  CCDP1877 (address_t nBase);
  virtual ~CCDP1877() {};
private:
  // Disallow copy and assignments!
  CCDP1877 (const CCDP1877 &) = delete;
  CCDP1877& operator= (CCDP1877 const &) = delete;

  // Public properties ...
public:
  // Set or clear the master interrupt enable ...
  void SetMasterEnable (bool fEnable=true);
  bool GetMasterEnable() const {return m_fMIEN;}
  // Return true if a given level is masked ...
  inline bool IsMasked (CPriorityInterrupt::IRQLEVEL nLevel) const
    {return ISSET(m_bMask, 1 << (nLevel-1));}
  // Return the current vector page address ...
  inline uint8_t GetPage() const {return m_bPage;}

  // CDP1877 interrupt methods from CInterrupt ...
public:
  // Return TRUE if any interrupt is requested ...
  virtual bool IsRequested() const override;
  // Clear all interrupt requests ...
  virtual void ClearInterrupt() override { ClearDevice(); }
  //   The Apple C++ compiler complains about the next declaration because
  // it hides the overloaded AcknowledgeRequest(IRQLEVEL ...) declaration
  // in the CPriorityInterrupt class.  That's not actually a problem for the
  // CDP1877 implementation, but to make the Apple compiler happy we'll
  // suppress that warning...
#if defined(__APPLE__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Woverloaded-virtual"
#endif
  // Acknowledge an interrupt request ...
  virtual void AcknowledgeRequest() override {};
#if defined(__APPLE__)
#pragma clang diagnostic pop
#endif

  // CDP1877 device methods from CDevice ...
public:
  virtual void ClearDevice() override;
  virtual word_t DevRead (address_t nRegister) override;
  virtual void DevWrite (address_t nRegister, word_t bData) override;
  virtual void ShowDevice (ostringstream &ofs) const override;

  // Other public CDP1877 methods ...
public:
  // Reset the PIC (called by MASTER RESET in the control register!) ...
  void ClearPIC();
  // Find an unmasked, active, interrupt request ...
  CPriorityInterrupt::IRQLEVEL FindInterrupt() const;
  // Compute the vector byte for a given interrupt level ...
  uint8_t ComputeVector (CPriorityInterrupt::IRQLEVEL nLevel) const;

  // Private methods ...
protected:
  // Read the "LBR xxyy" sequence ...
  uint8_t ReadVector();
  // Read the status and polling registers (with side effects!) ...
  uint8_t ReadStatus();
  uint8_t ReadPolling();
  //
  void WriteControl (uint8_t bControl);

  // Private member data...
protected:
  bool        m_fMIEN;          // SBC1802 master interrupt enable
  uint8_t     m_bControl;       // last value written to the control register
  uint8_t     m_bPage;          //   "    "     "  "   "  "  page     "   "
  uint8_t     m_bMask;          //   "    "     "  "   "  "  mask     "   "
  uint8_t     m_nVectorByte;    // current byte in the "LBR XXYY" state machine
};
