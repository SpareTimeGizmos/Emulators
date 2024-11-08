//++
// CDP1851.cpp -> RCA CDP1851 programmable peripheral interface emulator
//
// DESCRIPTION:
//   This class implements a generic emulation for the RCA CDP1851 "programmable
// peripheral interface" (aka PPI).  This device has two 8 bit parallel I/O
// ports that can be programmed as either inputs, outputs or (in the case of
// port A) bidirectional.  In bit programmable mode, pins can be programmed
// individually as inputs or outputs, and and also bit programmable pins can
// generate interrupts.
// 
// WARNING
//   This implementation is at very least incomplete and is most probably also
// incorrect, at least in some respect.  It's basically good enough to pass the
// POST in the SBC1802 firmware, and that's it!
// 
//    In particular, using the READY A, READY B, STROBE A and STROBE B pins as
// generic inputs or outputs is NOT implemented.
//
// NOTES
//   The CDP1851 INPUT and OUTPUT modes for ports A and B are actually equivalent
// to STROBED_INPUT and STROBED_OUTPUT in the PPI base class.  To get a simple, no
// handshaking, input or output mode with the CDP1851, use the bit programmable
// mode instead.
// 
//    The CDP1851 has the unique property that, in the bit programmable mode,
// individual port bits that are programmed as inputs can be used to generate
// interrupts.  These interrupt bits can be individually masked, and they can
// be combined with a logical AND, OR, NAND or NOR operation.  This is a bit of
// a problem for the base PPI class, since updating the state of an input port
// requires that the CPU read from that port, which calls the InputX() method
// and loads new port data.  Since interrupts are asynchronous, that's not much
// use here.
// 
//    To get around that, this class impllements the UpdateInputX() method.
// This may be called anytime by some peripheral class derived from this one,
// and asynchronously updates the corresponding input pins.  If any of those
// pins are enabled to interrupt, then UpdateInputX() will cause an interrupt.
// Note that if the CPU later reads from that input port, the InputX() function
// will still be called.
// 
// REVISION HISTORY:
// 31-OCT-24  RLA   New file.
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
#include "CDP1851.hpp"            // declarations for this module


CCDP1851::CCDP1851 (const char *pszName, address_t nPort, CEventQueue *pEvents,
                    address_t nReadySenseA, address_t nReadySenseB,
                    address_t nIntSenseA, address_t nIntSenseB)
     : CPPI (pszName, "CDP1851", nPort, 2*REG_COUNT-1, pEvents)
{
  //++
  //--
  m_nReadySenseA = nReadySenseA;  m_nReadySenseB = nReadySenseB;
  m_nIntSenseA = nIntSenseA;  m_nIntSenseB = nIntSenseB;
  ClearDevice();
}

void CCDP1851::ClearDevice()
{
  //++
  //  Reset the device ...
  //--
  CPPI::ClearDevice();
  m_ControlState = CTL_REG_IDLE;
  m_bLastControl = m_bPortAB = m_bStatus = 0;
  m_bIntMaskA = m_bIntMaskB = 0xFF;
  m_bIntFnA = m_bIntFnB = 0;
  m_bInputA = InputA(); // move this??? To mode setting???
  m_bInputB = InputB(); // move this??? To mode setting???
  //m_bMode = CTL_RESET;
  //m_bInputA = m_bOutputA = m_bInputB = m_bOutputB = 0;
  //m_bInputC = m_bOutputC = m_bStatus = 0;
}

/*static*/ const char *CCDP1851::ControlToString (CTL_REG_STATE nState)
{
  //++
  // Convert the control state to a string for debugging ...
  //--
  switch (nState) {
    case CTL_REG_IDLE:            return "IDLE";
    case CTL_REG_BITP_MASK_NEXT:  return "BIT PROGRAMMABLE MASK NEXT";
    case CTL_REG_BITP_CTL_NEXT:   return "BIT PROGRAMMABLE CONTROL NEXT";
    case CTL_REG_INTMASK_NEXT:    return "INTERRUPT MASK NEXT";
    default: assert(false);
  }
  return ""; // just to avoide C4715 warning!
}

bool CCDP1851::IsReadyA() const
{
  //++
  //   This method computes the state of the READY signal for port A.  Using
  // the PPI base class, if port A is configured for strobed input then READY
  // is true when the input buffer is full, and for strobed output READY is
  // true when the output buffer is empty.
  // 
  //    An extra complication is that if port A is in bidirectional mode,
  // then READY A corresponds to the INPUT side of the port only.  The CDP1854
  // separates these functions, but the base PPI class doesn't.
  // 
  //    If port A is in bit programmable mode, then READY A is an arbitrary
  // input pin, and we don't currently implement it.
  //--
  if ((m_ModeA == STROBED_INPUT) || (m_ModeA == BIDIRECTIONAL))
    return m_fIBFA;
  else if (m_ModeA == STROBED_OUTPUT)
    return m_fOBEA;
  else
    return false;
 }

bool CCDP1851::IsReadyB() const
{
  //++
  //   This is basically the same as IsReadyA(), except for port B this time.
  // One tricky bit though - if port A is in bidirectional mode, then READY B
  // corresponds to the OUTPUT side of PORT A (and not port B at all!).
  //--
  if (m_ModeA == BIDIRECTIONAL)
    return m_fOBEA;
  else if (m_ModeB == STROBED_INPUT)
    return m_fIBFB;
  else if (m_ModeB == STROBED_OUTPUT)
    return m_fOBEB;
  else
    return false;
}

bool CCDP1851::InterruptMask (uint8_t bData, uint8_t bMask, uint8_t bIntFn)
{
  //++
  //   In bit programmable mode, the CDP1851 allows any input bit to generate
  // an interrupt request.  Bits may be masked, and individual bits may be
  // combined with any one of AND, OR, NAND or NOR operations.  This routine
  // will take the current port data, interrupt mask, and interrupt function
  // and compute a 1 or 0 result for whether an interrupt should be requested.
  // 
  //   Note that a 1 bit in the mask indicates that the corresponding interrupt
  // is DISABLED!  That's backwards from what you might expect.
  //--
  uint1_t bInterrupt = 0;
  bIntFn &= CTL_INT_FNMASK;
  for (uint8_t i = 0; i < 8; ++i) {
    uint1_t bDB = bData & 1;  uint1_t bMB = bMask & 1;
    bData >>= 1;  bMask >>= 1;
    if (bMB == 1) continue;
    if ((bIntFn == CTL_INT_AND) || (bIntFn == CTL_INT_NAND))
      bInterrupt &= bDB;
    else
      bInterrupt |= bDB;
  }
  if ((bIntFn == CTL_INT_NAND) || (bIntFn == CTL_INT_NOR))
    bInterrupt ^= 1;
  return (bInterrupt == 1);
}

void CCDP1851::UpdateInterrupts()
{
  //++
  //    This routine will update the current contents of the status register,
  // m_bStatus, and as a side effect, it will update the interrupt A or B
  // request depending on the state of the INT A or INT B status bits.
  //--

  //    Note that the bottom four bits of the status register are used for the
  // READY A, STROBE A, READY B and STROBE B bits, HOWEVER these bits are active
  // only if the associated port is in the bit programmable mode.  These bits
  // are currently not implemented in that mode, and are always zero.
  m_bStatus = 0;

  // Check for interrupts caused by READY A or READY B ...
  if (m_ModeA == BIDIRECTIONAL) {
    if (m_fIENA && IsReadyA()) m_bStatus |= STS_AINT | STS_INTASTB;
    if (m_fIENA && IsReadyB()) m_bStatus |= STS_AINT | STS_INTBSTB;
  } else {
    if (m_fIENA && IsReadyA()) m_bStatus |= STS_AINT;
    if (m_fIENB && IsReadyB()) m_bStatus |= STS_BINT;
  }

  // Check for bit programmable interrupts ...
  if (m_fIENA && (m_ModeA == BIT_PROGRAMMABLE) && InterruptMask(MaskInput(m_bInputA, m_bDDRA), m_bIntMaskA, m_bIntFnA)) m_bStatus |= STS_AINT;
  if (m_fIENB && (m_ModeB == BIT_PROGRAMMABLE) && InterruptMask(MaskInput(m_bInputB, m_bDDRB), m_bIntMaskB, m_bIntFnB)) m_bStatus |= STS_BINT;

  // Update the interrupt requests accordingly ...
  m_fIRQA = ISSET(m_bStatus, STS_AINT);
  m_fIRQB = ISSET(m_bStatus, STS_BINT);
  RequestInterruptA(m_fIRQA);
  RequestInterruptB(m_fIRQB);
}

uint8_t CCDP1851::ReadStatus()
{
  //++
  // Read the status register ...
  //--
  UpdateInterrupts();
  return m_bStatus;
}

void CCDP1851::ModeSet (uint8_t bData)
{
  //++
  //   This routine is called when the first byte written to the control
  // register is a mode set command from Table I in the data sheet.  Input,
  // output and bidirectional modes are handled directly, but the bit
  // programmable mode requires two more argument bytes to follow.
  // 
  //   Note that this byte contains two bits, SET A and SET B, which determines
  // which port is modified.  I assume if both bits are set then both ports
  // are changed, but is that really true?  And better yet, what happens if
  // NEITHER bit is set?  Nothing??
  //--
  switch ((bData & CTL_MODE_MASK)) {
    case CTL_MODE_INPUT:
      // Set either or both ports to simple input mode ...
      if (ISSET(bData, CTL_MODE_SET_A)) SetModeA(STROBED_INPUT);
      if (ISSET(bData, CTL_MODE_SET_B)) SetModeB(STROBED_INPUT);
      break;
    case CTL_MODE_OUTPUT:
      // Set either or both ports to simple output mode ...
      if (ISSET(bData, CTL_MODE_SET_A)) SetModeA(STROBED_OUTPUT);
      if (ISSET(bData, CTL_MODE_SET_B)) SetModeB(STROBED_OUTPUT);
      break;
    case CTL_MODE_BIDIR:
      // Set port A (only!) to bidirectional mode ...
      if (ISSET(bData, CTL_MODE_SET_A)) SetModeA(BIDIRECTIONAL);
      break;
    case CTL_MODE_BITPR:
      //   Bitprogrammable needs two more argument bytes, so just wait for
      // the next one to be written to the control register.
      m_bPortAB = bData & (CTL_MODE_SET_A | CTL_MODE_SET_B);
      m_ControlState = CTL_REG_BITP_MASK_NEXT;  break;
  }
}

void CCDP1851::SetBitProgrammable (uint8_t bPortAB, uint8_t bMask, uint8_t bControl)
{
  //++
  //   This method handles the bit programmable mode set command. This actually
  // takes three bytes - the original command byte (which selects port A or B),
  // the mask bit (a 1 bit selects an output pin, 0 selects input), and an I/O
  // control byte.  This last byte determines whether the STROBE and READY bits
  // associated with the selected port are used as generic input or output bits.
  // 
  //   That part - programming the STROBE and READY pins as general purpose
  // inputs and outputs - is not currently implemented.  At the moment we just
  // ignore the control byte.  Unfortunately this doesn't fit well with the
  // IBF/OBE bits implemented by the generic PPI class, and I'll deal with that
  // later!
  //--
  if (ISSET(bPortAB, CTL_MODE_SET_A)) {
    SetModeA(BIT_PROGRAMMABLE);  SetDDRA(bMask);
  }
  if (ISSET(bPortAB, CTL_MODE_SET_B)) {
    SetModeB(BIT_PROGRAMMABLE);  SetDDRB(bMask);
  }
  // bControl byte is not currently implemented!
}

void CCDP1851::InterruptControl (uint8_t bData)
{
  //++
  //   THis method handles the interrupt control (table III in the RCA CDP1851
  // datasheet) control register writes.  This allows individual bits in ports
  // that are bit programmable to generate interrupts.
  // 
  //   This control word has an optional second byte, the interrupt mask.  This
  // is present if bit 0x10 is set in this control byte.
  //--
  if (ISSET(bData, CTL_INT_PORTB)) {
    m_bIntFnB = bData & CTL_INT_FNMASK;
  } else {
    m_bIntFnA = bData & CTL_INT_FNMASK;
  }
  if (ISSET(bData, CTL_INT_NEWMASK)) m_ControlState = CTL_REG_INTMASK_NEXT;
  UpdateInterrupts();
}

void CCDP1851::InterruptEnable (uint8_t bData)
{
  //++
  //   This method processes the interrupt enable (table IV in the datasheet)
  // write to the control port.  It either enables or disables interrupts for
  // either port A or port B.
  // 
  //   Note that the SetIENx() method always calls UpdateInterrupts() so we
  // don't have to worry about side effects of changing the interrupt enable!
  //--
  if (ISSET(bData, CTL_INT_PORTB))
    SetIENB(ISSET(bData, 0x80));
  else
    SetIENA(ISSET(bData, 0x80));
}

void CCDP1851::WriteControl (uint8_t bData)
{
  //++
  // Write the control register ...
  //--
  switch (m_ControlState) {

    // Possible control bytes when we're not expecting anything special ...
    case CTL_REG_IDLE:
           if ((bData & 0x03) == 0x03) ModeSet(bData);
      else if ((bData & 0x87) == 0x05) InterruptControl(bData);
      else if ((bData & 0x07) == 0x01) InterruptEnable(bData);
      else
        LOGF(WARNING, "invalid CDP1851 control byte 0x%02X", bData);
      break;

    // Interrupt mask argument for interrupt control command ..
    case CTL_REG_INTMASK_NEXT:
      if (ISSET(m_bLastControl, CTL_INT_PORTB))
        m_bIntMaskB = bData;
      else
        m_bIntMaskA = bData;
      m_ControlState = CTL_REG_IDLE;
      break;

    // Arguments for the bit programmable mode set ...
    case CTL_REG_BITP_MASK_NEXT:
      //   Save the mask for bit programmable mode in m_bLastControl and wait
      // for the strobe/ready I/O control byte next  ...
      m_ControlState = CTL_REG_BITP_CTL_NEXT;  break;
    case CTL_REG_BITP_CTL_NEXT:
      // We have the whole command now - set the bit programmable mode ...
      SetBitProgrammable(m_bPortAB, m_bLastControl, bData);
      m_ControlState = CTL_REG_IDLE;
      break;

    default: assert(false);
  }
  m_bLastControl = bData;
}

//++
//    Normally for input ports the PPI class will call the InputX() method
// when the CPU tries to read from the associated port.  That's OK as far as
// it goes, but the CDP1851 has the unique ability to configure individual
// input bits as interrupt sources.  This works only if the port is in bit
// programmable mode, but it's a problem because if the port data is only
// updated when the port is read, how can an asynchronous interrupt occur?
// 
//    To fix that, this class implements UpdateInputX() methods for ports
// A and B.  These methods can be called any time by a peripheral derived
// from this class, and will asynchronously update the current state of
// the input pins.  If any of those pins has been programmed to generate
// interrupts, then that'll happen.  Note that this works ONLY if the port
// is configured for bit programmable mode!
// 
//    And lastly, be aware that if an interrupt occurs and the CPU later
// reads from that same port, the InputX() method will still be called as
// usual.  It's up to the derived peripheral implementation to ensure that
// InputX() and UpdateINputX() return consistent results.
// 
//    And don't confuse this with StrobedInputX().  That method is used 
// only when the port is configured for strobed input mode.
//--
#define UPDATEINPUT(port)                                              \
void CCDP1851::UpdateInput##port (uint8_t bData)                       \
{                                                                      \
  if (m_Mode##port != BIT_PROGRAMMABLE) return;                        \
  m_bInput##port = MaskInput(bData, m_bDDR##port);                     \
  UpdateInterrupts();                                                  \
}

// UpdateInput routines for ports A and B ...
UPDATEINPUT(A)
UPDATEINPUT(B)

void CCDP1851::DevWrite (address_t nPort, uint8_t bData)
{
  //++
  //   Handle writing to the CDP1851.  Just figure out which register is to
  // be updated and let somebody else handle it.  Note that in the SBC1802
  // the address bits are shifted left one bit, so N=2 selects register 1
  // (control), N=4 selects register 2 (port A), and N=6 selects register
  // 3 (port B).  It's a bit of a kludge to include this here, since other
  // systems might not be the same, but I'm lazy today.
  //--
  assert(nPort >= GetBasePort());
  nPort = ((nPort-GetBasePort()) >> 1) + 1;
  switch (nPort) {
    case PORTA:   WriteA(bData);        break;
    case PORTB:   WriteB(bData);        break;
    case CONTROL: WriteControl(bData);  break;
    default:      assert(false);
  }
}

uint8_t CCDP1851::DevRead (address_t nPort)
{
  //++
  //   Handle reading from the CDP1851.  Just figure out which port is to be
  // accessed and then let somebody else handle it.  See the comments in
  // DevWrite() about why the port addressing is a little weird!!
  //--
  assert(nPort >= GetBasePort());
  nPort = ((nPort-GetBasePort()) >> 1) + 1;
  switch (nPort) {
    case PORTA:   return ReadA();
    case PORTB:   return ReadB();
    case STATUS:  return ReadStatus();
    default:      assert(false);
  }
  return 0xFF;  // just to avoid C4715 errors!
}

uint1_t CCDP1851::GetSense (address_t nSense, uint1_t bDefault)
{
  //++
  //   The CDP1851 emulation supports up to four sense lines (aka EF flags on
  // the 1802) - READY A, READY B, IRQ A and IRQ B.  The ready sense lines are
  // a bit tricky with the PPI base class, since for output ports READY is 
  // true when the output buffer is empty, and for input ports READY is true
  // when the input buffer is full.
  // 
  //   WARNING - it's entirely possible (and even likely) that the same EF
  // flag is used for more than one function.  For example m_nIntSenseA and
  // m_nIntSenseB may very well be the same!  In that event, the result is
  // a logical OR of both A and B IRQs.  The same applies to READY A/B.
  //--
  uint1_t bFlag = bDefault;
  if ((nSense == m_nReadySenseA) && IsReadyA()) bFlag |= 1;
  if ((nSense == m_nReadySenseB) && IsReadyB()) bFlag |= 1;
  UpdateInterrupts();
  if ((nSense == m_nIntSenseA) && ISSET(m_bStatus, STS_AINT)) bFlag |= 1;
  if ((nSense == m_nIntSenseB) && ISSET(m_bStatus, STS_BINT)) bFlag |= 1;
  return bFlag;
}

void CCDP1851::ShowDevice (ostringstream &ofs) const
{
  //++
  //   This routine will dump the state of the internal PPI registers.
  // It's used for debugging by the user interface SHOW DEVICE command.
  //--
  ofs << FormatString("CDP1851 control state=%s, last control=0x%02X, port A/B=0x%02X, status=0x%02X\n",
    ControlToString(m_ControlState), m_bLastControl, m_bPortAB, m_bStatus);
  ofs << std::endl;
  ofs << FormatString("PORT A %s mode, IBUF=0x%02X, OBUF=0x%02X, DDR=0x%02X, RDY=%d\n",
    ModeToString(m_ModeA), m_bInputA, m_bOutputA, m_bDDRA, IsReadyA());
  ofs << FormatString("       IntMask=0x%02X, IntFn=0x%02X, ReadySense=%d, IntSense=%d\n",
    m_bIntMaskA, m_bIntFnA, m_nReadySenseA, m_nIntSenseA);
  ofs << FormatString("       IBF=%d, OBE=%d, IEN=%d, IRQ=%d\n",
    m_fIBFA, m_fOBEA, m_fIENA, m_fIRQA);
  ofs << std::endl;
  ofs << FormatString("PORT B %s mode, IBUF=0x%02X, OBUF=0x%02X, DDR=0x%02X, RDY=%d\n",
    ModeToString(m_ModeB), m_bInputB, m_bOutputB, m_bDDRB, IsReadyB());
  ofs << FormatString("       IntMask=0x%02X, IntFn=0x%02X, ReadySense=%d, IntSense=%d\n",
    m_bIntMaskB, m_bIntFnB, m_nReadySenseB, m_nIntSenseB);
  ofs << FormatString("       IBF=%d, OBE=%d, IEN=%d, IRQ=%d\n",
    m_fIBFB, m_fOBEB, m_fIENB, m_fIRQB);
}
