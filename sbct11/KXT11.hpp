//++
// KXT11.hpp -> Global Declarations for the T11/KXT11 Emulator project
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
#define PROGRAM     "KXT11"     // used in prompts and error messages 
#define KXTVER           1      // version number of this release

// Console, log file and command parser objects.
extern class CConsoleWindow  *g_pConsole;     // console window object

//   These pointers reference the major parts of the KXT11 system being
// emulated - CPU, memory, switches, display, peripherals, etc.  They're all
// declared in the KXT11 cpp file and are used by the UI to implement various
// commands (e.g. "SET UART ...", "SHOW POST ...", "SET SWITCHES ...", etc).
extern class CMemory         *g_pMemory;      // memory emulation
extern class CDCT11          *g_pCPU;         // DEC T11 CPU

