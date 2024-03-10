//++
// Memory.hpp -> CMemory (mapped virtual memory) interface
//               CGenericMemory (generic memory emulation) class
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
//   The CMemory abstract class defines a generic interface to memory that can
// be used by the CPU and device emulations.  CMemory is an abstract class and
// cannot be instantiated by itself.  The CGenericMemory class defines an
// implementation of a generic memory model that can actually be instantiated
// and which will suffice for most emulations.
// 
//   The reason for the distinction between the two is to allow for systems with
// memory mapping hardware.  The memory mapping hardware would implement the
// CMemory interface, which will allow it to be used by CPU and devices to access
// mapped, virtual, memory.  In reality though the memory mapping implementation
// would contain several CGenericMemory, or perhaps only one but with a different
// size than the virtual address space.  The mapping object then assigns memory
// requests from the CPU and devices to the correct CGenericMemory object as is
// appropriate for the mapping selected.
// 
//   BTW, note that two sets of functions are provided for accessing memory - 
// CPUread() and CPUwrite(), and UIread() and UIwrite().  The former implement
// the mapping and RAM/ROM scheme in use (e.g. you can't use CPUwrite() to write
// to the ROM space), and the latter assume a flat memory model where all bytes
// are read/write.  
// 
//    For the same reason, note that all the UI related functions (e.g. read/write
// memory from/to a file, change R/W flags, etc) are specific to the CGenericMemory
// class.  The CMemory class exists only to allow the CPU and devices to address a
// virtual memory map via some mapping hardware, and it's assumed that the UI deals
// with the actual physical implementation directly.
//
// REVISION HISTORY:
// 24-Jul-19  RLA   New file.
// 21-JAN-20  RLA   Remove singleton assumptions.
// 16-JUN-22  RLA   Split up CMemory interface and CGenericMemory implementation
// 17-JUN-22  RLA   Add base/offset feature
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ std::string class, et al ...
#include <cstring>		// needed on Linux for memset() ...
#include "MemoryTypes.h"        // address_t and word_t data types
#include "DeviceMap.hpp"        // CDeviceMap class for I/O mapping
using std::string;              // ...

// Standard extensions for Intel hex and raw binary files ...
#define DEFAULT_INTEL_FILE_TYPE     ".hex"
#define DEFAULT_BINARY_FILE_TYPE    ".bin"
#define DEFAULT_MOTOROLA_FILE_TYPE  ".s19"


class CMemory {
  //++
  // Abstract memory interface for CPUs ...
  //--

  // Constants and magic numbers ...
public:
  enum {
    // Memory flags for m_pabFlags ...
    MEM_NONE  = 0x00,     // memory doesn't exist!
    MEM_READ  = 0x01,     // memory can be read (RAM or ROM)
    MEM_WRITE = 0x02,     // memory can be written (RAM only!)
    MEM_IO    = 0x40,     // memory is an I/O device (read or write)
    MEM_BREAK = 0x80,     // break on access to this location
    MEM_FLAGS = 0xFF,     // all flag bits
    // Special combinations of the above flags ...
    MEM_RAM   = MEM_READ|MEM_WRITE,         // generic RAM
    MEM_ROM   = MEM_READ,                   // generic ROM
    MEM_IORW  = MEM_IO|MEM_READ|MEM_WRITE,  // R/W I/O location
    MEM_IORO  = MEM_IO|MEM_READ,            // R/O  "     "
    MEM_IOWO  = MEM_IO|MEM_WRITE,           // W/O  "     "
  };

  // This is an abstract class - no constructor or destructor here!

  // CPU memory access functions ...
public:
  virtual word_t CPUread (address_t a) const = 0;
  virtual void CPUwrite (address_t a, word_t d) = 0;
  virtual bool IsBreak (address_t a) const = 0;
//virtual bool IsIO (address_t a) const = 0;
};


class CGenericMemory : public CMemory {
  //++
  // Generic microprocessor memory emulation class ...
  //--
  friend class CDECfile11;
  friend class CDECfile8;

public:
  // Constructor and destructor ...
  CGenericMemory (size_t cwMemory, address_t cwBase=0, uint8_t bFlags=0);
  virtual ~CGenericMemory();
private:
  // Disallow copy and assignments!
  CGenericMemory (const CGenericMemory &) = delete;
  CGenericMemory& operator= (CGenericMemory const &) = delete;

  // Basic memory properties ...
public:
  inline size_t Size() const {return m_cwMemory;}
  inline size_t ByteSize() const {return m_cwMemory*sizeof(word_t);}
  inline address_t Base() const {return m_cwBase;}
  inline address_t Top()  const {return (m_cwBase+m_cwMemory-1) & ADDRESS_MASK;}

  //   These inline methods should be used for all access to the actual memory
  // contents and flags.  They're the only ones that know about how the actual
  // data is stored internally (e.g. separate arrays for flags and data, base
  // address offset, etc).  Note that they all assume the caller will validate
  // the address parameter with IsValid() first!
public:
  inline bool IsValid (address_t a) const
    {return (a >= m_cwBase)  &&  ((size_t) (a-m_cwBase) < m_cwMemory);}
  inline bool IsValid (address_t nFirst, address_t nLast) const
    {return IsValid(nFirst) && IsValid(nLast) && (nFirst <= nLast);}
  inline word_t MemRead (address_t a) const {return m_pawMemory[a-m_cwBase];}
  inline void MemWrite (address_t a, word_t d) {m_pawMemory[a-m_cwBase] = d;}
  inline uint8_t GetFlags (address_t a) const {return m_pabFlags[a-m_cwBase];}
  inline void SetFlags (address_t a, uint8_t f) {m_pabFlags[a-m_cwBase] = f;}

  // Basic memory functions ...
public:
  // Read or write the location for the CPU ...
  virtual word_t CPUread (address_t a) const override;
  virtual void CPUwrite (address_t a, word_t d) override;
  // Return true if an address break is set at this location ...
  virtual bool IsBreak (address_t a) const override
    {assert(IsValid(a));  return ISSET(GetFlags(a), MEM_BREAK);}
  // RAM is defined as R/W memory that's NOT an I/O device ...
  virtual bool IsRAM (address_t a) const
    {assert(IsValid(a));  return (GetFlags(a) & (MEM_READ|MEM_WRITE|MEM_IO)) == (MEM_READ|MEM_WRITE);}
  // And ROM is defined as R/O memory that's NOT an I/O device ...
  virtual bool IsROM (address_t a) const
    {assert(IsValid(a));  return (GetFlags(a) & (MEM_READ|MEM_WRITE|MEM_IO)) == MEM_READ;}
  // Return true if this address is a memory mapped I/O device ...
  virtual bool IsIO (address_t a) const
    {assert(IsValid(a));  return ISSET(GetFlags(a), MEM_IO);}
  // Return true if this address doesn't exist ...
  virtual bool IsNXM (address_t a) const
    {if (!IsValid(a)) return true;  return (GetFlags(a) == 0);}

  // Other (UI) access functions ...
  //   Note that, unlike the CPUxxx() functions above, these functions will
  // allow you to read or write ANY memory location, regardless of whether
  // it's RAM, ROM, or even exists!  Also note that writing a location doesn't
  // change its flags...
  virtual word_t UIread (address_t a) const {assert(IsValid(a));  return MemRead(a);}
  virtual void UIwrite (address_t a, word_t d) {assert(IsValid(a));  MemWrite(a, d);}
//  virtual word_t& operator[] (address_t a)
//    {assert(IsValid(a));  return (m_pabMemory[a-m_cwBase]);}

  // Memory mapping functions ...
public:
  // Clear the flags on all of memory, regardless ...
  void ClearFlags (uint8_t bFlags=0)
    {assert(m_pabFlags != NULL);  memset(m_pabFlags, bFlags, m_cwMemory);}
  // Set or clear the flags on just one location ...
  inline void SetFlags (address_t a, uint8_t bSet, uint8_t bClear)
    {assert(IsValid(a));  SetFlags(a, (GetFlags(a) & ~bClear) | bSet);}
  // Set or clear the flags on a range of locations ...
  void SetFlags (address_t nFirst, address_t nLast, uint8_t bSet, uint8_t bClear);
  // Set the specified range to be RAM, ROM, I/O or non-existent ...
  void SetRAM (address_t nFirst, address_t nLast) {SetFlags(nFirst, nLast, MEM_READ|MEM_WRITE, 0);}
  void SetRAM (address_t nFirst=0) {SetRAM(nFirst, ADDRESS(Size()-1));}
  void SetROM (address_t nFirst, address_t nLast) {SetFlags(nFirst, nLast, MEM_READ, MEM_WRITE);}
  void SetROM (address_t nFirst=0) {SetROM(nFirst, ADDRESS(Size()-1));}
  void SetIO  (address_t nFirst, address_t nLast) {SetFlags(nFirst, nLast, MEM_IO|MEM_READ|MEM_WRITE, 0);}
  void SetIO  (address_t nFirst) {SetIO(nFirst, nFirst);}
  void SetNXM (address_t nFirst, address_t nLast) {SetFlags(nFirst, nLast, 0, MEM_FLAGS);}
  void SetNXM (address_t nFirst=0) {SetNXM(nFirst, ADDRESS(Size()-1));}
  // Count the number of consecutive locations with matching flags ...
  size_t CountFlags (address_t nFirst) const;
  // Set or clear the address break flag on a location or range of locations ...
  virtual void SetBreak (address_t a, bool fSet=true)
    {assert(IsValid(a));  SetFlags(a, (fSet ? MEM_BREAK : 0), (fSet ? 0 : MEM_BREAK));}
  virtual void SetBreak (address_t nFirst, address_t nLast, bool fSet=true)
    {SetFlags(nFirst, nLast, (fSet ? MEM_BREAK : 0), (fSet ? 0 : MEM_BREAK));}
  // Clear all address break flags everywhere...
  virtual void ClearAllBreaks();
  // Find the NEXT location with a breakpoint flag set ...
  virtual bool FindBreak(address_t& nAddr) const;

  // Other memory routines ...
public:
  // Clear all of memory (regardless of type!) ...
  virtual void ClearMemory (word_t bData=0)
    {assert(m_pawMemory != NULL);  memset(m_pawMemory, bData, ByteSize());}
  // Clear only locations marked as writable!
  virtual void ClearRAM();
  // Clear only locations marked as read only!
  virtual void ClearROM();
  // Load or save the memory in a raw binary image file ...
  virtual int32_t LoadBinary (string sFileName, address_t wBase=0, size_t cbLimit=0);
  virtual int32_t SaveBinary (string sFileName, address_t wBase=0, size_t cbBytes=0) const;
  // Load or save the memory in an Intel format .hex file ...
  virtual int32_t LoadIntel (string sFileName, address_t wBase=0, size_t cbLimit=0, address_t wOffset=0);
  virtual int32_t SaveIntel (string sFileName, address_t wBase=0, size_t cbBytes=0, address_t wOffset=0) const;

    // Device functions for memory mapped I/O ...
public:
  // Install or remvove I/O devices ...
  virtual bool InstallDevice (CDevice *pDevice);
  virtual bool InstallDevice (CDevice *pDevice, address_t nBase, address_t cbSize=0);
  virtual bool RemoveDevice (CDevice *pDevice);
  // Search for devices ...
  virtual CDevice *FindDevice (address_t nPort) const {return m_Devices.Find(nPort);}
  virtual CDevice *FindDevice (const string sName) const {return m_Devices.Find(sName);}
  // Clear (simulate a hardware reset) for all devices ...
  virtual void ClearAllDevices() {m_Devices.ClearAll();}
  // Delete all attached I/O devices (including flags and sense!) ...
  virtual void RemoveAllDevices() {m_Devices.RemoveAll();}

  // Private methods ...
private:
  // Report load/save file errors ...
  static int32_t FileError (string sFileName, const char *pszMsg, int nError=0);
  // Load or save segments of memory in raw binary ...
  static int32_t LoadBinary (uint8_t *pData, string sFileName, size_t cbLimit=0);
  static int32_t SaveBinary (const uint8_t *pData, string sFileName, size_t cbBytes=0);
  // Load or save segments of memory in Intel format .hex files ...
  static int32_t LoadIntel (uint8_t *pData, string sFileName, size_t cbLimit=0, address_t wOffset=0);
  static int32_t SaveIntel (const uint8_t *pData, string sFileName, size_t cbBytes=0, address_t wOffset=0);

  // Members ...
private:
  size_t      m_cwMemory;     // size of the memory
  address_t   m_cwBase;       // base address offset
  word_t     *m_pawMemory;    // the actual memory data lives here
  uint8_t    *m_pabFlags;     // memory flags - read/write or read only
  CDeviceMap  m_Devices;      // I/O devices for memory mapped I/O
};
