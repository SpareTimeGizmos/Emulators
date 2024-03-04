//++
// i8255.cpp -> Intel 8255 programmable peripheral interface emulator
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
//   This class implements a generic emulation for the Intel 8255 "programmable
// peripheral interface" (aka PPI).  This device has three 8 bit parallel I/O
// ports that can be programmed as either inputs, outputs or (in the case of
// port A) bidirectional.  Bits can also be programmed individually as inputs
// or outputs, and port C bits can be used for handshaking in strobed input
// and/or output modes.
// 
//   By themselves these devices don't really do much and their emulation is
// usually highly dependent on the way the PPI is actually wired up in the
// target system.  For that reason this class isn't really intended to be used
// alone, but rather as a base class for some system specific implementation.
//    
//   This class attempts to simulate both strobed input and output, however
// it required some cooperation from any derived class.  Here's a handy
// summary of the sequence that occurs for each direction.
// 
// STROBED MODE HANDSHAKING FOR OUTPUT
//   1) The simulation writes to port X.  This class sets the OBFx bit
//        and clears any pending interrupt, if enabled.
//   2) This class calls StrobedOutputX(bData) method.  This should be
//        overridden by a derived class to actually do something.
//   3) When ever it is ready the derived class, either now or later,
//        calls OutputDoneX() method in this class.
//   4) This class clears the OBFx bit and will interrupt if enabled.
//
// STROBED MODE HANDSHAKING FOR INPUT
//  1) A derived class must first call the StrobedInputX(bData) method
//        in this class and pass to it the input byte.
//  2) This class sets the IBF bit and will interrupt if enabled
//  3) Sometime later, the simulation reads from port X.
//  4) This class will clear the IBF bit and call the InputReadyX()
//        method.  This method should be overriden by a derived class.
//
// NOTES:
//   Ports A and B both work exactly the same way, with the substitution of
// the appropriate "X" in the routine names.
//
//   Port A in mode 2 works exactly the same way, except that it's capable
// of both input and output at the same time.  In this case the input side
// and the output side operate independently.  Port B can do either input or
// output, but not both.
//
// REVISION HISTORY:
//  7-JUL-22  RLA   New file.
// 11-JUL-22  RLA   Don't forget to update the IRQA/B bits in UpdateInterrupts()!
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include "EMULIB.hpp"           // emulator library definitions
#include "LogFile.hpp"          // emulator library message logging facility
#include "ConsoleWindow.hpp"    // console window functions
#include "CommandParser.hpp"    // needed for type KEYWORD
#include "MemoryTypes.h"        // address_t and word_t data types
#include "CPU.hpp"              // CPU definitions
#include "Device.hpp"           // generic device definitions
#include "i8255-original.hpp"   // declarations for this module


C8255::C8255 (const char *pszName, address_t nPort, address_t cPorts, CEventQueue *pEvents)
     : CDevice (pszName, "i8255", "Parallel Interface", INOUT, nPort, cPorts, pEvents)
{
  //++
  //--
  ClearDevice();
}

void C8255::ClearDevice()
{
  //++
  //   The datasheet says that the RESET input "clears the control (mode)
  // register and sets all ports to inputs".  That doesn't see right though,
  // since an input port requires a one bit in the mode register!  I choose
  // to follow the intent and set everything to inputs, even though that
  // doesn't zero the mode register.
  //--
  CDevice::ClearDevice();
  m_bMode = CTL_RESET;
  m_bInputA = m_bOutputA = m_bInputB = m_bOutputB = 0;
  m_bInputC = m_bOutputC = m_bStatus = 0;
}

void C8255::UpdateInterrupts()
{
  //++
  // Update the current interrupt request status ...
  // 
  //   Note that port A has two separate interrupt combinations - IEAI 
  // (interrupt enable for input) and IBFA (input buffer full), plus IEAO
  // (interrupt enable for output) and OBEA (output buffer empty).  Both
  // of these can be simulataneously active in mode 2 and they are connected
  // to separate interrupt requests.  If port A is in mode 1 then the bits
  // are the same, however only one can be active at any given time and they
  // both share the A interrupt.
  // 
  //   HOWEVER, for port B, IBFB and OBEB are the same bit since it can only
  // support one data transfer direction at a time.  There's no need to worry
  // about separate input and output cases there.  Port B has only one
  // associated interrupt and, if port A is in mode 2, then port B has no
  // interrupts at all.
  //--
  uint8_t bOldStatus = m_bStatus;

  //   Figure out the state of the IRQA bit for port A.  This varies, depending
  // on mode 1 vs mode 2.  Note that port A ONLY affects the IRQA status bit,
  // regardless of the mode!
  if (IsBiDirA()) {
    CLRBIT(m_bStatus, PC_IRQA);
    if (ISSET(m_bStatus, PC_IEAO) && ISSET(m_bStatus, PC_OBEA)) SETBIT(m_bStatus, PC_IRQA);
    if (ISSET(m_bStatus, PC_IEAI) && ISSET(m_bStatus, PC_IBFA)) SETBIT(m_bStatus, PC_IRQA);
  } else {
    if (IsStrobedA()) {
      CLRBIT(m_bStatus, PC_IRQA);
      if (IsInputA()) {
        if (ISSET(m_bStatus, PC_IEAI) && ISSET(m_bStatus, PC_IBFA)) SETBIT(m_bStatus, PC_IRQA);
      } else {
        if (ISSET(m_bStatus, PC_IEAO) && ISSET(m_bStatus, PC_OBEA)) SETBIT(m_bStatus, PC_IRQA);
      }
    }
  }

  //   Now update the port B IRQ.  Remember that port B can still be strobed 
  // even if port A is in mode 2!
  if (IsStrobedB()) {
    CLRBIT(m_bStatus, PC_IRQB);
    if (ISSET(m_bStatus, PC_IENB) && ISSET(m_bStatus, PC_IBFB)) SETBIT(m_bStatus, PC_IRQB);
  }

  // And lastly update the actual interrupt requests.
  if (ISSET((m_bStatus^bOldStatus), PC_IRQA))  RequestInterruptA(ISSET(m_bStatus, PC_IRQA));
  if (ISSET((m_bStatus^bOldStatus), PC_IRQB))  RequestInterruptB(ISSET(m_bStatus, PC_IRQB));
}

uint8_t C8255::GetStatusMask() const
{
  //++
  //   This routine returns a mask of the bits in port C which are currently
  // assigned to alternate functions, like IBF/OBE, interrupt enable, request,
  // etc.  This depends on the mode of both ports A and B...
  //--
  if (IsBiDirA())
    return PC_A_MODE_2;
  else {
    uint8_t bMask = 0;
    if (IsStrobedA() && IsInputA())  bMask |= PC_A_MODE_1_INPUT;
    if (IsStrobedA() && IsOutputA()) bMask |= PC_A_MODE_1_OUTPUT;
    if (IsStrobedB()) bMask |= PC_B_MODE_1;
    return bMask;
  }
}

void C8255::NewMode (uint8_t bNewMode)
{
  //++
  //   This method is called whenever the simulation loads a new byte into the
  // control register.  In addition to updating the mode register, this resets
  // all outputs to zero and clears all bits in the status register.
  //--
  assert(ISSET(bNewMode, CTL_MODE_SET));
  //if (bNewMode == m_bMode) return;
  m_bMode = bNewMode;
  m_bOutputA = m_bOutputB = m_bOutputC = 0;
  m_bStatus = 0;
  UpdateInterrupts();
}

void C8255::StrobedInputA (uint8_t bData)
{
  //++
  //   This method is called by some subclass derived from this one when the
  // simulation has new data ready to be strobed into input port A.  It will
  // latch the device data, set the input buffer full (IBF) bit in port C,
  // and request an interrupt (if enabled).  
  // 
  //   Note that if port A is in the simple (non-strobed) mode then calling
  // this routine does nothing.  In that situation the derived class should
  // override the InputA() method instead.
  // 
  //   Also, if port A is configured as an output right now, then this routine
  // still loads the input side latches for port A, but otherwise does nothing.
  // UNLESS, that is, port A is in mode 2.  Mode 2 is bidirectional I/O with
  // simultaneous input and output.  In that case it's not clear what effect the
  // CTL_A_INPUT bit actually has - presumably none ...
  //--
  if (IsSimpleA()) return;
  m_bInputA = bData;
  if (IsInputA() && !IsBiDirA()) return;
  SETBIT(m_bStatus, PC_IBFA);
  UpdateInterrupts();
}

uint8_t C8255::ReadA()
{
  //++
  //   This method is called by Read() when the simulated software tries to read
  // from the PPI port A.  In simple input mode there are no latches on input and
  // there is no handshaking, so we just call the InputA() method to get the
  // current state of the inputs.  The version of InputA() in this class always
  // just returns 0xFF, but a derived class can override this method to provide
  // whatever data it wants.  
  // 
  //   In strobed mode then this is the second half of the input handshaking.
  // It returns whatever was loaded into the input latches by StrobedInputA(),
  // clears the IBFA bit, and removes any interrupt request (if enabled).
  // 
  //   If port A is configured as an output, in either mode 0 or 1, then reading
  // the port just returns whatever was last written to the same port.  UNLESS,
  // that is, we're in mode 2 which is fully bidirectional.  In mode 2 the input
  // and output sides of port A are independent and this functions as above.
  //--
  if (IsOutputA() && !IsBiDirA()) {
    return m_bOutputA;
  } else if (IsSimpleA()) {
    m_bInputA = InputA();
  } else {
    CLRBIT(m_bStatus, PC_IBFA);
    UpdateInterrupts();
    InputReadyA();
  }
  return m_bInputA;
}

void C8255::StrobedInputB (uint8_t bData)
{
  //++
  // Same as StrobedInputA(), but for port B!
  //--
  if (IsSimpleB()) return;
  m_bInputB = bData;
  if (IsInputB()) return;
  SETBIT(m_bStatus, PC_IBFB);
  UpdateInterrupts();
}

uint8_t C8255::ReadB()
{
  //++
  // Same as ReadA(), but for port B ...
  //--
  if (IsOutputB()) {
    return m_bOutputB;
  } else if (IsSimpleB()) {
    m_bInputB = InputB();
  } else {
    CLRBIT(m_bStatus, PC_IBFB);
    UpdateInterrupts();
    InputReadyB();
  }
  return m_bInputB;
}

void C8255::WriteA (uint8_t bData)
{
  //++
  //   This routine handles writing to port A.  In simple output mode, this
  // updates the port A output latches (yes, even simple mode is latched!)
  // with the last data written to port A and then calls the OutputA() method.
  // The version of OutputA() this class does nothing, however a subclass
  // derived from this one can override that function and do anything it likes
  // with the new port value.
  // 
  //   In strobed mode, then calls the StrobedOutputA() method, clears the
  // output buffer empty bit (OBE), and removes any interrupt request (if one
  // was enabled).  Note that the Intel documentation calls the OBE bit OBF
  // ("output buffer FULL") but inverts the sense so that it's active low. This
  // is exactly the same as our output buffer empty bit.
  // 
  //   Also note that the version of StrobedOutputA() in this class definition
  // does nothing, however a subclass derived from this one can override that
  // function and do anything it likes with the data.  When it's ready to proceed
  // the subclass needs to call OutputDoneA().
  // 
  //   If Port A is currently configured as an input then this still loads the
  // output latches for port A, but otherwise does nothing.  UNLESS, that is,
  // port A is in mode 2.  Mode 2 is bidirectional and it's not clear what effect
  // (if any) the CTL_A_INPUT bit should have.  In mode 2 the input and output
  // sides of port A function independently and it behaves as above.
  //--
  m_bOutputA = bData;
  if (IsInputA() && !IsBiDirA()) return;
  if (IsSimpleA()) {
    OutputA(bData);
  } else {
    StrobedOutputA(bData);
    CLRBIT(m_bStatus, PC_OBEA);
    UpdateInterrupts();
  }
}

void C8255::OutputDoneA()
{
  //++
  //   This method is called by some subclass derived from this one when the
  // simulation has finished reading the output data from port A.  This will
  // clear set the output buffer empty (OBE) bit in port C and request a new
  // interrupt if they are enabled.
  // 
  //   Note that what we call the OBE bit Intel calls OBF (output buffer full)
  // however Intel's version is active low.  They inverted the active signal
  // state and I simply inverted the name, but they are otherwise the same.
  // 
  //   Also note that if port A is configured as an input then calling this
  // method does nothing.  UNLESS, port A is configured as a bidirectional
  // port in mode 2.  In that case the input and output sides of port A are
  // completely independent and the CTL_A_INPUT bit is ignored.
  //--
  if (IsSimpleA()) return;
  if (IsInputA() && !IsBiDirA()) return;
  SETBIT(m_bStatus, PC_OBEA);
  UpdateInterrupts();
}

void C8255::WriteB (uint8_t bData)
{
  //++
  // Same as WriteA(), except for port B ...
  //--
  m_bOutputB = bData;
  if (IsInputB()) return;
  if (IsSimpleB()) {
    OutputB(bData);
  } else {
    StrobedOutputB(bData);
    CLRBIT(m_bStatus, PC_OBEB);
    UpdateInterrupts();
  }
}

void C8255::OutputDoneB()
{
  //++
  // Same as OutputDoneA(), but for port B ...
  //--
  if (IsSimpleB()) return;
  if (IsInputB()) return;
  SETBIT(m_bStatus, PC_OBEB);
  UpdateInterrupts();
}

uint8_t C8255::ReadC()
{
  //++
  //   This method will read from port C, which can be a bit tricky because
  // of the handshaking functions port C has when ports A or B are in modes 1
  // or 2.  Port C has no input latches, so we call the InputC() method to get
  // the current state of whatever bits are NOT being used for port A or B
  // handshaking.  We then combine those with the appropriate status bits
  // as required for port A and B to get the result we return.
  // 
  //   The InputC() function defined in this class always returns 0xFF and as
  // such isn't very interesting, but a subclass derived from this one can
  // override the InputC() method and return whatever you like.
  // 
  //   Note that the m_bInputC member contains the last value returned when
  // we call InputC().  Because of the aforementioned status bits, that may
  // not match exactly what the simulation sees when reading port C.
  //--

  //   Always poll the port, regardless and leave the result in m_bInputC.
  // If either or both halves of port C are configured as outputs, ignore
  // that half of the input and replace it with the corresponding bits from
  // the outputs...
  m_bInputC = InputC();
  uint8_t bPortC = m_bInputC;
  if (!IsInputCL()) bPortC = (bPortC & 0xF0) | (m_bOutputC & 0x0F);
  if (!IsInputCU()) bPortC = (bPortC & 0x0F) | (m_bOutputC & 0xF0);

  // Combine the status bits and the inputs and we're done ...
  uint8_t bMask = GetStatusMask();
  return (m_bStatus & bMask) | (bPortC & ~bMask);
}

void C8255::WriteC (uint8_t bData)
{
  //++
  //   And this method will write to port C, which is once again a bit tricky
  // because of all the handshaking functions associated with ports A and B.
  // In this case though the rules are simpler - you can only alter port C bits
  // that are configured as outputs.  In particular, you cannot use this method
  // to change of the handshaking bits, including the interrupt enable bits.
  // To change those the 8255 requres that you use the bit set/reset function
  // instead.
  // 
  //   With that caveat, this works pretty much like WriteA() or WriteB().
  // The last data written to the port is always remembered in m_bOutputC, and
  // we call the OutputC() method to signal the change.  The OutputC() in this
  // class does nothing, but a subclass can override that with its own method
  // that can do anything you like.
  // 
  //   Note that OutputC() will not be called if BOTH halves of port C,  upper
  // and lower, are both configured as inputs.  However if either half is an
  // output then we call OutputC() with the entire byte written, regardless.
  // It's up to any subclass to know which bits really matter and ignore the
  // ones that don't!
  //--
  m_bOutputC = bData;
  if (IsInputCU() && IsInputCL()) return;
  OutputC(bData);
}

void C8255::BitSetReset (uint8_t bControl)
{
  //++
  //   The bit set/reset function allows you to set or reset ANY bit in port C,
  // including the ones with special functions.  The datasheet isn't explicit,
  // but AFAIK it's possible set or clear even the IBF/OBF or interrupt request
  // bits.  It's certainly possible to change the interrupt enable bits!
  // 
  //   Of course it's also possible to change regular output bits in port C
  // this way too, and doing this will call OuptutC() just as in the WriteC()
  // method.  Since we keep separate member variables for the current port C
  // outputs vs the current status bits, we have to be a little careful about
  // applying the changes to the correct place.
  //--
  uint8_t bBitMask = 1 << ((bControl & BSR_SELECT) >> 1);
  uint8_t bStatusMask = GetStatusMask();
  bool fSet = ISSET(bControl, BSR_SET);

  if (ISSET(bStatusMask, bBitMask)) {
    //   Change an alternate function bit in the status.  The actual port C
    // output register does not change.
    m_bStatus = fSet ? (m_bStatus | bBitMask) : (m_bStatus & ~bBitMask);
    UpdateInterrupts();
  } else {
    // Change an actual output bit in port C ...
    m_bOutputC = fSet ? (m_bOutputC | bBitMask) : (m_bOutputC & ~bBitMask);
    if (IsInputCU() && ((bBitMask & 0xF0) != 0)) return;
    if (IsInputCL() && ((bBitMask & 0x0F) != 0)) return;
    OutputC(m_bOutputC);
  }
}

uint8_t C8255::DevRead (address_t nPort)
{
  //++
  //   Handle reading from the 8255.  Just figure out which port is to be
  // accessed and then let somebody else handle it.
  // 
  //   Note that the datasheet for the NMOS 8255 specifically says that reading
  // the control/mode register is "invalid" and it's not clear what actually
  // happens if you try.  HOWEVER, the 82C55 CMOS version is equally specific
  // that you CAN read the control register.  Take you pick - we emulate the
  // latter...
  //--
  assert(nPort >= GetBasePort());
  switch (nPort-GetBasePort()) {
    case PORTA:   return ReadA();
    case PORTB:   return ReadB();
    case PORTC:   return ReadC();
    case CONTROL: return m_bMode|CTL_MODE_SET;
    default:      assert(false);
  }
  return 0xFF;  // just to avoid C4715 errors!
}

void C8255::DevWrite (address_t nPort, uint8_t bData)
{
  //++
  //   Handle writing to the 8255.  Just figure out which register is to
  // be updated and let somebody else handle it.  Note that writes to the
  // control register take two different forms - if the MSB is set then a
  // new mode byte is being loaded, but if the MSB is zero then it's a bit
  // set or reset command for port C.
  //--
  assert(nPort >= GetBasePort());
  switch (nPort-GetBasePort()) {
    case PORTA:   WriteA(bData);    break;
    case PORTB:   WriteB(bData);    break;
    case PORTC:   WriteC(bData);    break;
    case CONTROL:
      if (ISSET(bData, CTL_MODE_SET))  NewMode(bData);  else BitSetReset(bData);
      break;
    default:      assert(false);
  }
}

void C8255::ShowDevice (ostringstream &ofs) const
{
  //++
  //   This routine will dump the state of the internal PPI registers.
  // It's used for debugging by the user interface SHOW DEVICE command.
  //--

  bool fIEAI =    (IsBiDirA() && ISSET(m_bStatus, PC_IEAI))
               || (IsStrobedA() && IsInputA()  && ISSET(m_bStatus, PC_IEAI));
  bool fIEAO =    (IsBiDirA() && ISSET(m_bStatus, PC_IEAO))
               || (IsStrobedA() && IsOutputA() && ISSET(m_bStatus, PC_IEAO));
  bool fIENB = IsStrobedB() && ISSET(m_bStatus, PC_IENB);

  ofs << FormatString("PPI MODE=0x%02X, STATUS=0x%02X, IEAI=%d, IEAO=%d, IRQA=%d, IENB=%d, IRQB=%d\n",
    m_bMode, m_bStatus, fIEAI, fIEAO, ISSET(m_bStatus, PC_IRQA), fIENB, ISSET(m_bStatus, PC_IRQB));
  ofs << FormatString("Port A - mode %d,  %sPUT, InputA=0x%02x, OutputA=0x%02x\n",
    (m_bMode & CTL_A_MODE) >> 5, (IsInputA() ? "IN" : "OUT"), m_bInputA, m_bOutputA);
  ofs << FormatString("Port B - mode %d,  %sPUT, InputB=0x%02x, OutputB=0x%02x\n",
    (m_bMode & CTL_B_MODE) >> 2, (IsInputB() ? "IN" : "OUT"), m_bInputB, m_bOutputB);
  ofs << FormatString("Port CU %sPUT, CL %sPUT, InputC=0x%02X, OutputC=0x%02x\n",
    (IsInputCU() ? "IN" : "OUT"), (IsInputCL() ? "IN" : "OUT"), m_bInputC, m_bOutputC);
}
