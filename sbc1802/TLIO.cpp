//++
// TLIO.cpp -> RCA style two level I/O for the COSMAC CPU
//
//   COPYRIGHT (C) 2015-2024 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
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
//   This module implements RCA style "two level I/O" for the COSMAC CPU.  The
// COSMAC only allows for seven (yes, 7!) unique I/O addresses and needless to
// say that's not enough for many applications.  RCA defines a system where port
// 1 is used as an I/O "group select" and the value written to this port defines
// the group of devices that the remaining 6 addresses will select.
// 
//   In this emulator, this is implemented by first installing the same instance
// of a CTLIO object (which implements the CDEVICE interface) into each of the
// seven COSMAC CPU I/O ports.  Actual I/O devices (e.g. UARTs, IDE disks,
// parallel ports, whatever) are then installed into that CTLIO object along with
// both their I/O port AND their group number.  The CTLIO object implements the
// port 1 group select register directly, and whenever the CPU accesses another
// I/O port 2 thru 7 the CTLIO object will hand off the call to the appropriate
// device for the current group.
// 
//   In the RCA standard the four COSMAC EF flags are also affected by the I/O
// group select.  That's also handled by this CTLIO object by installing it into
// all four COSMAC GetSense() slots and then installing the actual device sense
// routines into CTLIO.  This module also contains code for mapping flag outputs
// (Q in the case of the COSMAC) by the group select too, however none of the
// RCA hardware seems to use this feature.
// 
// IMPLEMENTATION
//   This object is essentially a "map of maps".  It implmements a GROUP_MAP
// unordered set for each group select code, and the GROUP_MAP entry points to a
// CDeviceMap object for that group.  Separate GROUP_MAPs are implemented for
// input devices, output devices, sense inputs, and flag outputs.
// 
// NOTES:
//   The I/O group select register, port 1, is 8 bits and can be both written or
// read back.  RCA actually defines a complicated selection system where the upper
// 4 bits are binary encoded but the lower 4 bits use a "one hot" encoding.  This
// module doesn't bother with that - we simply implement a unique 8 bit selection
// code.
// 
//   Interrupt requests are NOT affected in any way by the group select logic.
//
// REVISION HISTORY:
// 17-DEC-23  RLA   New file.
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
//#include "SBC1802.hpp"          // global declarations for this project
//#include "Interrupt.hpp"        // interrupt simulation logic
//#include "EventQueue.hpp"       // I/O device event queue simulation
#include "Memory.hpp"           // basic memory emulation declarations ...
#include "Device.hpp"           // basic I/O device emulation declarations ...
#include "DeviceMap.hpp"        // CDeviceMap declaration
#include "CPU.hpp"              // CCPU base class definitions
#include "COSMAC.hpp"           // COSMAC 1802 CPU specific declarations
#include "TLIO.hpp"             // declarations for this module



CTLIO::CTLIO (address_t nTLIOport, address_t nFirstPort, address_t nLastPort)
  : CDevice("TLIO", "TLIO", "Two Level I/O", INOUT, nFirstPort,  nLastPort),
    m_nTLIOport(LOBYTE(nTLIOport))
{
  //++
  //   The constructor initializes any member variables AND also creates the
  // default I/O group.  The default group always exists, even if there are
  // no devices in it or if TLIO is disabled ...
  //--
  m_GroupMap.clear();  EnableTLIO();
  AddGroup(DEFAULT_GROUP);  SelectGroup(DEFAULT_GROUP);
}

CTLIO::~CTLIO()
{
  //++
  // The destructor deletes all I/O groups (even the default!) ...
  //--
  DeleteAllGroups();  m_nGroupSelect = 0;  m_pCurrentGroup = NULL;
}

CTLIO::GROUP *CTLIO::FindGroup (uint8_t nGroup) const
{
  //++
  // Find the GROUP structure corresponding to the given group select code ...
  //--
  if (nGroup == 0) nGroup = DEFAULT_GROUP;
  CONST_MAP_ITERATOR it = m_GroupMap.find(nGroup);
  return (it == MapEnd()) ? NULL : it->second;
}

CTLIO::GROUP *CTLIO::AddGroup (uint8_t nGroup)
{
  //++
  //   If a GROUP structure already exists for the give select code, then
  // return a pointer to that.  If none exists, then create a new one.
  //--
  if (nGroup == 0) nGroup = DEFAULT_GROUP;
  GROUP *pGroup = FindGroup(nGroup);
  if (pGroup == NULL) {
    pGroup = DBGNEW GROUP;
    m_GroupMap.insert(pair<uint8_t, GROUP *>(nGroup, pGroup));
  }
  return pGroup;
}

void CTLIO::DeleteAllGroups()
{
  //++
  // Delete ALL GROUP structures!!
  //--
  CONST_MAP_ITERATOR it = MapBegin();
  while (it != MapEnd()) {
    GROUP *pGroup = it->second;
    pGroup->m_Inputs.RemoveAll();
    pGroup->m_Outputs.RemoveAll();
    pGroup->m_Senses.RemoveAll();
    pGroup->m_Flags.RemoveAll();
    delete pGroup;  m_GroupMap.erase(it);
    it = MapBegin();
  }
  m_GroupMap.clear();
}

void CTLIO::SelectGroup (uint8_t nGroup)
{
  //++
  //   Select the I/O group specified by nGroup, if two level I/O is enabled.
  // If TLIO is NOT enabled, then the default group is always selected,
  // regardless of the parameter passed.  
  // 
  //   Note that if no group is defined that corresponds to nGroup, then
  // m_pCurrentGroup will be NULL!  The DevRead(), DevWrite, et al routines
  // had better be prepared to deal with that.
  //--
  if (nGroup == m_nGroupSelect) return;
  m_nGroupSelect = nGroup;
  if (IsTLIOenabled()) {
    m_pCurrentGroup = FindGroup(nGroup);
    if (m_pCurrentGroup == NULL) {
      LOGF(DEBUG, "undefined I/O group 0x%02X selected", nGroup);
    } else {
      LOGF(DEBUG, "I/O group 0x%02X selected", nGroup);
    }
  } else
    m_pCurrentGroup = FindGroup(DEFAULT_GROUP);
}

bool CTLIO::InstallDevice (uint8_t nGroup, CDevice *pDevice)
{
  //++
  //   Install the specified I/O device into the specified I/O group.  We get
  // the device data direction (INPUT, OUTPUT, or INOUT) and the range of
  // device I/O port assignments, directly from the device itself.  This method
  // will return false if any other device currently occupies any I/O port used
  // by this device.  In that case, the new device is not installed and nothing
  // is changed.
  //--
  assert(pDevice != NULL);
  GROUP *pGroup = AddGroup(nGroup);

  // Here's the real work!
  if (!CDeviceMap::InstallDevice(pDevice, pGroup->m_Inputs, pGroup->m_Outputs)) return false;

  // The rest of this just prints a nice message!
  address_t nPort = pDevice->GetBasePort();
  address_t nPorts = pDevice->GetPortCount();
  if (nPorts <= 1) {
    LOGF(DEBUG, "%s attached to group 0x%02X port %d", pDevice->GetDescription(), nGroup, nPort);
  } else {
    LOGF(DEBUG, "%s attached to group 0x%02X ports %d..%d", pDevice->GetDescription(), nGroup, nPort, nPort+nPorts-1);
  }
  return true;
}

bool CTLIO::InstallSense (uint8_t nGroup, CDevice *pDevice, address_t nSense)
{
  //++
  //   Install the specified device as the specified sense input.  Unlike I/O
  // devices, this one is pretty simple.
  //--
  assert(pDevice != NULL);
  GROUP *pGroup = AddGroup(nGroup);
  if (pGroup->m_Senses.IsInstalled(nSense)) return false;
  pGroup->m_Senses.Install(pDevice, nSense, 1);
  LOGF(DEBUG, "%s attached to group 0x%02X sense input %d", pDevice->GetDescription(), nGroup, nSense);
  return true;
}

bool CTLIO::InstallFlag (uint8_t nGroup, CDevice *pDevice, address_t nFlag)
{
  //++
  //   Install the specified device as the specified flag output.  Like sense
  // inputs, this one is pretty simple.
  //--
  assert(pDevice != NULL);
  GROUP *pGroup = AddGroup(nGroup);
  if (pGroup->m_Flags.IsInstalled(nFlag)) return false;
  pGroup->m_Flags.Install(pDevice, nFlag, 1);
  LOGF(DEBUG, "%s attached to group 0x%02X flag output %d", pDevice->GetDescription(), nGroup, nFlag);
  return true;
}

bool CTLIO::RemoveDevice (uint8_t nGroup, CDevice *pDevice)
{
  //++
  //   This method will remove all instances of the specified device from ANY
  // of the input, output, sense or flag device lists.  It returns TRUE if at
  // least one instance is found and removed, and FALSE if this particular
  // device is never used.
  //--
  assert(pDevice != NULL);
  GROUP *pGroup = FindGroup(nGroup);
  if (pGroup == NULL) return false;

  // Search the input, output, sense and flag devices ...
  bool fFound = false;
  fFound |= pGroup->m_Inputs.Remove(pDevice);
  fFound |= pGroup->m_Outputs.Remove(pDevice);
  fFound |= pGroup->m_Senses.Remove(pDevice);
  fFound |= pGroup->m_Flags.Remove(pDevice);

  if (fFound)
    LOGF(DEBUG, "removing %s from group 0x%02x", pDevice->GetDescription(), nGroup);
  return fFound;
}

CDevice *CTLIO::FindDevice (const string sName) const
{
  //++
  //   Search thru all the devices we know about for one with the specified
  // name and, if we find one, return a pointer to its CDevice object.  Note
  // that we have to be sure to check for our own, TLIO, name too!
  //--
  if (CCmdArgKeyword::Match(sName, GetName())) return const_cast<CTLIO *> (this);
  for (CONST_MAP_ITERATOR it = MapBegin(); it != MapEnd(); ++it) {
    CDevice *pDevice;  GROUP *pGroup = it->second;
    if ((pDevice = pGroup->m_Inputs.Find(sName)) != NULL) return pDevice;
    if ((pDevice = pGroup->m_Outputs.Find(sName)) != NULL) return pDevice;
    if ((pDevice = pGroup->m_Senses.Find(sName)) != NULL) return pDevice;
    if ((pDevice = pGroup->m_Flags.Find(sName)) != NULL) return pDevice;
  }
  return NULL;
}

CDevice *CTLIO::FindInputDevice (uint8_t nGroup, address_t nPort) const
{
  //++
  //   Return a pointer to the CDevice object for the peripheral that belongs
  // to the specified group and I/O address...
  //--
  GROUP *pGroup = FindGroup(nGroup);
  if (pGroup == NULL) return NULL;
  return pGroup->m_Inputs.Find(nPort);
}

CDevice *CTLIO::FindOutputDevice (uint8_t nGroup, address_t nPort) const
{
  //++
  //   Return a pointer to the CDevice object for the peripheral that belongs
  // to the specified group and I/O address...
  //--
  GROUP *pGroup = FindGroup(nGroup);
  if (pGroup == NULL) return NULL;
  return pGroup->m_Outputs.Find(nPort);
}

uint8_t CTLIO::FindGroup (const CDevice *pDevice) const
{
  //++
  //   Find and return the I/O group that a particular device belongs to.  This
  // implementation is pretty slow, but this function is only used by the UI
  // to display the configuration so we don't really care much.  If we don't
  // find a match for this device, then return zero.
  //--
  for (CONST_MAP_ITERATOR it = MapBegin(); it != MapEnd(); ++it) {
    uint8_t nGroup = it->first;  GROUP *pGroup = it->second;
    if (pGroup->m_Inputs.IsInstalled(pDevice)) return nGroup;
    if (pGroup->m_Outputs.IsInstalled(pDevice)) return nGroup;
    if (pGroup->m_Senses.IsInstalled(pDevice)) return nGroup;
    if (pGroup->m_Flags.IsInstalled(pDevice)) return nGroup;
  }
  return 0;
}

CDevice *CTLIO::FindSenseDevice (uint8_t nGroup, address_t nSense) const
{
  //++
  //   Find the CDevice object for the sense device with the specified I/O
  // group and sense input number.
  //--
  GROUP *pGroup = FindGroup(nGroup);
  if (pGroup == NULL) return NULL;
  return pGroup->m_Senses.Find(nSense);
}

CDevice *CTLIO::FindFlagDevice  (uint8_t nGroup, address_t nFlag) const
{
  //++
  //    Find the CDevice object for the flag device with the specified I/O
  // group and flag ouptut number.
  //--
  GROUP *pGroup = FindGroup(nGroup);
  if (pGroup == NULL) return NULL;
  return pGroup->m_Flags.Find(nFlag);
}

void CTLIO::ClearDevice()
{
  //++
  //   When our ClearDevice method is called, we need to pass the call along
  // to all the devices we own.  The tricky bit there is that we don't want
  // to call ClearDevice() more than once for any individual device.  This
  // is particularly a problem for devices which may be installed as both
  // input and output.  We deal with this in a simple, but brute force,
  // method.
  // 
  //   Note that this assumes that any give device is only installed in one
  // I/O group.  That's a reasonable assumption, but it's not really required
  // so take it with a grain of salt.
  // 
  //   Oh, and a ClearDevice() also resets the group select to the default.
  //--
  for (CONST_MAP_ITERATOR it = MapBegin(); it != MapEnd(); ++it) {
    GROUP *pGroup = it->second;
    CDeviceMap::ClearAllOnce(pGroup->m_Inputs, pGroup->m_Outputs, pGroup->m_Senses, pGroup->m_Flags);
  }
}

void CTLIO::DevWrite (address_t nPort, word_t bData)
{
  //++
  //   This routine is called by the CPU for any output to an I/O port that's
  // TLIO mapped (and usually that's ALL ports!).  If we're writing to the TLIO
  // port itself then update the selected group.  Otherwise write to the I/O
  // DeviceMap selected by the current group.  If nothing matches the current
  // group select, then treat it as a no-op.
  //--
  if (nPort == m_nTLIOport)
    SelectGroup(bData);
  else if (m_pCurrentGroup != NULL)
    m_pCurrentGroup->m_Outputs.DevWrite(nPort, bData);
}

word_t CTLIO::DevRead (address_t nPort)
{
  //++
  //   Reading from a TLIO port is pretty much the same as writing.  Note that
  // in this implementation it is possible to read back the group select
  // register (although not all hardware implementations allow that!).  Also
  // an input operation when no defined group is selected always returns 0xFF.
  //--
  if (nPort == m_nTLIOport)
    return IsTLIOenabled() ? m_nGroupSelect : 0xFF;
  else if (m_pCurrentGroup != NULL)
    return m_pCurrentGroup->m_Inputs.DevRead(nPort);
  else
    return 0xFF;
}

uint1_t CTLIO::GetSense (address_t nSense, uint1_t bDefault)
{
  //++
  //   Return the state of the specified sense input in the current I/O group.
  // If no I/O group is selected, then return the default for this sense.
  // The COSMAC CPU allows you to specify the default, unconnected, level for
  // each of the four EF inputs.  Turns out that this is important to some
  // software.
  //--
  if (m_pCurrentGroup != NULL)
    return m_pCurrentGroup->m_Senses.GetSense(nSense, bDefault);
  else
    return bDefault;
}

void CTLIO::SetFlag (address_t nFlag, uint1_t bData)
{
  //++
  //   Set the state of the specified flag ouptut in the current I/O group.
  // If the I/O group is undefined, then this is a no-op.  The COSMAC, of
  // course, has only one flag output - Q - and that's not usually affected
  // by the RCA two level I/O scheme (but it could be!).
  //--
  if (m_pCurrentGroup != NULL)
    m_pCurrentGroup->m_Flags.SetFlag(nFlag, bData);
}

void CTLIO::ShowDevice (ostringstream& ofs) const
{
  //++
  // Dump the device state for the UI command "SHOW DEVICE" ...
  //--
  if (IsTLIOenabled()) {
    ofs << FormatString("Group select port = %d, current group = 0x%02X\n", m_nTLIOport, m_nGroupSelect);
    for (CONST_MAP_ITERATOR it = MapBegin(); it != MapEnd(); ++it) {
      uint8_t nGroup = it->first;  GROUP* pGroup = it->second;
      ofs << FormatString("Group 0x%02X - %d input, %d output, %d sense, %d flag devices\n", nGroup,
        pGroup->m_Inputs.GetCount(), pGroup->m_Outputs.GetCount(),
        pGroup->m_Senses.GetCount(), pGroup->m_Flags.GetCount());
    }
  } else
    ofs << FormatString("Two level I/O disabled\n");
}

