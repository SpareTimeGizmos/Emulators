//++
//INS8070opcodes.hpp -> SC/MP-III opcodes, assembler and disassembler
//
// DESCRIPTION:
//    This file contains SC/MP-III opcodes, mnemonics, and the function
// prototypes for the one line assembler and disassembler routines ...
//
// REVISION HISTORY:
// 29-OCT-25  RLA   New file.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ std::string class, et al ...
#include "MemoryTypes.h"        // address_t and word_t data types
using std::string;              // ...
class CMemory;                  // ...

// Extract the register (R) and indirect (@) fields from an opcode ...
#define OP3_GET_REG(o)   ((o) & 0x3)
#define OP3_IS_IND(o)    ((((o) >> 2) & 1) != 0)

// Opcode argument types ...
enum _OP3_ARG_TYPES {
  OP3_ARG_NONE,                 // no argument at all
  OP3_ARG_IMM8,                 //  8 bit immediate operand
  OP3_ARG_IMM16,                // 16  "    "    "    "  "
  OP3_ARG_REGIMM16,             // register with 16 bit immediate
  OP3_ARG_DIRECT,               // direct addressing
  OP3_ARG_MEMORY,               // memory addressing
  OP3_ARG_BRANCH,               // relative branch instruction
  OP3_ARG_JUMP,                 // absolute address -1 for JMP/JSR
  OP3_ARG_CALL,                 // four bit CALL operand
  OP3_ARG_REG,                  // a single register
  OP3_ARG_REGEA,                // a single register and then "EA"
};
typedef enum _OP3_ARG_TYPES OP3_ARG_TYPE;

// Masks for opcodes (these eliminate the register and indirect bits) ...
enum _OP3_ARG_MASKS {
  OP3_MASK_NONE    = 0xFF,      // no argument or implied addressing
  OP3_MASK_MEMORY  = 0xF8,      // memory reference
  OP3_MASK_REG     = 0xFC,      // a single register
  OP3_MASK_CALL    = 0xF0,      // CALL instruction
};
typedef enum _OP3_ARG_MASKS OP3_ARG_MASK;

// Opcode definitions for the assember and disassembler ...
struct _OP3_CODE {
  const char   *pszName;        // the mnemonic for the opcode
  uint8_t       bOpcode;        // the actual opcode
  OP3_ARG_MASK  bMask;          // mask of significant bits
  OP3_ARG_TYPE  nType;          // argument/operand for this opcode
};
typedef struct _OP3_CODE OP3_CODE;

// Assemble or disassemble SC/MP-III instructions ...
extern size_t Disassemble3 (const CMemory *pMemory, size_t nPC, string &sCode);
extern size_t Assemble3 (CMemory *pMemory, const string &sCode, size_t nPC);
