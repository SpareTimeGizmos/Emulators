//++
// DCT11.cpp - DEC T11 microprocessor emulation
//
//   COPYRIGHT (C) 2015-2024 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
// 
// LICENSE:
//    This file is part of the SBCT11 emulator project.  SBCT11 is free
// software; you may redistribute it and/or modify it under the terms of
// the GNU Affero General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any
// later version.
//
//    SBCT11 is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License
// for more details.  You should have received a copy of the GNU Affero General
// Public License along with SBCT11.  If not, see http://www.gnu.org/licenses/.
//
// DESCRIPTION:
//   This module implements a simulation of the Digital Equipment Corporation
// DCT11 microprocessor.  In case you haven't heard, the T11 is a PDP-11 without
// memory management.  It's the little brother to the DEC F11 (11/23+) and J11
// (11/73/83/93) chips.
//
// MEMORY ARCHITECTURE
//   The T11 is unusual among microprocessors in that the same chip supports
// either an 8 bit or 16 bit data bus mode.  The Spare Time Gizmos SBCT11
// uses 16 bit mode, as does the DEC KXT11 Falcon, however this module pretends
// to use 8 bit mode.  This is more convenient here since the CMemory object
// and many of the shared UI routines assume an 8 bit micro.
// 
//   As far as accessing RAM or ROM goes, the 8 bit and 16 bit modes are
// indistinguishable except for the number of memory accesses required.  The
// header file for this module contains some inline routines to do word accesses
// as two byte operations, and this code can pretty much otherwise ignore the
// issue.
// 
//   Also, the PDP11 uses memory mapped I/O exclusively, but there's nothing
// in here to distinguish a RAM/ROM access from an I/O device.  We just call the
// CMemory CPUread()/write() methods either way, and the CMemoryMap object that
// sits between us and the real memory will take care of things.  FWIW, this is
// exactly the way the real T11 works too - it just accesses memory for everything
// and has no idea what's I/O.
// 
//   I/O devices are potentially a problem when it comes to 8 bit vs 16 bit
// memory, though.  Reading or writing a 16 bit I/O device register will actually
// occur as two distinct 8 bit operations rather than one 16 bit operation.  If
// accessing the I/O register has side effects, like clearing bits or interrupts,
// then this might be an issue.  In the case of the SBCT11 though, and indeed in
// most T11 designs, all peripherals are 8 bit devices.  They're mapped to an
// even bus address and the corresponding, odd, MSB address is ignored.  The only
// exception in the SBCT11 is the IDE data register which is actually a full 16
// bits wide.  That's handled with some special kludgy code in the IDE module.
//
// BUS TIMEOUTS and ODD ADDRESSES
//   The T11 chip does NOT implement a bus timeout and memory access never
// "fails" as it could in a real PDP-11.  This has some negative consequences
// for operating systems like RT or RSX that try to determine what peripherals
// are present by probing the associated I/O address and watching for a bus
// timeout trap.
// 
//   Both the Spare Time Gizmos SBCT11 and the DEC KXT11 Falcon have some
// special logic to fake bus timeouts for non-existent I/O devices, but that's
// beyond anything in this module.  The basic idea in both cases is for external
// logic to assert the DCT11 HALT request input, which in the DCT11 doesn't
// halt but rather traps to the restart address plus 2.  The code in the EPROM
// then simulates a bus timeout trap in software.  The SBCT11 has a "NXM control
// and status" register to control this behavior; the KXT11 just wings it.
// 
//   And, also unlike a real PDP-11, the T11 does not have an odd address trap
// mechanism either.  Everything that expects a word aligned operand - instruction
// fetches, trap vector fetches, word instructions, etc - all simply ignore the
// least significant bit of the address.  In effect, odd addresses will be rounded
// down to the next lower even address.  No indication is given to the program
// that this has happened.  Here, this is handled by the READW() and WRITEW()
// methods and works exactly the same as the real chip.
//
// TRAPS AND INTERRUPTS
//   The T11 has a fairly elaborate set of traps as well as a complex multiple
// device external vectored interrupt scheme.  At the end of every execute
// cycle (yes, the END, not before!) the T11 checks for the following conditions
// in this order - 
// 
//      1) HALT request (external or internal by executing the HALT instruction)
//      2) T (trace) bit trap
//      3) Power Fail interrupt
//      4) External device (CP<3:0>) interrupt request 
//      5) Instruction traps (EMT, TRAP, Illegal, Reserved, etc)
// 
// If any of these conditions are true then the DCT11 takes the appropriate trap
// and clears the associated request.  It's simple if only one of these conditions
// is true, but it's possible for more than one request to occur simultaneously.
// For example, what if an external interrupt occurs while while an EMT opcode
// is being executed?  The DCT11 will actually take BOTH traps/interrupts, in
// reverse priority order.  The T11 first pushes the PC and PSW of the EMT and
// loads the new PC and PSW for the EMT trap routine.  Then, before even one
// instruction of the EMT trap is executed, then T11 pushes that new PC and PSW
// and loads the PC and PSW for the external interrupt. 
// 
//   To implement that, this code keeps a bit vector in m_bRequests, with one 
// bit assigned to each of the five conditions listed above.  If n_bRequests is
// zero then nothing needs to be done and that makes testing easy, but if it's
// non-zero then we go thru it, bit by bit, and and service every request that's
// pending.
//
//    The T bit trap is a bit of a special case.  Since traps are handled at the
// END of instruction execution, the DCT11 actually tests the PSW T bit flag just
// BEFORE fetching an instruction and, if the T bit is set, the T11 sets an
// internal "trace trap pending" flag.  That latter flag is the one that's tested
// at the end of instruction execution.  This means that an instruction which sets
// the T bit will NOT be traced (since the PSW T bit wasn't set when that opcode
// was fetched) BUT an instruction which clears the T bit WILL be traced.  That's
// the way a real PDP11 works, and that's how the DEC debugger software expects
// us to behave.
// 
//   Also, remember that any trap or interrupt loads a new PSW in addition to
// the PC, and whether the interrupt/trap service routine is traced depends on
// whether that new PSW also has the T bit set.  Presumably it does not, and
// then execution of the ISR is transparent to any debugger.  When the ISR exits
// it will restore the original PSW from the stack and tracing will continue.
// TIMING
//   This module attempts to keep fairly accurate track of the simulated time for
// emulating peripherals and other things.  All timings computed here are for the
// T11 chip (and no other PDP-11!) in 16 bit bus mode and with refresh disabled.
// Execution times for 8 bit mode are of course longer, roughly double in most
// cases, and timing with refresh enabled is somewhat non-deterministic.  The latter
// all depends on where the refresh cycles happen to fall.
// 
//   MOST of the routines in this module that emulate something, such as CALCEA,
// ADD, SUB, CMP, BRANCH, or whatever, return a uint32 which is the number of T11
// MICROCYCLES (not clock cycles!) required to execute that operation.  With the
// PDP11 this can get complicated becuase of the various addressing modes and the
// differences between source and destination addressing times, and the code has to
// be careful to add everything up correctly.
//
//   On the T11 one microcycle is three clocks, or four clocks with the extended
// microcycle mode, and the AddCycles() method takes care of converting the cycle
// count to actual nanoseconds.
// 
// OTHER RANDOM NOTES
//
//   The T11 internal registers (R0-R7 and PS) are not addressable in the I/O page,
// as they are on most PDP-11s.  Also, address 177776 DOES NOT reference the PSW as
// it would on other -11s.  On the T11 you must use the MTPS and MFPS instructions.
//
//   RESET (MasterClear()) starts execution at one of half a dozen hardwired restart
// addresses, as selected by the T11 MODE register.  And the HALT instruction doesn't
// actually halt but instead causes a restart at the normal startup address +4.
//
//   The T11 DOES implement the reserved instruction trap.  
//
// REVISION HISTORY:
// 27-FEB-20  RLA  New file.
// 24-JUN-22  RLA  Massive rewrite and update.
// 30-JUN-22  RLA  For mode 6 and 7 addressing, be careful of the order when
//                  incrementing the PC and then adding!
//                 Byte instructions with a register destination modify the low
//                  byte and leave the upper byte unchanged.  MOVB to a register
//                  is an exception - it sign extends!
//  3-JUL-22  RLA  FETCHW() needs to handle odd addresses exactly as READW() does!
//                 BITB and BITW should be SRC1 & SRC2 (not SRC1 & ~SRC2!)
//                 NEGB and NEGW get the carry bit backward
//                 ASLB gets C wrong (use ISNEGB not ISNEGW!)
//  4-JUL-22  RLA  ADCB/ADCW/SBCB/SBCW all wrong!  Can't change C until after
//                  we compute the new result!
//                 MFPS to a register destination works like MOVB and sign
//                  extends to the upper register byte
//                 Implement the PSW T bit and trace bit traps
//                 We can now run all NKXAA DCT11 tests up thru T106!  Tests
//                  starting with T107 are all Falcon/QBUS specific...
//  9-JUL-22  RLA Add proper trap and interrupt handling.
// 10-JUL-22  RLA Add endless loop detection to the branch opcodes.
// 18-JUL-22  RLA Implement the WAIT instruction...
//                RESET needs to call CMemoryMap::ClearDevices()
//                Invent AddCycles() to handle long/short microcycles
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>             // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include <cstring>              // needed for memset()
#include "EMULIB.hpp"           // emulator library definitions
#include "LogFile.hpp"          // emulator library message logging facility
#include "CommandParser.hpp"    // needed for type KEYWORD
#include "Interrupt.hpp"        // interrupt simulation logic
#include "EventQueue.hpp"       // I/O device event queue simulation
#include "MemoryTypes.h"        // address_t and word_t data types
#include "Memory.hpp"           // CMemory memory emulation object
#include "MemoryMap.hpp"        // Memory mapping for the SBCT11/DCT11
#include "DCT11opcodes.hpp"     // T11 opcode definitions
#include "CPU.hpp"              // CCPU base class definitions
#include "Device.hpp"           // CDevice I/O device emulation objects
#include "DCT11.hpp"            // declarations for this module


// Internal CPU register names for GetRegisterNames() ...
const CCmdArgKeyword::KEYWORD CDCT11::g_keysRegisters[] = {
  {"R0",  CDCT11::REG_R0},  {"R1",  CDCT11::REG_R1},
  {"R2",  CDCT11::REG_R2},  {"R3",  CDCT11::REG_R3},
  {"R4",  CDCT11::REG_R4},  {"R5",  CDCT11::REG_R5},
  {"SP",  CDCT11::REG_SP},  {"PC",  CDCT11::REG_PC},
  {"PSW", CDCT11::REG_PSW}, {NULL, 0}
};

// Shortcuts for popular PDP11 registers ...
#define REG(n)  m_wR[n]
#define PC  REG(REG_PC)
#define SP  REG(REG_SP)
#define PSW m_bPSW


////////////////////////////////////////////////////////////////////////////////
///////////////////////////// CPU CONTROL METHODS //////////////////////////////
////////////////////////////////////////////////////////////////////////////////

CDCT11::CDCT11 (uint16_t wMode, CMemoryMap *pMemory, CEventQueue *pEvent, CPIC11 *pInterrupt)
  : CCPU(pMemory, pEvent, pInterrupt)
{
  //++
  //--
  SetCrystalFrequency(DEFAULT_CLOCK);
  SetMode(wMode);
  MasterClear();
}

uint16_t CDCT11::GetStartAddress() const
{
  //++
  // Compute the start address selected by the mode register ...
  //--
  switch (m_wMode & 0160000) {
    case MODE_172000:  return 0172000;
    case MODE_173000:  return 0173000;
    case MODE_000000:  return 0000000;
    case MODE_010000:  return 0010000;
    case MODE_020000:  return 0020000;
    case MODE_040000:  return 0040000;
    case MODE_100000:  return 0100000;
    case MODE_140000:  return 0140000;
  }
  return 0; // just to make the compiler happy!
}

void CDCT11::MasterClear()
{
  //++
  //   This routine resets the DCT11 to a power on state.  It clears all the
  // registers and sets the PC to the start address selected by the MODE register.
  // The PSW is set to priority 7, which disables all maskable interrupts.
  //--
  CCPU::MasterClear();
  memset(m_wR, 0, sizeof(m_wR));
  SetPC(GetStartAddress());
  m_bRequests = 0;  m_wItrapVector = 0;
  PSW = PSW_PRIO;
}

uint16_t CDCT11::GetRegister (cpureg_t nReg) const
{
  //++
  // This method will return the contents of an internal CPU register ...
  //--
  if (nReg < 8)
    return REG(nReg);
  else if (nReg == REG_PSW)
    return MKWORD(0, PSW);
  else
    return 0;
}

void CDCT11::SetRegister (cpureg_t nReg, uint16_t wData)
{
  //++
  // Change the contents of an internal CPU register ...
  //--
  if (nReg < 8)
    REG(nReg) = wData;
  else if (nReg == REG_PSW)
    PSW = MASK8(wData);
}

string CDCT11::GetPSW() const
{
  //++
  // Decode the current PC and return it as a string ....
  //--
  string sPSW = FormatString("PRI%d ", (PSW & PSW_PRIO) >> 5);
  if (ISPSW(PSW_T)) sPSW += "T";
  if (ISN()) sPSW += "N";
  if (ISZ()) sPSW += "Z";
  if (ISV()) sPSW += "V";
  if (ISC()) sPSW += "C";
  return sPSW;
}

void CDCT11::AddCycles (uint32_t lCycles)
{
  //++
  //   This routine will update the CEventQueue with the elapsed time required
  // to execute this instruction.  The argument we're given is the number of
  // DCT11 microcycles and we need to convert that to nanoseconds for the event
  // queue.  A standard DCT11 microcycle takes 3 clocks, and the "long" micro-
  // cycle takes 4.  The selection between the two is determined by a bit in
  // the mode register.
  //--
  assert(m_pEvents != NULL);
  uint64_t llTime = ((uint64_t) lCycles)
                  * HZTONS(m_lClockFrequency)
                  * (IsLMC() ? 4ULL : 3ULL);
  if (llTime > 0) m_pEvents->AddTime(llTime);
}


////////////////////////////////////////////////////////////////////////////////
///////////////////////// DOUBLE OPERAND INSTRUCTIONS //////////////////////////
////////////////////////////////////////////////////////////////////////////////

uint32_t CDCT11::CALCEA (bool fByte, uint8_t bMode, uint8_t bReg, uint16_t &wEA)
{
  //++
  //   Calculate the actual address of the operand given the addressing mode
  // and the associated register.  Normally word addresses are calculated, but
  // if fByte is true then a byte address will be calculated instead.  The only
  // instance where byte vs word makes a difference is for modes 2 and 4 ((R)+
  // autoincrement and -(R) autodecrement), in which case the register is
  // incremented or decremented by 1 for bytes and 2 for words.  In all other
  // cases the result is the same regardless of byte or word mode.
  //
  //   Yes, the deferred modes, (R)+ and -(R), both increment or decrement the
  // register by two even in byte mode.  That's because the register is
  // actually pointing to an address, which is always a word regardless of the
  // actual operand.  The same is true for the PC relative displacement modes.
  // And the PDP-11 has a special case for the SP or PC used in autoincrement
  // or autodecrement mode - these registers always adjust by 2, regardless of
  // the byte vs word.
  //
  //   Oh, and note that this routine will assert if called for mode zero.
  // That's because mode zero addresses a register, and registers don't have
  // an associated memory address...  The caller will have to weed out those
  // cases before we get here.
  // 
  //   Lastly, this routine returns the number of extra microcycles required
  // by this addressing mode, relative to mode 0!  These times don't include the
  // base time for the instruction; it's up to the caller to add that.  Also,
  // these additional addressing mode times are the same regardless of whether
  // we're calculating a source or a destination address, BUT in the case of a
  // destination then the T11 needs one more microcycle to actually store the
  // result.  That's true even for mode 0, and it's also up to the caller to
  // add in that extra microcycle.
  //--
  assert((bMode < 8) && (bReg < 8));
  uint16_t w;
  uint16_t wInc = (fByte && (bReg!=REG_SP) && (bReg!=REG_PC)) ? 1 : 2;
  switch (bMode) {
    case 0: assert(false);                                                // R!!
    case 1: wEA = REG(bReg);                                  return 2;   // (R)
    case 2: wEA = REG(bReg);  INC16(REG(bReg), wInc);         return 2;   // (R)+
    case 3: wEA = READW(REG(bReg));  INC16(REG(bReg), 2);     return 4;   // @(R)+
    case 4: DEC16(REG(bReg), wInc);  wEA = REG(bReg);         return 3;   // -(R)
    case 5: DEC16(REG(bReg), 2);  wEA = READW(REG(bReg));     return 5;   // @-(R)
    //   Be careful with the next two cases - FETCHW() will increment the PC and
    // if bReg happens to be 7 then the exact order is critical!
    case 6: w = FETCHW();  wEA = ADD16(w, REG(bReg));         return 5;   // disp(R)
    case 7: w = FETCHW();  wEA = READW(ADD16(w, REG(bReg)));  return 7;   // @disp(R)
  }
  return 0; // avoid C4715...
}

uint32_t CDCT11::MOVW (uint8_t bSRCmode, uint8_t bSRCreg, uint8_t bDSTmode, uint8_t bDSTreg)
{
  //++
  //   This is the basic code for the MOV (word) instruction. This routine spends
  // a lot of time explaining the subtleties of calculating the source and
  // destination addresses and fetching and storing the operands, since that's
  // all MOV does.  All the other two operand instructions - CMP, BIT, BIS, BIC,
  // ADD and SUB, are all pretty much the same in this regard, and the only
  // difference is what happens to the data in between source and destination.
  //--

  //   The first thing every PDP11 instruction does is to calculate the effective
  // address, and in the case of the DCT11 it calculates both EAs BEFORE doing
  // anything else, INCLUDING fetching the source operand.  This order is critical
  // when the EA calculation has side effects, like (R)+ or -(R).  Not all PDP11
  // models work the same way, but on the DCT11 instructions like OPR R,(R)+ or
  // OPR R,-(R), etc will use the MODIFIED value of R as the source operand!
  //
  //   Oh, and remember that in mode 0 for either the source or destination we
  // can't calculate an EA.  In that case the register _is_ the EA!  Also note
  // that in this emulator all instruction timing is calculated relative to
  // register mode addressing, since that is always the fastest.  So register
  // mode takes zero time for the EA calculation...
  uint16_t wSRCea = 0, wDSTea = 0;  uint32_t nCycles = 0;
  if (bSRCmode != 0) nCycles += CALCEAW(bSRCmode, bSRCreg, wSRCea);
  if (bDSTmode != 0) nCycles += CALCEAW(bDSTmode, bDSTreg, wDSTea);

  //   Now, fetch the source operand.  This takes no additional time, since that
  // was already included in the EA calculation ...
  uint16_t wSource = (bSRCmode == 0) ? REG(bSRCreg) : READW(wSRCea);

  //   Set the condition codes.  MOV sets or clears N and Z according to the
  // source operand.  It always clears V and leaves C unchanged.
  SetZNW(wSource);  SetV(false);

  //   And store the result.  Note that when we did CALCEAW() for the destination
  // that already included most of the time destination time, BUT writing the
  // destination takes two more cycles than reading the source operand, so we
  // account for that here.  EXCEPT, if it happens that the destination is a
  // register, and then that only takes one more cycle.
  if (bDSTmode == 0) {
    REG(bDSTreg) = wSource;  nCycles += 1;
  } else {
    WRITEW(wDSTea, wSource);  nCycles += 2;
  }

  //   The basic execution time for MOV, not counting all the source and
  // destination addressing time that we've already considered, is 3 cycles...
  return nCycles + 3;
}

uint32_t CDCT11::MOVB (uint8_t bSRCmode, uint8_t bSRCreg, uint8_t bDSTmode, uint8_t bDSTreg)
{
  //++
  //   And this is the code for MOVB - the same as above, but byte mode.  Note
  // that MOVB is unique among byte instructions in that a MOVB to a register
  // destination sign extends the result into the upper byte.  Every other byte
  // instruction with a register destination will zero fill.
  //--
  uint16_t wSRCea = 0, wDSTea = 0;  uint32_t nCycles = 0;
  if (bSRCmode != 0) nCycles += CALCEAB(bSRCmode, bSRCreg, wSRCea);
  if (bDSTmode != 0) nCycles += CALCEAB(bDSTmode, bDSTreg, wDSTea);
  uint8_t bSource = (bSRCmode == 0) ? LOBYTE(REG(bSRCreg)) : READB(wSRCea);
  SetZNB(bSource);  SetV(false);
  if (bDSTmode == 0) {
    REG(bDSTreg) = SXT8(bSource);  nCycles += 1;
  } else {
    WRITEB(wDSTea, bSource);  nCycles += 2;
  }
  return nCycles + 3;
}


//++
//   These macros are used by all the remaining double operand instructions, and
// by most of the single operand instructions, to generate exactly the same code
// as the two examples you see above.  This saves a lot of typing!
//--
#define FETCH_DOUBLE_OPERANDS_BYTE(bSRC, bDST)                            \
  uint16_t wSRCea = 0, wDSTea = 0;  uint32_t nCycles = 0;                 \
  if (bSRCmode != 0) nCycles += CALCEAB(bSRCmode, bSRCreg, wSRCea);       \
  if (bDSTmode != 0) nCycles += CALCEAB(bDSTmode, bDSTreg, wDSTea);       \
  uint8_t bSRC = (bSRCmode == 0) ? LOBYTE(REG(bSRCreg)) : READB(wSRCea);  \
  uint8_t bDST = (bDSTmode == 0) ? LOBYTE(REG(bDSTreg)) : READB(wDSTea);

#define FETCH_DOUBLE_OPERANDS_WORD(wSRC, wDST)                            \
  uint16_t wSRCea = 0, wDSTea = 0;  uint32_t nCycles = 0;                 \
  if (bSRCmode != 0) nCycles += CALCEAW(bSRCmode, bSRCreg, wSRCea);       \
  if (bDSTmode != 0) nCycles += CALCEAW(bDSTmode, bDSTreg, wDSTea);       \
  uint16_t wSRC = (bSRCmode == 0) ? REG(bSRCreg) : READW(wSRCea);         \
  uint16_t wDST = (bDSTmode == 0) ? REG(bDSTreg) : READW(wDSTea);

#define FETCH_SINGLE_OPERAND_BYTE(bDST)                                   \
  uint16_t wDSTea = 0;  uint32_t nCycles = 0;                             \
  if (bDSTmode != 0) nCycles += CALCEAB(bDSTmode, bDSTreg, wDSTea);       \
  uint8_t bDST = (bDSTmode == 0) ? LOBYTE(REG(bDSTreg)) : READB(wDSTea);

#define FETCH_SINGLE_OPERAND_WORD(wDST)                                   \
  uint16_t wDSTea = 0;  uint32_t nCycles = 0;                             \
  if (bDSTmode != 0) nCycles += CALCEAW(bDSTmode, bDSTreg, wDSTea);       \
  uint16_t wDST = (bDSTmode == 0) ? REG(bDSTreg) : READW(wDSTea);

#define STORE_RESULT_BYTE(bResult)                                        \
  if (bDSTmode == 0) {                                                    \
    REG(bDSTreg) = MKWORD(HIBYTE(REG(bDSTreg)), bResult);  nCycles += 1;  \
  } else {                                                                \
    WRITEB(wDSTea, bResult);  nCycles += 2;                               \
  }

#define STORE_RESULT_WORD(wResult)                                        \
  if (bDSTmode == 0) {                                                    \
    REG(bDSTreg) = wResult;  nCycles += 1;                                \
  } else {                                                                \
    WRITEW(wDSTea, wResult);  nCycles += 2;                               \
  }


uint32_t CDCT11::BISW (uint8_t bSRCmode, uint8_t bSRCreg, uint8_t bDSTmode, uint8_t bDSTreg)
{
  //++
  // BIT SET (WORD) ...
  //   Computes DST <= DST OR SRC.
  //   Sets Z and N; clears V and doesn't change C.
  //--
  FETCH_DOUBLE_OPERANDS_WORD(wSRC, wDST);
  wDST = wDST | wSRC;  SetZNW(wDST);  SetV(false);
  STORE_RESULT_WORD(wDST);
  return nCycles + 3;
}

uint32_t CDCT11::BISB (uint8_t bSRCmode, uint8_t bSRCreg, uint8_t bDSTmode, uint8_t bDSTreg)
{
  //++
  // BIT SET (BYTE) ...
  //   Computes DST <= DST OR SRC.
  //   Sets Z and N; clears V and doesn't change C.
  //--
  FETCH_DOUBLE_OPERANDS_BYTE(bSRC, bDST);
  bDST = bDST | bSRC;  SetZNB(bDST);  SetV(false);
  STORE_RESULT_BYTE(bDST);
  return nCycles + 3;
}

uint32_t CDCT11::BICW (uint8_t bSRCmode, uint8_t bSRCreg, uint8_t bDSTmode, uint8_t bDSTreg)
{
  //++
  // BIT CLEAR (WORD) ...
  //   Computes DST <= DST AND NOT SRC.
  //   Sets Z and N; clears V and doesn't change C.
  //--
  FETCH_DOUBLE_OPERANDS_WORD(wSRC, wDST);
  wDST = wDST & ~wSRC;  SetZNW(wDST);  SetV(false);
  STORE_RESULT_WORD(wDST);
  return nCycles + 3;
}

uint32_t CDCT11::BICB (uint8_t bSRCmode, uint8_t bSRCreg, uint8_t bDSTmode, uint8_t bDSTreg)
{
  //++
  // BIT CLEAR (BYTE) ...
  //   Computes DST <= DST AND NOT SRC.
  //   Sets Z and N; clears V and doesn't change C.
  //--
  FETCH_DOUBLE_OPERANDS_BYTE(bSRC, bDST);
  bDST = bDST & ~bSRC;  SetZNB(bDST);  SetV(false);
  STORE_RESULT_BYTE(bDST);
  return nCycles + 3;
}

uint32_t CDCT11::BITW (uint8_t bSRCmode, uint8_t bSRCreg, uint8_t bDSTmode, uint8_t bDSTreg)
{
  //++
  // BIT TEST (WORD) ...
  //   Computes DST AND SRC, but doesn't store the result!
  //   Sets Z and N; clears V and doesn't change C.
  //--
  FETCH_DOUBLE_OPERANDS_WORD(wSRC1, wSRC2);
  uint16_t wResult = wSRC2 & wSRC1;  SetZNW(wResult);  SetV(false);
  //   Note that even though this doesn't actually store the result, there is
  // still one extra cycle required for the destination time (but not two, as
  // it would be if the result was actually stored).  I've no idea what happens
  // in this extra cycle!
  return nCycles + 3 + 1;
}

uint32_t CDCT11::BITB (uint8_t bSRCmode, uint8_t bSRCreg, uint8_t bDSTmode, uint8_t bDSTreg)
{
  //++
  // BIT TEST (BYTE) ...
  //   Computes DST AND SRC, but doesn't store the result!
  //   Sets Z and N; clears V and doesn't change C.
  //--
  FETCH_DOUBLE_OPERANDS_BYTE(bSRC1, bSRC2);
  uint8_t bResult = bSRC2 & bSRC1;  SetZNB(bResult);  SetV(false);
  return nCycles + 3 + 1;  // See BITW() ...
}

uint32_t CDCT11::CMPW (uint8_t bSRCmode, uint8_t bSRCreg, uint8_t bDSTmode, uint8_t bDSTreg)
{
  //++
  // COMPARE (WORD) ...
  //   Computes SRC - DST, but doesn't store the result!
  //   Sets Z if the result is zero.
  //   Sets N if the signed result is .LT. zero.
  //   Sets V if signed result would have overflowed.
  //   Sets C if the unsigned result would have overflowed.
  //   Note that CMP and CMPB compute SRC - DST, which is the reverse of SUB!!
  //--
  FETCH_DOUBLE_OPERANDS_WORD(wSRC1, wSRC2);
  uint16_t wResult = MASK16(wSRC1 - wSRC2);
  SetN(ISNEGW(wResult));
  SetZ(wResult==0);
  SetV(ISNEGW((wSRC1 ^ wSRC2) & (~wSRC2 ^ wResult)));
  SetC(wSRC1 < wSRC2);
  return nCycles + 3 + 1;
}

uint32_t CDCT11::CMPB (uint8_t bSRCmode, uint8_t bSRCreg, uint8_t bDSTmode, uint8_t bDSTreg)
{
  //++
  // COMPARE (BYTE) ...
  //   Computes SRC - DST, but doesn't store the result!
  //   Sets Z if the result is zero.
  //   Sets N if the signed result is .LT. zero.
  //   Sets V if signed result would have overflowed.
  //   Sets C if the unsigned result would have overflowed.
  //   Note that CMP and CMPB compute SRC - DST, which is the reverse of SUB!!
  //--
  FETCH_DOUBLE_OPERANDS_BYTE(bSRC1, bSRC2);
  uint8_t bResult = MASK8(bSRC1 - bSRC2);
  SetN(ISNEGB(bResult));
  SetZ(bResult==0);
  SetV(ISNEGB((bSRC1 ^ bSRC2) & (~bSRC2 ^ bResult)));
  SetC(bSRC1 < bSRC2);
  return nCycles + 3 + 1;
}

uint32_t CDCT11::ADDW (uint8_t bSRCmode, uint8_t bSRCreg, uint8_t bDSTmode, uint8_t bDSTreg)
{
  //++
  // ADD (WORD) ...
  //   Computes DST <= DST + SRC
  //   Sets Z if the result is zero.
  //   Sets N if the signed result is .LT. zero.
  //   Sets V if signed result would have overflowed.
  //   Sets C if the unsigned result would have overflowed.
  //   Note that there is no ADDB (byte) - there's only a 16 bit word add!
  //--
  FETCH_DOUBLE_OPERANDS_WORD(wSRC1, wSRC2);
  uint16_t wResult = MASK16(wSRC2 + wSRC1);
  SetN(ISNEGW(wResult));
  SetZ(wResult == 0);
  SetV(ISNEGW((~wSRC1 ^ wSRC2) & (wSRC1 ^ wResult)));
  SetC(wResult < wSRC1); // ??? IS THIS REALLY RIGHT??  SEE simh!
  STORE_RESULT_WORD(wResult);
  return nCycles + 3;
}

uint32_t CDCT11::SUBW (uint8_t bSRCmode, uint8_t bSRCreg, uint8_t bDSTmode, uint8_t bDSTreg)
{
  //++
  // SUBTRACT (WORD) ...
  //   Computes DST <= DST - SRC
  //   Sets Z if the result is zero.
  //   Sets N if the signed result is .LT. zero.
  //   Sets V if signed result would have overflowed.
  //   Sets C if the unsigned result would have overflowed.
  //   Note that there is no SUBB (byte) - there's only a 16 bit word subtract!
  //   (Actually there is a "SUBB", but it's SUBTRACT WITH BORROW, not byte!)
  //--
  FETCH_DOUBLE_OPERANDS_WORD(wSRC1, wSRC2);
  uint16_t wResult = MASK16(wSRC2 - wSRC1);
  SetN(ISNEGW(wResult));
  SetZ(wResult == 0);
  SetV(ISNEGW((wSRC1 ^ wSRC2) & (~wSRC1 ^ wResult)));
  SetC(wSRC2 < wSRC1);
  STORE_RESULT_WORD(wResult);
  return nCycles + 3;
}

uint32_t CDCT11::XORW (uint8_t bSRCreg, uint8_t bDSTmode, uint8_t bDSTreg)
{
  //++
  // EXCLUSIVE OR (WORD)
  //   Computes DST <= REG XOR DST
  //   Sets Z and N; clears V and doesn't change C.
  //   Note that this is a "pseudo" two operand instruction
  //     - the source MUST be a register.
  //--
  uint16_t wDSTea = 0;  uint32_t nCycles = 0;
  if (bDSTmode != 0) nCycles += CALCEAW(bDSTmode, bDSTreg, wDSTea);
  uint16_t wDST = (bDSTmode == 0) ? REG(bDSTreg) : READW(wDSTea);
  wDST ^= REG(bSRCreg);  SetZNW(wDST);  SetV(false);
  STORE_RESULT_WORD(wDST);
  return nCycles + (bDSTmode == 0) ? 4 : 5;
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////// SINGLE OPERAND INSTRUCTIONS /////////////////////////
////////////////////////////////////////////////////////////////////////////////

uint32_t CDCT11::CLRW (uint8_t bDSTmode, uint8_t bDSTreg)
{
  //++
  // CLEAR (WORD)
  //   DST <= 0
  //   Always sets Z and clears N, V and C.
  //   Note that on the T11 this ALWAYS reads the destination location first,
  //     even though it obviously doesn't need to.  This is important and can
  //     have consequences for I/O registers, where reading may have side
  //     effects.
  //--
  FETCH_SINGLE_OPERAND_WORD(wDST);
  SetZ(true);  SetV(false);  SetN(false);  SetC(false);
  // Avoid "unused variable" warnings for wDST!
  wDST = 0;  STORE_RESULT_WORD(wDST);
  return nCycles + (bDSTmode == 0) ? 4 : 5;
}

uint32_t CDCT11::CLRB (uint8_t bDSTmode, uint8_t bDSTreg)
{
  //++
  // CLEAR (BYTE)
  //   DST <= 0
  //   Always sets Z and clears N, V and C.
  //   See other comments under CLRW()...
  //--
  FETCH_SINGLE_OPERAND_BYTE(bDST);
  SetZ(true);  SetV(false);  SetN(false);  SetC(false);
  // Avoid "unused variable" warnings for bDST!
  bDST = 0;  STORE_RESULT_BYTE(bDST);
  return nCycles + (bDSTmode == 0) ? 4 : 5;
}

uint32_t CDCT11::COMW (uint8_t bDSTmode, uint8_t bDSTreg)
{
  //++
  // COMPLEMENT (WORD)
  //   DST <= ~DST
  //   Updates N and Z; clears V and always sets C.
  //--
  FETCH_SINGLE_OPERAND_WORD(wDST);
  wDST = MASK16(~wDST);
  SetZNW(wDST);  SetV(false);  SetC(true);
  STORE_RESULT_WORD(wDST);
  return nCycles + (bDSTmode == 0) ? 4 : 5;
}

uint32_t CDCT11::COMB (uint8_t bDSTmode, uint8_t bDSTreg)
{
  //++
  // COMPLEMENT (BYTE)
  //   DST <= ~DST
  //   Updates N and Z; clears V and always sets C.
  //--
  FETCH_SINGLE_OPERAND_BYTE(bDST);
  bDST = MASK8(~bDST);
  SetZNB(bDST);  SetV(false);  SetC(true);
  STORE_RESULT_BYTE(bDST);
  return nCycles + (bDSTmode == 0) ? 4 : 5;
}

uint32_t CDCT11::INCW (uint8_t bDSTmode, uint8_t bDSTreg)
{
  //++
  // INCREMENT (WORD)
  //   DST <= DST+1
  //   Updates N and Z; doesn't change C.
  //   Sets V if DST rolled over from 077777 to 100000
  //--
  FETCH_SINGLE_OPERAND_WORD(wDST);
  SetV(wDST == 077777);
  wDST = MASK16(wDST+1);
  SetZNW(wDST);
  STORE_RESULT_WORD(wDST);
  return nCycles + (bDSTmode == 0) ? 4 : 5;
}

uint32_t CDCT11::INCB (uint8_t bDSTmode, uint8_t bDSTreg)
{
  //++
  // INCREMENT (BYTE)
  //   DST <= DST+1
  //   Updates N and Z; doesn't change C.
  //   Sets V if DST rolled over from 0177 to 0200
  //--
  FETCH_SINGLE_OPERAND_BYTE(bDST);
  SetV(bDST == 0177);
  bDST = MASK8(bDST + 1);
  SetZNB(bDST);
  STORE_RESULT_BYTE(bDST);
  return nCycles + (bDSTmode == 0) ? 4 : 5;
}

uint32_t CDCT11::DECW (uint8_t bDSTmode, uint8_t bDSTreg)
{
  //++
  // DECREMENT (WORD)
  //   DST <= DST-1
  //   Updates N and Z; doesn't change C.
  //   Sets V if DST rolled over from 100000 to 077777
  //--
  FETCH_SINGLE_OPERAND_WORD(wDST);
  SetV(wDST == 0100000);
  wDST = MASK16(wDST - 1);
  SetZNW(wDST);
  STORE_RESULT_WORD(wDST);
  return nCycles + (bDSTmode == 0) ? 4 : 5;
}

uint32_t CDCT11::DECB (uint8_t bDSTmode, uint8_t bDSTreg)
{
  //++
  // DECREMENT (BYTE)
  //   DST <= DST-1
  //   Updates N and Z; doesn't change C.
  //   Sets V if DST rolled over from 0200 to 0177
  //--
  FETCH_SINGLE_OPERAND_BYTE(bDST);
  SetV(bDST == 0200);
  bDST = MASK8(bDST - 1);
  SetZNB(bDST);
  STORE_RESULT_BYTE(bDST);
  return nCycles + (bDSTmode == 0) ? 4 : 5;
}

uint32_t CDCT11::NEGW (uint8_t bDSTmode, uint8_t bDSTreg)
{
  //++
  // TWO'S COMPLEMENT NEGATE (WORD)
  //   DST <= -DST
  //   Updates N and Z
  //   Sets V if DST is 100000 (which doesn't change when negated!)
  //   Clears C if result is 0; sets C otherwise
  //--
  FETCH_SINGLE_OPERAND_WORD(wDST);
  wDST = MASK16(-wDST);
  SetZNW(wDST);  SetV(wDST == 0100000);  SetC(wDST != 0);
  STORE_RESULT_WORD(wDST);
  return nCycles + (bDSTmode == 0) ? 4 : 5;
}

uint32_t CDCT11::NEGB (uint8_t bDSTmode, uint8_t bDSTreg)
{
  //++
  // TWO'S COMPLEMENT NEGATE (BYTE)
  //   DST <= -DST
  //   Updates N and Z
  //   Sets V if DST is 0200 (which doesn't change when negated!)
  //   Clears C if result is 0; sets C otherwise
  //--
  FETCH_SINGLE_OPERAND_BYTE(bDST);
  bDST = MASK8(-bDST);
  SetZNB(bDST);  SetV(bDST == 0200);  SetC(bDST != 0);
  STORE_RESULT_BYTE(bDST);
  return nCycles + (bDSTmode == 0) ? 4 : 5;
}

uint32_t CDCT11::ADCW (uint8_t bDSTmode, uint8_t bDSTreg)
{
  //++
  // ADD WITH CARRY (WORD)
  //   DST <= DST + C
  //   Updates N and Z
  //   Sets V if DST was 077777 and C is 1
  //   Sets C if DST was 177777 and C is 1
  //--
  FETCH_SINGLE_OPERAND_WORD(wDST);
  uint16_t wResult = MASK16(wDST + ISC());
  SetV((wDST == 0077777) && ISC());
  SetC((wDST == 0177777) && ISC());
  SetZNW(wResult);  STORE_RESULT_WORD(wResult);
  return nCycles + (bDSTmode == 0) ? 4 : 5;
}

uint32_t CDCT11::ADCB (uint8_t bDSTmode, uint8_t bDSTreg)
{
  //++
  // ADD WITH CARRY (BYTE)
  //   DST <= DST + C
  //   Updates N and Z
  //   Sets V if DST was 0177 and C is 1
  //   Sets C if DST was 0377 and C is 1
  //--
  FETCH_SINGLE_OPERAND_BYTE(bDST);
  uint8_t bResult = MASK8(bDST + ISC());
  SetV((bDST == 0177) && ISC());
  SetC((bDST == 0377) && ISC());
  SetZNB(bResult);  STORE_RESULT_BYTE(bResult);
  return nCycles + (bDSTmode == 0) ? 4 : 5;
}

uint32_t CDCT11::SBCW (uint8_t bDSTmode, uint8_t bDSTreg)
{
  //++
  // SUBTRACT WITH CARRY (WORD)
  //   DST <= DST - C
  //   Updates N and Z
  //   Sets V if DST was 100000 and C is 1
  //   Sets C if DST was 0 and C is 1
  //--
  FETCH_SINGLE_OPERAND_WORD(wDST);
  uint16_t wResult = MASK16(wDST - ISC());
  SetV((wDST == 0100000) && ISC());
  SetC((wDST == 0      ) && ISC());
  SetZNW(wResult);  STORE_RESULT_WORD(wResult);
  return nCycles + (bDSTmode == 0) ? 4 : 5;
}

uint32_t CDCT11::SBCB (uint8_t bDSTmode, uint8_t bDSTreg)
{
  //++
  // SUBTRACT WITH CARRY (BYTE)
  //   DST <= DST - C
  //   Updates N and Z
  //   Sets V if DST was 0200 and C is 1
  //   Sets C if DST was 0 and C is 1
  //--
  FETCH_SINGLE_OPERAND_BYTE(bDST);
  uint8_t bResult = MASK8(bDST - ISC());
  SetV((bDST == 0200) && ISC());
  SetC((bDST == 0   ) && ISC());
  SetZNB(bResult);  STORE_RESULT_BYTE(bResult);
  return nCycles + (bDSTmode == 0) ? 4 : 5;
}

uint32_t CDCT11::TSTW (uint8_t bDSTmode, uint8_t bDSTreg)
{
  //++
  // TEST (WORD)
  //   Updates N and Z according to DST
  //   Always clears C and V
  //   I don't know if the DCT11 actually writes the result (which would be
  //     unchanged, after all) back to memory or not.  However the timing for
  //     this instruction is one cycle faster than all other single operand
  //     instructions, so I take that to mean it DOESN'T write back a result
  //     As before, this point has some consequences for I/O registers with
  //     side effects.
  //--
  FETCH_SINGLE_OPERAND_WORD(wDST);
  SetZNW(wDST);  SetV(false);  SetC(false);
//STORE_RESULT_WORD(wDST);
  return nCycles + 4;
}

uint32_t CDCT11::TSTB (uint8_t bDSTmode, uint8_t bDSTreg)
{
  //++
  // TEST (BYTE)
  //   Updates N and Z according to DST
  //   Always clears C and V
  //   See notes in TSTW!
  //--
  FETCH_SINGLE_OPERAND_BYTE(bDST);
  SetZNB(bDST);  SetV(false);  SetC(false);
  //STORE_RESULT_BYTE(bDST);
  return nCycles + 4;
}

uint32_t CDCT11::RORW (uint8_t bDSTmode, uint8_t bDSTreg)
{
  //++
  // ROTATE RIGHT (WORD)
  //   DST <= (C << 15) | (DST >> 1)
  //   Updates N and Z
  //   Sets C to the LSB of DST before rotating
  //   Sets V C XOR N (after rotating!)
  //--
  FETCH_SINGLE_OPERAND_WORD(wDST);
  uint16_t wResult = (wDST >> 1) | (ISC() << 15);
  SetZNW(wResult);  SetC(ISODD(wDST));  SetV(ISN() ^ ISC());
  STORE_RESULT_WORD(wResult);
  return nCycles + (bDSTmode == 0) ? 4 : 5;
}

uint32_t CDCT11::RORB (uint8_t bDSTmode, uint8_t bDSTreg)
{
  //++
  // ROTATE RIGHT (BYTE)
  //   DST <= (C << 7) | (DST >> 1)
  //   Updates N and Z
  //   Sets C to the LSB of DST before rotating
  //   Sets V C XOR N (after rotating!)
  //--
  FETCH_SINGLE_OPERAND_BYTE(bDST);
  uint8_t bResult = (bDST >> 1) | (ISC() << 7);
  SetZNB(bResult);  SetC(ISODD(bDST));  SetV(ISN() ^ ISC());
  STORE_RESULT_BYTE(bResult);
  return nCycles + (bDSTmode == 0) ? 4 : 5;
}

uint32_t CDCT11::ROLW (uint8_t bDSTmode, uint8_t bDSTreg)
{
  //++
  // ROTATE LEFT (WORD)
  //   DST <= (DST << 1) | C
  //   Updates N and Z
  //   Sets C to the MSB of DST before rotating
  //   Sets V to C XOR N (after rotating!)
  //--
  FETCH_SINGLE_OPERAND_WORD(wDST);
  uint16_t wResult = MASK16(wDST << 1) | ISC();
  SetZNW(wResult);  SetC(ISNEGW(wDST));  SetV(ISN() ^ ISC());
  STORE_RESULT_WORD(wResult);
  return nCycles + (bDSTmode == 0) ? 4 : 5;
}

uint32_t CDCT11::ROLB (uint8_t bDSTmode, uint8_t bDSTreg)
{
  //++
  // ROTATE LEFT (BYTE)
  //   DST <= (DST << 1) | C
  //   Updates N and Z
  //   Sets C to the MSB of DST before rotating
  //   Sets V to C XOR N (after rotating!)
  //--
  FETCH_SINGLE_OPERAND_BYTE(bDST);
  uint8_t bResult = MASK8(bDST << 1) | ISC();
  SetZNB(bResult);  SetC(ISNEGB(bDST));  SetV(ISN() ^ ISC());
  STORE_RESULT_BYTE(bResult);
  return nCycles + (bDSTmode == 0) ? 4 : 5;
}

uint32_t CDCT11::ASRW (uint8_t bDSTmode, uint8_t bDSTreg)
{
  //++
  // ARITHMETIC SHIFT RIGHT (WORD)
  //   DST <= (DST >> 1) | (MSB(DST))
  //   Updates N and Z
  //   Sets C to the LSB of DST before shifting
  //   Sets V to C XOR N (after shifting!)
  //--
  FETCH_SINGLE_OPERAND_WORD(wDST);
  uint16_t wResult = (wDST >> 1) | (wDST & 0100000);
  SetZNW(wResult);  SetC(ISODD(wDST));  SetV(ISN() ^ ISC());
  STORE_RESULT_WORD(wResult);
  return nCycles + (bDSTmode == 0) ? 4 : 5;
}

uint32_t CDCT11::ASRB (uint8_t bDSTmode, uint8_t bDSTreg)
{
  //++
  // ARITHMETIC SHIFT RIGHT (BYTE)
  //   DST <= (DST >> 1) | (MSB(DST))
  //   Updates N and Z
  //   Sets C to the LSB of DST before shifting
  //   Sets V to C XOR N (after shifting!)
  //--
  FETCH_SINGLE_OPERAND_BYTE(bDST);
  uint8_t bResult = (bDST >> 1) | (bDST & 0200);
  SetZNB(bResult);  SetC(ISODD(bDST));  SetV(ISN() ^ ISC());
  STORE_RESULT_BYTE(bResult);
  return nCycles + (bDSTmode == 0) ? 4 : 5;
}

uint32_t CDCT11::ASLW (uint8_t bDSTmode, uint8_t bDSTreg)
{
  //++
  // ARITHMETIC SHIFT LEFT (WORD)
  //   DST <= (DST << 1) 
  //   Updates N and Z
  //   Sets C to the MSB of DST before shifting
  //   Sets V to C XOR N (after shifting!)
  //   Note that except for the LSB of the result, this is identical to ROLW()!
  //--
  FETCH_SINGLE_OPERAND_WORD(wDST);
  uint16_t wResult = MASK16(wDST << 1);
  SetZNW(wResult);  SetC(ISNEGW(wDST));  SetV(ISN() ^ ISC());
  STORE_RESULT_WORD(wResult);
  return nCycles + (bDSTmode == 0) ? 4 : 5;
}

uint32_t CDCT11::ASLB (uint8_t bDSTmode, uint8_t bDSTreg)
{
  //++
  // ARITHMETIC SHIFT LEFT (BYTE)
  //   DST <= (DST << 1) 
  //   Updates N and Z
  //   Sets C to the MSB of DST before shifting
  //   Sets V to C XOR N (after shifting!)
  //   Note that except for the LSB of the result, this is identical to ROLB()!
  //--
  FETCH_SINGLE_OPERAND_BYTE(bDST);
  uint8_t bResult = MASK8(bDST << 1);
  SetZNB(bResult);  SetC(ISNEGB(bDST));  SetV(ISN() ^ ISC());
  STORE_RESULT_BYTE(bResult);
  return nCycles + (bDSTmode == 0) ? 4 : 5;
}

uint32_t CDCT11::SXTW (uint8_t bDSTmode, uint8_t bDSTreg)
{
  //++
  // SIGN EXTEND (WORD)
  //   DST <= N ? 177777 : 0
  //   Updates Z and N (although N effectively won't change!)
  //   Always clears V and C is unchanged.
  //   Note that this doesn't necessarily work the way you might expect - the
  //     CURRENT value of DST has no effect.  The result is determined only
  //     by the N flag.  I assume that, as does CLRW(), this reads the source
  //     word from memory first even though it's not actually used.
  //--
  FETCH_SINGLE_OPERAND_WORD(wDST);
  wDST = ISN() ? 0177777 : 0;
  SetZ(wDST == 0);  SetV(false);
  STORE_RESULT_WORD(wDST);
  return nCycles + (bDSTmode == 0) ? 4 : 5;
}

uint32_t CDCT11::SWAB(uint8_t bDSTmode, uint8_t bDSTreg)
{
  //++
  // SWAB BYTES
  //   DST <= BYTE_SWAP(DST)
  //   N and Z are set according to the low order byte of the result.
  //   V and C are always cleared.
  //--
  FETCH_SINGLE_OPERAND_WORD(wDST);
  wDST = MKWORD(LOBYTE(wDST), HIBYTE(wDST));
  SetZNB(LOBYTE(wDST));  SetV(false);  SetC(false);
  STORE_RESULT_WORD(wDST);
  return nCycles + (bDSTmode == 0) ? 4 : 5;
}

uint32_t CDCT11::MTPS (uint8_t bDSTmode, uint8_t bDSTreg)
{
  //++
  // MOVE TO PSW (BYTE)
  //   PSW <= DST
  //   N, Z, V and C are all set or cleared according to DST!
  //   The processor priority level will also be changed.
  //   This instruction DOES NOT ALTER the T bit!!!
  //   Note that this treats the source operand as a byte address (which is
  //     important for address calculation side effects!).
  //--
  FETCH_SINGLE_OPERAND_BYTE(bDST);
  uint8_t bOldBits = PSW & (PSW_T);
  uint8_t bOldPrio = (PSW & PSW_PRIO) >> 5;
  PSW = (bDST & (PSW_N | PSW_Z | PSW_V | PSW_C | PSW_PRIO)) | bOldBits;
  uint8_t bNewPrio = (PSW & PSW_PRIO) >> 5;
  if (bOldPrio != bNewPrio)
    LOGF(DEBUG, "CPU priority changed from BR%d to BR%d", bOldPrio, bNewPrio);
  // TBA NYI TODO - RECALCULATE INTERRUPTS NOW THAT PRIO IS CHANGED?!?
  return nCycles + 8;
}

uint32_t CDCT11::MFPS (uint8_t bDSTmode, uint8_t bDSTreg)
{
  //++
  // MOVE FROM PSW (BYTE)
  //   DST <= PSW
  //   N and Z are set according to the value of the PSW.
  //   V is always cleared and C is not affected.
  //   Note that this treats the destination operand as a byte address!
  //   Like CLRB and SXT, I assume that on the T11 this reads the destination
  //     from memory even though the current value is never used.
  //   MFPS to a register destination works like MOVB and sign extends!
  //--
  FETCH_SINGLE_OPERAND_BYTE(bDST);
  bDST = MASK8(PSW);
  SetZNB(bDST);  SetV(false);
  if (bDSTmode == 0) {
    REG(bDSTreg) = SXT8(bDST);  nCycles += 1;
  } else {
    WRITEB(wDSTea, bDST);  nCycles += 2;
  }
  return nCycles + (bDSTmode == 0) ? 4 : 5;
}


////////////////////////////////////////////////////////////////////////////////
////////////////////// BRANCH, JUMP AND TRAP INSTRUCTIONS //////////////////////
////////////////////////////////////////////////////////////////////////////////

//++
//   This macro will execute a PDP11 branch instruction.  The offset is sign
// extended, multiplied by two, and added to the PC.  That gives a branch range
// of -128 words to +127 words from the current instruction.  Instructions must
// be word aligned anyway, so there's no point in allowing an odd number of bytes.
//--
#define BRANCH(o)   {PC = MASK16(PC + 2*SXT8(o));}


uint32_t CDCT11::DoBranch (bool fByte, uint8_t bOpcode, uint8_t bOffset)
{
  //++
  //   This method handles all 15 PDP11 branch instructions.  The opcodes for
  // these are divided into two groups of 7 and 8 instructions respectively,
  // and the differentiating factor is the MSB of the instruction.  That's
  // normally the bit that differentiates between byte and word instructions,
  // hence thhe "fByte" flag.  This is TRUE if the MSB of the opcode is 1.
  // 
  //   In case you're wondering the missing branch instruction (why there are
  // only 15 instead of 16) should be "BRANCH NEVER".  That could have been a
  // NOP, but the PDP11 designers found another NOP elsewhere and assigned the
  // "BRANCH NEVER" opcode to a different group of instructions.
  //
  //   And lastly, on the DCT11 all branch instructions take 4 microcycles,
  // regardless of whether the branch is taken or not.  That makes our life
  // easier...
  //--
  if (!fByte) {
    switch (bOpcode) {
      case 0: /* would be "BRANCH NEVER! */     assert(false);
      case 1: /* branch always */               BRANCH(bOffset);  break;  // BR
      case 2: if (!ISZ())                       BRANCH(bOffset);  break;  // BNE
      case 3: if ( ISZ())                       BRANCH(bOffset);  break;  // BEQ
      case 4: if (!(ISN() ^ ISV()))             BRANCH(bOffset);  break;  // BGE
      case 5: if ( (ISN() ^ ISV()))             BRANCH(bOffset);  break;  // BLT
      case 6: if (!(ISZ() | (ISN() ^ ISV())))   BRANCH(bOffset);  break;  // BGT
      case 7: if ( (ISZ() | (ISN() ^ ISV())))   BRANCH(bOffset);  break;  // BLE
    }
  } else {
    switch (bOpcode) {
      case 0: if (!ISN())                       BRANCH(bOffset);  break;  // BPL
      case 1: if ( ISN())                       BRANCH(bOffset);  break;  // BMI
      case 2: if (!(ISC() | ISZ()))             BRANCH(bOffset);  break;  // BHI
      case 3: if ( (ISC() | ISZ()))             BRANCH(bOffset);  break;  // BLOS
      case 4: if (!ISV())                       BRANCH(bOffset);  break;  // BVC
      case 5: if ( ISV())                       BRANCH(bOffset);  break;  // BVS
      case 6: if (!ISC())                       BRANCH(bOffset);  break;  // BCC
      case 7: if ( ISC())                       BRANCH(bOffset);  break;  // BCS
    }
  }

  //   If the current PC is the same as the original PC of this instruction,
  // AND if interrupts are disabled, then this is a "branch to self" and
  // we're in an endless loop.  Note that interrupts can never really be
  // totally disabled on the DCT11, since there are always the POWERFAIL and
  // HALT interrupts.
  if (((PSW & PSW_PRIO) == PSW_PRI7) && (PC == m_nLastPC))
    m_nStopCode = STOP_ENDLESS_LOOP;

  // All done!
  return 4;
}

uint32_t CDCT11::SOB (uint8_t bReg, uint8_t bOffset)
{
  //++
  // SUBTRACT ONE AND BRANCH (if not zero!) ...
  //   REG <= REG - 1
  //   if the result != 0, then PC <= PC - 2*OFFSET
  //   Condition codes are NOT affected
  //   Note that SOB can only branch BACKWARDS!  Also note that the time for
  //     SOB is independent of whether the branch is taken or not.
  //--
  REG(bReg) = MASK16(REG(bReg) - 1);
  if (REG(bReg) != 0)
    PC = MASK16(PC - bOffset - bOffset);
  return 6;
}

uint32_t CDCT11::JMP (uint8_t bMode, uint8_t bReg)
{
  //++
  // JUMP
  //   PC <= EA (condition codes unchanged)
  //   Note that mode 0 (jump to a register!) traps to 4.
  //--
  uint16_t wEA;  uint32_t nCycles = 0;
  if (bMode == 0)
    return InstructionTrap(VEC_RESERVED);
  else {
    nCycles += CALCEAW(bMode, bReg, wEA);
    PC = wEA;
  }
  return nCycles+3;
}


//++
//   Push things, usually the PC or PSW, on the stack.  The DCT11 has no stack
// overflow checking, so there's no way this can "fail".  Note that we define
// both word and byte stack operations, but that's a bit misleading.  The PDP11
// stack ALWAYS contains 16 bit words, and PUSHB/POPB do just the same.
//--
#define PUSHW(x)   {DEC16(SP, 2);  WRITEW(SP, (x));}
#define PUSHB(x)   {PUSHW(MKWORD(0, MASK8(x)));}
#define POPW(x)    {x = READW(SP);  INC16(SP, 2);}
#define POPB(x)    {uint16_t t;  POPW(t);  (x) = MASK8(t);}


uint32_t CDCT11::JSR (uint8_t bReg, uint8_t bDSTmode, uint8_t bDSTreg)
{
  //++
  // JUMP TO SUBROUTINE
  //   -(SP) <= REG;  REG <= PC;  PC <= EA;
  //   As with JMP, destination mode 0 causes a trap to 4...
  //--
  uint16_t wEA;  uint32_t nCycles = 0;
  if (bDSTmode == 0)
    return InstructionTrap(VEC_RESERVED);
  else {
    nCycles += CALCEAW(bDSTmode, bDSTreg, wEA);
    PUSHW(REG(bReg));  REG(bReg) = PC;  PC = wEA;
  }
  return nCycles+7;
}

uint32_t CDCT11::RTS (uint8_t bReg)
{
  //++
  // RETURN FROM SUBROUTINE
  //   PC <= REG;  REG <= (SP)+
  //   Condition codes are not changed (of course).
  //--
  PC = REG(bReg);  POPW(REG(bReg));
  return 7;
}

uint32_t CDCT11::RTI (bool fInhibit)
{
  //++
  // RETURN FROM INTERRUPT
  //   PC <= -(SP);  PSW <= -(SP);
  //
  //   If fInhibit is TRUE, then T bit traps are inhibited for one instruction
  // after we return.  This is the behavior for the RTT instruction.  Otherwise
  // RTT and RTI are the same.
  // 
  //   Also note that RTI/RTT _can_ load the T bit in the PSW.  They're the only
  // instructions that can, and they also have a couple of special cases that we
  // need to deal with.  If RTI sets the trace bit, then trap happens at the end
  // of the RTI instruction, BEFORE the next instruction is fetched.  If RTI
  // clears the trace bit, then it will still be traced (because the REQ_TRACE
  // bit is still set in m_bRequests!), but RTT will NOT be traced under the
  // same circumstances.
  //--
  POPW(PC);  POPB(PSW);
  if (fInhibit)
    CLRBIT(m_bRequests, REQ_TRACE);
  else if (ISSET(PSW, PSW_T))
    BreakpointRequest();
  return fInhibit ? 11 : 8;
}

uint32_t CDCT11::TrapNow (uint16_t wNewPC, uint16_t wNewPSW)
{
  //++
  //   This method will simulate any PDP11 trap or interrupt.  The DCT11 action is
  // the same in all cases - push the current PC and PSW, and then load a new PC
  // and PSW.  This version allows you to specify the new PC and PSW directly; it's
  // used by HALT and others unique to the DCT11.  The single parameter version
  // below allows you to specify a traditional vector instead.
  // 
  //   Note that, if the new PSW we loaded does NOT have the T bit set, then we
  // don't trap at the end of this instruction!!
  //--
  PUSHB(PSW);  PUSHW(PC);  PC = wNewPC;  PSW = MASK8(wNewPSW);
  LOGF(DEBUG, "TrapNow() new PC=%06o, new prio=BR%d", wNewPC, (wNewPSW & PSW_PRIO) >> 5);
  if (!ISSET(PSW, PSW_T)) CLRBIT(m_bRequests, REQ_TRACE);
  return 16;
}

uint32_t CDCT11::TrapNow (uint16_t wVector)
{
  //++
  // Trap or interrupt thru the vector specified ...
  //--
  return TrapNow (READW(wVector), READW(wVector+2));
}

uint32_t CDCT11::InstructionTrap (address_t wVector)
{
  //++
  //   This method is called to handle any of the "instruction" traps - illegal,
  // reserved, EMT, TRAP, etc.  They're all the same except for the vector used.
  // 
  //   The only exception is breakpoints and T bit traps - for those you should
  // call BreakpointRequest() instead.
  //--
  SETBIT(m_bRequests, REQ_INSTRUCTION);
  m_wItrapVector = wVector;
  return 6;
}


////////////////////////////////////////////////////////////////////////////////
///////////////////////// MISCELLLANEOUS INSTRUCTIONS //////////////////////////
////////////////////////////////////////////////////////////////////////////////

uint32_t CDCT11::SETCLRCC (uint16_t wOpcode)
{
  //++
  //  SET C, N, Z, V and CLEAR C, N, Z or V and of course NOP ...
  //--
  bool fSet = ISSET(wOpcode, 020);
  if (ISSET(wOpcode, 010)) SetN(fSet);
  if (ISSET(wOpcode, 004)) SetZ(fSet);
  if (ISSET(wOpcode, 002)) SetV(fSet);
  if (ISSET(wOpcode, 001)) SetC(fSet);
  return 6;
}

uint32_t CDCT11::HALT()
{
  //++
  //   This executes the HALT instruction.  The DCT11 is unique in that this
  // doesn't halt at all, but instead traps to the restart address +2.
  //--
  LOGF(WARNING, "halt at %06o", PC);
  HaltRequest();  return 14;
}

uint32_t CDCT11::WAIT()
{
  //++
  //   The PDP11 WAIT instruction idles the CPU until an interrupt request
  // arrives.  This isn't the real world though and we don't have to actually
  // wait for anything - we can just advance the simulated time to the time
  // of the next scheduled event.  That event may or may not assert an
  // interrupt request, but regardless nothing else can happen until then.
  // If the next event doesn't generate an interrupt, then we just repeat
  // until we get to an event that does.
  //
  //   Note that any interrupt request has to be at a greater priority than
  // the current PSW.  Lower priority requests don't do anything.  That also
  // means that if the current PSW priority is 7 then we'll be stuck forever.
  // 
  //   Well, almost.  On the DCT11 there are two things that are even higher
  // priority than 7 - POWER FAIL and HALT.  The SBCT11 and this simuator don't
  // really use POWER FAIL, but a HALT is possible BUT it requires that the
  // operator manually flip the RUN/HALT switch.  In this simulator you can
  // do that with the HALT command.  Anyway, that's why we also need to check
  // m_bRequests here - to see if a HALT or POWERFAIL is pending.
  // 
  //   And there is yet one more "gotcha" here - if the user types the break
  // character (break emulation - usually ^E) on the console while we're
  // waiting, then there'll be no interrupt request ('cause it's not really
  // input to the simulation) but we need to stop anyway.  I think that's the
  // only way m_nStopCode can get set while we're spinning here.
  //
  //   One last thing - note that the time required to execute this instruction
  // is zero.  Or at least that's what we return here, but remember that we're
  // also advancing the simulated time.  In reality this instruction takes as
  // much time as it needs for the next interrupt to occur.
  //--
  if ((PSW & PSW_PRIO) == PSW_PRI7)
    LOGF(WARNING, "WAIT at priority 7 - HALT is your only way out!");
  while (true) {
    m_pEvents->JumpAhead(m_pEvents->NextEvent());
    DoEvents();
    if (   (GetPIC()->FindRequest(PSW) != 0)
        || (m_bRequests != 0)
        || (m_nStopCode != STOP_NONE)) break;
  }
  return 0;
}

uint32_t CDCT11::RESET()
{
  //++
  //   The RESET instruction asserts BCLR ("bus clear") output on the DCT11.
  // This is supposed to reset all I/O devices in the system, however it has
  // NO EFFECT on the internal state of the CPU.  None of the CPU registers
  // are changed, nor is the PSW.
  // 
  //   Note that calling CCPU::ClearAllDevices() here ONLY clears the event
  // queue and the interrupt system.  It only works on I/O mapped devices, and
  // of course the PDP-11 has none of those.  We have to call the ClearDevices()
  // method in the memory controller to clear I/O devices here.  And be very
  // careful of the order - we want to clear the devices AFTER we clear the 
  // event queue and interrupts.  That's because some devices may need to start
  // out with an initial event scheduled on an interrupt requested!
  //--
  LOGF(DEBUG, "RESET at PC %06o", PC);
  ClearAllDevices();
  GetMemory()->ClearDevices();
  return 37; // Yes, 37 clocks!
}


////////////////////////////////////////////////////////////////////////////////
//////////////////////////// INSTRUCTION DECODING //////////////////////////////
////////////////////////////////////////////////////////////////////////////////

//++
// These macros extract various special parts of a PDP11 opcode ...
//--
#define SOURCE(op)      (((op) >> 6) & 077)
#define DESTINATION(op) (((op)     ) & 077)
#define SRCMODE(op)     (((op) >> 9) & 7)
#define DSTMODE(op)     (((op) >> 3) & 7)
#define SRCREG(op)      (((op) >> 6) & 7)
#define DSTREG(op)      (((op)     ) & 7)

uint32_t CDCT11::DoSingleOperand (bool fByte, uint8_t bOpcode, uint8_t bDSTmode, uint8_t bDSTreg)
{
  //++
  //   This method is called for all 00xxDD and 10xxDD instructions, where xx
  // is 050 thru 077.  On the PDP11 these are mostly the single operand
  // instructions like CLR(B), NEG(B), TST(B), etc, but with a few wildcards
  // thrown in like MTPS and MFPS.  And a large number of these opcodes are
  // not implemented on the DCT11.
  //--
  switch (bOpcode) {
    case 050:  if (fByte) return CLRB(bDSTmode, bDSTreg);  else return CLRW(bDSTmode, bDSTreg); // CLR(B)
    case 051:  if (fByte) return COMB(bDSTmode, bDSTreg);  else return COMW(bDSTmode, bDSTreg); // COM(B)
    case 052:  if (fByte) return INCB(bDSTmode, bDSTreg);  else return INCW(bDSTmode, bDSTreg); // INC(B)
    case 053:  if (fByte) return DECB(bDSTmode, bDSTreg);  else return DECW(bDSTmode, bDSTreg); // DEC(B)
    case 054:  if (fByte) return NEGB(bDSTmode, bDSTreg);  else return NEGW(bDSTmode, bDSTreg); // NEG(B)
    case 055:  if (fByte) return ADCB(bDSTmode, bDSTreg);  else return ADCW(bDSTmode, bDSTreg); // ADC(B)
    case 056:  if (fByte) return SBCB(bDSTmode, bDSTreg);  else return SBCW(bDSTmode, bDSTreg); // SBC(B)
    case 057:  if (fByte) return TSTB(bDSTmode, bDSTreg);  else return TSTW(bDSTmode, bDSTreg); // TST(B)
    case 060:  if (fByte) return RORB(bDSTmode, bDSTreg);  else return RORW(bDSTmode, bDSTreg); // ROR(B)
    case 061:  if (fByte) return ROLB(bDSTmode, bDSTreg);  else return ROLW(bDSTmode, bDSTreg); // ROL(B)
    case 062:  if (fByte) return ASRB(bDSTmode, bDSTreg);  else return ASRW(bDSTmode, bDSTreg); // ASR(B)
    case 063:  if (fByte) return ASLB(bDSTmode, bDSTreg);  else return ASLW(bDSTmode, bDSTreg); // ASL(B)
    case 064:  if (fByte) return MTPS(bDSTmode, bDSTreg);  else /* ILLEGAL! */ break;           // MTPS
    case 067:  if (fByte) return MFPS(bDSTmode, bDSTreg);  else return SXTW(bDSTmode, bDSTreg); // MFPS/SXT
  }
  return InstructionTrap(VEC_ILLEGAL);
}

uint32_t CDCT11::DoOpcode00 (uint16_t wOpcode)
{
  //++
  //   This method executes all opcodes 000000..000377. This is a
  // random assortment of things the PDP11 designers couldn't fit
  // in anywhere else.  And on the T11, a lot of these are not
  // implemented...
  //--
  assert(wOpcode < 0400);
  if (wOpcode < 0100) {
    switch (wOpcode) {
    case 00:  return HALT();                            // HALT
    case 01:  return WAIT();                            // WAIT
    case 02:  return RTI();                             // RTI
    case 03:  return BreakpointRequest();               // BPT
    case 04:  return InstructionTrap(VEC_IOT);          // IOT
    case 05:  return RESET();                           // RESET
    case 06:  return RTT();                             // RTT
    case 07:  REG(0) = PROCESSOR_TYPE;  return 5;       // MFPT
    default:  return InstructionTrap(VEC_ILLEGAL);      // illegal
    }
  } else if (wOpcode < 0200)
    return JMP(DSTMODE(wOpcode), DSTREG(wOpcode));
  else if (wOpcode < 0210)
    return RTS(DSTREG(wOpcode));
  else if (wOpcode < 0240)
    return InstructionTrap(VEC_ILLEGAL);
  else if (wOpcode < 0300)
    return SETCLRCC(wOpcode);
  else
    return SWAB(DSTMODE(wOpcode), DSTREG(wOpcode));
}

uint32_t CDCT11::DoRequests (CPIC11::IRQ_t nIRQ)
{
  //++
  //   This method is called at the end of every instruction when m_bRequests
  // is not zero.  This means that we have at least one, possibly several,
  // pending requests for HALT trap, breakpoint/trace, external interrupt, or
  // whatever.  This routine needs to figure out which and take the appropriate
  // traps.
  // 
  //   Note "traps" because more than one request may be pending.  If there are
  // several then they're all serviced, in reverse priority order (i.e. most
  // important one last).  The PC and PSW for lower priority requests will end
  // up on the stack but won't actually be executed until the highest priority
  // ISR returns.
  // 
  //   There's more discussion of this under "TRAPS AND INTERRUPTS" at the
  // top of this file, and also in the interrupt logic emulation module.
  //--
  uint32_t nCycles = 0;
  if (ISSET(m_bRequests, REQ_INSTRUCTION)) 
    nCycles += TrapNow(m_wItrapVector);
  if (ISSET(m_bRequests, REQ_EXTERNAL)) {
    //   Handle external interrupts.  Note that there's no need to check the
    // priority level here, since that was already done by FindRequest()..
    assert(nIRQ > 0);
    address_t wVector = GetPIC()->GetVector(nIRQ);
    nCycles += TrapNow(wVector);
    GetPIC()->AcknowledgeRequest(nIRQ);
    LOGF(DEBUG, "external interrupt CP%d, vector=%03o", nIRQ, wVector);
  }
  if (ISSET(m_bRequests, REQ_POWERFAIL)) {
    LOGF(DEBUG, "POWERFAIL trap");
    nCycles += TrapNow(VEC_POWERFAIL);
  }
  if (ISSET(m_bRequests, REQ_TRACE))
    nCycles += TrapNow(VEC_BPT);
  if (ISSET(m_bRequests, REQ_HALT)) {
    address_t wVector = GetRestartAddress();
    LOGF(DEBUG, "HALT restart trap to %06o", wVector);
    nCycles += TrapNow(wVector, PSW_PRI7);
  }
  m_bRequests = 0;
  return nCycles;
}

uint32_t CDCT11::DoExecute (uint16_t wIR)
{
  //++
  // Execute the instruction and return the number of cycles used ...
  //--

  // Start off by decoding just the upper 4 bits of the instruction ...
  uint8_t bOpcode = (wIR >> 12) & 017;
  switch (bOpcode) {
    // All double operand instructions ...
    case 001:  return MOVW(SRCMODE(wIR), SRCREG(wIR), DSTMODE(wIR), DSTREG(wIR));
    case 011:  return MOVB(SRCMODE(wIR), SRCREG(wIR), DSTMODE(wIR), DSTREG(wIR));
    case 002:  return CMPW(SRCMODE(wIR), SRCREG(wIR), DSTMODE(wIR), DSTREG(wIR));
    case 012:  return CMPB(SRCMODE(wIR), SRCREG(wIR), DSTMODE(wIR), DSTREG(wIR));
    case 003:  return BITW(SRCMODE(wIR), SRCREG(wIR), DSTMODE(wIR), DSTREG(wIR));
    case 013:  return BITB(SRCMODE(wIR), SRCREG(wIR), DSTMODE(wIR), DSTREG(wIR));
    case 004:  return BICW(SRCMODE(wIR), SRCREG(wIR), DSTMODE(wIR), DSTREG(wIR));
    case 014:  return BICB(SRCMODE(wIR), SRCREG(wIR), DSTMODE(wIR), DSTREG(wIR));
    case 005:  return BISW(SRCMODE(wIR), SRCREG(wIR), DSTMODE(wIR), DSTREG(wIR));
    case 015:  return BISB(SRCMODE(wIR), SRCREG(wIR), DSTMODE(wIR), DSTREG(wIR));
    case 006:  return ADDW(SRCMODE(wIR), SRCREG(wIR), DSTMODE(wIR), DSTREG(wIR));
    case 016:  return SUBW(SRCMODE(wIR), SRCREG(wIR), DSTMODE(wIR), DSTREG(wIR));

    // Single operand instructions, branch, trap, HALT, WAIT, etc ...
    case 000:
      if (wIR < 000400)
        // Opcodes 000000 .. 000377 ...
        return DoOpcode00(wIR);
      else if (wIR < 004000)
        // Opcodes 000400 .. 003777 ...
        return DoBranch(false, HIBYTE(wIR) & 7, LOBYTE(wIR));
      else if (wIR < 005000)
        // Opcodes 004000 .. 004777 ...
        return JSR(SRCREG(wIR), DSTMODE(wIR), DSTREG(wIR));
      else
        // Opcodes 005000 .. 007777 ...
        return DoSingleOperand(false, ((wIR>>6) & 077), DSTMODE(wIR), DSTREG(wIR));

    // ...
    case 010:
      if (wIR < 0104000)
        // Opcodes 100000 .. 103777 ...
        return DoBranch(true, HIBYTE(wIR) & 7, LOBYTE(wIR));
      else if (wIR < 0104400)
        // Opcodes 104000 .. 104377 ...
        return InstructionTrap(VEC_EMT);
      else if (wIR < 0105000)
        // Opcodes 104400 .. 104777 ...
        return InstructionTrap(VEC_TRAP);
      else
        // Opcodes 105000 .. 107777 ...
        return DoSingleOperand(true, ((wIR>>6) & 077), DSTMODE(wIR), DSTREG(wIR));

    //   The only two 07xxxx opcodes implemented on the T11 are SOB and XOR.
    // The others are mostly EIS/FIS instructions that are unimplemented.
    case 007:
      if (SRCMODE(wIR) == 4)
        return XORW(SRCREG(wIR), DSTMODE(wIR), DSTREG(wIR));
      else if (SRCMODE(wIR) == 7)
        return SOB(SRCREG(wIR), DESTINATION(wIR));
      else
        return InstructionTrap(VEC_ILLEGAL);

    // 17xxxx opcodes are all FPP instructions unimplemented on the T11 ...
    case 017:
      return InstructionTrap(VEC_ILLEGAL);
  }
  return 0;
}

CCPU::STOP_CODE CDCT11::Run (uint32_t nCount)
{
  //++
  //   This is the main "engine" of the DCT11 emulator.  The UI code is  
  // expected to call it whenever the user gives a START, GO, STEP, etc 
  // command and it will execute DCT11 instructions until it either a)  
  // executes the number of instructions specified by nCount, or b) some        
  // condition arises to interrupt the simulation such as a HLT opcode, 
  // an illegal opcode or I/O, the user entering the escape sequence on 
  // the console, etc.  If nCount is zero on entry, then we will run for-       
  // ever until one of the previously mentioned break conditions arises.        
  //--
  bool fFirst = true;
  m_nStopCode = STOP_NONE;

  while (m_nStopCode == STOP_NONE) {
    // If any device events need to happen, now is the time...
    DoEvents();

    //   If the T bit is set in the PSW, then set a trace trap request.
    // This will be handled at the END of this instruction!
    if (ISPSW(PSW_T)) BreakpointRequest();

    // Stop after we hit a breakpoint ...
    if (!fFirst && m_pMemory->IsBreak(GetPC())) {
      m_nStopCode = STOP_BREAKPOINT;  break;
    }
    fFirst = false;

    // Trace instruction execution ...
    //string sCode;
    //Disassemble(m_pMemory, PC, sCode);

    // Fetch, decode and execute an instruction...
    m_nLastPC = PC;
    uint16_t wIR = FETCHW();
    uint32_t nCycles = DoExecute(wIR);
    AddCycles(nCycles);

    //LOGF(TRACE, "%06o/ %-35s SP/%06o PSW/%03o", PC, sCode.c_str(), SP, PSW);

    // Look for an external interrupt .GT. PSW priority ...
    CPIC11::IRQ_t nIRQ = GetPIC()->FindRequest(PSW);
    if (nIRQ > 0) SETBIT(m_bRequests, REQ_EXTERNAL);

    // If there are any trap or interrupt requests pending, do them now ...
    if (m_bRequests != 0) {
      nCycles = DoRequests(nIRQ);
      AddCycles(nCycles);
    }

    // Terminate if we've executed enough instructions ... 
    if (m_nStopCode == STOP_NONE) {
      if ((nCount > 0)  &&  (--nCount == 0))  m_nStopCode = STOP_FINISHED;
    }
  }

  // Return the reason we stopped ...
  return m_nStopCode;
}

