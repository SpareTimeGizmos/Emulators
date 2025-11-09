//++
//INS8070opcodes.cpp -> SC/MP-III assembler and disassembler
//
// DESCRIPTION:
//   This file contains a table of ASCII mnemonics for the SC/MP-III opcodes,
// and one line assembler and disassembler methods ...
//
// REVISION HISTORY:
// 29-OCT-25  RLA   New file.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include "EMULIB.hpp"           // emulator library definitions
#include "SCMP3.hpp"             // global definitions for this project
#include "MemoryTypes.h"        // address_t and word_t data types
#include "Memory.hpp"           // main memory functions
#include "INS8070.hpp"          // SC/MP-III CPU emulation
#include "INS8070opcodes.hpp"   // declarations for this module

// SC/MP-II opcode definitions ...
PRIVATE const OP3_CODE g_aOpcodes3[] = {
  // All opcodes with implied addressing (i.e. no operands!) ...
  {"ADD\tA,E",		0x70, OP3_MASK_NONE, OP3_ARG_NONE},
  {"SUB\tA,E",		0x78, OP3_MASK_NONE, OP3_ARG_NONE},
  {"AND\tA,E",		0x50, OP3_MASK_NONE, OP3_ARG_NONE},
  {"OR\tA,E",		0x58, OP3_MASK_NONE, OP3_ARG_NONE},
  {"XOR\tA,E",		0x60, OP3_MASK_NONE, OP3_ARG_NONE},
  {"LD\tS,A",		0x07, OP3_MASK_NONE, OP3_ARG_NONE},
  {"LD\tA,S",		0x06, OP3_MASK_NONE, OP3_ARG_NONE},
  {"LD\tA,E",		0x40, OP3_MASK_NONE, OP3_ARG_NONE},
  {"LD\tE,A",		0x48, OP3_MASK_NONE, OP3_ARG_NONE},
  {"XCH\tA,E",		0x01, OP3_MASK_NONE, OP3_ARG_NONE},
  {"LD\tT,EA",		0x09, OP3_MASK_NONE, OP3_ARG_NONE},
  {"LD\tEA,T",		0x0B, OP3_MASK_NONE, OP3_ARG_NONE},
  {"PUSH\tA",		0x0A, OP3_MASK_NONE, OP3_ARG_NONE},
  {"PUSH\tEA",		0x08, OP3_MASK_NONE, OP3_ARG_NONE},
  {"POP\tA",		0x38, OP3_MASK_NONE, OP3_ARG_NONE},
  {"POP\tEA",		0x3A, OP3_MASK_NONE, OP3_ARG_NONE},
  {"RET",		0x5C, OP3_MASK_NONE, OP3_ARG_NONE},
  {"RR\tA",		0x3E, OP3_MASK_NONE, OP3_ARG_NONE},
  {"RRL\tA",		0x3F, OP3_MASK_NONE, OP3_ARG_NONE},
  {"SR\tA",		0x3C, OP3_MASK_NONE, OP3_ARG_NONE},
  {"SRL\tA",		0x3D, OP3_MASK_NONE, OP3_ARG_NONE},
  {"SL\tA",		0x0E, OP3_MASK_NONE, OP3_ARG_NONE},
  {"SR\tEA",		0x0C, OP3_MASK_NONE, OP3_ARG_NONE},
  {"SL\tEA",		0x0F, OP3_MASK_NONE, OP3_ARG_NONE},
  {"DIV\tEA,T",		0x0D, OP3_MASK_NONE, OP3_ARG_NONE},
  {"MPY\tEA,T",		0x2C, OP3_MASK_NONE, OP3_ARG_NONE},
  {"NOP",		0x00, OP3_MASK_NONE, OP3_ARG_NONE},
  // Instructions with immediate addressing ...
  {"ADD\tA,#",		0xF4, OP3_MASK_NONE, OP3_ARG_IMM8},
  {"ADD\tEA,#",		0xB4, OP3_MASK_NONE, OP3_ARG_IMM16},
  {"SUB\tA,#",		0xFC, OP3_MASK_NONE, OP3_ARG_IMM8},
  {"SUB\tEA,#",		0xBC, OP3_MASK_NONE, OP3_ARG_IMM16},
  {"AND\tA,#",		0xD4, OP3_MASK_NONE, OP3_ARG_IMM8},
  {"AND\tS,#",		0x39, OP3_MASK_NONE, OP3_ARG_IMM8},
  {"OR\tA,#",		0xDC, OP3_MASK_NONE, OP3_ARG_IMM8},
  {"OR\tS,#",		0x3B, OP3_MASK_NONE, OP3_ARG_IMM8},
  {"XOR\tA,#",		0xE4, OP3_MASK_NONE, OP3_ARG_IMM8},
  {"LD\tA,#",		0xC4, OP3_MASK_NONE, OP3_ARG_IMM8},
  {"LD\tEA,#",		0x84, OP3_MASK_NONE, OP3_ARG_IMM16},
  {"LD\tT,#",		0xA4, OP3_MASK_NONE, OP3_ARG_IMM16},
  // Instructions with direct addressing ...
  {"ADD\tA,",		0xF5, OP3_MASK_NONE, OP3_ARG_DIRECT},
  {"ADD\tEA,",		0xB5, OP3_MASK_NONE, OP3_ARG_DIRECT},
  {"SUB\tA,",		0xFD, OP3_MASK_NONE, OP3_ARG_DIRECT},
  {"SUB\tEA,",		0xBD, OP3_MASK_NONE, OP3_ARG_DIRECT},
  {"AND\tA,",		0xD5, OP3_MASK_NONE, OP3_ARG_DIRECT},
  {"OR\tA,",		0xDD, OP3_MASK_NONE, OP3_ARG_DIRECT},
  {"XOR\tA,",		0xE5, OP3_MASK_NONE, OP3_ARG_DIRECT},
  {"DLD\tA,",		0x9D, OP3_MASK_NONE, OP3_ARG_DIRECT},
  {"ILD\tA,",		0x95, OP3_MASK_NONE, OP3_ARG_DIRECT},
  {"LD\tA,",		0xC0, OP3_MASK_NONE, OP3_ARG_DIRECT},
  {"LD\tA,",		0xC5, OP3_MASK_NONE, OP3_ARG_DIRECT},
  {"LD\tEA,",		0x85, OP3_MASK_NONE, OP3_ARG_DIRECT},
  {"LD\tT,",		0xA5, OP3_MASK_NONE, OP3_ARG_DIRECT},
  {"ST\tA,",		0xCD, OP3_MASK_NONE, OP3_ARG_DIRECT},
  {"ST\tEA,",		0x8D, OP3_MASK_NONE, OP3_ARG_DIRECT},
  {"BND\t",		0x2D, OP3_MASK_NONE, OP3_ARG_BRANCH},
  // Branch instructions (relative addressing) ...
  {"BNZ\t",		0x7C, OP3_MASK_REG,  OP3_ARG_BRANCH},
  {"BP\t",		0x64, OP3_MASK_REG,  OP3_ARG_BRANCH},
  {"BZ\t",		0x6C, OP3_MASK_REG,  OP3_ARG_BRANCH},
  {"BRA\t",		0x74, OP3_MASK_REG,  OP3_ARG_BRANCH},
  // Instructions with memory addressing ...
  {"ADD\tA,",		0xF0, OP3_MASK_MEMORY, OP3_ARG_MEMORY},
  {"ADD\tEA,",		0xB0, OP3_MASK_MEMORY, OP3_ARG_MEMORY},
  {"SUB\tA,",		0xF8, OP3_MASK_MEMORY, OP3_ARG_MEMORY},
  {"SUB\tEA,",		0xB8, OP3_MASK_MEMORY, OP3_ARG_MEMORY},
  {"AND\tA,",		0xD0, OP3_MASK_MEMORY, OP3_ARG_MEMORY},
  {"OR\tA,",		0xD8, OP3_MASK_MEMORY, OP3_ARG_MEMORY},
  {"XOR\tA,",		0xE0, OP3_MASK_MEMORY, OP3_ARG_MEMORY},
  {"DLD\tA,",		0x98, OP3_MASK_MEMORY, OP3_ARG_MEMORY},
  {"ILD\tA,",		0x90, OP3_MASK_MEMORY, OP3_ARG_MEMORY},
  {"LD\tA,",		0xC0, OP3_MASK_MEMORY, OP3_ARG_MEMORY},
  {"LD\tEA,",		0x80, OP3_MASK_MEMORY, OP3_ARG_MEMORY},
  {"LD\tT,",		0xA0, OP3_MASK_MEMORY, OP3_ARG_MEMORY},
  {"ST\tA,",		0xC8, OP3_MASK_MEMORY, OP3_ARG_MEMORY},
  {"ST\tEA,",		0x88, OP3_MASK_MEMORY, OP3_ARG_MEMORY},
  // JMP, JSR and CALL instructions ...
  {"JMP\t",		0x24, OP3_MASK_NONE, OP3_ARG_JUMP},
  {"JSR\t",		0x20, OP3_MASK_NONE, OP3_ARG_JUMP},
  {"CALL\t",		0x10, OP3_MASK_CALL, OP3_ARG_CALL},
  // Register instructions ...
  {"LD\t",		0x44, OP3_MASK_REG, OP3_ARG_REGEA}, 
  {"LD\tEA,",		0x30, OP3_MASK_REG, OP3_ARG_REG}, 
  {"XCH\tEA,",		0x4C, OP3_MASK_REG, OP3_ARG_REG},
  {"PUSH\t",		0x54, OP3_MASK_REG, OP3_ARG_REG},
  {"POP\t",		0x5C, OP3_MASK_REG, OP3_ARG_REG},
  {"SSM\t",		0x2C, OP3_MASK_REG, OP3_ARG_REG},
  // Register immediate instructions ...
  {"LD\t",		0x24, OP3_MASK_REG, OP3_ARG_REGIMM16},
  {"PLI\t",		0x20, OP3_MASK_REG, OP3_ARG_REGIMM16},
};
#define OP3_COUNT (sizeof(g_aOpcodes3)/sizeof(OP3_CODE))

// SC/MP-III pointer register names ...
PRIVATE const char *g_apszRegisters3[] = {"PC", "SP", "P2", "P3"};
#define REGNAME(o) g_apszRegisters3[OP3_GET_REG(o)]

PRIVATE const OP3_CODE *FindOpcode (uint8_t bOpcode)
{
  //++
  // Search the opcode table for a match ...
  //--
  const OP3_CODE *pOpcode = NULL;
  for (size_t i = 0;  i < OP3_COUNT;  ++i) {
    if ((bOpcode & g_aOpcodes3[i].bMask) == g_aOpcodes3[i].bOpcode) {
      pOpcode = &g_aOpcodes3[i];  break;
    }
  }
  return pOpcode;
}

PRIVATE size_t FetchOperand (const OP3_CODE *pOpcode, const CMemory *pMemory, size_t nPC, uint16_t &wData)
{
  //++
  //    This routine will fetch the operand for the given opcode, which might
  // be any of one byte, two bytes, or none.  It returns the number of bytes in
  // the operand (so the total instruction length is this plus one, for the
  // opcode!).
  // 
  //    IMPORTANT - if the operand is 8 bits, then we sign extend it to 16
  // for the return value!  Some of the code that calculates displacements
  // depends on this...
  //--
  size_t cbOperand = 0;  uint8_t b1, b2;

  // If this opcode is 2 or 3 bytes long, then fetch the operand ...
  switch (pOpcode->nType) {
    case OP3_ARG_IMM8:
    case OP3_ARG_DIRECT:
    case OP3_ARG_MEMORY:
    case OP3_ARG_BRANCH:
      // All these take an 8 bit operand ...
      wData = pMemory->CPUread(ADDRESS(nPC+1));
      if ((wData & 0x80) != 0) wData |= 0xFF00;
      cbOperand = 1;  break;
    case OP3_ARG_IMM16:
    case OP3_ARG_REGIMM16:
    case OP3_ARG_JUMP:
      // These types take a two byte 16 bit operand ...
      b1 = pMemory->CPUread(ADDRESS(nPC+1));
      b2 = pMemory->CPUread(ADDRESS(nPC+2));
      wData = MKWORD(b2, b1);  cbOperand = 2;  break;
    default:
     // And everything else has no operand ...
     wData = 0;  cbOperand = 0;  break;
  }
  return cbOperand;
}

PUBLIC size_t Disassemble3 (const CMemory *pMemory, size_t nPC, string &sCode)
{
  //++
  //   Disassemble one instruction and return a string containg the result.
  // Since instructions are variable length, this can potentially require 1,
  // 2 or 3 bytes of data.  The memory address of the first byte should be
  // passed to the nPC parameter, and the return value is the number of
  // bytes actually used by the instruction.
  //--
  uint16_t wOperand;  size_t cbOpcode;  const OP3_CODE *pOpcode;

  // If there's no match then it's not a valid opcode ...
  uint8_t bOpcode = pMemory->CPUread(ADDRESS(nPC));
  if ((pOpcode = FindOpcode(bOpcode)) == NULL) {
    sCode = FormatString("invalid opcode");  return 1;
  }

  // Fetch the operand, if any ...
  cbOpcode = FetchOperand(pOpcode, pMemory, nPC, wOperand) + 1;

  // And print it ...
  switch (pOpcode->nType) {
    case OP3_ARG_NONE:
      // Implied addressing, no (additional) operands - this is easy!
      sCode = FormatString("%s", pOpcode->pszName);  break;
 
    case OP3_ARG_IMM8:
      // 8 bit (one byte) immediate operand ...
      sCode = FormatString("%s$%02X", pOpcode->pszName, LOBYTE(wOperand));  break;

    case OP3_ARG_IMM16:
      // 16 bit (two byte) immediate operand ...
      sCode = FormatString("%s$%04X", pOpcode->pszName, wOperand);  break;

    case OP3_ARG_REGIMM16:
      // A register name followed by a 16 bit operand (e.g. "PLI r,#imm") ...
      sCode = FormatString("%s%s,$%04X", pOpcode->pszName, REGNAME(bOpcode), wOperand);  break;

    case OP3_ARG_DIRECT:
      // Direct memory addressing (address in the range $FFxx) ...
      sCode = FormatString("%s$FF%02X", pOpcode->pszName, LOBYTE(wOperand));  break;

    case OP3_ARG_JUMP:
      //  JMP and JSR instructions just take a simple 16 bit absolute address
      // for the operand, BUT because of the stupid SC/MP pre-increment of the
      // PC the argument is actually one less than the actual destination!
      sCode = FormatString("%s$%04X", pOpcode->pszName, wOperand+1);  break;

    case OP3_ARG_CALL:
      // CALL instructions have a 4 bit operand in the low nibble of the opcode ...
      sCode = FormatString("%s$%01X", pOpcode->pszName, LONIBBLE(bOpcode));  break;

    case OP3_ARG_REG:
      // The argument is a single register name (e.g. "PUSH r", "POP r", etc) ...
      sCode = FormatString("%s%s", pOpcode->pszName, REGNAME(bOpcode));  break;

    case OP3_ARG_REGEA:
      // Identical to REG, except that we print "EA" after ("LD r,EA") ...
      sCode = FormatString("%s%s,EA", pOpcode->pszName, REGNAME(bOpcode));  break;

    case OP3_ARG_BRANCH:
      //   Branch instructions always use register and displacement addressing,
      // but if the register is the PC then we calculate the actual destination
      // address and print that, like a JMP or JSR instruction.  If the base
      // isn't the PC though, then we just print the displacement and index
      // register as-is.  Note that this has the same PC pre-increment problem
      // as JMP/JSR, EXCEPT that in this case the PC also points to the second
      // byte of the instruction.  
      //
      //   WARNING - mega hack follows!  The opcode BND, $2D, uses implied PC
      // relative branch style addressing BUT the lower 2 bits of that opcode
      // are 01 which would select the SP.  We just do a special case for it!
      if ((OP3_GET_REG(bOpcode) == 0) || (bOpcode == 0x2D))
        sCode = FormatString("%s$%04X", pOpcode->pszName, ADDRESS(nPC+wOperand+2));
      else
        sCode = FormatString("%s$%02X[%s]", pOpcode->pszName, LOBYTE(wOperand), REGNAME(OP3_GET_REG(bOpcode)));
      break;

    case OP3_ARG_MEMORY:
      //   And lastly, memory addressing which is the hardest case, but note
      // that we've already taken care of the special cases for immediate and
      // direct addressing.  If the base register is the PC then, like BRANCH,
      // we calculate the actual address and print that.  Otherwise we print
      // the displacement and the index register, and "@" if it is indirect.
      if (OP3_GET_REG(bOpcode) == 0)
        sCode = FormatString("%s$%04X", pOpcode->pszName, ADDRESS(nPC+wOperand+2));
      else
        sCode = FormatString("%s%s$%02X[%s]", pOpcode->pszName,
          OP3_IS_IND(bOpcode) ? "@" : "", LOBYTE(wOperand), REGNAME(bOpcode));
      break;
  }

  // Return the number of bytes in this instruction and we're done ...
  return cbOpcode;
}

size_t Assemble3 (CMemory *pMemory, const string &sCode, size_t nPC)
{
  //++
  //--
  return 0;
}
