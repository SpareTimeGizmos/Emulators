//++
// MemoryTypes.h -> Emulation dependent data types
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
//   This header defines data types for a memory address and for an
// addressible memory location.  In the system specific code it's fine
// to use uint8_t, uint16_t, uint32_t, or whatever as you wish, but in
// the shared modules we need definitions that change adapt to the current
// emulation.
//
// REVISION HISTORY:
// 19-JUN-22  RLA   New file.
// 26-AUG-22  RLA   Change register_t to cpureg_t (because stupid gcc has a
//                    built in definition for register_t!)
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...

//++
//   This type holds a memory address as implemented on this microprocessor.
// It is unfortunately not strictly generic (although most micros will have
// a 16 bit address!) and probably should be in the specific CPU module
// rather than here.  That would lead to a lot of complications, though,
// since many of the generic CPU routines need some concept of an address.
//
//   BTW, It's purposely limited to exactly 16 bits to ensure that address
// calculation overflows wrap around as expected!
//--
#ifndef ADDRESS_SIZE
#define ADDRESS_SIZE  16
#endif
#define ADDRESS_MASK  ((1<<ADDRESS_SIZE)-1)
#define ADDRESS_MAX   ((address_t) ADDRESS_MASK)
#define ADDRESS(x)    ((address_t) ((x) & ADDRESS_MASK))
typedef uint16_t address_t;


//++
//   And this type holds a single addressable memory location.  In MOST
// cases this will be a single byte.  That's true even for processors
// like the 8088 or PDP11/T11, because they are byte addressable 
// architectures.  There are exceptions, however.  The Nova/F9440 for
// example, is nominally a word addressable machine (yeah, I know that
// there are hacks for byte addressing, but...) so for that machine
// a word_t would be a uint16_t.  A better example would be the PDP-8,
// which is undeniably a word addressable, 12 bit machine.
//--
#ifndef WORD_SIZE
#define WORD_SIZE    8
#endif
#define WORD_MASK   ((1<<WORD_SIZE)-1)
#define WORD_MAX    ((word_t) WORD_MASK)
#define WORD(x)     ((word_t) ((x) & WORD_MASK))
#if (WORD_SIZE == 8)
typedef uint8_t word_t;
#else
typedef uint16_t word_t;
#endif


//++
//   The default radix for all messages is hexadecimal unless otherwise
// specified.  That covers everything except the PDP-11 and PDP-8!
//--
#ifndef RADIX
#define RADIX     16
#endif

//++
//   And likewise, this type holds the index of a CPU internal register.  It
// COULD be a simple address too (e.g. for the T11, where the registers are
// numbered 0..7).  Generally, though, CPU registers are not directly
// addressible and this type just holds a register index that's meaningful
// only to the emulator code.  The only real use for this type is as an
// argument for the GetRegister()/SetRegister() et al functions.
//--
typedef uint16_t cpureg_t;

//++
//   The uint4_t type is used internally for all four bit registers, and the
// uint1_t type is used for all single bit values.  You could use a bool for
// the latter, but semantically they are numeric values, not boolean.
// Both these types are expected to behave exactly the same as an 8 bit 
// value.  The emulation code is responsible for any masking required to
// ensure that any results values fit into 1 or 4 bits ...
//--
typedef uint8_t uint1_t;
typedef uint8_t uint2_t;
typedef uint8_t uint3_t;
typedef uint8_t uint4_t;
