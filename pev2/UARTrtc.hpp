//++
// UARTrtc.hpp -> PEV2 UART/RTC expansion board emulation
//
//   COPYRIGHT (C) 2015-2020 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
//
// LICENSE:
//    This file is part of the PicoElf emulator project.  EMULIB is free
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
// REVISION HISTORY:
// 29-JUL-22  RLA   New file.
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ std::string class, et al ...
#include <assert.h>             // assert() macro
#include "MemoryTypes.h"        // address_t and word_t data types
#include "Device.hpp"           // generic device definitions
#include "INS8250.hpp"          // UART emulation device
#include "DS12887.hpp"          // NVR/RTC emulation
using std::string;              // ...
class CEventQueue;              // ...
class C12887;                   // ...
class CINS8250;                 // ...

class CUARTrtc : public CDevice {
  //++
  //--

  // Constructor and destructor ...
public:
  CUARTrtc  (address_t nPort, CINS8250 *pUART, C12887 *pNVR);
  virtual ~CUARTrtc() {};
private:
  // Disallow copy and assignments!
  CUARTrtc(const CUARTrtc&) = delete;
  CUARTrtc& operator= (CUARTrtc const&) = delete;

  // CUARTrtc device methods ...
public:
  virtual void ClearDevice() override;
  virtual word_t DevRead (address_t nPort) override;
  virtual void DevWrite (address_t nPort, word_t bData) override;
  virtual void ShowDevice (ostringstream &ofs) const override;

  // Other public CUARTrtc methods ...
public:
  // Return pointers to our child objects ...
  CINS8250  *GetUART() {assert(m_pUART != NULL);  return m_pUART;}
  C12887 *GetNVR()  {assert(m_pNVR  != NULL);  return m_pNVR;}
  // Find a child device by name ...
  CDevice *FindDevice (string sName);

  // Private member data...
protected:
  uint8_t    m_bControl;        // last value written to the select register
  CINS8250  *m_pUART;           // UART emulation device
  C12887    *m_pNVR;            // NVR/RTC   "       "
};
