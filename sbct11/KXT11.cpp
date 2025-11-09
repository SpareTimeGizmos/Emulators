//++
// KXT11.cpp - DEC DCT11/KXT11 Emulator main program
//
// DESCRIPTION:
//
// LIMITATIONS (aka the TO DO list)
//
//                                  Bob Armstrong [4-MAR-20]
//
// REVISION HISTORY:
//  4-MAR-20  RLA   New file.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include "EMULIB.hpp"           // emulator library definitions
#include "ConsoleWindow.hpp"    // emulator console window methods
#include "LogFile.hpp"          // emulator library message logging facility
#include "ImageFile.hpp"        // emulator library image file methods
#include "CommandParser.hpp"    // emulator library command line parsing methods
#include "CommandLine.hpp"      // emulator library shell (argc/argv) parser methods
#include "StandardUI.hpp"       // emulator library standard UI commands
#include "KXT11.hpp"            // global declarations for this project
#include "Memory.hpp"           // main memory emulation
#include "CPU.hpp"              // CCPU base class definitions
#include "DCT11.hpp"            // T11 CPU emulation
#include "Device.hpp"           // CDevice I/O device emulation objects
#include "UserInterface.hpp"    // User interface parse table definitions


// Global objects ....
//   These objects are used (more or less) everywhere within this program, and
// you'll find "extern ..." declarations for them in KXT11.hpp.  Note that they
// are declared as pointers rather than the actual objects because we want
// to control the exact order in which they're created and destroyed!
CConsoleWindow  *g_pConsole     = NULL; // console window object
CLog            *g_pLog         = NULL; // message logging object (including console!)
CCmdParser      *g_pParser      = NULL; // command line parser
// These globals point to the objects being emulated ...
CMemory         *g_pMemory      = NULL; // memory emulation
CDCT11          *g_pCPU         = NULL; // T11 CPU


static bool ConfirmExit (CCmdParser &cmd)
{
  //++
  //   This routine is called whenever this application has been requested
  // to exit.  It returns true if we really should exit and false if we
  // shouldn't right now.
  //--
  return true;
}


void main (int argc, char *argv[])
{
  //++
  // Here's the main program for the KXT11 Emulator .
  //--
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
  _CrtSetBreakAlloc(184);
  _crtBreakAlloc = 184;

  //   The very first thing is to create and initialize the console window
  // object, and after that we create and initialize the log object.  We
  // can't issue any error messages until we done these two things!
  g_pConsole = DBGNEW CConsoleWindow();
  g_pLog = DBGNEW CLog(PROGRAM, g_pConsole);

  //   Parse the command options.  Note that we want to do this BEFORE we
  // setup the console window, since the command line may tell us to detach
  // and create a new window...
  if (!CStandardUI::ParseOptions(PROGRAM, argc, argv)) goto shutdown;

  //   Set the console window defaults - foreground and background color,
  // scrolling buffer size, title, and icon ...
  g_pConsole->SetTitle("KXT11 Emulator v%d", KXTVER);
  g_pConsole->SetBufferSize(132, 2000);
  g_pConsole->SetWindowSize(132, 40);
  g_pConsole->SetColors(CConsoleWindow::GREEN, CConsoleWindow::BLACK);
  g_pLog->SetDefaultConsoleLevel(CLog::WARNING);

  // We're finally ready to say hello ...
  CMDOUTF("KXT11 Emulator v%d emulator Library v%d", KXTVER, EMUVER);
  CMDOUTS("Built on " << __DATE__ << " " << __TIME__);

  // Create the emulated CPU, memory and peripheral devices ...
  g_pMemory = DBGNEW CMemory(CDCT11::MAXMEMORY);
  g_pCPU = DBGNEW CDCT11(*g_pMemory);

  //   Lastly, create the command line parser.  If a startup script was
  // specified on the command line, now is the time to execute it...
  g_pParser = DBGNEW CCmdParser(PROGRAM, CUI::g_aVerbs, &ConfirmExit, g_pConsole);
  if (!CStandardUI::g_sStartupScript.empty())
    g_pParser->OpenScript(CStandardUI::g_sStartupScript);

  //   This thread now becomes the background task, which loops forever
  // executing operator commands.  Well, almost forever - when the operator
  // types "EXIT" or "QUIT", the command parser exits and then we shutdown
  // the KXT11 program.
  g_pParser->CommandLoop();
  LOGS(DEBUG, "command parser exited");

shutdown:
  // Delete all our global objects.  Once again, the order here is important!
  delete g_pCPU;      // the CPU
  delete g_pMemory;   // the memory object
  delete g_pParser;   // the command line parser can go away first
  delete g_pLog;      // close the log file
  delete g_pConsole;  // lastly (always lastly!) close the console window
#ifdef _DEBUG
//system("pause");
  _CrtDumpMemoryLeaks();
#endif
  exit(0);
}
