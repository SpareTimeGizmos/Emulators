//++
// SBC6120.hpp -> Global Declarations for the SBC6120 Emulator project
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
//   This file contains global constants, universal macros, and a very few
// global objects ...
//
// REVISION HISTORY:
// 30-JUL-22  RLA   Adapted from ELF2K.
//--
#pragma once
#include <stdint.h>               // uint8_t, int32_t, and much more ...

// Program name and version ...
#define PROGRAM      "SBC6120"    // used in prompts and error messages 
#define SBCVER               1    // version number of this release

// SBC6120 memory configuration ...
#define EPROM_MEMORY_SIZE 32768UL // EPROM size, in 12 bit words
#define PANEL_MEMORY_SIZE 32768UL // main memory size
#define MAIN_MEMORY_SIZE  32768UL // control panel memory size

// SBC6120 peripherals ...
#define SLU_DEVICE_CODE     003   // console SLU IOTs 603x/604x ...
#define MMAP_DEVICE_CODE    040   // memory mapping IOTs 640x
#define MISC_DEVICE_CODE    041   // miscellaneous IOTs 641x
#define PANEL_DEVICE_CODE   043   // front panel IOTs 643x
#define POST_DEVICE_CODE    044   // POST display IOTs 644x
#define IDE_DEVICE_CODE     047   // IDE/PPI IOTs 647x

// Console, log file and command parser objects.
extern class CConsoleWindow  *g_pConsole;     // console window object

//   These pointers reference the major parts of the SBC6120 system being
// emulated - CPU, memory, switches, display, peripherals, etc.  They're all
// declared in the SBC6120 cpp file and are used by the UI to implement various
// commands (e.g. "SET UART ...", "SHOW POST ...", "SET SWITCHES ...", etc).
extern class CEventQueue      *g_pEvents;         // "to do" list of upcoming events
extern class CSimpleInterrupt *g_pPanelInterrupt; // control panel interrupt sources
extern class CSimpleInterrupt *g_pMainInterrupt;  // conventional "wire-or" interrupt system
extern class C6120          *g_pCPU;              // HD6120 CPU
// The SBC6120 has no less than three separate memory spaces.
extern class CGenericMemory   *g_pMainMemory;     // 32kW main PDP-8 memory
extern class CGenericMemory   *g_pPanelMemory;    // 32kW 6120 control panel memory
extern class CGenericMemory   *g_pEPROM;          // 32kW SBC6120 EPROM space
extern class CRAMdisk         *g_pRAMdisk;        // SBC6120 RAM disk memory
extern class CMemoryMap       *g_pMemoryMap;      // SBC6120 memory map hardware
extern class CPOST            *g_pPOST;           // three LED POST code display
extern class CSLU             *g_pSLU;            // console serial line
extern class CIOT641x         *g_pIOT641x;        // miscellaneous 641x IOTs
extern class CIOT643x         *g_pIOT643x;        // FP6120 front panel IOTs
extern class CIDEdisk         *g_pIDEdisk;        // IDE disk interface
