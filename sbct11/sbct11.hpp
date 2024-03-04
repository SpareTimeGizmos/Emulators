//++
// SBCT11.hpp -> Global Declarations for the T11/KXT11 Emulator project
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
//   This file contains global constants, universal macros, and a very few
// global objects ...
//
// REVISION HISTORY:
//  4-MAR-20  RLA   Stolen from the MCS85 project.
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...

// Program name and version ...
#define PROGRAM      "T11"    // used in prompts and error messages 
#define T11VER           3    // version number of this release

// SBCT11 memory configuration ...
#define RAMSIZE     65536UL   // number of bytes in RAM
#define ROMSIZE     65536UL   //    "    "   "    " EPROM
#define ROM_BASE_0  0002000   // start of EPROM (when RAM is false)
#define ROM_BASE_1  0170000   // start of permanently mapped EPROM
#define ROMTOP      0175777   // last address assigned to EPROM
#define RAMTOP      0167777   // last address in RAM (when RAM is true)
#define START_1     0172000   // DCT11 primary start/restart address
#define START_2     0173000   //   "   secondary "     "       "
#define SCRATCH_RAM 0176000   // scratch RAM set aside for firmware use
#define SCRATCH_TOP 0176377   // last location in scratch RAM

// SBCT11 I/O device addresses ...
#define IOPAGE      0176400   // start of all I/O devices
#define IDE_BASE    0176400   // IDE disk drive base address
#define SLU0_BASE   0177560   // DC319 serial line 0 (console) base address
#define SLU1_BASE   0176500   //   "      "     "  1 (TU58)     "     "  "
#define SPARE_BASE  0176560   // spare I/O select address
#define PPI_BASE    0177420   // 82C55 peripheral interface
#define RTC_BASE    0177460   // DS12887A real time clock and NVR
#define SWITCH      0177524   // switch register address (not implemented)
#define FLAGS       0177540   // SBCT11 flags and status registers
#define MEMCSR      FLAGS+0   // memory control register (RAM/ROM mapping)
#define NXMCSR      FLAGS+2   // NXM trap emulation register
#define SPARE_CSR   FLAGS+4   // "spare" flag/sense register
#define LTCCSR      FLAGS+6   // Line time clock control register

// SBCT11 interrupt (DCT11 CPx) assignments ...
#define LTC_IRQ          11   // line time clock   - CP11, BR6, vector 100
#define SLU1_RCV_IRQ      7   // TU58 receive      - CP7,  BR5, vector 120
#define SLU1_XMT_IRQ      6   // TU58 transmit     - CP6,  BR5, vector 124
#define PPI_IRQ           5   // 8255 PPI          - CP5,  BR5, vector 130
#define SPARE_IRQ         4   // spare (expansion) - CP4,  BR5, vector 134
#define SLU0_RCV_IRQ      3   // console receive   - CP3,  BR4  vector 060
#define SLU0_XMT_IRQ      2   // console transmit  - CP2,  BR4, vector 064
#define IDE_IRQ           1   // IDE disk          - CP1,  BR4, vector 070


// Console, log file and command parser objects.
extern class CSmartConsole   *g_pConsole;     // console window object

//   These pointers reference the major parts of the SBCT11 system being
// emulated - CPU, memory, switches, display, peripherals, etc.  They're all
// declared in the SBCT11 cpp file and are used by the UI to implement various
// commands (e.g. "SET UART ...", "SHOW POST ...", "SET SWITCHES ...", etc).
extern class CEventQueue     *g_pEvents;      // future things to do
extern class CPIC11          *g_pPIC;         // DCT11 interrupt controller
extern class CDCT11          *g_pCPU;         // DEC T11 CPU
extern class CMemoryMap      *g_pMMap;        // SBCT11 memory mapping hardware
extern class CGenericMemory  *g_pRAM, *g_pROM;// SRAM and EPROM emulation
extern class CDeviceMap      *g_pIOpage;      // PDP11 memory mapped I/O page
extern class CMemoryControl  *g_pMCR;         // MEMC/NXMCS registers
extern class CLTC11          *g_pLTC;         // line time clock
extern class CDC319          *g_pSLU0;        // console serial line
extern class CDC319          *g_pSLU1;        // TU58 serial port
extern class CRTC11          *g_pRTC;         // real time clock
extern class CPPI11          *g_pPPI;         // 8255 PPI and POST display
extern class CIDE11          *g_pIDE;         // IDE disk attachment
extern class CTU58           *g_pTU58;        // TU58 drive emulator
