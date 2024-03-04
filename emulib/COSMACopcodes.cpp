//++
//COSMACopcodes.cpp -> COSMAC assembler and disassembler
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
//   This file contains a table of ASCII mnemonics for COSMAC opcodes, and one
// line assembler and disassembler methods ...
//
// REVISION HISTORY:
// 13-JAN-20  RLA  New file.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include "EMULIB.hpp"           // emulator library definitions
#include "MemoryTypes.h"        // address_t and word_t data types
#include "Memory.hpp"           // main memory functions
#include "COSMACopcodes.hpp"    // declarations for this module

// COSMAC 1802 opcode definitions ...
PRIVATE const OPCODE g_aOpcodes[] = {
  {"IDL",  OP_IDL,  0xFF, OP_ARG_NONE},	    // wait for dma or interrupt
  {"LDN",  OP_LDN,  0xF0, OP_ARG_REG},      // load via N
  {"INC",  OP_INC,  0xF0, OP_ARG_REG},      // increment reg N
  {"DEC",  OP_DEC,  0xF0, OP_ARG_REG},      // decrement reg N
  {"BR",   OP_BR,   0xFF, OP_ARG_1BYTE},    // short branch
  {"BQ",   OP_BQ,   0xFF, OP_ARG_1BYTE},    // short branch if Q = 1
  {"BZ",   OP_BZ,   0xFF, OP_ARG_1BYTE},    // short branch if D = 0
  {"BDF",  OP_BDF,  0xFF, OP_ARG_1BYTE},    // short branch if DF = 1
  {"B1",   OP_B1,   0xFF, OP_ARG_1BYTE},    // short branch if EF1 = 1
  {"B2",   OP_B2,   0xFF, OP_ARG_1BYTE},    // short branch if EF2 = 1
  {"B3",   OP_B3,   0xFF, OP_ARG_1BYTE},    // short branch if EF3 = 1
  {"B4",   OP_B4,   0xFF, OP_ARG_1BYTE},    // short branch if EF4 = 1
  {"SKP",  OP_SKP,  0xFF, OP_ARG_NONE},	    // no short branch
  {"BNQ",  OP_BNQ,  0xFF, OP_ARG_1BYTE},    // short branch if Q = 0
  {"BNZ",  OP_BNZ,  0xFF, OP_ARG_1BYTE},    // short branch if D != 0
  {"BNF",  OP_BNF,  0xFF, OP_ARG_1BYTE},    // short branch if DF = 0
  {"BN1",  OP_BN1,  0xFF, OP_ARG_1BYTE},    // short branch if EF1 = 0
  {"BN2",  OP_BN2,  0xFF, OP_ARG_1BYTE},    // short branch if EF2 = 0
  {"BN3",  OP_BN3,  0xFF, OP_ARG_1BYTE},    // short branch if EF3 = 0
  {"BN4",  OP_BN4,  0xFF, OP_ARG_1BYTE},    // short branch if EF4 = 0
  {"LDA",  OP_LDA,  0xF0, OP_ARG_REG},      // load advance
  {"STR",  OP_STR,  0xF0, OP_ARG_REG},      // store via N
  {"IRX",  OP_IRX,  0xFF, OP_ARG_NONE},	    // increment reg X
  {NULL,   0x68,    0xFF, OP_ARG_EXTENDED}, // extended 1804/5/6 opcodes
  {"OUT",  OP_OUT,  0xF8, OP_ARG_IO},       // output
  {"INP",  OP_INP,  0xF8, OP_ARG_IO},       // input 
  {"RET",  OP_RET,  0xFF, OP_ARG_NONE},	    // return
  {"DIS",  OP_DIS,  0xFF, OP_ARG_NONE},	    // disable
  {"LDXA", OP_LDXA, 0xFF, OP_ARG_NONE},	    // load via X and advance
  {"STXD", OP_STXD, 0xFF, OP_ARG_NONE},	    // store via X and decrement
  {"ADC",  OP_ADC,  0xFF, OP_ARG_NONE},	    // add with carry
  {"SDB",  OP_SDB,  0xFF, OP_ARG_NONE},	    // subtract D with borrow
  {"SHRC", OP_SHRC, 0xFF, OP_ARG_NONE},	    // shift right with carry
  {"SMB",  OP_SMB,  0xFF, OP_ARG_NONE},	    // subtract memory with borrow
  {"SAV",  OP_SAV,  0xFF, OP_ARG_NONE},	    // save
  {"MARK", OP_MARK, 0xFF, OP_ARG_NONE},	    // push (X,P) to stack
  {"REQ",  OP_REQ,  0xFF, OP_ARG_NONE},	    // reset Q
  {"SEQ",  OP_SEQ,  0xFF, OP_ARG_NONE},	    // set Q
  {"ADCI", OP_ADCI, 0xFF, OP_ARG_1BYTE},    // add with carry, immediate
  {"SDBI", OP_SDBI, 0xFF, OP_ARG_1BYTE},    // subtract D with borrow, immediate
  {"SHLC", OP_SHLC, 0xFF, OP_ARG_NONE},     // shift left with carry
  {"SMBI", OP_SMBI, 0xFF, OP_ARG_1BYTE},    // subtract memory with borrow, immediate
  {"GLO",  OP_GLO,  0xF0, OP_ARG_REG},      // get low reg N
  {"GHI",  OP_GHI,  0xF0, OP_ARG_REG},      // get high reg N
  {"PLO",  OP_PLO,  0xF0, OP_ARG_REG},      // put low reg N
  {"PHI",  OP_PHI,  0xF0, OP_ARG_REG},      // put high reg N
  {"LBR",  OP_LBR,  0xFF, OP_ARG_2BYTES},   // long branch
  {"LBQ",  OP_LBQ,  0xFF, OP_ARG_2BYTES},   // long branch if Q = 1
  {"LBZ",  OP_LBZ,  0xFF, OP_ARG_2BYTES},   // long branch if D = 0
  {"LBDF", OP_LBDF, 0xFF, OP_ARG_2BYTES},   // long branch if DF = 1
  {"NOP",  OP_NOP,  0xFF, OP_ARG_NONE},	    // no operation
  {"LSNQ", OP_LSNQ, 0xFF, OP_ARG_NONE},     // long skip if Q = 0
  {"LSNZ", OP_LSNZ, 0xFF, OP_ARG_NONE},     // long skip if D != 0
  {"LSNF", OP_LSNF, 0xFF, OP_ARG_NONE},     // long skip if DF = 0
  {"LSKP", OP_LSKP, 0xFF, OP_ARG_NONE},	    // no long branch
  {"LBNQ", OP_LBNQ, 0xFF, OP_ARG_2BYTES},   // long branch lf Q = 0
  {"LBNZ", OP_LBNZ, 0xFF, OP_ARG_2BYTES},   // long branch if D != 0
  {"LBNF", OP_LBNF, 0xFF, OP_ARG_2BYTES},   // long branch if DF = 0
  {"LSIE", OP_LSIE, 0xFF, OP_ARG_NONE},	    // long skip if lE = 1
  {"LSQ",  OP_LSQ,  0xFF, OP_ARG_NONE},	    // long skip lf Q = 1
  {"LSZ",  OP_LSZ,  0xFF, OP_ARG_NONE},	    // long skip if D = 0
  {"LSDF", OP_LSDF, 0xFF, OP_ARG_NONE},	    // long skip if DF = 1
  {"SEP",  OP_SEP,  0xF0, OP_ARG_REG},      // set P
  {"SEX",  OP_SEX,  0xF0, OP_ARG_REG},      // set X
  {"LDX",  OP_LDX,  0xFF, OP_ARG_NONE},	    // load via X
  {"OR",   OP_OR,   0xFF, OP_ARG_NONE},	    // or
  {"AND",  OP_AND,  0xFF, OP_ARG_NONE},	    // and
  {"XOR",  OP_XOR,  0xFF, OP_ARG_NONE},	    // exclusive or
  {"ADD",  OP_ADD,  0xFF, OP_ARG_NONE},	    // add
  {"SD",   OP_SD,   0xFF, OP_ARG_NONE},	    // subtract D
  {"SHR",  OP_SHR,  0xFF, OP_ARG_NONE},	    // shift right
  {"SM",   OP_SM,   0xFF, OP_ARG_NONE},	    // subtract memory
  {"LDI",  OP_LDI,  0xFF, OP_ARG_1BYTE},    // load immediate
  {"ORI",  OP_ORI,  0xFF, OP_ARG_1BYTE},    // or immediate
  {"XRI",  OP_XRI,  0xFF, OP_ARG_1BYTE},    // exclusive or immediate
  {"ANI",  OP_ANI,  0xFF, OP_ARG_1BYTE},    // and immediate
  {"ADI",  OP_ADI,  0xFF, OP_ARG_1BYTE},    // add immediate
  {"SDI",  OP_SDI,  0xFF, OP_ARG_1BYTE},    // subtract D immediate
  {"SHL",  OP_SHL,  0xFF, OP_ARG_NONE},	    // shift left
  {"SMI",  OP_SMI,  0xFF, OP_ARG_1BYTE},    // subtract memory immediate
};
#define OP_COUNT (sizeof(g_aOpcodes)/sizeof(OPCODE))

// Extended 1804/5/6 opcode definitions ...
PRIVATE const OPCODE g_aExtendedOpcodes[] = {
  {"STPC", OP_STPC, 0xFF, OP_ARG_NONE},     // stop counter
  {"DTC",  OP_DTC,  0xFF, OP_ARG_NONE},     // decrement timer/counter
  {"SPM2", OP_SPM2, 0xFF, OP_ARG_NONE},     // set pulse width mode 2 and start
  {"SCM2", OP_SCM2, 0xFF, OP_ARG_NONE},     // set counter mode 2 and start
  {"SPM1", OP_SPM1, 0xFF, OP_ARG_NONE},     // set pulse width mode 1 and start
  {"SCM1", OP_SCM1, 0xFF, OP_ARG_NONE},     // set counter mode 1 and start
  {"LDC",  OP_LDC,  0xFF, OP_ARG_NONE},     // load counter
  {"STM",  OP_STM,  0xFF, OP_ARG_NONE},     // set timer mode and start
  {"GEC",  OP_GEC,  0xFF, OP_ARG_NONE},     // get counter
  {"ETQ",  OP_ETQ,  0xFF, OP_ARG_NONE},     // enable toggle Q
  {"XIE",  OP_XIE,  0xFF, OP_ARG_NONE},     // external interrupt enable
  {"XID",  OP_XID,  0xFF, OP_ARG_NONE},     // external interrupt disable
  {"CIE",  OP_CIE,  0xFF, OP_ARG_NONE},     // counter interrupt enable
  {"CID",  OP_CID,  0xFF, OP_ARG_NONE},     // counter interrupt disable
  {"DBNZ", OP_DBNZ, 0xF0, OP_ARG_R2BYTES},  // decrement reg N and long branch if not equal zero
  {"BCI",  OP_BCI,  0xFF, OP_ARG_1BYTE},    // short branch on counter interrupt
  {"BXI",  OP_BXI,  0xFF, OP_ARG_1BYTE},    // short branch on external interrupt
  {"RLXA", OP_RLXA, 0xF0, OP_ARG_REG},      // register load via X and advance
  {"DADC", OP_DADC, 0xFF, OP_ARG_NONE},     // decimal add with carry
  {"DSAV", OP_DSAV, 0xFF, OP_ARG_NONE},     // save T, D, DF
  {"DSMB", OP_DSMB, 0xFF, OP_ARG_NONE},     // decimal subtract memory with borrow
  {"DACI", OP_DACI, 0xFF, OP_ARG_1BYTE},    // decimal add with carry, immediate
  {"DSBI", OP_DSBI, 0xFF, OP_ARG_1BYTE},    // decimal subtract memory with borrow, immediate
  {"SCAL", OP_SCAL, 0xF0, OP_ARG_R2BYTES},  // standard call
  {"SRET", OP_SRET, 0xF0, OP_ARG_REG},      // standard return
  {"RSXD", OP_RSXD, 0xF0, OP_ARG_REG},      // register store via X and decrement
  {"RNX",  OP_RNX,  0xF0, OP_ARG_REG},      // register N to register X copy
  {"RLDI", OP_RLDI, 0xF0, OP_ARG_R2BYTES},  // register load immediate
  {"DADD", OP_DADD, 0xFF, OP_ARG_NONE},     // decimal add
  {"DSM",  OP_DSM,  0xFF, OP_ARG_NONE},     // decimal subtract memory
  {"DADI", OP_DADI, 0xFF, OP_ARG_1BYTE},    // decimal add immediate
  {"DSMI", OP_DSMI, 0xFF, OP_ARG_1BYTE},    // decimal subtract memory, immediate
};
#define OP_EXTENDED_COUNT (sizeof(g_aExtendedOpcodes)/sizeof(OPCODE))


PRIVATE size_t DisassembleFromTable (const OPCODE aOpcodes[], size_t caOpcodes,
                           const CMemory *pMemory, address_t nStart, string &sCode)
{
  //++
  //   Disassemble one instruction and return a string containg the result.
  // Since instructions are variable length, this can potentially require 1,
  // 2 or 3 bytes of data.  The memory address of the first byte should be
  // passed to the nStart parameter, and the return value is the number of
  // bytes actually used by the instruction.
  // 
  //   Note that this routine gets called recursively to decode the 1804/5/6
  // extended opcodes!
  //--
  uint8_t bOpcode = pMemory->CPUread(nStart);

  // Search the opcode table for a match ...
  const OPCODE *pOpcode = NULL;
  for (size_t i = 0;  i < caOpcodes;  ++i) {
    if ((bOpcode & aOpcodes[i].bMask) == aOpcodes[i].bOpcode) {
      pOpcode = &aOpcodes[i];  break;
    }
  }

  //   In the primary 1802 opcode table, all 256 possible opcodes are defined
  // and so there MUST have been a match.  However in the extended 1804/5/6
  // table there are lots of gaps, and it's very possible that we didn't find
  // a match...
  if (pOpcode == NULL) {
    sCode = "UNKNOWN";  return 1;
  }

  // Decode the operand(s) for this instruction ...
  uint8_t b2, b3;
  switch (pOpcode->nType) {

    // No operand - that's easy!
    case OP_ARG_NONE:
      sCode = pOpcode->pszName;
      return 1;

    // Single register number (GLO, PLO, LDN, etc) ...
    case OP_ARG_REG:
      FormatString(sCode, "%-4s R%X", pOpcode->pszName, (bOpcode&0xF));
      return 1;

    // 3 bit device address (INP and OUT) ...
    case OP_ARG_IO:
      FormatString(sCode, "%-4s %X", pOpcode->pszName, (bOpcode&7));
      return 1;
    
    // Single byte argument (ADI,  SMI, all branch instructions, etc) ...
    case OP_ARG_1BYTE:
      b2 = pMemory->CPUread(nStart+1);
      FormatString(sCode, "%-4s %02X", pOpcode->pszName, b2);
      return 2;

     // Two byte argument (long branch instructions) ...
    case OP_ARG_2BYTES:
      b2 = pMemory->CPUread(nStart+1);  b3 = pMemory->CPUread(nStart+2);
      FormatString(sCode, "%-4s %02X%02X", pOpcode->pszName, b2, b3);
      return 3;

    // Register number AND 2 bytes (RLDI, SCAL, DBNZ) ...
    case OP_ARG_R2BYTES:
      b2 = pMemory->CPUread(nStart+1);  b3 = pMemory->CPUread(nStart+2);
      FormatString(sCode, "%-4s R%X,%02X%02X", pOpcode->pszName, (bOpcode & 0xF), b2, b3);
      return 3;

    // 1804/5/6 extended opcode (just recurse!) ...
    case OP_ARG_EXTENDED:
      return DisassembleFromTable(g_aExtendedOpcodes, OP_EXTENDED_COUNT, pMemory, ADDRESS(nStart+1), sCode) + 1;

    // Should never happen, but just to make the compiler happy ...
    default:
      assert(false);  return 1;
  }
}

PUBLIC size_t Disassemble (const CMemory *pMemory, address_t nStart, string &sCode)
{
  //++
  //   Start with the primary 1802 opcode table, and work our way down to the
  // extended 1804/5/6 table if necessary ...
  //--
  return DisassembleFromTable(g_aOpcodes, OP_COUNT, pMemory, nStart, sCode);
}

size_t Assemble (CMemory *pMemory, const string &sCode, address_t nStart)
{
  //++
  //--
  return 0;
}
