//++
// CDP1877.cpp -> COSMAC Programmable Interrupt Controller emulation
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
//   This module emulates the RCA CDP1877, which is a priority interrupt
// controller (aka PIC) made specifically for the 1802 CPU.  This chip is an odd
// beast and is rarely seen, but the SBC1802 has one. The 1877 implements eight
// edge triggered interrupt inputs, IRQ0 thru IRQ7. There is also a mask register
// which allows for individual IRQ inputs to be selectively disabled.  If an
// interrupt request is present on any unmasked input then the 1877 will output
// a master interrupt request to the 1802.
//
//   The 1877 must be memory mapped as a peripheral (more on this later) and you
// are expected to point R1 (the interrupt PC) at the PIC vector register in the
// PIC's memory mapped address space.  When the CPU interrupts and tries to fetch
// an from this location, the CDP1877 will cleverly supply a $C0 byte (which is
// an 1802 LBR/long branch opcode) followed by two address bytes.  The first, high
// order byte, is programmer specified by loading the PIC's "page" register, and
// the second, low, byte is generated on by by the PIC on the fly.  This low byte
// encodes the highest priority interrupt currently active, and that gives the
// 1802 eight different prioritized vectored interrupts.  Clever, no?
//
//   Maybe too clever...  The CDP1877 datasheet isn't too clear on a lot of of the
// more subtle points of PIC operation.  Here are a few helpful things that I
// learned by fooling around with the real chip -
// 
//   * The datasheet would have you believe that the 1877 requires 4K of address
//     space for memory mapping.  4K for a chip that has only 3 actual registers!
//     That's hooey, and by playing some games you can get the footprint down to
//     only 16 bytes.  Refer to the source for the SBC1802 memory PLD if you're
//     interested in how this is done.  Of those 16 bytes, the low order two
//     address bits are ignored, and the upper two select one of three registers.
//
//        $xxx0 is the status (read) or mask (write) register
//        $xxx4 is the polling (read) or control (write) register
//        $xxx8 the vector (read) or page (write) register
//
//     It's apparent why the two low order address bits are ignored - remember
//     that the 1802 CPU wants to read a three byte LBR instruction from the
//     vector register at $xxx8.  Since the PC will increment while the CPU is
//     fetching, this requires addresses $xxx9 and $xxxA to select the same
//     vector register!
// 
//     Also note that address $xxxC isn't used and doesn't select the CDP1877 at
//     all.  Writing this address is ignored, and the bus floats if you try to
//     read it.
//
//   * A one bit in the MASK register DISABLES the corresponding interrupt and a
//     zero bit enables it.  The MASK RESET bit in the control register sets the
//     mask to all zeros and ENABLES everything.
//
//   * The IRn inputs are all EDGE TRIGGERED.  The actual chip is negative edge
//     triggered, but that's because the IRn inputs are active low.  Either way,
//     the F-F is set by the assertion of an interrupt request.  This feature is
//     emulated by the EDGE_TRIGGERED mode of each CSimpleInterrupt object.
//
//     The datasheet isn't too explicit about exactly when these F-Fs are reset,
//     however I know for sure that reading the polling register, or reading the
//     LSB from the vector register, will clear the F-F associated with highest
//     priority UNMASKED input.  Reading the status register will clear ALL
//     unmasked F-Fs.  I'm not clear whether reading the status will also clear
//     the flip flop associated with masked IRQ inputs.
// 
//   * Speaking of the status register, reading it will return the current state
//     of the interrupt request F-F for ALL inputs, masked or not.
//
//   * Consectutive reads of the vector register, $xxx8, will first return $C0,
//     then high vector byte, and then the low vector byte.  Of these three, only
//     the last, the low vector byte, is variable.  The $C0 is constant, and the
//     high vector byte comes verbatim from the CDP1877 "page" register.
//
//     There's obviously some kind of state machine inside the CDP1877 associated
//     with this register, since reading the same register three times gives three
//     different results.  Exactly how this state machine works, and in particular
//     what resets it to the first, $C0, state isn't clear.  Frankly, I'm just
//     faking that part here.
// 
//   * Speaking of which, when writing this code it was a little tricky to figure
//     out exactly how much internal state the CDP1877 really has.  There's vector
//     byte state machine mentioned above, however that might work. There are three
//     8 bit registers for control, page and mask, and there are eight flip flops
//     associated with the eight IRn inputs.  And I believe that's it.
// 
//     The vector generation logic, notably the part that generates the variable
//     least significant byte, is all combinatorial.  It simply encodes the
//     correct vector for the highest priority unmasked IRn input, whatever that
//     is, at the exact moment the polling or third vector byte is read.  As a
//     side effect, the request F-F associated with this vector is reset at the
//     moment the vector byte is read.  
// 
//     And that's it.  The 1802 interrupt acknowledge (S3) cycle does nothing to
//     the 1877.  There's no freezing of the priority chain, nor is the vector
//     computed in advance when an interrupt first occurs.  It's all computed
//     dynamically at the moment it's needed.
//
//   * And lastly, the "polling" register simply returns the third byte of the
//     vector address.  Reading this register is exactly like reading the vector
//     register three times and throwing away the first two bytes.  I think the
//     only use for this register is to allow the software to poll the highest
//     priority interrupt without going thru the whole LBR thing.  
//
//     Reading the polling register DOES reset the request flip-flop associated
//     with the vector returned!
//    
// REVISION HISTORY:
// 18-JUN-22  RLA   New file.
// 25-MAR-25  RLA   Add EnablePIC() ...
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include "EMULIB.hpp"           // emulator library definitions
#include "LogFile.hpp"          // emulator library message logging facility
#include "ConsoleWindow.hpp"    // console window functions
#include "MemoryTypes.h"        // address_t and word_t data types
#include "Interrupt.hpp"        // generic priority interrupt controller
#include "CPU.hpp"              // CPU definitions
#include "Device.hpp"           // generic device definitions
#include "CDP1877.hpp"          // declarations for this module


CCDP1877::CCDP1877 (address_t nBase)
  : CPriorityInterrupt(PICLEVELS, CSimpleInterrupt::EDGE_TRIGGERED),
    CDevice("PIC", "CDP1877", "Programmable Interrupt Control", INOUT, nBase, PICSIZE)
{
  //++
  // Constructor ...
  //--
  m_bControl = m_bPage = m_bMask = 0;
  m_nVectorByte = 0;  m_fMIEN = false;
  m_fEnablePIC = true;
}

void CCDP1877::SetMasterEnable (bool fEnable)
{
  //++
  //   Set or clear the master interrupt enable.  This bit is unique to the
  // SBC1802 and is actually external to the CDP1877.  The actual hardware
  // implements this bit as a part of the MCR, and the MCR emulation code
  // calls this routine to set or clear the enable.
  //--
  m_fMIEN = fEnable;
}

void CCDP1877::ClearPIC()
{
  //++
  //   This routine is called for the MASTER RESET bit in the control register.
  // The datasheet just says that it clears all of the edge triggered interrupt
  // pending latches.  This bit explicitly does NOT clear any of the control,
  // mask or page registers. I also assume that it resets the LBR vector state
  // machine back to the C0 byte, although the datasheet certainly doesn't say
  // that.
  // 
  //   Note that this most explicitly DOES NOT alter the master interrupt enable
  // (m_fMIEN) flag!  That's unique to the SBC1802 and is completely external
  // to the CDP1877...
  //--
  LOGF(TRACE, "CDP1877 MASTER RESET");
  CPriorityInterrupt::ClearInterrupt();
  m_nVectorByte = 0;
}

void CCDP1877::ClearDevice()
{
  //++
  //   This is a standard CDevice routine that's called to simulate a hardware
  // reset.  At this point we need to mention that the real CDP1877 chip does
  // NOT have any kind of reset or clear input, and thus it's unaffected by any
  // hardware reset in the SBC1802.  That's actually why we need the master
  // interrupt enable hack, and the master interrupt enable IS cleared by a reset.
  // 
  //   The CDP1877 does have a "MASTER RESET" bit in the control register however,
  // and that calls the ResetPIC() method.  Even though it isn't strictly correct,
  // we'll do the same here and pretend that our CDP1877 does have a reset pin.
  //--
  m_bControl = m_bPage = m_bMask = 0;
  SetMasterEnable(false);  ClearPIC();
}

CPriorityInterrupt::IRQLEVEL CCDP1877::FindInterrupt() const
{
  //++
  //   This routine will scan thru all priority levels, starting from the highest
  // and working down, looking for one that's both active and not masked by the
  // m_bMask register.  If we find one then we return the associated interrupt 
  // level, and if we don't find any then we return zero. Remember that interrupt
  // levels, at least for CPriorityInterrupt, are one based!
  //
  //   BTW, also remember that on the CDP1877 a one bit in the mask register 
  // DISABLEs the associated priority level.  A mask register of zero ENABLES the
  // interrupt!
  //
  //   And lastly, note that this method does NOT clear the request flip flop
  // associated with the active interrupt, and in fact does not change anything
  // about the state of the interrupt system at all.  That's important!
  //--
  for (CPriorityInterrupt::IRQLEVEL i = PICLEVELS; i > 0; --i) {
    if (IsMasked(i)) continue;
    if (IsRequestedAtLevel(i)) return i;
  }
  return 0;
}

uint8_t CCDP1877::ReadStatus()
{
  //++
  //   Reading the status register returns a byte with a one bit for every
  // interrupt level that has a pending interrupt, whether it's masked or not.
  // Reading the status register CLEARs the edge triggered interrupt request
  // flip flop for ALL levels.  Actually it's not clear whether it clears all
  // request F-Fs or only the ones for levels which aren't masked.  The
  // datasheet doesn't say, but I assume the former.
  //--
  uint8_t bStatus = 0;
  for (uint8_t i = 1; i <= PICLEVELS; ++i) {
    if (IsRequestedAtLevel(i)) {
      CPriorityInterrupt::AcknowledgeRequest(i);
      bStatus |= 1 << (i - 1);
    }
  }
  return bStatus;
}

uint8_t CCDP1877::ComputeVector(CPriorityInterrupt::IRQLEVEL nLevel) const
{
  //++
 //   Figure out the correct vector for this level and given the vector spacing
  // and upper address bits specified in the control register.  And yes, I'm
  // sure you could do this all in one statement with some clever shifting and
  // masking, but sometimes brute force is easier!
  //--
  switch (m_bControl & 0x03) {
    case 0: return (m_bControl & 0xF0) | ((nLevel-1) << 1);
    case 1: return (m_bControl & 0xE0) | ((nLevel-1) << 2);
    case 2: return (m_bControl & 0xC0) | ((nLevel-1) << 3);
    case 3: return (m_bControl & 0x80) | ((nLevel-1) << 4);
  }
  return 0;  // just to make the compiler happy!
}

uint8_t CCDP1877::ReadPolling()
{
  //++
  //   The polling register simply returns the low order, third, byte of the
  // vector address associated with the highest priority, unmasked, active
  // interrupt request.  AFAIK in the real CDP1877 this is all combinatorial
  // logicl - a priority encoder followed by some fancy logic to generate the
  // vector address.  There's no state associated with it and it just always
  // returns the correct vector for right now this moment.
  // 
  //   Note that this isn't a passive operation and there is one serious side
  // effect - the edge triggered interrupt request flip flop associated with
  // the returned vector will also be cleared.
  // 
  //   One thing that's not clear is what should happen if you read the polling
  // register when NO interrupt is active.  The datasheet is totally mim on this
  // topic.  I actually return all zeros for the vector byte in this case, but
  // I doubt that's what the actual chip does.
  //--
  CPriorityInterrupt::IRQLEVEL nLevel = FindInterrupt();
  if (nLevel == 0) return 0;
  uint8_t bPoll = ComputeVector(nLevel);
  CPriorityInterrupt::AcknowledgeRequest(nLevel);
  LOGF(TRACE, "CDP1877 ReadPolling, level=%d, vector=0x%02X", nLevel, bPoll);
  return bPoll;
}

bool  CCDP1877::IsRequested() const
{
  //++
  //   This method is called by the CPU object to determine whether it (the CPU)
  // needs to interrupt.  In the case of the CDP1877, that's true IF there is a
  // pending interrupt on any UNMASKED interrupt level.  And in the case of the
  // SBC1802, the master interrupt enable must be set as well.
  //--
  if (!m_fMIEN || !m_fEnablePIC) return false;
  return (FindInterrupt() > 0);
}

uint8_t CCDP1877::ReadVector()
{
  //++
  //   This routine is called when the CPU reads the CDP1877 vector register (at
  // offset 0x08 in the SBC1802).  This "magic" register returns the three byte
  // sequence for an 1802 long branch instruction one byte at a time.  Each
  // subsequent read of this register returns the next byte in sequence.  It's
  // not clear that the real CDP1877 would do if you read this register a fourth
  // time, but in our case we freeze on the final byte. The member m_nVectorByte
  // counts the bytes in the sequence so we know which is next ...
  //--
  if (m_nVectorByte == 0) {
    ++m_nVectorByte;  return LBR;
  } else if (m_nVectorByte == 1) {
    ++m_nVectorByte;  return m_bPage;
  } else
    return ReadPolling();
}

void CCDP1877::WriteControl (uint8_t bControl)
{
  //++
  //  Writing the control register, other than storing the value in m_bControl,
  // has two immediate side effects for the "MASTER RESET" and "RESET MASK" bits.
  // Note that resetting the mask register actually does set it to all zeros,
  // which ENABLES all interrupt levels!
  // 
  //   Even weirder, note that both reset bits are backwards - a "1" does nothing,
  // and a zero actually resets!
  //--
  LOGF(TRACE, "CDP1877 write control 0x%02X", bControl);
  if (!ISSET(bControl, CTL_NRPI)) ClearPIC();
  if (!ISSET(bControl, CTL_NRMR)) m_bMask = 0;
  //  After that, the only bits we care about are the vector address bits and
  // the vector spacing bits ...
  m_bControl = bControl & (CTL_VADN|CTL_VS16);
}

word_t CCDP1877::DevRead (address_t nRegister)
{
  //++
  //   This method will read from any PIC register.  Note that the PIC
  // is assigned to a 16 byte address space, but it only has three (!)
  // registers.  The addressing is such that the lower two address bits
  // are ignored, and we emulate that here.  In addition, the real chip
  // is not selected by any reference to addresses $C..$F. In the SBC1802
  // those addresses would leave the bus floating with weak pullups.
  //
  //   Lastly, if we read any register EXCEPT the vector register then
  // the long branch state machine for the vector register is reset
  // back to the $C0/LBR state.  I've no idea if this is the way the real
  // CDP1877 works, but it works for our purposes ...
  //--
  assert((nRegister >= GetBasePort())  &&  ((nRegister-GetBasePort()) < PICSIZE));
  nRegister = (nRegister - GetBasePort()) & 0x0C;
  uint8_t bData;
  switch (nRegister) {
    case PICSTATUS:  bData = ReadStatus();    break;
    case PICPOLLING: bData = ReadPolling();   break;
    case PICVECTOR:  bData = ReadVector();    break;
    default:         bData = 0xFF;            break;
  }
  if (nRegister != PICVECTOR) m_nVectorByte = 0;
  LOGF(TRACE, "CDP1877 read register 0x%02X returns 0x%02X", nRegister, bData);
  return bData;
}

void CCDP1877::DevWrite (address_t nRegister, word_t bData)
{
  //++
  //   This method will write to any PIC register.  It's subject to the
  // same odd addressing requirements as reading, so read the comments
  // before the Read() method too.
  //--
  assert((nRegister >= GetBasePort())  &&  ((nRegister-GetBasePort()) < PICSIZE));
  nRegister = (nRegister - GetBasePort()) & 0x0C;
  LOGF(TRACE, "CDP1877 write register 0x%02X = 0x%02X", nRegister, bData);
  switch (nRegister) {
    case PICMASK:     m_bMask = bData;      break;
    case PICCONTROL:  WriteControl(bData);  break;
    case PICPAGE:     m_bPage = bData;      break;
    default:                                break;
  }
}

void CCDP1877::ShowDevice (ostringstream &ofs) const
{
  //++
  // Dump the device state for the UI command "EXAMINE DISPLAY" ...
  //--
  if (!m_fEnablePIC) {
    ofs << FormatString("PIC DISABLED") << std::endl;
  } else {
    ofs << FormatString("Control register 0x%02X, page 0x%02X, mask 0x%02X, vector %d\n",
      m_bControl, m_bPage, m_bMask, m_nVectorByte);
    CPriorityInterrupt::IRQLEVEL nLevel = FindInterrupt();
    if (nLevel > 0)
      ofs << FormatString("Active request at IRQ%d", nLevel - 1);
    else
      ofs << "No active requests";
    ofs << FormatString(", master interrupts are %s",
      m_fMIEN ? "ENABLED" : "disabled");
  }
}

