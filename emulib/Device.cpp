//++
// Device.cpp -> Implementation of the CDevice base class
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
//   CDevice is the base class for all device emulation.  It defines the
// standard methods that all devices support - process input and output,
// receive events, clear (initialize), etc.  Note that this class contains
// many virtual functions but is not abstract - that means objects of type
// CDevice can be instanciated if you like.  This will produce a device
// emulator which does absolutely nothing - never interrupts, returns zero
// for all input and ignores all output - but sometimes that's useful ...
//
// DATA FLOW DIRECTION
//   Devices may be input only, output only, or both (aka "INOUT").  This
// designation doesn't necessarily refer to the device itself, but rather to
// the access to the device's registers.  A printer, for example, is an INOUT
// device because it has both control and data registers which can be written,
// and a status register which can be read.  Devices which are input only have
// registers which can only be read (i.e. they only respond to INP instructions
// on microprocessors that have explicit I/O instructions, or are read only
// memory for processors with memory mapped I/O).  Likewise, output only
// devices only respond to OUT instructions or are write only memory locations.
//
//   The distinction between input and output devices is particularly relevant
// for processors that have separate IN and OUT instructions, like the 1802 or
// the 8080.  An input device responds to the IN instruction, an output device
// responds to OUT, and an inout device responds to both.  For CPUs that use
// memory mapped I/O, an input device would be a read only memory location, an
// output device a write only location, and inout is R/W.
//
//   It's entirely possible to have two different devices with the same address,
// one for input and one for output.  In the ELF2K, for example, the LEDs and
// switches are both device 4 - LEDs are output and the switches are input.
//
// SENSE AND FLAGS
//   In addition to registers which can be read or written, devices may also
// have a flag input and/or a sense output.  Many micros have input pins that
// can be tested directly by the firmware (e.g. EFx pins on the 1802, or SENSE
// A/B on the SC/MP, or SID on the 8085), and these are often wired to
// peripheral devices.  Likewise, many micros have generic output pins that
// can be controlled by the firmware (e.g. Q on the 1802, FLAG 0, 1, or 2 on
// the SC/MP, or SOD on the 8085) and these are likewise often wired up to
// peripheral devices.  The GetSense() and SetFlag() functions are used to 
// emulate these connections.  Note the direction of data flow here - the sense
// output connections are device outputs and microprocessor inputs, and
// likewise flag inputs are device inputs and microprocessor outputs.  
//
//   BTW, one common use for the flag input and sense output is a "bit banged"
// software serial terminal interface, as implemented by the SoftwareSerial
// module.
//
// INTERRUPTS
//   To support interrupts, after the CDevice object is constructed you must
// call one of the AttachInterruptX() methods to connect it to an interrupt
// controller.  All devices assume a CSimpleInterrupt, "wire-OR",  interrupt
// control, however there are several multi-channel priority interrupt systems
// implemented as a collection of CSimpleInterrupt objects.  Either works with
// this interface.
// 
//   To request an interrupt, or to remove an interrupt request, a device can
// call one of the RequestInterruptX() methods.  Up to two interrupts per
// device, arbitrarily called the "A" and "B" interrupt channels, are supported.
// This gives us names like RequestInterruptA() and AttachInterruptB(), etc.
// In addition, RequestInterrupt() and AttachInterrupt() are defined for
// compatibility with devices that use only one interrupt.  These arbitrarily
// use channel A.
// 
// REVISION HISTORY:
// 12-AUG-19  RLA   New file.
//  7-FEB-20  RLA   Add flag input and sense output
// 20-JUN-22  RLA   Add the "unit" number to GetSense() and SetFlag()
// 15-JUL-22  RLA   Add second interrupt channel.
//                  Create a .cpp file for some of the implementation.
//--
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include <string>               // C++ std::string class, et al ...
#include <iostream>             // C++ style output for LOGS() ...
#include <sstream>              // C++ std::stringstream, et al ...
#include "EMULIB.hpp"           // emulator library definitions
#include "CommandParser.hpp"    // needed for type KEYWORD
#include "MemoryTypes.h"        // address_t and word_t data types
#include "EventQueue.hpp"       // CEventQueue declarations
#include "Interrupt.hpp"        // CInterrupt definitions
#include "Device.hpp"           // declarations for this module


CDevice::CDevice (const char *pszName, const char *pszType, const char *pszDescription, DEVICE_MODE nMode, address_t nPort, address_t nPorts, CEventQueue *pEvents)
{
  //++
  //   The constructor specifies all the basic device properties, as follows -
  //
  //   Name should be a short alphanumeric identifier that names the device
  //        for SET, SHOW, EXAMINE and DEPOSIT commands (e.g. "POST",
  //        "SWITCHES", "SLU0", etc).
  // 
  //   Type is a generic type of this device (e.g. "UART", "CDP1854", etc)
  //
  //   Description is an arbitrary ASCII string that's used to describe the
  //        device in show commands (e.g. "Console Serial Line") ...
  //
  //   nMode is the data flow direction of the device - INPUT, OUTPUT or INOUT.
  //        See the discussion above for a description of what that means.
  //
  //   nPort is the base I/O address of the device.  For CPUs with memory
  //        mapped I/O, this is a memory address.  For processors with INP or
  //        OUT instructions, this is a port number.
  //
  //   nPorts is the number of consecutive memory or port addresses that this
  //        device requires.  For example, an 8250 UART with 8 internal registers
  //        would normally use 8 addresses.  Something like an LED POST display
  //        probably uses only one.
  //
  //   pEvents is the CEventQueue object to use for scheduling future events.
  //--
  m_pszName = pszName;  m_pszType = pszType;  m_pszDescription = pszDescription;
  m_nMode = nMode;  m_nBasePort = nPort;  m_nPortCount = nPorts;
  m_pEvents = pEvents;
  m_lIRQmaskA = m_lIRQmaskB = 0;
  m_pInterruptA = m_pInterruptB = NULL;
}

CDevice::~CDevice()
{
  //++
  //   Release all interrupt assignments and cancel any event queue entries
  // when this device is deleted!
  //--
  ReleaseInterrupt();
  //  We need a way to cancel ALL events for this device regardless of the
  // lParam, but CEventQueue currently doesn't have one!!
  //CancelAllEvent()
}

void CDevice::AttachInterruptA (CSimpleInterrupt *pInterrupt)
{
  //++
  // Attach interrupt channel A to the interrupt controller ..
  //--
  assert(pInterrupt != NULL);
  m_pInterruptA = pInterrupt;
  m_lIRQmaskA = pInterrupt->AllocateMask();
  assert(m_lIRQmaskA != 0);
}

void CDevice::AttachInterruptB (CSimpleInterrupt *pInterrupt)
{
  //++
  // Attach interrupt channel B to the interrupt controller ..
  //--
  assert(pInterrupt != NULL);
  m_pInterruptB = pInterrupt;
  m_lIRQmaskB = pInterrupt->AllocateMask();
  assert(m_lIRQmaskB != 0);
}

void CDevice::AttachInterrupt (CSimpleInterrupt *pInterruptA, CSimpleInterrupt *pInterruptB)
{
  //++
  // Attach both channels with one call ...
  //--
  if (pInterruptA != NULL) AttachInterruptA(pInterruptA);
  if (pInterruptB != NULL) AttachInterruptB(pInterruptB);
}

void CDevice::ReleaseInterruptA()
{
  //++
  //   Release the interrupt assignment for channel A.  This is normally used
  // when this device is to be disconnected or destroyed, and the only way
  // to reconnect the interrupt is to call AttachInterruptX() again.
  //--
  if (m_pInterruptA != NULL) {
    m_pInterruptA->Request(m_lIRQmaskA);
    m_pInterruptA->ReleaseMask(m_lIRQmaskA);
    m_pInterruptA = NULL;  m_lIRQmaskA = 0;
  }
}

void CDevice::ReleaseInterruptB()
{
  //++
  // Ditto, but for interrupt channel B ...
  //--
  if (m_pInterruptB != NULL) {
    m_pInterruptB->Request(m_lIRQmaskB);
    m_pInterruptB->ReleaseMask(m_lIRQmaskB);
    m_pInterruptB = NULL;  m_lIRQmaskB = 0;
  }
}

void CDevice::ReleaseInterrupt()
{
  //++
  // Realease all interrupt assignments (if any!) ...
  //--
  ReleaseInterruptA();
  ReleaseInterruptB();
}

void CDevice::RequestInterruptA (bool fInterrupt)
{
  //++
  //   Request (if fInterrupt is true) an interrupt on channel A, or cancel
  // any existing request (if fInterrupt is false).  If InterruptA is not
  // attached, then silently do nothing ...
  //--
  if (m_pInterruptA != NULL)
    m_pInterruptA->Request(m_lIRQmaskA, fInterrupt);
}

void CDevice::RequestInterruptB (bool fInterrupt)
{
  //++
  // Same, but for interrupt channel B ...
  //--
  if (m_pInterruptB != NULL)
    m_pInterruptB->Request(m_lIRQmaskB, fInterrupt);
}

void CDevice::RequestInterrupt (bool fInterruptA, bool fInterruptB)
{
  //++
  // Update the request status for both interrupt channels at once.
  //--
  RequestInterruptA(fInterruptA);
  RequestInterruptB(fInterruptB);
}

bool CDevice::IsInterruptRequestedA() const
{
  //++
  // Return true if an interrupt is currently requested on channel A ...
  //--
  if (m_pInterruptA == NULL) return false;
  return m_pInterruptA->IsRequested(m_lIRQmaskA);
}

bool CDevice::IsInterruptRequestedB() const
{
  //++
  // Return true if an interrupt is currently requested on channel B ...
  //--
  if (m_pInterruptB == NULL) return false;
  return m_pInterruptB->IsRequested(m_lIRQmaskB);
}

void CDevice::ScheduleEvent (intptr_t lParam, uint64_t llDelay)
{
  //++
  //   Schedule a future event for this device.  When the specified simulated
  // time is reached, the Event() method will be called with lParam, and a
  // pointer to this device, as parameters.
  //--
  assert(m_pEvents != NULL);
  m_pEvents->Schedule(this, lParam, llDelay);
}

void CDevice::CancelEvent (intptr_t lParam)  
{
  //++
  // Cancel a scheduled event for this device ...
  //--
  assert(m_pEvents != NULL);
  m_pEvents->Cancel(this, lParam);
}

bool CDevice::IsEventPending(intptr_t lParam) const
{
  //++
  // Return true if an event with a matching lParam is currently scheduled.
  //--
  assert(m_pEvents != NULL);
  return m_pEvents->IsPending(this, lParam);
}

void CDevice::ShowDevice (ostringstream &ofs) const
{
  //++
  //   Dump the internal state of this device to an output stream.  This is
  // used for debugging and would normally be overridden by each derived
  // subclass.  This one isn't particularly useful.
  //--
  ofs << "NOT IMPLEMENTED!"; 
}
