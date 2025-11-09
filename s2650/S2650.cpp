//++
//S2650.cpp - Signetics 2650 microprocessor emulation
//
// DESCRIPTION:
//   This module implements a simulation of the Signetics 2650 CPU.
//
// REVISION HISTORY:
// 21-FEB-20  RLA  New file.
// 27-FEB-20  RLA  STRZ has the source and destination reversed!
//                 GetEA3A() has incrememnt and decrement reversed!
// 22-JUN-22  RLA  Add nSense and nFlag parameters to GetSense() and SetFlag()
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>             // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include "EMULIB.hpp"           // emulator library definitions
#include "LogFile.hpp"          // emulator library message logging facility
#include "CommandParser.hpp"    // needed for type KEYWORD
#include "SBC50.hpp"            // global declarations for this project
#include "Interrupt.hpp"        // interrupt simulation logic
#include "EventQueue.hpp"       // I/O device event queue simulation
#include "MemoryTypes.h"        // address_t and word_t data types
#include "S2650opcodes.hpp"     // COSMAC opcode definitions
#include "S2650.hpp"            // CCPU base class definitions
#include "Memory.hpp"           // CMemory memory emulation object
#include "Device.hpp"           // CDevice I/O device emulation objects
#include "S2650.hpp"            // declarations for this module

// Internal CPU register names for GetRegisterNames() ...
const CCmdArgKeyword::KEYWORD C2650::g_keysRegisters[] = {
  {"R0",  C2650::REG_R0},      {"R1",  C2650::REG_R1},
  {"R2",  C2650::REG_R2},      {"R3",  C2650::REG_R3},
  {"R1P", C2650::REG_R1P},     {"R2P", C2650::REG_R2P},
  {"R3P", C2650::REG_R3P},     {"PSU", C2650::REG_PSU},
  {"PSL", C2650::REG_PSL},     {"IAR", C2650::REG_IAR},
  {"S0",  C2650::REG_STACK+0}, {"S1",  C2650::REG_STACK+1},
  {"S2",  C2650::REG_STACK+2}, {"S3",  C2650::REG_STACK+3},
  {"S4",  C2650::REG_STACK+4}, {"S5",  C2650::REG_STACK+5},
  {"S6",  C2650::REG_STACK+6}, {"S7",  C2650::REG_STACK+7},
  {NULL, 0}
};

// Names for the sense inputs and flag outputs ...
const char *C2650::g_apszSenseNames[MAXSENSE] = {"SENSE"};
const char *C2650::g_apszFlagNames[MAXFLAG]   = {"FLAG"};


C2650::C2650 (CMemory *pMemory, CEventQueue *pEvents, CInterrupt *pInterrupt)
  : CCPU(pMemory, pEvents, pInterrupt)
{
  //++
  //--
  SetCrystalFrequency(DEFAULT_CLOCK);
  C2650::ClearCPU();
}

void C2650::ClearCPU()
{
  //++
  // This routine resets the 2650 to a power on state.
  //--
  CCPU::ClearCPU();
  memset(m_R,     0, sizeof(m_R));
  memset(m_RP,    0, sizeof(m_RP));
  memset(m_RAS, 0, sizeof(m_RAS));
  m_IAR = 0;  m_PSL = m_PSU = 0;
  SetPSU(m_PSU, true);
}

void C2650::SetPSU (uint8_t bPSU, bool fForce)
{
  //++
  //   Load the program status, upper, bits.  If the state of the FLAG bit
  // is changed, or if fForce is true, then tell any attached device about
  // the current state of the flag.
  //--
  if (fForce || ISSET((m_PSU^bPSU), PSU_F)) {
    SetFlag(0, ISSET(bPSU, PSU_F));
  }
  m_PSU = bPSU & PSU_MASK;
}

uint8_t C2650::GetPSU ()
{
  //++
  //   Return the program status, upper, bits AFTER first querying any attached
  // device for the current state of the SENSE input...
  //--
  uint1_t bBit = GetSense();
  if (bBit != 0) SETBIT(m_PSU, PSU_S);  else CLRBIT(m_PSU, PSU_S);
  return (m_PSU & PSU_MASK);
}

void C2650::TraceInstruction() const
{
  //++
  //   This routine will log the instruction that we're about to execute.
  // If tracing is not enabled, it does nothing ...
  //--
  if (!ISLOGGED(TRACE)) return;
#ifdef UNUSED
  // Disassemble the opcode and fetch any operands ...
  string sCode;  uint8_t bOpcode, bData1, bData2;  address_t wPC = GetPC();
  size_t nCount = Disassemble(g_pCPU->GetMemory(), wPC, sCode);
  bOpcode = m_Memory.CPUread(wPC);
  if (nCount > 1) bData1 = m_Memory.CPUread(INC16(wPC));
  if (nCount > 2) bData2 = m_Memory.CPUread(INC16(wPC,2));

  // Print it out neatly ...
  if (nCount <= 1) {
    LOGF(TRACE, "%04X/ %02X      \t%s", wPC, bOpcode, sCode.c_str());
  } else if (nCount <= 2) {
    LOGF(TRACE, "%04X/ %02X %02X   \t%s", wPC, bOpcode, bData1, sCode.c_str());
  } else {
    LOGF(TRACE, "%04X/ %02X %02X %02X\t%s", wPC, bOpcode, bData1, bData2, sCode.c_str());
  }
#endif
}

void C2650::ADD (uint8_t &bDST, uint8_t bSRC)
{
  //++
  //   Add two 8 bit operands, either signed or unsigned, update the condition
  // codes, and store the result.  DST is a register reference that is both
  // one of the source operands and also receives the result, and SRC is any
  // other 8 bit value.  It could be the contents of another register, an
  // immediate operand, or the contents of some relative or absolute memory
  // location.
  //
  //   The condition codes, POSITIVE, NEGATIVE or ZERO, are set according to
  // the result.  The CY bit is set if an 8 bit unsigned overflow occurs, and
  // the OVF bit will be set in the event of an 8 bit signed overflow.  Lastly,
  // the IDC flag is set when there is a carry between bits 3 and 4 (the lower
  // and upper BCD digits, if that's what the operands contain).
  //--

  // cin is 1 if the WC and CY flags are BOTH set; otherwise 0...
  uint8_t cin = (ISSET(m_PSL, PSL_WC) && ISSET(m_PSL, PSL_CY)) ? 1 : 0;

  // Do the addition, and set the CY flag if the result exceeds 8 bits ...
  uint16_t wResult = bDST + bSRC + cin;
  SetCY(HINIBBLE(wResult) != 0);
  uint8_t bResult = LOBYTE(wResult);

  //   Set the OVF bit if a signed 8 bit overflow occurred.  This is true if
  // a) we added two positive numbers and got a negative result, or
  // b) we added two negative numbers and got a positive result ...
  SetOVF(ISSET((bDST^bResult) & (bSRC^bResult), 0x80));

  //   Figure out the IDC flag by checking for overflow out of the lower four
  // bits.  This is probably not the fastest way to do this, but it works...
  SetIDC((LONIBBLE(bDST) + LONIBBLE(bSRC+cin)) > 0xF);

  // Set the regular condition codes based on the result and we're done ...
  UpdateCC((bDST = bResult));
}

void C2650::SUB (uint8_t &bDST, uint8_t bSRC)
{
  //++
  //   This is essentially the same operation as ADD, except of course that
  // we compute DST-SRC rather than DST+SRC.  This has a couple of subtle
  // consequences for the way the IDC and OVF (signed overflow) flags are
  // computed.
  //--

  // cin is 1 if the WC flag is set AND CY is CLEARED ...
  uint8_t cin = (ISSET(m_PSL, PSL_WC) && !ISSET(m_PSL, PSL_CY)) ? 1 : 0;

  // Do the subtraction and set the CY flag if there's a borrow ...
  uint16_t wResult = bDST - bSRC - cin;
  SetCY(HINIBBLE(wResult) != 0);
  uint8_t bResult = LOBYTE(wResult);

  //   Set the OVF bit if a signed 8 bit overflow occurred.  This is true if
  // a) we subtracted a negative from a positive and got a negative result, or
  // b) we subtracted a positive from a negative and got a positive result ...
  SetOVF(ISSET((bDST^bResult) & ((~bSRC)^bResult), 0x80));

  //   Figure out the IDC flag by checking for overflow out of the lower four
  // bits.  This is probably not the fastest way to do this, but it works...
  SetIDC(LONIBBLE(bDST) < LONIBBLE(bSRC+cin));

  // Set the regular condition codes based on the result and we're done ...
  UpdateCC((bDST = bResult));
}

void C2650::COM (uint8_t bSRC1, uint8_t bSRC2)
{
  //++
  //   The COMx instruction compares the source and destination and sets the
  // conditon codes accordingly - 
  //
  //      CC_EQUAL    if DST == SRC
  //      CC_GREATER  if DST >  SRC
  //      CC_LESS     if DST <  SRC
  //
  // The tricky part is the COM (compare) flag in the PSL - if this is set
  // then we do an unsigned comparison, and if COM is cleared we do a signed
  // comparison.  Note that in this case there are two source operands and no
  // destination.  The distinction is kind of arbitrary, but it reminds us
  // that COM doesn't update any register - only the status flags.
  //
  //   Most CPUs would treat compare as a subtract operation that doesn't save
  // the result, but that doesn't work here.  For one thing, SUB updates all
  // the flags - CY, OVF, IDC - and COM doesn't.  For another, there's the
  // whole signed/unsigned comparison thing.
  //--
  if (bSRC1 == bSRC2) {
    // Equal is equal, signed or unsigned!
    SetCC(CC_EQUAL);
  } else if (ISSET(m_PSL, PSL_COM)) {
    // Do an unsigned comparison - that's easy ...
    SetCC((bSRC1 > bSRC2) ? CC_GREATER : CC_LESS);
  } else {
    //   We need a signed comparison.  There's probably a better way to do
    // this, but this works ...
    int8_t iDST = bSRC1;  int8_t iSRC = bSRC2;
    SetCC((iDST > iSRC) ? CC_GREATER : CC_LESS);
  }
}

void C2650::DAR (uint8_t &bDST)
{
  //++
  //   The 2650 idea of decimal adjust is a little backwards - they want you 
  // to always add 0x66 to every BCD addition and then the DAR instruction
  // conditionally adds 0x00, 0x0A, 0xA0 or 0xAA afterwards to correct the
  // result.  Note that 0xA in BCD is equivalent to "-6"!  The carry and
  // IDC (aka half carry) bits control the addition as in other processors,
  // however the logic is backwards again - we add 0xA if there was NO carry
  // (rather than adding 6 if there WAS carry!).
  //
  //   Note that this instruction does not affect the carry, IDC or OVF flags.
  // It does, however, update the condition code based on the result (although
  // the CC_NEGATIVE flag is probably meaningless here).
  //
  //   One last thing - note that if we add 0xA to the lower nibble we must do
  // it WITHOUT carry to the upper digit!
  //--
  if (!ISSET(m_PSL, PSL_IDC))
    bDST = (bDST & 0xF0) | ((bDST+0x0A) & 0x0F);
  if (!ISSET(m_PSL, PSL_CY))
    bDST = bDST + 0xA0;
  UpdateCC(bDST);
}

void C2650::RRL (uint8_t &bDST)
{
  //++
  //    Implement the Rotate Left instruction.  On the 2650, this operation can
  // be either with carry (the WC bit in the PSL is SET) or without (the WC bit
  // is cleared).  If with carry, then the old carry becomes the new LSB and
  // the old MSB becomes the new carry value.  If without carry, then MSB of
  // the register rotates into the LSB an the carry flag is NOT affected!
  //
  //   The manual is a bit vague on the details, but it appears that if WC is
  // set then the IDC is also updated with the carry from bit 3 to bit 4.
  // And again, if WC is not set then the IDC is not affected.  Also, the OVF
  // bit will be set if the bit shifted out of bit 7 is not the same as the bit
  // shifted in (implying that the sign of the result has changed). And, you
  // guessed it - if WC is cleared than OVF is not affected.
  //
  //   Lastly, the condition code flags updated based on the result.  Unlike
  // OVF and CY, the condition codes are always updated regardless of WC.
  //--
  uint16_t wResult = bDST << 1;
  if (ISSET(m_PSL, PSL_WC)) {
    // Rotate with carry ...
    if (ISSET(m_PSL, PSL_CY)) wResult |= 1;
    SetCY(ISSET(wResult, 0x100));
    SetIDC(ISSET(wResult, 0x10));
    SetOVF(ISSET((wResult ^ bDST), 0x80));
  } else {
    // Without carry is pretty easy ...
    if (ISSET(wResult, 0x100)) wResult |= 1;
    // Should the always clear CY, IDC and OVF, or leave them unchanged?
    // The manual doesn't really say!!!
  }
  UpdateCC((bDST = LOBYTE(wResult)));
}

void C2650::RRR (uint8_t &bDST)
{
  //++
  //  This is pretty much the same as RRL() except, of course, in the other
  // direction ...
  //--
  uint8_t bResult = bDST >> 1;
  if (ISSET(m_PSL, PSL_WC)) {
    // Rotate with carry ...
    if (ISSET(m_PSL, PSL_CY)) bResult |= 0x80;
    SetCY(ISSET(bDST, 1));
    SetIDC(ISSET(bDST, 0x10));
    SetOVF(ISSET((bResult ^ bDST), 0x80));
  } else {
    // Without carry is pretty easy ...
    if (ISSET(bDST, 1)) bResult |= 0x80;
    // Should the always clear CY, IDC and OVF, or leave them unchanged?
    // The manual doesn't really say!!!
  }
  UpdateCC((bDST = bResult));
}

void C2650::TMI (uint8_t bSRC, uint8_t bMask)
{
  //++
  //   The test under mask instruction sets the CC to 0 if all of the bits
  // selected by the mask are 1s (yes, one, not zero!).  If not all the masked
  // bits are one, then the CC is set to 2.
  //
  //   Note that this is also used by the TPSL and TPSU instructions ...
  //--
  if (MASK8(bSRC | ~bMask)  == 0xFF)
    SetCC(0);
  else
    SetCC(2);
}

uint8_t C2650::DoInput (uint8_t bPort)
{
  //++
  //--
  UnimplementedIO();
  return 0xFF;
}

void C2650::DoOutput (uint8_t bData, uint8_t bPort)
{
  //++
  //--
  UnimplementedIO();
}

address_t C2650::GetEA2R (bool fAddTime)
{
  //++
  //   Fetch the next byte and calculate the effective address for "2R" type
  // (two bytes, relative offset) instructions.  This includes handling an
  // indirect address, if necessary.  Note that indirect addressing adds an
  // extra two processor cycles to the execution time.
  // 
  //   Except, that is, in the case of a branch instruction where the branch
  // is NOT taken.  In that case the S2650 is smart enough to skip the extra
  // cycles.  This routine still gets called in either case, though, and the
  // fAddCycles argument tells us whether we should add or ignore the extra
  // time in the case of an indirect address.
  //--
  uint8_t bOffset = Fetch8();
  address_t wEA = ADD13(m_IAR, SXT16(bOffset));
  if (ISSET(bOffset, 0x80)) {
    wEA = MASK15(Fetch16(wEA));
    if (fAddTime) AddTime(2ULL*CycleTime());
  }
  return wEA;
}

bool C2650::GetEA3A (uint2_t bReg, address_t &wEA)
{
  //++
  //   Fetch the next two bytes and calculate the effective address for "3A"
  // type (three bytes, absolute, non-branch) instructions.  If indirect
  // addressing is specified, then handle that first, remembering that the
  // 2650 does indirection BEFORE adding the index.
  //
  //   If the index control field is non-zero, then add an index using register
  // specified by bReg.  If increment or decrement is specified for the index,
  // then update the index register accordingly.
  //
  //   Finally, return TRUE if the index control field is zero, meaning that no
  // indexing was required and bReg actually specifies a destination register.
  // If bReg was used for indexing, however, then return FALSE (meaning that the
  // implicit destination is R0).
  //--

  //   Fetch the operand, extract the index control and indirect flags, and
  // convert it to a full address.  Remember that the 13 bit address that's
  // left over after the index and indirect bits are removed is implicitly
  // in the same page as this instruction!
  wEA = Fetch16();
  uint2_t bIndex = (wEA >> 13) & 3;
  bool fIndirect = ISSET(wEA, 0x8000);
  wEA = (m_IAR & 0x6000) | (wEA & 0x1FFF);

  // If indirect addressing is required, then do that now...
  if (fIndirect) {
    wEA = MASK15(Fetch16(wEA));  AddTime(2ULL*CycleTime());
  }

  // If no indexing is required, then we're done ...
  if (bIndex == 0) return true;

  //   The 2650 is pre-increment AND pre-decrement for indexing, so handle
  // both those cases first ...
  if (bIndex == 1)
    ++REG(bReg);
  else if (bIndex == 2)
    --REG(bReg);

  // Add the index to the EA and we're done ...
  wEA = ADD13(wEA, REG(bReg));
  return false;
}

address_t C2650::GetEA3B (bool fAddTime)
{
  //++
  //   Fetch the next two bytes and calculate the effective address for a "3B"
  // branch or call type instruction.  This is similar to the 3A, however in
  // this case only the indirect bit is allowed.  There is no indexing and no
  // index control bits.  A complete 15 bit address, possibly with a page that
  // differs from the current IAR, may be specified.
  //
  //   Note that this has the same timing consideration (branch instructions
  // only add two indirect cycles if the branch is taken) as GetEA2R()...
  //--
  address_t wEA = Fetch16();
  if (ISSET(wEA, 0x8000)) {
    wEA = MASK15(Fetch16(wEA));
    if (fAddTime) AddTime(2ULL*CycleTime());
  }
  return MASK15(wEA);
}

address_t C2650::GetEA3EB()
{
  //++
  //   Calculate a "3EB" type effective address, as used by the BXA and BSXA
  // instructions.  This is a 16 bit absolute address, possibly indirect,
  // just like the 3EB type instructions, however this one is implicitly
  // indexed by register 3.
  //--
  address_t wEA = GetEA3B();
  wEA = ADD13(wEA, REG(3));
  return wEA;
}

address_t C2650::GetEA2ER()
{
  //++
  //   And this method calculates a "3ER" effective address, as used only by
  // the ZBRR and ZBSR instructions.  This a single byte relative address like
  // the 2R type, however in this case the offset is always relative to page
  // zero address zero, NOT the current IAR.  Note that the result is always in
  // page zero, so this allows the destination to be in either the first 63
  // bytes or the last 64 bytes of the first 8K of memory.
  //
  //   Indirect addressing is still allowed!
  //--
  uint8_t bOffset = Fetch8();
  address_t wEA = MASK13(SXT16(bOffset));
  if (ISSET(bOffset, 0x80)) {
    wEA = MASK15(Fetch16(wEA));  AddTime(2ULL*CycleTime());
  }
  return wEA;
}

uint64_t C2650::DoExecute (uint8_t bOpcode)
{
  //++
  //   Execute the instruction and return the number of cycles used ...
  // Because most 2650 opcodes have a register number in the lower two bits,
  // the architecture lends itself well to decoding just the upper six bits
  // of every instruction.
  //--
  address_t wEA = 0;  bool fB = false;  uint2_t bReg = bOpcode & 0x3;
  switch ((bOpcode>>2) & 077) {

    // Load from memory ...
    case 000: UpdateCC((m_R[0] = REG(bReg)));			  return 2;   // LODZ	
    case 001: UpdateCC((REG(bReg) = Fetch8()));			  return 2;   // LODI	
    case 002: UpdateCC((REG(bReg) = MEMR(GetEA2R())));		  return 3;   // LODR	
    case 003: if (GetEA3A(bReg, wEA))                                         // LODA
                UpdateCC((REG(bReg) = MEMR(wEA)));
              else
                UpdateCC((m_R[0] = MEMR(wEA)));
              return 4;

    // Store register to memory ...
    //   Note that "STRZ 0" is explicitly defined as a NOP.  Also note that "STRZ r"
    // updates the condition codee where as all other STRx operations do not!
    case 060: if (bReg != 0) UpdateCC(REG(bReg) = m_R[0]);	  return 2;   // STRZ 
    //   STRI (store immediate) is illegal.  It's not clear what a real 2650 chip
    // does if you try this, but we trap it as an illegal opcode ...
    case 061: IllegalOpcode();  INC13(m_IAR);                     return 2;   // STRI??
    case 062: MEMW(GetEA2R(), REG(bReg));			  return 3;   // STRR	
    case 063: if (GetEA3A(bReg, wEA))                                         // STRA	
                MEMW(wEA, REG(bReg));
              else
                MEMW(wEA, m_R[0]);
              return 4;

    // Add ...
    case 040: ADD(m_R[0], REG(bReg));			          return 2;   // ADDZ	
    case 041: ADD(REG(bReg), Fetch8());			          return 2;   // ADDI	
    case 042: ADD(REG(bReg), MEMR(GetEA2R()));			  return 3;   // ADDR	
    case 043: if (GetEA3A(bReg, wEA))                                         // ADDA	
                ADD(REG(bReg), MEMR(wEA));
              else
                ADD(m_R[0], MEMR(wEA));
              return 4;

    // Subtract ...
    case 050: SUB(m_R[0], REG(bReg));			          return 2;   // SUBZ	
    case 051: SUB(REG(bReg), Fetch8());			          return 2;   // SUBI	
    case 052: SUB(REG(bReg), MEMR(GetEA2R()));			  return 3;   // SUBR	
    case 053: if (GetEA3A(bReg, wEA))                                         // SUBA	
                SUB(REG(bReg), MEMR(wEA));
              else
                SUB(m_R[0], MEMR(wEA));
              return 4;

    // Logical AND ...
    //   Note that "ANDZ 0" is explicitly defined as a HALT ...
    case 020: if (bReg == 0)
		m_nStopCode = STOP_HALT;				      // HALT
              else
                UpdateCC((m_R[0] &= REG(bReg)));			      // ANDZ	
            return 2;
    case 021: UpdateCC((REG(bReg) &= Fetch8()));		  return 2;   // ANDI	
    case 022: UpdateCC((REG(bReg) &= MEMR(GetEA2R())));		  return 3;   // ANDR	
    case 023: if (GetEA3A(bReg, wEA))                                         // ANDA
                UpdateCC((REG(bReg) &= MEMR(wEA)));
              else
                UpdateCC((m_R[0] &= MEMR(wEA)));
              return 4;

    // Exclusive OR ...
    case 010: UpdateCC((m_R[0] ^= REG(bReg)));		          return 2;   // EORZ	
    case 011: UpdateCC((REG(bReg) ^= Fetch8()));		  return 2;   // EORI	
    case 012: UpdateCC((REG(bReg) ^= MEMR(GetEA2R())));		  return 3;   // EORR	
    case 013: if (GetEA3A(bReg, wEA))                                         // EORA	
                UpdateCC((REG(bReg) ^= MEMR(wEA)));
              else
                UpdateCC((m_R[0] ^= MEMR(wEA)));
              return 4;

    // Inclusive OR ...
    case 030: UpdateCC((m_R[0] |= REG(bReg)));		          return 2;   // IORZ	
    case 031: UpdateCC((REG(bReg) |= Fetch8()));		  return 2;   // IORI	
    case 032: UpdateCC((REG(bReg) |= MEMR(GetEA2R())));		  return 3;   // IORR	
    case 033: if (GetEA3A(bReg, wEA))                                         // IORA	 
                UpdateCC((REG(bReg) |= MEMR(wEA)));
              else
                UpdateCC((m_R[0] |= MEMR(wEA)));
              return 4;

    // Compare ...
    case 070: COM(m_R[0], REG(bReg));			          return 2;   // COMZ	
    case 071: COM(REG(bReg), Fetch8());			          return 2;   // COMI	
    case 072: COM(REG(bReg), MEMR(GetEA2R()));			  return 3;   // COMR	
    case 073: if (GetEA3A(bReg, wEA))                                         // COMA
                COM(REG(bReg), MEMR(wEA));			    
              else
                COM(m_R[0], MEMR(wEA));
              return 4;

    // Decimal adjust ...
    case 045: DAR(REG(bReg));					  return 3;   // DAR

    // Rotate operations ...
    case 024: RRR(REG(bReg));					  return 2;   // RRR
    case 064: RRL(REG(bReg));					  return 2;   // RRL

    // Branch on condition TRUE ...
    case 006: fB = CompareCC(bReg);  BRANCH(GetEA2R(fB), fB);     return 3;   // BCTR
    case 007: fB = CompareCC(bReg);  BRANCH(GetEA3B(fB), fB);     return 3;   // BCTA

    // Branch on condition FALSE ...
    case 046: if (bReg == 3)
                BRANCH(GetEA2ER());                                           // ZBRR
              else {
                fB = !CompareCC(bReg);  BRANCH(GetEA2R(fB), fB);              // BCFR
              }
              return 3;
    case 047: if (bReg == 3)
                BRANCH(GetEA3EB());                                           // BXA
              else {
                fB = !CompareCC(bReg);  BRANCH(GetEA3B(fB), fB);              // BCFA
	      }
              return 3;

     // Branch on non-zero register ...
    case 026: fB = REG(bReg) != 0;  BRANCH(GetEA2R(fB), fB);      return 3;   // BRNR
    case 027: fB = REG(bReg) != 0;  BRANCH(GetEA3B(fB), fB);      return 3;   // BRNA

    // Incremenent or decrement register and branch if non-zero ...
    case 066: fB = ++REG(bReg) != 0;  BRANCH(GetEA2R(fB), fB);    return 3;   // BIRR
    case 067: fB = ++REG(bReg) != 0;  BRANCH(GetEA3B(fB), fB);    return 3;   // BIRA
    case 076: fB = --REG(bReg) != 0;  BRANCH(GetEA2R(fB), fB);    return 3;   // BDRR
    case 077: fB = --REG(bReg) != 0;  BRANCH(GetEA3B(fB), fB);    return 3;   // BDRA

    // Subroutine call on condition TRUE ...
    case 016: fB = CompareCC(bReg);  CALL(GetEA2R(fB), fB);       return 3;   // BSTR
    case 017: fB = CompareCC(bReg);  CALL(GetEA3B(fB), fB);       return 3;   // BSTA

    // Subroutine call on condition FALSE ...
    case 056: if (bReg == 3)
                CALL(GetEA2ER());                                             // ZBSR
              else {
                fB = !CompareCC(bReg);  CALL(GetEA2R(fB), fB);                // BSFR
              }
              return 3;
    case 057: if (bReg == 3)
                CALL(GetEA3EB());                                             // BSXA
              else {
                fB = !CompareCC(bReg);  CALL(GetEA3B(fB), fB);                // BSFA
              }

    // Subroutine call on non-zero register ...
    case 036: fB = REG(bReg) != 0;  CALL(GetEA2R(fB), fB);        return 3;   // BSNR
    case 037: fB = REG(bReg) != 0;  CALL(GetEA3B(fB), fB);        return 3;   // BSNA

    // Return from subroutine ...
    case 005: RETURN(CompareCC(bReg));                            return 3;   // RETC
    case 015: if (RETURN(CompareCC(bReg))) CLRBIT(m_PSU, PSU_II); return 3;   // RETE

    // Test under mask instruction ...
    case 075: TMI(REG(bReg), Fetch8());                           return 3;   // TMI

    // Load program status (upper and lower) ...
    case 044: if (bReg == 2)
                SetPSU(m_R[0]);                                               // LPSU
              else if (bReg == 3)
                m_PSL = m_R[0];                                               // LPSL
              else
                IllegalOpcode();
              return 2;

    // Store program status (upper and lower) ...
    case 004: switch (bReg) {
                case 2:   UpdateCC((m_R[0] = GetPSU()));  break;              // SPSU
                case 3:   UpdateCC((m_R[0] = m_PSL));     break;              // SPSL
                default:  IllegalOpcode();
              }
              return 2;

    case 035: switch (bReg) {
                case 0: SetPSU(GetPSU() & ~Fetch8());   break;                // CPSU
                case 1: m_PSL &= ~Fetch8();             break;                // CPSL
                case 2: SetPSU(GetPSU() | Fetch8());    break;                // PPSU
                case 3: m_PSL |= Fetch8();              break;                // PPSL
              }
              return 3;

    // Test program status ...
    case 055: switch (bReg) {
                case 0:   TMI(GetPSU(), Fetch8());  break;                    // TPSU
                case 1:   TMI(m_PSL, Fetch8());     break;                    // TPSL
                default:  IllegalOpcode();
              }
              return 3;

    // Input/Output instructions ...
    //   These are weird 2650 I/O instructions that don't allow any way to
    // specify a device address.  They're limited to two input and two output,
    // or two bidirectional, ports.  It's not clear how we want to handle
    // these, so for now we'll just make them unimplemented...
    case 074: UnimplementedIO();                                  return 2; // WRTD
    case 034: UnimplementedIO();                                  return 2; // REDD
    case 054: UnimplementedIO();                                  return 2; // WRTC
    case 014: UnimplementedIO();                                  return 2; // REDC

    // (Extended) Input/Output instructions ...
    //   THESE are normal I/O instructions, with an 8 bit port number ...
    case 065: DoOutput(REG(bReg), Fetch8());                      return 3; // WRTE
    case 025: UpdateCC(REG(bReg) = DoInput(Fetch8()));            return 3; // REDE

    // Everything else is invalid!
    default:
      IllegalOpcode();  return 2;
  }
}

CCPU::STOP_CODE C2650::Run (uint32_t nCount)
{
  //++
  //   This is the main "engine" of the 2650 emulator.  The UI code is  
  // expected to call it whenever the user gives a START, GO, STEP, etc 
  // command and it will execute 2650 instructions until it either a)  
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
      //if (m_pInterrupt->IsRequested() && ISSET(m_SR, SR_IE))  DoInterrupt();
    }

    // Stop if we've hit a breakpoint ...
    if (!fFirst && g_pMemory->IsBreak(GetPC())) {
      m_nStopCode = STOP_BREAKPOINT;  break;
    } else
      fFirst = false;

    // If tracing is on, then log the instruction we're about to execute.
    if (ISLOGGED(TRACE)) TraceInstruction();

    // Fetch, decode and execute an instruction...
    m_nLastPC = m_IAR;  uint8_t bOpcode = Fetch8();
    AddTime(DoExecute(bOpcode)*CycleTime());

    // Check for some termination conditions ...
    if (m_nStopCode == STOP_NONE) {
      // Terminate if we've executed enough instructions ... 
      if ((nCount > 0)  &&  (--nCount == 0))  m_nStopCode = STOP_FINISHED;
    }
  }

  return m_nStopCode;
}

uint16_t C2650::GetRegister (cpureg_t nReg) const
{
  //++
  // This method will return the contents of an internal CPU register ...
  //--
  switch (nReg) {
    case REG_R0:  case REG_R1:  case REG_R2:  case REG_R3:
      return m_R[nReg];
    case REG_R1P:  case REG_R2P:  case REG_R3P:
      return m_RP[nReg-REG_R1P];
    case REG_PSU:   return m_PSU;
    case REG_PSL:   return m_PSL;
    case REG_IAR:   return m_IAR;
    default:
      if ((nReg >= REG_STACK) && (nReg < REG_STACK+MAXSTACK))
        return m_RAS[nReg-REG_STACK];
      else
        return 0;
  }
}

void C2650::SetRegister (cpureg_t nReg, uint16_t bData)
{
  //++
  // Change the contents of an internal CPU register ...
  //--
  switch (nReg) {
    case REG_R0:  case REG_R1:  case REG_R2:  case REG_R3:
      m_R[nReg] = MASK8(bData);  break;
    case REG_R1P:  case REG_R2P:  case REG_R3P:
      m_RP[nReg-REG_R1P] = MASK8(bData);  break;
    case REG_PSU:   m_PSU = MASK8(bData); break;
    case REG_PSL:   m_PSL = MASK8(bData); break;
    case REG_IAR:   m_IAR = bData;        break;
    default:
      if ((nReg >= REG_STACK) && (nReg < REG_STACK+MAXSTACK))
        m_RAS[nReg-REG_STACK] = bData;
      break;
  }
}
