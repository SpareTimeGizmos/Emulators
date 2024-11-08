//++
// PSG.hpp -> AY-3-891x Programmable Sound Generator emulation
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
//   This class emulates the basic functions of an General Instruments
// AY-3-8910, 8912, 8913, et al programmable sound generators.
//
// REVISION HISTORY:
// 72-FEB-24  RLA   New file.
// 30-OCT-24  RLA   PAOUT and PBOUT are backwards!
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ std::string class, et al ...
#include "EMULIB.hpp"           // emulator library definitions
#include "MemoryTypes.h"        // address_t and word_t data types
#include "EventQueue.hpp"       // CEventQueue declarations
#include "Device.hpp"           // generic device definitions
using std::string;              // ...


class CPSG : public CDevice
{
  //++
  // Generic programmable sound generator emulation ...
  //--

  // PSG registers and bits ...
public:
  enum {
    //   This is the geometry for a 32Mb disk drive or CompactFlash card.  No,
    //   The 8910 et al has a four bit register address, but it actually decodes
    // all eight bits loaded into the address register.  Normally the upper four
    // bits must be zero, but GI would actually manufacture custom versions of
    // the 8910 that expected other, non-zero, nibbles on the upper four bits.
    // The idea was to allow multiple PSG chips on the same bus to be separately
    // addressed by these extra bits.
    PSGSEL          = 0x00,   // PSG select code (e.g.$10, $20, etc)
    //   Define mnemonics for the PSG registers just to make it easier to use the
    // GI documentation.BTW, note that GI numbered the registers in OCTAL!
    R0         = PSGSEL+000,  // tone generator low byte, channel A
    R1         = PSGSEL+001,  //  "    "    "   high  ", "    "
    R2         = PSGSEL+002,  //  "    "    "   low byte, channel B
    R3         = PSGSEL+003,  //  "    "    "   high  ", "    "
    R4         = PSGSEL+004,  //  "    "    "   low byte, channel C
    R5         = PSGSEL+005,  //  "    "    "   high  ", "    "
    R6         = PSGSEL+006,  // noise generator control
    R7         = PSGSEL+007,  // mixerand I / O control
    R10        = PSGSEL+010,  // amplitude control, channel A
    R11        = PSGSEL+011,  //  "     "     "        "    B
    R12        = PSGSEL+012,  //  "     "     "        "    C
    R13        = PSGSEL+013,  // envelope period, low byte
    R14        = PSGSEL+014,  //  "    "     "    high  "
    R15        = PSGSEL+015,  // envelope shape and cycle control
    R16        = PSGSEL+016,  // I/O port A data
    R17        = PSGSEL+017,  //  "   "   B   "
    MAXREG     = 16,          // total number of registers
    // Bits in the mixer control register R7 ...
    R7_PBOUT        = 0x80,   // port B output mode
    R7_PAOUT        = 0x40,   //  "   A  "  "   "
    R7_CNOISE       = 0x20,   // channel C noise generator enable
    R7_BNOISE       = 0x10,   //  "   "  B   "    "    "     "
    R7_ANOISE       = 0x08,   //  "   "  A   "    "    "     "
    R7_CTONE        = 0x04,   // channel C tone generator enable
    R7_BTONE        = 0x02,   //  "   "  B   "    "    "    "
    R7_ATONE        = 0x01,   //  "   "  A   "    "    "    "
    // Number of I/O ports used by the PSG ...
    MAXPORT         = 2,      // two - address and data
  };

  // Constructor and destructor ...
public:
  CPSG (const char *pszName, const char *pszType, const char *pszDescription, address_t nPort, address_t nPorts, CEventQueue *pEvents);
  CPSG (const char *pszName, address_t nPort, CEventQueue *pEvents)
    : CPSG(pszName, "AY-3-891x", "Programmable Sound Generator", nPort, MAXPORT, pEvents) {};
  virtual ~CPSG();
private:
  // Disallow copy and assignments!
  CPSG(const CPSG&) = delete;
  CPSG& operator= (CPSG const&) = delete;

  // CPSG device methods from CDevice ...
public:
  virtual void ClearDevice() override;
  virtual void EventCallback(intptr_t lParam) override {};
  virtual word_t DevRead (address_t nRegister) override;
  virtual void DevWrite (address_t nRegister, word_t bData) override;
  virtual void ShowDevice (ostringstream &ofs) const override;

  // Other public CPSG methods ...
public:
  // Attach a .WAV file to receive the output ...
  bool Attach (const string &sFileName) {return false;}
  void Detach() {};
  // Return TRUE if a WAV file is attached ...
  bool IsAttached() const {return false;}
  // Return the external file that we're attached to ...
  string GetFileName() const {return "";}

  // Private methods ...
protected:
  // Return TRUE if port A or B is configured for output ...
  bool IsOutputA() const {return ISSET(m_abRegisters[R7], R7_PAOUT);}
  bool IsOutputB() const {return ISSET(m_abRegisters[R7], R7_PBOUT);}

  // Private member data...
protected:
  uint8_t   m_abRegisters[MAXREG];  // the entire register file
  uint8_t   m_bAddress;             // register address
};
