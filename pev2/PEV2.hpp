//++
// PEV2.hpp -> Global Declarations for the PEV2 Emulator project
//
//   COPYRIGHT (C) 2015-2020 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
//
// LICENSE:
//    This file is part of the PicoElf emulator project.  PicoElf is free
// software; you may redistribute it and/or modify it under the terms of
// the GNU Affero General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any
// later version.
//
//    PicoElf is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License
// for more details.  You should have received a copy of the GNU Affero General
// Public License along with PicoElf. If not, see http://www.gnu.org/licenses/.
//
// DESCRIPTION:
//   This file contains global constants, universal macros, and a very few
// global objects ...
//
// REVISION HISTORY:
// 28-JUL-22  RLA   Adapted from ELF2K.
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...

// Program name and version ...
#define PROGRAM      "PEV2"  // used in prompts and error messages 
#define PEV2VER           1  // version number of this release

// PEV2 memory configuration ...
#define RAMSIZE 32768UL           // number of bytes in RAM
#define ROMSIZE 32768UL           //    "    "   "    " ROM
#define MEMSIZE (RAMSIZE+ROMSIZE) // total memory space size, in bytes
#define RAMBASE 0                 // starting address of RAM
#define ROMBASE (RAMSIZE)         // starting address of ROM

// PEV2 standard I/O ports ...
#define PORT_COMBO      6         // UART/RTC board uses ports 6/7
#define PORT_POST       4         // TIL311 POST display (write only!)
#define PORT_IDE        2         // IDE uses ports 2/3

// PEV2 EF bit assignments ...
//#define EF_SERIAL     CCOSMAC::EF2// software serial input

// Console, log file and command parser objects.
extern class CConsoleWindow  *g_pConsole;     // console window object

//   These pointers reference the major parts of the PEV2 system being
// emulated - CPU, memory, POST display, peripherals, etc.
extern class CEventQueue     *g_pEvents;      // "to do" list of upcoming eventz
extern class CCOSMAC         *g_pCPU;         // 1802 COSMAC CPU
extern class CGenericMemory  *g_pMemory;      // memory emulation
extern class CTIL311         *g_pTIL311;      // POST display
extern class CElfDisk        *g_pIDE;         // IDE disk emulation
#ifdef EF_SERIAL
extern class CSoftwareSerial *g_pSerial;      // software serial port
#else
extern class CINS8250        *g_pUART;        // INS8250 console UART
extern class C12887          *g_pRTC;         // DS12887 RTC/NVR emulation
extern class CUARTrtc        *g_pCombo;       // UART/RTC expansion card
#endif
