//++
// Memory.cpp -> implementation of the CGenericMemory class
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
//   A CGenericMemory objects emulates the 64Kb (usually, at least) memory of your
// average microprocessor system.  The memory object implements methods to load
// or save memory from or to a disk file, and it also implements the memory
// scheme used by the target system.  Unfortunatley the latter is a bit messy.
//
//   Pretty much all systems have some combination of RAM and ROM, but some
// have the RAM at the bottom of the address space (lower numerical addresses)
// and others put the RAM at the top.  Some oddball systems, like the RCA
// microboards and other development systems, want to put the ROM in the middle
// of the address space (e.g. 0x8000..0x87FF) and then have RAM both below AND
// above that!
//
//   In addition to the m_pabMemory array, which stores the 64k bytes of actual
// memory data, we keep another array, m_pabFlags.  This array is also 64K bytes
// and each flag byte contains bits describing whether the corresponding memory
// byte is readable, writable, or both.  Note that if neither bit is set, i.e.
// the flag byte is zero, then the corresponding memory byte "doesn't exist."
// Reads always return 0xFF and writes are ignored.
//
//   Yes, this means we need 128K bytes of memory to simulate 64K, and also
// slows simulated memory access somewhat, but on a modern PC, who cares?
//
// MEMORY MAPPED I/O DEVICES
//   In an I/O mapped system (i.e. one with explicit INPUT and OUTPUT opcodes)
// the CPU owns all I/O devices and the memory knows nothing about them.  In
// a system using memory mapped I/O, the memory object owns all the I/O devices
// and the CPU knows nothing about them.
//
//   The CGenericMemory object implements a simple memory mapped I/O scheme.  To
// install a device, call the CGenericMemory::InstallDevice() rather than the
// CCPU::InstallDevice().  This method will find a free slot in the m_apDevices
// array and save the CDevice object pointer there.  It then obtains the base port
// and number of addresses required by quering the CDevice object and, for each
// address used by the device, it sets the MEM_IO flag AND stores the corresponding
// index to m_apDevices array in the m_pabMemory data bytes.
// 
//   In other words, for memory mapped I/O devices the actual "data" stored in
// simulated memory is the index of the corresponding CDevice object.  This has
// the consequence of limiting us to a maximum of 256 devices, since memory data
// is typically a byte.  That's generally not a problem.
// 
//   When the CPU accesses a memory location that has the MEM_IO flag set, the
// CPUread() and CPUwrite() methods will call the IOread() and IOwrite() methods.
// Those two will find the correct CDevice object and then call CDevice::Read()
// or CDevice::Write() method for that device.  The actual m_pabMemory data is
// not used by the CPU, except to locate the device object.
// 
// REVISION HISTORY:
// 24-JUL-19  RLA   New file.
// 21-JAN-20  RLA   Remove singleton assumptions.
//                  Move the memory mapping functions here.
//  3-FEB-20  RLA   Add m_abFlags and generic mapping.
//  4-JUL-22  RLA   Add memory mapped I/O support.
// 26-AUG-22  RLA   Clean up Linux/WIN32 conditionals.
//  5-MAR-24  RLA   Add ClearROM() and change ClearRAM to use IsRAM() ...
// 24-MAR-25  RLA   Add warning for write to unwritable memory
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#if defined(_WIN32)
#include <io.h>                 // _chsize(), _fileno(), etc...
#elif defined(__linux__) || defined(__APPLE__) || defined(__unix__)
#include <unistd.h>             // ftruncate(), etc ...
#include <sys/stat.h>           // needed for fstat() (what else??)
#endif
#include <string>               // C++ std::string class, et al ...
#include "EMULIB.hpp"           // emulator library definitions
#include "SafeCRT.h"		// replacements for Microsoft "safe" CRT functions
#include "LogFile.hpp"          // emulator library message logging facility
#include "MemoryTypes.h"        // address_t and word_t data types
#include "Memory.hpp"           // declarations for this module
using std::string;              // too lazy to type "std::string..."!


CGenericMemory::CGenericMemory (size_t cwMemory, address_t cwBase, uint8_t bFlags)
{
  //++
  // Initialize our members and allocate space for the memory ...
  //--
  assert(cwMemory > 0);
  m_cwMemory = cwMemory;  m_cwBase = cwBase;
  m_pawMemory = DBGNEW word_t[m_cwMemory];
  m_pabFlags = DBGNEW uint8_t[m_cwMemory];
  ClearFlags(bFlags);  ClearMemory();
}

CGenericMemory:: ~CGenericMemory()
{
  //++
  // Delete the memory array ...
  //--
  assert((m_pawMemory != NULL)  &&  (m_pabFlags != NULL));
  delete[] m_pawMemory;  delete[] m_pabFlags;
  m_cwMemory = m_cwBase = 0;  m_pawMemory = NULL;  m_pabFlags = NULL;
}

word_t CGenericMemory::CPUread (address_t a) const
{
  //++
  //   Read a byte from memory for the microprocessor.  If the address is an
  // I/O device, then call the CDevice object.  If the address isn't readable
  // then return 0xFF instead.
  // 
  //   Note that this routine gets called by the CPU emulation for EVERY BYTE
  // READ from main memory!  That's a lot of calls, and it's a testament to the
  // speed of modern PCs that we can get away with this.
  //--
  assert(IsValid(a));
  if (IsIO(a))
    return m_Devices.DevRead(a);
  else if (IsReadable(a))
    return MemRead(a);
  else
    return WORD_MAX;
}

void CGenericMemory::CPUwrite (address_t a, word_t d)
{
  //++
  //   Called by the CPU emulation to write a byte to memory.  If the address
  // points to a memory mapped I/O device, then call the correct CDevice object
  // to handle it.  If the address points to ROM or non-existent memory, then
  // do nothing.
  //--
  assert(IsValid(a));
  if (IsIO(a))
    m_Devices.DevWrite(a, d);
  else if (IsWritable(a))
    MemWrite(a, d);
  else
    LOGF(WARNING, "write to un-writable memory at 0x%04x", a);
}

void CGenericMemory::SetFlags (address_t nFirst, address_t nLast, uint8_t bSet, uint8_t bClear)
{
  //++
  // Set or clear the flag bits for a whole range of addresses ...
  //--
  assert(IsValid(nFirst, nLast));
  for (size_t i = nFirst;  i <= nLast;  ++i)
    SetFlags(ADDRESS(i), bSet, bClear);
}

size_t CGenericMemory::CountFlags (address_t nFirst) const
{
  //++
  //   This method will count and return the number of consecutive memory
  // locations that have the same READ/WRITE/IO flags as the first.  It may
  // sound a bit obscure, but it's used to figure out and print the memory map.
  //--
  assert(IsValid(nFirst));  size_t nLast;
  uint8_t bFlags = GetFlags(nFirst) & (MEM_READ|MEM_WRITE|MEM_IO);
  for (nLast = nFirst+1;  nLast < Size();  ++nLast) {
    if ((GetFlags(ADDRESS(nLast)) & (MEM_READ|MEM_WRITE)) != bFlags) break;
  }
  return nLast-nFirst;
}

void CGenericMemory::ClearRAM()
{
  //++
  //   Zero the data in every location marked as writable.  Leave all other
  // locations (e.g. ROM!) unmolested.  Note that this ignores I/O locations!
  //--
  for (size_t i = Base();  i <= Top();  ++i)
    if (IsRAM(ADDRESS(i))) MemWrite(ADDRESS(i), 0);
}

void CGenericMemory::ClearROM()
{
  //++
  // Zero the data in every location marked as read only.
  //--
  for (size_t i = Base();  i <= Top();  ++i)
    if (IsROM(ADDRESS(i))) MemWrite(ADDRESS(i), 0);
}

void CGenericMemory::ClearAllBreaks()
{
  //++
  // Clear all address break flags everywhere...
  //--
  for (size_t i = Base();  i <= Top();  ++i)
    if (IsBreak(ADDRESS(i))) SetBreak(ADDRESS(i), false);
}

bool CGenericMemory::FindBreak (address_t &nAddr) const
{
  //++
  //   Search memory, starting at nAddr+1, for the next location with the
  // breakpoint flag set.  If we find one, return that location in nAddr,
  // but if there are no more then return false instead.
  //--
  for (size_t i = ADDRESS(nAddr+1); i <= Top(); ++i) {
    if (IsBreak(ADDRESS(i))) {
      nAddr = ADDRESS(i);  return true;
    }
  }
  return false;
}

bool CGenericMemory::InstallDevice (CDevice *pDevice, address_t nBase, address_t cbSize)
{
  //++
  //   Install the specified device at memory address nBase using cbSize bytes.
  // If cbSize is zero, then query the device for the number of bytes that it
  // wants to use.  This also marks the associated memory addresses as I/O and
  // read/write.  In principle you can have I/O registers that are read only or
  // write only, but in practice those are handled thru the CDevice object.
  //--
  assert(pDevice != NULL);
  if (cbSize == 0)  cbSize = pDevice->GetPortCount();
  assert(cbSize > 0);
  if (!m_Devices.Install(pDevice, nBase, cbSize)) return false;
  SetIO(nBase, nBase+cbSize-1);
  if (pDevice->GetBasePort() != nBase) pDevice->SetBasePort(nBase);
  return true;
}

bool CGenericMemory::InstallDevice (CDevice *pDevice)
{
  //++
  // Install the device using the default address ...
  //--
  assert(pDevice != NULL);
  address_t nBase = pDevice->GetBasePort();
  return InstallDevice(pDevice, nBase);
}

bool CGenericMemory::RemoveDevice (CDevice *pDevice)
{
  //++
  //   Remove the mapping for the specified device, and set the associated
  // memory addresses to non-existent.  Return FALSE if no such device is
  // currently installed.
  //--
  int16_t nPort = m_Devices.Find(pDevice);
  if (nPort == -1) return false;
  while (nPort != -1) {
    SetNXM(ADDRESS(nPort));  nPort = m_Devices.Find(pDevice);
  }
  return m_Devices.Remove(pDevice);
}

int32_t CGenericMemory::FileError (string sFileName, const char *pszMsg, int nError)
{
  //++
  // Print a file related error message and then always return -1.  Why
  // always return -1?  So we can say something like
  //
  //    if ... return Error("fail", errno);
  //--
  char sz[80];
  if (nError > 0) {
    LOGS(ERROR, "error (" << nError << ") " << pszMsg << " " << sFileName);
    strerror_s(sz, sizeof(sz), nError);
    LOGS(ERROR, sz);
  } else {
    LOGS(ERROR, pszMsg << " - " << sFileName);
  }
  return -1;
}

int32_t CGenericMemory::LoadBinary (uint8_t *pData, string sFileName, size_t cbLimit)
{
  //++
  //   This method will load a raw binary image file into memory.  The
  // parameters are -
  //
  //    pData     - address (in m_abMemory) where the data is to be stored
  //    sFileName - name of the file to be loaded
  //    cbLimit   - maximum number of bytes to load
  //
  // Remember that binary files don't contain any address information the way
  // .HEX files do, so binary files always load into memory starting from
  // address zero.  
  //
  //   The number of bytes actually read is returned, or -1 if there was an
  // error reading the file.  Note that this method doesn't distinguish between
  // RAM and ROM, and is equally happy to load either or both.
  //--
  assert(cbLimit > 0);

  //  Open the file and get its length, in bytes. If either of these operations
  // fails, then give up and quit now ...
  FILE *pFile;  int err = fopen_s(&pFile, sFileName.c_str(), "rb");
  if (err != 0) return FileError(sFileName, "opening", err);
#if defined(_WIN32)
  size_t cbFile = _filelength(_fileno(pFile));
#elif defined(__linux__) || defined(__APPLE__) || defined(__unix__)
  struct stat st;
  if (fstat(fileno(pFile), &st) != 0) return FileError(sFileName, "fstat", errno);
  size_t cbFile = st.st_size;
#endif

  //   If the actual length of the file is longer than cbMaxBytes then complain
  // and truncate the read operation ...
  if (cbFile > cbLimit) {
    LOGS(WARNING, sFileName << " is too long");  cbFile = cbLimit;
  }

  // Now read the file ...
  if (fread(pData, 1, cbFile, pFile) != cbFile)
    return FileError(sFileName, "reading", errno);

  // Close the file and we're done ...
  fclose(pFile);
  return (int32_t) cbFile;
}

int32_t CGenericMemory::SaveBinary (const uint8_t *pData, string sFileName, size_t cbBytes) 
{
  //++
  //   This method saves a chunk of memory to a raw binary file.  The
  // parameters are -
  //
  //    pData     - address (in m_abMemory) where the data is stored
  //    sFileName - name of the file to be loaded
  //    cbBytes   - number of bytes to save
  //
  // Remember that binary files don't contain any address information the way
  // Intel HEX files do, so it's up to you to remember what addresses are
  // stored in this file.
  //
  //   This method returns the number of bytes actually written (i.e. cbBytes)
  // or -1 if some error occurs while writing the file.  Note that this method
  // doesn't distinguish between RAM and ROM and is equially happy to save
  // either or both.
  //--
  assert(cbBytes  > 0);

  // Open the file for writing ...
  FILE *pFile;  int err = fopen_s(&pFile, sFileName.c_str(), "wb+");
  if (err != 0) return FileError(sFileName, "opening", err);

  // Now write the file ...
  if (fwrite(pData, 1, cbBytes, pFile) != cbBytes)
    return FileError(sFileName, "writing", errno);

  // Close the file and we're done ...
  fclose(pFile);
  return (int32_t) cbBytes;
}

int32_t CGenericMemory::LoadIntel (uint8_t *pData, string sFileName, size_t cbLimit, address_t wOffset)
{
  //++
  //   This function will load a standard Intel format .HEX file into memory.
  // The parameters are -
  //
  //    pData     - address (in m_abMemory) where the data is to be stored
  //    sFileName - name of the file to be read
  //    cbLimit   - maximum memory address to load
  //    wOffset   - 16 bit offset applied to addresses in the HEX file
  //
  // cbLimit specifies the largest address allowed in the HEX file - any address
  // larger than this will report an error and be ignored.  wOffset is a 16 bit
  // offset that is applied to every address read from the HEX file - this can
  // be used to relocate memory images if desired.  Note that the offset is
  // applied with 16 bit two's complement arithmetic, so either either positive
  // or negative offsets are possible.
  //
  //   This method supports only the traditional 16-bit address format hex
  // files (not the newer ones with 32 bit addresses!).  This puts anvupper
  // limit of 64K on the amount of data which may be loaded.  Only hex record
  // types 00 (data) and 01 (end of file) are recognized.
  //
  //   This method returns the actual number of bytes read, or -1 if any error
  // occurs.
  //
  //   Lastly, notice the usage of "unsigned" data types for some locals below.
  // This was done to fix problems with fscanf() corrupting the stack while
  // trying to read items shorter than a native integer.  Yes, I do know about
  // the "h" and "hh" width specifiers and those didn't fix the problem.  I
  // actually think it's a bug in MSVC, but whatever this is the workaround.
  // Since we're always careful to mask anyway, the actual size doesn't matter.
  //--
  FILE          *pFile;           // handle of the input file
  uint32_t       cbLoaded = 0;    // number of bytes loaded from file
  unsigned       cbRecord;	  // length of the current .HEX record
  unsigned       nAddress;	  // load address "   "     "      "     "
  uint16_t       wRelocated;      // relocated address      "      "     "
  unsigned       nType;	          // type         "   "     "      "     "
  uint8_t        bChecksum;	  // checksum     "   "     "      "     "
  unsigned       nData;  	  // temporary for the data byte read...
  bool           fWarned = false; // true if we've already issued a warning
  
  // Open the file for reading ...
  int err = fopen_s(&pFile, sFileName.c_str(), "rt");
  if (err != 0) return FileError(sFileName, "opening", err);

  // Read records and load them until EOF ...
  while (!feof(pFile)) {
    if (fscanf_s(pFile, ":%2x%4x%2x", &cbRecord, &nAddress, &nType) != 3)
      return FileError(sFileName, "bad .HEX file format (1)");
    if (nType > 1)
      return FileError(sFileName, "unknown record type");
    bChecksum = MASK8(cbRecord) + HIBYTE(nAddress) + LOBYTE(nAddress) + nType;
    for (;  cbRecord > 0;  --cbRecord) {
      if (fscanf_s(pFile, "%2x", &nData) != 1)
        return FileError(sFileName, "bad .HEX file format (2)");
      wRelocated = MASK16(nAddress + wOffset);
      if (wRelocated > cbLimit) {
        if (!fWarned) LOGS(WARNING, sFileName << " illegal address");
        fWarned = true;
      } else {
        pData[wRelocated] = nData;  ++nAddress;  ++cbLoaded;
      }
      bChecksum += nData;
    }
    if (fscanf_s(pFile, "%2x\n", &nData) != 1)
      return FileError(sFileName, "bad .HEX file format (3)");
    bChecksum = MASK8(bChecksum +nData);
    if (bChecksum != 0)
      return FileError(sFileName, "checksum error");
    if (nType == 1) break;
  }
  
  // Close the file and we're done ...
  fclose(pFile);
  return (int32_t) cbLoaded;
}

int32_t CGenericMemory::SaveIntel (const uint8_t *pData, string sFileName, size_t cbBytes, address_t wOffset)
{
  //++
  //   This function will save all or part of memory into a file using the
  // standard Intel HEX format. The parameters are -
  //
  //    pData     - address (in m_abMemory) where the data is to be stored
  //    sFileName - name of the file to be written
  //    cbBytes   - number of bytes to save
  //    wOffset   - 16 bit offset applied to addresses in the HEX file
  //
  // wOffset is a 16 bit offset that is applied to every address before it is
  // written to the HEX file - this can be used to relocate memory images if
  // desired.
  //
  //   Only the traditional 16 bit hex file format is supported and memory is
  // limited to 64K or less.  The only records generated are type 00 (data) and
  // 01 (end of file).  This method always returns the number of bytes written
  // (i.e. cbBytes) or -1 if any file error occurs ...
  //--
  FILE    *pFile;           // handle of the input file
  unsigned cbWritten = 0;   // number of bytes written to the file
  unsigned cbRecord;	    // length       of the current .HEX record
  uint16_t wAddress = 0;    // load address "   "     "      "     "
  uint16_t wRelocated;      // relocated address      "      "     "
  uint8_t  bChecksum;	    // checksum     "   "     "      "     "

  // Open the file for writing ...
  int err = fopen_s(&pFile, sFileName.c_str(), "wt");
  if (err != 0) return FileError(sFileName, "opening", err);

  // Write records until all bytes have been saved ...
  while (cbBytes > 0) {
    cbRecord = (cbBytes > 16) ? 16 : (cbBytes & 0xF);
    wRelocated = MASK16(wAddress + wOffset);
    fprintf(pFile, ":%02X%04X00", cbRecord, wRelocated);
    bChecksum = MASK8(cbRecord) + HIBYTE(wRelocated) + LOBYTE(wRelocated) + 0x00 /* Type */;
    for (unsigned i = 0; i < cbRecord; ++i) {
      uint8_t bData = pData[wAddress+i];
      fprintf(pFile, "%02X", bData);  bChecksum += bData;
    }
    fprintf(pFile, "%02X\n", MASK8(-bChecksum));
    cbBytes -= cbRecord;  cbWritten += cbRecord;  wAddress += cbRecord;
  }

  // Write the 01 EOF record, close the file, and we're done ...
  fprintf(pFile, ":00000001FF\n");
  fclose(pFile);
  return (int32_t) cbWritten;
}

int32_t CGenericMemory::LoadBinary (string sFileName, address_t wBase, size_t cbLimit)
{
  //++
  //   Load a raw binary image file into memory and return the number of bytes
  // actually read, or -1 if there is an error.
  //
  // Parameters -
  //    sFileName - name of the file to be loaded
  //    wBase     - address (0..65535) of first memory byte to load
  //    cbLimit   - maximum number of bytes to load
  //--
  if (cbLimit == 0) cbLimit = ByteSize();
  assert((wBase+cbLimit) <= ByteSize());
  return LoadBinary((uint8_t *) &(m_pawMemory[wBase]), sFileName, cbLimit);
}

int32_t CGenericMemory::SaveBinary (string sFileName, address_t wBase, size_t cbBytes) const
{
  //++
  //   Save memory to a raw binary image file and return the number of bytes
  // actually written, or -1 if there is an error.
  //
  // Parameters -
  //    sFileName - name of the file to write
  //    wBase     - address (0..65535) of first memory byte to save
  //    cbBytes   - number of bytes to save
  //--
  if (cbBytes == 0) cbBytes = ByteSize();
  assert((wBase+cbBytes) <= ByteSize());
  return SaveBinary((uint8_t *) &(m_pawMemory[wBase]), sFileName, cbBytes);
}

int32_t CGenericMemory::LoadIntel (string sFileName, address_t wBase, size_t cbLimit, address_t wOffset)
{
  //++
  //   Load memory from a standard Intel format .HEX file and return the number
  // of bytes read, or -1 if an error occurs.
  //
  // Parameters -
  //    sFileName - name of the file to be read
  //    wBase     - address (0..65535) of first memory byte to load
  //                All HEX file addresses are relative to this address...
  //    cbLimit   - maximum memory address to load
  //    wOffset   - 16 bit offset applied to addresses in the HEX file
  //--
  if (cbLimit == 0) cbLimit = ByteSize();
  assert((wBase+cbLimit) <= ByteSize());
  return LoadIntel((uint8_t *) &(m_pawMemory[wBase]), sFileName, cbLimit, wOffset);
}

int32_t CGenericMemory::SaveIntel (string sFileName, address_t wBase, size_t cbBytes, address_t wOffset) const
{
  //++
  //   Save memory to a standard Intel format .HEX file and return the number
  // of bytes written, or -1 if an error occurs.
  //
  // Parameters -
  //    sFileName - name of the file to write
  //    wBase     - address (0..65535) of first memory byte to save
  //    cbBytes   - number of bytes to save
  //    wOffset   - 16 bit offset applied to addresses in the HEX file
  //--
  if (cbBytes == 0) cbBytes = ByteSize();
  assert((wBase+cbBytes) <= ByteSize());
  return SaveIntel((uint8_t *) &(m_pawMemory[wBase]), sFileName, cbBytes, wOffset);
}
