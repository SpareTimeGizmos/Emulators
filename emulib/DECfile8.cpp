//++
// DECfile8.cpp -> DEC Absolute Loader Paper Tape Routines
//
//   Copyright (C) 1999-2022 by Spare Time Gizmos.  All rights reserved.
//
// DESCRIPTION:
//   The CDECfile8 class adds routines to load and save paper tape images in
// the DEC PDP-8 absolute loader format to the CGenericMemory class.  That's
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
#include "Memory.hpp"           // CGenericMemory declarations
#include "DECfile8.hpp"         // declarations for this module
using std::string;              // too lazy to type "std::string..."!


bool CDECfile8::GetByte (FILE *pFile, uint8_t &bData)
{
  //++
  //   This function reads and returns the next byte from the BIN file.	
  // If there are no more bytes, it returns FALSE.  It's fairly simple...	
  //--
  if (fread(&bData, 1, 1, pFile) != 1) return false;
  return true;
}

bool CDECfile8::GetFrame (FILE *pFile, uint16_t &wFrame, uint16_t &wChecksum)
{
  //++
  //   This routine reads the next frame from the BIN file, which is a	
  // little more complex.  Since paper tape is only 8 bits wide, twelve	
  // bit PDP-8 words are split up into two six bit bytes which together	
  // are called a "frame".  The upper two bits of the first byte are used	
  // as a code to describe the type of the frame.  The upper bits of the	
  // second byte are, so far as I know, unused and are always zero.  This	
  // function returns a 14 bit tape frame - 12 bits of data plus the type	
  // bits from the first byte.  A complication is that the BIN checksum	
  // accumulates the bytes on the tape, not the frames, so we have to	
  // calculate it here.  The caller can't do it...			
  //--
  uint8_t bByte;

  // Read the first byte - this is the high order part of the frame... 
  if (!GetByte(pFile, bByte)) return false;
  wFrame = bByte << 6;
  
  //   Frame types 2 (leader/trailer) and 3 (field settings) are single	
  // bytes - there is no second data byte in these frames!  Moreover, 	
  // these frame types are _not_ counted towards the checksum!		
  if ((wFrame & 020000) != 0) return true;
  
  // Frame types 0 (data) and 1 (address) are normal 12 bit data.... 
  wChecksum += bByte;
  if (!GetByte(pFile, bByte)) return false;
  wFrame |= bByte & 077;  wChecksum += bByte;
  return true;
}

int32_t CDECfile8::LoadSegment (FILE *pFile, CGenericMemory *pMemory, uint16_t &wFrame, uint16_t &wChecksum)
{
  //++
  //   This function loads one segment of a BIN format tape image.  Most  
  // tapes have only one segment (i.e. <leader> <data> <trailer> <EOT>),  
  // but a few (e.g. FOCAL69 with the INIT segment) are <leader> <data-1> 
  // <trailer-1>/<leader-2> <data-2> ... <trailer-n> <EOT>.  When this    
  // function is called the leader has already been skipped and the first 
  // actual data frame is passed in m_nFrame. When we return, the data will 
  // have been read and m_nFrame will contain a leader/trailer code.  The   
  // big problem is the checksum, which looks just like a data frame.  In 
  // fact, the only way we can tell that it is a checksum and not data is 
  // because of its position as the very last frame on the tape.  This    
  // means that every time we find a data frame, we have to look ahead at 
  // the next frame to see whether it's a leader/trailer.  If it is, then 
  // the current frame is a checksum. If it isn't, then the current frame 
  // is data to be stored!		                                
  //									
  //   The return value is the number of data words stored in memory, or	
  // -1 if the tape format is bad (e.g. a mismatched checksum).		
  //--
  uint16_t   wAddress  = 00200;	// current load address (00200 default)	
  uint16_t   nCount    = 0;	// count of data words read and stored	
  uint16_t   wNextFrame;	// next tape frame, for look ahead

  // Load tape frames until we hit the end!   
  while (true) {
    switch (wFrame & 030000) {

      case 000000:
        //   This is a data frame. It could be either data to be stored	
        // in memory, or a checksum.  The only way to know is to peek	
        // at the next frame...						
        if (!GetFrame(pFile, wNextFrame, wChecksum)) return -1;
        if (wNextFrame == 020000) {
          //   This is the end of the tape, so this frame must be the	
          // checksum.  Make sure that it matches our value and then we	
          // are done loading.  One slight problem is that the checksum	
          // bytes shouldn't be added to the checksum accumulator, but	
          // ReadBIm_nFrame() has already done it.  We'll subtract them	
          // back out to compensate...					
          wChecksum = (wChecksum - (wFrame >> 6) - (wFrame & 077)) & 07777;
          if (wChecksum != wFrame) return -1;
          return nCount;
        } else if (wAddress >= pMemory->Size()) {
          // Real data, but the load address is invalid... 
          return 0;
        } else {
          // This is, as they say, the good stuff... 
          pMemory->m_pawMemory[wAddress] = wFrame;
          ++wAddress;  ++nCount;
        }
        wFrame = wNextFrame;
        // Don't read the next frame this time - we already did that! 
        continue;

      case 010000:
        //   Frame type 1 sets the loading origin.  The address is only	
        // twelve bits - the field can be set by frame type 3...	
        wAddress = (wAddress & 070000) | (wFrame & 07777);
        break;
        
      case 020000:
        //  Frame type 2 is leader/trailer codes, and these are usually	
        // handled under case 000000, above.  If we find one here it	
        // means that the tape didn't end with a data frame, and hence	
        // there was no checksum.  I don't think this ever happens in	
        // real life, so we'll consider it a bad tape...		
        return -1;
        
      case 030000:
        //  Type 3 frames set the loading field... 
        wAddress = (wFrame & 007000) | 0200;
        break;
    }

    // Read the next tape frame and keep on going. 
    if (!GetFrame(pFile, wFrame, wChecksum)) return -1;

  }
}

int32_t CDECfile8::LoadPaperTape (CGenericMemory *pMemory, string sFileName)
{
  //++
  //   This function will load an entire BIN tape, given its file name.	
  // It returns a DWORD with the total number of words loaded in the	
  // right (LSW) half and the total number of segments in the left.  If	
  // the tape file is corrupted in any way, then it returns zero.		
  //--
  assert((WORD_SIZE == 12)  &&  (ADDRESS_SIZE == 15));
  FILE      *pFile;         // handle of the input file
  uint16_t   nSegments = 0; // number of segments in this tape            
  uint16_t   nWords    = 0; //   "     " words     "   "    "		
  int32_t    nCount;        // number of words loaded from this segment
  uint16_t   wFrame;
  uint16_t   wChecksum;
  uint8_t    bData;         // temporary 8 bit data

  // Open the file for reading ...
  int err = fopen_s(&pFile, sFileName.c_str(), "rb");
  if (err != 0) return CGenericMemory::FileError(sFileName, "opening", err);

  //   Some tape images begin with the name of the actual name of the 	
  // program, in plain ASCII text.  Apparently the real DEC BIN loader	
  // ignores anything before the start of the leader, so we'll do the	
  // same...								
  do
    if (!GetByte(pFile, bData)) return 0;
  while (bData != 0200);

  while (true) {
    // Skip the leader and find the first data frame... 
    wChecksum = 0;
    do
      if (!GetFrame(pFile, wFrame, wChecksum)) {
        fclose(pFile);
        return MKLONG(nSegments, nWords);
      }
    while (wFrame == 020000);
  
    // Load this segment of the tape... 
    nCount = LoadSegment(pFile, pMemory, wFrame, wChecksum);
    if (nCount <= 0) return 0;
    nWords += nCount;  ++nSegments;
  }
}

int32_t CDECfile8::Load2Intel (CGenericMemory *pMemory, string sFileNameHigh, string sFileNameLow, address_t wBase, size_t cwLimit, address_t wOffset)
{
  //++
  //   This routine will load PDP-8 memory from two separate Intel .hex files.
  // Being a 12 bit machine, 8 bits alone just aren't enough and the SBC6120
  // uses two EPROMs to get all 12 bits.  The High hex file is the image of
  // the EPROM that contains bits 0-5 (remember that PDP-8 bits are numbered
  // from the left, so that's the most significant half!) and the Low hex file
  // is the EPROM image that contains bits 6-11.
  //--
  if (cwLimit == 0) cwLimit = pMemory->Size();
  assert((wBase+cwLimit) <= pMemory->Size());
  uint8_t *pabHigh = DBGNEW uint8_t[pMemory->Size()];
  uint8_t *pabLow  = DBGNEW uint8_t[pMemory->Size()];
  int32_t cbHigh = CGenericMemory::LoadIntel(pabHigh, sFileNameHigh, cwLimit, wOffset);
  int32_t cbLow  = CGenericMemory::LoadIntel(pabLow, sFileNameLow, cwLimit, wOffset);
  if (cbHigh != cbLow)
    LOGS(WARNING, " hex files contain different number of bytes");
  for (address_t i = wBase; i < cwLimit; ++i) {
    pMemory->m_pawMemory[i] = ((pabHigh[i] & 077) << 6) | (pabLow[i] & 077);
  }
  delete []pabHigh;
  delete []pabLow;
  return cbHigh;
}

int32_t CDECfile8::SavePaperTape(CGenericMemory *pMemory, string sFileName, address_t wBase, size_t cbBytes)
{
  //++
  //--
  assert(false);
  return -1;
}

int32_t CDECfile8::Save2Intel (CGenericMemory *pMemory, string sFileNameHigh, string sFileNameLow, address_t wBase, size_t cwLimit)
{
  //++
  //--
  assert(false);
  return -1;
}
