//++                                                                    
// SLU.cpp - SBC6120 terminal interface implementation
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
//   The SBC6120 has a more or less KL8/E compatible serial interface for the
// console terminal.  The only difference between the SBC6120 and the KL8/E are
// that the KCF, TFL, KIE and TSK instructions are omitted from the SBC6120.
// Console interrupts are permanently enabled, as they were in the original
// PDP-8.
//
//      IOT       Function
//      --------  ----------------------------------------------------------
//      KSF 6031  Skip if the console receive flag is set
//      KCC 6032  Clear the receive flag and AC
//      KRS 6034  OR AC with the receive buffer but don’t clear the flag
//      KRB 6036  Read the receive buffer into AC and clear the flag
//      TSF 6041  Skip if the console transmit flag is set
//      TCF 6042  Clear transmit flag, but not the AC
//      TPC 6044  Load AC into transmit buffer, but don't clear flag
//      TLS 6046  Load AC into transmit buffer and clear the flag
//
//   In the SBC6120 the console IOTs are decoded directly by PLDs, and no 6121
// chip is used.  In later versions of the SBC6120 two additional IOTs were
// added which allowed the console SLU device codes to be changed from 03/04 to
// 36/37.  This allowed a daugter card with a different console terminal to be
// used, such as Jim Kearney's IOB6120.
//
//      IOT           Function
//      --------      ----------------------------------------------------------
//      PRISLU 6412   Use device codes 03/04 for the onboard SLU
//      SECSLU 6413    "     "     "   36/37  "   "   "  "    "
//
// RESTRICTIONS
//   Note that this code assumes that the IOT device code for the keyboard and
// the printer are consectutive, with the printer device being the keyboard
// device plus 1.  For example, it's device 03 for the console ekyboard and 04
// for the console printer, or 36 for the keyboard and 37 for the printer.  I'm
// not sure if the real KL8/E required IOT codes to be assigned in this way,
// but in practice all PDP-8 software assumes they are, even when multiple SLU
// devices are in use.
//                                                                      
// REVISION HISTORY:                                                    
// dd-mmm-yy    who     description
// 21-Aug-22    RLA     Steal from WinEight.
//--                                                                    
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include <string>               // C++ std::string class, et al ...
#include <iostream>             // C++ style output for LOGS() ...
#include <sstream>              // C++ std::stringstream, et al ...
#include "EMULIB.hpp"           // emulator library definitions
#include "LogFile.hpp"          // emulator library message logging facility
#include "ConsoleWindow.hpp"    // console window functions
#include "CommandParser.hpp"    // needed for type KEYWORD
#include "MemoryTypes.h"        // address_t and word_t data types
#include "Interrupt.hpp"        // CInterrupt definitions
#include "EventQueue.hpp"       // CEventQueue definitions
#include "HD6120opcodes.hpp"    // PDP8/6120 opcode mnemonics
#include "CPU.hpp"              // CPU definitions
#include "Device.hpp"           // generic device definitions
#include "UART.hpp"             // generic UART base class
#include "HD6120.hpp"           // HD6120 CPU specific definitions
#include "SLU.hpp"              // declarations for this module

// Define this symbol to generate the full KL8/E instruction set ...
//#define KL8E


CSLU::CSLU (const char *pszName, word_t nIOT, CEventQueue *pEvents, CVirtualConsole *pConsole, CCPU *pCPU)
  : CUART(pszName, "SLU", "Serial Interface", nIOT, 2, pEvents, pConsole, pCPU)
{
  //++
  //  Note - two device IOTs assigned here, one for the keyboard and one for
  // the printer.
  //--
  m_fIEN = true;  m_fKBDflag = m_fTPRflag = false;  m_bKBDbuffer = 0;
}

void CSLU::ClearDevice()
{
  //++
  //   CAF or RESET clears both the keyboard and printer flags (yes, it CLEARS
  // the printer flag, so the code has to force the first character out!).  It
  // sets the interrupt enable, clears the keyboard buffer and clears any
  // interrupt request (because both flags are cleared now) ...
  //--
  m_fKBDflag = m_fTPRflag = false;  m_fIEN = true;  m_bKBDbuffer = 0;
  RequestInterrupt(false);  CUART::ClearDevice();
}

void CSLU::UpdateRBR (uint8_t bData)
{
  //++
  //   This method is called whenever the console detects a new keypress, and
  // It will load the received byte into the receiver buffer, set the receiver
  // done flag, and request an interrupt if that's enabled.
  //--
  m_bKBDbuffer = (bData & 0177) | 0200;
  m_fKBDflag = true;  RequestInterrupt(IsIRQ());
}

void CSLU::TransmitterDone()
{
  //++
  //   Here for a transmitter done event - this means that enough simulated
  // time has elapsed for the last character to have been transmitted.  Set
  // the transmitter done flag and request an interrupt if enabled.
  //--
  m_fTPRflag = true;  RequestInterrupt(IsIRQ());
}

bool CSLU::KeyboardIOT (word_t wIOT, word_t &wAC, word_t &wPC)
{
  //++
  // Handle all keyboard 603x IOTs ...
  //--
  switch (wIOT & 7) {

    case (OP_KSF & 7):
      // Skip if keyboard flag is set 
      if (m_fKBDflag) wPC = INC12(wPC);
      break;

    case (OP_KCC & 7):
      // Clear keyboard flag and AC, set reader run 
      wAC = 0;  m_fKBDflag = false;
      break;

#ifdef KL8E
    case (OP_KCF & 7):
      // Clear keyboard flag, do not set reader run 
      m_fKBDflag = false;
      break;
#endif

    case (OP_KRS & 7):
      // Read keyboard buffer 
      wAC |= m_bKBDbuffer;
      break;

    case (OP_KRB & 7):
      // Combination of KRS and KCC 
      wAC = m_bKBDbuffer;  m_fKBDflag = false;
      break;

#ifdef KL8E
    case (OP_KIE & 7):
      // Load interrupt enable from AC bit 11.
      m_fIEN = ISSET(wAC, 1);
      break;
#endif

    default:
      // Anything else is unimplemented! 
      return false;
  }

  //   Setting or clearing a flag, or setting or clearing the interrupt enable,
  // will cause the interrupt status to change.  Rather than try to figure out
  // what happened, just update the interrupt status regardless.  It's easy!
  RequestInterrupt(IsIRQ());
  return true;
}

bool CSLU::PrinterIOT (word_t wIOT, word_t &wAC, word_t &wPC)
{
  //++
  // Handle all printer 604x IOTs ...
  //--
  switch (wIOT & 7) {

#ifdef KL8E
    case (OP_TFL & 7):
      // Set printer flag 
      m_fTPRflag = true;
      break;
#endif

    case (OP_TSF & 7):
      // Skip if printer flag is set 
      if (m_fTPRflag) wPC = INC12(wPC);
      break;

    case (OP_TCF & 7):
      // Clear printer flag 
      m_fTPRflag = false;
      break;

    case (OP_TLS & 7):
      // Combination of TPC and TCF 
      m_fTPRflag = false;
      // Fall into TPC...

    case (OP_TPC & 7):
      // Load printer buffer ...
      StartTransmitter(LOBYTE(wAC));
      break;

#ifdef KL8E
    case (OP_TSK & 7):
      // Skip if interrupt request.  This affects both devices!
      if (IsIRQ()) wPC = INC12(wPC);
      break;
#endif

    // Anything else is unimplemented! 
    default:
      return false;
  }

  //   Setting or clearing a flag, or setting or clearing the interrupt enable,
  // will cause the interrupt status to change.  Rather than try to figure out
  // what happened, just update the interrupt status regardless.  It's easy!
  RequestInterrupt(IsIRQ());
  return true;
}

bool CSLU::DevIOT(word_t wIOT, word_t &wAC, word_t &wPC)
{
  //++
  //   This method is called for any 603x or 604x IOT.  We need to figure out
  // which device it is and then call the appropriate handler.  In theory we
  // should never be called for any other devices, but if we ever are then
  // it's unimplemented.
  //--
  word_t wDevice = (wIOT & 0770) >> 3;
  if (wDevice == GetBasePort())
    return KeyboardIOT(wIOT, wAC, wPC);
  else if (wDevice == (GetBasePort()+1))
    return PrinterIOT(wIOT, wAC, wPC);
  else
    return false;
}

void CSLU::ShowDevice (ostringstream &ofs) const
{
  //++
  // Show the SLU status for debugging ...
  //--
  ofs << FormatString("Keyboard flag=%d, printer flag=%d, keyboard buffer=%03o",
    m_fKBDflag, m_fTPRflag, m_bKBDbuffer);
  if (isprint(m_bKBDbuffer & 0177))
    ofs << FormatString(" (\"%c\")", (m_bKBDbuffer & 0177));
  ofs << FormatString(" IEN=%d", m_fIEN);
  ofs << std::endl;
  CUART::ShowDevice(ofs);
}
