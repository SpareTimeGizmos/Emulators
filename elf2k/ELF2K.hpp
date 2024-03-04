//++
// ELF2K.hpp -> Global Declarations for the ELF2K Emulator project
//
//   COPYRIGHT (C) 2015-2020 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
//
// LICENSE:
//    This file is part of the ELF2K emulator project.  EMULIB is free
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
//   This file contains global constants, universal macros, and a very few
// global objects ...
//
// REVISION HISTORY:
// 23-Jul-19  RLA   New file.
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...

// Program name and version ...
#define PROGRAM          "elf"  // used in prompts and error messages 
#define ELFVER               1  // version number of this release

// ELF2K memory configuration ...
#define RAMSIZE 32768UL           // number of bytes in RAM
#define ROMSIZE 32768UL           //    "    "   "    " ROM
#define MEMSIZE (RAMSIZE+ROMSIZE) // total memory space size, in bytes
#define RAMBASE 0                 // starting address of RAM
#define ROMBASE (RAMSIZE)         // starting address of ROM
//RAMEND     = RAMSIZE-1,   // ending     "  "  "   "
//ROMEND     = MEMSIZE-1,   // ending     "  "  "   "

// ELF2K I/O configuration ...
#define PORT_VIDEO          1     // CDP1861 and 80 column video
#define PORT_DISK_UART_RTC  2     // Disk/UART/RTC board base port
//                          3     // Disk/UART/RTC secondary port
#define PORT_POST           4     // POST display (output only)
#define PORT_SWITCHES       4     // Switches (input only)
#define PORT_VIDEO_80       5     // 80 column video secondary port
#define PORT_PORT_GPIO      6     // GPIO/PS2 keyboard base port
//#define PORT_CDP1854      2     // Microboard CDP1854 port
//                          7     // GPIO/PS2 secondary port

// Console, log file and command parser objects.
extern class CSmartConsole   *g_pConsole;     // console window (with file transfer!)
//extern class CLog            *g_pLog;         // message logging object (including console!)
//extern class CCmdParser      *g_pParser;      // command line parser

//   These pointers reference the major parts of the ELF2K system being
// emulated - CPU, memory, switches, display, peripherals, etc.  They're all
// declared in the ELF2K cpp file and are used by the UI to implement various
// commands (e.g. "SET UART ...", "SHOW POST ...", "SET SWITCHES ...", etc).
extern class CCOSMAC         *g_pCPU;         // 1802 COSMAC CPU
extern class CEventQueue     *g_pEvents;      // list of things to do
extern class CGenericMemory  *g_pMemory;      // memory emulation
extern class CTIL311         *g_pTIL311;      // ELF2K POST display
extern class CSwitches       *g_pSwitches;    // ELF2K toggle switches
extern class CUART           *g_pUART;        // generic UART (e.g. CDP1854)
extern class CDiskUARTrtc    *g_pDiskUARTrtc; // ELF2K Disk/UART/RTC card
extern class CSoftwareSerial *g_pSerial;      // software serial port
