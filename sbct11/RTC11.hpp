//++
// RTC11.hpp -> SBCT11 Real Time Clock
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
//   This class defines the SBCT11 DS12887 real time clock.
//
// REVISION HISTORY:
// 11-JUL-22  RLA   New file.
// 16-SEP-25  RLA   Add m_fEnable to allow the chip to be "uninstalled".
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ std::string class, et al ...
#include "Device.hpp"           // generic device definitions
using std::string;              // ...
class CEventQueue;              // ...
class C12887;                   // ...


class CRTC11 : public CDevice {
  //++
  // SBCT11 Real Time clock emulation ...
  //--

  // Constants and magic numbers ...
public:
  // RTC register offsets (from the base address) ...
  enum {
    REG_READ      = 0,      // read data from the DS12887
    REG_WRITE     = 2,      // write data to the DS12887
    REG_ADDRESS   = 6,      // load the DS12887 address
    REG_COUNT     = 8,      // total bytes in the 12887 address space
  };

  // Constructor and destructor ...
public:
  CRTC11 (address_t nBase, CEventQueue *pEvents, bool fOldPCB=false);
  virtual ~CRTC11();
private:
  // Disallow copy and assignments!
  CRTC11(const CRTC11&) = delete;
  CRTC11& operator= (CRTC11 const &) = delete;

  // RTC11 specific properties ...
public:
  // Return the DS12887 chip that is the actual RTC/NVR ...
  C12887 *Get12887() const {return m_p12887;}
  // Get or set the new vs old PCB emulation ...
  bool IsOldPCB() const {return m_fOldPCB;}
  void SetOldPCB (bool fOldPCB=true) {m_fOldPCB = fOldPCB;}
  // Enable or disable the RTC chip ...
  void Enable (bool fEnable=true) {m_fEnabled = fEnable;}
  bool IsEnabled() const {return m_fEnabled;}

  // Device methods from CDevice ...
public:
  virtual void ClearDevice() override;
  virtual uint8_t DevRead (address_t nPort) override;
  virtual void DevWrite (address_t nPort, uint8_t bData) override;
  virtual void ShowDevice (ostringstream &ofs) const override;
  
  // Local methods ...
private:
  uint8_t ReadByte (bool fOdd);
  void WriteByte (uint8_t bData, bool fOdd);

  // Private member data...
protected:
  uint8_t   m_bAddress;   // last DS12887 address selected
  C12887   *m_p12887;     // this does all the real work!
  bool      m_fOldPCB;    // TRUE if we're emulating the old PCB revision
  uint16_t  m_wCache;     // high/low byte cache for read/write for old PCBs
  bool      m_fEnabled;   // TRUE if the DS12887 chip is installed
};
