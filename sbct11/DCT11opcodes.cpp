//++
//Opcodes.cpp -> PDP11 and disassembler
//
// LICENSE:
//    This file is part of the emulator library project.  SBCT11 is free
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
//   This file contains a table of ASCII mnemonics for PDP11 opcodes, and one
// line assembler and disassembler methods ...
//
// REVISION HISTORY:
//  4-MAR-20  RLA   New file.
// 30-HYN-22  RLA   Fix SOB, which was totally screwed up.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include "EMULIB.hpp"           // emulator library definitions
#include "sbct11.hpp"           // global definitions for this project
#include "Memory.hpp"           // main memory functions
#include "DCT11.hpp"            // T11 CPU emulation
#include "DCT11opcodes.hpp"     // declarations for this module

// T11 opcode definitions ...
PRIVATE const OP_CODE g_aOpcodes[] = {
  {"CLC",   0000241, 0177777, OP_ARG_NONE},
  {"CLV",   0000242, 0177777, OP_ARG_NONE},
  {"CLZ",   0000244, 0177777, OP_ARG_NONE},
  {"CLN",   0000250, 0177777, OP_ARG_NONE},
  {"CCC",   0000257, 0177777, OP_ARG_NONE},
  {"SEC",   0000261, 0177777, OP_ARG_NONE},
  {"SEV",   0000262, 0177777, OP_ARG_NONE},
  {"SEZ",   0000264, 0177777, OP_ARG_NONE},
  {"SEN",   0000270, 0177777, OP_ARG_NONE},
  {"SCC",   0000277, 0177777, OP_ARG_NONE},
  {"NOP",   0000240, 0177777, OP_ARG_NONE},
  {"HALT",  0000000, 0177777, OP_ARG_NONE},
  {"WAIT",  0000001, 0177777, OP_ARG_NONE},
  {"RTI",   0000002, 0177777, OP_ARG_NONE},
  {"BPT",   0000003, 0177777, OP_ARG_NONE},
  {"IOT",   0000004, 0177777, OP_ARG_NONE},
  {"RESET", 0000005, 0177777, OP_ARG_NONE},
  {"RTT",   0000006, 0177777, OP_ARG_NONE},
  {"MFPT",  0000007, 0177777, OP_ARG_NONE},
  {"RTS",   0000200, 0177770, OP_ARG_RTS},
  {"??1",   0000210, 0177770, OP_ARG_RTS},
  {"??2",   0000220, 0177770, OP_ARG_RTS},
  {"CLR",   0005000, 0177700, OP_ARG_DST},
  {"CLRB",  0105000, 0177700, OP_ARG_DST},
  {"COM",   0005100, 0177700, OP_ARG_DST},
  {"COMB",  0105100, 0177700, OP_ARG_DST},
  {"INC",   0005200, 0177700, OP_ARG_DST},
  {"INCB",  0105200, 0177700, OP_ARG_DST},
  {"DEC",   0005300, 0177700, OP_ARG_DST},
  {"DECB",  0105300, 0177700, OP_ARG_DST},
  {"NEG",   0005400, 0177700, OP_ARG_DST},
  {"NEGB",  0105400, 0177700, OP_ARG_DST},
  {"ADC",   0005500, 0177700, OP_ARG_DST},
  {"ADCB",  0105500, 0177700, OP_ARG_DST},
  {"SBC",   0005600, 0177700, OP_ARG_DST},
  {"SBCB",  0105600, 0177700, OP_ARG_DST},
  {"TST",   0005700, 0177700, OP_ARG_DST},
  {"TSTB",  0105700, 0177700, OP_ARG_DST},
  {"ROR",   0006000, 0177700, OP_ARG_DST},
  {"RORB",  0106000, 0177700, OP_ARG_DST},
  {"ROL",   0006100, 0177700, OP_ARG_DST},
  {"ROLB",  0106100, 0177700, OP_ARG_DST},
  {"ASR",   0006200, 0177700, OP_ARG_DST},
  {"ASRB",  0106200, 0177700, OP_ARG_DST},
  {"ASL",   0006300, 0177700, OP_ARG_DST},
  {"ASLB",  0106300, 0177700, OP_ARG_DST},
  {"MTPS",  0106400, 0177700, OP_ARG_SRC},
  {"MFPS",  0106700, 0177700, OP_ARG_DST},
  {"SWAB",  0000300, 0177700, OP_ARG_DST},
  {"JMP",   0000100, 0177700, OP_ARG_DST},
  {"BR",    0000400, 0177400, OP_ARG_BRANCH},
  {"BNE",   0001000, 0177400, OP_ARG_BRANCH},
  {"BEQ",   0001400, 0177400, OP_ARG_BRANCH},
  {"BGE",   0002000, 0177400, OP_ARG_BRANCH},
  {"BLT",   0002400, 0177400, OP_ARG_BRANCH},
  {"BGT",   0003000, 0177400, OP_ARG_BRANCH},
  {"BLT",   0003400, 0177400, OP_ARG_BRANCH},
  {"BPL",   0100000, 0177400, OP_ARG_BRANCH},
  {"BMI",   0100400, 0177400, OP_ARG_BRANCH},
  {"BHI",   0101000, 0177400, OP_ARG_BRANCH},
  {"BLOS",  0101400, 0177400, OP_ARG_BRANCH},
  {"BVC",   0102000, 0177400, OP_ARG_BRANCH},
  {"BVS",   0102400, 0177400, OP_ARG_BRANCH},
  {"BCC",   0103000, 0177400, OP_ARG_BRANCH},
  {"BHIS",  0103000, 0177400, OP_ARG_BRANCH},
  {"BCS",   0103400, 0177400, OP_ARG_BRANCH},
  {"BLO",   0103400, 0177400, OP_ARG_BRANCH},
  {"EMT",   0104000, 0177400, OP_ARG_TRAP},
  {"TRAP",  0104400, 0177400, OP_ARG_TRAP},
  {"SOB",   0077000, 0177000, OP_ARG_SOB},
  {"XOR",   0074000, 0177000, OP_ARG_XOR},
  {"JSR",   0004000, 0177000, OP_ARG_XOR},
  {"MOV",   0010000, 0170000, OP_ARG_DSTSRC},
  {"MOVB",  0110000, 0170000, OP_ARG_DSTSRC},
  {"CMP",   0020000, 0170000, OP_ARG_DSTSRC},
  {"CMPB",  0120000, 0170000, OP_ARG_DSTSRC},
  {"BIT",   0030000, 0170000, OP_ARG_DSTSRC},
  {"BITB",  0130000, 0170000, OP_ARG_DSTSRC},
  {"BIC",   0040000, 0170000, OP_ARG_DSTSRC},
  {"BICB",  0140000, 0170000, OP_ARG_DSTSRC},
  {"BIS",   0050000, 0170000, OP_ARG_DSTSRC},
  {"BISB",  0150000, 0170000, OP_ARG_DSTSRC},
  {"ADD",   0060000, 0170000, OP_ARG_DSTSRC},
  {"SUB",   0160000, 0170000, OP_ARG_DSTSRC},
};
#define OP_COUNT (sizeof(g_aOpcodes)/sizeof(OP_CODE))

// T11 register names, pairs and condition codes ...
PRIVATE const char *g_apszRegisters[8] = {"R0", "R1", "R2", "R3", "R4", "R5", "SP", "PC"};


// Fetch a word from PDP-11 memory ...
//   Remember that the word must be aligned on an even address!
PRIVATE inline uint16_t GetWord (const CMemory *pMemory, address_t a)
  {uint8_t l = pMemory->CPUread(a & 0177776);  uint8_t h = pMemory->CPUread(a | 1);  return MKWORD(h, l);}

PRIVATE string DisassembleOperand (const CMemory *pMemory, address_t &wLoc, uint16_t &cbOpcode, uint8_t bMode, uint8_t bReg)
{
  //++
  // Disassemble one PDP-11 operand, including all possible addressing modes..
  //--
  assert((bMode < 8) && (bReg < 9));
  uint16_t wEA;
  switch (bMode) {
    case 0:
      // Register mode - Rn ...
      return g_apszRegisters[bReg];

    case 1:
      // Register deferred - (Rn) ...
      return FormatString("(%s)", g_apszRegisters[bReg]);

    case 2:
      // Register autoincrement - (Rn)+ ..
      //  UNLESS R is the PC, in which case it's immediate - #oooooo ...
      if (bReg != 7)
        return FormatString("(%s)+", g_apszRegisters[bReg]);
      else {
        wEA = GetWord(pMemory, wLoc);  cbOpcode += 2;  wLoc += 2;
        return FormatString("#%06o", wEA);
      }

    case 3:
      // Register autoincrement deferred - @(Rn)+ ...
      //   UNLESS it's the PC, in which case it's direct addressing - @#oooooo ... 
      if (bReg != 7)
        return FormatString("@(%s)+", g_apszRegisters[bReg]);
      else {
        wEA = GetWord(pMemory, wLoc);  cbOpcode += 2;  wLoc += 2;
        return FormatString("@#%06o", wEA);
      }

    case 4:
      // Register autodecrement - -(Rn) ...
      return FormatString("-(%s)", g_apszRegisters[bReg]);

    case 5:
      // Register autodecrement deferred - @-(Rn) ...
      return FormatString("@-(%s)", g_apszRegisters[bReg]);

    case 6:
      // Indexed - oooooo(Rn) ...
      //    UNLESS the PC is used, then it's a PC relative address ...
      wEA = GetWord(pMemory, wLoc);  cbOpcode += 2;  wLoc += 2;
      if (bReg != 7)
        return FormatString("%06o(%s)", wEA, g_apszRegisters[bReg]);
      else
        return FormatString("%06o", MASK16(wEA+wLoc));

    case 7:
      // Indexed deferred - @oooooo(Rn) ...
      //   UNLESS the PC is used, then it's PC relative deferred ...
      wEA = GetWord(pMemory, wLoc);  cbOpcode += 2;  wLoc += 2;
      if (bReg != 7)
        return FormatString("@%06o(%s)", wEA, g_apszRegisters[bReg]);
      else
        return FormatString("@%06o", MASK16(wEA+wLoc)); 
  }

  // Just to make the compiler happy...
  return "";
}

PRIVATE string DisassembleBranch (address_t &wLoc, uint16_t wOpcode)
{
  //++
  // Calculate the target address for a branch instruction...
  //--
  address_t wEA;
  if (!ISSET(wOpcode, 0200))
    // Branch fowards ...
    wEA = MASK16(wLoc+ (((wOpcode<<1) & 0000377)));
  else
    // Branch backwards ...
    wEA = MASK16(wLoc + ((wOpcode<<1) | 0177400));
  return FormatString("%06o", wEA);
}

PUBLIC uint16_t Disassemble (const CMemory *pMemory, address_t wLoc, string &sCode)
{
  //++
  //   Disassemble one instruction and return a string containg the result.
  // On the PDP11, instructions are multiples of two bytes in length and can
  // be either 1, 2 or 3 words (2, 4 or 6 bytes) in length depending on the
  // addressing mode of either or both operands.  The memory address of the
  // instruction word should be passed in the wLoc parameter, and the return
  // value is the number of bytes actually used by the instruction.
  //--

  // Fetch the instruction and extract the source and destination fields ...
  uint16_t wOpcode  = GetWord(pMemory, wLoc);   // the actual instruction
  uint16_t cbOpcode = 2;                        // number of bytes in this instruction
  uint8_t  bSRC     = (wOpcode >> 6)  & 077;    // source mode and register
  uint8_t  bDST     =  wOpcode        & 077;    // destination mode and register
  uint8_t  bSRCmode = (bSRC >> 3) & 7;          // source addressing mode
  uint8_t  bDSTmode = (bDST >> 3) & 7;          // destination addressing mode
  uint8_t  bSRCreg  =  bSRC       & 7;          // source register
  uint8_t  bDSTreg  =  bDST       & 7;          // destination register
  uint16_t wEA;
  wLoc += 2;

  // Search the opcode table for a match ...
  const OP_CODE *pOpcode = NULL;
  for (size_t i = 0;  i < OP_COUNT;  ++i) {
    if ((wOpcode & g_aOpcodes[i].wMask) == g_aOpcodes[i].wOpcode) {
      pOpcode = &g_aOpcodes[i];  break;
    }
  }

  // If there's no match then it's not a valid opcode ...
  if (pOpcode == NULL) {
    sCode = FormatString("invalid opcode");  return 2;
  }

  // Format the opcode and operands as required ...
  string sDST, sSRC;
  switch (pOpcode->nType) {
    case OP_ARG_NONE:
      // No operands - that's easy!
      sCode = pOpcode->pszName;
      break;

    case OP_ARG_DSTSRC:
      // Two operand instructions - full destination and source ...
      sSRC = DisassembleOperand(pMemory, wLoc, cbOpcode, bSRCmode, bSRCreg);
      sDST = DisassembleOperand(pMemory, wLoc, cbOpcode, bDSTmode, bDSTreg);
      sCode = FormatString("%s\t%s, %s", pOpcode->pszName, sSRC.c_str(), sDST.c_str());
      break;

    case OP_ARG_XOR:
      // XOR has only a register for the source, but a full address for the destination...
      sDST = DisassembleOperand(pMemory, wLoc, cbOpcode, bDSTmode, bDSTreg);
      sCode = FormatString("%s\t%s, %s", pOpcode->pszName, g_apszRegisters[bSRCreg], sDST.c_str());
      break;

    case OP_ARG_DST:
    case OP_ARG_SRC:
      // Single operand instructions - destination only ...
      //   Note that OP_ARG_SRC is used for MTPS, where the source address
      // actually appears in the destination field.  They're the same as
      // far as we're concerned...
      sDST = DisassembleOperand(pMemory, wLoc, cbOpcode, bDSTmode, bDSTreg);
      sCode = FormatString("%s\t%s", pOpcode->pszName, sDST.c_str());
      break;

    case OP_ARG_BRANCH:
      // Branch instructions (the destination is an offset from the PC) ...
      sDST = DisassembleBranch(wLoc, wOpcode);
      sCode = FormatString("%s\t%s", pOpcode->pszName, sDST.c_str());
      break;

    case OP_ARG_SOB:
      //   SOB is a little like branch, except that it has a register and
      // can only branch backwards with a 6 bit offset!
      wEA = MASK16(wLoc - ((wOpcode & 077) << 1));
      sCode = FormatString("%s\t%s,%06o", pOpcode->pszName, g_apszRegisters[bSRCreg], wEA);
      break;

    case OP_ARG_TRAP:
      // TRAP and EMT have an 8 bit "trap number" operand ...
      sCode = FormatString("%s\t%03o", pOpcode->pszName, bDST);
      break;

    case OP_ARG_RTS:
      // And RTS has just a register number and no more ...
      sCode = FormatString("%s\t%s", pOpcode->pszName, g_apszRegisters[bDSTreg]);  break;

    // We should never get here!
    default: assert(false);
  }

  // Return the number of bytes disassembled and we're done ...
  return cbOpcode;
}

size_t Assemble (CMemory &Memory, const string &sCode, size_t nStart)
{
  //++
  //--
  return 0;
}
