//++
//Opcodes.hpp -> 8085 opcodes, assembler and disassembler
//
// DESCRIPTION:
//    This file contains 8085 opcodes, mnemonics, and the function prototypes
// for the one line assembler and disassembler routines ...
//
// REVISION HISTORY:
// 19-FEB-20  RLA   Copied from the ELF2K project.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ std::string class, et al ...
#include "MemoryTypes.h"        // address_t and word_t data types
using std::string;              // ...
class CMemory;                  // ...

// Opcode argument types ...
//   Note that in some instances these values may be ORed together - for
// example "MVI r, imm8" has both DST and IMM8 aguments ...
enum _OP_ARG_TYPES {
  // Opcodes which DON'T use the register (RR) field ...
  OP_ARG_1E,      // no argument at all
  OP_ARG_2EI,     // immediate operand
  OP_ARG_2ER,     // relative address
  OP_ARG_3EB,     // absolute branch address
  // Opcodes which DO use the register field ...
  OP_ARG_1Z,      // register only
  OP_ARG_1ZCC,    // condition code only
  OP_ARG_2R,      // register and relative address
  OP_ARG_2RCC,    // condition code and relative address
  OP_ARG_2I,      // register and immediate operand
  OP_ARG_3A,      // register and absolute address
  OP_ARG_3B,      // register and absolute branch address
  OP_ARG_3BCC,    // condition code and absolute branch address
};
typedef enum _OP_ARG_TYPES OP_ARG_TYPE;

// Opcode definitions for the assember and disassembler ...
struct _OP_CODE {
  const char   *pszName;        // the mnemonic for the opcode
  uint8_t       bOpcode;        // the actual opcode
  uint8_t       bMask;          // mask of significant bits
  OP_ARG_TYPE   nType;          // argument/operand for this opcode
};
typedef struct _OP_CODE OP_CODE;

// Assemble or disassemble 8085 instructions ...
extern uint16_t Disassemble (const CMemory *pMemory, uint16_t nStart, string &sCode);
extern size_t Assemble (CMemory *pMemory, const string &sCode, size_t nStart);
