//++
// CDP18S651.cpp -> RCA CDP18S651 floppy disk interface emulation
//
//   COPYRIGHT (C) 2024 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
//
// LICENSE:
//    This file is part of the SBC1802 emulator project.  SBC1802 is free
// software; you may redistribute it and/or modify it under the terms of
// the GNU Affero General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any
// later version.
//
//    SBC1802 is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License
// for more details.  You should have received a copy of the GNU Affero General
// Public License along with SBC1802.  If not, see http://www.gnu.org/licenses/.
//
// DESCRIPTION:
//    This class emulates the RCA CDP18S651 floppy diskette controller, as used
// in the MS2000 development system.  This system came with up to four 5-1/4",
// single sided single density diskette drives, each one holding 322,560 bytes.
// As far as I know no other drives were used on this system, and the MicroDOS
// I have doesn't support any other geometry, so that's all we allow for here.
// 
//    The CDP18S651 itself is basically an NEC uPD765 floppy disk controller
// chip with an external data separator and some other random logic.  The latter
// controls the 1802 DMA functions, counts the number of bytes transferred,
// and turns the drive motors on and off.  I think that's about all it does.
// The uPD765 is emulated by the CUPD765 module, and we take care of the rest
// of the 18S651 specific things here.
//
// REVISION HISTORY:
//  5-MAR-24  RLA   New file.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include <string>               // C++ std::string class, et al ...
#include <iostream>             // C++ style output for LOGS() ...
#include <sstream>              // C++ std::stringstream, et al ...
#include <map>                  // C++ std::map template
#include "EMULIB.hpp"           // emulator library definitions
#include "LogFile.hpp"          // emulator library message logging facility
#include "Interrupt.hpp"        // interrupt simulation logic
#include "EventQueue.hpp"       // I/O device event queue simulation
#include "Memory.hpp"           // basic memory emulation declarations ...
#include "Device.hpp"           // basic I/O device emulation declarations ...
#include "DeviceMap.hpp"        // CDeviceMap declaration
#include "CPU.hpp"              // CCPU base class definitions
#include "uPD765.hpp"           // NEC uPD765 FDC emulation
#include "COSMAC.hpp"           // COSMAC 1802 CPU specific declarations
#include "CDP18S651.hpp"        // declarations for this module



C18S651::C18S651 (CEventQueue *pEvents, class CCOSMAC *pCPU, address_t nSenseIRQ, address_t nSenseMotor)
  : CDevice("FDC", "18S651", "floppy disk controller", CDevice::INOUT, 2, CCOSMAC::MAXDEVICE-1, pEvents),
    CUPD765(pEvents)
{
  //++
  //  Initialize all member variables and set the default diskette geometry ...
  // 
  //   Note that there's no need to call ClearDevice() here, or to initialize
  // the UPD765 emulation.  It's own constructor will take care of that.
  //--
  assert((pCPU != NULL) && (pEvents != NULL));
  m_nSenseIRQ = nSenseIRQ;  m_nSenseMotor = nSenseMotor;
  m_bDMAcontrol = m_bDMAcountH = m_bDMAcountL = 0;
  m_fIRQ = m_fMotorOn = false;  m_pCPU = pCPU;

  // Set the geometry for each drive ...
  for (uint8_t i = 0; i < CUPD765::MAXUNIT; ++i)
    SetGeometry(i, SECTOR_SIZE, SECTORS_PER_TRACK, TRACKS_PER_DISK, NUMBER_OF_HEADS);
}

C18S651::~C18S651()
{
  //++
  //--
}

void C18S651::ClearDevice()
{
  //++
  //   Reset the CDP18S651 and the uPD765 to a known state (the same as what
  // happens with the bus CLEAR signal is asserted!).
  //--
  m_bDMAcontrol = m_bDMAcountH = m_bDMAcountL = 0;
  FDCinterrupt(false);
  m_fMotorOn = false;
  ResetFDC();
}

uint8_t C18S651::DMAread()
{
  //++
  //    This is called by the UPD765 object when it wants to DMA transfer data
  // from memory to the FDC.  We first check the DMACTL register to be sure
  // we've been programmed for DMA in the first place and, if we have, then we
  // call the COSMAC DoDMAoutput() method to simulate a DMA transfer.  After
  // the transfer we decrement the DMA byte count and, if this was the last
  // byte, call the uPD765 TerminalCount() method to end the operation.
  //--
  uint8_t bDMA = m_bDMAcontrol & DMACTL_DMAMASK;
  if ((bDMA != DMACTL_DMAREAD) && (bDMA != DMACTL_CRCREAD)) return 0xFF;
  uint8_t bData = m_pCPU->DoDMAoutput();
  if (--m_bDMAcountL != 0) return bData;
  m_bDMAcountL = DMA_BLOCK_SIZE;
  if (--m_bDMAcountH != 0) return bData;
  TerminalCount();
  return bData;
}

void C18S651::DMAwrite (uint8_t bData)
{
  //++
  //    This routine is called by the CUPD765 object when it wants to DMA
  // transfer data from the FDC to memory.  From the COSMAC's point of view,
  // this is a DoDMAinput() method.  Other than that, it's pretty much the
  // same as the DMAread() method, above ...
  //--
  if ((m_bDMAcontrol & DMACTL_DMAMASK) != DMACTL_DMAWRITE) return;
  m_pCPU->DoDMAinput(bData);
  if (--m_bDMAcountL != 0) return;
  m_bDMAcountL = DMA_BLOCK_SIZE;
  if (--m_bDMAcountH != 0) return;
  TerminalCount();
}

void C18S651::FDCinterrupt (bool fInterrupt)
{
  //++
  //    This method is called by the CUPD765 class whenever it wants to 
  // interrupt the host CPU.  On the CDP18S651 there's a local interrupt enable
  // bit in the DMACTL register that can mask the uPD765 IRQ output, but if
  // that local IE bit is set then we try to interrupt the COSMAC.  
  // 
  //    On the CDP18S651 the interrupt output also goes to a flag bit, normally
  // EF3, and this is NOT affected by the interrupt enable bit.
  //--
  m_fIRQ = fInterrupt;
  if (ISSET(m_bDMAcontrol, DMACTL_IE) && fInterrupt)
    RequestInterrupt(true);
  else
    RequestInterrupt(false);
}

uint1_t C18S651::GetSense (address_t nSense, uint1_t bDefault)
{
  //++
  //   The CDP18S651 drives two EF flags - one is set whenever the uPD765 is
  // requesting an interrupt, regardless of the state of the IE bit in DMACTL.
  // The other EF flag is set when the drive motor is enabled.  Normally this
  // is controlled by the motor on bit in the DMACTL register, HOWEVER the real
  // CDP18S651 has a timer that automatically turns off the drive motor after
  // five seconds of inactivity.  We don't currently emulate that!
  //--
  if (nSense == m_nSenseIRQ)
    return m_fIRQ ? 1 : 0;
  else if (nSense == m_nSenseMotor)
    return ISSET(m_bDMAcontrol, DMACTL_MOTOR) ? 1 : 0;
  else
    return bDefault;
}

void C18S651::DevWrite (address_t nPort, word_t bData)
{
  //++
  //   The CDP18S651 actually uses up the entire I/O space of it's assigned
  // group, however only three ports exist that are actually writable - the
  // uPD765 data register, the DMACNT and DMACTL registers.  Writing to any
  // other port is just ignored.
  //--
  if (nPort == DATA_PORT) {
    // Let the uPD765 handle it...
    WriteData(bData);
  } else if (nPort == DMACNT_PORT) {
    // Reset both the high and low bytes of the DMA count register ...
    LOGF(DEBUG, "CDP18S651 DMACNT=%d", bData);
    m_bDMAcountH = bData;  m_bDMAcountL = DMA_BLOCK_SIZE;
  } else if (nPort == DMACTL_PORT) {
    //   Writing the DMACTL port has a couple of side effects.  First, if the
    // state of the IE bit has changed then we may need to update the CPU
    // interrupt request.  And second, if the TC bit makes a 0 -> 1 transition,
    // then we call the TerminalCount() method.  This aborts any current uPD765
    // operation in progress...
    LOGF(DEBUG, "CDP18S651 DMACTL=0x%02X", bData);
    uint8_t bOld = m_bDMAcontrol;  m_bDMAcontrol = bData;
    if (((bOld^m_bDMAcontrol) & DMACTL_IE) != 0) FDCinterrupt(m_fIRQ);
    if (!ISSET(bOld, DMACTL_TC) && ISSET(m_bDMAcontrol, DMACTL_TC)) TerminalCount();
  }
}

word_t C18S651::DevRead (address_t nPort)
{
  //++
  //    Even though it uses the entire I/O address space of it's group, the
  // CDP18S651 has only two ports that are actually readable - the uPD765 data
  // register, and the uPD765 status register.  Everythng else returns 0xFF.
  //--
  if (nPort == DATA_PORT)
    return ReadData();
  else if (nPort == STATUS_PORT)
    return ReadStatus();
  else
    return 0xFF;
}

void C18S651::ShowDevice (ostringstream& ofs) const
{
  //++
  // Dump the device state for the UI command "SHOW DEVICE" ...
  //--
  ofs << FormatString("CDP18S651 Floppy Diskette Interface\n");
  string str;
  if ((m_bDMAcontrol & DMACTL_DMAMASK) == DMACTL_NODMA)    str += "NO DMA";
  if ((m_bDMAcontrol & DMACTL_DMAMASK) == DMACTL_CRCREAD)  str += "CRC READ";
  if ((m_bDMAcontrol & DMACTL_DMAMASK) == DMACTL_DMAREAD)  str += "DMA READ";
  if ((m_bDMAcontrol & DMACTL_DMAMASK) == DMACTL_DMAWRITE) str += "DMA WRITE";
  if (ISSET(m_bDMAcontrol, DMACTL_TC))    str += " TC";
  if (ISSET(m_bDMAcontrol, DMACTL_IE))    str += " IE";
  if (ISSET(m_bDMAcontrol, DMACTL_MOTOR)) str += " MOTOR ON";
  ofs << FormatString("  DMACTL=0x%02X (%s), DMACNT=%d/%d, IRQ=%d\n",
    m_bDMAcontrol, str.c_str(), m_bDMAcountH, m_bDMAcountL, m_fIRQ);
  ofs << std::endl;
  ShowFDC(ofs);
}

