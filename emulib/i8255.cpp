//++
// i8255.cpp -> Intel 8255 programmable peripheral interface emulator
//
// DESCRIPTION:
//   This class implements a generic emulation for the Intel 8255 "programmable
// peripheral interface" (aka PPI).  This device has three 8 bit parallel I/O
// ports that can be programmed as either inputs, outputs or (in the case of
// port A) bidirectional.  Bits can also be programmed individually as inputs
// or outputs, and port C bits can be used for handshaking in strobed input
// and/or output modes.
// 
// REVISION HISTORY:
//  7-JUL-22  RLA   New file.
// 11-JUL-22  RLA   Don't forget to update the IRQA/B bits in UpdateInterrupts()!
//  7-JUL-23  RLA   Change to use CPPI generic PPI base class
//  7-NOV-24  RLA   Actually make it work (using SBCT11) with the CPPI base class!
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
#include "PPI.hpp"              // generic parallel interface
#include "i8255.hpp"            // declarations for this module


C8255::C8255 (const char *pszName, address_t nPort, CEventQueue *pEvents, address_t cPorts)
     : CPPI (pszName, "i8255", nPort, cPorts, pEvents)
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
  m_bMode = CTL_RESET;  m_fIEIA = m_fIEOA = false;
  m_bInputA = m_bOutputA = m_bInputB = m_bOutputB = 0;
  m_bInputC = m_bOutputC = m_bStatus = 0;
}

void C8255::UpdateInterrupts()
{
  //++
  //   This method will update m_bStatus based on the various OBE/IBF flags,
  // interrupt enable bits, and what not.  This status byte corresponds to the
  // alternate functions of the 8255 port C bits when ports A and/or B are in
  // something other than simple input or output mode.
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
  // 
  //   NOTE THAT THIS IMPLEMENTATION COMPLETEY REPLACES THE ONE IN THE CPPI
  // CLASS.  DON'T CALL THE CPPI::UpdateInterrupts() METHOD HERE!
  //--

  // Just copy the current interrupt enable bits directly to the status ...
  m_bStatus = 0;
  if (m_fIEIA) SETBIT(m_bStatus, PC_IEIA);
  if (m_fIEOA) SETBIT(m_bStatus, PC_IEOA);
  if (m_fIENB) SETBIT(m_bStatus, PC_IENB);

  //   Figure out the state of the IRQA bit for port A.  This varies, depending
  // on mode 1 vs mode 2.  Note that port A ONLY affects the IRQA status bit,
  // regardless of the mode!
  if (IsBiDirA()) {
    if (m_fIBFA) SETBIT(m_bStatus, PC_IBFA);
    if (m_fOBEA) SETBIT(m_bStatus, PC_OBEA);
    if (m_fIEIA && m_fIBFA) SETBIT(m_bStatus, PC_IRQA);
    if (m_fIEOA && m_fOBEA) SETBIT(m_bStatus, PC_IRQA);
  } else if (IsStrobedA()) {
    if (IsInputA()  && m_fIBFA) SETBIT(m_bStatus, PC_IBFA);
    if (IsOutputA() && m_fOBEA) SETBIT(m_bStatus, PC_OBEA);
    if (IsInputA()  && m_fIEIA && m_fIBFA) SETBIT(m_bStatus, PC_IRQA);
    if (IsOutputA() && m_fIEOA && m_fOBEA) SETBIT(m_bStatus, PC_IRQA);
  }

  //   Now update the port B IRQ.  Remember that port B can still be strobed 
  // even if port A is in mode 2!
  if (IsStrobedB()) {
    if (IsInputB()  && m_fIBFB) SETBIT(m_bStatus, PC_IBFB);
    if (IsOutputB() && m_fOBEB) SETBIT(m_bStatus, PC_OBEB);
    if (IsInputB()  && m_fIENB && m_fIBFB) SETBIT(m_bStatus, PC_IRQB);
    if (IsOutputB() && m_fIENB && m_fOBEB) SETBIT(m_bStatus, PC_IRQB);
  }

  // And lastly update the actual interrupt requests ...
  RequestInterruptA(ISSET(m_bStatus, PC_IRQA));
  RequestInterruptB(ISSET(m_bStatus, PC_IRQB));
}

void C8255::UpdateFlags (bool fSet, uint8_t bMask)
{
  //++
  //   This routine is called when port C is changed by the bit set/reset
  // command to update the flags (IEN, IBE, OBF, etc) associated with the
  // special functions of port C.
  // 
  //   I don't think (although I'll admit that I'm not at all certain) that
  // a simple byte wide write to port C can change these special function
  // bits.  For the moment at least, I assume they can only be modified by
  // the bit set/reset command.
  //--
  switch (bMask) {
    // These are all pretty straight forward ...
    case PC_OBEA: m_fOBEA = fSet;   break;
    case PC_IEOA: m_fIEOA = fSet;   break;
    case PC_IBFA: m_fIBFA = fSet;   break;
    case PC_IEIA: m_fIEIA = fSet;   break;
    case PC_IENB: m_fIENB = fSet;   break;

    //   For port B, the IBF and OBE bits in port C are the same, so the
    // flag that's affected depends on the current mode of port B!
    case PC_IBFB: /* PC_OBEB */
      if (IsInputB()) m_fIBFB = fSet;  else m_fOBEB = fSet;
      break;

    //   I'm not sure if it was possible to set or clear an IRQ on the 8255
    // by altering the corresponding port C bit, but for now we don't
    // allow it.  If you change this be careful, as UpdateInterrupts() will
    // erase all the bits currently in m_bStatus!
    case PC_IRQA:
    case PC_IRQB:
      break;

    // Anything else is bad!!
    default: assert(false);
  }

  //   Update the status flags and interrupt request now that the flags
  // have changed.
  UpdateInterrupts();
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

void C8255::NewModeA()
{
  //++
  // Set the mode for port A according to m_bMode ...
  //--
  if (IsBiDirA())
    CPPI::SetModeA(BIDIRECTIONAL);
  else if (IsStrobedA())
    CPPI::SetModeA((IsInputA() ? STROBED_INPUT : STROBED_OUTPUT));
  else
    CPPI::SetModeA((IsInputA() ? SIMPLE_INPUT : SIMPLE_OUTPUT));
}

void C8255::NewModeB()
{
  //++
  // Set the mode for port B according to m_bMode ...
  //--
  if (IsStrobedB())
    CPPI::SetModeB((IsInputB() ? STROBED_INPUT : STROBED_OUTPUT));
  else
    CPPI::SetModeB((IsInputB() ? SIMPLE_INPUT : SIMPLE_OUTPUT));
}

void C8255::NewModeC()
{
  //++
  //   Set the mode for port B according to m_bMode ...  Port C is a bit wierd
  // because half of it can be programmed for simple input and the other half
  // programmed for simple output, or both halves can be programmed the same.
  // In this case if both parts of port C are programmed for input then we set
  // the entire port to simple input mode, but if either half is output then
  // we set the whole thing to simple output and let the DDR mask take care of
  // resolving which is which.
  //--
  if (IsInputCU() && IsInputCL())
    CPPI::SetModeC(SIMPLE_INPUT);
  else {
    CPPI::SetModeC(SIMPLE_OUTPUT);
    if (IsInputCU()) CPPI::SetDDRC(0x0F);
    if (IsInputCL()) CPPI::SetDDRC(0xF0);
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
  NewModeA();  NewModeB();  NewModeC();
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
  if (bMask != 0) {
    UpdateInterrupts();
    bPortC = (m_bStatus & bMask) | (bPortC & ~bMask);
  }
  return bPortC;
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
  //   Note that OutputC() will not be called if BOTH halves of port C, upper
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
  bool fSet = ISSET(bControl, BSR_SET);

  //   The bStatusMask is a mask of the bits in port C that are currently
  // assiged to alternate functions for ports A and/or B.  We use this to
  // decide whether we need to update the flags or just simply write new
  // data to the port C output.
  uint8_t bStatusMask = GetStatusMask();

  if (ISSET(bStatusMask, bBitMask)) {
    //   Change an alternate function bit in the status.  The actual port C
    // output register does not change.
    UpdateFlags(fSet, bBitMask);
  } else {
    //   Change an actual output bit in port C ...  Note that if the bit
    // being changed is an input bit, then we don't call OutputC() ...
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

  bool fIEAI =    (IsBiDirA() && ISSET(m_bStatus, PC_IEIA))
               || (IsStrobedA() && IsInputA()  && ISSET(m_bStatus, PC_IEIA));
  bool fIEAO =    (IsBiDirA() && ISSET(m_bStatus, PC_IEOA))
               || (IsStrobedA() && IsOutputA() && ISSET(m_bStatus, PC_IEOA));
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
