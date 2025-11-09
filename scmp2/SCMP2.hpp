//++
// SCMP2.hpp -> Global Declarations for the SC/MP-II Emulator project
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
#define PROGRAM        "SCMP2"  // used in prompts and error messages 
#define SCMPVER             2   // version number of this release

// SC/MP memory configuration ...
#define RAMSIZE 32768UL           // number of bytes in RAM
#define ROMSIZE 32768UL           //    "    "   "    " ROM
#define MEMSIZE (RAMSIZE+ROMSIZE) // total memory space size, in bytes
#define RAMBASE 0                 // starting address of RAM
#define ROMBASE (RAMSIZE)         // starting address of ROM
//RAMEND     = RAMSIZE-1,   // ending     "  "  "   "
//ROMEND     = MEMSIZE-1,   // ending     "  "  "   "

// SC/MP I/O configuration ...

// Console, log file and command parser objects.
extern class CConsoleWindow  *g_pConsole;     // console window object

//   These pointers reference the major parts of the SC/MP system being
// emulated - CPU, memory, switches, display, peripherals, etc.  They're all
// declared in the SCMP cpp file and are used by the UI to implement various
// commands (e.g. "SET UART ...", "SHOW POST ...", "SET SWITCHES ...", etc).
extern class CSCMP2          *g_pCPU;         // INS8060 SC/MP-II CPU
extern class CEventQueue     *g_pEvents;      // event queue of things to do
extern class CGenericMemory  *g_pMemory;      // memory emulation
extern class CSoftwareSerial *g_pSerial;      // software serial port
