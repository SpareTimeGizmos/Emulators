//++
// SCMP.hpp -> Global Declarations for the SC/MP Emulator project
//
// DESCRIPTION:
//   This file contains global constants, universal macros, and a very few
// global objects ...
//
// REVISION HISTORY:
// 13-FEB-20  RLA   Stolen from the ELF2K project.
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...

// Program name and version ...
#define PROGRAM       "SCMP3"   // used in prompts and error messages 
#define SCMPVER             1   // version number of this release

// SC/MP memory configuration ...
#define RAMSIZE 32768UL           // number of bytes in RAM
#define ROMSIZE 32768UL           //    "    "   "    " ROM
#define MEMSIZE (RAMSIZE+ROMSIZE) // total memory space size, in bytes
#define RAMBASE 0                 // starting address of RAM
#define ROMBASE (RAMSIZE)         // starting address of ROM
//RAMEND     = RAMSIZE-1,   // ending     "  "  "   "
//ROMEND     = MEMSIZE-1,   // ending     "  "  "   "

// SC/MP I/O configuration ...
//#define PORT_VIDEO          1     // CDP1861 and 80 column video
//#define PORT_DISK_UART_RTC  2     // Disk/UART/RTC board base port
//                          3     // Disk/UART/RTC secondary port
//#define PORT_POST           4     // POST display (output only)
//#define PORT_SWITCHES       4     // Switches (input only)
//#define PORT_VIDEO_80       5     // 80 column video secondary port
//#define PORT_PORT_GPIO      6     // GPIO/PS2 keyboard base port
//#define PORT_CDP1854        2     // Microboard CDP1854 port
//                          7     // GPIO/PS2 secondary port

// Console, log file and command parser objects.
extern class CConsoleWindow  *g_pConsole;     // console window object

//   These pointers reference the major parts of the SC/MP system being
// emulated - CPU, memory, switches, display, peripherals, etc.  They're all
// declared in the SCMP cpp file and are used by the UI to implement various
// commands (e.g. "SET UART ...", "SHOW POST ...", "SET SWITCHES ...", etc).
extern class CSCMP3          *g_pCPU;         // INS8060 SC/MP-II CPU
extern class CEventQueue     *g_pEvents;      // event queue of things to do
extern class CGenericMemory  *g_pMemory;      // memory emulation
extern class CSoftwareSerial *g_pSerial;      // software serial port
