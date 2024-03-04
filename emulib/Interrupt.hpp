//++
// Interrupt.hpp -> CInterrupt (generic interrupt) interface
//                  CSimpleInterrupt (basic "wire OR" interrupt) class
//                  CPriorityInterrpt (multilevel priority interrupt) class
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
//   This file gives the definitions for three classes - CInterrupt is an
// abstract class which defines the common interface to the interrupt system.
// This is used by the CPU and peripheral device implementations and allows
// them to be independent of the actual interrupt implementation.
//
//   CSimpleInterrupt is (what else?) a simple, single level, interrupt
// emulation where all device interrupt requests are logically ORed together
// to produce an interrupt request for the CPU.  The CPU interrupt may be
// either level sensitive or edge triggered.
// 
//   CPriorityInterrupt emulates a multilevel priority interrupt system.  Each
// level is an independent CSimpleInterrupt object with one or more devices
// attached.  The CPriorityInterrupt object then interfaces with the CPU (in
// some implementation specific way) to identify the highest priority interrupt
// and service interrupts in priority order.
//
// REVISION HISTORY:
// 23-JUN-22  RLA   New file.
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include "EMULIB.hpp"           // generic project wide declarations


class CInterrupt {
  //++
  // Abstract interrupt system interface for CPUs ...
  //--

  // This is an abstract class - no constructor or destructor here!

  // Interrupt methods ...
public:
  // Return TRUE if any interrupt is requested ...
  virtual bool IsRequested() const = 0;
  // Acknowledge an interrupt request ...
  virtual void AcknowledgeRequest() = 0;
  // Clear all interrupt requests ...
  virtual void ClearInterrupt() = 0;
};


class CSimpleInterrupt : public CInterrupt {
  //++
  // Basic, wire-OR, single level interrupt system emulation ...
  //--

  // Magic numbers and constants ...
public:
  enum {
    //    NOTE - the number of devices per level is really determined by the
    // number of bits in an IRQMASK variable.  You CAN'T change MAXDEVICE
    // without rewriting some code!
    MAXDEVICE   = 32,       // maximum number of devices per CSimpleInterrupt
  };
  //   An IRQMASK must contain at least MAXDEVICE bits because one mask bit
  // is allocated to each device.
  typedef uint32_t IRQMASK;
  // Interrupt modes - level triggered or edge triggered ...
  enum _INTERRUPT_MODES {LEVEL_TRIGGERED, EDGE_TRIGGERED};
  typedef enum _INTERRUPT_MODES INTERRUPT_MODE;

public:
  // Constructor and destructor ...
  CSimpleInterrupt (INTERRUPT_MODE nMode=LEVEL_TRIGGERED);
  virtual ~CSimpleInterrupt() {};
private:
  // Disallow copy and assignments!
  CSimpleInterrupt (const CSimpleInterrupt &) = delete;
  CSimpleInterrupt& operator= (CSimpleInterrupt const &) = delete;

  // Interrupt methods ...
public:
  // Get or change the level/edge triggered mode ...
  INTERRUPT_MODE GetMode() const {return m_nMode;}
  void SetMode (INTERRUPT_MODE nMode) {m_nMode = nMode;}
  // Allocate or free interrupt request bits/masks ...
  IRQMASK AllocateMask();
  void ReleaseMask (IRQMASK lMask);
  // Request (or clear a request) an interrupt ...
  void Request (IRQMASK lMask, bool fInterrupt=true);
  // Return TRUE if any interrupt is requested ...
  virtual bool IsRequested() const override {return m_fRequested;}
  // Return TRUE if an interrupt is requested by the specified device ...
  virtual bool IsRequested (IRQMASK lMask) const {return ISSET(m_lRequests, lMask);}
  // Acknowledge an interrupt request ...
  virtual void AcknowledgeRequest() override;
  // Clear all interrupt requests ...
  virtual void ClearInterrupt() override;
  // Return true if any device is attached to this interrupt ...
  virtual bool IsAttached() const {return m_lMasksUsed != 0;}
  
  // CSimpleInterrupt members ...
private:
  INTERRUPT_MODE m_nMode;       // level or edge triggered
  bool           m_fRequested;  // TRUE if an interrupt is requested now!
  IRQMASK        m_lMasksUsed;  // mask of interrupt masks that are used
  IRQMASK        m_lRequests;   // mask of devices requesting interrupt
};


class CPriorityInterrupt {
  //++
  //  Multi-level priority interrupt system emulation ...
  //--

  // Magic numbers and constants ...
public:
  enum {
    //   MAXLEVEL is the maximum number of priority levels that can be supported,
    // although the actual number in any given implementation may be less.  Note
    // that this constant is related to both the IRQLEVEL and IRQVECTOR types -
    // see the comments above...
    MAXLEVEL    = 8,          // maximum number of priority levels
  };
  //   IRQLEVEL should be big enough to hold a CPriorityInterrupt::MAXLEVEL
  // constant (i.e. LOG2(MAXLEVEL) bits!).  IRQVECTOR is a bitmap of active
  // priority requests, and should contain at least MAXLEVEL bits.
  typedef uint8_t IRQLEVEL;   // an interrupt level
  typedef uint8_t IRQVECTOR;  // mask of active interrupts

public:
  // Constructor and destructor ...
  CPriorityInterrupt (IRQLEVEL nLevels=MAXLEVEL,
    CSimpleInterrupt::INTERRUPT_MODE nMode=CSimpleInterrupt::EDGE_TRIGGERED);
  virtual ~CPriorityInterrupt();
private:
  // Disallow copy and assignments!
  CPriorityInterrupt (const CPriorityInterrupt &) = delete;
  CPriorityInterrupt& operator= (CPriorityInterrupt const &) = delete;

  // CPriorityInterrupt properties ...
public:
  // Return the number of levels implemented ...
  IRQLEVEL GetLevels() const {return m_nLevels;}
  // Return a pointer to a specific interrupt level ...
  inline CSimpleInterrupt *GetLevel (IRQLEVEL n)
    {assert((n > 0) && (n <= m_nLevels));  return m_pLevel[n-1];}
  inline CSimpleInterrupt const *GetLevel (IRQLEVEL n) const
    {assert((n > 0) && (n <= m_nLevels));  return m_pLevel[n-1];}
  // Get or set the level/edge triggered mode for a level ...
  CSimpleInterrupt::INTERRUPT_MODE GetMode (IRQLEVEL n) const
    {return GetLevel(n)->GetMode();}
  void SetMode (IRQLEVEL n, CSimpleInterrupt::INTERRUPT_MODE nMode)
    {return GetLevel(n)->SetMode(nMode);}
  // Return TRUE if an interrupt is requested one specific level ...
  bool IsRequestedAtLevel (IRQLEVEL n) const
    {return GetLevel(n)->IsRequested();}
  // Return a bitmap of requests ...
  IRQVECTOR GetRequests() const;

  // Interrupt methods ...
public:
  // Acknowledge any interrupt on nLevel ...
  virtual void AcknowledgeRequest (IRQLEVEL n) {GetLevel(n)->AcknowledgeRequest();}
  // Clear all interrupt requests on all levels ...
  virtual void ClearInterrupt();

  // Return TRUE if any interrupt is requested at nLevel OR ABOVE ...
//virtual bool IsRequested (IRQLEVEL nLevel=1) const;
  // Return the highest priority level with an active request ...
 //virtual IRQLEVEL RequestedLevel() const;

  // Allocate or free interrupt request bits/masks ...
//IRQMASK AllocateMask (IRQLEVEL nLevel);
//void ReleaseMask (IRQLEVEL nLevel, IRQMASK lMask);
  // Request (or clear a request) an interrupt ...
//virtual void Request (IRQLEVEL nLevel, IRQMASK lMask, bool fInterrupt=true);
  // Clear all interrupt requests on a particular level ...
//virtual void ClearLevel (IRQLEVEL nLevel);
  // Clear all interrupt requests on all levels ...
//virtual void ClearAll();

  // CInterrupt members ...
private:
  IRQLEVEL          m_nLevels;          // number of levels implemented
  CSimpleInterrupt *m_pLevel[MAXLEVEL]; // simple interrupt for each level
};
