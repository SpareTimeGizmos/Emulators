//++
//COSMAC.cpp - RCA CDP1802 COSMAC microprocessor emulation
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
//   This module implements a simulation of the COSMAC CDP1802 CPU, and maybe
// someday the 1804/5/6 extended version.  that's roughly similar to the CDP1802
// chip.  
//
// REVISION HISTORY:
// 14-AUG-19  RLA  New file.
// 17-JAN-20  RLA  Fix the long skip/NOP opcodes.
// 21-JAN-20  RLA  Fix bug in DF calculation for subtract instructions
//                 Initialize all EF flags to 1, not 0!
// 22-JAN-20  RLA  Add software serial support.
// 21-JUN-22  RLA  Invert the sense of the EF inputs for GetSense()
//                 Add the nSense/nFlag parameters to GetSense() and SetFlag()
// 27-JUL-22  RLA  Add extended 1804/5/6 instruction set
//                 Add 1804/5/6 internal counter/timer
//                 Implement the IDL (wait for interrupt) instruction
// 22-Aug-22  RLA  We don't need or want to implement MasterClear().
//                 We want to implement ClearCPU() instead!
// 20-DEC-23  RLA  Add default parameter to GetSense() for TLIO
// 17-JUL-24  RLA  LDC is wrong - should set m_CNTR = m_D if stopped
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include <cstring>              // needed for memset()
#include "EMULIB.hpp"           // emulator library definitions
#include "LogFile.hpp"          // emulator library message logging facility
#include "CommandParser.hpp"    // needed for type KEYWORD
#include "Interrupt.hpp"        // interrupt simulation logic
#include "EventQueue.hpp"       // I/O device event queue simulation
#include "MemoryTypes.h"        // address_t and word_t data types
#include "COSMACopcodes.hpp"    // COSMAC opcode definitions
#include "CPU.hpp"              // CCPU base class definitions
#include "Memory.hpp"           // CMemory memory emulation object
#include "Device.hpp"           // CDevice I/O device emulation objects
#include "COSMAC.hpp"           // declarations for this module

// Internal CPU register names for GetRegisterNames() ...
const CCmdArgKeyword::KEYWORD CCOSMAC::g_keysRegisters[] {
  {"R0",   CCOSMAC::REG_R0},   {"R1",   CCOSMAC::REG_R1},   {"R2",   CCOSMAC::REG_R2},
  {"R3",   CCOSMAC::REG_R3},   {"R4",   CCOSMAC::REG_R4},   {"R5",   CCOSMAC::REG_R5},
  {"R6",   CCOSMAC::REG_R6},   {"R7",   CCOSMAC::REG_R7},   {"R8",   CCOSMAC::REG_R8},
  {"R9",   CCOSMAC::REG_R9},   {"RA",   CCOSMAC::REG_RA},   {"RB",   CCOSMAC::REG_RB},
  {"RC",   CCOSMAC::REG_RC},   {"RD",   CCOSMAC::REG_RD},   {"RE",   CCOSMAC::REG_RE},
  {"RF",   CCOSMAC::REG_RF},   {"D",    CCOSMAC::REG_D},    {"DF",   CCOSMAC::REG_DF},
  {"P",    CCOSMAC::REG_P},    {"X",    CCOSMAC::REG_X},    {"I",    CCOSMAC::REG_I},
  {"N",    CCOSMAC::REG_N},    {"T",    CCOSMAC::REG_T},  /*{"B",    CCOSMAC::REG_B},*/
  {"IE",   CCOSMAC::REG_IE},   {"Q",    CCOSMAC::REG_Q},    {"EF1",  CCOSMAC::REG_EF1},
  {"EF2",  CCOSMAC::REG_EF2},  {"EF3",  CCOSMAC::REG_EF3},  {"EF4",  CCOSMAC::REG_EF4},
  {"XIE",  CCOSMAC::REG_XIE},  {"CIE",  CCOSMAC::REG_CIE},  {"CIL",  CCOSMAC::REG_CIL},
  {"CNTR", CCOSMAC::REG_CNTR}, {"CH",   CCOSMAC::REG_CH},   {"ETQ",  CCOSMAC::REG_ETQ},
  {NULL, 0}
};

// Names for the sense inputs and flag outputs ...
const char *CCOSMAC::g_apszSenseNames[MAXSENSE] = {
  "EF1", "EF2", "EF3", "EF4"
};


CCOSMAC::CCOSMAC (CMemory *pMemory, CEventQueue *pEvents, CInterrupt *pInterrupt)
  : CCPU(pMemory, pEvents, pInterrupt)
{
  //++
  //--
  m_fExtended = false;
  for (uint16_t i = 0;  i < MAXSENSE;  ++i)  SetDefaultEF(i, 0);
  SetCrystalFrequency(DEFAULT_CLOCK);
  CCOSMAC::ClearCPU();
}

void CCOSMAC::ClearCPU()
{
  //++
  //   This routine resets the COSMAC to a power on state.  Actually it does
  // more than that - a real COSMAC only resets X, P, Q and R[0] and sets IE.
  // The other registers are unchanged.  This implementation clears everything.
  //
  //   Note that a reset/clear ENABLES interrupts.  That's right, interrupts
  // are enabled after a RESET!  Don't blame me - I didn't design it...
  //--
  CCPU::ClearCPU();
  m_I = m_N = m_P = m_X = m_T = m_D = m_B = 0;
  m_DF = m_Q = m_XIR = m_CIR = m_ETQ = m_LastEF = 0;
  m_XIE = m_CIE = m_MIE = 1;
  m_CNTR = m_CH = m_Prescaler = 0;
  memset(m_R, 0, sizeof(m_R));
  memset(m_EF, 1, sizeof(m_EF));
  StopCounter();
  UpdateQ(m_Q);
}

void CCOSMAC::DoALU()
{
  //++
  //   This method executes the ALU operation specified by the I and N registers
  // and using the values from the D, DF and B registers.  The result is placed
  // in D and DF.
  //
  //   Some of the code here may seem a bit obscure or overly verbose, but
  // that's because it's stolen (more or less verbatim) from my Verilog COSMAC
  // implementation.  This code is as close as possible to the Verilog so that
  // we can test the logic ...
  //--
  switch (MKBYTE(m_I, m_N)) {
    // OR/ORI, AND/ANI, and XOR/XRI ...
    case OP_OR:  case OP_ORI:  m_D = m_D | m_B;  break;
    case OP_AND: case OP_ANI:  m_D = m_D & m_B;  break;
    case OP_XOR: case OP_XRI:  m_D = m_D ^ m_B;  break;

    //  There are a total of six (twelve if you count the immediate variations
    // as well) arithmetic instructions.  These are -
    //
    //    ADD($F4) and ADI ($FC) -> {DF, D} = D + B
    //	  ADC($74) and ADCI($7C) -> {DF, D} = D + B + DF
    //	  SD ($F5) and SDI ($FD) -> {DF, D} = B - D
    //	  SDB($75) and SDBI($7D) -> {DF, D} = B - D - (~DF)
    //	  SM ($F7) and SMI ($FF) -> {DF, D} = D - B
    //	  SMB($77) and SMBI($7F) -> {DF, D} = D - B - (~DF)
    //
    //   We could just write these out as six separate cases (and believe me,
    // it's tempting!) but in Verilog that would result in the creation of six
    // separate eight bit adders.  That's pretty wasteful, even by my standards.
    // We can improve things by remembering the rules for two's complement and
    // rewriting the subtractions as additions - 
    //
    //	  SD ($F5) and SDI ($FD) -> {DF, D} = B + (~D) + 1
    //	  SDB($75) and SDBI($7D) -> {DF, D} = B + (~D) + 1 - (~DF)
    //	  SM ($F7) and SMI ($FF) -> {DF, D} = D + (~B) + 1
    //	  SMB($77) and SMBI($7F) -> {DF, D} = D + (~B) + 1 - (~DF)
    //
    // Notice that (1 - (~DF)) is 1 if DF=1 and 0 if DF=0, so -
    //
    //	  SDB($75) and SDBI($7D) -> {DF, D} = B + (~D) + DF
    //	  SMB($77) and SMBI($7F) -> {DF, D} = D + (~B) + DF
    //
    // That gives us these six operations -
    //
    //	  ADD($F4) and ADI ($FC) -> {DF, D} =   D  +   B
    //	  ADC($74) and ADCI($7C) -> {DF, D} =   D  +   B  + DF
    //	  SD ($F5) and SDI ($FD) -> {DF, D} = (~D) +   B  + 1
    //	  SDB($75) and SDBI($7D) -> {DF, D} = (~D) +   B  + DF
    //	  SM ($F7) and SMI ($FF) -> {DF, D} =   D  + (~B) + 1
    //	  SMB($77) and SMBI($7F) -> {DF, D} =   D  + (~B) + DF
    //
    // And that's what we'll actually implement now ...
    default:
      // ADD, ADC , SD , SDB , SM , SMB
      // ADI, ADCI, SDI, SDBI, SMI, SMBI
      uint1_t cin =   (m_I      == 0x7)  ?  m_DF  :  ((m_N&0x7) == 0x4) ? 0 : 1;
      uint16_t t  = (((m_N&0x7) == 0x5)  ?  MASK8(~m_D)  :  m_D)
                  + (((m_N&0x7) == 0x7)  ?  MASK8(~m_B)  :  m_B)
                  + cin;
      uint1_t cout = MASK1(t >> 8);
      m_DF = /*((m_N&0x7) == 0x4)  ?*/  cout /* :  (~cout & 1)*/;
      m_D  = MASK8(t);
      break;
  } // switch {I,N} ...
}

void CCOSMAC::DoDecimal()
{
  //++
  //   And this method executes the 1804/5/6 decimal ALU instructions.  In this
  // case there are only eight -
  // 
  //    DADD ($68F4) and DADI ($68FC) -> {DF, D} = D +   B
  //    DADC ($6874) and DACI ($687C) -> {DF, D} = D +   B  + DF
  //    DSM  ($68F7) and DSMI ($68FF) -> {DF, D} = D + (-B) 
  //    DSMB ($6877) and DSBI ($687F) -> {DF, D} = D + (-B) - (~DF)
  // 
  //   Note that in this case, "-B" refers to the ten's complement of B, not
  // the two's complement.  Also note that the 1804/5/6 has no decimal version
  // of SD, SDB, SDI, or SDB, which means that only B needs to be negated; it's
  // never D.
  // 
  //   Also, remember that "- (~DF)" is just DF, so "- (~DF)" is the same as
  // "+ DF".
  //--
  uint4_t d0, d1;

  // Set cin to DF for any of the "with carry/borrow" instructions ....
  uint1_t cin = ISSET(m_N, 0x80) ? 0 : m_DF;

  // Ten's complement B for any subtract instruction ...
  uint8_t b = m_B;
  if (ISSET(m_N, 1)) {
    d0 = 9 - LONIBBLE(m_B) + 1;  d1 = 9 - HINIBBLE(m_B);
    if (d0 > 9) {++d1;  d0 -= 10;}
    b = MKBYTE(d1, d0);
  }

  // And finally decimal add D + B ...
  m_DF = 0;
  d0 = LONIBBLE(m_D) + LONIBBLE(b) + cin;
  d1 = HINIBBLE(m_D) + HINIBBLE(b);
  if (d0 > 9) {++d1;  d0 -= 10;}
  if (d1 > 9) {m_DF = 1;  d1 -= 10;}
  m_D = MKBYTE(d1, d0);
}

void CCOSMAC::DoIdle()
{
  //++
  //   The IDL instruction idles the CPU until an interrupt request arrives.
  // This isn't the real world though and we don't have to actually wait for
  // anything - we can just advance the simulated time to the time of the
  // next scheduled event.  That event may or may not assert an interrupt
  // request, but if it doesn't then we just repeat until we get to an event
  // that does.
  //
  //   Note that interrupts must be enabled, both the master enable and at
  // least one of the XIE or CIE bits.  Plus, if CIE is enabled but XIE is not
  // then the counter/timer can't be stopped.  If these conditions aren't true
  // then we're guaranteed to be stuck here forever.  We can't really tell what
  // the external I/O devices are programmed to do, so we may end up stuck here
  // forever anyway, but this is the best we can do.
  // 
  //   The CEventQueue class has methods to just step ahead directly to the
  // next scheduled event, but we can't use those here because of the 1804/5/6
  // internal counter/timer.  If that puppy is running then we have to step
  // one cycle at a time just to be sure we get the correct timing.
  // 
  //   And there is yet one more "gotcha" here - if the user types the break
  // character (break emulation - usually ^E) on the console while we're
  // waiting, then there'll be no interrupt request ('cause it's not really
  // input to the simulation) but we need to stop anyway.  I think that's the
  // only way m_nStopCode can get set while we're spinning here.
  //--
  if (   (m_pInterrupt == NULL)
      || (m_MIE == 0)
      || ((m_XIE | m_CIE) == 0)
      || ((m_XIE == 0) && (m_CTmode == CT_STOPPED))) {
    LOGF(WARNING, "IDL with interrupts disabled!");
    m_nStopCode = STOP_BREAK;  return;
  }
  while (true) {
    AddCycles(1);  DoEvents();
    if ((m_XIE != 0) && m_pInterrupt->IsRequested()) break;
    if ((m_CIE & m_CIR) != 0) break;
    if (m_nStopCode != STOP_NONE) break;
  }
}

void CCOSMAC::UpdateQ (uint1_t bNew)
{
  //++
  // Update Q and handle any software serial emulation ...
  //--
  m_Q = MASK1(bNew);
  LOGF(TRACE, "CDP1802 set Q=%d at %04X", m_Q, GetPC());
  SetFlag(Q, m_Q);
}

uint1_t CCOSMAC::UpdateEF (uint16_t nSense)
{
  //++
  //   This routine is called whenever an EF flag is tested by a branch
  // instruction.  It will check to see if an installed device is connected
  // to this input and, if one is, query the device for the current state
  // of this flag.
  // 
  //   There's a problem with Mike's BIOS for the Elf2K and PEV2 when a UART
  // is also installed.  The Elf2K BIOS wants the unconnected serial EF input
  // to be a one when the UART is used, but the PEV2 BIOS wants a zero under
  // the same circumstances.  If you don't meet that requirement, then the
  // BIOS will refuse to use the UART even if it exists.
  // 
  //   Because of this we allow the default state for unconnected EF inputs
  // to be specified by a call to SetDefaultEF().  This value is effective
  // ONLY if that EF sense input is not connected to some device.  If it is
  // connected, then the default is ignored.
  //--
  assert(nSense < MAXSENSE);
  CDevice *pSense = GetSenseDevice(nSense);
  uint1_t bData = (pSense != NULL) ? pSense->GetSense(nSense, m_EFdefault[nSense]) : m_EFdefault[nSense];
  m_EF[nSense] = MASK1(bData);
  //LOGF(TRACE, "CDP1802 EF%d=%d at %04X", nSense+1, m_EF[nSense], GetPC());
  return m_EF[nSense];
}

void CCOSMAC::DecrementCounter()
{
  //++
  //   Decrement the counter/timer by exactly one count and, when the count
  // reaches zero, handle reloading, interrupts, and toggle Q.
  //--
  if (m_CNTR != 1) {
    m_CNTR = MASK8(m_CNTR-1);
  } else {
    m_CNTR = m_CH;  m_CIR = 1;
    if (m_ETQ != 0) UpdateQ(~m_Q);
  }
}

void CCOSMAC::UpdateCounter()
{
  //++
  //   This routine is called once every machine cycle, by the AddCycle()
  // routine, to update the counter/timer.  If we're not emulating the extended
  // instruction set, or if the counter/timer is currently stopped, then this
  // is trivial.  In timer mode the prescaller counts 32 calls to this routine,
  // simulating 32 TPA clocks, before decrementing the counter.  In the pulse
  // counter modes, we look for a change from true to false (a falling edge) on
  // the selected EFx input.  In pulse counter mode, we decrement the counter
  // every time we're here as long as the EFx input is asserted.
  //--
  uint1_t ef;
  if (!m_fExtended || (m_CTmode == CT_STOPPED)) return;
  switch (m_CTmode) {
    case CT_TIMER:
      if (++m_Prescaler >= TPAPRESCALE) {
        DecrementCounter();  m_Prescaler = 0;
      }
      break;
    case CT_EVENT1:
    case CT_EVENT2:
      ef = UpdateEF((m_CTmode == CT_EVENT1) ? 1 : 2);
      if ((m_LastEF != 0) && (ef == 0)) DecrementCounter();
      m_LastEF = ef;
      break;
    case CT_PULSE1:
    case CT_PULSE2:
      ef = UpdateEF((m_CTmode == CT_EVENT1) ? 1 : 2);
      if (ef == 1) DecrementCounter();
      m_LastEF = ef;
      break;
    case CT_STOPPED:
      break;
  }
}

void CCOSMAC::AddCycles (uint32_t lCycles)
{
  //++
  //   This routine is called to keep track of the simulated time, measured
  // in 1802 major cycles.  Remember that on the 1802 a major cycle takes
  // eight clock cycles.  On most processors this is just a matter of updating
  // the EventQueue so that we can fire off scheduled I/O events at the 
  // correct simulated time.
  // 
  //   The COSMAC needs that too, but for the 1804/5/6 we have the added task
  // of calling UpdateCounter() every cycle to update the internal counter
  // /timer.  If the counter/timer is stopped, or is in the timer mode, then
  // the operation doesn't depend on anything outside of the CPU and just
  // updating the count is enough.
  // 
  //   BUT, simulated time doesn't pass evenly - sometimes this routine might
  // be called for 1 cycle, and sometimes it might be 7 or 8 cycles at once.
  // If the counter/timer is in any of the modes that are controlled by EF1 or
  // EF2, then this could produce incorrect results since the simulated devices
  // controlling those EF inputs don't get a chance to update every cycle.
  // 
  //   So instead we step thru one cycle at a time, processesing events as
  // we go.  That guarantees that the EF inputs are potentially updated every
  // major cycle, and that should give the same result that you'd get with
  // real hardware.
  //--
  if (!m_fExtended || (m_CTmode == CT_STOPPED)) {
    // The counter/timer is not is use, so it's safe to skip updating it!
    AddTime(lCycles * CLOCKS_PER_CYCLE * HZTONS(m_lClockFrequency));
  } else {
    // We'll have to do things the hard way...
    while (lCycles-- > 0) {
      AddTime(CLOCKS_PER_CYCLE * HZTONS(m_lClockFrequency));
      DoEvents();  UpdateCounter();
    }
  }
}

void CCOSMAC::DoCounter()
{
  //++
  // Execute the 1804/5/6 counter/timer instructions, opcodes $68 $0x ...
  // 
  //   Note that these all take three machine cycles to execute, so no extra
  // AddCycle() calls are required here!
  //--
  switch (m_N) {
    case OP_STPC: StopCounter();                 break;
    case OP_SCM1: StartCounter(CT_EVENT1);       break;
    case OP_SCM2: StartCounter(CT_EVENT2);       break;
    case OP_SPM1: StartCounter(CT_PULSE1);       break;
    case OP_SPM2: StartCounter(CT_PULSE2);       break;
    case OP_STM:  StartCounter(CT_TIMER);        break;
    case OP_DTC:  DecrementCounter();            break;
    case OP_LDC:  m_CH = m_D;
                  if (m_CTmode == CT_STOPPED) {
                    m_CNTR = m_D;  m_CIR = 0;
                  }
                  break;
    case OP_GEC:  m_D = m_CNTR;                     break;
    case OP_ETQ:  m_ETQ = 1;                        break;
    case OP_XIE:  m_XIE = 1;                        break;
    case OP_XID:  m_XIE = 0;                        break;
    case OP_CIE:  m_CIE = 1;                        break;
    case OP_CID:  m_CIE = 0;                        break;
    default: IllegalOpcode();                       break;
  }
}

void CCOSMAC::Do7xFx()
{
  //++
  //   This routine emulates all the ALU and "miscellaneous" COSMAC instructions.
  // That's all the 0x7x or 0xFx opcodes, or all the ones that aren't a branch
  // and where N isn't a register.  About half of these are ALU functions, for
  // which we just fetch the operand and then pass off control to DoALU().  The
  // remainder are "miscellaneous" instructions like set/reset Q, save and
  // restore T, disable interrupts, etc.
  //--
  uint8_t b;  uint1_t t;

  switch (MKBYTE(m_I, m_N)) {
    // All the random, miscellaneous operations ...
    case OP_RET:                                  // 0x70 - RETURN
    case OP_DIS:                                  // 0x71 - DISABLE
      b = MemReadInc(m_X);  m_X = HINIBBLE(b);  m_P = LONIBBLE(b);  m_MIE = MASK1(~m_N);  break;
    case OP_LDXA: m_D = MemReadInc(m_X);  break;  // 0x72 - LOAD VIA X AND ADVANCE
    case OP_STXD: MemWriteDec(m_X, m_D);  break;  // 0x73 - STORE VIA X AND DECREMENT
    case OP_SAV:  MemWrite(m_X, m_T);     break;  // 0x78 - SAVE T
    case OP_MARK:                                 // 0x79 - PUSH X, P TO STACK
      b = MKBYTE(m_X, m_P);  MemWriteDec(2, b);  m_T = b;  m_X = m_P;  break;
    case OP_REQ:  UpdateQ(0);             break;  // 0x7A - RESET Q
    case OP_SEQ:  UpdateQ(1);             break;  // 0x7B - SET Q
    case OP_LDX:  m_D = MemRead(m_X);     break;  // 0xF0 - LOAD VIA X
    case OP_LDI:  m_D = MemReadInc(m_P);  break;  // 0xF8 - LOAD IMMEDIATE

    // All the shift and rotate opcodes ...
    case OP_SHR:                                  // 0xF6 - SHIFT RIGHT
      m_DF = MASK1(m_D);  m_D >>= 1;  break;
    case OP_SHL:                                  // 0xFE - SHIFT LEFT
      m_DF = (m_D & 0x80) >> 7;  m_D = (m_D << 1) & 0xFF;  break;
    case OP_SHRC:                                 // 0x76 - SHIFT RIGHT WITH CARRY
      t = MASK1(m_D);  m_D = (m_D>>1) | (m_DF<<7);  m_DF = t;  break;
    case OP_SHLC:                                 // 0x7E - SHIFT LEFT WITH CARRY
      t = MASK1(m_D>>7);  m_D = ((m_D<<1) | m_DF) & 0xFF;  m_DF = t;  break;

    //   All COSMAC ALU operations store the operand in temporary register B.
    // For the directly addressed opodes that operand is at M[R[X]], and for 
    // the immediate opcodes it's M[R[P]].  In the latter case R[P] must also
    // be incremented ...
    case OP_ADC:                                  // 0x74 - ADD WITH CARRY
    case OP_SDB:                                  // 0x75 - SUBTRACT D WITH BORROW
    case OP_SMB:                                  // 0x77 - SUBTRACT MEMORY WITH BORROW
    case OP_OR:                                   // 0xF1 - OR
    case OP_AND:                                  // 0xF2 - AND
    case OP_XOR:                                  // 0xF3 - EXCLUSIVE OR
    case OP_ADD:                                  // 0xF4 - ADD
    case OP_SD:                                   // 0xF5 - SUBTRACT D
    case OP_SM:                                   // 0xF7 - SUBTRACT MEMORY
      m_B = MemRead(m_X);  DoALU();  break;
    case OP_ADCI:                                 // 0x7C - ADD WITH CARRY, IMMEDIATE
    case OP_SDBI:                                 // 0x7D - SUBTRACT D WITH BORROW, IMMEDIATE
    case OP_SMBI:                                 // 0x7F - SUBTRACT MEMORY WITH BORROW, IMMEDIATE
    case OP_ORI:                                  // 0xF9 - OR IMMEDIATE
    case OP_XRI:	                          // 0xFB - EXCLUSIVE OR IMMEDIATE
    case OP_ANI:	                          // 0xFA - AND IMMEDIATE
    case OP_ADI:	                          // 0xFC - ADD IMMEDIATE
    case OP_SDI:	                          // 0xFD - SUBTRACT D IMMEDIATE
    case OP_SMI:	                          // 0xFF - SUBTRACT MEMORY IMMEDIATE
      m_B = MemReadInc(m_P);  DoALU();  break;

    // Any other opcode is seriously bad...
    default: assert(false);
  }
}

void CCOSMAC::Do7xFxExtended()
{
  //++
  //   This routine is called to simulate the extended $68 $7x and $68 $Fx
  // opcodes.  With the sole exception of DSAV, these are all decimal (BCD)
  // versions of the standard binary arithmetic instructions.
  // 
  //   Note that the opcodes for the decimal instructions, DADC, DADD, DSMB,
  // DSBI, etc all map exactly onto the standard "binary" opcodes for ADC,
  // ADD, SMB, SMBI, etc.  This means that can call the DoALU() method here
  // to do the same job.
  // 
  //   Also note that the extended opcode for DSAV is the same as the non-
  // extended opcode for SHRC, which makes implementing DSAV easier too.
  // No doubt these equivalences are NOT accidental ...
  //--
  switch (MKBYTE(m_I, m_N)) {
    // DSAV is the odd one out here ...
    case OP_DSAV: DecReg(m_X);  MemWriteDec(m_X, m_T);
                  MemWriteDec(m_X, m_D);
                  Do7xFx(); /* SHRC! */
                  MemWrite(m_X, m_D);
                  break;
    
    // The rest are all decimal arithmetic operations ...
    case OP_DADC:
    case OP_DSMB:
    case OP_DADD:
    case OP_DSM:
      m_B = MemRead(m_X);  DoDecimal();  break;
    case OP_DACI:
    case OP_DSBI:
    case OP_DADI:
    case OP_DSMI:
      m_B = MemReadInc(m_P);  DoDecimal();  break;

    // Any other opcode is bad...
    default: IllegalOpcode();  break;
  }
}

void CCOSMAC::DoInOut()
{
  //++
  //   This method handles the 0x6N opcodes.  These are the COSMAC input and
  // output instructions, with two exceptions - 0x60 increments R[X] but
  // performs no I/O, and 0x68 is an escape for 1804/5/6 extended opcodes.
  // Note that the COSMAC I/O instructions are an odd combination of memory
  // read and write.  INPUT loads D from the bus, but also asserts MWR and
  // thus writes M[R[X]] too.  OUTPUT doesn't use D or drive the bus at all, but
  // rather asserts MRD and thus writes the peripheral register with M[R[X]]).
  // The special cases 0x60 and 0x68 suppress MWR/MRD and don't use memory.
  //--
  address_t nDevice = m_N & 7;
  if (nDevice == 0) {
    //   Device 0 is not used and these are not I/O instructions.  Opcode 0x60
    // is IRX, and 0x68 is the escape for 1804/5/6 extended instructions ...
    if (m_N == 0x0)
      IncReg(m_X);
    else if (m_fExtended)
      DoExtended();
    else
      IllegalOpcode();
  } else {
    if (m_N > 0x8) {
      // INPUT - D, M[R[X]] <= device ...
      uint8_t bData = ReadInput(nDevice);
      m_D = bData;  MemWrite(m_X, bData);
      LOGF(TRACE, "COSMAC read data 0x%02X from port %d", bData, nDevice);
    } else {
      // OUTPUT - device <= M[R[X]], R[X] <= R[X] + 1 ...
      uint8_t bData = MemReadInc(m_X);
      WriteOutput(nDevice, bData);
      LOGF(TRACE, "COSMAC wrote data 0x%02X to port %d", bData, nDevice);
    }
  }
}

void CCOSMAC::DoShortBranch()
{
  //++
  //   COSMAC short branch instructions are two bytes long and replace the
  // lower byte of the PC with the second byte IF the branch condition is
  // met.  If the condition isn't met, then the second byte is skipped.  The
  // upper byte of the PC doesn't change.
  //--
  bool fBranch = false;

  //   There are only 8 branch conditions the COSMAC can test - the first eight
  // opcodes branch on the condition, and the last 8 branch on not condition ...
  switch (m_N & 0x7) {
    case 0x0: fBranch = true;               break;  // BR/SKP
    case 0x1: fBranch = m_Q         != 0;   break;  // BQ/BNQ
    case 0x2: fBranch = m_D         == 0;   break;  // BZ/BNZ
    case 0x3: fBranch = m_DF        != 0;   break;  // BDF/BNF
    case 0x4: fBranch = UpdateEF(0) != 0;   break;  // B1/BN1
    case 0x5: fBranch = UpdateEF(1) != 0;   break;  // B2/BN2
    case 0x6: fBranch = UpdateEF(2) != 0;   break;  // B3/BN3
    case 0x7: fBranch = UpdateEF(3) != 0;   break;  // B4/BN4
  }
  if (m_N & 0x8) fBranch = !fBranch;
  
  //   Short branch instructions either load R[P].0 with the next byte (when
  // the branch condition is true), or increment R[P] to skip over it.
  if (fBranch)  PutRegLo(m_P, MemRead(m_P));  else IncReg(m_P);

  //   If this was a branch on a condition that can't change externally (i.e.
  // NOT one of the EF inputs!), AND interrupts are disabled, AND the branch
  // destination is the same as the address of this instruction, THEN we're
  // in an endless loop!
  if (    ((m_N & 0x7) < 4)  &&  (m_MIE == 0)
      &&  (GetRegLo(m_P) == LOBYTE(m_nLastPC))) m_nStopCode = STOP_ENDLESS_LOOP;
}

void CCOSMAC::DoInterruptBranch()
{
  //++
  //   This routine handles the 1804/5/6 extended "branch on interrupt"
  // instructions.  There are only two - branch on counter/timer interrupt, 
  // and branch on external interrupt.  Except for the condition tested, both
  // of these work exactly the same as the regular branch instructions.
  // 
  //   Note that both test the state of the interrupt request, regardless of
  // any interrupt enable bits!
  // 
  //   Another note - BCI has the side effect of clearing ETQ IF the branch
  // is taken!
  //--
  bool fBranch = false;
  switch (m_N) {
    case 0x0E:  fBranch = (m_CIR != 0);  if (fBranch) m_ETQ = 0;  // BCI
                break;
    case 0x0F:  fBranch = (m_XIR != 0);  break;                   // BXI
    default:    IllegalOpcode();  return;
  }
  if (fBranch)  PutRegLo(m_P, MemRead(m_P));  else IncReg(m_P);
}

void CCOSMAC::DoLongBranch()
{
  //++
  //   COSMAC long branch instructions are three bytes long, where the second
  // and third bytes are a full 16 bit address.  These bytes are loaded into
  // the PC, high byte first, if the branch condition is met.  They're skipped
  // if the branch condition isn't met.
  //
  //   COSMAC long skip instructions are only one byte long, and will skip the
  // next two bytes if the skip condition is met.  If the condition isn't met,
  // execution continues with the next byte.
  //
  //   It doesn't take a genius to see that a successful long skip is the same
  // thing as a failing long branch.  An unsuccessful skip is really just a NOP,
  // and a successful long branch loads the PC.  So there are really only three
  // cases to be implemented here...
   //--

  //   fTest is TRUE if the condition is met - i.e. the branch should be taken
  // or the skip skipped.  Bit 3 of the opcode inverts the sense of the test.
  // Note that there are really only four conditions - always true, Q, D==0,
  // and DF (plus a special case for LSIE).
  bool fTest = false;
  switch (m_N & 3) {
    case 0x0: fTest = (m_N==0xC) ? (m_MIE != 0) : true;  break;  // LBR/LSKP,  NOP/LSIE
    case 0x1: fTest = m_Q  != 0;                         break;  // LBQ/LBNQ,  LSNQ/LSQ
    case 0x2: fTest = m_D  == 0;                         break;  // LBZ/LBNZ,  LSNZ/LSZ
    case 0x3: fTest = m_DF != 0;                         break;  // LBDF/LBNF, LSNF/LSDF
  }
  if (((m_N & 0xC) == 0x8) || ((m_N & 0xC) == 0x4)) fTest = !fTest;

  // fSkip is true if this is a skip (as opposed to branch) instruction ...
  bool fSkip = (m_N & 4) != 0;

  //   All long branch/skip instructions require two execute (S1) cycles and
  // this table summarizes what happens during each cycle -
  //
  //       fTest              fSkip=false (branch)                 fSkip=true (skip)
  //    -----------    ------------------------------------       -------------------
  //        true       B=M[R[P]], ++R[P] / R[P]={B,M[R[P]]}        ++R[P] / ++R[P]
  //        false                 ++R[P] / ++R[P]                     do nothing
  //
  //   To simplify things, all cases EXCEPT a failing skip read M[R[P]] and store
  // the byte in B.  For a a sucessful long branch this is the high byte of the
  // address, but for the other a failing branch or successful skip this byte
  // will be unused.  Not sure if a real 1802 actually reads memory in this case,
  // but it's harmless.
  if (fTest || !fSkip) m_B = MemReadInc(m_P);

  //   The second S1 cycle either loads R[P] (for a successful branch), or just
  // increments R[P] (for a failing branch or a successful skip), or does
  // does nothing (for a failing skip!).
  if (fTest && !fSkip) {
    m_R[m_P] = MKWORD(m_B, MemRead(m_P));
  } else if ((!fTest && !fSkip) || (fTest && fSkip)) {
    IncReg(m_P);
  }

  // Add 8 clocks for the second S1 cycle ...
  AddCycles(1);

  //   If the current PC is the same as the original PC of this instruction,
  // AND if interrupts are disabled, then this is a "branch to self" and
  // we're in an endless loop.  Note that it's impossible to create a loop
  // with skip instructions, but those don't alter the results of the test.
  if ((m_MIE == 0) && (m_R[m_P] == m_nLastPC)) m_nStopCode = STOP_ENDLESS_LOOP;
}

void CCOSMAC::DoSCAL()
{
  //++
  //   Emulate the extended SCAL instruction, which is the hardware 
  // implementation of the 1802 SCRT call.  It's fairly complex as COSMAC
  // instructions go, and requires 10 cycles total (7 more than the two
  // fetch and one execute that we've already allowed for).
  //--
  m_T = LOBYTE(m_R[m_N]);  m_B = HIBYTE(m_R[m_N]);
  MemWriteDec(m_X, m_T);  MemWriteDec(m_X, m_B);
  m_R[m_N] = m_R[m_P];
  m_T = MemReadInc(m_N);  m_B = MemReadInc(m_N);
  m_R[m_P] = MKWORD(m_T, m_B);
  AddCycles(7);
}

void CCOSMAC::DoSRET()
{
  //++
  // And this is the SRET instruction (the reverse of SCAL!) ...
  //--
  m_R[m_P] = m_R[m_N];
  IncReg(m_X);
  m_T = MemReadInc(m_X);  m_B = MemRead(m_X);
  m_R[m_N] = MKWORD(m_T, m_B);
  AddCycles(5);
}

void CCOSMAC::DoExecute()
{
  //++
  //   This method is called after an instruction has been fetched and loaded
  // into the I (high nibble) and N (low nibble) registers.  About two thirds
  // of the COSMAC instructions of the form $xN, where x is a four bit opcode
  // and N is a register number.  These are easily executed directly here, and
  // the others are sent off to more specialized routines...
  //--
  switch (m_I<<4) {
    // N == 0 is IDLE, but N != 0 is LDN!
    case   0x00:  if (m_N==0) DoIdle(); else m_D=MemRead(m_N);  break;  // 0x0N - LOAD VIA REG N
    case OP_INC:  IncReg(m_N);					break;  // 0x1N - INCREMENT REG N
    case OP_DEC:  DecReg(m_N);					break;  // 0x2N - DECREMENT REG N
    case   0x30:  DoShortBranch();                              break;  // 0x3N - SHORT BRANCH
    case OP_LDA:  m_D = MemReadInc(m_N);			break;  // 0x4N - LOAD ADVANCE VIA REG N
    case OP_STR:  MemWrite(m_N, m_D);				break;  // 0x5N - STORE VIA REG N
    case   0x60:  DoInOut();                                    break;  // 0x6N - INPUT/OUTPUT/EXTENDED
    case   0x70:  Do7xFx();                                     break;  // 0x7N - ALU OPERATIONS
    case OP_GLO:  m_D = GetRegLo(m_N);				break;  // 0x8N - GET LOW REG N
    case OP_GHI:  m_D = GetRegHi(m_N);				break;  // 0x9N - GET HIGH REG N
    case OP_PLO:  PutRegLo(m_N, m_D);				break;  // 0xAN - PUT LOW REG N
    case OP_PHI:  PutRegHi(m_N, m_D);				break;  // 0xBN - PUT HIGH REG N
    case   0xC0:  DoLongBranch();                               break;  // 0xCN - LONG BRANCH
    case OP_SEP:  m_P = m_N;					break;  // 0xDN - SET P TO N
    case OP_SEX:  m_X = m_N;					break;  // 0xEN - SET X TO N
    case   0xF0:  Do7xFx();                                     break;  // 0xFN - ALU OPERATIONS
  }
}

void CCOSMAC::DoExtended()
{
  //++
  //   This routine is called when we discover an 1804/5/6 extended opcode.
  // It requires a second fetch cycle to get the actual opcode, and then
  // there is a completely different set of instructions we can execute.
  // 
  //   Note that the code at Run() assumes all instructions take two cycles
  // - one for fetch and one for execute.  All of the extended instructions
  // at least one extra cycle for an additional fetch, and many of them
  // take several extra cycles.  SCAL, for example, takes a whopping eight
  // machine cycles to execute (plus two for the fetch).  None of that is a
  // problem, but we have to add extra cycles here to correct for that.
  //--
  uint8_t eop = MemReadInc(m_P);  AddCycles(1);
  m_I = HINIBBLE(eop);  m_N = LONIBBLE(eop);
  switch (m_I << 4) {
    // Counter/timer and interrupt enable instructions ...
    case 0x00:    DoCounter();            break;
    // Branch on interrupt request instructions ...
    case 0x30:    DoInterruptBranch();    break;
    // Decimal arithmetic and other miscellaneous instructions ...
    case 0x70:
    case 0xF0:    Do7xFxExtended();       break;
    // Standard call and return instructions ...
    case OP_SCAL: DoSCAL();               break;
    case OP_SRET: DoSRET();               break;
    // Decrement register and long branch if not zero ...
    case OP_DBNZ: DecReg(m_N);  m_B = MemReadInc(m_P);
                  if (m_R[m_N] != 0)
                    m_R[m_P] = MKWORD(m_B, MemRead(m_P));
                  else
                    IncReg(m_P);
                  AddCycles(2);
                  break;
    // Register load via X and advance (POP register, more or less!) ...
    case OP_RLXA: m_T = MemReadInc(m_X);  m_B = MemReadInc(m_X);
                  m_R[m_N] = MKWORD(m_T, m_B);  AddCycles(2);
                  break;
    // Register store via X and decrement (PUSH register) ...
    case OP_RSXD: m_T = LOBYTE(m_R[m_N]);  m_B = HIBYTE(m_R[m_N]);
                  MemWriteDec(m_X, m_T);  MemWriteDec(m_X, m_B);
                  AddCycles(2);
                  break;
    // Transfer register N to register X ...
    case OP_RNX:  m_R[m_X] = m_R[m_N];  AddCycles(1);
                  break;
    // Load register N immediate ...
    case OP_RLDI: m_T = MemReadInc(m_P);  m_B = MemReadInc(m_P);
                  m_R[m_N] = MKWORD(m_T, m_B);  AddCycles(2);
                  break;
    // And everthing else is illegal ...
    default:  IllegalOpcode();  break;
  }
}

void CCOSMAC::DoInterrupt()
{
  //++
  //   This method will simulate a COSMAC S2, interrupt acknowledge, cycle.
  // It stores X,P in T, and then forces X=2 and P=1.  The IE flop is cleared
  // to disable future interrupts...
  //
  //   Note that this doesn't check first to see if interrupts are actually
  // enabled now - that's the caller's job!!
  //--
  m_T = MKBYTE(m_X, m_P);  m_X = 2;  m_P = 1;  m_MIE = 0;
  // This takes a whole major cycle (8 clocks) to execute ...
  AddCycles(1);
}

CCPU::STOP_CODE CCOSMAC::Run (uint32_t nCount)
{
  //++
  //   This is the main "engine" of the COSMAC emulator.  The UI code is	
  // expected to call it whenever the user gives a START, GO, STEP, etc	
  // command and it will execute COSMAC instructions until it either a)	
  // executes the number of instructions specified by nCount, or b) some	
  // condition arises to interrupt the simulation such as an IDL opcode,	
  // an illegal opcode or I/O, the user entering the escape sequence on	
  // the console, etc.  If nCount is zero on entry, then we will run for-	
  // ever until one of the previously mentioned break conditions arises.	
  //--
  bool fFirst = true;
  m_nStopCode = STOP_NONE;
  while (m_nStopCode == STOP_NONE) {

    // If any device events need to happen, now is the time...
    m_pEvents->DoEvents();

    //   See if any I/O device is requesting an interrupt now.  If one is, and
    // if COSMAC interrupts are enabled, then simulate an interrupt acknowledge.
    if (m_pInterrupt != NULL) m_XIR = m_pInterrupt->IsRequested();
    if ((((m_XIR & m_XIE) | (m_CIR & m_CIE)) & m_MIE) != 0) {
      DoInterrupt();  m_pInterrupt->AcknowledgeRequest();
    }

    // Stop if we've hit a breakpoint ...
    if (!fFirst && m_pMemory->IsBreak(m_R[m_P])) {
      m_nStopCode = STOP_BREAKPOINT;  break;
    } else
      fFirst = false;

    // Fetch, decode and execute an instruction...
    //   This automatically increments the simulated time for the fetch (S0)
    // cycle and one execute (S1) cycle.  For the very few instructions that
    // have a second S1 cycle, they're responsible for adding another 8 cycles.
    m_nLastPC = GetPC();  AddCycles(1);
    uint8_t op = MemReadInc(m_P);  m_I = HINIBBLE(op);  m_N = LONIBBLE(op);
    DoExecute();  AddCycles(1);

    // Check for some termination conditions ...
    if (m_nStopCode == STOP_NONE) {
      // Terminate if we've executed enough instructions ... 
      if ((nCount > 0)  &&  (--nCount == 0))  m_nStopCode = STOP_FINISHED;
    }
  }

  return m_nStopCode;
}

unsigned CCOSMAC::GetRegisterSize (cpureg_t nReg) const
{
  //++
  //   This method returns the size of a given register, IN BITS!
  // It's used only by the UI, to figure out how to print and mask register
  // values...
  //--
  switch (nReg) {
    case REG_DF:  case REG_IE:  case REG_Q:
    case REG_EF1: case REG_EF2: case REG_EF3: case REG_EF4:
    case REG_XIE: case REG_CIE: case REG_CIL: case REG_ETQ:
      return 1;
    case REG_P:   case REG_X:   case REG_I:   case REG_N:
      return 4;
    case REG_D:   case REG_T:   /*case REG_B:*/
    case REG_CH:  case REG_CNTR:
      return 8;
    case REG_R0:  case REG_R1:  case REG_R2:  case REG_R3:
    case REG_R4:  case REG_R5:  case REG_R6:  case REG_R7: 
    case REG_R8:  case REG_R9:  case REG_RA:  case REG_RB:
    case REG_RC:  case REG_RD:  case REG_RE:  case REG_RF:
      return 16;
    default:
      return 0;
  }
}

uint16_t CCOSMAC::GetRegister (cpureg_t nReg) const
{
  //++
  // This method will return the contents of an internal CPU register ...
  //--
  switch (nReg) {
    case REG_D:     return m_D;
    case REG_DF:    return m_DF;
    case REG_T:     return m_T;
  //case REG_B:     return m_B;
    case REG_P:     return m_P;
    case REG_X:     return m_X;
    case REG_I:     return m_I;
    case REG_N:     return m_N;
    case REG_IE:    return m_MIE;
    case REG_XIE:   return m_XIE;
    case REG_CIE:   return m_CIE;
    case REG_CIL:   return m_CIR;
    case REG_ETQ:   return m_ETQ;
    case REG_Q:     return m_Q;
    case REG_EF1:   return m_EF[0];
    case REG_EF2:   return m_EF[1];
    case REG_EF3:   return m_EF[2];
    case REG_EF4:   return m_EF[3];
    case REG_CNTR:  return m_CNTR;
    case REG_CH:    return m_CH;
    case REG_R0:  case REG_R1:  case REG_R2:  case REG_R3:
    case REG_R4:  case REG_R5:  case REG_R6:  case REG_R7:
    case REG_R8:  case REG_R9:  case REG_RA:  case REG_RB:
    case REG_RC:  case REG_RD:  case REG_RE:  case REG_RF:
      return m_R[nReg];
    default:
      return 0;
  }
}

void CCOSMAC::SetRegister (cpureg_t nReg, uint16_t nVal)
{
  //++
  // Change the contents of an internal CPU register ...
  //
  //   Note that registers I and N cannot be set - there would be no point in
  // doing that, since they're recomputed with every instruction fetch.
  //--
  switch (nReg) {
    case REG_D:   m_D     = nVal & 0xFF;  break;
    case REG_DF:  m_DF    = nVal & 0x01;  break;
  //case REG_T:   m_T     = nVal & 0xFF;  break;
  //case REG_B:   m_B     = nVal & 0xFF;  break;
    case REG_P:   m_P     = nVal & 0x0F;  break;
    case REG_X:   m_X     = nVal & 0x0F;  break;
  //case REG_I:   m_I     = nVal & 0x0F;  break;
  //case REG_N:   m_N     = nVal & 0x0F;  break;
    case REG_CH:  m_CH    = nVal & 0xFF;  break;
  //case REG_CNTR:m_CNTR  = nVal & 0xFF;  break;
    case REG_IE:  m_MIE   = nVal & 0x01;  break;
    case REG_XIE: m_XIE   = nVal & 0x01;  break;
    case REG_CIE: m_CIE   = nVal & 0x01;  break;
  //case REG_CIL: m_CIL   = nVal & 0x01;  break;
    case REG_ETQ: m_ETQ   = nVal & 0x01;  break;
    case REG_Q:   m_Q     = nVal & 0x01;  break;
    case REG_EF1: m_EF[0] = nVal & 0x01;  break;
    case REG_EF2: m_EF[1] = nVal & 0x01;  break;
    case REG_EF3: m_EF[2] = nVal & 0x01;  break;
    case REG_EF4: m_EF[3] = nVal & 0x01;  break;
    case REG_R0:  case REG_R1:  case REG_R2:  case REG_R3:
    case REG_R4:  case REG_R5:  case REG_R6:  case REG_R7:
    case REG_R8:  case REG_R9:  case REG_RA:  case REG_RB:
    case REG_RC:  case REG_RD:  case REG_RE:  case REG_RF:
      m_R[nReg] = nVal;  break;
    default:
      break;
  }
}

address_t CCOSMAC::GetPC() const
{
  //++
  //   Return the current program counter.  This isn't really hard, but on
  // the COSMAC we have to use the P register to find the PC first...
  //--
  return m_R[m_P];
}

const char *CCOSMAC::CounterModeToString(CTMODE mode)
{
  //++
  // Convert the timer mode to an ASCII string for debugging ...
  //--
  switch (mode) {
    case CT_STOPPED:  return "STOPPED";
    case CT_EVENT1:   return "EF1 EVENT COUNTER";
    case CT_EVENT2:   return "EF2 EVENT COUNTER";
    case CT_PULSE1:   return "EF1 PULSE COUNTER";
    case CT_PULSE2:   return "EF2 PULSE COUNTER";
    case CT_TIMER:    return "TIMER";
    default:          return "UNKNOWN";
  }
}
