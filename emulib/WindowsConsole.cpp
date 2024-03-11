//++
// WindowsConsole.cpp -> CConsoleWindow implementation for Windows/WIN32
//
//   COPYRIGHT (C) 2015-2020 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
//
// LICENSE:
//    This file is part of the emulator library project.  EMULIB is free
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
// NOTE:
//   This file implements the CConsoleWindow class for the Microsoft Windows
// platform.  The Linux version is implemented by the LinuxWindow.cpp file.
// Both share the same ConsoleWindow.hpp header and class definitions.
//
// DESCRIPTION:
//   This class implements an interface to the WIN32 console window.  In the
// beginning we just did fgets() to read commands and fputs() to print messages
// and that was pretty much all there was. But as with all things life got more
// complicated as we went on, and there were requests to do WIN32 specific
// console things like set the window title, change the window size and/or
// reposition it on the screen and, ugliest of all, trap control-C and Windows
// Shutdown and LogOff events.  
//
//   Hence we now have this class - one of the first things the main program
// does is to create a CConsoleWindow object and from then on it takes over
// management of the console window, including all I/O to the window.  Note
// that it's assumed that the application already has a console window and
// creating an instance of this class simply attaches to that existing window.
// It DOES NOT CREATE A NEW CONSOLE WINDOW!  That might be a useful feature
// some day, but not today.
//
//   IMPORTANT - because of this it is intended that there be only one
// CConsoleWindow instance per application, ever, and this class follows a
// somewhat modified Singleton design pattern.  It's modified because we want
// to allow the constructor to be explicitly called, but only once.  Subsequent
// calls to the constructor will generate assertion failures, and a pointer to
// the original CConsoleWindow object can be retrieved at any time by calling
// CConsoleWindow::GetConsole().
//
//   Much of the code in this class is devoted to trapping and handling
// Control-C, Control-BREAK, console window close, user logoff and system
// shutdown events.
//
//   * Control-C simply erases the current command line, if any, and returns a
//     new command prompt.  It most certainly DOES NOT exit the program.
//
//   * Control-BREAK exits immediately if no devices are attached.  If one or
//     more devices are attached, then the operator is asked for confirmation
//     and has the option to change his mind. This is exactly the same behavior
//     as the EXIT command.
//
//   * Closing the window (either by Alt-F4 or by clicking the "X" close icon
//     on the upper right of the title bar), logging off the current user, or
//     shutting down windows, all immediately clean up and exit gracefully.  
//
//   Lastly, why go to all the trouble of having a special class for this?
// Well, two reasons - 1) it isolates all the WIN32 specific stuff so that the
// Linux port woll be easier. More importantly, 2) it eliminates the need for
// other source files to include windows.h, which pulls in tons of baggage
// along with it.
//
// Bob Armstrong <bob@jfcl.com>   [11-JUN-2015]
//
// REVISION HISTORY:
// 11-JUN-15  RLA   New file.
// 12-JUN-15  RLA   Add break/shutdown/etc handling ...
//  1-JUL-15  RLA   Add SetConsoleIcon() ...
// 21-OCT-15  RLA   Add SetColors() ...
// 22-OCT-15  RLA   Add Get/Set window size & position, buffer size ...
// 24-OCT-15  RLA   Add CreateNewConsole() and AttachNewConsole(), and friends
// 28-FEB-17  RLA   Make 64 bit clean.
// 29-JAN-20  RLA   Clear mode PROCESSED_INPUT in ReadKey()
// 18-JUL-22  RLA   Change console raw I/O to RawRead() and RawWrite().
// 21-JUL-22  RLA   Make RawWrite suppress non-ASCII characters and nulls
// 26-AUG-22  RLA   Clean up Linux/WIN32 conditionals.
// 17-NOV-23  RLA   Move the break character handling (^E) to this module.
// 20-NOV-23  RLA   Add keyboard buffer and make IsConsoleBreak() read ahead
//                  DON'T call RawWrite() from Write() ...
// 10-MAR-24  RLA   Add Serial Break functions for RCA MS2000.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#if defined(_WIN32)                   // THIS ENTIRE FILE IS ONLY FOR MSWINDOWS!
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include <string.h>             // strcpy(), strerror(), etc ...
#include <cstring>              // needed for memset()
#include <windows.h>            // WIN32 API functions and declarations ...
#include <process.h>            // needed for _beginthread(), et al ...
#include "EMULIB.hpp"           // emulator library definitions
#include "VirtualConsole.hpp"   // CVirtualConsole abstract class
#include "ConsoleWindow.hpp"    // declarations for this module
#include "LogFile.hpp"          // message logging facility


// Initialize the pointer to the one and only CConsoleWindow instance ...
CConsoleWindow *CConsoleWindow::m_pConsole = NULL;


void CConsoleWindow::SendConsoleKey (char chKey, uint16_t vkKey, bool fControl)
{
  //++
  //   This routine will jam a fake keystroke into the console input buffer.  
  // It could be used for all kinds of things, but the only thing it's really
  // used for is to force an ENTER key into the console buffer when we detect
  // a window close, logoff or system shutdown event.  Why??  Because when
  // these events happen the main UI thread is almost certainly blocked in the
  // ReadConsole() method, and stupid as it sounds, this is the only way I
  // know to force it to wake up!
  //--
  INPUT_RECORD ir[2];  DWORD dwTmp;

  // Create the key down event ...
  ir[0].EventType = KEY_EVENT;
  ir[0].Event.KeyEvent.bKeyDown = true;
  ir[0].Event.KeyEvent.dwControlKeyState = fControl ? 1 : 0;
  ir[0].Event.KeyEvent.uChar.UnicodeChar = chKey;
  ir[0].Event.KeyEvent.wRepeatCount = 1;
  ir[0].Event.KeyEvent.wVirtualKeyCode = vkKey;
  ir[0].Event.KeyEvent.wVirtualScanCode = MapVirtualKey(vkKey, MAPVK_VK_TO_VSC);

  // And then the key up event ...
  ir[1].EventType = KEY_EVENT;
  ir[1].Event.KeyEvent.bKeyDown = false;
  ir[1].Event.KeyEvent.dwControlKeyState = fControl ? 1 : 0;
  ir[1].Event.KeyEvent.uChar.UnicodeChar = chKey;
  ir[1].Event.KeyEvent.wRepeatCount = 1;
  ir[1].Event.KeyEvent.wVirtualKeyCode = vkKey;
  ir[1].Event.KeyEvent.wVirtualScanCode = MapVirtualKey(vkKey, MAPVK_VK_TO_VSC);

  // Jam it in the buffer and we're done ...
  WriteConsoleInput(m_hInput, ir, 2, &dwTmp);
}

void CConsoleWindow::HandleWindowClosed()
{
  //++
  //   This routine is called when we detect that the console window is about
  // to be closed.  It sets the forced shutdown flag (because the user has no
  // choice at this point), wakes up the UI thread, and then goes to sleep.
  // See the comments in ConsoleControlHandler() for more explanation.
  //--
  WriteLine();  LOGF(ERROR, "console window closed");
  SetSystemShutdown();  SendConsoleKey();  Sleep(5000);
}

void CConsoleWindow::HandleSystemShutdown()
{
  //++
  //   This routine is called when we detect that the current user is logging
  // off (which means that this application is about to be closed!) or when
  // the system is shutting down (which means the same thing as far as we're
  // concerned!).  It sets the forced shutdown flag (because the user has no
  // choice at this point), wakes up the UI thread, and then goes to sleep.
  // See the comments in ConsoleControlHandler() for more explanation.
  //--
  WriteLine();  LOGF(ERROR, "user logoff or system shutdown");
  SetSystemShutdown();  SendConsoleKey();  Sleep(5000);
}

BOOL WINAPI ConsoleControlHandler (DWORD dwCtrlType)
{
  //++
  //   This callback is invoked by Windows when the console window receives a
  // Control-C, a Control-Break, or the window is closed.  It _may_ be called
  // when the user logs off, but that apparently is not reliable and depends
  // on which Windows version you're running.  The documentation also says that
  // this routine can be called for a Windows Shutdown but that, unfortunately,
  // applies only to services and doesn't happen for applications.
  //
  //   Regardless, our only goal here is to make this application exit and the
  // way to do that is to trick our ReadLine() method into returning false.
  // The command scanner sees that as an end of file on the console and will
  // gracefully shut down.
  //
  //   There's one rather nasty catch in this plan, though.  Windows actually
  // creates a separate thread for invoking this handler, and that gives us
  // two problems.  One is that the main UI thread is probably still blocked
  // in ReadLine() waiting for console input, and we have to find some way to
  // get it going again.  The only way to do that is to force an end of line
  // into the console input buffer by calling SendConsoleControlC().
  //
  //   The other problem is that Windows assumes the cleanup is done in this
  // handler and once this handler returns Windows assumes it's free to kill
  // the entire process.  That doesn't work for us at all because we're doing
  // the cleanup in the main UI thread, and this routine does very little.
  // The hack for getting around that is to just make this handler sleep after
  // it wakes the UI task and that ensures that the UI task has time to exit.
  // Note that it's not necessary for this routine to ever actually return -
  // when the whole process exits this thread will be automatically killed.
  // Ugly, but it gets the job done.
  //
  //   And one last complaint (while I'm on a roll!) - for a callback routine
  // like this I would normally like to pass the address of the associated
  // CConsoleWindow object in the callback's lParam.  That makes it possible
  // for the callback method to find the object it should work on, but wait -
  // this callback doesn't have any user defined parameter!!  Way to go,
  // Microsoft!  There's no choice except to use the global g_pConsole
  // pointer to find out CConsoleWindow object.  Really, really, ugly!
  //--
  switch (dwCtrlType) {
    case CTRL_C_EVENT:
      //   Control-C does NOT cause the program to exit - it simply erases any
      // partial command line that might have been typed and then prints a new
      // command scanner prompt.  Note that we don't have to do anything to
      // wake up the UI thread since ^C will unblock any console read.
      CConsoleWindow::GetConsole()->WriteLine("^C");  return true;
    case CTRL_BREAK_EVENT:
      //   Control-BREAK causes the program to exit politely (without the forced
      // shutdown flag).  Once again, we don't have to worry about waking up the
      // UI thread since ^BREAK will unblock any console read in progress.
      CConsoleWindow::GetConsole()->WriteLine("^C");
      LOGF(ERROR, "Control-BREAK received");
      CConsoleWindow::GetConsole()->SetForcedExit();  Sleep(5000);  return true;
    case CTRL_CLOSE_EVENT:
      //   Closing the console window forces an immediate exit without any
      // opportunity for user intervention ...
      CConsoleWindow::GetConsole()->HandleWindowClosed();  return true;
    case CTRL_LOGOFF_EVENT:
    case CTRL_SHUTDOWN_EVENT:
      //  Ditto for system shutdown or user log off ...
      CConsoleWindow::GetConsole()->HandleSystemShutdown();  return true;
    default:
      // Everything else (I don't think there is anything else!) we ignore.
      return false;
  }
}

static LRESULT CALLBACK InvisibleWindowProcedure (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  //++
  //   Unfortunately, the ConsoleControlHandler() won't catch user logoff or 
  // system shutdown events under Windows 7 or 8.  This is a known problem,
  // see: 
  //
  //  https://social.msdn.microsoft.com/Forums/windowsdesktop/en-US/abf09824-4e4c-4f2c-ae1e-5981f06c9c6e/windows-7-console-application-has-no-way-of-trapping-logoffshutdown-event?forum=windowscompatibility
  //
  //   The only solution, ugly as it is, is to create an invisible window that
  // does nothing except to trap WM_ENDSESSION messages.  That's what this
  // code does, and this is the WNDPROC for that window!
  //--
  switch (msg) {
    case WM_ENDSESSION:
      //   System shutdown or user logoff (the lParam actually tells us
      // which, but we just don't care!)
      CConsoleWindow::GetConsole()->HandleSystemShutdown();
      return TRUE;

    case WM_CLOSE:
      //   This window should never close (it's invisible, after all!) but
      // if it does, handle it like closing the console window...
      CConsoleWindow::GetConsole()->HandleWindowClosed();
      return TRUE;

    case WM_DESTROY:
      PostQuitMessage(0);
      return TRUE;
    default:
      return DefWindowProc(hwnd, msg, wParam, lParam);
  }
}

static DWORD WINAPI InvisibleWindowThread (LPVOID lpParam)
{
  //++
  //   And this is the background thread for the invisible window.  It creates
  // the invisible window class, then creates the invisible window, and finally
  // runs the message loop for the window.  Notice that it has no way to exit
  // gracefully - the CConsoleWindow destructor just kills it when we're done.
  //--
  MSG msg;  HWND hwnd;  WNDCLASS wc;

  // Initialize and register the invisible window class ...
  memset(&wc, 0, sizeof(wc));
  wc.lpfnWndProc = (WNDPROC) InvisibleWindowProcedure;
  wc.hInstance = GetModuleHandle(NULL);
  wc.hIcon = LoadIcon(GetModuleHandle(NULL), INVISIBLE_WINDOW_ICON);
  wc.lpszClassName = INVISIBLE_WINDOW_CLASS;
  if (RegisterClass(&wc) == 0) {
    // Don't fret if we've been here before!
    assert(GetLastError() == ERROR_CLASS_ALREADY_EXISTS);
  }

  // And then create the invisible window ...
  hwnd = CreateWindowEx(
    0, INVISIBLE_WINDOW_CLASS, INVISIBLE_WINDOW_TITLE, WS_OVERLAPPEDWINDOW,
    CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
    (HWND) NULL, (HMENU) NULL, GetModuleHandle(NULL), (LPVOID) NULL
  );
  assert(hwnd != NULL);

  // Spend the rest of our time running the message pump for this window ...
  while (GetMessage(&msg, (HWND) NULL, 0, 0)) {
    TranslateMessage(&msg);  DispatchMessage(&msg);
  }
  return 0;
}

void CConsoleWindow::BeginInvisibleThread()
{
  //++
  // Start the invisible window thread running ...
  //--
  DWORD tid;
  m_hInvisibleThread = CreateThread(NULL, 0, InvisibleWindowThread, (LPVOID) this, 0, &tid);
  assert(m_hInvisibleThread != NULL);
}

void CConsoleWindow::EndInvisibleThread()
{
  //++
  // Terminate (rather ungracefully!) the invisible window thread ...
  //--
  if (m_hInvisibleThread != NULL) CloseHandle((HANDLE) m_hInvisibleThread);
  m_hInvisibleThread = NULL;
}

void CConsoleWindow::AttachCurrentConsole()
{
  //++
  //   This method will connect this class instance to the current console
  // window.  It initializes all class members, assigns handles for console
  // input and output, gets the console window handle, sets the console mode,
  // and establishes the trap handler for ^C, ^BREAK and shutdown events.
  //
  //   This method really only gets called in two cases - one, when this object
  // is created, and two, if this object is attached to a new (and different)
  // console window.
  //--

  // Initialize the members ...
  m_fForceExit = m_fSystemShutdown = m_fConsoleBreak = m_fSerialBreak = false;
  m_hInvisibleThread = NULL;  m_hInput = NULL;  m_hOutput = NULL;  m_hWindow = NULL;
  m_dwOriginalMode = 0;  m_bOriginalForeground = m_bOriginalBackground = BLACK;
  m_wOriginalWindowWidth = m_wOriginalWindowHeight = 0;
  m_wOriginalBufferWidth = m_wOriginalBufferHeight = 0;
  m_KeyBuffer.Clear();

  // Open handles for the current window and the input and output buffers ...
  m_hWindow = GetConsoleWindow();
  assert(m_hWindow != NULL);
  m_hInput  = CreateFileW(L"CONIN$", GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
  m_hOutput = CreateFileW(L"CONOUT$", GENERIC_READ|GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
  assert((m_hInput != NULL) && (m_hOutput != NULL));

  // Save the current console mode and then set the modes we care about ...
  GetConsoleMode(m_hWindow, (DWORD *) &m_dwOriginalMode);
  SetMode(m_hOutput, ENABLE_PROCESSED_OUTPUT);   // interpret control characters on output
  SetMode(m_hOutput, ENABLE_WRAP_AT_EOL_OUTPUT); // just like it says!

  // Save the other important console characteristics ...
  GetBufferSize(m_wOriginalBufferWidth, m_wOriginalBufferHeight);
  GetWindowSize(m_wOriginalWindowWidth, m_wOriginalWindowHeight);
  GetColors(m_bOriginalForeground, m_bOriginalBackground);

  // Trap Control-C, Control-BREAK, window close, and system shutdown events ...
  SetConsoleCtrlHandler(&ConsoleControlHandler, true);
  BeginInvisibleThread();
}

void CConsoleWindow::DetachCurrentConsole()
{
  //++
  //   And this method disconnects this object from the current console.  it
  // closes the handles, restores the original console mode, and ends the trap
  // handler.
  //--

  //   We want to restore the original window and buffer size, but that's
  // harder than you might expect.  Suppose the user has increased the window
  // size - that increases the buffer size too.  We must restore the original
  // window size first because Windows won't allow the window to be larger
  // than the buffer.  
  //
  //   Fine - restore the window size first.  But since the buffer is still
  // big, scroll bars will appear on the window and that'll make the effective
  // window size smaller.  When we next restore the buffer size, the scroll
  // bars will go away but the window stays slightly shrunken!  Bummer.  What
  // are we to do?
  //
  //   Well, the easy and cheap way out is just to restore the window size
  // twice - once before restoring the buffer, and once again after.  Then the
  // window will regain its original size the second time around.
  if ((m_wOriginalWindowHeight != 0) && (m_wOriginalWindowWidth != 0))
    SetWindowSize(m_wOriginalWindowWidth, m_wOriginalWindowHeight);
  if ((m_wOriginalBufferHeight != 0) && (m_wOriginalBufferWidth != 0))
    SetBufferSize(m_wOriginalBufferWidth, m_wOriginalBufferHeight);
  if ((m_wOriginalWindowHeight != 0) && (m_wOriginalWindowWidth != 0))
    SetWindowSize(m_wOriginalWindowWidth, m_wOriginalWindowHeight);

  // Restore the colors and mode ...
  if (m_bOriginalForeground != m_bOriginalBackground)
    SetColors(m_bOriginalForeground, m_bOriginalBackground);
  if (m_dwOriginalMode != 0)
    SetConsoleMode(m_hWindow, m_dwOriginalMode);

  //   Dump the invisible thread that's looking for WM_CLOSE and WM_ENDSESSION
  // messages from Windows ...
  EndInvisibleThread();

  // Close the handles ...
  if (m_hInput  != NULL) CloseHandle(m_hInput);
  if (m_hOutput != NULL) CloseHandle(m_hOutput);

  // And reset all the related members so we'll know we've been here.
  m_hInput = NULL;  m_hOutput = NULL;
  m_hWindow = NULL;  m_dwOriginalMode = 0;
  m_wOriginalWindowWidth = m_wOriginalWindowHeight = 0;
  m_wOriginalBufferWidth = m_wOriginalBufferHeight = 0;
  m_bOriginalForeground = m_bOriginalBackground = BLACK;
  Sleep(100);
}

CConsoleWindow::CConsoleWindow (const char *pszTitle)
{
  //++
  //   The CConsoleWindow constructor will either "connect" this class to the
  // current console window, or if no console exists, it will create a new
  // console and then connect to that.  Even though this is a CUI application
  // and Windows normally automatically creates a console for us, it's possible
  // for this process to initially exist without one if we (or someone else)
  // calls CreateProcess() to create a new instance of this application.
  //
  //   Connecting to a console window, whether old or new, requires assigning
  // -handles for console input and output, getting the console window handle,
  // setting the console mode, and establishing the trap handler for ^C, ^BREAK
  // and shutdown events.
  //
  //   Lastly, note that the CConsoleWindow is a singleton object - only one
  // instance ever exists and we keep a pointer to that one in a static member,
  // m_pConsole.
  //--
  assert(m_pConsole == NULL);
  m_pConsole = this;
  if (GetConsoleWindow() == NULL) AllocConsole();
  AttachCurrentConsole();
  if (pszTitle != NULL) SetTitle(pszTitle);
}

CConsoleWindow::~CConsoleWindow()
{
  //++
  //   The destructor closes the handles for the console input and output
  // buffers, and restores the original console mode, just in case we've
  // changed it to something funny...
  //--
  DetachCurrentConsole();
  //   Reset the pointer to this Singleton object. In theory this would allow
  // another CConsoleWindow instance to be created, but that's not likely to
  // be useful.
  assert(m_pConsole == this);
  m_pConsole = NULL;
}

void CConsoleWindow::SetMode (void *hBuffer, uint32_t dwSet)
{
  //++
  // Set (a bitwise OR operation!) bits in the current console mode ...
  //--
  DWORD dwMode;
  if (!GetConsoleMode(hBuffer, &dwMode)) assert(false);
  dwMode |= dwSet;
  // A little hack for the INSERT and QUICK EDIT modes!
  if (ISSET(dwSet,ENABLE_INSERT_MODE) || ISSET(dwSet,ENABLE_QUICK_EDIT_MODE))
    dwMode |= ENABLE_EXTENDED_FLAGS;
  if (!SetConsoleMode(hBuffer, dwMode)) assert(false);
}

void CConsoleWindow::ClearMode (void *hBuffer, uint32_t dwClear)
{
  //++
  // Clear (a bitwise AND operation!) bits in the current console mode ...
  //--
  DWORD dwMode;
  if (!GetConsoleMode(hBuffer, &dwMode)) assert(false);
  dwMode &= ~dwClear;
  if (ISSET(dwClear, ENABLE_INSERT_MODE) || ISSET(dwClear, ENABLE_QUICK_EDIT_MODE))
    dwMode |= ENABLE_EXTENDED_FLAGS;
  if (!SetConsoleMode(hBuffer, dwMode)) assert(false);
}

string CConsoleWindow::GetTitle() const
{
  //++
  // Return the current title string for the console window ...
  //--
  wchar_t wszTitle[MAX_PATH];  size_t cwTitle;
  //   Note that, at least with Windows 7 & 8, GetConsoleTitle() wants the
  // buffer size to be given in bytes, not characters!  There's some argument
  // about whether this is the Right Thing, but for now this is how it is.
  cwTitle = GetConsoleTitle(wszTitle, /*sizeof(wszTitle)*/ MAX_PATH);
  char szTitle[MAX_PATH];  size_t cbTitle;
  wcstombs_s(&cbTitle, szTitle, sizeof(szTitle), wszTitle, cwTitle);
  return string(szTitle);
}

void CConsoleWindow::SetTitle (const char *pszTitle, ...)
{
  //++
  //   This method sets the title string of the console window.  It's pretty
  // simple, but notice that it allows for extra printf() style arguments.
  // This makes it easy to include the version number, build date, disk status
  // or whatever else, in the title bar.
  //--
  assert((pszTitle != NULL) && (strlen(pszTitle) > 0));

  // First format the title string as ASCII ...
  char szBuffer[MAX_PATH];  va_list args;  size_t cbBuffer;
  va_start(args, pszTitle);  memset(szBuffer, 0, sizeof(szBuffer));
  vsnprintf_s(szBuffer, sizeof(szBuffer), _TRUNCATE, pszTitle, args);
  va_end(args);  cbBuffer = strlen(szBuffer);

  // Then convert the title to wide characters ...
  wchar_t wszTitle[MAX_PATH];  size_t cwTitle;
  mbstowcs_s(&cwTitle, wszTitle, cbBuffer+1, szBuffer, _TRUNCATE);

  // And lastly we can set the title ... 
  SetConsoleTitle(wszTitle);
}

void CConsoleWindow::SetColors (uint8_t bForeground, uint8_t bBackground)
{
  //++
  //   This routine will set the foreground (text) and background (window)
  // colors for the console.  The console palette is very limited and 
  // essentially emulates the CGA display with a range of only 16 colors.
  // Both bForeground and bBackground are four bit values where the value
  // 1 is blue, 2 is green, 4 is red and 8 is the intensify bit.  
  //--

  // This sets the colors for any new text written to the screen...
  uint16_t wColor = (MASK4(bBackground) << 4) | MASK4(bForeground);
  SetConsoleTextAttribute(m_hOutput, wColor);

  //   The problem is that SetConsoleTextAttribute() alone doesn't change the
  // background for the text that's already in the screen buffer.  To change
  // the color of everything, we have to use FillConsoleOutputAtrribute() to
  // set the atributes on the entire buffer ...
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  GetConsoleScreenBufferInfo(m_hOutput, &csbi);
  DWORD dwBufferSize = csbi.dwSize.X * csbi.dwSize.Y;
  DWORD dwCharsWritten;  COORD coord00 = {0, 0};
  FillConsoleOutputAttribute(m_hOutput, wColor, dwBufferSize, coord00, &dwCharsWritten);
}

bool CConsoleWindow::GetColors (uint8_t &bForeground, uint8_t &bBackground)
{
  //++
  //   Return the current console window colors (see the SetColors() method
  // for more details on the color set).
  //--
  bForeground = WHITE;  bBackground = BLACK;
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  if (!GetConsoleScreenBufferInfo(m_hOutput, &csbi)) return false;
  bForeground = MASK4(csbi.wAttributes);
  bBackground = MASK4(csbi.wAttributes >> 4);
  return true;
}

bool CConsoleWindow::SetIcon (uint32_t nIcon)
{
  //++
  //   Use the undocumented (!!) Kernel32 function SetConsoleIcon to set the
  // icon for this console window.  nIcon is assumed to be a small integer
  // which is the handle of the desired icon in our own resource file (e.g.
  // IDI_MAIN_APPLICATION).
  //--
  HMODULE hKernel32 = LoadLibrary(L"Kernel32.dll");
  assert(hKernel32 != NULL);
  typedef DWORD (__stdcall *SET_CONSOLE_ICON) (HICON);
  SET_CONSOLE_ICON pfnSetConsoleIcon = (SET_CONSOLE_ICON) GetProcAddress(hKernel32, "SetConsoleIcon");
  assert(pfnSetConsoleIcon != NULL);
  HICON hIcon = LoadIcon(GetModuleHandle(NULL), (LPCWSTR) nIcon+0);
  assert(hIcon != NULL);
  DWORD dwReturn = pfnSetConsoleIcon(hIcon);
  FreeLibrary(hKernel32);
  return (dwReturn != 0);
}

void CConsoleWindow::RawWrite (const char *pabBuffer, size_t cbBuffer)
{
  //++
  //   Write a buffer of one or more characters to the console window.  Note
  // that a lot of the emulated machines and software like to send funky
  // characters to what they think is an ASR33.  This includes NULLs, RUBOUTs,
  // ASCII characters with the 8th bit set, and all kinds of junk.  We do
  // our best to filter those out, although that involves making a copy of
  // the caller's buffer, which is kind of slow...
  //--

  //   Run thru the caller's buffer first, and if there's nothing bad in there
  // then we can just print it and quit ...
  bool fBad = false;
  for (size_t i = 0; i < cbBuffer; ++i) {
    if (   ISSET(pabBuffer[i], 0x80)
        || (pabBuffer[i] == 0)      ) {fBad = true;  break;}
  }
  if (!fBad) {
    WriteConsoleA(m_hOutput, pabBuffer, MKINT32(cbBuffer), NULL, NULL);
    return;
  }

  // Print the buffer one character at a time, fixing each one as we go ...
  for (size_t i = 0; i < cbBuffer; ++i) {
    char bChar = pabBuffer[i] & 0x7F;
    if (bChar != 0)
      WriteConsoleA(m_hOutput, &bChar, 1, NULL, NULL);
  }
}

void CConsoleWindow::Write (const char *pszText)
{
  //++
  //   Write a string to the console window.  This is pretty easy (we could
  // just as well use fputs() for now!) but it's here for completeness.
  //--
 
  //   Write() is only called by the comman scanner, the error message and log
  // file routines.  It's never called by the emulation engine, so we DON'T
  // want to call RawWrite() here.  For one, RawWrite() is slower, but for two,
  // RawWrite() output gets sent to the SmartConsole log file where as output
  // that goes thru Write() does not.
  //RawWrite(pszText, strlen(pszText));
  WriteConsoleA(m_hOutput, pszText, strlen(pszText), NULL, NULL);
}

void CConsoleWindow::Print (const char *pszFormat, ...)
{
  //++
  // Send printf() style formatted output to the console.
  //--
  char szBuffer[MAX_PATH];  va_list args;
  memset(szBuffer, 0, sizeof(szBuffer));
  va_start(args, pszFormat);
  vsnprintf_s(szBuffer, sizeof(szBuffer), _TRUNCATE, pszFormat, args);
  va_end(args);
  Write(szBuffer);
}

void CConsoleWindow::WriteLine (const char *pszLine)
{
  //++
  // Write a string followed by a newline ...
  //--
  if (pszLine != NULL) Write(pszLine);
  Write("\n");
}

bool CConsoleWindow::ReadLine (const char *pszPrompt, char *pszBuffer, size_t cbBuffer)
{
  //++
  //   This routine will read one line of input from the console window.  It's
  // fairly easy, but there are a couple of nasty things to worry about. Notice
  // that we set the console mode to LINE INPUT and PROCESSED INPUT - this
  // means the ReadConsole() call won't return until the user enters a carriage
  // return AND it allows use of all the standard DOS/command window line 
  // editing keys (the arrow keys, insert/delete, backspace, and F1/F2/F3).
  //
  //   It returns true if a line was read successfully, although that line may
  // be blank!  It returns false only if it was unable to read anything at all
  // - this should be taken as a signal for the program to exit.  Note that we
  // want ^Z (EOF) to cause an exit, but Windows provides no special handling
  // for this character.  We have to check the buffer for it and handle it by
  // ourself.
  //
  //   Also note that Windows will return the carriage return and line feed
  // characters ("\r\n") at the end of the buffer.  We strip those out and
  // the string returned to the caller has no special line terminator.
  //
  //   Also see CONSOLE_READCONSOLE_CONTROL...
  //--
  DWORD cbRead;
  if (m_fForceExit) return false;

  // If there's a prompting string, print it first ...
  if (pszPrompt != NULL) Write(pszPrompt);

  // Select line input mode and read a buffer of characters ...
  memset(pszBuffer, 0, cbBuffer);
  SetMode(m_hInput, ENABLE_ECHO_INPUT|ENABLE_LINE_INPUT|ENABLE_PROCESSED_INPUT);
  if (!ReadConsoleA(m_hInput, pszBuffer, MKINT32(cbBuffer), (LPDWORD) &cbRead, NULL)) {
    m_fForceExit = true;  return false;
  }

  //   We're back here now for one of two reasons - either ReadConsole() really
  // did read something from the user, or the ReadConsole() was aborted because
  // a system event (^C, Control-BREAK, close window or even Windows Shutdown)
  // occurred.  The Control-C and Control-BREAK events will cause ReadConsole()
  // to return a zero count and we can check for that.  Other events, including
  // closing the console window, logoff or system shutdown, don't actually
  // interrupt ReadConsole() at all.  For those cases there's a lot of code in
  // this module (just look up above) to detect them, set the m_fForceEOF and/
  // or m_fShutdown flags, and then force an ENTER into the keyboard buffer.
  // The fake ENTER makes ReadConsole() return right away, and then we just need
  // to figure out what really happened.
  //
  //   One last little kludge - the ConsoleControlHandler, which handles the
  // Control-C and Control-BREAK cases, actually runs in a separate thread.
  // Since ReadConsole() automatically aborts for those inputs, there's a
  // chance that we'll get here before ConsoleControlHandler() has had a
  // chance to set the m_fForceEOF flag.  The only solution I have for that
  // is to sleep here for a few milliseconds, just to be sure that thread
  // has had time to do its job.
  //
  //   BTW, note that even a blank line always returns two bytes (CR and LF),
  // so if ReadConsole() returns a zero length buffer then that always means
  // something unusual has happened.
  if (cbRead == 0) {Sleep(10); return !m_fForceExit;}
//Print("** ReadConsole() returned %d, ForceEOF=%d ***\n", cbRead, m_fForceEOF);
  if (m_fForceExit) return false;

  //   Check for an ^Z anywhere in the buffer.  If we find one and it's the
  // first character on the line, then return EOF now and a null line.  If
  // there's some text before the ^Z, then return that text this time and
  // return EOF next time around.
  char *pch = strchr(pszBuffer, (char) CHEOF);
  if (pch != NULL) {
    SetForcedExit();  *pch = '\0';  return strlen(pszBuffer) > 0;
  }

  //   Check the last two characters of the buffer for carriage return and
  // line feed and, if they're there, strip them off.  AFAIK there's no way
  // ReadConsole() can return without the CRLF at the end, but we'll check
  // anyway just to be safe ...
  if ((cbRead > 0)  && (pszBuffer[cbRead-1] == CHLFD)) --cbRead;
  if ((cbRead > 0)  && (pszBuffer[cbRead-1] == CHCRT)) --cbRead;
  pszBuffer[cbRead] = '\0';
  return true;
}

int32_t CConsoleWindow::ReadKey (uint8_t &bData, uint32_t lTimeout)
{
  //++
  //   This routine will read a single keystroke from the console window,
  // potentially with a time out.  It will return, without blocking forever,
  // if the timeout period expires.  The return value is +1 if a key is
  // read, zero if the time out period expired, or -1 if some other error
  // (e.g. window closed, EOF, etc) occurred.
  //
  //   It's probably obvious, but this function reads the console keyboard in
  // "raw" mode.  Everything that can be translated to an ASCII character is
  // returned, and everything else is ignored.  Control-C and Control-Z are
  // treated like any other ASCII characters.  Control-BREAK, PRINT SCREEN,
  // and things like that are ignored.  
  //--
  DWORD dwEvents = 0;  INPUT_RECORD inEvent;

  // Disable Control-C and other special processing ...
  ClearMode(m_hInput, ENABLE_ECHO_INPUT|ENABLE_LINE_INPUT|ENABLE_PROCESSED_INPUT);

  //   Windows puts all kinds of events - mouse clicks, focus changes, key
  // strokes, and God knows what - into the console input queue.  We only
  // care about keys, and so we have to keep reading and discarding anything
  // else until we either time out or find a real keystroke.
  while (true) {
    // If there's nothing waiting in the queue then we'll have to wait ...
    if (!GetNumberOfConsoleInputEvents(m_hInput, &dwEvents)) return -1;
    if (dwEvents == 0) {
      DWORD dwResult = WaitForSingleObject(m_hInput, lTimeout);
      if (dwResult == WAIT_TIMEOUT) return 0;
      if (dwResult != WAIT_OBJECT_0) return -1;
    }
    if (m_fForceExit) return -1;

    // Pull the next event from the queue and see if it's a key press ...
    if (!ReadConsoleInput(m_hInput, &inEvent, 1, &dwEvents)) return -1;
    assert(dwEvents == 1);
    if ((inEvent.EventType == KEY_EVENT) && (inEvent.Event.KeyEvent.bKeyDown)) {
      // BREAK key - inEvent.Event.KeyEvent.VirtualKeyCode == VK_PAUSE
      bData = inEvent.Event.KeyEvent.uChar.AsciiChar;
      if (   ((GetSerialBreak() != 0) && (bData == GetSerialBreak()))
          || (inEvent.Event.KeyEvent.wVirtualKeyCode == VK_PAUSE)) {
        // Serial break character - simulate a RS232 break condition ...
        m_fSerialBreak = true;  return 0;
      } else if ((GetConsoleBreak() != 0) && (bData == GetConsoleBreak())) {
        // Console break character - interrupt emulation ...
        m_fConsoleBreak = true;  return 0;
      } else
        return (bData != 0) ? 1 : 0;
    }
  }
}

bool CConsoleWindow::IsConsoleBreak (uint32_t lTimeout)
{
  //++
  //   Return TRUE if the console break flag is set (i.e. a Control-E or
  // whatever has been seen on input) and then clear the flag.
  // 
  //   Notice that this routine will call ReadKey() repeatedly until there are
  // no more keystrokes waiting.  That's necessary so that we can detect a
  // console break even if the emulated program stops reading keyboard input.
  // Characters we read this way are not discarded, though, and instead we stuff
  // them into the keyboard buffer, m_KeyBuffer.  The RawRead() routine will
  // return characters from this buffer next time it's called.
  //--

  // First poll for any keyboard input ...
  uint8_t bData;
  while (ReadKey(bData, lTimeout) > 0) m_KeyBuffer.Put(bData);

  // Now return the state of the console break flag, and also clear it ...
  bool fBreak = m_fConsoleBreak;
  m_fConsoleBreak = false;
  return fBreak;
}

bool CConsoleWindow::IsReceivingSerialBreak (uint32_t lTimeout)
{
  //++
  //   Return TRUE if the serial break flag is set (i.e. the PAUSE/BREAK key
  // has been pressed) and then clear the flag.  Despite the confusing names,
  // this is different from the Console Break condition (^E) which interrupts
  // emulation and returns input to the command parser.  A serial break
  // simulates pressing the BREAK key on an ASCII terminal connected to a UART.
  // Some software, notably RCA MicroDOS for the MS2000, uses this break
  // condition to interrupt programs.
  // 
  //    Like the m_fConsoleBreak flag, m_fSerialBreak is set by ReadKey() as
  // we receive keyboard input.  And like IsConsoleBreak(), this routine
  // will call ReadKey() until there are no more keystrokes waiting.  That's
  // necessary becasuse the emulated program may have stopped polling for
  // keyboard input, but we still want to detect the serial break condition.
  // It's probably pointless though, because anything that calls this routine
  // will most likely call IsConsoleBreak() first, and that will have already
  // processed any keystrokes waiting.
  //--

  // First poll for any keyboard input ...
  uint8_t bData;
  while (ReadKey(bData, lTimeout) > 0) m_KeyBuffer.Put(bData);

  // Now return the state of the serial break flag, and also clear it ...
  bool fBreak = m_fSerialBreak;  m_fSerialBreak = false;
  return fBreak;
}

int32_t CConsoleWindow::RawRead (uint8_t *pabBuffer, size_t cbBuffer, uint32_t lTimeout)
{
  //++
  //   This method will read one or more characters from the keyboard in "raw" 
  // mode, as returned by ReadKey().  The raw characters are stuffed into the
  // buffer until either the buffer becomes full, the ReadKey() times out, or
  // an error occurs.  It returns the number of characters actually read, or
  // zero if the time out expired before any data was entered, or -1 if some
  // error (window closed, EOF, etc) occurred.
  //--
  int32_t cbRead = 0;  uint8_t bData;

  // First try to pull characters from the keyboard typeahead buffer ...
  while (((size_t) cbRead < cbBuffer) && m_KeyBuffer.Get(bData)) {
    *pabBuffer++ = bData;  ++cbRead;
  }

  //   Then if the type ahead buffer is empty and we still want more input,
  // try reading from the actual keyboard ...
  while ((size_t) cbRead < cbBuffer) {
    int32_t nRet = ReadKey(bData, lTimeout);
    if (nRet < 0) return -1;
    if (nRet == 0) return cbRead;
    *pabBuffer++ = bData;  ++cbRead;
  }

  // All done - return the number of characters actually read ...
  return cbRead;
}

void CConsoleWindow::GetWindowPosition (int32_t &X, int32_t &Y)
{
  //++
  // Return the current (X,Y) position of the window, in screen coordinates ...
  //--
  RECT rc;  GetWindowRect((HWND) m_hWindow, &rc);
  X = rc.left;  Y = rc.top;
}

bool CConsoleWindow::GetWindowSize (uint16_t &nColumns, uint16_t &nRows)
{
  //++
  // Return the current console window size, in rows and columns ...
  //--
  nColumns = 80;  nRows = 24;
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  if (!GetConsoleScreenBufferInfo(m_hOutput, &csbi)) return false;
  nColumns = csbi.srWindow.Right - csbi.srWindow.Left + 1;
  nRows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
  return true;
}

bool CConsoleWindow::GetBufferSize (uint16_t &nColumns, uint16_t &nRows)
{
  //++
  // Return the current console buffer size, in rows and columns ...
  //--
  nColumns = 80;  nRows = 24;
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  if (!GetConsoleScreenBufferInfo(m_hOutput, &csbi)) return false;
  nColumns = csbi.dwSize.X;  nRows = csbi.dwSize.Y;
  return true;
}

bool CConsoleWindow::SetBufferSize (uint16_t nColumns, uint16_t nRows)
{
  //++
  //  This method sets the size of the console window buffer, in rows and
  // columns.  WARNING - if the new buffer size is smaller than the current
  // window size, Windows will automatically shrink the window to match!
  //--
  CONSOLE_SCREEN_BUFFER_INFOEX csbi;
  memset(&csbi, 0, sizeof(csbi));  csbi.cbSize = sizeof(csbi);
  if (!GetConsoleScreenBufferInfoEx(m_hOutput, &csbi)) return false;
  csbi.dwSize.X = nColumns;  csbi.dwSize.Y = nRows;
  if (!SetConsoleScreenBufferInfoEx(m_hOutput, &csbi)) return false;
  return true;
}

bool CConsoleWindow::SetWindowSize (uint16_t nColumns, uint16_t nRows, int32_t nX, int32_t nY)
{
  //++
  //   This method will attempt to set the console window size and, optionally,
  // the window position on the screen.  The size arguments are specified in
  // character columns and rows, however the screen coordinates are specified
  // in pixels.  Unfortunately, there are lots of special cases -
  //
  //   * If nX and nY are both -1, the current window position is unchanged.
  //
  //   * If the new window size would be bigger than the associated buffer,
  //     then Windows will automatically increase the size of the buffer.
  //
  //   * If the new window size is bigger than the maximum allowable console
  //     window size, then we'll reduce the new size to fit.
  //
  //   * If the new window size and/or position would place all or part of
  //     the window off screen, then we'll drag it back onto the screen.
  //--

  // If the new (X,Y) are both zero, use the current position ...
  if ((nX < 0) || (nY < 0)) GetWindowPosition(nX, nY);

  // Enlarge the screen buffer (if necessary) ...
  uint16_t nBufferRows, nBufferColumns;
  GetBufferSize(nBufferColumns, nBufferRows);
  if ((nColumns > nBufferColumns) || (nRows > nBufferRows)) {
    nBufferColumns = MAX(nBufferColumns, nColumns);
    nBufferRows = MAX(nBufferRows, nRows);
    SetBufferSize(nBufferColumns, nBufferRows);
  }

  // Convert the rows and columns to pixels using the console font ...
  CONSOLE_FONT_INFOEX ccfi;
  memset(&ccfi, 0, sizeof(ccfi));  ccfi.cbSize = sizeof(ccfi);
  if (!GetCurrentConsoleFontEx(m_hOutput, FALSE, &ccfi)) return false;
  RECT rcWindow;  rcWindow.left = nX;  rcWindow.top = nY;
  rcWindow.right = nX + (ccfi.dwFontSize.X * nColumns) - 1;
  rcWindow.bottom = nY + (ccfi.dwFontSize.Y * nRows) - 1;

  // Adjust the window rectangle to allow for decorations ...
  DWORD dwStyle   = (DWORD) GetWindowLong((HWND) m_hWindow, GWL_STYLE);
  DWORD dwStyleEx = (DWORD) GetWindowLong((HWND) m_hWindow, GWL_EXSTYLE);
  AdjustWindowRectEx(&rcWindow, dwStyle, FALSE, dwStyleEx);

  //   The problem with AdjustWindowRect() (above!) is that it will adjust
  // the top and left edges in the negative direction to allow for the top
  // and left borders.  Since the caller specified the exact (X,Y) window
  // position he wants, that's not the result we need.  We have to adjust the
  // rectangle back again so that the (left,top) is at the original (X,Y).
  OffsetRect(&rcWindow, nX-rcWindow.left, nY-rcWindow.top);

  // Adjust the window rectangle to allow for the scroll bars ...
  uint32_t nVScroll = GetSystemMetrics(SM_CXVSCROLL);
  uint32_t nHScroll = GetSystemMetrics(SM_CXHSCROLL);
  if (nBufferRows    > nRows   ) rcWindow.right  += nVScroll;
  if (nBufferColumns > nColumns) rcWindow.bottom += nHScroll;

  // Find the overall dimensions of the screen ...
  HMONITOR hMonitor = MonitorFromWindow((HWND) m_hWindow, MONITOR_DEFAULTTOPRIMARY);
  MONITORINFO mi;  memset(&mi, 0, sizeof(mi));  mi.cbSize = sizeof(mi);
  GetMonitorInfo(hMonitor, &mi);

  // Drag the new window onto the screen if necessary ...
  if (rcWindow.right > mi.rcWork.right)
    OffsetRect(&rcWindow, -(MIN(rcWindow.left, (rcWindow.right-mi.rcWork.right))), 0);
  if (rcWindow.left < 0) OffsetRect(&rcWindow, -rcWindow.left, 0);
  if (rcWindow.bottom > mi.rcWork.bottom)
    OffsetRect(&rcWindow, 0, -(MIN(rcWindow.top, (rcWindow.bottom-mi.rcWork.bottom))));
  if (rcWindow.top < 0) OffsetRect(&rcWindow, 0, -rcWindow.top);

  // And finally, move the window!
  if (!MoveWindow((HWND) m_hWindow, rcWindow.left, rcWindow.top,
    rcWindow.right-rcWindow.left+1, rcWindow.bottom-rcWindow.top+1, true)) return false;
  return true;
}

bool CConsoleWindow::CreateNewConsole()
{
  //++
  //   This method will detach from the current console, create a new console
  // window, and then attaches to that.  It makes this process independent of
  // the original console window, HOWEVER it does not fork a new process. So,
  // if this program was started from a command line in the original console
  // window, that window's shell will still be waiting for us to finish!
  //
  //   Note that the detach part of this method is optional.  If it fails, say
  // because we're an independent process without any current console, then
  // no errors are reported.  We just go ahead and make a new one.
  //
  //   If this method returns false, then it means we failed to create a new
  // console.  That means we're currently without any console, and probably
  // the only reasonable option is to give up and quit.
  //
  //   One final parting comment - note that this is a static routine.  It
  // simply executes the WIN32 functions needed to create and attach the new
  // console without worrying about the effect on any existing CConsoleWindow
  // object. Calling this routine alone WILL RENDER ANY EXISTING CConsoleWindow
  // OBJECT INVALID!  If you need to create a new console window AFTER the
  // CConsoleWindow object already exists, then call the non-static member
  // AttachNewConsole() instead.
  //--
  FreeConsole();
  // This rather odd arrangement avoids C4800 errors due to BOOL vs bool!
  if (!AllocConsole()) return false;
  return true;
}

bool CConsoleWindow::AttachNewConsole()
{
  //++
  //   This method executes a CreateNewConsole() and creates a new console
  // window, HOWEVER this one takes care of the effect on this existing 
  // CConsoleWindow object.  Calling this method will create a new console
  // window AND this object will still work on the new one after we return.
  //--
  DetachCurrentConsole();
  if (!CreateNewConsole()) return false;
  AttachCurrentConsole();
  return true;
}


#endif      // end of #ifdef _WIN32 from the very top of this file!
