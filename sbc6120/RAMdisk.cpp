//++
// RAMdisk.cpp -> SBC6120 RAM disk emulation
//
//   Copyright (C) 1999-2022 by Spare Time Gizmos.  All rights reserved.
//
// LICENSE:
//    This file is part of the SBC6120 emulator project. SBC6120 is free
// software; you may redistribute it and/or modify it under the terms of
// the GNU Affero General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any
// later version.
//
//    SBC6120 is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License
// for more details.  You should have received a copy of the GNU Affero General
// Public License along with SBC6120.  If not, see http://www.gnu.org/licenses/.
//
// DESCRIPTION:
//   The SBC6120 could support up to two RAM disk daughter cards, and each card
// could contain up to four battery backed up SRAM chip.  Each SRAM chip could
// be eitehr 128K or 512K bytes (and we're talking 8 bit bytes here, not 12 bit
// words!) so a fully expanded system could have as much as 8 x 512K or 4Mb of
// RAMdisk.  That's pretty good for a PDP-8!
// 
//   4MB of RAMdisk needs 22 address bits, which is a lot more than the PDP-8
// can manage.  RAMdisk was mapped into panel indirect space using memory map
// mode 3 (see the memory mapping module for more on that), and that gives us
// 12 address bits.  A special 7 bit register called the DAR (disk address
// register) gave us 19 bits, which is enough to address one 512K chip.  And
// then the three EMA data field bits are used to select one of eight memory
// chips.  Using the EMA bits as chip selects makes it convenient to deal with
// missing SRAM chips.
//
//   For the purposes of writing an OS/8 device handler, each 4K RAM disk bank
// contains 21 pages of 128 twelve bit words, packed using the standard OS/8
// "three for two" scheme.  A 512K SRAM chip can hold 128 of these banks,
// corresponding to DAR addresses 0..127, for a total capacity of 2688 PDP-8
// pages or 1344 OS/8 blocks.  A 128K SRAM would contain only 32 banks, for a
// total of 672 PDP-8 pages or 336 OS/8 blocks.
//
//  Sixty-four bytes are wasted in each bank by this packing scheme, which
// works out to about 21 OS/8 blocks lost in a 512K SRAM.  More clever software
// could reclaim these, but it would require that the three-for-two packing
// algorithm split PDP-8 pages across RAM disk banks.  That's too much work!
//
//   Each RAMdisk chip was independent and optional, and the firmware treated
// each chip as a separate "unit".  The OS/8 VM01 RAMdisk driver thus allowed
// up to 8 units, and each unit was either 128Kb (672 pages or 336 blocks) or
// 512Kb (2688 pages or 1344 blocks).
// 
// IMPLEMENTATION
//   This code stores each RAM disk image in a file using the CDiskImageFile
// objects with a 4K byte sector size.  When the Attach() method is called,
// a memory buffer the size of the SRAM chip (yes, either 128K or 512K bytes)
// is allocated and the entire file is read into memory at once.  The image
// remains in memory and the code can do whatever it wants with the buffer,
// and then the whole thing is written out to disk again when the Detach()
// method is called.
// 
//   This is a bit extravagant in terms of memory usage, but on a modern PC
// it's really inconsequential and it makes the rest of the code much easier.
// There's no need to worry about caching banks or anything.  Just be sure to
// properly detach the file so that the data gets written back out, or all
// changes will be lost.  
// 
// REVISION HISTORY:
// 21-AUG-22  RLA   New file.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include <string>               // C++ std::string class, et al ...
#include <cstring>              // needed for memset() ...
#include "EMULIB.hpp"           // emulator library definitions
#include "SafeCRT.h"		// replacements for Microsoft "safe" CRT functions
#include "LogFile.hpp"          // emulator library message logging facility
#include "MemoryTypes.h"        // address_t and word_t data types
#include "Memory.hpp"           // CMemory and CGenericMemory declarations
#include "RAMdisk.hpp"          // declarations for this module
using std::string;              // too lazy to type "std::string..."!


CRAMdisk::CRAMdisk()
{
  //++
  // Initialize our members and allocate the CDiskImageFile objects ...
  //--
  // Allocate the CDiskImageFile objects ...
  for (uint8_t i = 0; i < NDRIVES; ++i) {
    m_apImages[i] = DBGNEW CDiskImageFile(BANK_SIZE);
  }
  memset(m_apBuffers, 0, sizeof(m_apBuffers));
}

CRAMdisk:: ~CRAMdisk()
{
  //++
  // The destructor closes all image files and deletes the objects ...
  //--
  DetachAll();
  for (uint8_t i = 0; i < NDRIVES; ++i)  delete m_apImages[i];
}

uint8_t *CRAMdisk::GetAddress (address_t a) const
{
  //++
  //   This routine will figure out which RAM disk buffer, and then exactly
  // which byte in that buffer, is being addressed.  Remember that the field
  // from the PDP8 address selects the RAM disk unit/chip, and the remainder
  // of the PDP8 address selects the byte within a 4K bank/sector.  The disk
  // address register (aka DAR) supplies the upper five or seven bits required
  // to complete the address.
  // 
  //   One complication is that the LOW ORDER field bit selects one of two
  // RAM disk daughter cards, so that means that fields 0, 2, 4 and 6 select
  // units/chips 0 thru 3, and fields 1, 3, 5, and 7 select units/chips 4 to
  // 7.
  // 
  //   Note that for 128K SRAM chips, the addresses in the DAR wrap around
  // (i.e. for 128K chips the upper 2 DAR bits are ignored).  If the selected
  // unit is offline, then this routine returns NULL instead.
  //--
  uint8_t nUnit = (a >> 12) & 7;
  nUnit = ((nUnit >> 1) & 3) | (ISODD(nUnit) ? 4 : 0);
  if (!IsAttached(nUnit)) return NULL;
  uint32_t lMask = (GetCapacity(nUnit) * BANK_SIZE) - 1;
  uint32_t lAddress = (MASK12(a) | (m_bDAR << 12)) & lMask;
  uint8_t *pBuffer = m_apBuffers[nUnit];
  assert(pBuffer != NULL);
  return &(pBuffer[lAddress]);
}

word_t CRAMdisk::CPUread (address_t a) const
{
  //++
  //   Read a RAM disk byte.  Remember that the RAM disk chips are only eight
  // bits wide, so the upper four bits are always ones.  Attempts to read from
  // SRAM chips that aren't installed (i.e. offline units) return all ones.
  //--
  uint8_t *pData = GetAddress(a);
  if (pData == NULL) return WORD_MAX;
  return *pData | 07400;
}

void CRAMdisk::CPUwrite (address_t a, word_t d)
{
  //++
  //   Write a RAM disk byte.  Only the lower 8 bits of the PDP8 word are used
  // because the RAM disk memory is only eight bits wide.  Attempts to write to
  // non-existent SRAM chips (i.e. offline units) are NOPs.
  //--
  uint8_t *pData = GetAddress(a);
  if (pData != NULL) *pData = MASK8(d);
}

bool CRAMdisk::ReadImage (uint8_t nUnit)
{
  //++
  //   Allocate space for the RAM disk and then read the entire image file
  // (yes, all 512K of it!) into memory.  If anything goes wrong, return false.
  //--
  assert((nUnit < NDRIVES) && IsAttached(nUnit) && (m_apBuffers[nUnit] == NULL));
  uint32_t nBanks = GetCapacity(nUnit);
  uint8_t *pBuffer = DBGNEW uint8_t[nBanks * BANK_SIZE];
  for (uint32_t i = 0;  i < nBanks;  ++i) {
    if (!m_apImages[nUnit]->ReadSector(i, pBuffer + (BANK_SIZE * i))) {
      delete []pBuffer;  return false;
    }
  }
  LOGF(DEBUG, "RAM disk unit %d loaded from %s capacity %ldK",
    nUnit, GetFileName(nUnit).c_str(), m_apImages[nUnit]->GetCapacity() * BANK_SIZE/1024);
  m_apBuffers[nUnit] = pBuffer;  return true;
}

void CRAMdisk::WriteImage (uint8_t nUnit)
{
  //++
  //   This will write the buffer back to the image file, unless the original
  // was read only in which case any changes are lost.  
  //--
  assert((nUnit < NDRIVES) && IsAttached(nUnit) && (m_apBuffers[nUnit] != NULL));
  if (IsReadOnly(nUnit)) {
    // If the file is read only, then we can't write it back ...
    LOGS(WARNING, "RAM disk unit " << nUnit << " not saved because it is read only");
  } else {
    // Write the data back to the file ...
    for (uint32_t i = 0;  i < GetCapacity(nUnit);  ++i) {
      if (!m_apImages[nUnit]->WriteSector(i, m_apBuffers[nUnit] + (BANK_SIZE*i))) {
        LOGS(ERROR, "Error writing RAM disk image " << GetFileName(nUnit));
        delete[]m_apBuffers[nUnit];  m_apBuffers[nUnit] = NULL;
        return;
      }
    }
    LOGF(DEBUG, "RAM disk unit %d saved to %s capacity %ldK",
      nUnit, GetFileName(nUnit).c_str(), m_apImages[nUnit]->GetCapacity() * BANK_SIZE/1024);
  }
  delete []m_apBuffers[nUnit];  m_apBuffers[nUnit] = NULL;
}

bool CRAMdisk::Attach (uint8_t nUnit, const string &sFileName, uint32_t lCapacity)
{
  //++
  //   This method will attach one RAM disk unit to an image file.  The file
  // will be opened and the RAM disk image read into memory, and if all goes
  // well then we return true.  If opening or reading the image fails for any
  // reason, then we return false and the RAM disk unit remains offline.
  // 
  //   It's also possible to set the unit capacity here.  If the lCapacity
  // parameter is the size of the RAM disk, IN BANKS.  For a 128K SRAM chip
  // this would be 32, and for a 512K SRAM it would be 128.  If the lCapacity
  // parameter is zero, then we'll attempt to use the file size to determine
  // the RAM disk size.  No other values for the capacity are legal, and
  // anything else will result in an error.
  // 
  //   The SBC6120 RAM disk doesn't really support the idea of a write protect
  // or a read only unit, but if an existing disk file is write protected then
  // the RAM disk image will not be written back when the unit is detached.
  // It's still possible to modify the RAM disk image while in the simulator,
  // but any changes are lost when you exit.  We'll warn if that situation
  // arises!
  //--
  assert(!sFileName.empty());
  if (IsAttached(nUnit)) Detach(nUnit);

  // Verify that the capacity is one of the legal values ...
  if ((lCapacity != 0) && (lCapacity != RAM128_BANKS) && (lCapacity != RAM512_BANKS)) {
    LOGS(ERROR, "Invalid capacity for RAM disk unit " << nUnit);
    return false;
  }

  // Try to open the image file ...
  if (!m_apImages[nUnit]->Open(sFileName)) return false;

  //   Even though we specified that the image file be opened for read/write
  // access, if the file is read only then the image will be read only.  Check
  // for that, and warn if it happens.
  if (IsReadOnly(nUnit)) {
    LOGS(WARNING, "RAM disk unit " << nUnit << " is read only!");
  }

  // Set the drive capacity as necessary ...
  if (m_apImages[nUnit]->GetCapacity() == 0) {
    // This is an empty file, so it was probably just created ...
    m_apImages[nUnit]->SetCapacity((lCapacity != 0) ? lCapacity : RAM512_BANKS);
  } else {
    // Ignore the lCapacity parameter and use the file size instead ...
    lCapacity = m_apImages[nUnit]->GetCapacity();
    if ((lCapacity != RAM128_BANKS) && (lCapacity != RAM512_BANKS)) {
      LOGS(ERROR, "Invalid file size for RAM disk image " << sFileName);
      m_apImages[nUnit]->Close();  return false;
    }
  }

  // Read the entire image file into memory ...
  if (!ReadImage(nUnit)) {
    LOGS(ERROR, "Error reading RAM disk image " << sFileName);
    m_apImages[nUnit]->Close();  return false;
  }

  // Success!
  return true;
}

void CRAMdisk::Detach (uint8_t nUnit)
{
  //++
  // Take the unit offline and close the image file associated with it.
  //--
  if (!IsAttached(nUnit)) return;
  WriteImage(nUnit);  m_apImages[nUnit]->Close();
}

void CRAMdisk::DetachAll()
{
  //++
  // Detach ALL drives ...
  //--
  for (uint8_t i = 0;  i < NDRIVES;  ++i)  Detach(i);
}

void CRAMdisk::ShowStatus (ostringstream &ofs) const
{
  //++
  // Show the status of all attached RAM disk units.
  //--
  bool fAttached = false;
  for (uint8_t i = 0; i < NDRIVES; i++) {
    if (!IsAttached(i)) continue;
    if (!fAttached) ofs << "RAM disk:\n";
    ofs << FormatString("  Unit %d: %s %ldK\n",
      i, GetFileName(i).c_str(), m_apImages[i]->GetCapacity() * BANK_SIZE/1024);
    fAttached = true;
  }
  if (!fAttached) ofs << "No RAM disk units attached.\n";
}
