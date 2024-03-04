//++
// IDE11.hpp -> SBCT11 IDE disk interface emulation
//
// LICENSE:
//    This file is part of the emulator library project.  SBCT11 is free
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
//   This class emulates the IDE disk interface as used in the SBCT11.
//
// REVISION HISTORY:
// 11-JUL-22  RLA   New file.
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ std::string class, et al ...
#include "MemoryTypes.h"        // address_t and word_t data types
#include "EventQueue.hpp"       // CEventQueue declarations
#include "Device.hpp"           // generic device definitions
#include "IDE.hpp"              // IDE disk emulation
using std::string;              // ...

class CIDE11 : public CIDE {
  //++
  // SBCT11 IDE disk emulation ...
  //--

  // Magic constants ...
public:
  enum {
    WRITE_ONLY  = 000040,     // inhibit any read before write
    DATA_REG    = 0,          // IDE data register
    CS1FX       = 000020,     // select the CS1FX address space
    CS3FX       = 000000,     //   "     "  CS3FX   "  "    "
    PORT_COUNT  = 8*2*2*2,    // total number of ports required
  };

  // Constructor and destructor ...
public:
  CIDE11(address_t nPort, CEventQueue *pEvents);
  virtual ~CIDE11() {};
private:
  // Disallow copy and assignments!
  CIDE11(const CIDE11&) = delete;
  CIDE11& operator= (CIDE11 const &) = delete;

  // CIDE11 device methods ...
public:
//virtual void ClearDevice() override {CIDE::ClearDevice();}
  virtual word_t DevRead (address_t nAddress) override;
  virtual void DevWrite (address_t nAddress, word_t bData) override;
//virtual void ShowDevice (ostringstream &ofs) const override {CIDE::ShowDevice(ofs);}

  // Private member data...
protected:
};
