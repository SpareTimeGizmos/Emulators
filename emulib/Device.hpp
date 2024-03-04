//++
// Device.hpp -> definitions for the CDevice base class
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
// receive events, clear (initialize), etc.  
//
// REVISION HISTORY:
// 12-AUG-19  RLA   New file.
//  7-FEB-20  RLA   Add flag input and sense output
// 20-JUN-22  RLA   Add the "unit" number to GetSense() and SetFlag()
// 15-JUL-22  RLA   Add second interrupt channel.
//                  Create a .cpp file for some of the implementation.
// 20-DEC-23  RLA   Add bDefault parameter to GetSense()
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ std::string class, et al ...
#include <iostream>             // C++ style output for LOGS() ...
#include <sstream>              // C++ std::stringstream, et al ...
#include "CommandParser.hpp"    // needed for type KEYWORD
#include "MemoryTypes.h"        // address_t and word_t data types
#include "EventQueue.hpp"       // CEventQueue declarations
#include "Interrupt.hpp"        // CInterrupt definitions
using std::string;              // ...
using std::ostringstream;       // ...


class CDevice : public CEventHandler {
  //++
  //   This enum determines the "MODE" of this device - input, output or both
  // (aka "inout").  Be careful, though, because this might not be exactly what
  // you expect.  In this instance a "device" means an I/O address or register
  // accessible by the CPU, and the direction refers to whether this register is
  // write only, read only or read/writable by the CPU.  A real world device,
  // like a printer, a UART, or a disk drive, will typically have several of
  // these registers or I/O addresses.  
  //--
public:
  enum _DEVICE_MODES {
    INPUT  = 1,               // data flows from device -> CPU
    OUTPUT = 2,               // data flows from CPU -> device
    INOUT  = 3                // both ways
  };
  typedef enum _DEVICE_MODES DEVICE_MODE;

  // Constructor and destructor...
public:
  CDevice (const char *pszName, const char *pszType, const char *pszDescription, DEVICE_MODE nMode, address_t nPort, address_t nPorts=1, CEventQueue *pEvents=NULL);
  virtual ~CDevice();
private:
  // Disallow copy and assignments!
  CDevice (const CDevice&) = delete;
  CDevice& operator= (CDevice const &) = delete;

  // Device properties ...
public:
  // Return the name, type and/or description ...
  const char *GetName() const {return m_pszName;}
  const char *GetType() const {return m_pszType;}
  const char *GetDescription() const {return m_pszDescription;}
  // Test whether the device is input, output, or both ...
  bool IsInput() const {return (m_nMode == INPUT) || (m_nMode == INOUT);}
  bool IsOutput() const {return (m_nMode == OUTPUT) || (m_nMode == INOUT);}
  bool IsInOut() const {return (m_nMode == INOUT);}
  // Return the port address and/or range for this device ...
  address_t GetBasePort() const {return m_nBasePort;}
  void SetBasePort (address_t nBase) {m_nBasePort = nBase;}
  address_t GetPortCount() const {return m_nPortCount;}
  // Set or get the event queue associated with this device ...
  inline void SetEvents (CEventQueue *pEvents) {assert(m_pEvents != NULL);  m_pEvents = pEvents;}
  inline CEventQueue *GetEvents() const {assert(m_pEvents != NULL);  return m_pEvents;}

  // Device methods used by the CPU to interact with this device ...
public:
  // Clear (i.e. a hardware reset) this device ...
  virtual void ClearDevice() {};
  //   The next two methods are used by both CPUs with I/O mapped devices (e.g.
  // explicit IN and OUT instructions) and CPUs with memory mapped devices to
  // read and write device registers.
  virtual word_t DevRead (address_t nPort) {return WORD_MAX;}
  virtual void DevWrite (address_t nPort, word_t bData) {};
  //   And this method is used by CPUs like the PDP-8 where it's the device
  // that determines what I/O operation is to be performed!
  virtual bool DevIOT (word_t wIOT, word_t &wAC, word_t &wPC) {return false;}
  // Dump the device state for the user ...
  virtual void ShowDevice (ostringstream &ofs) const;

  // CEventHandler methods used by the event queue ...
public:
  virtual void EventCallback (intptr_t lParam) override {};
  virtual const char *EventName() const override {return GetName();}

  // Sense and flag support ...
public:
  // Called by the CPU when a flag output changes state ...
  virtual void SetFlag (address_t nFlag, uint1_t bData) {};
  // Called by the CPU to test the state of a sense input ...
  virtual uint1_t GetSense (address_t nSense, uint1_t bDefault=0) {return bDefault;}

  // Device interrupt support ...
  //   Note that we support two interrupt channels per device - "A" and "B".
  // We also define procedure names without the A/B suffix for backward
  // compatability with devices that use only one interrupt...
public:
  // Attach this device to an interrupt controller ...
  virtual void AttachInterruptA (CSimpleInterrupt *pInterrupt);
  virtual void AttachInterruptB (CSimpleInterrupt *pInterrupt);
  virtual void AttachInterrupt (CSimpleInterrupt *pInterruptA, CSimpleInterrupt *pInterruptB);
  inline void AttachInterrupt (CSimpleInterrupt *pInterrupt) {AttachInterruptA(pInterrupt);}
  // Called by the device emulation to update the interrupt request state ...
  virtual void RequestInterruptA (bool fInterrupt=true);
  virtual void RequestInterruptB (bool fInterrupt=true);
  virtual void RequestInterrupt (bool fInterruptA, bool fInterruptB);
  inline void RequestInterrupt (bool fInterrupt=true) {RequestInterruptA(fInterrupt);}
  // Return true if an interrupt is requested ...
  virtual bool IsInterruptRequestedA() const;
  virtual bool IsInterruptRequestedB() const;
  inline bool IsInterruptRequested() const {return IsInterruptRequestedA();}
  // Return the pointer to the attached CSimpleInterrupt object ...
  virtual CSimpleInterrupt *GetInterruptA() const {return m_pInterruptA;}
  virtual CSimpleInterrupt *GetInterruptB() const {return m_pInterruptB;}
  inline CSimpleInterrupt *GetInterrupt()  const {return GetInterruptA();}
  // Called when the device is destroyed to release the interrupt assignment ...
  virtual void ReleaseInterruptA();
  virtual void ReleaseInterruptB();
  virtual void ReleaseInterrupt();

  // Event queue functions ...
protected:
  void ScheduleEvent (intptr_t lParam, uint64_t llDelay);
  void CancelEvent (intptr_t lParam);
  bool IsEventPending (intptr_t lParam) const;

  // Private member data...
protected:
  const char   *m_pszName;        // name of this device instance (e.g. "SLU0")
  const char   *m_pszType;        // generic type of this device (e.g. "UART")
  const char   *m_pszDescription; // long form description of the device
  DEVICE_MODE   m_nMode;          // device mode - input, output or inout
  address_t     m_nBasePort;      // base I/O port address
  address_t     m_nPortCount;     // number of port addresses required
  CEventQueue  *m_pEvents;        // event queue for scheduling events
  CSimpleInterrupt          *m_pInterruptA; // interrupt controller emulation #1
  CSimpleInterrupt::IRQMASK  m_lIRQmaskA;   // device interrupt mask #1
  CSimpleInterrupt          *m_pInterruptB; // ... and #2
  CSimpleInterrupt::IRQMASK  m_lIRQmaskB;   // ...
};
