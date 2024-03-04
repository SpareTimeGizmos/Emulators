//++
// IDE11.hpp -> SBCT11 IDE disk interface emulation
//
// LICENSE:
//    This file is part of the emulator library project.  SBCT11 is free
// software; you may redistribute it and/or modify it under the terms of
// the GNU Affero General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any
// later version.
//
//    SBCT11 is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License
// for more details.  You should have received a copy of the GNU Affero General
// Public License along with SBCT11.  If not, see http://www.gnu.org/licenses/.
//
// DESCRIPTION:
//   The SBCT11 implements a very simple ATA/IDE hardware interface (it doesn't
// really need to be complicated, since the drive does all the work!).  The
// standard ATA interface implements two blocks of eight registers each, and
// the interface has a separate select/enable signal for each one, called CS1FX
// and CS3FX.  The CS1FX register set is the primary one and the CS3FX register
// set is rarely used.  The entire ATA/IDE register set is mapped into the
// DCT11 I/O memory space.
//
//   In principle all ATA registers are 16 bits wide, however in reality only
// the data register uses all 16 bits and all the rest are limited to 8 bits.
// ATA presents a unique problem in that reading some ATA registers will clear
// certain bits once they're read - this presents a problem because the DCT11
// ALWAYS reads a memory location before writing to it.  It's impossible to
// write to a memory location without first reading it, and with ATA that can
// have unexpected side effects.  To work around this the SBCT11 sets aside
// two distinct address spaces for the ATA register set - the first address
// space being read/write and the second is write only.  The latter explicitly
// inhibits any read operations to defeat the "read before write" problem.
//
//   So all told we have 8 sixteen bit registers, times 2 for the CS1FX and
// CS3FX register sets, and then times 2 again for the read only and read/write
// address spaces.  That takes a total of 64 bytes of I/O page address space
// for the IDE interface.  None of the fancy ATA DMA modes are supported on the
// SBCT11.  Programmed I/O is the only data transfer mode possible.  Sorry!
// Interrupts however are implemented and can be used to interrupt the DCT11
// when the drive is ready for another command.
// 
// REVISION HISTORY:
// 20-JAN-20  RLA   New file.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include "EMULIB.hpp"           // emulator library definitions
#include "LogFile.hpp"          // emulator library message logging facility
#include "Interrupt.hpp"        // interrupt simulation logic
#include "EventQueue.hpp"       // I/O device event queue simulation
#include "MemoryTypes.h"        // address_t and word_t data types
#include "Memory.hpp"           // basic memory emulation declarations ...
#include "CPU.hpp"              // CCPU base class definitions
#include "Device.hpp"           // basic I/O device emulation declarations ...
#include "IDE.hpp"              // IDE/ATA disk emulation
#include "IDE11.hpp"            // declarations for this module


CIDE11::CIDE11 (address_t nPort, CEventQueue *pEvents)
  : CIDE("DISK", "IDE11", "IDE/ATA Disk Interface", nPort, PORT_COUNT, pEvents)
{
  //++
  //   We're emulating an 8 bit bus here, but the real SBCT11 has a 16 bit bus
  // and expects to transfer data a word at a time.  Turns out that everything
  // will work OK if we force the drive to use 8 bit mode.  We have to force it
  // beause the SBCT11 firmware isn't going to set the 8 bit feature on its own.
  //--
  CIDE::Set8BitMode(0, true);
  CIDE::Set8BitMode(1, true);
}

word_t CIDE11::DevRead (address_t nAddress)
{
  //++
  //   Read a byte from the IDE drive registers.  Should be easy, but there are
  // a few things to watch out for.  First, there's the whole write only address
  // mapping - reading there always returns zeros and doesn't talk to the drive
  // at all.
  // 
  //   Then notice that the real SBCT11 hardware inverts the states of the CS1FX
  // and CS3FX bits (i.e. addresses with the 000020 bit SET are to CS1FX, and
  // with 000020 CLEAR are to CS3FX).
  // 
  //   And lastly, references to the high order (odd) byte always return zeros
  // EXCEPT for the data register.  The real SBCT11 has a 16 bit data bus, and
  // reading both high and low bytes of the data register should work.
  //--
  assert(nAddress >= GetBasePort());
  address_t nRegister = nAddress - GetBasePort();
  assert(nRegister < PORT_COUNT);
  if (ISSET(nRegister, WRITE_ONLY)) return 0;
  nRegister ^= CS1FX;
  if (ISODD(nRegister)) {
    if (nRegister == DATA_REG+1) return CIDE::DevRead(DATA_REG);
    return 0;
  } else {
    return CIDE::DevRead((nRegister>>1) & 0xF);
  }
}

void CIDE11::DevWrite (address_t nAddress, word_t bData)
{
  //++
  //   Write to an IDE register.  Most of the same conditions mentioned in
  // Read() apply here too, except for the one about the write only address
  // space.  That's one's not a problem now...
  //--
  assert(nAddress >= GetBasePort());
  address_t nRegister = nAddress - GetBasePort();
  assert(nRegister < PORT_COUNT);
  nRegister ^= CS1FX;
  if (ISODD(nRegister)) {
    if (nRegister == DATA_REG+1) CIDE::DevWrite(DATA_REG, bData);
  } else {
    CIDE::DevWrite((nRegister >> 1) & 0xF, bData);
  }
}
