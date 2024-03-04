//++
// MiscellaneousIOTs.hpp -> SBC6120 "miscellaneous" IOTs
//
//   Copyright (C) 1999-2022 by Spare Time Gizmos.  All rights reserved.
//
// LICENSE:
//    This file is part of the SBC6120 emulator project. SBC6120 is free
// software; you may redistribute it and/or modify it under the terms of
// the GNU Affero General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any
// later version.
//
//    SBC6120 is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License
// for more details.  You should have received a copy of the GNU Affero General
// Public License along with SBC6120.  If not, see http://www.gnu.org/licenses/.
//
// DESCRIPTION:
//   This file defines a class for the SBC6120 641x IOTs, which perform
// miscellaneous functions - skip on battery low, load disk address register,
// select SLU secondary mode, etc.
// 
//   It also defines another class which implements the IOTs for the FP6120
// front panel.  That's not emulated here and the SBC6120 firmware doesn't
// require that it be present, but we still need to define all the associated
// IOTs as NOPs.
//
// REVISION HISTORY:
// 22-AUG-22  RLA   New file.
//--
#pragma once
#include <stdint.h>
#include <string>               // C++ std::string class, et al ...
#include "MemoryTypes.h"        // address_t and word_t data types
#include "CPU.hpp"              // CPU definitions
#include "Device.hpp"           // generic device definitions
using std::string;              // ...
class CRAMdisk;                 // ...
class CIDEdisk;                 // ...
class CSLU;                     // ...


class CIOT641x : public CDevice
{
  //++
  // SBC6120 Miscellaneous IOTs 641x ...
  //--

public:
  // Constructor and destructor...
  CIOT641x (word_t nIOT, CSLU *pSLU, CRAMdisk *pRAMdisk, CIDEdisk *pIDEdisk);
  virtual ~CIOT641x() {};
private:
  // Disallow copy and assignments!
  CIOT641x (const CIOT641x &) = delete;
  CIOT641x& operator= (CIOT641x const &) = delete;

  // Device methods from CDevice ...
public:
  virtual bool DevIOT (word_t wIR, word_t &wAC, word_t &wPC) override;
  virtual void ShowDevice (ostringstream &ofs) const override;

  // Member variables ...
private:
  CRAMdisk  *m_pRAMdisk;      // needed for the LDAR and SBBLO opcodes
  CIDEdisk  *m_pIDEdisk;      // needed for SDASP ...
  CSLU      *m_pSLU;          // needed for PRISLU and SECSLU ...
};


class CIOT643x : public CDevice {
  //++
  // SBC6120 FP6120 front panel IOTs ...
  //--

public:
  // Constructor and destructor...
  CIOT643x (word_t nIOT)
    : CDevice("FP6120", "FP6120", "Front Panel IOTs", INOUT, nIOT) {};
  virtual ~CIOT643x() {};
private:
  // Disallow copy and assignments!
  CIOT643x (const CIOT643x &) = delete;
  CIOT643x& operator= (CIOT643x const &) = delete;

  // Device methods from CDevice ...
public:
  virtual bool DevIOT (word_t wIR, word_t &AC, word_t &wPC) override {return true;}
  virtual void ShowDevice (ostringstream &ofs) const override
    {ofs << "FP6120 EMULATION NOT IMPLEMENTED!";}
};
