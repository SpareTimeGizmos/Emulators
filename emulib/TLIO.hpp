//++
// TLIO.hpp -> RCA style two level I/O for the COSMAC CPU
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
//   This class emulates the RCA standard "two level" I/O scheme for the CDP1802.
//
// REVISION HISTORY:
// 17-DEC-23  RLA   New file.
//  6-NOV-24  RLA   Add Find(pInterrupt) ...
//--
#pragma once
#include <stdint.h>
#include <string>               // C++ std::string class, et al ...
#include <map>                  // C++ std::map template
#include "MemoryTypes.h"        // address_t and word_t data types
#include "CPU.hpp"              // CPU definitions
#include "Device.hpp"           // generic device definitions
#include "DeviceMap.hpp"        // CDeviceMap declaration
#include "COSMAC.hpp"           // COSMAC 1802 CPU specific declarations
using std::string;              // ...
using std::iterator;            // ...
using std::map;                 // ...
using std::pair;                // ...

class CTLIO : public CDevice {
  //++
  // Two level I/O for the COSMAC CPU ...
  //--

  // TLIO magic constants ...
public:
  enum {
    DEFAULT_GROUP     = 1,      // default group at power up
  };

public:
  // Constructor and destructor...
  CTLIO (address_t nTLIOport, address_t nFirstPort=1, address_t nLastPort=CCOSMAC::MAXDEVICE);
  virtual ~CTLIO();
private:
  // Disallow copy and assignments!
  CTLIO (const CTLIO &) = delete;
  CTLIO& operator= (CTLIO const &) = delete;

  // Special types ...
private:
  // Each entry in the GROUP_MAP is one of these ...
  struct _GROUP {
    CDeviceMap      m_Inputs;   // input  (CPU <- device) devices by address
    CDeviceMap      m_Outputs;  // output (CPU -> device)    "    "      "
    CDeviceMap      m_Senses;   // devices connected to sense inputs
    CDeviceMap      m_Flags;    //    "      "   "    " flag  outputs
  };
  typedef struct _GROUP GROUP;

  // The GROUP_MAP ordered map ...
  typedef map<uint8_t, GROUP *> GROUP_MAP;
  typedef GROUP_MAP::iterator MAP_ITERATOR;
  typedef GROUP_MAP::const_iterator CONST_MAP_ITERATOR;

  // GROUP_MAP iterators ...
public:
  inline MAP_ITERATOR       MapBegin() {return m_GroupMap.begin();}
  inline MAP_ITERATOR       MapEnd()   {return m_GroupMap.end();}
  inline CONST_MAP_ITERATOR MapBegin() const {return m_GroupMap.begin();}
  inline CONST_MAP_ITERATOR MapEnd()   const {return m_GroupMap.end();}

  // Public two level I/O properties
public:
  //   Enable or disable two level I/O.  When TLIO is disabled, only
  // the default I/O group is accessible!
  bool IsTLIOenabled() const {return m_fTLIOenabled;}
  void EnableTLIO (bool fEnable=true) {m_fTLIOenabled = fEnable;}

  // Add or remove devices, senses or flags from the TLIO group ...
public:
  // Install or remvove I/O devices ...
  bool InstallDevice (uint8_t nGroup, CDevice *pDevice);
  bool RemoveDevice (uint8_t nGroup, CDevice *pDevice);
  // Install sense or flag devices ...
  bool InstallSense (uint8_t nGroup, CDevice *pDevice, address_t nSense=0);
  bool InstallFlag (uint8_t nGroup, CDevice *pDevice, address_t nFlag=0);
  // Find any device with a matching name ...
  CDevice *FindDevice (const string sName) const;
  CDevice *FindInputDevice (uint8_t nGroup, address_t nPort) const;
  CDevice *FindOutputDevice (uint8_t nGroup, address_t nPort) const;
  // Find any input or output device with a matching IRQ ...
  CDevice *FindDevice (const class CSimpleInterrupt *pInterrupt) const;
  // Find the I/O group this device belongs to ...
  uint8_t FindGroup (const CDevice *pDevice) const;
  // Find sense or flag devices from group and address ...
  CDevice *FindSenseDevice (uint8_t nGroup, address_t nSense=0) const;
  CDevice *FindFlagDevice  (uint8_t nGroup, address_t nFlag=0) const;

  // CDevice methods implemented by two level I/O ...
public:
  // Clear (i.e. a hardware reset) this device ...
  virtual void ClearDevice() override;
  // Read or write from/to the device ...
  virtual word_t DevRead (address_t nPort) override;
  virtual void DevWrite (address_t nPort, word_t bData) override;
  // Called by the CPU to test a sense input or set a flag output ...
  virtual uint1_t GetSense (address_t nSense, uint1_t bDefault=0) override;
  virtual void SetFlag (address_t nFlag, uint1_t bData) override;
  // Dump the device state for the user ...
  virtual void ShowDevice (ostringstream &ofs) const override;

  // Private methods ...
private:
  // Find the GROUP for a particular code, or NULL if none ...
  GROUP *FindGroup (uint8_t nGroup) const;
  // Add or delete group(s) ...
  GROUP *AddGroup (uint8_t nGroup);
  void DeleteAllGroups();
  // Select the current group ...
  void SelectGroup (uint8_t nGroup);

  // Private member data...
protected:
  //   Note that the TLIO port can only be set by the constructor, and
  // never changes there after.  Hence the "const" here ...
  const uint8_t m_nTLIOport;    // two level I/O port
  // Other local data ...
  bool          m_fTLIOenabled; // TRUE if two level I/O is enabled
  uint8_t       m_nGroupSelect; // contents of the group select register
  GROUP        *m_pCurrentGroup;// pointer to the current GROUP structure
  GROUP_MAP     m_GroupMap;     // mapping of group numbers to I/O devices
};
