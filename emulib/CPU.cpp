//++
//CPU.cpp - generic microprocessor emulation
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
//   This module contains some basic methods that are shared by all CPU
// emulations.  This includes 
// 
//   * Code to track the simulated execution time (with help from the individual
//   CPU emulators).
// 
//   * An event queue, for scheduling upcoming I/O events in the future.
// 
//   * Collections of input, output, sense and flag devices for processors with
//   I/O mapped devices (memory mapped I/O is handled by the memory object!).
// 
//   * Generic routines for accessing internal processor state and registers.
// 
//   * Generic routines for common errors, like illegal opcode or unimplemented I/O.
//
//   * Probably more ...
// 
//TODO
// Add a way to catch ^E on the console even if no UART or SERIAL is attached?
//
// REVISION HISTORY:
// 17-JAN-20  RLA  New file.
//  7-FEB-20  RLA  Add Sense and Flag devices.
// 15-FEB-20  RLA  Move clearing the interrupts and event queue to ClearAllDevices()
//  4-JUL-22  RLA  Remove breakpoint stuff (it's handled by memory now!)
// 22-Aug-22  RLA  Constructor should call ClearCPU(), not MasterClear()!
// 14-Jun-23  RLA  MasterClear() should clear the event queue first!
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include "EMULIB.hpp"           // emulator library definitions
#include "LogFile.hpp"          // emulator library message logging facility
#include "Interrupt.hpp"        // interrupt simulation logic
#include "EventQueue.hpp"       // I/O device event queue simulation
#include "MemoryTypes.h"        // address_t and word_t data types
#include "Memory.hpp"           // basic memory emulation declarations ...
#include "CPU.hpp"              // CCPU base class definitions
#include "Device.hpp"           // basic I/O device emulation declarations ...


CCPU::CCPU (CMemory *pMemory, CEventQueue *pEvents, CInterrupt *pInterrupt)
{
  //++
  // CCPU constructor - initialize everything...
  //--
  assert ((pMemory != NULL)  &&  (pEvents != NULL));
  m_pInterrupt = pInterrupt;
  m_pMemory = pMemory;
  m_pEvents = pEvents;
  m_fStopOnIllegalIO = false;
  m_fStopOnIllegalOpcode = true;
  m_nLastPC = 0;
  ClearCPU();
}

CCPU::~CCPU()
{
  //++
  //   Delete all linked devices before the CPU goes away.  Note that this 
  // DOES NOT delete the memory - that's up to the caller.  Maybe we should?
  //--
  RemoveAllDevices();
}

void CCPU::MasterClear()
{
  //++
  //   Clear (reset) the CPU and all I/O devices!  This is the equivalent of a
  // power on or asserting the RESET hardware signal and clears way more than
  // just the internal state of the CPU.  Most derived CPU implementations
  // just need to implement ClearCPU() and don't need to implement this one!
  //--
  m_pEvents->ClearEvents();
  ClearAllDevices();
  ClearCPU();
}

void CCPU::ClearCPU()
{
  //++
  //   This clears the internal state of the CPU, however it does NOT clear
  // any events, interrupts, external devices, or the simulated time!
  //
  //   DON'T CLEAR EVENTS OR INTERRUPTS HERE!  That's because ClearAllDevices()
  // has already been called, and some of those devices want to schedule events
  // or interrupts!
  //--
  m_nStopCode = STOP_NONE;
}

void CCPU::ClearAllDevices()
{
  //++
  //   Loop thru all the I/O devices and call their Clear() method too.
  // We take a little bit of care to make sure that the Clear() method is
  // called only once for every device no matter how many times it appears
  // in the address space, but that's not 100% guaranteed!
  //--

  // Clear the event queue and all device interrupt requessts...
  m_pEvents->CancelAllEvents();
  if (m_pInterrupt != NULL) m_pInterrupt->ClearInterrupt();

  // Clear all the devices, exactly once!
  CDeviceMap::ClearAllOnce(m_InputDevices, m_OutputDevices, m_SenseDevices, m_FlagDevices);
}

bool CCPU::InstallDevice (CDevice *pDevice)
{
  //++
  //   Install the specified I/O device into this CPU.  We can get the device
  // data direction (INPUT, OUTPUT, or INOUT) and the range of device I/O port
  // assignments, diectly from the device itself.  This method will return
  // false if any other device currently occupies any I/O port used by this
  // device.  In that case, the new device is not installed and nothing is
  // changed.
  //--
  assert(pDevice != NULL);
  if (!CDeviceMap::InstallDevice(pDevice, m_InputDevices, m_OutputDevices)) return false;

  // The rest of this just prints a nice message!
  address_t nPort = pDevice->GetBasePort();
  address_t nPorts = pDevice->GetPortCount();
  if (nPorts <= 1) {
    LOGF(DEBUG, "%s attached to port %d", pDevice->GetDescription(), nPort);
  } else {
    LOGF(DEBUG, "%s attached to ports %d..%d", pDevice->GetDescription(), nPort, nPort+nPorts-1);
  }
  return true;
}

bool CCPU::InstallSense (CDevice *pDevice, address_t nSense)
{
  //++
  //   Install the specified device as the specified sense input.  Unlike I/O
  // devices, this is pretty simple and there are no port ranges to worry about.
  //--
  assert(pDevice != NULL);
  if (m_SenseDevices.IsInstalled(nSense)) return false;
  m_SenseDevices.Install(pDevice, nSense, 1);
  LOGF(DEBUG, "%s attached to external sense input %d", pDevice->GetDescription(), nSense);
  return true;
}

bool CCPU::InstallFlag (CDevice *pDevice, address_t nFlag)
{
  //++
  // Same as above, except install a flag output device ...
  //--
  assert(pDevice != NULL);
  if (m_FlagDevices.IsInstalled(nFlag)) return false;
  m_FlagDevices.Install(pDevice, nFlag, 1);
  LOGF(DEBUG, "%s attached to external flag output %d", pDevice->GetDescription(), nFlag);
  return true;
}

bool CCPU::RemoveDevice (CDevice *pDevice)
{
  //++
  //   This method will remove all instances of the specified device from ANY
  // of the input, output, sense or flag device lists.  It returns TRUE if at
  // least one instance is found and removed, and FALSE if this particular
  // device is never used.
  //
  //   It also DELETES THE DEVICE object, regardless of whether it is found or
  // not.  The devices "belong" to the CPU after they're installed, and we're
  // responsible for deleting them when we're not using them.
  //--
  assert(pDevice != NULL);
  bool fFound = false;
  LOGF(DEBUG, "removing %s", pDevice->GetDescription());

  // Search the input, output, sense and flag devices ...
  fFound |= m_InputDevices.Remove(pDevice);
  fFound |= m_OutputDevices.Remove(pDevice);
  fFound |= m_SenseDevices.Remove(pDevice);
  fFound |= m_FlagDevices.Remove(pDevice);
  return fFound;
}

void CCPU::RemoveAllDevices()
{
  //++
  // Remove (and delete!) ALL installed devices...
  //--
  m_InputDevices.RemoveAll();
  m_OutputDevices.RemoveAll();
  m_SenseDevices.RemoveAll();
  m_FlagDevices.RemoveAll();
}

CDevice *CCPU::FindDevice (const string sName) const
{
  //++
  //   Search ALL devices - input, output, flag and sense - for one matching
  // the name given and return its pointer.  If no match can be found, return
  // NULL.
  //--
  CDevice *pDevice;
  if ((pDevice = m_InputDevices.Find(sName))  != NULL) return pDevice;
  if ((pDevice = m_OutputDevices.Find(sName)) != NULL) return pDevice;
  if ((pDevice = m_SenseDevices.Find(sName))  != NULL) return pDevice;
  if ((pDevice = m_FlagDevices.Find(sName))   != NULL) return pDevice;
  return NULL;
}

CDevice *CCPU::FindDevice (const class CSimpleInterrupt* pInterrupt) const
{
  //++
  //   Search all input or output devices, but not flags or sense, for one
  // attached to the given interrupt channel and return its pointer.  If no
  // match is found, return NULL ...
  //--
  CDevice *pDevice;
  if ((pDevice = m_InputDevices.Find(pInterrupt))  != NULL) return pDevice;
  if ((pDevice = m_OutputDevices.Find(pInterrupt)) != NULL) return pDevice;
  return NULL;
}
