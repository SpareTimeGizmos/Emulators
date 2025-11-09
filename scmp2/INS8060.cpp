//++
//INS8060.cpp - National INS8050/INS8060 SC/MP microprocessor emulation
//
// DESCRIPTION:
//   This module implements an emulation of the the two original National SC/MP
// microprocessors, the INS8050 (aka SC/MP or SC/MP-I) and the INS8060 (SC/MP-II).
// The INS8050 and INS8060 are, as far as I know, identical as far as the software
// goes.  The instructions sets are the same, and they are 100% binary compatible.
// The only differences are in the manufacturing process - the INS8050 is a PMOS
// chip and requires two supply voltages, and the INS8060 is NMOS needs only a
// single 5V supply.  The INS8060 also can run at twice the clock frequency of
// the INS8050.
// 
//   The SC/MP has several "oddities" that bear mention -
//
//    * All address arithmetic is done with 12 bits only.  The upper 4 bits
//    of the address are fixed.  This applies to things like calculating the
//    EA, and also to incrementing the PC.
//
//    * Speaking of the PC, the SC/MP incremements the PC _before_ every fetch,
//    not after.  That has lots of subtle consequences; for example, the
//    address you jump to should be one LESS than where you want to go!
//
//    * The SENSE A input is also the interrupt request, if interrupts are
//    enabled.
//
//    * There are no separate "ADD" and "ADD with carry" instructions.  The one
//    and only binary ADD instruction always includes a carry in.  If you don't
//    want that, then you have to clear carry first.
//
// REVISION HISTORY:
// 13-FEB-20  RLA  New file.
// 22-JUN-22  RLA  Add nSense and nFlag parameters to GetSense() and SetFlag()
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
#include "SCMP2.hpp"            // global declarations for this project
#include "Interrupt.hpp"        // interrupt simulation logic
#include "EventQueue.hpp"       // I/O device event queue simulation
#include "INS8060opcodes.hpp"   // COSMAC opcode definitions
#include "CPU.hpp"              // CCPU base class definitions
#include "Memory.hpp"           // CMemory memory emulation object
#include "Device.hpp"           // CDevice I/O device emulation objects
#include "INS8060.hpp"          // declarations for this module

// Internal CPU register names for GetRegisterNames() ...
const CCmdArgKeyword::KEYWORD CSCMP2::g_keysRegisters[] = {
  {"PC", CSCMP2::REG_PC}, {"P1", CSCMP2::REG_P1}, {"P2", CSCMP2::REG_P2},
  {"P3", CSCMP2::REG_P3}, {"AC", CSCMP2::REG_AC}, {"E",  CSCMP2::REG_EX},
  {"SR", CSCMP2::REG_SR}, {NULL, 0}
};

// Names for the sense inputs and flag outputs ...
const char *CSCMP2::g_apszSenseNames[MAXSENSE] = {
  "SENSEA", "SENSEB", "SIN"
};
const char *CSCMP2::g_apszFlagNames[MAXFLAG] = {
  "FLAG0", "FLAG1", "FLAG2", "SOUT"
};


CSCMP2::CSCMP2 (CMemory *pMemory, CEventQueue *pEvents, CInterrupt *pInterrupt)
  : CCPU(pMemory, pEvents, pInterrupt)
{
  //++
  //--
  SetCrystalFrequency(DEFAULT_CRYSTAL);
  CSCMP2::ClearCPU();
}

void CSCMP2::ClearCPU()
{
  //++
  // This routine resets the SC/MP to a power on state.
  //--
  CCPU::ClearCPU();
  m_P[0] = m_P[1] = m_P[2] = m_P[3] = 0;
  m_AC = m_EX = m_SR = 0;
  UpdateFlag(FLAG0, 0);  UpdateFlag(FLAG1, 0);
  UpdateFlag(FLAG2, 0);  UpdateFlag(SOUT, 0);
}

void CSCMP2::UpdateFlag (uint16_t nFlag, uint1_t bNew)
{
  //++
  // Update a flag output and handle any software serial emulation ...
  //--
  SetFlag(nFlag, MASK1(bNew));
}

uint1_t CSCMP2::UpdateSense (uint16_t nSense)
{
  //++
  //   This routine is called whenever an sense input is tested by the CPU.
  // It will check to see if an installed device is connected to this input
  // and, if one is, query the device for the current state of this input.
  //--
  assert(nSense < MAXSENSE);
  uint1_t bData = GetSense(nSense);
  if (nSense == SENSEA) {
    CLRBIT(m_SR, SR_SA);  if (bData != 0) SETBIT(m_SR, SR_SA);
  } else if (nSense == SENSEB) {
    CLRBIT(m_SR, SR_SB);  if (bData != 0) SETBIT(m_SR, SR_SB);
  }
  return bData;
}

void CSCMP2::SetStatus (uint8_t bData)
{
  //++
  // Load the status register and update all flags ...
  //--
  bool fOldF0 = ISSET(m_SR, SR_F0), fNewF0 = ISSET(bData, SR_F0);
  bool fOldF1 = ISSET(m_SR, SR_F1), fNewF1 = ISSET(bData, SR_F1);
  bool fOldF2 = ISSET(m_SR, SR_F2), fNewF2 = ISSET(bData, SR_F2);
  m_SR = (m_SR & (SR_SA|SR_SB)) | (bData & ~(SR_SA|SR_SB));
  if (fOldF0 != fNewF0) UpdateFlag(FLAG0, fNewF0 ? 1 : 0);
  if (fOldF1 != fNewF1) UpdateFlag(FLAG1, fNewF1 ? 1 : 0);
  if (fOldF2 != fNewF2) UpdateFlag(FLAG2, fNewF2 ? 1 : 0);
}

uint8_t CSCMP2::GetStatus()
{
  //++
  // Return the current status byte, but update all sense inputs first ...
  //--
  UpdateSense(SENSEA);  UpdateSense(SENSEB);
  return m_SR;
}

void CSCMP2::DoInterrupt()
{
  //++
  //   If the SENSE A input is high AND the interrupt enable bit is set, then
  // simulate an XPPC(P3) instruction (and clear the IE bit so we don't hang
  // forever!).  Tthis takes a total of 13 clocks - 6 for the interrupt
  // acknowledge, and another 7 for the XPPC.
  //--
  if (ISSET(m_SR, SR_IE) && (UpdateSense(0) != 0)) {
    CLRBIT(m_SR, SR_IE);
    XPPC(3);
    AddTime(13ULL*m_qMicrocycleTime);
    LOGF(TRACE, "INTERRUPTED - old PC=0x%04X, new PC=0x%04X", m_P[REG_P3], GetPC());
  }
}

address_t CSCMP2::CalculateEA (uint2_t p, bool fAuto)
{
  //++
  //   Calculate an effective address for this instruction, after first
  // fetching the displacement byte from memory.  P is the pointer to be used
  // as the base register, and fAuto indicates whether auto-increment should
  // occur.
  //
  // Remember that offset == -128 means to use the E register instead!
  //--
  uint8_t bDisp = LoadImmediate();
  if (bDisp == 0x80) bDisp = m_EX;
  if (fAuto) {
    if (ISSET(bDisp, 0x80)) {
      // Autoindex displacement is negative, so pre-decrement ...
      address_t wEA = (m_P[p] & 0xF000) | ((m_P[p]+MKWORD(0xFF, bDisp)) & 0x0FFF);
      return m_P[p] = wEA;
    } else {
      // Autoindex displacement is positive, so post-increment ...
      address_t wEA = m_P[p];
      m_P[p] = (wEA & 0xF000) | ((wEA+bDisp) & 0x0FFF);
      return wEA;
    }
  } else {
    // Not autoindexed - don't change the pointer register ...
    return (m_P[p] & 0xF000) | ((m_P[p]+SEXT(bDisp)) & 0x0FFF);
  }
}

uint8_t CSCMP2::AddMemory (address_t wEA, int8_t bAdd)
{
  //++
  //   Add a signed (!!) constant to a memory location, and update memory with
  // the new value.  Return the new value also.  This is used by the ILD and
  // DLD instructions ...
  //--
  uint8_t bData = m_pMemory->CPUread(wEA);
  bData += bAdd;
  m_pMemory->CPUwrite(wEA, bData);
  return bData;
}

uint64_t CSCMP2::JMP (bool fJump, address_t wTarget)
{
  //++
  //   If fJump is true, then update the PC with the target address.  If fJump
  // is false, then do nothing.  In either case, return the number of clock
  // cycles required - it's always 11 for a jump taken, or 9 for no jump.
  //
  //   Note that the PC is updated with all 16 bits of the EA - this is one of
  // those special cases where the SC/MP does not use 12 bit address arithmetic!
  //--
  if (fJump) {
    m_P[REG_PC] = wTarget;  return 11;
  } else
    return 9;
}

uint8_t CSCMP2::ADD (uint8_t bOp1, uint8_t bOp2)
{
  //++
  //   Do a binary addition of two values and return the result.  The CY/L and
  // OV flags are also updated as a result.   Presumably the first operand is
  // the AC and the result will be stored in the AC, but this doesn't have to
  // be the case.
  //
  //   BTW, note that on the SC/MP, ALL additions are "with carry".  The CY/L
  // flag is always an input to the addition!
  //--
  uint16_t wResult = bOp1 + bOp2 + (ISSET(m_SR, SR_CYL) ? 1 : 0);
  if (HIBYTE(wResult) != 0) SETBIT(m_SR, SR_CYL);  else CLRBIT(m_SR, SR_CYL);
  //  There are two cases that result in a twos complement overflow - if we
  // add two positive numbers and get a negative result, or if we add two
  // negative numbers and get a positive result.  Any other combination can
  // never overflow.
  CLRBIT(m_SR, SR_OV);
  if (   (!ISSET(bOp1, 0x80) && !ISSET(bOp2, 0x80) &&  ISSET(wResult, 0x80))
      || ( ISSET(bOp1, 0x80) &&  ISSET(bOp2, 0x80) && !ISSET(wResult, 0x80)))
    SETBIT(m_SR, SR_OV);
  return LOBYTE(wResult);
}

uint8_t CSCMP2::DADD (uint8_t bOp1, uint8_t bOp2)
{
  //++
  //  Do a BCD addition of two values and return the result.  The CY/L flag
  // will be updated as a result, however (unlike binary ADD()) the overflow
  // flag is not affected.
  //
  //   Note that this is a rather cheesy way to implement a BCD addition
  // (convert to straight binary, add, and then convert back to BCD!), but
  // it works...
  //--
  bOp1 = HINIBBLE(bOp1)*10 + LONIBBLE(bOp1);
  bOp2 = HINIBBLE(bOp2)*10 + LONIBBLE(bOp2);
  uint16_t wResult = bOp1 + bOp2;
  if (wResult > 99) SETBIT(m_SR, SR_CYL);  else CLRBIT(m_SR, SR_CYL);
  return MKBYTE((wResult/10), (wResult%10));
}

uint64_t CSCMP2::Delay (uint8_t bData)
{
  //++
  //   This executes the DLY (delay) instruction and will return the number
  // of clock cycles that we should delay for.  It's rather arcane, but this
  // formula is straight out of the National manual.
  //
  //   BTW, it's not documented but it appears that this instruction always
  // leaves the AC set to 0xFF!
  //--
  uint64_t llDelay = 13 + 2*m_AC + (2+512)*bData;
  m_AC = 0xFF;
  return llDelay;
}

void CSCMP2::SIO()
{
  //++
  //   This implements the serial I/O instruction.  The LSB of the E register
  // (NOT the AC!) goes to the serial output; E is shifted right by one bit,
  // and the serial input goes to the MSB of E.
  //--
  UpdateFlag(SOUT, (m_EX & 1));
  m_EX >>= 1;
  if (UpdateSense(SIN) != 0) m_EX |= 0x80;
}

void CSCMP2::TraceInstruction() const
{
  //++
  //   This routine will log the instruction that we're about to execute.
  // If tracing is not enabled, it does nothing ...
  //--
  if (!ISLOGGED(TRACE)) return;

  // Disassemble the opcode and fetch any operands ...
  string sCode;  uint8_t bOpcode, bData;  address_t wPC = GetPC();
  size_t nCount = Disassemble2(m_pMemory, wPC, sCode);
  bOpcode = m_pMemory->CPUread(wPC);
  if (nCount > 1) bData = m_pMemory->CPUread(INC12(wPC));

  // Print it out neatly ...
  if (nCount <= 1) {
    LOGF(TRACE, "%04X/ %02X      \t%s", wPC, bOpcode, sCode.c_str());
  } else {
    LOGF(TRACE, "%04X/ %02X %02X   \t%s", wPC, bOpcode, bData, sCode.c_str());
  }
}

uint64_t CSCMP2::DoExecute(uint8_t bOpcode)
{
  //++
  // Execute the opcode and return the number of clock cycles used ...
  //--
  switch (bOpcode) {
    // Miscellaneous instructions ...
    case 0x00: m_nStopCode = STOP_HALT;                   return  8;  // HALT
    case 0x01: XAE();                                     return  7;  // XAE
    case 0x02: CLRBIT(m_SR, SR_CYL);                      return  5;  // CCL
    case 0x03: SETBIT(m_SR, SR_CYL);                      return  5;  // SCL
    case 0x04: CLRBIT(m_SR, SR_IE);                       return  6;  // DINT
    case 0x05: SETBIT(m_SR, SR_IE);                       return  6;  // IEN
    case 0x06: m_AC = GetStatus();                        return  5;  // CSA
    case 0x07: SetStatus(m_AC);                           return  6;  // CAS
    case 0x08:                                            return  5;  // NOP
    case 0x8f: return Delay(LoadImmediate());                         // DLY count

    // Shift and rotate instructions ...
    case 0x19: SIO();                                     return  5;  // SIO
    case 0x1c: m_AC = SR(m_AC);                           return  5;  // SR
    case 0x1d: m_AC = SRL(m_AC);                          return  5;  // SRL
    case 0x1e: m_AC = RR(m_AC);                           return  5;  // RR
    case 0x1f: m_AC = RRL(m_AC);                          return  5;  // RRL

    // Pointer register instructions ...
    case 0x30: XPAL(REG_PC);                              return  8;  // XPAL PC
    case 0x31: XPAL(REG_P1);                              return  8;  // XPAL P1
    case 0x32: XPAL(REG_P2);                              return  8;  // XPAL P2
    case 0x33: XPAL(REG_P3);                              return  8;  // XPAL P3
    case 0x34: XPAH(REG_PC);                              return  8;  // XPAH PC
    case 0x35: XPAH(REG_P1);                              return  8;  // XPAH P1
    case 0x36: XPAH(REG_P2);                              return  8;  // XPAH P2
    case 0x37: XPAH(REG_P3);                              return  8;  // XPAH P3
    case 0x3c:                                            return  7;  // XPPC PC - NOP???
    case 0x3d: XPPC(REG_P1);                              return  7;  // XPPC P1
    case 0x3e: XPPC(REG_P2);                              return  7;  // XPPC P2
    case 0x3f: XPPC(REG_P3);                              return  7;  // XPPC P3

    // Extension register instructions ...
    case 0x40: m_AC  = m_EX;                              return  6;  // LDE
    case 0x50: m_AC &= m_EX;                              return  6;  // ANE
    case 0x58: m_AC |= m_EX;                              return  6;  // ORE
    case 0x60: m_AC ^= m_EX;                              return  6;  // XRE
    case 0x68: m_AC = DADD(m_AC, m_EX);                   return 11;  // DAE
    case 0x70: m_AC =  ADD(m_AC, m_EX);                   return  7;  // ADE
    case 0x78: m_AC =  ADD(m_AC,~m_EX);                   return  8;  // CAE

    // Transfer instructions ...
    case 0x90: return JMP(true,               CalculateEA(REG_PC));   // JMP disp(PC)
    case 0x91: return JMP(true,               CalculateEA(REG_P1));   // JMP disp(P1)
    case 0x92: return JMP(true,               CalculateEA(REG_P2));   // JMP disp(P2)
    case 0x93: return JMP(true,               CalculateEA(REG_P3));   // JMP disp(P3)
    case 0x94: return JMP(!ISSET(m_AC, 0x80), CalculateEA(REG_PC));   // JP  disp(PC)
    case 0x95: return JMP(!ISSET(m_AC, 0x80), CalculateEA(REG_P1));   // JP  disp(P1)
    case 0x96: return JMP(!ISSET(m_AC, 0x80), CalculateEA(REG_P2));   // JP  disp(P2)
    case 0x97: return JMP(!ISSET(m_AC, 0x80), CalculateEA(REG_P3));   // JP  disp(P3)
    case 0x98: return JMP((m_AC == 0),        CalculateEA(REG_PC));   // JZ  disp(PC)
    case 0x99: return JMP((m_AC == 0),        CalculateEA(REG_P1));   // JZ  disp(P1)
    case 0x9a: return JMP((m_AC == 0),        CalculateEA(REG_P2));   // JZ  disp(P2)
    case 0x9b: return JMP((m_AC == 0),        CalculateEA(REG_P3));   // JZ  disp(P3)
    case 0x9c: return JMP((m_AC != 0),        CalculateEA(REG_PC));   // JNZ disp(PC)
    case 0x9d: return JMP((m_AC != 0),        CalculateEA(REG_P1));   // JNZ disp(P1)
    case 0x9e: return JMP((m_AC != 0),        CalculateEA(REG_P2));   // JNZ disp(P2)
    case 0x9f: return JMP((m_AC != 0),        CalculateEA(REG_P3));   // JNZ disp(P3)

    // Memory increment/decrement instructions ...
    case 0xa8: m_AC = AddMemory(CalculateEA(REG_PC),  1); return 22;  // ILD disp(PC)
    case 0xa9: m_AC = AddMemory(CalculateEA(REG_P1),  1); return 22;  // ILD disp(P1)
    case 0xaa: m_AC = AddMemory(CalculateEA(REG_P2),  1); return 22;  // ILD disp(P2)
    case 0xab: m_AC = AddMemory(CalculateEA(REG_P3),  1); return 22;  // ILD disp(P3)
    case 0xb8: m_AC = AddMemory(CalculateEA(REG_PC), -1); return 22;  // DLD disp(PC)
    case 0xb9: m_AC = AddMemory(CalculateEA(REG_P1), -1); return 22;  // DLD disp(P1)
    case 0xba: m_AC = AddMemory(CalculateEA(REG_P2), -1); return 22;  // DLD disp(P2) 
    case 0xbb: m_AC = AddMemory(CalculateEA(REG_P3), -1); return 22;  // DLD disp(P3)

    // Memory Reference instructions - LOAD ...
    case 0xc0: m_AC = Load(REG_PC);                       return 18;  // LD disp(PC)
    case 0xc1: m_AC = Load(REG_P1);                       return 18;  // LD disp(P1)
    case 0xc2: m_AC = Load(REG_P2);                       return 18;  // LD disp(P2)
    case 0xc3: m_AC = Load(REG_P3);                       return 18;  // LD disp(P3)
    case 0xc4: m_AC = LoadImmediate();                    return 10;  // LDI #data
    case 0xc5: m_AC = Load(REG_P1, true);                 return 18;  // LD @disp(P1)
    case 0xc6: m_AC = Load(REG_P2, true);                 return 18;  // LD @disp(P2)
    case 0xc7: m_AC = Load(REG_P3, true);                 return 18;  // LD @disp(P3)

    // Memory Reference instructions - STORE ...
    case 0xc8: Store(m_AC, REG_PC, false);                return 18;  // ST disp(PC)
    case 0xc9: Store(m_AC, REG_P1, false);                return 18;  // ST disp(P1)
    case 0xca: Store(m_AC, REG_P2, false);                return 18;  // ST disp(P2)
    case 0xcb: Store(m_AC, REG_P3, false);                return 18;  // ST disp(P3)
    case 0xcc: IllegalOpcode();  INCPC();                 return 18;  // store immediate??
    case 0xcd: Store(m_AC, REG_P1, true);                 return 18;  // ST @disp(P1)
    case 0xce: Store(m_AC, REG_P2, true);                 return 18;  // ST @disp(P2)
    case 0xcf: Store(m_AC, REG_P3, true);                 return 18;  // ST @disp(P3)

    // Memory Reference instructions - AND ...
    case 0xd0: m_AC &= Load(REG_PC, false);               return 18;  // AND disp(PC)
    case 0xd1: m_AC &= Load(REG_P1, false);               return 18;  // AND disp(P1)
    case 0xd2: m_AC &= Load(REG_P2, false);               return 18;  // AND disp(P2)
    case 0xd3: m_AC &= Load(REG_P3, false);               return 18;  // AND disp(P3)
    case 0xd4: m_AC &= LoadImmediate();                   return 10;  // ANI #data
    case 0xd5: m_AC &= Load(REG_P1, true);                return 18;  // AND @disp(P1)
    case 0xd6: m_AC &= Load(REG_P2, true);                return 18;  // AND @disp(P2)
    case 0xd7: m_AC &= Load(REG_P3, true);                return 18;  // AND @disp(P3)

    // Memory Reference instructions - OR ...
    case 0xd8: m_AC |= Load(REG_PC, false);               return 18;  // OR disp(PC)
    case 0xd9: m_AC |= Load(REG_P1, false);               return 18;  // OR disp(P1)
    case 0xda: m_AC |= Load(REG_P2, false);               return 18;  // OR disp(P2)
    case 0xdb: m_AC |= Load(REG_P3, false);               return 18;  // OR disp(P3)
    case 0xdc: m_AC |= LoadImmediate();                   return 10;  // ORI #data
    case 0xdd: m_AC |= Load(REG_P1, true);                return 18;  // OR @disp(P1)
    case 0xde: m_AC |= Load(REG_P2, true);                return 18;  // OR @disp(P2)
    case 0xdf: m_AC |= Load(REG_P3, true);                return 18;  // OR @disp(P3)

    // Memory Reference instructions - XOR ...
    case 0xe0: m_AC ^= Load(REG_PC, false);               return 18;  // XOR disp(PC)
    case 0xe1: m_AC ^= Load(REG_P1, false);               return 18;  // XOR disp(P1)
    case 0xe2: m_AC ^= Load(REG_P2, false);               return 18;  // XOR disp(P2)
    case 0xe3: m_AC ^= Load(REG_P3, false);               return 18;  // XOR disp(P3)
    case 0xe4: m_AC ^= LoadImmediate();                   return 10;  // XRI #data
    case 0xe5: m_AC ^= Load(REG_P1, true);                return 18;  // XOR @disp(P1)
    case 0xe6: m_AC ^= Load(REG_P2, true);                return 18;  // XOR @disp(P2)
    case 0xe7: m_AC ^= Load(REG_P3, true);                return 18;  // XOR @disp(P3)

    // Memory Reference instructions - decimal ADD ...
    case 0xe8: m_AC = DADD(m_AC, Load(REG_PC, false));    return 23;  // DAD disp(PC)
    case 0xe9: m_AC = DADD(m_AC, Load(REG_P1, false));    return 23;  // DAD disp(P1)
    case 0xea: m_AC = DADD(m_AC, Load(REG_P2, false));    return 23;  // DAD disp(P2)
    case 0xeb: m_AC = DADD(m_AC, Load(REG_P3, false));    return 23;  // DAD disp(P3)
    case 0xec: m_AC = DADD(m_AC, LoadImmediate());        return 15;  // DAI #data
    case 0xed: m_AC = DADD(m_AC, Load(REG_P1, true));     return 23;  // DAD @disp(P1)
    case 0xee: m_AC = DADD(m_AC, Load(REG_P2, true));     return 23;  // DAD @disp(P2)
    case 0xef: m_AC = DADD(m_AC, Load(REG_P3, true));     return 23;  // DAD @disp(P3)

    // Memory Reference instructions - binary ADD ...
    case 0xf0: m_AC = ADD(m_AC, Load(REG_PC, false));     return 19;  // ADD disp(PC)
    case 0xf1: m_AC = ADD(m_AC, Load(REG_P1, false));     return 19;  // ADD disp(P1)
    case 0xf2: m_AC = ADD(m_AC, Load(REG_P2, false));     return 19;  // ADD disp(P2)
    case 0xf3: m_AC = ADD(m_AC, Load(REG_P3, false));     return 19;  // ADD disp(P3)
    case 0xf4: m_AC = ADD(m_AC, LoadImmediate());         return 11;  // ADI #data
    case 0xf5: m_AC = ADD(m_AC, Load(REG_P1, true));      return 19;  // ADD @disp(P1)
    case 0xf6: m_AC = ADD(m_AC, Load(REG_P2, true));      return 19;  // ADD @disp(P2)
    case 0xf7: m_AC = ADD(m_AC, Load(REG_P3, true));      return 19;  // ADD @disp(P3)

    // Memory Reference instructions - complement ADD ...
    case 0xf8: m_AC = ADD(m_AC, ~Load(REG_PC, false));    return 20;  // CAD disp(PC)
    case 0xf9: m_AC = ADD(m_AC, ~Load(REG_P1, false));    return 20;  // CAD disp(P1)
    case 0xfa: m_AC = ADD(m_AC, ~Load(REG_P2, false));    return 20;  // CAD disp(P2)
    case 0xfb: m_AC = ADD(m_AC, ~Load(REG_P3, false));    return 20;  // CAD disp(P3)
    case 0xfc: m_AC = ADD(m_AC, ~LoadImmediate());        return 12;  // CAI #data
    case 0xfd: m_AC = ADD(m_AC, ~Load(REG_P1, true));     return 20;  // CAD @disp(P1)
    case 0xfe: m_AC = ADD(m_AC, ~Load(REG_P2, true));     return 20;  // CAD @disp(P2)
    case 0xff: m_AC = ADD(m_AC, ~Load(REG_P3, true));     return 20;  // CAD @disp(P3)

    // Everything else is invalid!
    default:
      IllegalOpcode();  return 6;
  }
}

CCPU::STOP_CODE CSCMP2::Run (uint32_t nCount)
{
  //++
  //   This is the main "engine" of the SC/MP emulator.  The UI code is	
  // expected to call it whenever the user gives a START, GO, STEP, etc	
  // command and it will execute SC/MP instructions until it either a)	
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
    m_pEvents->DoEvents();

    // See if an interrupt is required ...
    if (m_pInterrupt != NULL) {
      // Do we really need a CInterrupt object here?  The only source
      // for interrupts is SENSE A, after all!
      //if (ISSET(m_SR, SR_IE))  DoInterrupt();
    }

    // Stop if we've hit a breakpoint ...
    if (!fFirst && m_pMemory->IsBreak(GetPC())) {
      m_nStopCode = STOP_BREAKPOINT;  break;
    } else
      fFirst = false;

    // If tracing is on, then log the instruction we're about to execute.
    if (ISLOGGED(TRACE)) TraceInstruction();

    // Fetch, decode and execute an instruction...
    //   Note that the SC/MP is super weird - it incrememebts the PC _before_
    // fetching the opcode, not after!!
    m_nLastPC = INCPC();
    uint8_t bOpcode = m_pMemory->CPUread(m_P[REG_PC]);
    AddTime(DoExecute(bOpcode)*m_qMicrocycleTime);

    // Check for some termination conditions ...
    if (m_nStopCode == STOP_NONE) {
      // Terminate if we've executed enough instructions ... 
      if ((nCount > 0)  &&  (--nCount == 0))  m_nStopCode = STOP_FINISHED;
    }
  }

  return m_nStopCode;
}

unsigned CSCMP2::GetRegisterSize (cpureg_t nReg) const
{
  //++
  //   This method returns the size of a given register, IN BITS!
  // It's used only by the UI, to figure out how to print and mask register
  // values...
  //--
  switch (nReg) {
    case REG_PC:  case REG_P1:  case REG_P2:   case REG_P3:
      return 16;
    case REG_AC:  case REG_EX:  case REG_SR:
      return 8;
    default:
      return 0;
  }
}

uint16_t CSCMP2::GetRegister (cpureg_t nReg) const
{
  //++
  // This method will return the contents of an internal CPU register ...
  //--
  switch (nReg) {
    case REG_AC:  return m_AC;
    case REG_EX:  return m_EX;
    case REG_SR:  return m_SR;
    case REG_PC:  case REG_P1:  case REG_P2:  case REG_P3:
      return m_P[nReg];
    default:
      return 0;
  }
}

void CSCMP2::SetRegister (cpureg_t nReg, uint16_t nVal)
{
  //++
  // Change the contents of an internal CPU register ...
  //--
  switch (nReg) {
    case REG_AC:  m_AC = MASK8(nVal);  break;
    case REG_EX:  m_EX = MASK8(nVal);  break;
    case REG_SR:  m_SR = MASK8(nVal);  break;
    case REG_PC:  case REG_P1:  case REG_P2:  case REG_P3:
      m_P[nReg] = nVal;  break;
    default:
      break;
  }
}
