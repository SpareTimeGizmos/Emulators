//++
// EventQueue.hpp -> definitions for the CEventQueue class
//
//   COPYRIGHT (C) 2015-2024 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
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
// DESCRIPTION:
//   The Event Queue is used by I/O devices to schedule something that needs
// to happen after a specific interval of simulated time (e.g. I/O done
// interrupt, character received on the serial port, etc).  A device may call
// the Schedule() method to add an entry to the event queue, and the object's
// EventCallback() method will be called when that time arrives.
//
//   A side function of the event queue, albeit a critical one, is that it also
// tracks the current simulated time.  The CPU object is expected to increment
// the simulated time as instructions are executed, and also to call our
// DoEvents() method at some point in the emulation main loop.
//
// REVISION HISTORY:
// 12-AUG-19  RLA   New file.
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ std::string class, et al ...
using std::string;              // ...
class CDevice;                  // we need pointers to device objects


class CEventHandler {
  //++
  //   This abstract class must be inherited (and implemented) by any object
  // that wants to receive event handler callbacks ...
  //--

  // No constructor or destructor (or rather, the default which does nothing) ...

  // This is the callback from the event handler ...
public:
  virtual void EventCallback (intptr_t lParam) = 0;

  //   And this routine should return the "name" of this event/callback.  It's
  // used only by the event queue code to print debugging messages, so it's
  // up to you whether you want to implement it.
public:
  virtual const char *EventName() const {return "unknown";}
};


class CEventQueue {
  //++
  // Schedule events to occur at some (simulated) time in the future ...
  //--

  // Event queue structure ...
private:
  //   Device actions (e.g. completing an I/O) are scheduled by means of
  // an event queue.  Entries on the queue have a scheduled time and the
  // pointer to a device callback function which processes the event.
  struct _EVENT {		// structure of event queue entries
    uint64_t         qTime;     //  simulated CPU time when this event occurs
    CEventHandler   *pHandler;  //  pointer to callback routine
    intptr_t	     lParam;    //  arbitrary parameter for the event routine
    struct _EVENT   *pNext;     //  next event in the queue
  };
  typedef struct _EVENT EVENT;

public:
  // Constructor and destructor ...
  CEventQueue();
  virtual ~CEventQueue();
private:
  // Disallow copy and assignments!
  CEventQueue (const CEventQueue&) = delete;
  CEventQueue& operator= (CEventQueue const&) = delete;

  // Simulation time methods ...
public:
  // Return the current simulated time ...
  uint64_t CurrentTime() const  {return m_qCurrentTime;}
  // Increment the simulated time ...
  uint64_t AddTime (uint64_t qTime);
  // Jump ahead (forward only!) to the specified time ...
  uint64_t JumpAhead (uint64_t qTime);
  // Return the time of the next event scheduled ...
  uint64_t NextEvent() const {return m_qNextEvent;}
  //   Note that the only way to reset the simulated time to zero is by
  // calling the Clear() method, which will clear the entire event queue ...

  // Event queue methods ...
public:
  // Clear the event queue and all pending events ...
  void ClearEvents();
  // Schedule an upcoming event at a specific simulated time ...
  void Schedule (CEventHandler *pHandler, intptr_t lParam, uint64_t qDelay);
  // Cancel an already scheduled event ...
  void Cancel (CEventHandler *pHandler, intptr_t lParam);
  // Cancel all pending events w/o resetting the emulation time
  void CancelAllEvents();
  // Return TRUE if the device has one or more events pending ...
  bool IsPending (const CEventHandler *pHandler, intptr_t lParam) const;
  // Process all current events ...
  void DoEvents();

  // CEventQueue members ...
private:
  uint64_t  m_qCurrentTime; // current simulation virtual time
  uint64_t  m_qNextEvent;   // time of the next scheduled event
  EVENT    *m_pNextEvent;   // root of the event queue and the next event
  EVENT    *m_pFreeEvents;  // list of free event blocks for re-use
};
