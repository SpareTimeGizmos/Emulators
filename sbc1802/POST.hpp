//++
// POST.hpp -> DIP Switches and 7 Segment LED Display definitions
//
//   COPYRIGHT (C) 2015-2024 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
//
// LICENSE:
//    This file is part of the SBC1802 emulator project.  SBC1802 is free
// software; you may redistribute it and/or modify it under the terms of
// the GNU Affero General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any
// later version.
//
//    SBC1802 is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License
// for more details.  You should have received a copy of the GNU Affero General
// Public License along with SBC1802.  If not, see http://www.gnu.org/licenses/.
//
// DESCRIPTION:
//   These two classes emulate the 8 DIP switches and the 7 segment LED display
// used on the SBC1802.  The switches are input device #4 and the LEDs are output
// device #4.  Neither is affected by any group select hardware!
//
// REVISION HISTORY:
// 17-JUN-22  RLA   New file.
//--
#pragma once
#include <stdint.h>
#include <string>               // C++ std::string class, et al ...
#include "MemoryTypes.h"        // address_t and word_t data types
#include "CPU.hpp"              // CPU definitions
#include "Device.hpp"           // generic device definitions
using std::string;              // ...

class CLEDS : public CDevice {
  //++
  // SBC1802 7 segment LED display emulation ...
  //--

public:
  enum {
    // Bits to display segment mapping ...
    SEGA=0x01, SEGB=0x02, SEGC=0x04, SEGD=0x08,
    SEGE=0x10, SEGF=0x20, SEGG=0x40, SEGDP=0x80,
    //   And these are the post codes, F..0, displayed using the traditional seven
    // segment hexadecimal font.  Note that the decimal point is not used ...
    POSTF = SEGA                      + SEGE + SEGF + SEGG,
    POSTE = SEGA               + SEGD + SEGE + SEGF + SEGG,
    POSTD =        SEGB + SEGC + SEGD + SEGE        + SEGG,
    POSTC = SEGA               + SEGD + SEGE + SEGF,
    POSTB =               SEGC + SEGD + SEGE + SEGF + SEGG,
    POSTA = SEGA + SEGB + SEGC        + SEGE + SEGF + SEGG,
    POST9 = SEGA + SEGB + SEGC               + SEGF + SEGG,
    POST8 = SEGA + SEGB + SEGC + SEGD + SEGE + SEGF + SEGG,
    POST7 = SEGA + SEGB + SEGC,
    POST6 = SEGA        + SEGC + SEGD + SEGE + SEGF + SEGG,
    POST5 = SEGA        + SEGC + SEGD        + SEGF + SEGG,
    POST4 =        SEGB + SEGC               + SEGF + SEGG,
    POST3 = SEGA + SEGB + SEGC + SEGD               + SEGG,
    POST2 = SEGA + SEGB        + SEGD + SEGE        + SEGG,
    POST1 = SEGB        + SEGC,
    POST0 = SEGA + SEGB + SEGC + SEGD + SEGE + SEGF
  };

public:
  // Constructor and destructor...
  CLEDS (address_t nPort);
  virtual ~CLEDS() {};
private:
  // Disallow copy and assignments!
  CLEDS (const CLEDS &) = delete;
  CLEDS& operator= (CLEDS const &) = delete;

  // Public POST display properties
public:
  // Get the last POST code displayed ...
  virtual uint8_t GetPOST() const {return m_bPOST;}

  // CDevice methods implemented by the POST display ...
public:
  // Clear (i.e. a hardware reset) this device ...
  virtual void ClearDevice() override {m_bPOST=0;}
  // Write to the device ...
  virtual void DevWrite (address_t nPort, word_t bData) override;
  // Dump the device state for the user ...
  virtual void ShowDevice (ostringstream &ofs) const override;

  // Private methods ...
private:
  static string DecodePOST (uint8_t bPOST);

  // Private member data...
protected:
  uint8_t m_bPOST;              // last byte sent to the display
};

class CSwitches : public CDevice {
  //++
  // SBC1802 DIP configuration switches emulation ...
  //--

public:
  // Constructor and destructor...
  CSwitches (address_t nPort);
  virtual ~CSwitches() {};
private:
  // Disallow copy and assignments!
  CSwitches (const CSwitches&) = delete;
  CSwitches& operator= (CSwitches const &) = delete;

  // Public CSwitches properties ...
public:
  // Set the switches (for the SET SWITCHES command ...) ...
  void SetSwitches (uint8_t bData) {m_bSwitches=bData;}
  uint8_t GetSwitches() const {return m_bSwitches;}
  // Set the attention request F-F (i.e. push the INPUT button!) ...
  void RequestAttention (bool fAttention=true);

  //  CDevice methods implemented by the switches ...
public:
  // Clear (i.e. a hardware reset) this device ...
  virtual void ClearDevice() override;
  // Read from a device register ...
  virtual word_t DevRead (address_t nPort) override;
  // Return the state of the input/attention flag ...
  virtual uint1_t GetSense (address_t nSense, uint1_t bDefault=0) override {return m_fAttention;}
  // Dump the device state for the user ...
  void ShowDevice (ostringstream &ofs) const override;

  // Private member data...
protected:
  uint8_t m_bSwitches;        // current switch register settings
  bool    m_fAttention;       // INPUT/ATTENTION request state
};
