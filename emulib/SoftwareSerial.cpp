//++
// SoftwareSerial.hpp -> "Bit Banged" serial console emulation
//
//   COPYRIGHT (C) 2015-2020 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
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
//   Many microprocessors have a single bit software serial interface of some
// kind or another.  The 1802 has its Q output and EF inputs; the 8085 has SID
// and SOD, and the SC/MP and the 2650 have their SENSE and FLAG.  This class
// implements a simple software serial terminal connected to the console window.
// This is a typical console terminal for any number of microprocessors.
//
//   This class inherits from CDevice, and indeed it needs some of the regular
// device functions (like belonging to a CPU or scheduling events), but it's
// not a regular device.  The Read() and Write() methods do nothing and are
// never called, and all I/O is thru the SerialInput() and SerialOutput()
// methods instead.
//
//   SerialOutput() is a method in this class; the CPU should call it whenever
// the state of the software serial output is changed.  Normally this output
// is controlled via some special instructions (e.g. SEQ/REQ, RIM/SIM, etc)
// and not ny normal I/O instructions.  Likewise, SerialInput() is a CPU method
// that this object will call whenever the state of our serial output changes.
// Once again, the CPU's dedicated serial input is usually tested by some
// special instructions (e.g. B3/BN3, etc) and isn't a normal I/O port.
//
//   BTW, note that SerialInput() and SerialOutput() are named from the CPU
// point of view - they're an input to the CPU or an output from it.  For this
// module the SerialOutput() method drives our receiver, and our transmitter
// drives the SerialInput() calls.
//
// SIGNAL LEVELS
//   With bit banged serial there's always the question of data polarity.  For
// real EIA signalling levels, the idle state is a negative voltage; the start
// bit positive; the data bits are negative for a 1 and positive for a 0, and
// the stop bits are negative again.  Typical EIA level shifters, like the
// MAX232, invert so that negative EIA levels correspond to a TTL high or 1,
// and positive EIA levels are a TTL low or zero.
//
//   This means that the idle state is 1; the start bit is 0; the data bits are
// true (i.e. 1 is 1 and 0 is 0), and the stop bits are 1s.  Those are the
// conventions used in this module!  HOWEVER, some microprocessor boards that
// we might want to emulate don't use proper EIA level shifters and those don't
// invert the signals.  In those cases the firmware for the microprocessor will
// flip the sense of the bits (that's trivial to do with software serial).
// This module accomodates those situations by inverting the state of the data
// in the SerialInput() and SerialOutput() methods; the rest of the internal
// logic and the state machine is unchanged.
//
// TO DO:
//   Make number of stop bits, parity and data bits selectable
//   SHOULD THIS INHERIT CUART??  Probably can, if we adjust the baud rate...
//
// REVISION HISTORY:
// 21-JAN-20  RLA   New file.
// 27-JAN-20  RLA   Add break support.
// 22-JUN-22  RLA   Add nSense and nFlag parameters to GetSense() and SetFlag()
// 20-JUL-22  RLA   On transmitting, DON'T DROP ZERO BYTES!
// 27-AUG-22  RLA   If the user types ^E to interrupt the simulation and then
//                   tries to continue, serial input will be hung.  That's
//                   because PollKeyboard() doesn't schedule another polling
//                   event in that case.  Fix it!
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include "EMULIB.hpp"           // emulator library definitions
#include "LogFile.hpp"          // emulator library message logging facility
#include "VirtualConsole.hpp"   // console window functions
#include "CPU.hpp"              // CPU definitions
#include "Device.hpp"           // generic device definitions
#include "SoftwareSerial.hpp"   // declarations for this module


CSoftwareSerial::CSoftwareSerial (CEventQueue *pEvents, CVirtualConsole *pConsole, CCPU *pCPU)
  : CDevice("SERIAL", "SERIAL", "Software Serial Emulation", INOUT, 0, 0, pEvents)
{
  //++
  //--
  assert(pConsole != NULL);
  m_pConsole = pConsole;  m_pCPU = pCPU;
  m_nTXstate = m_nRXstate = STATE_IDLE;
  m_bTXbuffer = m_bRXbuffer = 0;
  m_bTXbit = m_bRXlast = MARK;  m_llLastBitTime = 0;
  //m_llBitTime = 1000000000ULL/300ULL;  // TBA TODO NYI TEMPORARY!!!
  m_llBitTime = 1000000000ULL/1200ULL;  // TBA TODO NYI TEMPORARY!!!
  m_llPollingInterval = 1000000000ULL/10000ULL;  // TBA TODO NYI TEMPORARY!!!
  m_fTXinvert = m_fRXinvert = true;
}

void CSoftwareSerial::ClearDevice()
{
  //++
  //   Clear the receiver and transmitter state and buffers, and reset the
  // serial output level ...
  //--
  m_nTXstate = m_nRXstate = STATE_IDLE;
  m_bTXbuffer = m_bRXbuffer = 0;  m_llLastBitTime = 0;
  //   You might think we should initialize m_bRXlast here, but don't believe
  // it!  The thing is, the ClearDevice() routines get called after ClearCPU()
  // and ClearCPU() should initialize the state of the CPU's flag outputs.
  // That will call our SetFlag() method, which will initialize m_bRXlast.
  // If we initialize it here then we'll just undo that!
  //m_bRXlast = m_fRXinvert ? 0 : 1;
  TransmitBit(MARK);
  ScheduleEvent(EVENT_TXPOLL, m_llPollingInterval);
}

void CSoftwareSerial::TransmitBit (uint8_t bData)
{
  //++
  //   This method will send one bit of serial data to the CPU, handling the
  // correct signal polarity (inverted or not inverted) as we go ...
  //--
  m_bTXbit = MASK1(m_fTXinvert ? ~bData : bData);
}

void CSoftwareSerial::StartTransmitter (uint8_t bData)
{
  //++
  //   Start up the transmitter state machine and send the byte specified,
  // rembering to transmit the start bit first.  If the transmitter is
  // currently busy, then just return without doing anything.
  //--
  if (IsTXbusy()) return;
  m_bTXbuffer = bData;  m_nTXstate = STATE_START;
  TransmitBit(SPACE);
  ScheduleEvent(EVENT_TXBIT, m_llBitTime);
}

void CSoftwareSerial::TransmitNext()
{
  //++
  //   This method is the transmitter state machine.  It's called once every
  // bit time by the Event() method and it will shift out and transmit the
  // next bit.  When all 8 data abits have been transmitted, it will transmit
  // one or two stop bits and then return to the idle state.
  //--
  ++m_nTXstate;
  if ((m_nTXstate >= STATE_DATA)  &&  (m_nTXstate < STATE_STOP)) {
    // Transmit the next data bit ...
    TransmitBit(MASK1(m_bTXbuffer));  m_bTXbuffer >>= 1;
  } else if ((m_nTXstate >= STATE_STOP)  &&  (m_nTXstate < (STATE_STOP+STOP_BITS))) {
    // Send stop bits ...
    TransmitBit(MARK);
  } else {
    // We're done!  Set the state back to idle ...
    m_nTXstate = STATE_IDLE;
    return;
  }
  ScheduleEvent(EVENT_TXBIT, m_llBitTime);
}

void CSoftwareSerial::PollKeyboard()
{
  //++
  //   This method is called by the TXPOLL event - it will poll the keyboard
  // to see if the operator has typed anything and, if he has, then it will
  // start up the transmitter state machine to send that charactere to the CPU.
  // If nothing was typed, then we just schedule another poll sometime in the
  // near future ...
  //
  //   Note that if the transmitter state machine is currently busy and we call
  // StartTransmitter() again, then nothing happens.  The transmitter state
  // machine continues with the existing character in progress, and this new
  // character is dropped.  
  //--
  uint8_t bData = 0;
  if (!IsTXbusy() && !IsRXbusy()) {
    int32_t nRet = m_pConsole->RawRead(&bData, 1);
    if (m_pConsole->IsConsoleBreak())
      m_pCPU->Break();
    else if (nRet > 0)
      StartTransmitter(MASK8(bData));
  }
  ScheduleEvent(EVENT_TXPOLL, m_llPollingInterval);
}

void CSoftwareSerial::StartReceiver()
{
  //++
  //   This method is called by SerialOutput() when it detects a 1 to zero
  // (i.e. a MARK to SPACE) transition.  This marks the leading edge of the
  // start bit, and it's time for us to crank up the receiver state machine.
  // This will initialize the reeiver buffer and then start scheduling events
  // to sample the serial data.  Note that the delay for the very first sample
  // is 1.5x the bit time - that's because we want to skip over the start bit
  // (one bit time) and then sample all the remaining bits in the middle of
  // their window (that's the 1/2).
  //--
  LOGF(TRACE, "Serial start receiver");
  if (m_nRXstate != STATE_IDLE) return;
  m_nRXstate = STATE_DATA;  m_bRXbuffer = 0;
  uint64_t llDelay = m_llBitTime + (m_llBitTime/2ULL);
  ScheduleEvent(EVENT_RXBIT, llDelay);
}

void CSoftwareSerial::ReceiveNext()
{
  //++
  //   This method is the receiver state machine.  It will sample the current
  // serial output state and shift bits into the receiver buffer.  When we have
  // received 8 data bits, it will verify that the next bit is a stop bit.
  // If we don't find a stop bit, then we have a framing error and this byte
  // is invalid.  Either way, we shut down the receiver and wait for the next
  // start bit leading edge ...
  //--
  LOGF(TRACE, "Serial RXpoll, state=%d, data=%d, rxbuf=0x%02X, time=%lld", m_nRXstate, m_bRXlast, m_bRXbuffer, m_pEvents->CurrentTime());
  if ((m_nRXstate >= STATE_DATA)  &&  (m_nRXstate < (STATE_DATA+DATA_BITS))) {
    // Another data bit - shift it into the receiver buffer ...
    ++m_nRXstate;  m_bRXbuffer >>= 1;
    if (m_bRXlast != 0)  m_bRXbuffer |= 0x80;
    ScheduleEvent(EVENT_RXBIT, m_llBitTime);
  } else {
    // This is the stop bit ...
    ReceiverDone(m_bRXlast != MARK);
  }
}

void CSoftwareSerial::ReceiverDone (bool fError)
{
  //++
  //   This routine is called when we've finished receiving all eight data
  // bits.  If a framing error (that's the only error we can detect!) occurred
  // then fError should be true and the received data will be discarded.  If
  // all is well, then the receiver buffer is sent to the console window for
  // display.  Either way we stop scheduling receiver events and reset the
  // receiver state to idle.
  //--
  if (fError) {
    LOGF(WARNING, "software serial framing error detected");
  }
  //   This screws up the XMOMDEM protocol and I don't think it's necessary
  // since the CConsoleWindow::RawWrite() already does this ...
  //uint8_t ch = m_bRXbuffer & 0x7F;
  uint8_t ch = m_bRXbuffer;
  LOGF(TRACE, "Serial RXdone, buffer=0x%02X, char=0x%02X", m_bRXbuffer, ch);
  m_pConsole->RawWrite((const char *) &ch, 1);
  m_nRXstate = STATE_IDLE;
}

void CSoftwareSerial::EventCallback (intptr_t lParam)
{
  //++
  // Handle event callbacks for this device ...
  //--
  switch (lParam) {
    case EVENT_TXPOLL:    PollKeyboard();     break;
    case EVENT_TXBIT:     TransmitNext();     break;
    case EVENT_RXBIT:     ReceiveNext();      break;
    default: assert(false);
  }
}

void CSoftwareSerial::SetFlag (address_t nFlag, uint1_t bData)
{
  //++
  //   This method is called by the CPU whenever the state of the dedicated
  // serial output is changed.  We watch for a 1 to 0 transition to signal
  // the start of the next character.
  //
  //   Note that m_bRXlast always contains the current state of the CPU's
  // serial output.  It's used to detect transitions, and by the receiver
  // state machine to sample the data.
  //--
  if (m_fRXinvert) bData = MASK1(~bData);
  LOGF(TRACE, "Serial RX, state=%d, time=%lld", bData, m_pEvents->CurrentTime());
  m_llLastBitTime = m_pEvents->CurrentTime();
  if ((m_nRXstate==STATE_IDLE)  &&  (m_bRXlast==MARK)  &&  (bData==SPACE)) {
    StartReceiver();
  }
  m_bRXlast = MASK1(bData);
}

void CSoftwareSerial::ShowDevice (ostringstream &ofs) const
{
  //++
  // Dump the device state for the UI command "EXAMINE DISPLAY" ...
  //--
  string sInvert;
  if (IsTXinverted())
    sInvert = IsRXinverted() ? "BOTH" : "TX";
  else
    sInvert = IsRXinverted() ? "RX" : "NONE";
  ofs << FormatString("Invert=%s, Baud=%ld, Bit time=%lldus, Polling interval=%lldus\n",
    sInvert.c_str(), GetBaud(), NSTOUS(m_llBitTime), NSTOUS(GetPollDelay()));
  ofs << FormatString("RXstate=%d, RXlast=%d, RXbuffer=0x%02X\n", m_nRXstate, m_bRXlast, m_bRXbuffer);
  ofs << FormatString("TXstate=%d, TXlast=%d, TXbuffer=0x%02X\n", m_nTXstate, m_bTXbit, m_bTXbuffer);
}
