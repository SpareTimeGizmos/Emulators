//++
// CPU.hpp -> CCPU (generic microprocessor) class
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
//   CCPU is an abstract class intended to be used as the base class for all
// microprocessor CPU emulations.  CCPU is to microprocessors what the CDevice
// class is to peripherals, however unlike CDevice, CCPU is an incomplete class
// and can never be instanciated directly.  It's only used as a base class for
// real CPU emulation ...
//
// WARNING:
//   This code assumes, for better or worse, that uint8_t variables are really
// exactly 8 bits and that uint16_t variables are exactly 16 bits.  This has
// important consequences for the handling of overflows and wrap around for
// 8 and 16 bit arithmetic.  Strictly speaking this isn't portable, but it
// shouldn't be an issue unless you port this code to some wierd, non-byte
// oriented, architecture.
//
// REVISION HISTORY:
// 12-AUG-19  RLA   New file.
// 21-JAN-20  RLA   Add devices, software serial, and much more.
// 22-JUN-22  RLA   Change m_Interrupt to m_pInterrupt.  Make the interrupt
//                    control a constructor parameter and make it optional.
//  5-JUL-22  RLA   Change to use CDeviceMap class ...
// 18-JUL-22  RLA   Change NSTOMS to return uint64_t, not uint32_t!
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ string functions
#include "Interrupt.hpp"        // interrupt simulation logic
#include "EventQueue.hpp"       // I/O device event queue simulation
#include "MemoryTypes.h"        // address_t and word_t data types
#include "DeviceMap.hpp"        // CDeviceMap class
#include "Memory.hpp"           // basic memory emulation declarations ...
#include "CommandParser.hpp"    // needed for type KEYWORD
using std::string;              // ...


//++
//   Nanoseconds are used exclusively as the timing unit internally, but the
// user may prefer to use more convenient units.  These macros convert from
// various units (e.g. milliseconds, characters per second, etc to nanoseconds
// and back.  Note that there is the universal special case that 0 for one
// unit gives zero for the converted unit, regardless!
//--
#define USTONS(x)  ((uint64_t) (x) * 1000ULL)
#define NSTOUS(x)  ((uint64_t) (x) / 1000ULL)
#define MSTONS(x)  ((uint64_t) (x) * 1000000ULL)
#define NSTOMS(x)  ((uint64_t) (x) / 1000000ULL)
#define HZTONS(x)  ((uint64_t) ((x) == 0 ? 0 : (1000000000ULL / (x))))
#define NSTOHZ(x)  ((uint32_t) ((x) == 0 ? 0 : (1000000000ULL / (x))))
#define CPSTONS(x) HZTONS(x)
#define NSTOCPS(x) NSTOHZ(x)


class CCPU {
  //++
  // Generic CPU emulator class ...
  //--
 
  // Return codes from the CPU Run() method...
public:
  enum _STOP_CODES {
    STOP_NONE,  	// (used internally while emulation is running)
    STOP_FINISHED,	// instruction count was reached
    STOP_ILLEGAL_IO,	// an illegal address or instruction was found
    STOP_ILLEGAL_OPCODE,//  "    "    opcode           "    "
    STOP_HALT,		// a halt instruction was executed
    STOP_ENDLESS_LOOP,  // an endless loop was entered
    STOP_BREAKPOINT,    // breakpoint reached
    STOP_BREAK		// Break() was called
  };
  typedef enum _STOP_CODES STOP_CODE;

  // Constructors and destructors...
public:
  CCPU (CMemory *pMemory, CEventQueue *pEvents, CInterrupt *pInterrupt=NULL);
  virtual ~CCPU();
private:
  // Disallow copy and assignments!
  CCPU (const CCPU&) = delete;
  CCPU& operator= (CCPU const &) = delete;

  // CCPU properties ...
public:
  // Set the break on unimplemented I/O or instruction flags ...
  inline void StopOnIllegalIO (bool fStop=true) {m_fStopOnIllegalIO = fStop;}
  inline void StopOnIllegalOpcode (bool fStop=true) {m_fStopOnIllegalOpcode = fStop;}
  inline bool IsStopOnIllegalIO() const {return m_fStopOnIllegalIO;}
  inline bool IsStopOnIllegalOpcode() const {return m_fStopOnIllegalOpcode;}
  // Get the address of the instruction that was just executed ...
  inline address_t GetLastPC() const {return m_nLastPC;}
  // Get or set the address of the next instruction to be executed ...
  virtual address_t GetPC() const {return 0;}
  virtual void SetPC (address_t a) {};
  // Get the CPU's simulated crystal frequency in MHz ...
  virtual uint32_t GetCrystalFrequency() const {return 0;}
  // Get a constant string for the CPU name, type or options ...
  virtual const char *GetDescription() const {return "unknown";}
  virtual const char *GetName() const {return "none";}

  // Emulation control...
public:
  // Reset the CPU ...
  virtual void MasterClear();
  virtual void ClearCPU();
  // Simulate one or more instructions ...
  virtual STOP_CODE Run (uint32_t nCount=0) = 0;
  // Interrupt the simulation gracefully ...
  virtual void Break (STOP_CODE nStop=STOP_BREAK) {m_nStopCode = nStop;}
  // Error traps...
  void IllegalOpcode() {if (m_fStopOnIllegalOpcode) Break(STOP_ILLEGAL_OPCODE);}
  void UnimplementedIO() {if (m_fStopOnIllegalIO) Break(STOP_ILLEGAL_IO);}

  // Read or write CPU registers ... 
public:
  virtual const CCmdArgKeyword::KEYWORD *GetRegisterNames() const {return NULL;}
  virtual unsigned GetRegisterSize (cpureg_t nReg) const {return 1;}
  virtual uint16_t GetRegister (cpureg_t nReg) const = 0;
  virtual void SetRegister (cpureg_t nReg, uint16_t nVal) = 0;

  // Device functions ...
public:
  // Install or remvove I/O devices ...
  virtual bool InstallDevice (CDevice *pDevice);
  virtual bool RemoveDevice (CDevice *pDevice);
  // Search for devices ...
  virtual CDevice *FindInputDevice  (address_t nPort)   const {return m_InputDevices.Find(nPort);}
  virtual CDevice *FindOutputDevice (address_t nPort)   const {return m_OutputDevices.Find(nPort);}
  // Find any device with a matching name ...
  virtual CDevice *FindDevice (const string sName) const;
  // Find any input or output device with a matching IRQ ...
  virtual CDevice *FindDevice (const class CSimpleInterrupt *pInterrupt) const;
  // Clear (simulate a hardware reset) for all devices ...
  virtual void ClearAllDevices();
  // Read or write data from or to a device ...
  virtual word_t ReadInput (address_t nPort) {return m_InputDevices.DevRead(nPort);}
  virtual void WriteOutput (address_t nPort, word_t bData) {return m_OutputDevices.DevWrite(nPort, bData);}
  // Delete all attached I/O devices (including flags and sense!) ...
  virtual void RemoveAllDevices();

  //   Sense inputs and flag outputs.  Note that these are handled by CDevice
  // objects, the same as any other device.  These can be a single bit I/O
  // device (e.g. a bit banged software serial terminal), or they may be status
  // or control flag for devices that are also mapped to I/O ports.
public:
  // Install sense or flag devices ...
  virtual bool InstallSense (CDevice *pDevice, address_t nSense=0);
  virtual bool InstallFlag (CDevice *pDevice, address_t nFlag=0);
  // Search for sense or flag devices ...
  virtual CDevice *GetSenseDevice (address_t nSense=0) const {return m_SenseDevices.Find(nSense);}
  virtual CDevice *GetFlagDevice  (address_t nFlag=0)  const {return m_FlagDevices.Find(nFlag);}
  virtual bool IsSenseInstalled   (address_t nSense=0) const {return m_SenseDevices.IsInstalled(nSense);}
  virtual bool IsFlagInstalled    (address_t nFlag=0)  const {return m_FlagDevices.IsInstalled(nFlag);}
  virtual int16_t FindSense (const CDevice *pDevice) const {return m_SenseDevices.Find(pDevice);}
  virtual int16_t FindFlag (const CDevice *pDevice) const {return m_FlagDevices.Find(pDevice);}
  virtual const char *GetSenseName (address_t nSense=0) const {return "unknown";}
  virtual const char *GetFlagName (address_t nFlag=0) const {return "unknown";}
  // Sense inputs or update flag ouputs ...
  virtual uint1_t GetSense (address_t nSense=0, uint1_t bDefault=0) {return m_SenseDevices.GetSense(nSense, bDefault);}
  virtual void SetFlag (address_t nFlag, uint1_t bData) {return m_FlagDevices.SetFlag(nFlag, bData);}

  // Event Queue functions ...
public:
  // Return the simulated elapsed time ...
  uint64_t ElapsedTime() const {return m_pEvents->CurrentTime();}
  void AddTime (uint64_t llTime) {m_pEvents->AddTime(llTime);}
  // Process outstanding events ...
  void DoEvents() {m_pEvents->DoEvents();}

  // Local methods ...
protected:

  // Private member data...
protected:
  bool      m_fStopOnIllegalIO;     // break simulation on unimplemented I/Os
  bool      m_fStopOnIllegalOpcode; //   "      "   "    "   "     "     opcodes
  STOP_CODE       m_nStopCode;      // reason for stopping the emulator
  address_t       m_nLastPC;        // address of instruction that was just executed
  CMemory        *m_pMemory;        // main memory for this CPU
  CEventQueue    *m_pEvents;        // "to do" list of upcoming events
  CInterrupt     *m_pInterrupt;     // interrupt control logic (if any!)
  CDeviceMap      m_InputDevices;   // input  (CPU <- device) devices by address
  CDeviceMap      m_OutputDevices;  // output (CPU -> device)    "    "      "
  CDeviceMap      m_SenseDevices;   // devices connected to sense inputs
  CDeviceMap      m_FlagDevices;    //    "      "   "    " flag  outputs
};
