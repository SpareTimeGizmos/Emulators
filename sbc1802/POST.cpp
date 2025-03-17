//++
// POST.cpp -> DIP Switches and 7 Segment LED Display emulation
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
// used on the SBC1802.  The 7 segment LED is directly driven by the eight bit
// output port, one segment per bit with the MSB driving the decimal.  The
// SBC1802 firmware uses this to display hexadecimal values.  The display is
// written by an output to port 4 and is not affected by the group select.
// 
//    The DIP switches are a simple 8 bit input port that's read from port 4.
// The SBC1802 firmware uses this for startup/configuration options, but you
// could do anything you wanted with it including implementing a traditional
// Elf style switch register.
// 
//    The switches have one interesting addition however - there's also a
// push button.  This is wired to EF1 like the traditional Elf INPUT button, but
// it's also wired up to the CDP1877 interrupt controller.  Pressing the button
// sets an "attention request" flip flop, and this F-F is cleared either by
// reading the switch register or by a hardware reset.
//
// REVISION HISTORY:
// 17-JUN-22  RLA   New file.
// 23-JUN-22  RLA   Add INPUT/ATTENTION switch.
//--
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include <string>               // C++ std::string class, et al ...
#include <iostream>             // C++ style output for LOGS() ...
#include <sstream>              // C++ std::stringstream, et al ...
#include "EMULIB.hpp"           // emulator library definitions
#include "LogFile.hpp"          // emulator library message logging facility
#include "SBC1802.hpp"          // global declarations for this project
#include "Interrupt.hpp"        // interrupt simulation logic
#include "EventQueue.hpp"       // I/O device event queue simulation
#include "Memory.hpp"           // basic memory emulation declarations ...
#include "Device.hpp"           // basic I/O device emulation declarations ...
#include "CPU.hpp"              // CCPU base class definitions
#include "POST.hpp"             // declarations for this module


CLEDS::CLEDS (address_t nPort)
  : CDevice("POST", "LEDS", "7 Segment Display", OUTPUT, nPort)
{
  //++
  //--
  m_bPOST = 0;
}

string CLEDS::DecodePOST (uint8_t bPOST)
{
  //++
  // Decode a seven segment "bitmap" into ASCII, more or less ...
  //--
  string sPOST;
  switch (bPOST & ~SEGDP) {
    case POSTF: sPOST = "F";  break;
    case POSTE: sPOST = "E";  break;
    case POSTD: sPOST = "D";  break;
    case POSTC: sPOST = "C";  break;
    case POSTB: sPOST = "B";  break;
    case POSTA: sPOST = "A";  break;
    case POST9: sPOST = "9";  break;
    case POST8: sPOST = "8";  break;
    case POST7: sPOST = "7";  break;
    case POST6: sPOST = "6";  break;
    case POST5: sPOST = "5";  break;
    case POST4: sPOST = "4";  break;
    case POST3: sPOST = "3";  break;
    case POST2: sPOST = "2";  break;
    case POST1: sPOST = "1";  break;
    case POST0: sPOST = "0";  break;
    default:    sPOST =  "";  break;
  }
  if (ISSET(bPOST, SEGDP)) sPOST += ".";
  return sPOST;
}

void CLEDS::DevWrite (address_t nPort, word_t bData)
{
  //++
  // Update the seven segment display ...
  //--
  assert(nPort == GetBasePort());
  m_bPOST = bData;
  LOGF(DEBUG, "POST=\"%s\" (0x%02X)", DecodePOST(bData).c_str(), bData);
  if ((bData == POST3) || (bData == 0x03)) LOGF(WARNING, "AUTOBAUD NOW");
}

void CLEDS::ShowDevice (ostringstream &ofs) const
{
  //++
  // Dump the device state for the UI command "SHOW DEVICE" ...
  //--
  ofs << FormatString("DISPLAY=\"%s\" (0x%02X)", DecodePOST(m_bPOST).c_str(), m_bPOST);
}

CSwitches::CSwitches (address_t nPort)
  : CDevice("SWITCHES", "SWITCH", "Switch Register", INPUT, nPort)
{
  //++
  // Constructor ...
  //--
  m_bSwitches = 0;  m_fAttention = false;
}

void CSwitches::ClearDevice()
{
  //++
  //   Clear (i.e. a hardware reset) this device.  This doesn't affect the
  // switch register, but it will clear any attention interrupt request.
  //--
  m_fAttention = false;  RequestAttention(false);
}

word_t CSwitches::DevRead (address_t nPort)
{
  //++
  //   Read the current DIP switch settings.  Note that, as a side effect,
  // this will clear any attention interrupt request!
  //--
  assert(nPort == GetBasePort());
  m_fAttention = false;  RequestAttention(false);
  return m_bSwitches;
}

void CSwitches::RequestAttention (bool fAttention)
{
  //++
  // Set or clear the attention interrupt request ...
  //--
  m_fAttention = fAttention;  RequestInterrupt(fAttention);
}

void CSwitches::ShowDevice (ostringstream &ofs) const
{
  //++
  // Dump the device state for the UI command "SHOW DEVICE" ...
  //--
  ofs << FormatString("Switches set to 0x%02X", m_bSwitches);
  if (m_fAttention) ofs << ", ATTENTION requested";
  ofs << std::endl;
}
