//++
// LTC11.hpp -> SBCT11 Line Time Clock
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
//   This class defines the SBCT11 line time clock.
//
// REVISION HISTORY:
// 10-JUL-22  RLA   New file.
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ std::string class, et al ...
#include "Device.hpp"           // generic device definitions
using std::string;              // ...
class CEventQueue;              // ...


class CLTC11 : public CDevice {
  //++
  // SBCT11 Line Time clock emulation ...
  //--

  // Constants and magic numbers ...
public:
  enum {
    HERTZ         = 60,     // line frequency in Hertz
    // Bits in the LTC CSR ...
    LTC_FLAG      = 0200,   // LTC "tick" flag
    LTC_ENABLE    = 0100,   // LTC enable bit
    // DeviceEvent() callbacks (there's only one!) ...
    EVENT_TICK    = 1,      // clock tick event
  };

  // Constructor and destructor ...
public:
  CLTC11 (address_t nBase, CEventQueue *pEvents);
  virtual ~CLTC11() {};
private:
  // Disallow copy and assignments!
  CLTC11(const CLTC11&) = delete;
  CLTC11& operator= (CLTC11 const &) = delete;

  // Device methods from CDevice ...
public:
  virtual void ClearDevice() override;
  virtual uint8_t DevRead (address_t nPort) override;
  virtual void DevWrite (address_t nPort, uint8_t bData) override;
  virtual void EventCallback (intptr_t lParam) override;
  virtual void AttachInterrupt (CSimpleInterrupt *pInterrupt);

  // Private methods ...
private:

  // Private member data...
protected:
  bool    m_fEnable;      // TRUE if clock interrupts are enabled
};
