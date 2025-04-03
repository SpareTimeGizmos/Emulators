//++
// CDP1878.cpp -> RCA CDP1878 dual counter/timer emulator
//
// DESCRIPTION:
//   This class implements a generic emulation for the RCA CDP1878 dual
// counter/timer.  The 1878 contains two identical and independent units. Each
// counter contains a 16 bit register that counts down in any one of five
// different modes.  
//
// NOTES
//    The CDP1878 contains six addressible registers and effectively fills the
// entire COSMAC I/O space, with register/port 1 being unused.  Because of that
// it's pretty much mandatory to use this device with RCA two level I/O, where
// this chip fills one entire I/O group (with the group select, port 1, being
// conveniently left unused!  That's why the constructor doesn't take a base
// address nor a register count.
// 
//    In this implementation each counter is assumed to be driven by a fixed
// frequency clock.  Of course, in real life that doesn't have to be true, but
// we can't emulate anything else. In the SBC1802, counter A is clocked by the
// CPU clock and counter B is clocked by the baud rate clock /4 (4.9152MHz/4 or
// 1.2288MHz).  The latter has the advantage of being constant regardless of
// the CPU clock.
// 
//    This class has two virtual methods, TerminalCountA() and TerminalCountB(),
// which do nothing here but may be overriden by a class derived from this one.
// As with the PPI implementation, real hardware that uses the CTC for some more
// complex operation can inherit this one and then override the TerminalCountX
// methods to implement so particular action associated with the timer output.
// And in addition, there are TimerGateA() and TimerGateB() methods that can be
// called to simulate the gate input for each timer.
// 
// REVISION HISTORY:
//  2-NOV-24  RLA   New file.
// 25-MAR-25  RLA   Add EnableCTC() ...
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
#include "CPU.hpp"              // CPU definitions
#include "Device.hpp"           // generic device definitions
#include "Timer.hpp"            // generic counter/timer emulation
#include "CDP1878.hpp"          // declarations for this module


CCDP1878::CCDP1878 (const char *pszName, CEventQueue *pEvents,
                    address_t nSenseInt, address_t nSenseA, address_t nSenseB)
     : CDevice(pszName, "CDP1878", "Counter/Timer", INOUT, 2, REG_COUNT, pEvents),
       m_TimerA(TIMER_A, pEvents), m_TimerB(TIMER_B, pEvents)
{
  //++
  // Constructor ...
  //--
  m_nSenseInt = nSenseInt;  m_nSenseA = nSenseA;  m_nSenseB = nSenseB;
  m_TimerA.SetCallback(&TimerCallback, this);
  m_TimerB.SetCallback(&TimerCallback, this);
  m_fEnableCTC = true;
  ClearDevice();
}

void CCDP1878::ClearDevice()
{
  //++
  //  Reset the device ...  Reset both timer/counters, clear both timer A and
  // B status bits, and reset the interrupt request.
  //--
  CDevice::ClearDevice();
  m_TimerA.Clear();  m_TimerB.Clear();
  m_bStatus = 0;  UpdateStatus();
}

uint8_t CCDP1878::UpdateStatus()
{
  //++
  //   THis method is called whenever the timer status bits are changed,
  // and it will update the associated interrupt request.  It returns the
  // current status byte as a coding convenience, but doesn't actually
  // change any status bits.
  //--
  m_fIRQ = false;
  if (ISSET(m_bStatus, STS_A) && m_TimerA.GetIEN()) m_fIRQ = true;
  if (ISSET(m_bStatus, STS_B) && m_TimerB.GetIEN()) m_fIRQ = true;
  RequestInterrupt(m_fIRQ && m_fEnableCTC);
  return m_bStatus;
}

/*static*/ void CCDP1878::TimerCallback (CDevice *pDevice, CTimer *pTimer)
{
  //++
  //   This method is called by either of the m_TimerA or m_TimerB objects
  // whenever the associated counter rolls over from $0000 to $FFFF.  THis
  // sets the status bit associated with that timer and requests an interrupt
  // if enabled.
  //--
  assert((pDevice != NULL) && (pTimer != NULL));
  CCDP1878 *pThis = dynamic_cast<CCDP1878 *>(pDevice);

  //   Set the status bit associated with this timer and call its
  // TerminalCount() method ...
  if (pTimer->GetIndex() == TIMER_A) {
    pThis->m_bStatus |= STS_A;  pThis->TerminalCountA();
  } else if (pTimer->GetIndex() == TIMER_B) {
    pThis->m_bStatus |= STS_B;  pThis->TerminalCountB();
  }

  // Update the interrupt request and we're done ...
  pThis->UpdateStatus();
}

void CCDP1878::LoadControl (CTimer &Timer, uint8_t bData)
{
  //++
  //   This routine is called for a write to the timer control register, for
  // either timer A or B (they both work exactly the same way).  The data
  // written to the control register contain a bunch of bit fields that control
  // everything the timer can do ...
  //--

  //   In the CDP1878, any write to the control register (even if it otherwise
  // does nothing) clears the associated timer status bit (and interrupt if it
  // is so enabled!).
  if (Timer.GetIndex() == TIMER_A) m_bStatus &= ~STS_A;
  if (Timer.GetIndex() == TIMER_B) m_bStatus &= ~STS_B;
  UpdateStatus();

  // Update the timer mode as required by the 3 LSBs ...
  switch (bData & CTL_MODE_MASK) {
    case MODE_NOCHANGE:
      // Don't change the current mode, whatever it is ...
      break;
    case MODE_TIMEOUT:
    case MODE_STROBE:
    case MODE_ONESHOT:
      //   These all count down to zero and then stop, so we emulate all of
      // them the same way.  That might not be strictly true, but it's all
      // we have for now.
      Timer.SetMode(CTimer::TIMER_ONESHOT);  break;
    case MODE_RATE:
      // And this mode counts down, resets, and counts again ...
      Timer.SetMode(CTimer::TIMER_REPEAT);  break;
    case MODE_PWM:
      // PWM mode is not yet implemented!
    default:
      assert(false);
  }

  //   Set or clear the interrupt enable associated with this timer.  Note
  // that the CTimer class doesn't do anything with the interrupt enable (we
  // use it later here), but since there's one interrupt enable per timer it's
  // convenient to stash it here.
  Timer.SetIEN(ISSET(bData, CTL_IEN));

  // If the START bit is set, then start the timer running.  Otherwise stop it.
  if (ISSET(bData, CTL_START))
    Timer.Start(ISSET(bData, CTL_JAM));
  else
    Timer.Stop();

  // And if the FREEZE bit is set, freeze the holding register ...
  Timer.Freeze(ISSET(bData, CTL_FREEZE));
}

void CCDP1878::DevWrite (address_t nPort, uint8_t bData)
{
  //++
  // Write to a counter/timer register ...
  //--
  assert((nPort > 1) && (nPort <= 7));
  if (!m_fEnableCTC) return;
  switch (nPort) {
    case COUNTER_A_MSB:  LoadMSB(m_TimerA, bData);      break;
    case COUNTER_A_LSB:  LoadLSB(m_TimerA, bData);      break;
    case COUNTER_B_MSB:  LoadMSB(m_TimerB, bData);      break;
    case COUNTER_B_LSB:  LoadLSB(m_TimerB, bData);      break;
    case CONTROL_A:      LoadControl(m_TimerA, bData);  break;
    case CONTROL_B:      LoadControl(m_TimerB, bData);  break;
    default: assert(false);
  }
}

uint8_t CCDP1878::DevRead (address_t nPort)
{
  //++
  //  This routine reads from a counter/timer register.  
  //
  //    Note that although there are two I/O ports assigned to the status
  // register, one in the A group and one in the B group, there is in fact
  // only one status register and both addresses access the same thing.
  //--
  assert((nPort > 1) && (nPort <= 7));
  if (!m_fEnableCTC) return 0xFF;
  switch (nPort) {
    case STATUS_A:
    case STATUS_B:        return ReadStatus();
    case COUNTER_A_MSB:   return ReadMSB(m_TimerA);
    case COUNTER_A_LSB:   return ReadLSB(m_TimerA);
    case COUNTER_B_MSB:   return ReadMSB(m_TimerB);
    case COUNTER_B_LSB:   return ReadLSB(m_TimerB);
    default: assert(false);
  }
  return 0xFF;  // just to avoid C4715 errors!
}

uint1_t CCDP1878::GetSense (address_t nSense, uint1_t bDefault)
{
  //++
  //   REturn the state of a timer output connected to an 1802 EF input.
  // There are three possible EF connections - one for the timer A output,
  // one for the timer B output, and one for the interrupt request.  Remember
  // that the former two are not affected by the interrupt enable flag!
  //--
  UpdateStatus();  
  if (!m_fEnableCTC) return bDefault;
  if (nSense == m_nSenseInt)
    return m_fIRQ ? 1 : 0;
  else if (nSense == m_nSenseA)
    return ISSET(m_bStatus, STS_A) ? 1 : 0;
  else if (nSense == m_nSenseB)
    return ISSET(m_bStatus, STS_B) ? 1 : 0;
  else
    return bDefault;
}

void CCDP1878::ShowDevice (ostringstream &ofs) const
{
  //++
  //   This routine will dump the state of the internal PPI registers.
  // It's used for debugging by the user interface SHOW DEVICE command.
  //--
  if (!m_fEnableCTC) {
    ofs << FormatString("CTC DISABLED") << std::endl;
  } else {
    m_TimerA.Show(ofs);
    ofs << std::endl;
    m_TimerB.Show(ofs);
  }
}
