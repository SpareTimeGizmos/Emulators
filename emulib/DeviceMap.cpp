//++
// DeviceMap.Cpp -> Port or Memory address to Device Mapping implementation
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
//   A CDeviceMap object is a container for a set of CDevice objects.  Each
// device has a range of port or memory addresses associated with it and a
// pointer to the corresponding CDevice object.  This class provides various
// methods for managing and using this collection.
//
//   Two separate STL collection objects are actually used here.  First there's
// a map, which maps a single port address to a device pointer.  There's one
// entry in this collection for each port address used, BUT remember that several
// addresses may map to the same device.  Because it's sometimes handy to have
// a list of the unique devices without regard to how many individual addresses
// map to that same device, there's a second collection, an unordered map.
// This collection contains exactly one device pointer entry for each device
// that's mapped on this system.
// 
// REVISION HISTORY:
//  4-JUL-22  RLA   Split out of CCPU ...
//  5-JUL-22  RLA   Revise to use map rather than vector ...
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include <map>                  // C++ std::map template
#include <unordered_set>        // C++ std::unordered_set template
#include "EMULIB.hpp"           // emulator library definitions
#include "LogFile.hpp"          // emulator library message logging facility
#include "MemoryTypes.h"        // address_t and word_t data types
#include "Device.hpp"           // basic I/O device emulation declarations ...
#include "DeviceMap.hpp"        // declarations for this module


CDevice *CDeviceMap::Find (address_t nPort) const
{
  //++
  //   Return a pointer to the device mapped to the specified port, or NULL
  // if the port is currently unmapped...
  //--
  CONST_MAP_ITERATOR it = m_Map.find(nPort);
  return (it == MapEnd()) ? NULL : it->second;
}

int16_t CDeviceMap::Find (const CDevice *pDevice) const
{
  //++
  //   Search the address map for ANY port that points to the specified device.
  // If it finds one then it returns it, and if no match can be found it will
  // return -1.  Note that the map is NOT SORTED and the ports will not be
  // returned in any particular order!
  //--
  for (CONST_MAP_ITERATOR it = MapBegin();  it != MapEnd();  ++it)
    if (it->second == pDevice) return it->first;
  return -1;
}

CDevice *CDeviceMap::Find (const string sName) const
{
  //++
  // Search the map for a device with this name ...
  //--
  for (CONST_SET_ITERATOR it = SetBegin();  it != SetEnd();  ++it) {
    if ((*it)->GetName() == sName) return (*it);
  }
  return NULL;
}

CDevice *CDeviceMap::Find (const class CSimpleInterrupt *pInterrupt) const
{
  //++
  // Search the map for a device attached to the specified interrupt channel ...
  //--
  for (CONST_SET_ITERATOR it = SetBegin();  it != SetEnd();  ++it) {
    if ((*it)->GetInterruptA() == pInterrupt) return (*it);
    if ((*it)->GetInterruptB() == pInterrupt) return (*it);
  }
  return NULL;
}

bool CDeviceMap::IsInstalled (address_t nPort, address_t cPorts) const
{
  //++
  // Return true if ANY device is installed in the specified range ...
  //--
  for (address_t n = nPort; n < (nPort+cPorts); ++n)
    if (Find(n) != NULL) return true;
  return false;
}

bool CDeviceMap::IsInstalled (const CDevice *pDevice) const
{
  //++
  // Return TRUE if at least one mapping to pDevice exists ...
  //--
  return (m_Set.find(const_cast<CDevice *> (pDevice)) != SetEnd());
}

bool CDeviceMap::Install (CDevice *pDevice, address_t nPort, address_t cPorts)
{
  //++
  // Install the device for the specified range of I/O addresses ...
  //--
  assert(pDevice != NULL);
  if (IsInstalled(nPort, cPorts)) return false;
  for (address_t n = nPort; n < (nPort+cPorts); ++n)
    m_Map.insert(pair<address_t, CDevice*>(n, pDevice));
  if (m_Set.find(pDevice) == SetEnd()) m_Set.insert(pDevice);
  return true;
}

bool CDeviceMap::Install (CDevice *pDevice)
{
  //++
  // Install the specified I/O device using its default address ...
  //--
  assert(pDevice != NULL);
  address_t nPort = pDevice->GetBasePort();
  address_t nPorts = pDevice->GetPortCount();
  return Install (pDevice, nPort, nPorts);
}

bool CDeviceMap::InstallDevice (CDevice *pDevice, CDeviceMap &Inputs, CDeviceMap &Outputs)
{
  //++
  //   Install the specified I/O device into either the input map, the output
  // map, or both.  We can get the device data direction (INPUT, OUTPUT, or
  // INOUT) and the range of device I/O port assignments, directly from the
  // device itself. This method will return false if any other device currently
  // occupies any I/O port used by this device.  In that case, the new device
  // is not installed and nothing is changed.
  //--
  assert(pDevice != NULL);
  address_t nPort = pDevice->GetBasePort();
  address_t nPorts = pDevice->GetPortCount();

  //  Be careful to avoid a case where an INOUT device gets installed in one
  // set but not both because of address conflicts!
  if (pDevice->IsInOut()) {
    if (Inputs.IsInstalled(nPort, nPorts)) return false;
    if (Outputs.IsInstalled(nPort, nPorts)) return false;
  }
  // Install this device into the input, output or both, collections ...
  if (pDevice->IsInput()) Inputs.Install(pDevice, nPort, nPorts);
  if (pDevice->IsOutput()) Outputs.Install(pDevice, nPort, nPorts);
  return true;
}

bool CDeviceMap::Remove (address_t nPort)
{
  //++
  //   Remove the device mapping at the specifed address.  If this is the last
  // reference to that device in the entire map, then also remove the device
  // from the set.  It returns TRUE if successful and FALSE if nothing is 
  // currently mapped to the port given.
  // 
  //   IMPORTANT - this does NOT delete the device object.  That's the caller's
  // problem.  We didn't make it; we're not cleaning it up :)
  //--
  CDevice *pDevice = Find(nPort);
  if (pDevice == NULL) return false;
  m_Map.erase(nPort);
  //   WARNING! Don't be tempted to call IsInstalled() here, because that will
  // search the _set_.  What we want to know is whether there are any more map
  // entries that point to pDevice!
  if (Find(pDevice) == -1) m_Set.erase(pDevice);
  return true;
}

bool CDeviceMap::Remove (CDevice *pDevice)
{
  //++
  //   Remove all references to the specified device, and then delete the
  // device!  Return FALSE if no instance of the device exists at all ...
  // 
  //   The plan is to simply look for any port address mapped to this device,
  // delete that address, and repeat until we find no more mappings.  This
  // isn't very efficient, but this has the advantage of confining all the
  // removal logic to the Remove(nPort) method ...
  //--
  int32_t nPort = Find(pDevice);
  if (nPort == -1) return false;
  while (nPort != -1) {
    Remove(ADDRESS(nPort));  nPort = Find(pDevice);
  }
  return true;
}

void CDeviceMap::RemoveAll()
{
  //++
  // Remove (and delete!) ALL installed devices...
  //--
  CONST_SET_ITERATOR it = SetBegin();
  while (it != SetEnd()) {
    Remove((*it));  it = SetBegin();
  }
}

void CDeviceMap::ClearAll() const
{
  //++
  // Iterate thru all the I/O devices and call their ClearDevice() method.
  //--
  for (CONST_SET_ITERATOR it = SetBegin(); it != SetEnd(); ++it)
    (*it)->ClearDevice();
}

void CDeviceMap::ClearAllOnce (CDeviceMap &Inputs, CDeviceMap &Outputs)
{
  //++
  //   This method will call the ClearDevice() method for all devices in the
  // Inputs group, and then it will clear all the devices in the Outputs map,
  // BUT it will insure that devices in both maps are cleared only once!
  // This algorithm isn't super efficient, but ClearDevice() isn't called
  // very often and this is good enough.
  //--
  Inputs.ClearAll();

  //   Now clear all output devices that aren't also input devices.  This
  // is pretty brute force and slow, but it's good enough for this job.
  CDeviceMap::CONST_SET_ITERATOR it;
  for (it = Outputs.SetBegin(); it != Outputs.SetEnd(); ++it) {
    CDevice *pDevice = (*it);
    if (!Inputs.IsInstalled(pDevice)) pDevice->ClearDevice();
  }
}

void CDeviceMap::ClearAllOnce (CDeviceMap &Inputs, CDeviceMap &Outputs, CDeviceMap &Senses)
{
  //++
  //   This is the same as the previous ClearAllOnce, but this time incldoing
  // sense devices as well as inputs and outputs.  Once again, ClearDevice() is
  // called only once, even if a given device is in multiple sets.
  //--
  ClearAllOnce(Inputs, Outputs);
  //   Clear all sense devices that aren't already cleared as input or output
  // devices.  This is even slower, but still good enough.
  CDeviceMap::CONST_SET_ITERATOR it;
  for (it = Senses.SetBegin();  it != Senses.SetEnd();  ++it) {
    CDevice *pDevice = (*it);
    if (   !Inputs.IsInstalled(pDevice)
        && !Outputs.IsInstalled(pDevice)) pDevice->ClearDevice();
  }
}

void CDeviceMap::ClearAllOnce (CDeviceMap &Inputs, CDeviceMap &Outputs, CDeviceMap &Senses, CDeviceMap &Flags)
{
  //++
  // And still the same, but this time for Input, Output, Sense AND Flag devices...
  //--
  ClearAllOnce(Inputs, Outputs, Senses);
  CDeviceMap::CONST_SET_ITERATOR it;
  for (it = Flags.SetBegin();  it != Flags.SetEnd();  ++it) {
    CDevice *pDevice = (*it);
    if (   !Inputs.IsInstalled(pDevice)
        && !Outputs.IsInstalled(pDevice)
        && !Senses.IsInstalled(pDevice)) pDevice->ClearDevice();
  }
}
