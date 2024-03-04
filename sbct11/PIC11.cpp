//++
// PIC11.hpp -> DCT11 Priority Interrupt Controller definitions
//
//   COPYRIGHT (C) 2015-2024 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
// 
// LICENSE:
//    This file is part of the SBCT11 emulator project.  SBCT11 is free
// software; you may redistribute it and/or modify it under the terms of
// the GNU Affero General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any
// later version.
//
//    SBCT11 is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License
// for more details.  You should have received a copy of the GNU Affero General
// Public License along with SBCT11.  If not, see http://www.gnu.org/licenses/.
//
// DESCRIPTION:
//   The CPIC11 object emulates the functions of the SBCT11 INTERRUPT and
// ACKNOWLEDGE GALs. Together these handle all the interrupts, or at least all
// the external interrupts, used in the SBCT11.  Real PDP11s use a vetored
// interrupt system, and each peripheral device is expected to supply its own
// vector to the CPU.  The DCT11 is able to do that too, but providing a unique
// interrupt vector for every UART, PPI, IDE, etc chip adds a lot of extra logic
// to the system.  
// 
//   Fortunately the DCT11 also provides an easier way.  The chip can support up
// to 15 external interrupts and can internally generate the vector for each one.
// Some of the vectors correspond to standard PDP11 devices, like the console
// terminal, and others are unique to the T11.  Logic external to the DCT11 is
// expected to supply a 4 bit binary encoded value, called "CP", which encodes
// the highest priority interrupt request.  Each request has a priority, called
// the "BR" ("Bus Request") level.  The DCT11 will interrupt only if the current
// processor priority in the PSW is less than BR level of the request.
// 
//   When the DCT11 does interrupt it will output an acknowledge code which
// corresponds to the CP code of the interrupt.  External logic can decode this
// and use it to generate individual interrupt acknowledge signals for each
// peripheral.  Interrupts on the PDP11 are typically, but not always, edge
// triggered and the acknowledge is a critical step in clearing this request
// flip flop.
// 
//   In the SBCT11 specifically, the INTERRUPT GAL takes interrupt request inputs
// from eight different peripheral chips.  The GAl selects the highest priority
// request, generates the correct CP code for that input, and gives it to the
// DCT11.  The ACKNOWLEDGE GAL captures the interrupt acknowledgement from the
// T11, decodes the CP code, and resets the corresponding interrupt request.
//
//   Note that when no external interrupt is requested, the DCT11 CP inputs are
// zero to indicate "no request".
// 
// IMPLEMENTATION NOTES
//   This class implements the SBCT11/DCT11 scheme as an array of fifteen
// CSimpleInterrupt objects, one for each CP code.  Each CSimpleInterrupt object
// may have one or more devices attached to it, for example the CDC319 object
// for SLU0 is attached to the level 6 (receive) and level 7 (transmit) 
// CSimpleInterrupt objects.  The DCT11 emulation calls our RequestedCP() method
// to return the CP code of the highest priority active CSimpleInterrupt.
// 
//   The CSimpleInterrupt objects are normally configured as edge triggered.
// When the DCT11 takes the interrupt, it will call the AcknowledgeCP() method
// to reset the edge triggered flip flop associated with that object.  In the
// case of the SBCT11 a few interrupts, notably the IDE, are NOT edge triggered.
// For these the caller must explicitly change the mode of the CSimpleInterrupt
// object, and that case the AcknowledgeCP() call will be ignored.
// 
// HALT AND POWER FAIL
//   The DCT11 has two additional interrupt request inputs - HALT and POWERFAIL.
// These are non-maskable interrupts, so they don't care about the current CPU
// PSW priority level, and they are independent of the CPx interrupt inputs.
// HALT has the highest priority, then POWERFAIL, and lastly any CP requesst.
// The vector for POWERFAIL is 24 and this interrupt is not used in the standard
// SBCT11, however the signal is available on the expansion connector and could
// be wired up to something someday.
// 
//   The HALT input doesn't halt the CPU at all and instead acts like any other
// trap or interrupt, however HALT doesn't have a vector.  Instead a HALT trap
// always loades the PC with the restart address, as configured by the startup
// mode, plus 2.  Halt always loads the PSW is always loaded with 340 (priority
// level 7).  The SBCT11 uses HALT for NXM traps, the HALT toggle switch, and
// for SLU0 console break.
// 
//   Neither HALT nor POWERFAIL are handled by this module.  The CDCT11 class
// has explicit HaltRequest() and PowerFailRequest() methods that can be called
// directly to invoke those interrupts.
//    
// REVISION HISTORY:
// 18-JUN-22  RLA   New file.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include <cstring>              // needed for memset()
#include "EMULIB.hpp"           // emulator library definitions
#include "LogFile.hpp"          // emulator library message logging facility
#include "ConsoleWindow.hpp"    // console window functions
#include "MemoryTypes.h"        // address_t and word_t data types
#include "Interrupt.hpp"        // generic priority interrupt controller
#include "CPU.hpp"              // CPU definitions
#include "Device.hpp"           // generic device definitions
#include "PIC11.hpp"           // declarations for this module
#include "DCT11.hpp"            // DEC DCT11 CPU definitions

//++
//   These two tables give the priority level, equivalent to the PDP11 BR
// (bus request) level, and the interrupt vector associated with each of the
// 15 DCT11 IRQ/CP (coded priority) inputs.  Remember that zero is not used
// here (it means "no interrupt request"!).
//--
const uint8_t CPIC11::g_abPriority[CPIC11::IRQLEVELS] = {
                    CDCT11::PSW_PRI4, CDCT11::PSW_PRI4, CDCT11::PSW_PRI4, // CP1..3
  CDCT11::PSW_PRI5, CDCT11::PSW_PRI5, CDCT11::PSW_PRI5, CDCT11::PSW_PRI5, // CP4..7
  CDCT11::PSW_PRI6, CDCT11::PSW_PRI6, CDCT11::PSW_PRI6, CDCT11::PSW_PRI6, // CP8..11
  CDCT11::PSW_PRI7, CDCT11::PSW_PRI7, CDCT11::PSW_PRI7, CDCT11::PSW_PRI7  // CP12..15
};
const address_t CPIC11::g_awVectors[CPIC11::IRQLEVELS] = {
        0070, 0064, 0060, 0134, 0130, 0124, 0120,   // CP1..7
  0114, 0110, 0104, 0100, 0154, 0150, 0144, 0140    // CP8..15
};

CPIC11::CPIC11()
{
  //++
  // The constructor creates all 15 CSimpleInterrupt objects ...
  //--
  for (IRQ_t i = 0;  i < IRQLEVELS;  ++i)
    m_pLevel[i] = DBGNEW CSimpleInterrupt(CSimpleInterrupt::EDGE_TRIGGERED);
  m_nLastIRQ = 0;
}

CPIC11::~CPIC11()
{
  //++
  // And the destructor deletes all 15 interrupt objects ...
  //--
  for (IRQ_t i = 0;  i < IRQLEVELS;  ++i)  delete m_pLevel[i];
  memset(m_pLevel, 0, sizeof(m_pLevel));
}

void CPIC11::ClearInterrupt()
{
  //++
  // Clear all interrupt requests ...
  //--
  for (IRQ_t i = 1; i <= IRQLEVELS; ++i)  GetLevel(i)->ClearInterrupt();
  m_nLastIRQ = 0;
}

CPIC11::IRQ_t CPIC11::FindRequest (uint8_t bPSW)
{
  //++
  //   Find and return the highest priority request with a priority that's
  // greater than the specified CPU priority level.  Note that if you don't
  // care about the CPU and just want the highest priority requests, pass
  // zero as the PSW!
  //--
  bPSW &= CDCT11::PSW_PRIO;
  for (IRQ_t i = IRQLEVELS;  i > 0;  --i) {
    if (GetPriority(i) <= bPSW) return (m_nLastIRQ = 0);
    if (GetLevel(i)->IsRequested()) return (m_nLastIRQ = i);
  }
  return (m_nLastIRQ = 0);
}

void CPIC11::AcknowledgeRequest (IRQ_t nIRQ)
{
  //++
  // Acknowledge the interrupt request on the specified CP ...
  // nCP == 0 -> acknowledge highest level
  //--
  if (nIRQ == 0) nIRQ = m_nLastIRQ;
  if (nIRQ == 0) return;
  GetLevel(nIRQ)->AcknowledgeRequest();
}

CPIC11::IRQ_t CPIC11::FindInterrupt (CSimpleInterrupt const *pInterrupt) const
{
  //++
  //   Search all the interrupt channels for one matching the specified
  // CSimpleInterrupt channel.  This is used by the UI to discover the PIC
  // channel and DCT11 CP level associated with a particular CDevice object.
  // It's not especially fast (it just does a simple linear search) and
  // shouldn't be used to any time critical application.
  // 
  //   If no match can be found, then zero is returned.
  //--
  for (IRQ_t i = IRQLEVELS; i > 0; --i) {
    if (pInterrupt == GetLevel(i)) return i;
  }
  return 0;
}
