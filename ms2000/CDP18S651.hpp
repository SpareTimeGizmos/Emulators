//++
// CDP18S651.hpp -> RCA CDP18S651 floppy disk interface emulation
//
//   COPYRIGHT (C) 2024 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
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
//   This class emulates the RCA CDP18S651 floppy diskette interface as used
// on the MS2000 development system.
//
// REVISION HISTORY:
//  5-MAR-24  RLA   New file.
//--
#pragma once
#include <stdint.h>
#include <string>               // C++ std::string class, et al ...
#include <map>                  // C++ std::map template
#include "MemoryTypes.h"        // address_t and word_t data types
#include "CPU.hpp"              // CPU definitions
#include "Device.hpp"           // generic device definitions
#include "DeviceMap.hpp"        // CDeviceMap declaration
#include "uPD765.hpp"           // NEC uPD765 FDC emulation
using std::string;              // ...

class C18S651 : public CDevice, public CUPD765 {
  //++
  // RCA CDP18S651 floppy diskette interface ...
  //--

  // CDP18S651 magic constants ...
public:
  enum {
    //   The CDP18S651 occupies an entire I/O group, however only a few of the
    // ports are actually used, and those have fixed addresses.
    STATUS_PORT       = 4,      // uPD765 status register (read only!)
    DMACTL_PORT       = 4,      // CDP18S651 DMA control register (write only!)
    DATA_PORT         = 5,      // uPD765 data register (read/write)
    DMACNT_PORT       = 7,      // CDP18S651 DMA byte count (write only!)
    DMA_BLOCK_SIZE    = 128,    // DMACNT register counts in 128 byte blocks
    //   IN theory the CDP18S6541 can be configured to support a number of
    // different floppy drives and formats, including 3-1/2 and 8 inch ones.
    // The MS2000 and MicroDOS, however, only supports one diskette format,
    // and this is it...
    SECTOR_SIZE       = 512,    // 512 bytes per sector
    SECTORS_PER_TRACK =   9,    //   9 sectors per track
    TRACKS_PER_DISK   =  70,    //  70 tracks per diskette
    NUMBER_OF_HEADS   =   1,    // and diskettes are single sided
    // Bits in the CDP18S651 DMA control register (DMACTL_PORT) ...
    DMACTL_NODMA      = 0x00,   // no DMA operation
    DMACTL_CRCREAD    = 0x01,   // DMA read with CRC
    DMACTL_DMAREAD    = 0x02,   // DMA read (no CRC calculation)
    DMACTL_DMAWRITE   = 0x03,   // DMA write
    DMACTL_DMAMASK    = 0x03,   // mask for above DMA bits
    DMACTL_TC         = 0x04,   // assert uPD765 terminal count
    DMACTL_MOTOR      = 0x08,   // enable drive motor
    DMACTL_IE         = 0x10,   // master interrupt enable
  };

public:
  // Constructor and destructor...
  C18S651 (CEventQueue *pEvents, class CCOSMAC *pCPU=NULL, address_t nSenseIRQ=UINT16_MAX, address_t nSenseMotor=UINT16_MAX);
  virtual ~C18S651();
private:
  // Disallow copy and assignments!
  C18S651(const C18S651&) = delete;
  C18S651& operator= (C18S651 const &) = delete;

  // Public diskette interface properties ...
public:

  // CDevice methods implemented by the floppy diskette ...
public:
  // Clear (i.e. a hardware reset) this device ...
  virtual void ClearDevice() override;
  // Read or write from/to the device ...
  virtual word_t DevRead (address_t nPort) override;
  virtual void DevWrite (address_t nPort, word_t bData) override;
  // Called by the CPU to test the state of a sense input ...
  virtual uint1_t GetSense (address_t nSense, uint1_t bDefault) override;
  // Dump the device state for the user ...
  virtual void ShowDevice (ostringstream &ofs) const override;

  // CUPD765 methods implemented here ...
public:
  // Emulate CDP1802 DMA operations ...
  virtual uint8_t DMAread() override;
  virtual void DMAwrite (uint8_t bData) override;
  // Emulate CDP1802 interrupt requests ...
  virtual void FDCinterrupt (bool fInterrupt=true) override;

  // Private member data...
protected:
  uint8_t   m_bDMAcontrol;    // CDP18S651 DMA control register
  uint8_t   m_bDMAcountH;     // CDP18S651 DMA count register (high byte)
  uint8_t   m_bDMAcountL;     //  "   "     "    "    "   "   (low byte)
  bool      m_fIRQ;           // true if an interrupt is requested by uPD765
  bool      m_fMotorOn;       // true if the drive motor is turned on
  address_t m_nSenseIRQ;      // GetSense() address for testing IRQ
  address_t m_nSenseMotor;    // GetSense() address for motor ON status
  class CCOSMAC *m_pCPU;      // pointer to the COSMAC CPU for DMA
};
