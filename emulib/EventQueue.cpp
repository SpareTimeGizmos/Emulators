//++
// EventQueue.cpp -> implementation of the CEventQueue class
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
//   The event queue is used by I/O devices and other objects to schedule
// something that needs to happen after a specific interval of simulated time
// (e.g. I/O done interrupt, character received on the serial port, etc).  An
// object may call the Schedule() method to add an entry to the event queue,
// and the object's EventHandler() method will be called when that time arrives.
//
//   A side function of the event queue, albeit a critical one, is that it also
// tracks the current simulated time.  The CPU object is expected to increment the
// simulated time as instructions are executed, and also to call our DoEvents()
// method at some point in the emulation main loop.
//
//   The event queue itself is a simple linked list of blocks containing a
// device pointer, an optional parameter for the device, and the time of the
// event.  The queue is sorted so that the next event is always at the front
// of the list.  To save a little time, when an event occurs we don't actually
// delete the event block; instead we add it to a free list where it can be
// reused for scheduling future events.  Free events aren't actually deleted
// until the Clear() method is called.  This minimizes the amount of memory
// allocation and deallocation needed.
//
//   IMPORTANT!
//   The event queue is currently a singleton object.  The constructor enforces
// the rule that there can be only one, and it saves a pointer to the object
// in the global g_pEventQueue.  The main reason for this is so that all the
// device emulators, and various other gizmos like timers and counters, can
// find the event queue so that they can schedule callbacks.
// 
//   Unfortunately, since the event queue belongs to the CCPU object, this
// limits us to one CPU instance per program.  That's not really a problem,
// since I don't have any plans to emulate any multiprocessor systems just yet.
// 
//   FWIW, yes I probably could have used std::list or (better yet!) std::set
// for the event queue, but there would be a few problems.  std::list is not
// ordered and so we'd either have to search for the correct insertion point,
// or resort the list, after every call to Schedule().  std::set is a better
// choice since it's already sorted, but set doesn't allow duplicate keys and
// we need to be able to schedule two or more events for the same time.  Yes,
// we could work around that, but it's super easy to implement our own ordered
// list or queue, so why bother?
//    
// REVISION HISTORY:
// 12-AUG-19  RLA   New file.
// 19-NOV-23  RLA   Invent CEventHandler and use it for all callbacks...
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include "EMULIB.hpp"           // emulator library definitions
#include "MemoryTypes.h"        // address_t and word_t data types
#include "LogFile.hpp"          // emulator library message logging facility
#include "CPU.hpp"              // generic CPU declarations
//#include "Device.hpp"           // needed to define the Event() method ...
#include "EventQueue.hpp"       // declarations for this module


CEventQueue::CEventQueue()
{
  //++
  // Constructor ...
  //--
  m_qCurrentTime = m_qNextEvent = 0;
  m_pNextEvent = m_pFreeEvents = NULL;
}

CEventQueue::~CEventQueue()
{
  //++
  // Destructor ...
  //--
  ClearEvents();
}

uint64_t CEventQueue::AddTime (uint64_t qTime)
{
  //++
  // Increment the current simulated time and return the new value ..
  //--
  assert(qTime > 0);
  m_qCurrentTime += qTime;
  return m_qCurrentTime;
}

uint64_t CEventQueue::JumpAhead (uint64_t qTime)
{
  //++
  // Jump ahead to the specified time, which MUST BE IN THE FUTURE!
  //--
  assert(qTime >= m_qCurrentTime);
  m_qCurrentTime = qTime;
  return m_qCurrentTime;
}

void CEventQueue::Schedule (CEventHandler *pHandler, intptr_t lParam, uint64_t qDelay)
{
  //++
  //   This routine will add a new event to the event queue.  The event queue
  // is _always_ sorted by time, so that the next event to occur is on the front
  // of the queue.  The global variable qEventTime is set to reflect the time of
  // the next event, and the main emulation loop uses this do decide whether it's
  // time to process events or not.
  //
  //   Note that the interval given is a delay and it's always relative to the
  // current time!
  //--
  EVENT *pEvent, *pLast, *pThis;

  //   First, allocate an event block.  Since doing malloc()s all the time is
  // fairly expensive, we keep a free list of old event blocks for re-use. Only
  // if this list is empty do we go to the trouble of a malloc()...
  if (m_pFreeEvents != NULL) {
    pEvent = m_pFreeEvents;  m_pFreeEvents = pEvent->pNext;
  } else
    pEvent = DBGNEW EVENT;

  // Fill it in...
  pEvent->pHandler = pHandler;  pEvent->lParam = lParam;
  pEvent->qTime = m_qCurrentTime + qDelay;  pEvent->pNext = NULL;

  //   Some devices can schedule events during initialization.  If the delays
  // for these devices are zero (because we want those devices to be really
  // fast!) then the event time we compute here will also be zero because,
  // at initialization time, qCurrentTime is zero!  Unfortunately a zero time
  // is interpreted as "no event pending", so things get confused.  This little
  // hack fixes that...
  if (pEvent->qTime == 0) pEvent->qTime = 1;

  LOGF(TRACE, "Scheduled event #%d for %s at %lld", lParam, pHandler->EventName(), pEvent->qTime);

  //   Do a simple insertion sort to stick it in the right place on the list.
  // Remember that the events that happen first - that is, those with the 
  // _smallest_ time value, are on the front of the list
  if ((m_pNextEvent == NULL) || (pEvent->qTime < m_pNextEvent->qTime)) {
    // Special case - insert at the head of the queue...
    pEvent->pNext = m_pNextEvent;  m_pNextEvent = pEvent;
    m_qNextEvent = pEvent->qTime;
  } else {
    // Insert this event somewhere in the middle of the list...
    pLast = m_pNextEvent;  pThis = pLast->pNext;
    while ((pThis != NULL) && (pEvent->qTime >= pThis->qTime))
      pLast = pThis, pThis = pThis->pNext;
    pEvent->pNext = pLast->pNext;  pLast->pNext = pEvent;
  }
}

void CEventQueue::Cancel (CEventHandler *pHandler, intptr_t lParam)
{
  //++
  //   This routine searches the event queue for entries that match the specified
  // callback (both the event routine AND the lParam must match!) and deletes them.
  // No other events on the queue are affected.  It's normally used by the device
  // emulator when a device reset type function is executed...
  //--
  EVENT *pThis = m_pNextEvent, *pLast = NULL, *pFree;
  LOGF(TRACE, "Cancelling all events #%d for %s", lParam, pHandler->EventName());
  while (pThis != NULL) {
    if ((pThis->pHandler != pHandler) || (pThis->lParam != lParam)) {
      // No match - on to the next item...
      pLast = pThis;  pThis = pThis->pNext;
    } else if (pLast == NULL) {
      // Remove the first item on the list!
      pFree = pThis;  m_pNextEvent = pThis->pNext;  pThis = pThis->pNext;
      pFree->pNext = m_pFreeEvents;  m_pFreeEvents = pFree;
      m_qNextEvent = (m_pNextEvent == NULL) ? 0 : m_pNextEvent->qTime;
    } else {
      // Remove an item from the middle of the list...
      pFree = pThis;  pLast->pNext = pThis->pNext;  pThis = pThis->pNext;
      pFree->pNext = m_pFreeEvents;  m_pFreeEvents = pFree;
    }
  }
}

void CEventQueue::CancelAllEvents()
{
  //++
  //   This routine cancels all current events for all devices but, unlike
  // ClearEvents(), this simply adds the event queue blocks to the free list
  // and does not reset the current emulation time.
  //--
  EVENT *pThis;
  LOGF(TRACE, "Clearing event queue");
  while (m_pNextEvent != NULL) {
    pThis = m_pNextEvent;  m_pNextEvent = pThis->pNext;
    pThis->pNext = m_pFreeEvents;  m_pFreeEvents = pThis;
  }
  m_qNextEvent = 0;
}

bool CEventQueue::IsPending (const CEventHandler *pHandler, intptr_t lParam) const
{
  //++
  //   This routine searches the event queue and returns TRUE if at least one
  // event is pending for the specified callback.  Both the callback pointer
  // AND the lParam must match!  The event queue is not changed by this call.
  //--
  EVENT *pEvent = m_pNextEvent;
  while (pEvent != NULL) {
    if ((pEvent->pHandler==pHandler) && (pEvent->lParam==lParam)) return true;
    pEvent = pEvent->pNext;
  }
  return false;
}

void CEventQueue::DoEvents()
{
  //++
  //   This function removes and executes all events from the queue which have
  // reached their scheduled time.  Any events that haven't arrived yet are left
  // alone.  Normally the CPU emulator calls this before every instruction ...
  //--
  EVENT *pEvent;

  // There's nothing to do unless qCurrentTime is .GE. qNextEvent ...
  if ((m_qNextEvent == 0) || (m_qCurrentTime < m_qNextEvent)) return;

  //   You need to be a little careful here, since a device's event routine may
  // very well schedule a new event.  This means that the event on the front of
  // the queue NOW may not be the same after we call the device's Event() method.
  // Likewise, it's possible, although not likely, that a device Event() method
  // will schedule a new event for which the time has already passed and in that
  // case the newly scheduled event needs to be executed immediately...
  while ((m_pNextEvent != NULL) && (m_pNextEvent->qTime <= m_qCurrentTime)) {
    // Remove this event from the queue, but don't free it yet!
    pEvent = m_pNextEvent;  m_pNextEvent = m_pNextEvent->pNext;
    // Execute the event procedure ...
    LOGF(TRACE, "Executing event #%d for %s", pEvent->lParam, pEvent->pHandler->EventName());
    pEvent->pHandler->EventCallback(pEvent->lParam);
    // Now free the event...
    pEvent->pNext = m_pFreeEvents;  m_pFreeEvents = pEvent;
  }
  m_qNextEvent = (m_pNextEvent == NULL) ? 0 : m_pNextEvent->qTime;
}

void CEventQueue::ClearEvents()
{
  //++
  //   This routine will remove, _without_ executing, all events from the event
  // queue.  And unlike DoEvents, this time the event blocks are actually freed.
  // Note that calling this method also resets the simulation time to zero!
  // This method is normally only called when the simulation is reset.
  //--
  EVENT *pFree;
  while (m_pNextEvent != NULL) {
    pFree = m_pNextEvent;  m_pNextEvent = m_pNextEvent->pNext;  delete pFree;
  }
  while (m_pFreeEvents != NULL) {
    pFree = m_pFreeEvents;  m_pFreeEvents = m_pFreeEvents->pNext;  delete pFree;
  }
  m_qNextEvent = m_qCurrentTime = 0;
  m_pNextEvent = m_pFreeEvents = NULL;
}
