//++
// DeviceMap.hpp -> Port or Memory address to Device Mapping class
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
// device has a range of port or memory addresses associated with it, and this
// class provides various methods for managing and using this collection.
//
// REVISION HISTORY:
//  4-JUL-22  RLA   Split out of CCPU ...
// 20-DEC-23  RLA   Add bDefault parametter to GetSense()...
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ string functions
#include <map>                  // C++ std::map template
#include <unordered_set>        // C++ std::unordered_set template
#include "MemoryTypes.h"        // address_t and word_t data types
#include "Device.hpp"           // CDevice class delcarations
using std::string;              // ...
using std::iterator;            // ...
using std::map;                 // ...
using std::pair;                // ...
using std::unordered_set;       // ...


class CDeviceMap {
  //++
  // Map ports or memory addresses to devices ...
  //--

  // Constructors and destructors...
public:
  CDeviceMap() {m_Map.clear();  m_Set.clear();}
  virtual ~CDeviceMap() {RemoveAll();}
private:
  // Disallow copy and assignments!
  CDeviceMap (const CDeviceMap&) = delete;
  CDeviceMap& operator= (CDeviceMap const &) = delete;

  // Special types ...
public:
  typedef map<address_t, CDevice *> DEVICE_MAP;
  typedef unordered_set<CDevice *> DEVICE_SET;
  typedef DEVICE_MAP::iterator MAP_ITERATOR;
  typedef DEVICE_MAP::const_iterator CONST_MAP_ITERATOR;
  typedef DEVICE_SET::iterator SET_ITERATOR;
  typedef DEVICE_SET::const_iterator CONST_SET_ITERATOR;

  // CDeviceMap iterators ...
public:
  inline MAP_ITERATOR       MapBegin() {return m_Map.begin();}
  inline MAP_ITERATOR       MapEnd()   {return m_Map.end();}
  inline CONST_MAP_ITERATOR MapBegin() const {return m_Map.begin();}
  inline CONST_MAP_ITERATOR MapEnd()   const {return m_Map.end();}
  inline SET_ITERATOR       SetBegin() {return m_Set.begin();}
  inline SET_ITERATOR       SetEnd()   {return m_Set.end();}
  inline CONST_SET_ITERATOR SetBegin() const {return m_Set.begin();}
  inline CONST_SET_ITERATOR SetEnd()   const {return m_Set.end();}

  // CDeviceMap properties ...
public:
  // Find a device by name or port address ...
  CDevice *Find (const string sName) const;
  CDevice *Find (address_t nPort) const;
  int16_t Find (const CDevice *pDevice) const;
  CDevice *Find (const class CSimpleInterrupt *pInterrupt) const;
  // Test whether any device is installed at the given address range ...
  bool IsInstalled (address_t nPort, address_t cPorts=1) const;
  // Test whether a particular device is already installed ...
  bool IsInstalled (const CDevice *pDevice) const;
  // Return the total number of devices installed ...
  address_t GetCount() const {return (address_t) m_Map.size();}
  address_t GetUniqueCount() const {return (address_t) m_Set.size();}

  // CDeviceMap device methods ...
public:
  // Add a device to the map ...
  bool Install (CDevice *pDevice, address_t nPort, address_t cPorts=1);
  bool Install (CDevice *pDevice);
  // Remove a device from the map ...
  bool Remove (address_t nPort);
  bool Remove  (CDevice *pDevice);
  // Remove ALL devices from the map ...
  void RemoveAll();

  // CDeviceMap sense and flag methods ...
public:
  // Call the DevRead() or DevWrite() methods for the specified device ...
  //   If no such device exists, then for DevRead() return 0xFF and for
  // DevWrite() just do nothing ...
  word_t DevRead (address_t nPort, word_t wDefault=WORD_MAX) const
    {CDevice *p = Find(nPort);  return (p == NULL) ? wDefault : p->DevRead(nPort);}
  void DevWrite (address_t nPort, word_t bData) const
    {CDevice *p = Find(nPort);  if (p != NULL) p->DevWrite(nPort, bData);}
  // Same, but for sense and flag I/Os ...
  uint1_t GetSense (address_t nSense, uint1_t bDefault=0) const
    {CDevice *p = Find(nSense);  return (p == NULL) ? bDefault : p->GetSense(nSense, bDefault);}
  void SetFlag (address_t nFlag, uint1_t bData) const
    {CDevice *p = Find(nFlag);  if (p != NULL) p->SetFlag(nFlag, bData);}
  // Call the ClearDevice() method for all devices in the map ...
  void ClearAll() const;
  // Clear all devices exactly once!
  static void ClearAllOnce(CDeviceMap &Inputs, CDeviceMap &Outputs);
  static void ClearAllOnce(CDeviceMap &Inputs, CDeviceMap &Outputs, CDeviceMap &Senses);
  static void ClearAllOnce(CDeviceMap &Inputs, CDeviceMap &Outputs, CDeviceMap &Senses, CDeviceMap &Flags);
  // Install a device in the input map, output or both as appropriate ...
  static bool InstallDevice (CDevice *pDevice, CDeviceMap &Inputs, CDeviceMap &Ouptuts);



  // CDevicemap private methods ...
private:
//  void Insert (address_t nPort, CDevice *pDevice)
//    {m_Devices.insert(pair<address_t, CDevice*>(nPort, pDevice));}

  // Private member data...
protected:
  DEVICE_MAP  m_Map;          // mapping of port addresses to device pointers
  DEVICE_SET  m_Set;          // set of all unique devices used
};
