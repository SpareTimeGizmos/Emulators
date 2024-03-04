//++
// Interrupt.cpp -> CSimpleInterrupt (basic "wire OR" interrupt) implementation
//                  CPriorityInterrpt (multilevel priority interrupt) implementation
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
//   The generic interrupt system emulation consists of three classes -
//
//   CInterrupt is a pure abstract class that defines the CPU interface to the
// interrupt system.  Any class that inherits CInterrupt, and that includes
// CSimpleInterrupt as well as the i8259 and CDP1877 priority interrupt controller
// emulations, can be "plugged into" the CCPU implementation.
// 
//   CSimpleInterrupt is a basic "wire OR" interrupt system.  Any number of
// CDevice emulations may be "plugged in" to a CSimpleInterrupt object and
// if any device requests an interrupt, the CPU will interrupt.  Note that the
// standard interrupt interface to the CPU is CInterrupt, but the standard
// interrupt interface for a device is CSimpleInterrupt.  That's because pretty
// much all peripheral devices or chips have an "interrupt request" wire or pin,
// and that's exactly what CSimpleInterrupt models.
// 
//   It's worth pointing out that a CSimpleInterrupt object can be interfaced to
// both a CPU and a set of I/O devices.  One CSimpleInterrupt object is a complete
// emulation of a basic interrupt system where the CPU has a single interrupt pin
// and one or more peripheral chip interrupt requests are ORed together.  A similar
// plan also works for CPUs with multiple interrupt inputs, such as the 8085.  Just
// create a separate CSimpleInterrupt instance for each input, and let the CPU code
// itself handle prioritizing them.
//
//   CPriorityInterrupt emulates an N-level priority interrupt controller, such
// as the Intel 8259 or the RCA CDP1877.  A CPriorityInterrupt object is basically
// an array of CSimpleInterrupt objects, one for each interrupt priority level.
// CPriorityInterrupt probably isn't usable by itself, but it can be inherited by
// a specific PIC implementation.  Note that CPriorityInterrupt DOES NOT implement
// the CInterrupt interface, so it can't be used directly by a CPU.  Once again,
// it's expected that the 1877, 8259, or whatever, emulation will take care of that.
// 
//   Two important notes about CPriorityInterrupt - one, the number of interrupt
// levels implemented is passed as a parameter to the constructor.  This may be 1,
// althought that would give essentially the same behavior as CSimpleInterrupt.
// And two, interrupt levels are numbered STARTING FROM 1 up to the highest level.
// Interrupt level 1 is the lowest priority and m_nLevels is the highest.  Zero
// is NOT AN INTERRUPT LEVEL and is frequently used to indicate that no interrupt
// is pending.
// 
// REVISION HISTORY:
// 12-AUG-19  RLA   New file.
// 22-JUN-22  RLA   Add priority levels.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <cstring>              // needed for memset()
#include <assert.h>             // assert() (what else??)
#include "EMULIB.hpp"           // generic project wide declarations
#include "Interrupt.hpp"        // declarations for this module


CSimpleInterrupt::CSimpleInterrupt (INTERRUPT_MODE nMode)
{
  //++
  //   The parameter determines whether this simple interrupt system operates
  // in level sensitive (fEdgeTriggered=false) or rising edge triggered mode.
  //--
  m_nMode = nMode;  m_fRequested = false;
  m_lMasksUsed = m_lRequests = 0;
}

CSimpleInterrupt::IRQMASK CSimpleInterrupt::AllocateMask()
{
  //++
  //   Search m_lMasksUsed for a ZERO bit - this indicates an IRQ mask which
  // is not currently in use.  If there are none (m_lMasksUsed == 0xFFFFFFFF)
  // then there are no free interrupts left and we return zero instead.
  //--
  IRQMASK lMask = 1;
  for (uint16_t i = 0; i < MAXDEVICE; ++i) {
    if ((m_lMasksUsed & lMask) == 0) {
      m_lMasksUsed |= lMask;
    //fprintf(stderr,"ALLOCATE MASK m_lMasksUsed=0x%08x lMask=0x%08x\n", m_lMasksUsed, lMask);
      return lMask;
    }
    else
      lMask <<= 1;
  }
  return 0;
}

void CSimpleInterrupt::ReleaseMask (IRQMASK lMask)
{
  //++
  //   This routine will return to the free pool an IRQ mask allocated by
  // AllocateMask()...  There's no other garbage collection for IRQ masks,
  // so every device must be sure to call this function when its instance is
  // destroyed!
  //--
  //fprintf(stderr,"RELEASE MASK m_lMasksUsed=0x%08x, lMask=0x%08x\n", m_lMasksUsed, lMask);
  if ((m_lMasksUsed & lMask) == 0) {
  }
    
  assert((m_lMasksUsed & lMask) != 0);
  // Make sure this device doesn't have an outstanding request!
  Request(lMask, false);
  // And clear the corresponding MasksUsed bit ...
  m_lMasksUsed &= ~lMask;
}

void CSimpleInterrupt::Request (IRQMASK lMask, bool fInterrupt)
{
  //++
  //   This routine is called by a device emulator to set or clear the interrupt
  // request associated with that device.  That part is easy - the tricky bit
  // comes when we have to determine whether it's time to interrupt the CPU or
  // not...
  //--

  // First, set or clear the associated bit in m_lRequests...
  IRQMASK lOldRequests = m_lRequests;
  if (fInterrupt)
    SETBIT(m_lRequests, lMask);
  else
    CLRBIT(m_lRequests, lMask);

  //   Now we need to figure out whether the CPU should be interrupted.  If
  // we're in level sensitive mode then that's easy - if the request mask is
  // not zero, then the CPU should interrupt.
  //
  //   But if we're edge triggered then it's harder - we want to look for
  // m_lRequests changing from a zero to a non-zero value, and set the
  // m_fRequest flag when that happens.  Note that we don't care about the
  // specific device mask here - ALL devices are wire ORed together and
  // they're all the same as far as the IRQ flip flop is concerned.
  if (m_nMode == EDGE_TRIGGERED) {
    if ((lOldRequests == 0)  &&  (m_lRequests != 0)) m_fRequested = true;
  } else
    m_fRequested = (m_lRequests != 0);
}

void CSimpleInterrupt::AcknowledgeRequest()
{
  //++
  //   This method should be called by the CPU code when an interrupt
  // is granted, and what we do depends on whether we're edge triggered
  // or not.  In level sensitive mode, absolutely nothing happens - it's
  // up to each individual device to remove its interrupt request by
  // calling Request() with false for the second parameter.  This is the
  // way real hardware works with level sensitive interrupts - if the
  // device does not remove the interrupt request, then the CPU will just
  // continue to interrupt until it does.
  // 
  //   In edge triggered mode, however, this routine clears the m_fRequested
  // flag so that the CPU will not continue to interrupt until this flag gets
  // set again.  That happens on the rising edge of a request - i.e. when
  // m_lRequests changes from zero to non-zero.  This is all handled in the
  // Request() method.
  //--
  if (m_nMode == EDGE_TRIGGERED) {
    m_fRequested = false;  m_lRequests = 0;
  }
}

void CSimpleInterrupt::ClearInterrupt()
{
  //++
  //   This routine is a reset of the interrupt system - it will clear the
  // m_fRequested flag AND ALL CURRENT DEVICE INTERRUPT REQUESTS.  It does
  // NOT deallocate any assigned masks, however - those are still valid.
  //--
  m_lRequests = 0;  m_fRequested = false;
}


CPriorityInterrupt::CPriorityInterrupt (IRQLEVEL nLevels, CSimpleInterrupt::INTERRUPT_MODE nMode)
{
  //++
  // Allocate a CSimpleInterrupt object for each level ...
  //--
  assert(nLevels <= MAXLEVEL);
  m_nLevels = nLevels;
  memset(&m_pLevel, 0, sizeof(m_pLevel));
  for (IRQLEVEL i = 0;  i < nLevels; ++i)
    m_pLevel[i] = DBGNEW CSimpleInterrupt(nMode);
}

CPriorityInterrupt::~CPriorityInterrupt()
{
  //++
  //--
  assert((m_nLevels > 0)  &&  (m_nLevels <= MAXLEVEL));
  for (IRQLEVEL i = 0; i < m_nLevels; ++i)
    if (m_pLevel[i] != NULL) delete m_pLevel[i];
  memset(&m_pLevel, 0, sizeof(m_pLevel));
}

CPriorityInterrupt::IRQVECTOR CPriorityInterrupt::GetRequests() const
{
  //++
  //   This method returns a bit map of the priority levels with active
  // interrupt requests, starting with level 1 as bit 0 (the LSB) and
  // working up to m_nLevels-1.
  //--
  IRQVECTOR lIRQs = 0;
  for (IRQLEVEL i = 1;  i <= m_nLevels;  ++i) {
    if (IsRequestedAtLevel(i)) SETBIT(lIRQs, (1 << (i-1)));
  }
  return lIRQs;
}

void CPriorityInterrupt::ClearInterrupt()
{
  //++
  // Clear all interrupts on all levels ...
  //--
  for (IRQLEVEL i = 0;  i < m_nLevels;  ++i)
    m_pLevel[i]->ClearInterrupt();
}
