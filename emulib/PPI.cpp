//++
// PPI.cpp -> Generic programmable parallel interface emulation
//
// DESCRIPTION:
//   This class implements a generic emulation for a number of programmable
// parallel interface chips, such as the classic Intel 8255, the Intel 8155/6
// (a simplified 8255, but with timer and RAM added), the RCA CDP1851 (maybe
// even smarter than the 8255!), the National Semiconductor INS8156 (an SC/MP
// family chip) or the NSC810 (partner to the NSC800 CPU).
// 
//   All of these chips have at least two 8 bit parallel ports that (in most
// cases) can be programmed as input, output, or bidirectional.  Some devices
// allow individual port bits to be independently programmed as an input or
// output.  I/O may be also strobed, with output buffer full/input buffer empty
// flags, an data transfer request, and (usually) an interrupt enable and
// interrupt request.
// 
//   By themselves these devices don't really do much and their emulation is
// usually highly dependent on the way the PPI is actually wired up in the
// target system.  For that reason this class isn't really intended to be used
// alone, but rather as a base class for some system specific implementation.
// This class attempts to simulate both strobed input and output, however it
// requires some cooperation from any derived class.  Here's a handy summary
// of the sequence that occurs for each direction.
// 
// STROBED MODE HANDSHAKING FOR OUTPUT
//   1) The simulation writes to port X.  This class clears the OBEx bit
//        and clears any pending interrupt, if enabled.
//   2) This class calls StrobedOutputX(bData) method.  This should be
//        overridden by a derived class to actually do something.
//   3) When ever it is ready the derived class, either now or later,
//        calls OutputDoneX() method in this class.
//   4) This class sets the OBEx bit and will interrupt if enabled.
// 
// NON-STROBED OUTPUT
//   1) When the simulation writes to port X, this class calls the OutputX()
//        method, and that's it.  A derived class can override this method to
//        intercept the data output and handle it as necessary.
//
// STROBED MODE HANDSHAKING FOR INPUT
//   1) A derived class must first call the StrobedInputX(bData) method
//        in this class and pass to it the input byte.
//   2) This class sets the IBF bit and will interrupt if enabled
//   3) Sometime later, the simulation reads from port X.
//   4) This class will clear the IBF bit and call the InputReadyX()
//        method.  This method should be overriden by a derived class.
//
// NON-STROBED INPUT
//   1) When the simulation reads port X, this class calls the InputX() method
//        to poll the current port value.  A derived class can override this
//        method to return whatever port value is necessary.
//
//   Ports A and B are normally configured for either input or output but not
// both, however bidirectional operation is possible with some devices.  In
// that case the input and output sides of the port operate independently.
//
//   Port A in mode 2 works exactly the same way, except that it's capable
// of both input and output at the same time.  In this case the input side
// and the output side operate independently.  Port B can do either input or
// output, but not both.
//
// NOTES ON VARIOUS PPI CHIPS
// INTEL 8255
//  The Intel 8255 PPI is our "reference standard" and is the basis for the PPI
// class.  The other parallel interface devices supported - RCA CDP1851, National
// NSC810, National INS8154, and the Intel 8155/6 and 8755 - are all simplified
// versions of the 8255, in one way or another.  Note that some of these other
// chips also provide RAM, timers, or both, but this class doesn't concern itself
// with those parts, only the parallel interface.
// 
// RCA CDP1851
//   The CDP1851 "programmable I/O interface" is probably the closest to the 8255.
// It has two 8 bit ports, A and B, either of which can be programmed as input or
// output.  Like the 8255, port A can also be programmed as a bidirectional port.
// Unlike the 8255 though, each port A or B pin can individually be programmed
// as an input or output.  The CDP1851 has no individual bit set or reset function
// for individual output bits.  However, the CDP1851 can apply, via AND, NAND, OR,
// or NOR, a mask to either port A or B inputs and interrupt if the result is one.
// 
//   The CDP1851 datasheet doesn't describe a "port C", however it does have the
// standard six handshaking pins - RDY, STB, and IRQ for both ports A and B.  And
// in non-strobed mode, these handshake pins can be used as general purpose I/Os.
// As GPIOs, each pin can be programmed as an input or output.
// 
//   The CDP1851 has no timers.
// 
// INTEL 8155/6
//   The Intel 8155 "RAM with I/O PORTS and TIMER" has two independent 8 bit ports
// A and B, as well as a six bit port C.  Ports A and B can be programmed, as a
// whole, for either input or output, and port C can be programmed for any one of
// input, output, or as handshaking pins for ports A and/or B.  Ports A and B are
// both capable of strobed operation with BF, RDY and IRQ outputs using pins from
// port C.  The bidrectional mode of the 8255 is absent, as is the bit set/reset
// function for port C.
// 
//   The 8155 has a single 14 bit timer, which can be programmed to generate
// either a square wave or pulse output, and either continuously or as a one shot.
// The timer does not share any pins with ports A, B or C.
// 
//   The 8156 is identical to the 8155 except for the polarity of the CS input.
// 
// NSC810
//   The NSC810 "RAM-I/O-TIMER" has two independent 8 bit ports, A and B.  Port A
// alone is capable of strobed mode I/O, either input.  The NSC810 does not have
// a true bidirectional mode, however when in output mode port A does have tri-
// state output buffers.  Each bit in each port is individually programmable as
// an input or output and, like the CDP1854, there is a bit set/reset function
// for each port.
// 
//   Port C contains six bits only and can be programmed as a simple I/O port, or
// as handshake signals (STB, BF and IRQ) for port A, or as timer 1 control
// signals (T1OUT, T1IN and TG).  
// 
//   The NSC810 has not one but two timers, one of which (T0) has independent
// I/O pins and the other (T1) depends on alternate functions for port C pins.
// 
// INS8154
//   The INS8154 "RAM-I/O", a companion to the INS8160 SC/MP, is the simplest of
// the group.  It has two 8 bit ports, A and B.  Each pin in each port can be
// individually programmed as an input or output.  Port A only is capable of
// strobed input or output, using two bits from port B (yes, B not C) as BF and
// RDY signals.  There is a separate, independent IRQ pin in this mode. Individual
// bit set/reset operations are possible on either ports A or B.
// 
//   There is no port C, and there is no timer.
// 
// REVISION HISTORY:
//  7-JUL-22  RLA   New file.
// 11-JUL-22  RLA   Don't forget to update the IRQA/B bits in UpdateInterrupts()!
//  2-NOV-24  RLA   Reset the InputX ports to 0xFF in ClearDevice() ...
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
#include "EventQueue.hpp"       // CEventQueue declarations
#include "Interrupt.hpp"        // CInterrupt definitions
#include "CPU.hpp"              // CPU definitions
#include "Device.hpp"           // generic device definitions
#include "PPI.hpp"              // declarations for this module


CPPI::CPPI (const char *pszName, const char *pszType, address_t nPort, address_t cPorts, CEventQueue *pEvents)
     : CDevice (pszName, pszType, "Parallel Interface", INOUT, nPort, cPorts, pEvents)
{
  //++
  // The constructor just initializes the PPI to the defaults..
  //--
  ClearDevice();
}

void CPPI::ClearDevice()
{
  //++
  //   ClearDevice() will reset the PPI to its default configuration.  This
  // means simple input mode for all ports and interrupts disabled.
  //--
  CDevice::ClearDevice();
  m_bInputA = 0xFF;  m_bOutputA = m_bDDRA = 0;
  m_bInputB = 0xFF;  m_bOutputB = m_bDDRB = 0;
  m_bInputC = 0xFF;  m_bOutputC = m_bDDRC = 0;
  m_fIBFA = m_fIBFB = false;
  m_fOBEA = m_fOBEB = true;
  m_fIENA = m_fIENB = false;
  m_fIRQA = m_fIRQB = false;
  m_ModeA = m_ModeB = m_ModeC = SIMPLE_INPUT;
  m_bDDRA = m_bDDRB = m_bDDRC = 0x00;
  m_bMaskC = 0xFF;
  RequestInterruptA(false);
  RequestInterruptB(false);
}

void CPPI::UpdateInterrupts()
{
  //++
  //  This routine will update the interrupt request for ports A and B depending
  // on the current IEN and OBE/IBF status.  Note that interrupts are only
  // possible for strobed or bidrectional ports!
  //--
  m_fIRQA = m_fIRQB = false;
  if (((m_ModeA == STROBED_OUTPUT) || (m_ModeA == BIDIRECTIONAL)) && m_fOBEA) m_fIRQA = true;
  if (((m_ModeA == STROBED_INPUT)  || (m_ModeA == BIDIRECTIONAL)) && m_fIBFA) m_fIRQA = true;
  if (((m_ModeB == STROBED_OUTPUT) || (m_ModeB == BIDIRECTIONAL)) && m_fOBEB) m_fIRQB = true;
  if (((m_ModeB == STROBED_INPUT)  || (m_ModeB == BIDIRECTIONAL)) && m_fIBFB) m_fIRQB = true;
  RequestInterruptA(m_fIENA && m_fIRQA);
  RequestInterruptB(m_fIENB && m_fIRQB);
 }

/*static*/ const char *CPPI::ModeToString(PORT_MODE nMode)
{
  //++
  // Convert a port mode to a string ...
  //--
  switch (nMode) {
    case SIMPLE_INPUT:      return "SIMPLE INPUT";
    case SIMPLE_OUTPUT:     return "SIMPLE OUTPUT";
    case STROBED_INPUT:     return "STROBED INPUT";
    case STROBED_OUTPUT:    return "STROBED OUTPUT";
    case BIDIRECTIONAL:     return "BIDIRECTIONAL";
    case BIT_PROGRAMMABLE:  return "BIT PROGRAMMABLE";
    default: assert(false);
  }
  return ""; // just to avoide C4715 warning!
}


//++
//   This macro generates the StrobedInput method for each of the standard
// ports.  The StrobedInputX method should be called by some subclass derived
// from this one when the simulation has new data ready to be strobed into the
// input port.  It will latch the device data, set the input buffer full flag,
// and request an interrupt if enabled.
// 
//   Note that if the port is in the simple (non-strobed) mode then calling
// this routine does nothing.  In that situation the derived class should
// override the InputX() method instead.
// 
//   Also, if the port is configured as an output right now, then this routine
// still loads the input side latches for the port, but otherwise does nothing.
// UNLESS the port is in bidirectional mode.
//--
#define STROBEDINPUT(port)                                                        \
void CPPI::StrobedInput##port (uint8_t bData)                                     \
{                                                                                 \
  if ((m_Mode##port == SIMPLE_INPUT) || (m_Mode##port == SIMPLE_OUTPUT)) return;  \
  m_bInput##port = MaskInput(bData, m_bDDR##port);                                \
  if ((m_Mode##port == STROBED_INPUT) || (m_Mode##port == BIDIRECTIONAL)) {       \
    m_fIBF##port = true;  UpdateInterrupts();                                     \
  }                                                                               \
}

// Strobed input routines for ports A and B ...
STROBEDINPUT(A)
STROBEDINPUT(B)


//++
//   This macro generates the ReadX() for each of the standard ports. This
// method is called by DevRead() when the simulated software tries to read from
// the PPI port.  In simple input mode there are no latches on input and there
// is no handshaking, so we just call the InputX() method to get the current
// state of the inputs. The version of InputX() in this class definition always
// returns 0xFF, but a derived class can override this method to provide
// whatever data it wants.  
// 
//   In strobed input mode then this completes the second half of the input
// handshaking.  It returns whatever was last loaded into the input latches
// by calling the StrobedInputX() routine, clears the IBF bit, and removes
// any interrupt request if enabled. In bidirectional mode the input and output
// sides of the port are independent and this routine does exactly the same
// thing as it would for strobed input.
// 
//   If the port A is configured as an output, either simple or strobed, then
// reading the port just returns whatever was last written to the same port.
//--
#define READPORT(port)                                              \
uint8_t CPPI::Read##port()                                          \
{                                                                   \
  uint8_t bData;                                                    \
  switch (m_Mode##port) {                                           \
    case SIMPLE_INPUT:                                              \
      m_bInput##port = MaskInput(Input##port(), m_bDDR##port);      \
      return MaskIO(m_bInput##port, m_bOutput##port, m_bDDR##port); \
    case STROBED_INPUT:                                             \
    case BIDIRECTIONAL:                                             \
      bData = m_bInput##port;  m_fIBF##port = false;                \
      InputReady##port();  UpdateInterrupts();                      \
      return MaskIO(bData, m_bOutput##port, m_bDDR##port);          \
    case SIMPLE_OUTPUT:                                             \
    case STROBED_OUTPUT:                                            \
      if (m_bDDR##port != 0xFF)                                     \
        m_bInput##port = MaskInput(Input##port(), m_bDDR##port);    \
      return MaskIO(m_bInput##port, m_bOutput##port, m_bDDR##port); \
    default:                                                        \
      return 0xFF;                                                  \
  }                                                                 \
}                                                                   \

// Generate the ReadX() methods for ports A and B ...
READPORT(A)
READPORT(B)


//++
//   This macro generates the WriteX() method for each of the standard ports.
// This method should be called by the DevWrite() method whenever the emulated
// software writes to the PPI port.  In simple output mode, this updates the
// port output latches (yes, even simple mode is latched!) with the data just
// written to the port and then calls the OutputX() method.  The implementation
// of OutputX() this class does nothing, however a subclass derived from this
// one can override that function and do anything it likes with the new port
// value.
// 
//   In strobed mode, this calls the StrobedOutputX() method, clears the output
// buffer empty flag (OBE), and removes any interrupt request if one was enabled.
// Note that the some documentation (talking about you, Intel!) calls the OBE
// bit OBF ("output buffer FULL") but inverts the sense so that it's active low.
// This is exactly the same result as our output buffer empty bit.
// 
//   The implementation of StrobedOutputX() in this class definition does
// nothing, however a subclass derived from this one can override that function
// and do anything it likes with the data.  When it's ready to proceed the
// subclass needs to call OutputDoneX() which will again set the OBE flag and
// clear the way for the next output byte.
// 
//   If the port is in bidirectional mode then we do exacty the same thing as
// strobed output.  In bidirectional mode the port input and output sides are
// completely independent.
// 
//   If the port is currently configured as an input then this still loads the
// output latches for the port, but otherwise does nothing.
//--
#define WRITEPORT(port)                                               \
void CPPI::Write##port (uint8_t bData)                                \
{                                                                     \
  m_bOutput##port = MaskOutput(bData, m_bDDR##port);                  \
  switch (m_Mode##port) {                                             \
    case SIMPLE_OUTPUT:                                               \
      Output##port(m_bOutput##port);                                  \
      break;                                                          \
    case STROBED_OUTPUT:                                              \
    case BIDIRECTIONAL:                                               \
      m_fOBE##port = false;                                           \
      StrobedOutput##port(m_bOutput##port);                           \
      UpdateInterrupts();                                             \
      break;                                                          \
    default:                                                          \
      break;                                                          \
  }                                                                   \
}

// Generate the WriteX() methods for ports A and B ...
WRITEPORT(A)
WRITEPORT(B)


//++
//   This macro generates the OutputDoneX() method for each of the standard
// ports.  This method can be called by a subclass derived from this one, when
// the simulation has finished reading the output data from the port. This will
// set the output buffer empty (OBE) flag and request an interrupt if enabled.
// 
//   Note that if the port is configured as an input then calling this method
// does nothing.  However, for bidirectional ports the input and output parts
// are completely independent and the output side works as above.
//--
#define OUTPUTDONE(port)                                                      \
void CPPI::OutputDone##port()                                                 \
{                                                                             \
  if ((m_Mode##port == STROBED_OUTPUT) || (m_Mode##port == BIDIRECTIONAL)) {  \
    m_fOBE##port = true;  UpdateInterrupts();                                 \
  }                                                                           \
}

// Generate the OutputDoneX methods for ports A and B ...
OUTPUTDONE(A)
OUTPUTDONE(B)


//++
//   This macro generates the SetModeX() method for all of the standard ports
// A, B and C.  This method sets the mode associated with the port and, by
// default, also initializes the associated data direction register (aka DDR).  
// If the fDDR parameter is false, however, then the current DDR contents are
// preserved.  This is necessary for some chips like the INS8154 that allow
// individual bits to be programmed independently of the overall port type.
#define SETMODE(port)                                                         \
void CPPI::SetMode##port (PORT_MODE mode, bool fDDR)                          \
{                                                                             \
  m_Mode##port = mode;                                                        \
  if (fDDR) {                                                                 \
    if ((mode == SIMPLE_INPUT) || (mode == STROBED_INPUT)) {                  \
      m_bDDR##port = 0x00;  m_bInput##port = Input##port();                   \
    } else                                                                    \
      m_bDDR##port = 0xFF;                                                    \
  }                                                                           \
}

// Generate the SetMoeX methods for ports A, B and C ...
SETMODE(A)
SETMODE(B)
SETMODE(C)


//++
//   These methods implement the ReadC() and WriteC() methods for port C.  This
// port is a special case - it doesn't support strobed, let alone bidirectional
// mode, so there's no OBE or IBF flag, no StrobedInputC() nor OutputDoneC()
// methods, and there are no interrupts associated with port C. Some PPIs don't
// even implement a port C, and for them this code is never used.  
//
//   Many PPIs use port C for alternate functions, like controlling the
// interrupt enable, for the port A or B status bits (OBE, IBF, etc), to set
// the PPI mode, and who knows what else.  None of that is handled here - the
// class that inherits from this one should override the ReadC and WriteC
// methods locally to implement whatever special behavior is required.
// 
//   One last thing - many devices implement fewer than 8 bits in port C and
// because of that port C has an extra mask that's used to mask out the unused
// bits.
//--
uint8_t CPPI::ReadC()
{
  if (m_ModeC == SIMPLE_INPUT) {
    m_bInputC = MaskInput(InputC(), m_bDDRC) & m_bMaskC;
    return m_bInputC;
  } else if (m_ModeC == SIMPLE_OUTPUT) {
    if (m_bDDRC != 0xFF)
      m_bInputC = MaskInput(InputC(), m_bDDRC) & m_bMaskC;
    return MaskIO(m_bInputC, m_bOutputC, m_bDDRC);
  } else
    return 0xFF;
}

void CPPI::WriteC (uint8_t bData)
{
  if (m_ModeC == SIMPLE_OUTPUT) {
    m_bOutputC = MaskOutput(bData, m_bDDRC) & m_bMaskC;
    OutputC(m_bOutputC);
  }
}


void CPPI::ShowDevice (ostringstream &ofs) const
{
  //++
  //   This routine will dump the state of the internal PPI registers.
  // It's used for debugging by the user interface SHOW DEVICE command.
  //--
  ofs << FormatString("PPI port A MODE=%d, DDR=0x%02X, IBUF=0x%02X, OBUF=0x%02X, IBF=%d, OBE=%d, IRQ=%d, IEN=%d\n",
    m_ModeA, m_bDDRA, m_bInputA, m_bOutputA, m_fIBFA, m_fOBEA, m_fIRQA, m_fIENA);
  ofs << FormatString("PPI port B MODE=%d, DDR=0x%02X, IBUF=0x%02X, OBUF=0x%02X, IBF=%d, OBE=%d, IRQ=%d, IEN=%d\n",
    m_ModeB, m_bDDRB, m_bInputB, m_bOutputB, m_fIBFB, m_fOBEB, m_fIRQB, m_fIENB);
  ofs << FormatString("PPI port C MODE=%d, DDR=0x%02X, MASK=0x%02x, IBUF=0x%02X, OBUF=0x%02X\n",
    m_ModeC, m_bDDRC, m_bMaskC, m_bInputC, m_bOutputC);
}
