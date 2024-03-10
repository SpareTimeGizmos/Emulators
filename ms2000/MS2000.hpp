//++
// MS2000.hpp -> Global Declarations for the MS2000 Emulator project
//
//   COPYRIGHT (C) 2015-2024 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
//
// LICENSE:
//    This file is part of the MS2000 emulator project.  MS2000 is free
// software; you may redistribute it and/or modify it under the terms of
// the GNU Affero General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any
// later version.
//
//    MS2000 is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License
// for more details.  You should have received a copy of the GNU Affero General
// Public License along with MS2000.  If not, see http://www.gnu.org/licenses/.
//
// DESCRIPTION:
//   This file contains global constants, universal macros, and a very few
// global objects ...
//
// REVISION HISTORY:
//  5-MAR-24  RLA   New file..
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...

// Program name and version ...
#define PROGRAM       "ms2000"  // used in prompts and error messages 
#define MSVER                1  // version number of this release

// MS2000 memory configuration ...
#define MEMSIZE         65536UL   // total bytes in all memory
#define ROMSIZE          2048UL   // number of bytes in UT71 ROM
#define ROMBASE         0x8000    // starting address of ROM

// MS2000 I/O ports ...
#define TLIO_PORT       1         // I/O group select register
#define CDP18S605_GROUP 1         // CPU board group select
#define SLU_PORT        2         // console CDP1854 UART (2 ports!)
#define CDP18S651_GROUP 8         // floppy diskette controller group

// MS2000 EF assignments ...
#define SLU_IRQ_EF   CCOSMAC::EF3 // CDP1854 SLU interrupt request
#define SLU_BREAK_EF CCOSMAC::EF4 // CDP1854 SLU RXD (break detect!)
#define FDC_IRQ_EF   CCOSMAC::EF3 // CDP18S651 floppy disk interrupt
#define FDC_MOTOR_EF CCOSMAC::EF1 // CDP18S651 selected motor on

//   These pointers reference the major parts of the MS2000 system being
// emulated - CPU, memory, switches, display, peripherals, etc.  They're all
// declared in the MS2000 cpp file and are used by the UI to implement various
// commands...
extern class CConsoleWindow   *g_pConsole;    // console window object
extern class CEventQueue      *g_pEvents;     // "to do" list of upcoming eventz
extern class CCOSMAC          *g_pCPU;        // 1802 COSMAC CPU
extern class CSimpleInterrupt *p_pInterrupt;  // simple wire-OR interrupt system
extern class CGenericMemory   *g_pMemory;     // generic memory (RAM and ROM) emulation
extern class CTLIO            *g_pTLIO;       // RCA style two level I/O mapping
extern class CCDP1854         *g_pSLU;        // console UART
extern class C18S651          *g_pFDC;        // CDP18S651 floppy disk controller
