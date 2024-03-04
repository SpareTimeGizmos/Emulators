//++
// DECfile11.hpp -> DEC Absolute Loader Paper Tape Routines
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
//   The CDECfile11 class adds routines to load and save paper tape images in
// the DEC PDP-11 absolute loader format to the CGenericMemory class.  That's
// it - nothing more!
// 
//   Note that this class cannot be instantiated.  It simply contains a few
// static routines that you're expected to call when you need them.
//
// REVISION HISTORY:
//  1-AUG-22  RLA   Split from the Memory.hpp file.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#ifdef _WIN32
#include <io.h>                 // _chsize(), _fileno(), etc...
#elif __linux__
#include <unistd.h>             // ftruncate(), etc ...
#include <sys/stat.h>           // needed for fstat() (what else??)
#endif
#include <string>               // C++ std::string class, et al ...
#include "EMULIB.hpp"           // emulator library definitions
#include "SafeCRT.h"		// replacements for Microsoft "safe" CRT functions
#include "LogFile.hpp"          // emulator library message logging facility
#include "MemoryTypes.h"        // address_t and word_t data types
#include "Memory.hpp"           // CGenericMemory friend class
#include "DECfile11.hpp"        // declarations for this module
using std::string;              // too lazy to type "std::string..."!


bool CDECfile11::GetWord (FILE *pFile, uint16_t &wData, uint8_t &bChecksum)
{
  //++
  //   Read one 16 bit word (two bytes) from a DEC paper tape image.  If there
  // are no more words (i.e. EOF is true) then return false...
  //--
  uint8_t bLow, bHigh;
  if (fread(&bLow, 1, 1, pFile) != 1) return false;
  if (fread(&bHigh, 1, 1, pFile) != 1) return false;
  bChecksum += bLow;  bChecksum += bHigh;
  wData = MKWORD(bHigh, bLow);
  return true;
}

void CDECfile11::PutWord (FILE *pFile, uint16_t wData, uint8_t &bChecksum)
{
  //++
  // Write a 16 bit word (two bytes) to a DEC paper tape image.
  //--
  uint8_t bLow = LOBYTE(wData);
  uint8_t bHigh = HIBYTE(wData);
  fputc(bLow, pFile);  fputc(bHigh, pFile);
  bChecksum += bLow;  bChecksum += bHigh;
}

int32_t CDECfile11::LoadPaperTape (uint8_t *pData, string sFileName, size_t cbLimit)
{
  //++
  //   This function will load a standard DEC PDP-11 absolute loader paper tape
  // image into memory.  The parameters are -
  //
  //    pData     - address (in m_abMemory) where the data is to be stored
  //    sFileName - name of the file to be read
  //    cbLimit   - maximum memory address to load
  //
  //   This method returns the actual number of bytes read, or -1 if any error
  // occurs.
  //--
  FILE          *pFile;           // handle of the input file
  uint32_t       cbLoaded = 0;    // number of bytes loaded from file
  uint8_t        bData = 0;       // temporary byte for reading the file
  uint16_t       wSignature;      // absolute loader "signature" word
  address_t      cbRecord;	  // length of the current block
  address_t      wAddress;	  // load address "   "      "
  uint8_t        bChecksum = 0;	  // checksum     "   "      "

  // Open the file for reading ...
  int err = fopen_s(&pFile, sFileName.c_str(), "rb");
  if (err != 0) return CGenericMemory::FileError(sFileName, "opening", err);

  // Read a standard absolute loader block ...
  while (!feof(pFile)) {
    // Ignore any blank leader tape ...
    while (!feof(pFile) && ((bData = fgetc(pFile)) == 0));
    ungetc(bData, pFile);
    // The "signature word" is always 0x0001 ...
    if (!GetWord(pFile, wSignature, bChecksum)) break;
    if (wSignature != 1) {
      //   Many tape images end with some garbage characters, so if we find
      // an invalid header but we've already loaded something, then just
      // quit without complaining ...
      if (cbLoaded == 0)
        return CGenericMemory::FileError(sFileName, "absolute loader signature not found");
      break;
    }
    // Read the block length and the load address, and sanity check both ...
    //   Note that a block length of zero means we've reached the end of tape.
    if (!GetWord(pFile, cbRecord, bChecksum) || !GetWord(pFile, wAddress, bChecksum))
      return CGenericMemory::FileError(sFileName, "header words missing");
    if ((cbRecord < 6) /*|| ISSET(cbRecord, 1)*/ || (cbRecord >= 32768))
      return CGenericMemory::FileError(sFileName, "invalid block length");
    if ((cbRecord -= 6) == 0) continue;
    //  if (ISSET(wAddress, 1))
//    return FileError(sFileName, "invalid load address");
    // Load the block of data.  
    LOGF(TRACE, "%s loading %d bytes at 0%06o", sFileName.c_str(), cbRecord, wAddress);
    while (cbRecord > 0) {
      if (wAddress > cbLimit)
        return CGenericMemory::FileError(sFileName, "load address out of range");
      if (feof(pFile))
        return CGenericMemory::FileError(sFileName, "premature end of file");
      bData = fgetc(pFile);  pData[wAddress++] = bData;  bChecksum += bData;
      ++cbLoaded;  --cbRecord;
    }
    // And, finally, verify the checksum...
    if (feof(pFile))
      return CGenericMemory::FileError(sFileName, "checksum byte missing");
    bChecksum += fgetc(pFile);
    if (bChecksum != 0)
      return CGenericMemory::FileError(sFileName, "checksum mismatch");
  }

  // Close the file and we're done ...
  fclose(pFile);
  return (int32_t) cbLoaded;
}

int32_t CDECfile11::SavePaperTape (const uint8_t *pData, string sFileName, size_t cbBytes, uint16_t wAddress)
{
  //++
  //   This routine will save memory to a file in the DEC PDP-11 absolute
  // loader format.  It's pretty simply minded and always writes everything
  // as one, large, single block.  It works though, and these files may be
  // read back with LoadDEC() ...
  //
  // The parameters are -
  //
  //    pData     - address (in m_abMemory) where the data is to be stored
  //    sFileName - name of the file to be read
  //    cbBytes   - number of bytes to save
  //    wAddress  - address of block to be saved
  //
  // It returns the actual number of bytes saved, or -1 if any error occurs.
  //--
  FILE    *pFile;           // handle of the input file
  uint8_t  bData;           // temporary 8 bit data byte
  unsigned cbWritten = 0;   // number of bytes written to the file
  uint8_t  bChecksum = 0;   // checksum of this block

  // Open the file for writing ...
  int err = fopen_s(&pFile, sFileName.c_str(), "wb");
  if (err != 0) return CGenericMemory::FileError(sFileName, "opening", err);

  // Write out the signature, count and load address ...
  PutWord(pFile, 1, bChecksum);
  PutWord(pFile, (uint16_t) (cbBytes+6), bChecksum);
  PutWord(pFile, wAddress, bChecksum);

  // Now save the data and checksum ...
  for (size_t i = 0;  cbBytes > 0;  ++i, --cbBytes) {
    bData = pData[i];
    bChecksum += bData;
    fputc(bData, pFile);
    ++cbWritten;
  }
  fputc(-bChecksum, pFile);

  // And write an "end of file" header ...
  bChecksum = 0;
  PutWord(pFile, 1, bChecksum);
  PutWord(pFile, 6, bChecksum);
  PutWord(pFile, wAddress, bChecksum);
  fputc(-bChecksum, pFile);

  // All done!
  fclose(pFile);
  return (int32_t) cbWritten;
}

int32_t CDECfile11::LoadPaperTape (CGenericMemory *pMemory, string sFileName)
{
  //++
  //   Load memory from a standard DEC PDP-11 absolute loader paper tape image,
  // and return the number of bytes read, or -1 if an error occurs.
  //--
  assert(pMemory != NULL);
  return LoadPaperTape((uint8_t *) &(pMemory->m_pawMemory), sFileName, pMemory->ByteSize());
}

int32_t CDECfile11::SavePaperTape (CGenericMemory *pMemory, string sFileName, address_t wBase, size_t cbBytes)
{
  //++
  //   Save memory to a standard DEC PDP-11 absolute loader paper tape image,
  // and return the number of bytes written, or -1 if an error occurs.
  //--
  assert(pMemory != NULL);
  if (cbBytes == 0) cbBytes = pMemory->ByteSize();
  assert((wBase+cbBytes) <= pMemory->ByteSize());
  return SavePaperTape((uint8_t *) &(pMemory->m_pawMemory[wBase]), sFileName, cbBytes, (uint16_t) wBase);
}
