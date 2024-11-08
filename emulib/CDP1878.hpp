//++
// CDP1878.hpp -> CCDP1878 RCA dual counter/timer interface class
//
// DESCRIPTION:
//   This class implements a generic emulation of the RCA CDP1878 dual counter
// timer.  It's not all that useful by itself, but is intended to be used as a
// base class for system specific emulations.
//
// REVISION HISTORY:
//  2-NOV-24  RLA   New file.
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ std::string class, et al ...
#include "MemoryTypes.h"        // address_t and word_t data types
#include "EventQueue.hpp"       // CEventQueue declarations
#include "Interrupt.hpp"        // CInterrupt definitions
#include "Device.hpp"           // generic device definitions
#include "Timer.hpp"            // generic counter/timer
using std::string;              // ...


class CCDP1878 : public CDevice {
  //++
  // RCA CDP1878 dual counter/timer emulation ...
  //--
 
  // CDP1878 registers and bits ...
public:
  enum {
    // Mnemonics for the callback lParam ...
    TIMER_A       = 'A',    // timer A invoked the callback
    TIMER_B       = 'B',    //   "   B   "  "   "    "   "
    // CDP1878 register offsets relative to the base address ...
    CONTROL_A     = 4,      // control register timer A (write only)
    STATUS_A      = 4,      // status (read only)
    COUNTER_A_MSB = 6,      // counter A count register (MSB)
    COUNTER_A_LSB = 2,      //  "   "  "   "    "    "  (LSB)
    CONTROL_B     = 5,      // control register timer B (write only)
    STATUS_B      = 5,      // status (read only)
    COUNTER_B_MSB = 7,      // counter B count register (MSB)
    COUNTER_B_LSB = 3,      //  "   "  "   "    "    "  (LSB)
    REG_COUNT     = 6,      // total number of byte wide registers
    // Status register bits ...
    STS_A         = 0x80,   // timer A interrupt request
    STS_B         = 0x40,   //   "   B  "     "     "
    // Control register bits (the same for timer A and B!) ...
    CTL_MODE_MASK = 0x07,   // mask for timer mode
    CTL_GPOLARITY = 0x08,   // get input positive level/edge triggered
    CTL_IEN       = 0x10,   // interrupt enable
    CTL_START     = 0x20,   // start the timer
    CTL_FREEZE    = 0x40,   // freeze the holding register
    CTL_JAM       = 0x80,   // jam enable
    // Timer modes ...
    MODE_NOCHANGE = 0,      // don't change current mode
    MODE_TIMEOUT  = 1,      // timeout 
    MODE_STROBE   = 2,      // timeout with strobe
    MODE_ONESHOT  = 3,      // gate controlled one shot
    MODE_RATE     = 4,      // (baud) rate generator
    MODE_PWM      = 5,      // variable duty cycle
  };

  // Constructor and destructor ...
public:
  CCDP1878 (const char *pszName, CEventQueue *pEvents=NULL,
    address_t nSenseInt=UINT16_MAX, address_t nSenseA=UINT16_MAX, address_t nSenseB=UINT16_MAX);
  virtual ~CCDP1878() {};
private:
  // Disallow copy and assignments!
  CCDP1878 (const CCDP1878&) = delete;
  CCDP1878& operator= (CCDP1878 const &) = delete;

  // Public properties ...
public:
  // Return the specific timer subtype ...
  virtual TIMER_TYPE GetType() const {return TIMER_CDP1878;}
  // Set the clock frequency for timers A and B ...
  void SetClockA (uint32_t fFrequency) {m_TimerA.SetClock(fFrequency);}
  void SetClockB (uint32_t fFrequency) {m_TimerB.SetClock(fFrequency);}
  uint32_t GetClockA() const {return m_TimerA.GetClock();}
  uint32_t GetClockB() const {return m_TimerB.GetClock();}

  // Device methods inherited from CDevice ...
public:
  virtual void ClearDevice() override;
  virtual uint8_t DevRead (address_t nPort) override;
  virtual void DevWrite (address_t nPort, uint8_t bData) override;
  virtual void ShowDevice (ostringstream &ofs) const override;
  virtual uint1_t GetSense (address_t nSense, uint1_t bDefault=0) override;

  // Public CDP1878 methods ...
public:
  //   These methods are called when each timer reaches its terminal count.
  // By default they do nothing, but they can be overriden by a derived class
  // to take some more implementation specific action.
  virtual void TerminalCountA() {};
  virtual void TerminalCountB() {};
  //   These methods enable or disable counting for timers A and/or B.  They're
  // equivalent to the timer gate input of the real chip.
  void TimerGateA (bool fEnable=true) {m_TimerA.Enable(fEnable);}
  void TimerGateB (bool fEnable=true) {m_TimerB.Enable(fEnable);}

  // Local methods ...
private:
  // Update the current status and interrupt request ...
  uint8_t UpdateStatus();
  // Timer terminal count callback ...
  static void TimerCallback (CDevice *pDevice, CTimer *pTimer);
  // Load counter registers ...
  void LoadControl (CTimer &Timer, uint8_t bData);
  void LoadMSB (CTimer &Timer, uint8_t bData) {Timer.WriteH(bData);}
  void LoadLSB (CTimer &Timer, uint8_t bData) {Timer.WriteL(bData);}
  // Read the (one shared common!) status register
  uint8_t ReadStatus() {return UpdateStatus();}
  // Read counter count ...
  uint8_t ReadMSB (CTimer &Timer) {return Timer.ReadL();}
  uint8_t ReadLSB (CTimer &Timer) {return Timer.ReadH();}

  // Private member data...
private:
  CTimer    m_TimerA;         // counter/timer unit 1
  CTimer    m_TimerB;         // counter/timer unit 2
  address_t m_nSenseInt;      // sense flag (EF) for interrupts
  address_t m_nSenseA;        //   "     "    "  for timer A output
  address_t m_nSenseB;        //   "     "    "  for timer B output
  uint8_t   m_bStatus;        // current status byte
  bool      m_fIRQ;           // true if either timer is interrupting
};
