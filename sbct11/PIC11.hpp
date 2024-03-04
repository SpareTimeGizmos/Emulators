//++
// PIC11.hpp -> DCT11 Priority Interrupt Controller definitions
//
//   COPYRIGHT (C) 2015-2024 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
// 
// LICENSE:
//    This file is part of the SBCT11 emulator project.  SBCT11 is free
// software; you may redistribute it and/or modify it under the terms of
// the GNU Affero General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any
// later version.
//
//    SBCT11 is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License
// for more details.  You should have received a copy of the GNU Affero General
// Public License along with SBCT11.  If not, see http://www.gnu.org/licenses/.
//
// DESCRIPTION:
//   Emulate the functions of the SBCT11 Interrupt and Acknowledge GALs.
//
// REVISION HISTORY:
//  7-JUL-22  RLA   New file.
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ std::string class, et al ...
#include "MemoryTypes.h"        // address_t and word_t data types
#include "Interrupt.hpp"        // generic priority interrupt controller
using std::string;              // ...


class CPIC11 : public CInterrupt
{
  //++
  // SBCT11 and DCT11 interrupt emulation ...
  //--
  friend class CDCT11;

  // Magic numbers and constants ...
public:
  //   The DCT11 has a 4 bit binary coded interrupt request input which allows
  // selection of any one of 15 built in interrupt vectors.  The DEC manuals
  // call these four bits CP0..3 (short for "Coded Priority")...
  enum {
    IRQLEVELS = 15,           // number of DCT11 predefined interrupt vectors
  };
  typedef uint8_t IRQ_t;      // a DCT11 CP input code

  // Constructor and destructor ...
public:
  CPIC11();
  virtual ~CPIC11();
private:
  // Disallow copy and assignments!
  CPIC11(const CPIC11&) = delete;
  CPIC11& operator= (CPIC11 const &) = delete;

  // Public properties ...
public:
  // Return a pointer to a specific interrupt level ...
  inline CSimpleInterrupt *GetLevel (IRQ_t n)
    {assert((n > 0) && (n <= IRQLEVELS));  return m_pLevel[n-1];}
  inline CSimpleInterrupt const *GetLevel (IRQ_t n) const
    {assert((n > 0) && (n <= IRQLEVELS));  return m_pLevel[n-1];}
  // Allow array style syntax for the same thing ...
  inline CSimpleInterrupt* operator[] (IRQ_t n) {return GetLevel(n);}
  inline CSimpleInterrupt const * operator[] (IRQ_t n) const {return GetLevel(n);}
  // Get or set the level/edge triggered mode for a level ...
  CSimpleInterrupt::INTERRUPT_MODE GetMode (IRQ_t n) const
    {return GetLevel(n)->GetMode();}
  void SetMode (IRQ_t n, CSimpleInterrupt::INTERRUPT_MODE nMode)
    {return GetLevel(n)->SetMode(nMode);}
  // Return TRUE if an interrupt is requested one specific level ...
  bool IsRequestedAtLevel (IRQ_t n) const
    {return GetLevel(n)->IsRequested();}

  // CPIC11 interrupt methods from CInterrupt ...
public:
  // Clear all interrupt requests ...
  virtual void ClearInterrupt() override;
  //  These two methods are not used on the T11, so assert if they're ever
  // called.  Use FindRequest() and AcknowledgeRequest(nIRQ) instead!
  virtual bool IsRequested() const override {assert(false); return false;}
  // Acknowledge an interrupt request ...
  virtual void AcknowledgeRequest() override {assert(false);}

  // Unique public CPIC11 methods ...
public:
  // Return the CP code for the highest priority active interrupt request ...
  IRQ_t FindRequest (uint8_t bPSW=0);
  // Acknowledge the interrupt request on the specified CP ...
  void AcknowledgeRequest (IRQ_t nIRQ);
  // Return the vector associated with a given CP ...
  static address_t GetVector (IRQ_t n)
    {assert(n <= IRQLEVELS);  return (n==0) ? 0 : g_awVectors[n-1];}
  // Return the priority (i.e. bus request level) associated with a given CP ...
  static uint8_t GetPriority (IRQ_t n)
    {assert(n <= IRQLEVELS);  return (n==0) ? 0 : g_abPriority[n-1];}
  // Find the IRQ index that matches the given CSimpleInterrupt channel ...
  IRQ_t FindInterrupt (CSimpleInterrupt const *pInterrupt) const;

  // Private methods ...
protected:

  // Private member data...
protected:
  CSimpleInterrupt *m_pLevel[IRQLEVELS];// simple interrupt for each level
  IRQ_t             m_nLastIRQ;         // last IRQ returned by FindRequest()

  // Static tables ...
protected:
  static const uint8_t   g_abPriority[IRQLEVELS]; // BR associated with each CP
  static const address_t g_awVectors[IRQLEVELS];  // vector  "   "   "   "   "
};
