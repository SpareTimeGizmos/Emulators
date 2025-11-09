//++
//INS8070.cpp - National INS807x SC/MP-III emulation
//
// DESCRIPTION:
//   This module implements a simulation of the National SC/MP-III (INS807x)
// microprocessors.  The three chips in this family that I know of are -
// 
//   INS8070 - SC/MP-III with 64 bytes of internal RAM
//   INS8072 - INS8070 with 2.5K of internal mask ROM
//   INS8073 - INS8072 with NIBL2 BASIC programmed in the ROM
// 
//   The INS8070 is a different beast from the INS8050 and INS8060.  National
// fixed many of the shortcomings in the original instruction set, and the
// INS8070 has many added instructions including stack operations, a real
// subroutine call, 16 bit double precision math functions, and even multiply
// and divide instructions.  Better yet, the INS8070 has a flat 16 bit address
// space with none of the 4 bit page number, 12 bit page offset nonsense of its
// predecessors.  The INS8070 is really quite a nice architecture, but it's
// not even remotely backward compatible with the INS8050/8060.
// 
//  The SC/MP-III has several "oddities" that bear mention -
//
//    * The INS8070 still has the PC pre-increment, BEFORE an instruction fetch
//    "feature" of all the SC/MP family.
// 
//    * The INS8070 lacks the explicit serial input/serial output and SIO
//    instruction of the INS8050/8060.  It still has two sense inputs and three
//    flag ouptuts, but you have to explicitly program these as a software UART.
//
//    * On the INS8070, both the SENSE A and SENSE B inputs cause interrupts.
//    Better yet, on the INS8070 interrupts are vectored thru locations in low
//    memory and SENSE A and B have independent vectors.
//
//    * The INS8070 has no "add with carry" or "subtract with carry"
//    instructions, but as a consolation prize it does have 16 bit arithmetic
//    built in.
// 
//    * One nasty problem is that the instruction timing differs depending on
//    whether internal memory or external memory is accessed.  The times given
//    here are all for INTERNAL memory, RAM or ROM, access.  Access to external
//    memory takes one extra microcycle for every access, read or write.  That
//    goes for both instruction fetch and operand access.
//
// REVISION HISTORY:
// 30-OCT-25  RLA  New file.
//  5-NOV-25  RLA  DoExecute() can't return zero for an illegal opcode!
//                 JSR has to push the PC _after_ fetching the operand
//                 Can't use REL8(m_PC) we have to use REL8(m_PC+1)
//                 Opcode $8B, ST EA,xx[P3] wrongly uses P2 instead of P3
//                 Opcode $CB, ST A,xx[P3] made the same mistsake!
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include "EMULIB.hpp"           // emulator library definitions
#include "LogFile.hpp"          // emulator library message logging facility
#include "CommandParser.hpp"    // needed for type KEYWORD
#include "MemoryTypes.h"        // address_t and word_t data types
#include "SCMP3.hpp"             // global declarations for this project
#include "Interrupt.hpp"        // interrupt simulation logic
#include "EventQueue.hpp"       // I/O device event queue simulation
#include "INS8070opcodes.hpp"   // COSMAC opcode definitions
#include "CPU.hpp"              // CCPU base class definitions
#include "Memory.hpp"           // CMemory memory emulation object
#include "Device.hpp"           // CDevice I/O device emulation objects
#include "INS8070.hpp"          // declarations for this module

// Internal CPU register names for GetRegisterNames() ...
const CCmdArgKeyword::KEYWORD CSCMP3::g_keysRegisters[] = {
  {"PC", CSCMP3::REG_PC}, {"SP", CSCMP3::REG_SP}, {"P2", CSCMP3::REG_P2},
  {"P3", CSCMP3::REG_P3}, {"A", CSCMP3::REG_A},   {"E",  CSCMP3::REG_E},
  {"T",  CSCMP3::REG_T},  {"S", CSCMP3::REG_S},   {NULL, 0}
};

// Names for the sense inputs and flag outputs ...
const char *CSCMP3::g_apszSenseNames[MAXSENSE] = {
  "SENSEA", "SENSEB"
};
const char *CSCMP3::g_apszFlagNames[MAXFLAG] = {
  "FLAG1", "FLAG2", "FLAG3"
};


CSCMP3::CSCMP3 (CMemory *pMemory, CEventQueue *pEvents, CInterrupt *pInterrupt)
  : CCPU(pMemory, pEvents, pInterrupt)
{
  //++
  //--
  SetCrystalFrequency(DEFAULT_CLOCK);
  CSCMP3::ClearCPU();
}

void CSCMP3::ClearCPU()
{
  //++
  //   This routine resets the SC/MP to a power on state.  On the real INS8070
  // the RESET input just sets the PC, SP and status to zeros and all other
  // registers are unaffected.  We set everything to zero to give consistent
  // results for every simulation.
  //--
  CCPU::ClearCPU();
  m_PC = m_SP = m_P2 = m_P3 = m_T = 0;  m_A = m_E = 0;
  // DON'T clear the sense bits in the status, but clear everything else ...
  CLRBIT(m_S, ~(SR_SA | SR_SB));
  UpdateFlag(FLAG1, 0);  UpdateFlag(FLAG2, 0);  UpdateFlag(FLAG3, 0);
}

void CSCMP3::UpdateFlag (uint16_t nFlag, uint1_t bNew)
{
  //++
  // Update a flag output and handle any software serial emulation ...
  //--
  SetFlag(nFlag, MASK1(bNew));
}

uint1_t CSCMP3::UpdateSense (uint16_t nSense)
{
  //++
  //   This routine is called whenever an sense input is tested by the CPU.
  // It will check to see if an installed device is connected to this input
  // and, if one is, query the device for the current state of this input.
  //--
  assert(nSense < MAXSENSE);
  uint1_t bData = GetSense(nSense);
  if (nSense == SENSEA) {
    CLRBIT(m_S, SR_SA);  if (bData != 0) SETBIT(m_S, SR_SA);
  } else if (nSense == SENSEB) {
    CLRBIT(m_S, SR_SB);  if (bData != 0) SETBIT(m_S, SR_SB);
  }
  return bData;
}

void CSCMP3::SetStatus (uint8_t bData)
{
  //++
  // Load the status register and update all flag outputs ...
  //--
  bool fOldF1 = ISSET(m_S, SR_F1), fNewF1 = ISSET(bData, SR_F1);
  bool fOldF2 = ISSET(m_S, SR_F2), fNewF2 = ISSET(bData, SR_F2);
  bool fOldF3 = ISSET(m_S, SR_F3), fNewF3 = ISSET(bData, SR_F3);
  m_S = (m_S & (SR_SA|SR_SB)) | (bData & ~(SR_SA|SR_SB));
  if (fOldF1 != fNewF1) UpdateFlag(FLAG1, fNewF1 ? 1 : 0);
  if (fOldF2 != fNewF2) UpdateFlag(FLAG2, fNewF2 ? 1 : 0);
  if (fOldF3 != fNewF3) UpdateFlag(FLAG3, fNewF3 ? 1 : 0);
}

uint8_t CSCMP3::GetStatus()
{
  //++
  // Return the current status byte, but update all sense inputs first ...
  //--
  UpdateSense(SENSEA);  UpdateSense(SENSEB);
  return m_S;
}

uint8_t CSCMP3::MEMR8 (address_t wAddr)
{
  //++
  //   This routine is called for ALL memory read operations.  The reason we
  // need this is because EXTERNAL memory accesses on the INS807x take one
  // extra microcyle compared to INTERNAL RAM/ROM accesses.  All the timing
  // DoExecute() and elsewhere is calculated for internal memory, and if
  // this access is really external then we need to add an extra cycle.
  // 
  //   It's a bummer that we have to add this extra layer to memory accesses,
  // but it affects the timing significantly.  In particular, this screws up
  // the software bit banged UART in NIBL BASIC unless we get it right.
  // 
  //   From our point of view, external memory (either RAM or ROM) is defined
  // by the SLOW memory attribute and is normally set with a UI command.
  //--
  //if (m_pMemory->IsBreak(wAddr)) Break(STOP_BREAKPOINT);
  if (m_pMemory->IsSlow(wAddr)) AddCycles(1);
  return m_pMemory->CPUread(wAddr);
}

void CSCMP3::MEMW8 (address_t wAddr, uint8_t bData)
{
  //++
  //   And this routine should be called for ALL memory write operations.
  // Just like MEMR8(), the only reason we need it is to handle the extra
  // microcycle for external memory access.
  //--
  //if (m_pMemory->IsBreak(wAddr)) Break(STOP_BREAKPOINT);
  if (m_pMemory->IsSlow(wAddr)) AddCycles(1); 
  m_pMemory->CPUwrite(wAddr, bData);
}

address_t CSCMP3::AUTO (uint16_t &wReg, uint8_t bOffset)
{
  //++
  //   This routine computes the effect address for the auto-indexing addressing
  // mode.  This works like regular indexing, EXCEPT in this case the offset is
  // treated as a signed value and is added to the current contents of the
  // register.  The trick is that negative offsets are pre-deccremented meaning
  // that the index register is updated first and then the result is used as the
  // address.  For positive offsets the current contents of the register are
  // used as the address, and then the register value is updated after that.
  // 
  //   Note that auto-indexing takes two extra cycles, but that is accounted
  // for in the DoExecute() procedure.
  //--
  if (ISNEG8(bOffset)) {
    wReg += SEXT16(bOffset);  return wReg;
  } else {
    uint16_t w = wReg;  wReg += bOffset;  return w;
  }
}

void CSCMP3::DoInterrupt()
{
  //++
  //   If either the SENSE A or SENSE B input is high AND the interrupt enable
  // bit is set, then simulate a JSR (aka PLI PC,) to the corresponding
  // interrupt vector.  This takes a total of 9 microcyles ...
  //--
  if (ISSET(m_S, SR_IE) && (ISSET(GetStatus(), SR_SA|SR_SB))) {
    uint16_t wOldPC = m_PC;
    CLRBIT(m_S, SR_IE);  Push16(m_PC);  AddCycles(9);
    //   Remember the PC pre-increment thing, so the correct value for the PC
    // is the interrupt vector MINUS ONE!!
    m_PC = (ISSET(m_S,SR_SA) ? INTA_VEC : INTB_VEC) - 1;
    LOGF(TRACE, "INTERRUPTED - old PC=0x%04X, new PC=0x%04X", wOldPC, GetPC());
  }
}

void CSCMP3::AddA (uint8_t bData)
{
  //++
  //   Add an 8 bit binary value to A.  The CY/L and OV flags are also updated
  // according to the result.
  //--
  uint16_t wResult = m_A + bData;  CLRBIT(m_S, SR_CYL|SR_OV);
  // The carry flag is easy ...
  if (HIBYTE(wResult) != 0) SETBIT(m_S, SR_CYL);
  //  There are two cases that result in a twos complement overflow - if we
  // add two positive numbers and get a negative result, or if we add two
  // negative numbers and get a positive result.  Any other combination can
  // never overflow.
  uint8_t bResult = LOBYTE(wResult);
  if (   (!ISNEG8(m_A) && !ISNEG8(bData) &&  ISNEG8(bResult))
      || ( ISNEG8(m_A) &&  ISNEG8(bData) && !ISNEG8(bResult)))
    SETBIT(m_S, SR_OV);
  // Update A and we're done ...
  m_A = bResult;
}

void CSCMP3::AddEA (uint16_t wData)
{
  //++
  //    This routine adds a 16 bit value to the EA register pair.  It's pretty
  // much the same as ADDA(), except that this one is a little messy because
  // the E and A registers are stored separately.  Except for the number of bits
  // involved, the rules for setting CY and OV are the same.
  //--
  uint16_t wEA = GetEA();  CLRBIT(m_S, SR_CYL | SR_OV);
  uint32_t lResult = wEA + wData;
  if (HIWORD(lResult) != 0) SETBIT(m_S, SR_CYL);
  uint16_t wResult = LOWORD(lResult);
  if (   (!ISNEG16(wEA) && !ISNEG16(wData) &&  ISNEG16(wResult))
      || ( ISNEG16(wEA) &&  ISNEG16(wData) && !ISNEG16(wResult)))
    SETBIT(m_S, SR_OV);
  SetEA(wResult);
}

void CSCMP3::Multiply()
{
  //++
  //   The MUL instruction multiplies a 16 bit signed value in EA by the 16
  // bit signed value in T.  The most significant 16 bits of the result are
  // stored in EA and the least significant 16 bits are stored in T, replacing
  // the original values.  
  // 
  //    The datasheet says that this instruction affects the CY and OV flags,
  // but it says nothing about exactly how they are affected.  I'm not sure
  // what actually happens here since a 16x16 multiply can never overflow 
  // 32 bits.  At the moment we just arbitrarily clear both flags.
  //--
  int32_t lResult = ((int16_t) GetEA()) * ((int16_t) m_T);
  m_T = LOWORD(lResult);  SetEA(HIWORD(lResult));
  CLRBIT(m_S, SR_CYL|SR_OV);
}

void CSCMP3::Divide()
{
  //++
  //    The DIV instruction divides a 16 bit signed or unsigned dividend in EA
  // by a 15 bit unsigned divisor in T.  The divisor must be in the range 0 ..
  // 32767.  The 16 bit quotient is stored in EA and the contents of the T
  // register are lost.  The datasheet says that "the partial remainder returned
  // in the T register is not a true remainder and for most purposes should be
  // treated as undefined".  That's sad, National!
  // 
  //    The datasheet also says that this affects the CY and OV flags, but it
  // doesn't say exactly how, nor does it say what happens if you try to divide
  // by zero.  In this implementation I set the CY flag if the divisor is zero,
  // and always clear OV.  I also return the proper remainder in T, but that
  // probably doesn't matter since real INS807x software should ignore it.
  //--
  CLRBIT(m_S, SR_CYL|SR_OV);
  if (m_T == 0) {
    LOGF(DEBUG, "Division by zero at 0x%04X", GetPC());
    SETBIT(m_S, SR_CYL);  return;
  } else if (ISNEG16(m_T)) {
    LOGF(DEBUG, "Negative divisor at 0x%04X", GetPC());
    SETBIT(m_S, SR_CYL);  return;
  } else {
    int16_t wResult = ((int16_t) GetEA()) / ((int16_t) m_T);
    int16_t wRemainder = ((int16_t) GetEA()) % ((int16_t) m_T);
    SetEA(wResult);  m_T = wRemainder;
  }
}

void CSCMP3::ShortCall (uint8_t bVector)
{
  //++
  //   The CALL instruction is simply a short form of a subroutine call (sort
  // of like the 8080 RST instructions).  The operand is a single 4 bit value
  // which calls a subroutine via one of 16 vectors stored in low memory. It
  // is otherwise identical to a JSR or PLI PC, instruction ...
  //--
  assert(bVector <= 15);
  uint16_t wAddr = CALL_VEC + 2*bVector;
  Push16(m_PC);  m_PC = MEMR16(wAddr);
}

uint64_t CSCMP3::BranchNotDigit (address_t wAddr)
{
  //++
  //   The BND instruction tests the accumulator for an ASCII digit (i.e. a
  // value in the range 0x30 to 0x39).  If it finds one then it subtracts
  // 0x30 from A and proceeds with the next instruction.  If A does not
  // contain an ASCII digit, then it branches to the address given (in the
  // same manner as all the other branch instructions).
  // 
  //    The use for this is obvious, but I'm not sure why National thought it
  // was worth inventing a special instruction just for this.  But hey, it's
  // easy enough to implement.
  // 
  //    Note that the timing for this instruction is weird - it takes 7 cycles
  // if A is .LT. 0x30, and 9 cycles otherwise.  In the latter case it doesn't
  // matter whether A is also .LE. 0x39 or whether the branch is taken or not.
  // Because of this, we return the actual number of cycles needed .
  // 
  //    The datasheet doesn't say whether this affects any flags or not, but
  // we assume not.
  //--
  if (m_A < 0x30) {
    m_PC = wAddr;  return 7;
  } else if (m_A <= 0x39) {
    m_A -= 0x30;  return 9;
  } else {
    m_PC = wAddr;  return 9;
  }
}

uint64_t CSCMP3::SearchAndSkip (uint16_t &wReg)
{
  //++
  //   The SSM (search and skip if character match) instruction searches memory
  // for a byte that matches the current accumulator contents.  The address
  // where searching begins is specified by the pointer register and the maximum
  // number of bytes searched is ALWAYS 256.  There's no provision for any other
  // length.  
  // 
  //   If a match is found then the PC is incremented by two, presumably skipping
  // a BRA instruction following this one.  And if a match is found then the 
  // pointer register will be left pointing to the next byte AFTER the match.
  // 
  //   If no match is found then the next instruction in sequence is executed and
  // the pointer register is left with 255 plus the original contents (i.e. one
  // less than what it would have been if a match were found).  
  // 
  //   The number of microcycles required is variable, with each unsuccessful
  // iteration taking four cycles.  If a match is found and we need to increment
  // the PC, then 7 extra cycles are needed.  If no match is found and we don't
  // increment the PC, then 5 extra cycles are needed.
  //--
  uint64_t qCycles = 0;  uint16_t wCount = 256;
  while (m_A != MEMR8(wReg)) {
    qCycles += 4;
    if (--wCount == 0) return qCycles+5;
    INC16(wReg);
  }
  INC16(wReg);  INC16(m_PC, 2);
  return qCycles+4+7;
}

void CSCMP3::AddMemory (address_t wAddr, int8_t bAdd)
{
  //++
  //   Add a signed (!!) constant to a memory location, and update memory with
  // the new value.  Load the new value into the A register as well.  This is
  // used by the ILD and DLD instructions.
  // 
  //   Note that in real SC/MP systems this instruction is interlocked and is
  // intended to implement a semaphore in multiprocessor systems.  We don't
  // care about that here, of course.
  // 
  //   This doesn't affect the CY or OV flags!
  //--
  uint8_t bData = MEMR8(wAddr);
  m_A = bData+bAdd;
  MEMW8(wAddr, m_A);
}

void CSCMP3::TraceInstruction() const
{
  //++
  //   This routine will log the instruction that we're about to execute.
  // If tracing is not enabled, it does nothing ...
  //--
  if (!ISLOGGED(TRACE)) return;

  // Disassemble the opcode and fetch any operands ...
  string sCode;  uint8_t bOpcode, bData;  address_t wPC = GetPC();
  size_t nCount = Disassemble3(m_pMemory, wPC, sCode);
  bOpcode = m_pMemory->CPUread(wPC);
  if (nCount > 1) bData = m_pMemory->CPUread(INC16(wPC));

  // Print it out neatly ...
  if (nCount <= 1) {
    LOGF(TRACE, "%04X/ %02X      \t%s", wPC, bOpcode, sCode.c_str());
  } else {
    LOGF(TRACE, "%04X/ %02X %02X   \t%s", wPC, bOpcode, bData, sCode.c_str());
  }
}

uint64_t CSCMP3::DoExecute (uint8_t bOpcode)
{
  //++
  //   Execute the opcode and return the number of clock cycles used ...
  // We take the brute force (but fastest!) approach of just decoding all
  // 256 possible opcodes with one giant switch statement.  Note that the
  // instruction set for the INS807x is fairly sparse, so there are a lot
  // of inimplemented cases here!
  //--
  uint8_t b;  uint16_t w;
  switch (bOpcode) {

    // Opcodes $0x - Miscellaneous instructions ...
    case 0x00: 						 return  3;  // NOP
    case 0x01: b = m_A;  m_A = m_E;  m_E = b;		 return  5;  // XCH A,E
    //   0x02 - unimplemented
    //   0x03 - unimplemented
    //   0x04 - unimplemented
    //   0x05 - unimplemented
    case 0x06: m_A = GetStatus();			 return  3;  // LD A,S
    case 0x07: SetStatus(m_A);				 return  3;  // LD S,A
    case 0x08: Push16(GetEA());	        		 return  8;  // PUSH EA
    case 0x09: m_T = GetEA();				 return  4;  // LD T,EA
    case 0x0A: Push8(m_A);				 return  5;  // PUSH A
    case 0x0B: SetEA(m_T);				 return  4;  // LD EA,T
    case 0x0C: ShiftRightEA();				 return  4;  // SR EA
    case 0x0D: Divide();				 return 43;  // DIV EA,T
    case 0x0E: ShiftLeftA();				 return  3;  // SL A
    case 0x0F: ShiftLeftEA(); 				 return  4;  // SL EA

    // Opcodes $1x - CALL instructions ...
    case 0x10: ShortCall( 0);				 return 16;  // CALL 0
    case 0x11: ShortCall( 1);				 return 16;  // CALL 1
    case 0x12: ShortCall( 2);				 return 16;  // CALL 2
    case 0x13: ShortCall( 3);				 return 16;  // CALL 3
    case 0x14: ShortCall( 4);				 return 16;  // CALL 4
    case 0x15: ShortCall( 5);				 return 16;  // CALL 5
    case 0x16: ShortCall( 6);				 return 16;  // CALL 6
    case 0x17: ShortCall( 7);				 return 16;  // CALL 7
    case 0x18: ShortCall( 8);				 return 16;  // CALL 8
    case 0x19: ShortCall( 9);				 return 16;  // CALL 9
    case 0x1A: ShortCall(10);				 return 16;  // CALL 10
    case 0x1B: ShortCall(11);				 return 16;  // CALL 11
    case 0x1C: ShortCall(12);				 return 16;  // CALL 12
    case 0x1D: ShortCall(13);				 return 16;  // CALL 13
    case 0x1E: ShortCall(14);				 return 16;  // CALL 14
    case 0x1F: ShortCall(15);				 return 16;  // CALL 15

    // Opcodes $2x - register load immediate (including JSR/JMP) and miscellaneous ...
    case 0x20: w = IMM16();  Push16(m_PC);  m_PC = w;	 return 16;  // JSR addr
    //   0x21 - unimplemented
    case 0x22: Push16(m_P2);  m_P2 = IMM16();		 return 16;  // PLI P2, #addr16
    case 0x23: Push16(m_P3);  m_P3 = IMM16();		 return 16;  // PLI P3, #addr16
    case 0x24: m_PC = IMM16();				 return  9;  // JMP addr16
    case 0x25: m_SP = IMM16();				 return  9;  // LD SP, #data16
    case 0x26: m_P2 = IMM16();				 return  9;  // LD P2, #data16
    case 0x27: m_P3 = IMM16();				 return  9;  // LD P3, #data16
    //   0x28 - unimplemented
    //   0x29 - unimplemented
    //   0x2A - unimplemented
    //   0x2B - unimplemented
    case 0x2C: Multiply();				 return 37;  // MPY EA,T
    case 0x2D: return BranchNotDigit(REL8(m_PC+1));       	     // BND disp8[PC]
    case 0x2E: return SearchAndSkip(m_P2);			     // SSM P2
    case 0x2F: return SearchAndSkip(m_P3);			     // SSM P3

    // Opcodes $3x - load EA, shift/rotate A, and status register bit set/clear ...
    case 0x30: SetEA(m_PC);				 return  4;  // LD EA,PC
    case 0x31: SetEA(m_SP);				 return  4;  // LD EA,SP
    case 0x32: SetEA(m_P2);				 return  4;  // LD EA,P2
    case 0x33: SetEA(m_P3);				 return  4;  // LD EA,P3
    //   0x34 - unimplemented
    //   0x35 - unimplemented
    //   0x36 - unimplemented
    //   0x37 - unimplemented
    case 0x38: m_A = Pop8();				 return  6;  // POP A
    case 0x39: SetStatus(m_S & IMM8());			 return  5;  // AND S, #data8
    case 0x3A: SetEA(Pop16());				 return  9;  // POP EA
    case 0x3B: SetStatus(m_S | IMM8());			 return  5;  // OR S, #data8
    case 0x3C: ShiftRightA();				 return  3;  // SR A
    case 0x3D: ShiftRightAL();				 return  3;  // SRL A
    case 0x3E: RotateRightA();				 return  3;  // RR A
    case 0x3F: RotateRightAL(); 			 return  3;  // RRL A
    
    // Opcodes $4x - move EA to register, exchange EA with register ...
    case 0x40: m_A = m_E;				 return  4;  // LD A,E
    //   0x41 - unimplemented
    //   0x42 - unimplemented
    //   0x43 - unimplemented
    case 0x44: m_PC = GetEA();				 return  5;  // LD PC,EA
    case 0x45: m_SP = GetEA();				 return  5;  // LD SP,EA
    case 0x46: m_P2 = GetEA();				 return  5;  // LD P2,EA
    case 0x47: m_P3 = GetEA();				 return  5;  // LD P3,EA
    case 0x48: m_E = m_A;				 return  4;  // LD E,A
    //   0x49 - unimplemented
    //   0x4A - unimplemented
    //   0x4B - unimplemented
    case 0x4C: ExchangeEA(m_PC);			 return  7;  // XCH PC,EA
    case 0x4D: ExchangeEA(m_SP);			 return  7;  // XCH SP,EA
    case 0x4E: ExchangeEA(m_P2);			 return  7;  // XCH P2,EA
    case 0x4F: ExchangeEA(m_P3);			 return  7;  // XCH P3,EA

    // Opcodes $5x - PUSH/POP register (including RETurn) ... 
    case 0x50: m_A &= m_E;				 return  4;  // AND A,E
    //   0x51 - unimplemented
    //   0x52 - unimplemented
    //   0x53 - unimplemented
    case 0x54: Push16(m_PC);				 return  8;  // PUSH PC
    case 0x56: Push16(m_P2);				 return  8;  // PUSH P2
    case 0x57: Push16(m_P3);				 return  8;  // PUSH P3
    case 0x58: m_A |= m_E;				 return  4;  // OR A,E
    //   0x59 - unimplemented
    //   0x5A - unimplemented
    //   0x5B - unimplemented
    case 0x5C: m_PC = Pop16();				 return 10;  // RET
    //   0x5D - unimplemented
    case 0x5E: m_P2 = Pop16();				 return 10;  // POP P2
    case 0x5F: m_P3 = Pop16();				 return 10;  // POP P3

    // Opcodes $6x - branch if A positive/zero ...
    case 0x60: m_A ^= m_E;				 return  4;  // XOR A,E
    //   0x61 - unimplemented
    //   0x62 - unimplemented
    //   0x63 - unimplemented
    case 0x64: w=REL8(m_PC+1); if (!ISNEG8(m_A)) m_PC=w; return  5;  // BP disp8[PC]
    //   0x65 - unimplemented
    case 0x66: w=REL8(m_P2);   if (!ISNEG8(m_A)) m_PC=w; return  5;  // BP disp8[P2]
    case 0x67: w=REL8(m_P3);   if (!ISNEG8(m_A)) m_PC=w; return  5;  // BP disp8[P3]
    //   0x68 - unimplemented
    //   0x69 - unimplemented
    //   0x6A - unimplemented
    //   0x6B - unimplemented
    case 0x6C: w = REL8(m_PC+1); if (m_A == 0) m_PC = w; return  5;  // BZ disp8[PC]
    //   0x6D - unimplemented
    case 0x6E: w = REL8(m_P2);   if (m_A == 0) m_PC = w; return  5;  // BZ disp8[P2]
    case 0x6F: w = REL8(m_P3);   if (m_A == 0) m_PC = w; return  5;  // BZ disp8[P3]

    // Opcodes $7x - branch if A not zero, branch always ...
    case 0x70: AddA(m_E);				 return  4;  // ADD A,E
    //   0x71 - unimplemented
    //   0x72 - unimplemented
    //   0x73 - unimplemented
    case 0x74: m_PC = REL8(m_PC+1);			 return  5;  // BRA disp8[PC]
    //   0x75 - unimplemented
    case 0x76: m_PC = REL8(m_P2);			 return  5;  // BRA disp8[P2]
    case 0x77: m_PC = REL8(m_P3);			 return  5;  // BRA disp8[P3]
    case 0x78: AddA(-m_E);				 return  4;  // SUB A,E
    //   0x79 - unimplemented
    //   0x7A - unimplemented
    //   0x7B - unimplemented
    case 0x7C: w = REL8(m_PC+1); if (m_A != 0) m_PC = w; return  5;  // BNZ disp8[PC]
    //   0x7D - unimplemented
    case 0x7E: w = REL8(m_P2);   if (m_A != 0) m_PC = w; return  5;  // BNZ disp8[P2]
    case 0x7F: w = REL8(m_P3);   if (m_A != 0) m_PC = w; return  5;  // BNZ disp8[P3]

    // Opcodes $8x - load/store EA ...
    case 0x80: SetEA(MEMR16(REL8(m_PC+1)));		 return 10;  // LD EA,disp8[PC]
    case 0x81: SetEA(MEMR16(REL8(m_SP)));		 return 10;  // LD EA,disp8[SP]
    case 0x82: SetEA(MEMR16(REL8(m_P2)));		 return 10;  // LD EA,disp8[P2]
    case 0x83: SetEA(MEMR16(REL8(m_P3)));		 return 10;  // LD EA,disp8[P3]
    case 0x84: SetEA(IMM16());				 return  8;  // LD EA,#data16
    case 0x85: SetEA(MEMR16(DIRECT(IMM8())));		 return 10;  // LD EA,$FFxx
    case 0x86: SetEA(MEMR16(AUTO(m_P2,IMM8())));	 return 12;  // LD EA,@disp8[P2]
    case 0x87: SetEA(MEMR16(AUTO(m_P3,IMM8())));	 return 12;  // LD EA,@disp8[P3]
    case 0x88: MEMW16(REL8(m_PC+1), GetEA());		 return 10;  // ST EA,disp8[PC]
    case 0x89: MEMW16(REL8(m_SP),   GetEA());		 return 10;  // ST EA,disp8[SP]
    case 0x8A: MEMW16(REL8(m_P2),   GetEA());		 return 10;  // ST EA,disp8[P2]
    case 0x8B: MEMW16(REL8(m_P3),   GetEA());		 return 10;  // ST EA,disp8[P3]
    //   0x8C - unimplemented (would be store EA immediate!)
    case 0x8D: MEMW16(DIRECT(IMM8()), GetEA()); 	 return 10;  // ST EA,$FFxx
    case 0x8E: MEMW16(AUTO(m_P2,IMM8()), GetEA());	 return 12;  // ST EA,@disp8[P2]
    case 0x8F: MEMW16(AUTO(m_P3,IMM8()), GetEA());	 return 12;  // ST EA,@disp8[P3]

    // Opcodes $9x - increment/decmrement memory and load ...
    case 0x90: AddMemory(REL8(m_PC+1), 1);		 return  8;  // ILD A,disp8[PC]
    case 0x91: AddMemory(REL8(m_SP),   1);		 return  8;  // ILD A,disp8[SP]
    case 0x92: AddMemory(REL8(m_P2),   1);		 return  8;  // ILD A,disp8[P2]
    case 0x93: AddMemory(REL8(m_P3),   1);		 return  8;  // ILD A,disp8[P3]
    //   0x94 - unimplemented
    case 0x95: AddMemory(DIRECT(IMM8()), 1);		 return  8;  // ILD A,$FFxx
    case 0x96: AddMemory(AUTO(m_P2,IMM8()), 1);		 return 10;  // ILD A,@disp8[P2]
    case 0x97: AddMemory(AUTO(m_P3,IMM8()), 1);		 return 10;  // ILD A,@disp8[P3]
    case 0x98: AddMemory(REL8(m_PC+1), -1);		 return  8;  // DLD A,disp8[PC]
    case 0x99: AddMemory(REL8(m_SP),   -1);		 return  8;  // DLD A,disp8[SP]
    case 0x9A: AddMemory(REL8(m_P2),   -1);		 return  8;  // DLD A,disp8[P2]
    case 0x9B: AddMemory(REL8(m_P3),   -1);		 return  8;  // DLD A,disp8[P3]
    //   0x9C - unimplemented
    case 0x9D: AddMemory(DIRECT(IMM8()), -1);		 return  8;  // DLD A,$FFxx
    case 0x9E: AddMemory(AUTO(m_P2,IMM8()), -1);	 return 10;  // DLD A,@disp8[P2]
    case 0x9F: AddMemory(AUTO(m_P3,IMM8()), -1);	 return 10;  // DLD A,@disp8[P3]

    // Opcodes $Ax - load T ...
    case 0xA0: m_T = MEMR16(REL8(m_PC+1));		 return 10;  // LD T,disp8[PC]
    case 0xA1: m_T = MEMR16(REL8(m_SP));		 return 10;  // LD T,disp8[SP]
    case 0xA2: m_T = MEMR16(REL8(m_P2));		 return 10;  // LD T,disp8[P2]
    case 0xA3: m_T = MEMR16(REL8(m_P3));		 return 10;  // LD T,disp8[P3]
    case 0xA4: m_T = IMM16();				 return  8;  // LD T,#data16
    case 0xA5: m_T = MEMR16(DIRECT(IMM8()));		 return 10;  // LD T,$FFxx
    case 0xA6: m_T = MEMR16(AUTO(m_P2,IMM8()));		 return 12;  // LD T,@disp8[P2]
    case 0xA7: m_T = MEMR16(AUTO(m_P3,IMM8()));		 return 12;  // LD T,@disp8[P3]
    //   0xA8 - unimplemented
    //   0xA9 - unimplemented
    //   0xAA - unimplemented
    //   0xAB - unimplemented
    //   0xAC - unimplemented
    //   0xAD - unimplemented
    //   0xAE - unimplemented
    //   0xAF - unimplemented

    // Opcodes $Bx - Add/subtract to/from EA ...
    case 0xB0: AddEA(MEMR16(REL8(m_PC+1)));		 return 10;  // ADD EA,disp8[PC]
    case 0xB1: AddEA(MEMR16(REL8(m_SP)));		 return 10;  // ADD EA,disp8[SP]
    case 0xB2: AddEA(MEMR16(REL8(m_P2)));		 return 10;  // ADD EA,disp8[P2]
    case 0xB3: AddEA(MEMR16(REL8(m_P3)));		 return 10;  // ADD EA,disp8[P3]
    case 0xB4: AddEA(IMM16()); 			         return  8;  // ADD EA,#data16
    case 0xB5: AddEA(MEMR16(DIRECT(IMM8())));		 return 10;  // ADD EA,$FFxx
    case 0xB6: AddEA(MEMR16(AUTO(m_P2,IMM8())));	 return 12;  // ADD EA,@disp8[P2]
    case 0xB7: AddEA(MEMR16(AUTO(m_P3,IMM8())));	 return 12;  // ADD EA,@disp8[P3]
    case 0xB8: AddEA(-MEMR16(REL8(m_PC+1)));		 return 10;  // SUB EA,disp8[PC]
    case 0xB9: AddEA(-MEMR16(REL8(m_SP)));		 return 10;  // SUB EA,disp8[SP]
    case 0xBA: AddEA(-MEMR16(REL8(m_P2)));		 return 10;  // SUB EA,disp8[P2]
    case 0xBB: AddEA(-MEMR16(REL8(m_P3)));		 return 10;  // SUB EA,disp8[P3]
    case 0xBC: AddEA(-IMM16());   	 	 	 return  8;  // SUB EA,#data16
    case 0xBD: AddEA(-MEMR16(DIRECT(IMM8())));	 	 return 10;  // SUB EA,$FFxx
    case 0xBE: AddEA(-MEMR16(AUTO(m_P2,IMM8())));	 return 12;  // SUB EA,@disp8[P2]
    case 0xBF: AddEA(-MEMR16(AUTO(m_P3,IMM8())));	 return 12;  // SUB EA,@disp8[P3]

    // Opcodes $Cx - load/store A ...
    case 0xC0: m_A = MEMR8(REL8(m_PC+1));			 return  7;  // LD A,disp8[PC]
    case 0xC1: m_A = MEMR8(REL8(m_SP));			 return  7;  // LD A,disp8[SP]
    case 0xC2: m_A = MEMR8(REL8(m_P2));			 return  7;  // LD A,disp8[P2]
    case 0xC3: m_A = MEMR8(REL8(m_P3));			 return  7;  // LD A,disp8[P3]
    case 0xC4: m_A = IMM8();				 return  5;  // LD A,#data8
    case 0xC5: m_A = MEMR8(DIRECT(IMM8()));		 return  7;  // LD A,$FFxx
    case 0xC6: m_A = MEMR8(AUTO(m_P2,IMM8()));		 return  9;  // LD A,@disp8[P2]
    case 0xC7: m_A = MEMR8(AUTO(m_P3,IMM8()));		 return  9;  // LD A,@disp8[P3]
    case 0xC8: MEMW8(REL8(m_PC+1), m_A);		 return  7;  // ST A,disp8[PC]
    case 0xC9: MEMW8(REL8(m_SP),   m_A);		 return  7;  // ST A,disp8[SP]
    case 0xCA: MEMW8(REL8(m_P2),   m_A);		 return  7;  // ST A,disp8[P2]
    case 0xCB: MEMW8(REL8(m_P3),   m_A);		 return  7;  // ST A,disp8[P3]
    //   0xCC - unimplemented (would be store A immediate!)
    case 0xCD: MEMW8(DIRECT(IMM8()), m_A);		 return  7;  // ST A,$FFxx
    case 0xCE: MEMW8(AUTO(m_P2,IMM8()), m_A);		 return  9;  // ST A,@disp8[P2]
    case 0xCF: MEMW8(AUTO(m_P3,IMM8()), m_A);		 return  9;  // ST A,@disp8[P3]

    // Opcodes $Dx - AND/OR with A ...
    case 0xD0: m_A &= MEMR8(REL8(m_PC+1));		 return  7;  // AND A,disp8[PC]
    case 0xD1: m_A &= MEMR8(REL8(m_SP));		 return  7;  // AND A,disp8[SP]
    case 0xD2: m_A &= MEMR8(REL8(m_P2));		 return  7;  // AND A,disp8[P2]
    case 0xD3: m_A &= MEMR8(REL8(m_P3));		 return  7;  // AND A,disp8[P3]
    case 0xD4: m_A &= IMM8();				 return  5;  // AND A,#data8
    case 0xD5: m_A &= MEMR8(DIRECT(IMM8()));		 return  7;  // AND A,$FFxx
    case 0xD6: m_A &= MEMR8(AUTO(m_P2,IMM8()));		 return  9;  // AND A,@disp8[P2]
    case 0xD7: m_A &= MEMR8(AUTO(m_P3,IMM8()));		 return  9;  // AND A,@disp8[P3]
    case 0xD8: m_A |= MEMR8(REL8(m_PC+1));		 return  7;  // OR A,disp8[PC]
    case 0xD9: m_A |= MEMR8(REL8(m_SP));		 return  7;  // OR A,disp8[SP]
    case 0xDA: m_A |= MEMR8(REL8(m_P2));		 return  7;  // OR A,disp8[P2]
    case 0xDB: m_A |= MEMR8(REL8(m_P3));		 return  7;  // OR A,disp8[P3]
    case 0xDC: m_A |= IMM8();				 return  5;  // OR A,#data8
    case 0xDD: m_A |= MEMR8(DIRECT(IMM8()));		 return  7;  // OR A,$FFxx
    case 0xDE: m_A |= MEMR8(AUTO(m_P2,IMM8()));		 return  9;  // OR A,@disp8[P2]
    case 0xDF: m_A |= MEMR8(AUTO(m_P3,IMM8()));		 return  9;  // OR A,@disp8[P3]

    // Opcodes $Ex - Exclusive OR with A ...
    case 0xE0: m_A ^= MEMR8(REL8(m_PC+1));		 return  7;  // XOR A,disp8[PC]
    case 0xE1: m_A ^= MEMR8(REL8(m_SP));		 return  7;  // XOR A,disp8[SP]
    case 0xE2: m_A ^= MEMR8(REL8(m_P2));		 return  7;  // XOR A,disp8[P2]
    case 0xE3: m_A ^= MEMR8(REL8(m_P3));		 return  7;  // XOR A,disp8[P3]
    case 0xE4: m_A ^= IMM8();				 return  5;  // XOR A,#data8
    case 0xE5: m_A ^= MEMR8(DIRECT(IMM8()));		 return  7;  // XOR A,$FFxx
    case 0xE6: m_A ^= MEMR8(AUTO(m_P2,IMM8()));		 return  9;  // XOR A,@disp8[P2]
    case 0xE7: m_A ^= MEMR8(AUTO(m_P3,IMM8()));		 return  9;  // XOR A,@disp8[P3]
    //   0xE8 - unimplemented
    //   0xE9 - unimplemented
    //   0xEA - unimplemented
    //   0xEB - unimplemented
    //   0xEC - unimplemented
    //   0xED - unimplemented
    //   0xEE - unimplemented
    //   0xEF - unimplemented

    // Opcodes $Fx Add/subtract to/from A ...
    case 0xF0: AddA(MEMR8(REL8(m_PC+1))); 		 return  7;  // ADD A,disp8[PC]
    case 0xF1: AddA(MEMR8(REL8(m_SP))); 		 return  7;  // ADD A,disp8[SP]
    case 0xF2: AddA(MEMR8(REL8(m_P2))); 		 return  7;  // ADD A,disp8[P2]
    case 0xF3: AddA(MEMR8(REL8(m_P3))); 		 return  7;  // ADD A,disp8[P3]
    case 0xF4: AddA(IMM8());				 return  5;  // ADD A,#data8
    case 0xF5: AddA(MEMR8(DIRECT(IMM8())));		 return  7;  // ADD A,$FFxx
    case 0xF6: AddA(MEMR8(AUTO(m_P2,IMM8())));		 return  9;  // ADD A,@disp8[P2]
    case 0xF7: AddA(MEMR8(AUTO(m_P3,IMM8())));		 return  9;  // ADD A,@disp8[P3]
    case 0xF8: AddA(-MEMR8(REL8(m_PC+1))); 		 return  7;  // SUB A,disp8[PC]
    case 0xF9: AddA(-MEMR8(REL8(m_SP))); 		 return  7;  // SUB A,disp8[SP]
    case 0xFA: AddA(-MEMR8(REL8(m_P2))); 		 return  7;  // SUB A,disp8[P2]
    case 0xFB: AddA(-MEMR8(REL8(m_P3))); 		 return  7;  // SUB A,disp8[P3]
    case 0xFC: AddA(-IMM8());				 return  5;  // SUB A,#data8
    case 0xFD: AddA(-MEMR8(DIRECT(IMM8())));		 return  7;  // SUB A,$FFxx
    case 0xFE: AddA(-MEMR8(AUTO(m_P2,IMM8())));		 return  9;  // SUB A,@disp8[P2]
    case 0xFF: AddA(-MEMR8(AUTO(m_P3,IMM8())));		 return  9;  // SUB A,@disp8[P3]

    // Everything else is invalid!
    default:
      IllegalOpcode();  return 4;
  }
}

CCPU::STOP_CODE CSCMP3::Run (uint32_t nCount)
{
  //++
  //   This is the main "engine" of the SC/MP emulator.  The UI code is	expected
  // to call it whenever the user gives a START, GO, STEP, etc command and it
  // will execute SC/MP instructions until it either a)	executes the number of
  // instructions specified by nCount, or b) some condition arises to interrupt
  // the simulation such as an illegal opcode or I/O, the user entering the escape
  // sequence on the console, etc.  If nCount is zero on entry, then we will run
  // forever until one of the previously mentioned break conditions occurs.	
  //--
  bool fFirst = true;
  m_nStopCode = STOP_NONE;
  while (m_nStopCode == STOP_NONE) {

    // If any device events need to happen, now is the time...
    m_pEvents->DoEvents();

    // See if an interrupt is required ...
    // TBA TODO NYI - SUPPRESS INTERRUPTS FOR ONE INSTRUCTION AFTER IE IS SET!!??!
    if ((m_pInterrupt != NULL) && ISSET(m_S, SR_IE))  DoInterrupt();

    // Stop if we've hit a breakpoint ...
    if (!fFirst && m_pMemory->IsBreak(GetPC())) {
      m_nStopCode = STOP_BREAKPOINT;  break;
    } else
      fFirst = false;

    // If tracing is on, then log the instruction we're about to execute.
    if (ISLOGGED(TRACE)) TraceInstruction();

    // Fetch, decode and execute an instruction...
    //   Note that the SC/MP is super weird - it increments the PC _before_
    // fetching the opcode, not after!!
    m_nLastPC = GetPC();
    uint8_t bOpcode = MEMR8(INC16(m_PC));
    AddCycles(DoExecute(bOpcode));

    // Check for some termination conditions ...
    if (m_nStopCode == STOP_NONE) {
      // Terminate if we've executed enough instructions ... 
      if ((nCount > 0)  &&  (--nCount == 0))  m_nStopCode = STOP_FINISHED;
    }
  }

  return m_nStopCode;
}

unsigned CSCMP3::GetRegisterSize (cpureg_t nReg) const
{
  //++
  //   This method returns the size of a given register, IN BITS!  It's
  // used only by the UI, to figure out how to print and mask register
  // values...
  //--
  switch (nReg) {
    case REG_PC:  case REG_SP:  case REG_P2:  case REG_P3:  case REG_T:
      return 16;
    case REG_A:  case REG_E:  case REG_S:
      return 8;
    default:
      return 0;
  }
}

uint16_t CSCMP3::GetRegister (cpureg_t nReg) const
{
  //++
  // This method will return the contents of an internal CPU register ...
  //--
  switch (nReg) {
    case REG_A:  return m_A;
    case REG_E:  return m_E;
    case REG_S:  return m_S;
    case REG_PC: return m_PC;
    case REG_SP: return m_SP;
    case REG_P2: return m_P2;
    case REG_P3: return m_P3;
    case REG_T:  return m_T;
    default:
      return 0;
  }
}

void CSCMP3::SetRegister (cpureg_t nReg, uint16_t wVal)
{
  //++
  // Change the contents of an internal CPU register ...
  //--
  switch (nReg) {
    case REG_A:  m_A = MASK8(wVal);       break;
    case REG_E:  m_E = MASK8(wVal);       break;
    case REG_S:  SetStatus(MASK8(wVal));  break;
    case REG_PC: m_PC = wVal;             break;
    case REG_SP: m_SP = wVal;             break;
    case REG_P2: m_P2 = wVal;             break;
    case REG_P3: m_P3 = wVal;             break;
    case REG_T:  m_T  = wVal;             break;
    default:
      break;
  }
}
