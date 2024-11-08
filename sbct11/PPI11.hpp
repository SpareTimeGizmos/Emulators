//++
// PPI11.hpp -> SBCT11 8255 PPI parallel port emulation
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
//   This class implements the 8255 PPI as it is used in the SBCT11.  Port A
// and the handshaking bits of port C are used to implement a general purpose
// bidirectional 8 bit parallel port, which could possibly be Centronics
// printer compatible.  The remaining bits in port B are used to control the
// POST display and the RUN LED.
//
// REVISION HISTORY:
//  7-JUL-22  RLA   New file.
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ std::string class, et al ...
#include "CPU.hpp"              // CPU definitions
#include "Interrupt.hpp"        // CInterrupt definitions
#include "Device.hpp"           // generic device definitions
#include "i8255.hpp"            // Intel 8255 PPI base class
using std::string;              // ...


class CPPI11 : public C8255 {
  //++
  // SBCT11 Parallel port emulation ...
  //--

  // Constructor and destructor ...
public:
  CPPI11 (const char *pszName, address_t nBase, CEventQueue *pEvents);
  virtual ~CPPI11() {};
private:
  // Disallow copy and assignments!
  CPPI11 (const CPPI11&) = delete;
  CPPI11& operator= (CPPI11 const &) = delete;

  // CPPI11 device methods from CDevice ...
public:
  virtual void ClearDevice() override;
  virtual uint8_t DevRead (address_t nPort) override;
  virtual void DevWrite (address_t nPort, uint8_t bData) override;
  virtual void ShowDevice (ostringstream &ofs) const override;

  // Overridden methods from C8255 ...
public:
  virtual void OutputB (uint8_t bData) override;

  // Private methods ...
private:

  // Private member data...
protected:
  uint8_t     m_bPOST;        // last POST code written to port B
};
