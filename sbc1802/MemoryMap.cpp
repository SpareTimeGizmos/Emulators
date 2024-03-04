//++
// MemoryMap.hpp -> CMemoryControl (SBC1802 memory control register) 
//                  CMemoryMap (SBC1802 memory mapping)
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
//   This module implements the memory mapping hardware that's unique to
// the SBC1802.  There are two devices we're primarily interested in -
// a six bit read/write memory control register (hereafter known as the
// MCR), and a GAL that uses the MCR bits and CPU address bits to decide
// which memory or peripheral chip should be selected.  Yes, that's right,
// the SBC1802 has some memory mapped peripherals, including the MCR itself
// as well as the CDP1877 priority interrupt controller and the CDP1879
// real time clock.
//
//   The goal is to be able to run both MicroDOS and ElfOS, which have very
// different memory layouts. The OS selection is made at boot time and it is
// possible to run either OS without any hardware changes (no jumpers moved;
// no GALs reprogrammed, etc).  It's also desirable to have as much RAM
// avaiTlable to either OS as is practical, certainly more than just 32K.
// PLUS, the plan is to stuff in an expanded version of the STG EPROM monitor,
// which will require most of a 32K EPROM.	
//									
//    The following five memory maps, selectable by the MCR, cover all the
// cases (with a little additional software magic) ...
//
// FFFF  +-------+    +-------+    +-------+    +-------+    +-------+	
//       | EPROM |    | EPROM |    |       |    |       |    |       |	
// F000  +-------+    +-------+    |       |    |       |    |       |	
//       |       |    |       |    |       |    |       |    |       |	
//       | RAM 1 |    |       |    | EPROM |    | EPROM |    | EPROM |	
//       |       |    | RAM 1 |    |       |    |       |    |       |	
// 8400  +-------+    |       |    |       |    |       |    |       |	
//       | EPROM |    |       |    |       |    |       |    |       |	
// 8000  +-------+    +-------+    +-------+    +-------+    +-------+	
//       |       |    |       |    |       |    |       |    |       |	
//       |       |    |       |    |       |    |       |    |       |	
//       |       |    |       |    |       |    |       |    |       |	
//       | RAM 0 |    | RAM 0 |    | RAM 0 |    | RAM 1 |    | EPROM |	
//       |       |    |       |    |       |    |       |    |       |	
//       |       |    |       |    |       |    |       |    |       |	
//       |       |    |       |    |       |    |       |    |       |	
// 0000  +-------+    +-------+    +-------+    +-------+    +-------+	
//       MICRODOS       ELFOS        ROM0         ROM1         BOOT	
//
//   The MicroDOS and ElfOS maps are for running those respective operating
// systems.  Note that it's not necessary to have the EPROM mapped from
// 0xF000 to 0xFFFF for MicroDOS but it is convenient, at the expense of
// chopping 4K off the MicroDOS address space.  The ROM0 and ROM1 maps are
// used when the STG monitor is in control, and it's necessary to have two
// maps so that the EPROM can access all of user RAM.  The BOOT map is used
// only after an 1802 RESET so that the	EPROM can get control.     	   	    	       	       		
//									
//   BUT ... it can never be quite that simple.  One issue is that the STG
// EPROM needs some RAM that it can call its own, and another is that the
// SBC1802 contains two memory mapped peripharals. Both the CDP1877 priority
// interrupt controller (aka PIC) and the CDP1879 real time clock (aka RTC)
// are memory mapped. And lastly the memory control register itself is mapped
// into a memory location, mostly because of I/O limitations on the SBC1802
// base board.				
//									
//  All these devices exist in the 0xF000 to 0xFFFF BIOS range, but the BIOS
// also has a lot of code that must be at fixed locations, notably in pages
// $F8xx and $FFxx.  Here's a more detailed layout of the BIOS memory region.
//
//   F000..F7FF -> mapped to EPROM (general BIOS code)
//   F800..F8FF -> mapped to EPROM (BIOS entry vectors)
//   F900..FDFF -> mapped to EPROM (general BIOS code)
//   FE00..FEDF -> mapped to RAM1 (STG monitor scratch pad area)
//   FEE0..FEE6 -> not mapped (do not use!)
//   FEE7       -> memory control register
//   FEE8..FEEF -> mapped to CDP1879 RTC registers
//   FEF0..FEF3 -> mapped to CDP1877 PIC mask/status registers
//   FEF4..FEF7 -> mapped to CDP1877 PIC control/polling registers
//   FEF8..FEFB -> mapped to CDP1877 PIC page/vector registers
//   FEFC..FEFF -> not mapped (do not use!)
//   FF00..FFFF -> mapped to EPROM (more BIOS vectors, checksum, etc)
//
//   Note that this BIOS region, addresses $Fxxx, is mapped exactly the same
// in ALL memory modes.  In particular, the scratch pad RAM, PIC, RTC and
// especially the MCR, are still accessible in ROM0, ROM1 and BOOTSTRAP modes.   	       	   	 	       	     	  	
//
//   In this module the CMemoryMap class implements all the above address
// selection logic.  This class implements the CMemory interface, which is
// compatible with the CCPU emulation class as a memory device.  The memory
// control register is treated as its own independent single byte I/O device.
// This might be overkill - we could bury this in the CMemoryMap code after
// all - but it's convenient for the MCR to be a separate device.  This allows
// it to show up in the UI "SHOW CONFIGURATION" command as a device that you
// can examine the current state of, and for another it allows the MCR to have
// a Clear() function that's called automatically when the CPU is reset.
//
//   One final comment - there is no address mapping hardware. The MCR simply
// controls which RAM/EPROM chip is selected by which address range.  That means,
// for example, that address 0x0000 in the BOOT map will address the same EPROM
// location as 0x8000.  Likewise, the RAM1 address, say, 0xA123 in the MICRODOS
// or ELFOS maps becomes address 0x2123 in the ROM1 map. The EPROM firmware just
// has to deal with that.
//    
// REVISION HISTORY:
// 17-JUN-22  RLA   New file.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include <string>               // C++ std::string class, et al ...
#include "EMULIB.hpp"           // emulator library definitions
#include "SafeCRT.h"		// replacements for Microsoft "safe" CRT functions
#include "LogFile.hpp"          // emulator library message logging facility
#include "SBC1802.hpp"          // global declarations for this project
#include "MemoryTypes.h"        // address_t and word_t data types
#include "CDP1877.hpp"          // CDP1877 programmable interrupt controller
#include "CDP1879.hpp"          // CDP1879 real time clock
#include "Memory.hpp"           // CMemory and CGenericMemory declarations
#include "MemoryMap.hpp"        // declarations for this module
using std::string;              // too lazy to type "std::string..."!


CMemoryControl::CMemoryControl (uint16_t nPort, CCDP1877 *pPIC)
  : CDevice("MCR", "MCR", "Memory Control Register", INOUT, nPort)
{
  //++
  //   The MCR plays like a fairly normal, albeit simple, I/O device EXCEPT
  // that this constructor also requires a pointer to the CDP1877 programmable
  // interrupt controller.  That's because the MCR has a couple of extra bits
  // that control the master interrupt enable and also report the current
  // interrupt request status.
  //
  //    Those functions don't really have anything to do with memory control,
  // but the hardware had extra bits available in the MCR register.  What else
  // can we do?
  //--
  assert(pPIC != NULL);
  m_bMap = MCR_BOOT;  m_pPIC = pPIC;
}

CMemoryMap::CMemoryMap (CGenericMemory *pRAM, CGenericMemory *pROM, 
                        CMemoryControl *pMCR, CCDP1879 *pRTC, CCDP1877 *pPIC)
{
  //++
  //   The constructor for CMemoryMap assembles and remembers all the
  // components required for the memory subsystem, including two separate
  // CGenericMemory spaces, one for RAM and one for EPROM.  It also needs
  // to know about the three memory mapped devices - the real time clock,
  // the programmable interrupt controller, and the memory control register
  // itself.  We just remember pointers to all these objects so that we can
  // access them later when required.
  //--
  assert((pRAM != NULL)  &&  (pROM != NULL));
  assert((pRAM->Base() == RAMBASE)  &&  (pRAM->Size() == RAMSIZE));
  assert((pROM->Base() == ROMBASE)  &&  (pROM->Size() == ROMSIZE));
  assert((pMCR != NULL) && (pRTC != NULL)  &&  (pPIC != NULL));
  m_pRAM = pRAM;  m_pROM = pROM;  m_pCPU = NULL;
  m_pMCR = pMCR;  m_pRTC = pRTC;  m_pPIC = pPIC;
}

const char *CMemoryControl::MapToString (uint8_t bMap)
{
  //++
  // Convert a memory map to a string, for debugging ...
  //--
  switch (bMap) {
    case MCR_BOOT: return "BOOT";
    case MCR_ROM0: return "ROM0";
    case MCR_ROM1: return "ROM1";
    case MCR_MDOS: return "MicroDOS";
    case MCR_ELOS: return "ElfOS";
    default:       return "Unknown";
  }
}

void CMemoryControl::ClearDevice()
{
  //++
  //   A reset clears the MCR (which selects BOOT mapping mode) and also
  // clears the master interrupt enable ...
  //--
  m_bMap = MCR_BOOT;  m_pPIC->SetMasterEnable(false);
}

word_t CMemoryControl::DevRead (address_t nPort)
{
  //++
  //   Read back the contents of the MCR.  The lower 3 bits are always exactly
  // whatever was last written to the MCR, however we'll have to query the PIC
  // to get the state of the master interrupt enable and the current interrupt
  // request.
  // 
  //   For the moment at least, the backup battery OK bit is always set.  Also
  // notice that the PIC interrupt request bit is inverted (1 -> NO interrupt
  // request).
  //--
  uint8_t bMap = (m_bMap & MCR_MASK) | MCR_BBOK;
  if (m_pPIC->GetMasterEnable())   bMap |= MCR_MIEN;
  //   Note that we don't want to use m_pPIC->IsRequested() here, because that
  // will always return false as long as the MIEN is cleared.  We've got to
  // actually check and see if an interrupt is pending.  Most importantly,
  // remember that the PIRQ bit IS INVERTED!!
  if (!(m_pPIC->FindInterrupt() > 0)) bMap |= MCR_PIRQ;
  LOGF(TRACE, "MCR read returns 0x%02X (map=%s, MIEN=%d, PIRQ=%d)", bMap, MapToString(bMap&7), ISSET(bMap, MCR_MIEN), ISSET(bMap, MCR_PIRQ));
  return bMap;
}

void CMemoryControl::DevWrite (address_t nPort, word_t bData)
{
  //++
  //   Writing the MCR just stores the least significant three bits in
  // the MCR register, however bit 7 sets or clears the master interrupt
  // enable for the CDP1877 PIC.
  //--
  assert(nPort == MCRBASE);
  m_bMap = bData & MCR_MASK;
  m_pPIC->SetMasterEnable(ISSET(bData, MCR_MIEN));
  LOGF(TRACE, "MCR write 0x%02X (map=%s, MIEN=%d)", bData, MapToString(m_bMap), m_pPIC->GetMasterEnable());
}

CMemoryMap::CHIP_SELECT CMemoryMap::ChipSelect (uint8_t bMap, address_t &a)
{
  //++
  //   This routine will figure out what device - RAM, ROM, RTC, PIC or
  // MCR - should be selected by a given memory address and the current
  // memory mapping mode selected by m_bMap.  This is exactly the function
  // of the memory GAL in the SBC1802 design, and the ultimate gold standard
  // for this behavior is the PLD source for that device.
  //
  //   Note that in some cases it's necessary to modify the address by
  // setting the MSB to a 1 (i.e. this "flips" addresses in the range
  // 0x0000 .. 0x7FFF up to 0x8000 .. 8xFFFF).  This is used in the BOOT
  // and ROM1 modes.  That's why the "a" parameter is passed by reference!
  //
  //   It's worth sparing a moment to think about the expense, in CPU time,
  // of this function.  After all, this function will be called for EVERY
  // SINGLE MEMORY access performed by the CPU emulation!  That's a lot, and
  // you could undoubtedly find ways to optimize this considerably.  It's
  // not really necessary though, because this implementation seems to be
  // "fast enough."  It's a testament to how fast modern PCs have become that
  // this is so...
  //--

  //   Any address in the first half of memory (i.e. less than $8000) is
  // RAM in the MICRODOS, ELFOS and ROM0 modes, and it's flipped to the
  // upper half of RAM in the ROM1 mode (this allows all of RAM to be
  // accessed, including the part that's "shadowed" in the other modes).
  // In BOOT mode, RAM is inaccessible and these addresses are redirected
  // to EPROM.
  if ((a & 0x8000) == 0) {
    if (bMap == CMemoryControl::MCR_BOOT) {
      a |= ROMBASE;  return CS_ROM;
    } else if (bMap == CMemoryControl::MCR_ROM1) {
      a |= 0x8000;  return CS_RAM;
    } else
      return CS_RAM;
  }

  //   Addresses from $8000 to $83FF form the MicroDOS "ROM hole"
  // in memory.  These are mapped to EPROM in all modes EXCEPT
  // ElfOS mode...
  if ((a & 0xFC00) == 0x8000) {
    return (bMap == CMemoryControl::MCR_ELOS) ? CS_RAM : CS_ROM;
  }

  //   The remaining space up to $EFFF is RAM in MICRODOS and ELFOS
  // modes, and EPROM in all other modes ...
  if ((a & 0xF000) != 0xF000) {
    return (   (bMap == CMemoryControl::MCR_ELOS)
            || (bMap == CMemoryControl::MCR_MDOS)) ? CS_RAM : CS_ROM;
  }

  //   All addresses $F000 and up are ALWAYS mapped to EPROM EXCEPT for
  // those on the page $FExx.  Those are mapped to either the monitor's
  // scratchpad RAM or to memory mapped I/O devices ...
  if ((a & 0xFF00) != DPBASE) return CS_ROM;

  // The first 224 bytes of page $FE00 are the scratchpad RAM ...
  if (LOBYTE(a) < DPSIZE) return CS_RAM;

  // The MCR, CDP1879 RTC and CDP1877 PIC are all memory mapped...
  if (a == MCRBASE) return CS_MCR;
  if ((a & 0xFFF8) == RTCBASE) return CS_RTC;
  if ((a & 0xFFF0) == PICBASE)
    return ((a & 0x000C) != 0xC) ? CS_PIC : CS_NONE;

  // And anything else (it's just a few bytes!) must be invalid ...
  return CS_NONE;
}

const char *CMemoryMap::ChipToString (CHIP_SELECT bChip)
{
  //++
  // Convert a chip select to a string, for debugging ...
  //--
  switch (bChip) {
    case CS_RAM:  return "RAM";
    case CS_ROM:  return "ROM";
    case CS_RTC:  return "RTC";
    case CS_PIC:  return "PIC";
    case CS_MCR:  return "MCR";
    case CS_NONE: return "NONE";
    default:      return "Unknown";
  }
}

word_t CMemoryMap::CPUread (address_t a) const
{
  //++
  //   This method is called for _every_ CPU memory read operation (yikes!).
  // It runs the memory mapping algorithm to figure out which address space
  // and chip should be selected, and then delegates the request to the
  // corresponding object.
  //--
  switch (ChipSelect(m_pMCR->GetMap(), a)) {
    case CS_ROM: return m_pROM->CPUread(a);
    case CS_RAM: return m_pRAM->CPUread(a);
    case CS_RTC: return m_pRTC->DevRead(a);
    case CS_PIC: return m_pPIC->DevRead(a);
    case CS_MCR: return m_pMCR->DevRead(a);
    default:
      LOGF(WARNING, "invalid memory reference to %04X", LOWORD(a));
      return 0;
  }
}

void CMemoryMap::CPUwrite (address_t a, word_t d)
{
  //++
  // The same idea as CPUread(), except this time write to a location...
  //--
  switch (ChipSelect(m_pMCR->GetMap(), a)) {
    case CS_ROM: m_pROM->CPUwrite(a, d);  break;
    case CS_RAM: m_pRAM->CPUwrite(a, d);  break;
    case CS_RTC: m_pRTC->DevWrite(a, d);  break;
    case CS_PIC: m_pPIC->DevWrite(a, d);  break;
    case CS_MCR: m_pMCR->DevWrite(a, d);  break;
    default:
      LOGF(WARNING, "invalid memory reference to %04X", LOWORD(a));
  }
}

bool CMemoryMap::IsBreak (address_t a) const
{
  //++
  //   Return TRUE if a breakpoint is set on the specified memory address.
  // This works only for RAM and EPROM, however it needs to be careful to
  // figure out which one of those two is currently selected first.  Break
  // points are not supported, and we always return FALSE, for I/O devices.
  // That also includes the PIC, which is unfortunate since we do actually
  // execute code (or at least a single LBR instruction) fetched from the
  // PIC, but you'll just have to deal with that.
  //--
  switch (ChipSelect(m_pMCR->GetMap(), a)) {
    case CS_ROM: return m_pROM->IsBreak(a);
    case CS_RAM: return m_pRAM->IsBreak(a);
    default: return false;
  }
}

bool CMemoryMap::IsIO (address_t a) const
{
  //++
  //   Return true if the specified (and mapped) address is an I/O device and
  // false if it is either RAM or EPROM.  This is pretty easy to figure out.
  //--
  switch (ChipSelect(m_pMCR->GetMap(), a)) {
    case CS_RTC: return true;
    case CS_PIC: return true;
    case CS_MCR: return true;
    default: return false;
  }
}

void CMemoryControl::ShowDevice(ostringstream &ofs) const
{
  //++
  // Dump the MCR status for debugging ...
  //--
  ofs << FormatString("Memory map 0x%02X (%s), master interrupts %s",
    m_bMap, MapToString(m_bMap), m_pPIC->GetMasterEnable() ? "ENABLED" : "disabled");
}
