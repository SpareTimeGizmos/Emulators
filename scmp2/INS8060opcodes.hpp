//++
//INS8060opcodes.hpp -> SC/MP-II opcodes, assembler and disassembler
//
// DESCRIPTION:
//    This file contains SC/MP-II opcodes, mnemonics, and the function prototypes
// for the one line assembler and disassembler routines ...
//
// REVISION HISTORY:
// 13-FEB-20  RLA   Copied from the ELF2K project.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ std::string class, et al ...
#include "MemoryTypes.h"        // address_t and word_t data types
using std::string;              // ...
class CMemory;                  // ...

// Extract the pointer (P) and autoindex (M) fields from an opcode ...
#define OP2_GET_P(o)   ((o) & 0x3)
#define OP2_GET_M(o)   (((o) >> 2) & 1)

// Opcode argument types ...
enum _OP2_ARG_TYPES {
  OP2_ARG_NONE   = 0,           // no argument at all
  OP2_ARG_IMM    = 1,           // immediate addressing
  OP2_ARG_REG    = 2,           // register (XPAH, XPAL, XPPC)
  OP2_ARG_JMP    = 3,           // transfer, increment/decrement
  OP2_ARG_MRI    = 4,           // memory reference instruction
};
typedef enum _OP2_ARG_TYPES OP2_ARG_TYPE;

// Masks for opcodes (these eliminate the register and indirect bits) ...
enum _OP2_ARG_MASKS {
  OP2_MASK_NONE  = 0xFF,        // no argument
  OP2_MASK_IMM   = 0xFF,        // immediate addressing
  OP2_MASK_REG   = 0xFC,        // register (XPAH, XPAL, XPPC)
  OP2_MASK_JMP   = 0xFC,        // transfer, increment/decrement
  OP2_MASK_MRI   = 0xF8,        // memory reference
};
typedef enum _OP2_ARG_MASKS OP2_ARG_MASK;

// Opcode definitions for the assember and disassembler ...
struct _OP2_CODE {
  const char   *pszName;        // the mnemonic for the opcode
  uint8_t       bOpcode;        // the actual opcode
  OP2_ARG_MASK  bMask;          // mask of significant bits
  OP2_ARG_TYPE  nType;          // argument/operand for this opcode
};
typedef struct _OP2_CODE OP2_CODE;

// Assemble or disassemble SC/MP instructions ...
extern size_t Disassemble2 (const CMemory *pMemory, size_t nStart, string &sCode);
extern size_t Assemble2 (CMemory *pMemory, const string &sCode, size_t nStart);
