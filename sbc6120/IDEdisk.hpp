//++
// IDEdisk.hpp -> SBC6120 IDE disk interface class
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
//   This class emulates the SBC6120 specific IDE interface (which actually uses
// an Intel 8255 programmable peripheral interface).
//
// REVISION HISTORY:
// 22-AUG-22  RLA   New file.
//--
#pragma once
#include <stdint.h>
#include <string>               // C++ std::string class, et al ...
#include "MemoryTypes.h"        // address_t and word_t data types
#include "EventQueue.hpp"       // CEventQueue declarations
#include "Device.hpp"           // generic device definitions
#include "CPU.hpp"              // CPU definitions
#include "IDE.hpp"              // IDE disk emulation
using std::string;              // ...

class CIDEdisk : public CIDE
{
  //++
  // IDE disk interface ...
  //--

  // Magic bits and constants ...
public:
  enum {
    NDRIVES         = 1,      // number of drives supported (MASTER only)
  };
private:
    // 8255 control bytes that can be written to the control port ...
  enum {
    IDE_INPUT       = 0222,   // set ports A and B as inputs, C as output
    IDE_OUTPUT      = 0200,   // set ports A and B (and C too) as outputs
    IDE_SET_DIOR    = 0007,   // assert DIOR L (PC.3) in the IDE interface
    IDE_CLR_DIOR    = 0006,   // clear	 "   "    "    "  "   "    "    "
    IDE_SET_DIOW    = 0011,   // assert DIOW L (PC.4) in the IDE interface
    IDE_CLR_DIOW    = 0010,   // clear	 "   "    "    "  "   "    "    "
    IDE_SET_RESET   = 0013,   // assert DRESET L (PC.5) in the IDE interface
    IDE_CLR_RESET   = 0012,   // clear	 "     "    "    "  "   "    "    "
    IDE_CS1FX       = 0100,   // select IDE CS1FX register space
    IDE_CS3FX       = 0200,   //   "     "  CS3FX   "   "    "
  };

public:
  // Constructor and destructor...
  CIDEdisk (word_t nIOT, CEventQueue *pEvents);
  virtual ~CIDEdisk() {};
private:
  // Disallow copy and assignments!
  CIDEdisk (const CIDEdisk &) = delete;
  CIDEdisk& operator= (const CIDEdisk &) = delete;

  // Public IDE disk properties
public:
  // ???
  bool IsDASP() const {return false;}

  // CDisk device methods from CDevice ...
public:
  virtual void ClearDevice() override;
  virtual bool DevIOT (word_t wIR, word_t &wAC, word_t &wPC) override;
  virtual void ShowDevice (ostringstream &ofs) const override;

  // Local methods ...
private:
  address_t GetRegister() const;
  bool WriteControl (uint8_t bControl);
  void WriteRegister();
  void ReadRegister();

  // Private member data...
protected:
  bool    m_fInputMode;       // true if ports A and B are inputs
//bool    m_fOutputMode;      //  "    "   "   "  "  "  "  outputs
//bool    m_fCS1FX;           // true if CS1FX register set is selected
//bool    m_fCS3FX;           //   "  "  CS3FX   "   "   "   "   "   "
  bool    m_fDIOR;            // true if DIOR is asserted
  bool    m_fDIOW;            //  "   "  DIOW  "   "   "
  bool    m_fRESET;           //  "   "  RESET "   "   "
  uint8_t m_bPortA;           // current contents of port A
  uint8_t m_bPortB;           //   "  "    "   "   "   "  B
  uint8_t m_bPortC;           //   "  "    "   "   "   "  C
};
