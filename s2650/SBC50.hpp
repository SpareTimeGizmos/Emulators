//++
// SBC50.hpp -> Global Declarations for the 2650 Emulator project
//
// DESCRIPTION:
//   This file contains global constants, universal macros, and a very few
// global objects ...
//
// REVISION HISTORY:
// 21-FEB-20  RLA   Stolen from the MCS85 project.
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...

// Program name and version ...
#define PROGRAM       "SBC50"   // used in prompts and error messages 
#define SBC50VER           2    // version number of this release

// SBC50 I/O ports ...
#define PORT_SLU0         0xF0  // console 2651 UART (2 ports!)

// Console, log file and command parser objects.
extern class CConsoleWindow  *g_pConsole;     // console window object

//   These pointers reference the major parts of the SBC50 system being
// emulated - CPU, memory, switches, display, peripherals, etc.  They're all
// declared in the SBC50 cpp file and are used by the UI to implement various
// commands (e.g. "SET UART ...", "SHOW POST ...", "SET SWITCHES ...", etc).
extern class CGenericMemory  *g_pMemory;      // memory emulation
extern class CEventQueue     *g_pEvents;      // list of things to do
extern class C2650           *g_pCPU;         // Signetics 2650 CPU
extern class CSoftwareSerial *g_pSerial;      // software serial port
