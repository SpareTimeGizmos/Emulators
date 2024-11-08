//++
// Timer.hpp -> CTimer Generic programmable timer emulation
//
// DESCRIPTION:
//   This class implements a generic emulation of a programmable timer as is
// used in many chips, such as the 8253/4, 8155/6, NSC810, etc.  It's not all
// that useful by itself, but is intended to be used as a base class for
// system specific emulations.
//
// REVISION HISTORY:
//  3-JUL-23  RLA   New file.
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ std::string class, et al ...
#include "MemoryTypes.h"        // address_t and word_t data types
#include "EventQueue.hpp"       // event queue class
#include "CPU.hpp"              // needed for HZTONS and NSTOHZ ...
using std::string;              // ...

// Specific Timer types ...
enum _TIMER_TYPES {
  TIMER_UNKNOWN =    0,     // undefined
  TIMER_I8254   = 8254,     // Intel 8253/4 triple timer
  TIMER_I8155   = 8155,	    // Intel 8155/6 RAM-I/O-TIMER
  TIMER_NSC810	=  810,	    // National NSC810 (partner to the NSC800)
  TIMER_CDP1878 = 1878,     // RCA CDP1878 dual counter/tmer
};
typedef enum _TIMER_TYPES TIMER_TYPE;

// Timer callback functions ...
typedef void (TIMER_CALLBACK) (class CDevice *pDevice, class CTimer *pTimer);


class CTimer : CEventHandler {
  //++
  // Generic "Programmable timer" emulation ...
  //--

  // Local definitions
public:
  // Possible timer modes ...
  enum _TIMER_MODES {
    TIMER_STOPPED,          // timer does nothing
    TIMER_ONESHOT,          // count down to zero and then stop
    TIMER_REPEAT,           // count down, reload, and count again
  };
  typedef enum _TIMER_MODES TIMER_MODE;

  // Constructor and destructor ...
public:
  CTimer (unsigned nTimer, CEventQueue *pEvents, uint32_t lPeriod=0);
  virtual ~CTimer() {};
private:
  // Disallow copy and assignments!
  CTimer (const CTimer &) = delete;
  CTimer& operator= (CTimer const &) = delete;

  // Public properties ...
public:
  // Return the timer index or name ...
  //   Most counter/timer chips have multiple units, and the index property
  // allows the parent device to keep track of which is which.  The name is
  // used for messages ...
  unsigned GetIndex() const {return m_nTimer;}
  const char *GetName() const {return m_strName.c_str();}
  // Get or Set the timer clock frequency ...
  void SetPeriod (uint32_t lPeriod)
    {assert(lPeriod != 0);  m_lPeriod = lPeriod;}
  void SetClock (uint32_t lFrequency)
    {assert(lFrequency != 0);  SetPeriod(HZTONS(lFrequency));}
  uint32_t GetPeriod() const {return m_lPeriod;}
  uint32_t GetClock() const {return NSTOHZ(m_lPeriod);}
  // Set the timer callback function ...
  void SetCallback (TIMER_CALLBACK *pCallback, class CDevice *pDevice)
    {assert(pCallback != NULL);  m_pCallback = pCallback;  m_pParent = pDevice;}
  // Set or get the timer mode ...
  void SetMode(TIMER_MODE nMode) {m_nMode = nMode;}
  TIMER_MODE GetMode() const {return m_nMode;}
  //   Get or set the interrupt enable flag ...   This class does nothing with
  // interrupts, however most timer chips have individual interrupt enables for
  // each counter/timer unit and it's convenient to let this object store it.
  void SetIEN (bool fIEN=true) {m_fIEN = fIEN;}
  bool GetIEN() const {return m_fIEN;}

  // CEventHandler methods ...
public:
  virtual void EventCallback (intptr_t lParam) override;
  virtual const char *EventName() const override {return GetName();}

  // Public methods ...
public:
  // Start or stop the timer ...
  virtual void Start (bool fJam=false);
  virtual void Stop();
  // Reset the timer ...
  virtual void Clear();
  //   Set the timer count jam register (16 bits!) ...  Note that it's possible
  // to load the jam register even while the timer is running, but the change
  // won't take effect until the next reload.  And be careful using WriteH/L
  // while running - you could inadvertently load the wrong value if the reload
  // happens in between the two calls!
  virtual void Write (uint16_t wCount)  {m_wJam = wCount;}
  virtual void WriteH (uint8_t bCountH) {m_wJam = MKWORD(bCountH, LOBYTE(m_wJam));}
  virtual void WriteL (uint8_t bCountL) {m_wJam = MKWORD(HIBYTE(m_wJam), bCountL);}
  // Load the current count register from the jam register ...
  virtual void Jam() {m_wCount = m_wJam;}
  // Freeze the holding register for reading ..
  virtual void Freeze (bool fFreeze=true) {m_fFreeze = fFreeze;}
  //   Read the timer count holding register (16 bits) ...   It is possible to
  // read the holding register even if the count is not frozen, but you may get
  // the wrong results if the timer updates in between calls to ReadH() and
  // ReadL().  Calling Read() should be safe any time, however.
  virtual uint16_t Read() const {return m_wHold;}
  virtual uint8_t ReadH() const {return HIBYTE(m_wHold);}
  virtual uint8_t ReadL() const {return LOBYTE(m_wHold);}
  // Gate (enable or disable) counting ...
  void Enable (bool fGate=true) {m_fEnabled = fGate;}
  bool IsEnabled() const {return m_fEnabled;}
  // Show the timer status ...
  virtual void Show(ostringstream &ofs) const;
  static string ModeToString (TIMER_MODE nMode);

  // Local methods ...
private:

  // Private member data...
private:
  unsigned    m_nTimer;       // timer index for parent device
  string      m_strName;      // "name" of this timer for messages
  TIMER_MODE  m_nMode;        // timer mode selected
  bool        m_fEnabled;     // true if the timer is running
  bool        m_fFreeze;      // true if the holding register is frozen
  uint16_t    m_wJam;         // "jam" count used to reset/recycle the counter
  uint16_t    m_wCount;       // current count
  uint16_t    m_wHold;        // frozen count for reading
  uint32_t    m_lPeriod;      // counting period, in nanoseconds
  bool        m_fIEN;         // local interrupt enable for this timer
  CEventQueue    *m_pEvents;  // "to do" list of upcoming events
  TIMER_CALLBACK *m_pCallback;// callback function for terminal count
  class CDevice  *m_pParent;  // parent CDevice that owns this timer
};
