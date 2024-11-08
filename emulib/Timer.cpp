//++
// Timer.cpp -> CTimer Generic programmable timer emulation
//
// DESCRIPTION:
//   This class implements a generic emulation for the programmable counter
// timer that's present in many chips, such as the Intel 8253/4 (contains no
// less than three counter/timers!), the Intel 8155/6, the National NSC810,
// etc.
//
//   Each timer counts down at regular intervals as dermined by the SetCloc()
// or SetPeriod() methods.  Of course, in real live a counter might be clocked
// by some external event, but we don't currently emulate that.  Each counter
// may be enabled or disabled; disabling the counter will temporarily pause
// counting but otherwise leaves the current counter state unchanged.  This is
// equivalent to a clock GATE input found on many chips.
// 
//   Each timer/counter supports three different modes of operation -
//
//   * STOPPED - the timer does nothing and the count doesn't change.
//   * ONESHOT - the count register is initialized to the value in the jam
//               register, counts down to zero, and then stops.
//   * REPEAT  - the count register is initialized to the jam value as above,
//               counts down to zero, and then resets to the jam value and
//               counts down again.  This continues until the timer mode is
//               changed.
//
//   Each simulated timer consists of three registers -
// 
//   * jam   - the jam register is set by the timer WriteX() functions and
//             contains the value which is loaded or reloaded into the count
//             register when a new count cycle is started
//   * count - this register contains the current count and is decremented by
//             every clock interrupt.
//   * hold  - the count register is normally copied to the hold(ing) register
//             every time the count is decremented.  This can be stopped by the
//             Freeze() function, which freezes the holding register while the
//             count contiues to decrement.  Freezing allows the holding regiser
//             to be read by the ReadX() functions without it changing.
// 
//   When the count down rolls over from $0000 to $ffff, the timer will invoke
// a callback function.  The callback function is established by calling
// SetCallback(), and the parent CDevice should use this to set a status bit,
// activate an output, generate an interrupt, or whatever is appropriate.  In
// ONESHOT mode the timer stops after invoking the callback, but in REPEAT mode
// the count register is reloaded from the jam register, and the countdown begins
// again.
//
// REVISION HISTORY:
//  3-JUL-23  RLA   New file.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include "EMULIB.hpp"           // emulator library definitions
#include "LogFile.hpp"          // emulator library message logging facility
#include "ConsoleWindow.hpp"    // console window functions
#include "CommandParser.hpp"    // needed for type KEYWORD
#include "MemoryTypes.h"        // address_t and word_t data types
#include "EventQueue.hpp"       // CEventQueue declarations
#include "CPU.hpp"              // needed for HZTONS and NSTOHZ ...
#include "Timer.hpp"            // declarations for this module


CTimer::CTimer (unsigned nTimer, CEventQueue *pEvents, uint32_t lPeriod)
{
  //++
  // Constructor ...
  //--
  assert(pEvents != NULL);
  m_nTimer = nTimer;  m_strName = FormatString("Timer%c", nTimer);
  m_pEvents = pEvents;  m_lPeriod = lPeriod;
  m_pCallback = NULL;  m_pParent = NULL;
  Clear();
}

/*static*/ string CTimer::ModeToString(TIMER_MODE nMode)
{
  //++
  // Convert a timer mode to a string for messages ...
  //--
  switch (nMode) {
    case TIMER_STOPPED:   return "STOPPED";
    case TIMER_ONESHOT:   return "ONE SHOT";
    case TIMER_REPEAT:    return "CLOCK DIVIDER";
    default:              return "UNKOWN";
  }
}

void CTimer::Clear()
{
  //++
  //   This will reset the timer - all internal registers are cleared, the
  // gate is enabled, the holding register is unfrozen, and the counter is
  // stopped.
  //--
  m_nMode = TIMER_STOPPED;  m_fEnabled = true;  m_fFreeze = false;
  m_wJam = m_wCount = m_wHold = 0;  m_fIEN = false;
  m_pEvents->Cancel(this, 0);
}

void CTimer::EventCallback (intptr_t lParam)
{
  //++
  //   This is the actual "counting" part of the timer/counter.  It's invoked
  // by the event queue when the timer clock period expires, and it decrements
  // the count register (plus a lot of other stuff too!).
  //--

  //   If the counter is not enabled (i.e. the gate is not asserted), then just
  // schedule the next clock cycle event and do nothing else ...
  if (m_fEnabled) {
    //   Decrement the count and, if the holding register is not frozen,
    // transfer the current count to the holding register ...
    --m_wCount;
    if (!m_fFreeze) m_wHold = m_wCount;

    // Did we just roll over from $0000 to $FFFF??
    if (m_wCount == 0xFFFF) {
      //   Yes - if we're in REPEAT mode then reload the count from the jam
      // register and keep counting.  Otherwise stop the timer now.  Note that
      // we do these things BEFORE invoking the callback, just in case the
      // parent's callback function wants to restart us for some reason.
      if (m_nMode == TIMER_REPEAT)
        m_wCount = m_wJam;
      else
        m_nMode = TIMER_STOPPED;
      // Now invoke the callback ...
      if (m_pCallback != NULL) (*m_pCallback) (m_pParent, this);
      //   If we're now stopped, then dismiss this event without scheduling
      // another one.
      if (m_nMode == TIMER_STOPPED) return;
    }
  }

  // Schedule the next clock cycle and count ...
  m_pEvents->Schedule(this, 0, m_lPeriod);
}

void CTimer::Start (bool fJam)
{
  //++
  //   Start the timer running, assuming the mode has already been set by a
  // call to SetMode(). If fJam is true, then start by reloading the count
  // register from the jam, otherwise start counting where we last left off.
  //--
  assert(m_nMode != TIMER_STOPPED);
  if (fJam) Jam();
  m_pEvents->Schedule(this, 0, m_lPeriod);
}

void CTimer::Stop()
{
  //++
  //   Stop the timer by cancelling any future clock tick events.  Other than
  // that, it's important that this do NOTHING to alter the state of the timer.
  // The count can be restarted where we left off by calling Start(false) ...
  //--
  m_nMode = TIMER_STOPPED;
  m_pEvents->Cancel(this, 0);
}

void CTimer::Show (ostringstream &ofs) const
{
  //++
  //   Show the current state of this timer.  This is normally used by the
  // Show() method of the parent CDevice ...
  //--
  ofs << FormatString("%s: %s, Jam=0x%04X, Count=0x%04X, Hold=0x%04X\n",
    GetName(), ModeToString(m_nMode).c_str(), m_wJam, m_wCount, m_wHold);
  ofs << FormatString("\tEnabled=%d, Frozen=%d, IEN=%d, Period=%uns (%uHz)\n",
    m_fEnabled, m_fFreeze, m_fIEN, m_lPeriod, NSTOHZ(m_lPeriod));
}
