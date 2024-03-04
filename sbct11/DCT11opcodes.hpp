//++
//Opcodes.hpp -> PDP11 opcodes, assembler and disassembler
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
//    This file contains PDP11 opcodes, mnemonics, and the function prototypes
// for the one line assembler and disassembler routines ...
//
// REVISION HISTORY:
//  4-MAR-20  RLA   Copied from the ELF2K project.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ std::string class, et al ...
using std::string;              // ...
class CMemory;                  // ...

// Opcode argument types ...
enum _OP_ARG_TYPES {
  OP_ARG_NONE,    // no arguments
  OP_ARG_DSTSRC,  // BOTH destination and source address
  OP_ARG_DST,     // destination only
  OP_ARG_SRC,     // source only
  OP_ARG_BRANCH,  // branch address
  OP_ARG_TRAP,    // trap address/number
  OP_ARG_XOR,     // special case for XOR
  OP_ARG_RTS,     //   "       "   "  RTS
  OP_ARG_SOB,     //   "       "      SOB
};
typedef enum _OP_ARG_TYPES OP_ARG_TYPE;

// Opcode definitions for the assember and disassembler ...
struct _OP_CODE {
  const char   *pszName;        // the mnemonic for the opcode
  uint16_t      wOpcode;        // the actual opcode
  uint16_t      wMask;          // mask of significant bits
  OP_ARG_TYPE   nType;          // argument/operand for this opcode
};
typedef struct _OP_CODE OP_CODE;

// Assemble or disassemble PDP11 instructions ...
extern uint16_t Disassemble (const CMemory *pMemory, uint16_t nStart, string &sCode);
extern size_t Assemble (CMemory *pMemory, const string &sCode, size_t nStart);
