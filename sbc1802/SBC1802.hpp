//++
// SBC1802.hpp -> Global Declarations for the SBC1802 Emulator project
//
//   COPYRIGHT (C) 2015-2024 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
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
//   This file contains global constants, universal macros, and a very few
// global objects ...
//
// REVISION HISTORY:
// 16-JUN-22  RLA   Adapted from ELF2K.
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...

// Program name and version ...
#define PROGRAM      "sbc1802"  // used in prompts and error messages 
#define SBCVER               2  // version number of this release
#define CPUCLK    2500000UL     // CPU crystal frequency
#define BAUDCLK   4915200UL     // baud rate crystal frequency

// SBC1802 memory configuration ...
#define RAMSIZE         65536UL   // number of bytes in RAM
#define ROMSIZE         32768UL   //    "    "   "    " ROM
#define RAMBASE         0x0000    // starting address of RAM
#define ROMBASE         0x8000    // starting address of ROM

// SBC1802 memory mapped peripherals ...
#define DPBASE          0xFE00    // RAM page set aside for the firmware
#define DPSIZE          0x00E0    // size of the firmware data page
#define MCRBASE         0xFEE7    // memory mapping control register
#define RTCBASE         0xFEE8    // CDP1879 real time clock address
#define PICBASE         0xFEF0    // CDP1877 programmable interrupt controller

// SBC1802 base board standard I/O ports ...
#define BASE_GROUP      1         // base board I/O group
#define TLIO_PORT       1         // I/O group select register
#define SLU0_PORT       2         // console CDP1854 UART (2 ports!)
#define LEDS_PORT       4         // LED POST display (write only!)
#define SWITCHES_PORT   4         // DIP switchefs (read only!)
#define IDE_PORT        5         // IDE interface (2 ports!)
#define BAUD_PORT       7         // SLU0 and SLU1 baud rate generator

// SBC1802 base board EF bit assignments ...
//   IMPORTANT - for MicroDOS compatibility, the SLU0 BREAK EF needs to
// be EF4 and the INPUT_EF needs to be EF1!!
#define SLU0_BREAK_EF CCOSMAC::EF4// CDP1854 SLU0 RXD (break detect!)
#define SLU0_IRQ_EF   CCOSMAC::EF3// CDP1854 SLU0 interrupt request
#define RTC_IRQ_EF    CCOSMAC::EF2// CDP1879 RTC interrupt request
#define INPUT_EF      CCOSMAC::EF1// INPUT/ATTENTION button

// EXP1802 expansion board ports and groups ...
#define SLU1_GROUP      4         // SLU1 group (shared with MDU)
#define MDU_GROUP       4         // CDP1855 group (shared with SLU1!)
#define PPI_GROUP       5         // CDP1851 PPI group
#define TIMER_GROUP     6         // CDP1878 timer/counter group
#define PSG_GROUP       7         // AY-3-8912 sound generator group
#define TIMER_PORT      2         // CDP1878 timer/counter base port
#define SLU1_PORT       2         // CDP1854 base port
#define MDU_PORT        4         // CDP1855 multiply/divide unit group
#define PPI_PORT        2         // CDP1851 PPI base port
#define PSG1_PORT       2         // AY-3-8192 PSG#1 base port
#define PSG2_PORT       6         // AY-3-8192 PSG#2 base port

// EXP1802 expansion board EF bit assignments ...
#define TIMER_IRQ_EF  CCOSMAC::EF3// CDP1878 timer counter IRQ
#define MDU_OVF_EF    CCOSMAC::EF2// CDP1855 multiply/divide overflow flag
#define SLU1_IRQ_EF   CCOSMAC::EF3// CDP1854 SLU1 interrupt request
#define SLU1_SID_EF   CCOSMAC::EF4// SLU1 serial input data
#define PPI_ARDY_EF   CCOSMAC::EF1// PPI port A data ready
#define PPI_BRDY_EF   CCOSMAC::EF2// PPI port B data ready
#define PPI_IRQ_EF    CCOSMAC::EF3// PPI IRQ

// CDP1877 interrupt levels assignments ...
#define IRQ_INPUT   CCDP1877::IRQ0// INPUT/ATTENTION button
#define IRQ_RTC     CCDP1877::IRQ1// CDP1879 real time clock
                                  // note that IRQ2 is unused
#define IRQ_DISK    CCDP1877::IRQ3// IDE disk controller
#define IRQ_SLU0    CCDP1877::IRQ4// CDP1854 console serial SLU0
#define IRQ_TIMER   CCDP1877::IRQ5// CDP1878 timer
#define IRQ_PPI     CCDP1877::IRQ6// CDP1851 parallel interface
#define IRQ_SLU1    CCDP1877::IRQ7// CDP1854 second serial SLU1

//   These pointers reference the major parts of the SBC1802 system being
// emulated - CPU, memory, switches, display, peripherals, etc.  They're all
// declared in the SBC1802 cpp file and are used by the UI to implement various
// commands (e.g. "SET UART ...", "SHOW POST ...", "SET SWITCHES ...", etc).
extern class CSmartConsole  *g_pConsole;      // console window object
extern class CEventQueue    *g_pEvents;       // "to do" list of upcoming eventz
extern class CCOSMAC        *g_pCPU;          // 1802 COSMAC CPU
extern class CGenericMemory *g_pRAM, *g_pROM; // SRAM and EPROM emulation
extern class CMemoryControl *g_pMCR;          // SBC1802 memory control register
extern class CMemoryMap     *g_pMemoryMap;    // SBC1802 memory mapping hardware
extern class CTLIO          *g_pTLIO;         // RCA style two level I/O mapping
extern class CLEDS          *g_pLEDS;         // SBC1802 7 segment LED display
extern class CSwitches      *g_pSwitches;     // SBC1802 DIP switches
extern class CBaud          *g_pBRG;          // baud rate generator for SLU0 & 1
extern class CCDP1854       *g_pSLU0;         // console UART
extern class CElfDisk       *g_pIDE;          // generic IDE disk interface
extern class CCDP1879       *g_pRTC;          // CDP1879 real time clock
extern class CCDP1877       *g_pPIC;          // CDP1877 programmable interrupt controller
// Extension board devices ...
extern class CCDP1854       *g_pSLU1;         // secondary UART (for TU58)
extern class CTU58          *g_pTU58;         // TU58 drive emulator
extern class CPSG           *g_pPSG1;         // AY-3-8912 programmable sound generator #1
extern class CPSG           *g_pPSG2;         // AY-3-8912 programmable sound generator #2
extern class CTwoPSGs       *g_pTwoPSGs;      // SBC1802 implementation of two PSGs
extern class CCDP1851       *g_pPPI;          // CDP1851 programmable I/O interface
extern class CCDP1878       *g_pCTC;          // CDP1878 counter/timer
