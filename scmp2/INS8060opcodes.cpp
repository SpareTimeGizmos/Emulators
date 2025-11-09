//++
//INS8060opcodes.cpp -> SC/MP-II assembler and disassembler
//
// DESCRIPTION:
//   This file contains a table of ASCII mnemonics for SC/MP opcodes, and one
// line assembler and disassembler methods ...
//
// REVISION HISTORY:
// 13-FEB-20  RLA   Copied from the ELF2K project.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include "EMULIB.hpp"           // emulator library definitions
#include "SCMP2.hpp"             // global definitions for this project
#include "MemoryTypes.h"        // address_t and word_t data types
#include "Memory.hpp"           // main memory functions
#include "INS8060.hpp"          // SC/MP=II CPU emulation
#include "INS8060opcodes.hpp"   // declarations for this module

// SC/MP-II opcode definitions ...
PRIVATE const OP2_CODE g_aOpcodes[] = {
  // Miscellaneous instructions ...
  {"HALT", 0x00, OP2_MASK_NONE, OP2_ARG_NONE},
  {"XAE",  0x01, OP2_MASK_NONE, OP2_ARG_NONE},
  {"CCL",  0x02, OP2_MASK_NONE, OP2_ARG_NONE},
  {"SCL",  0x03, OP2_MASK_NONE, OP2_ARG_NONE},
  {"DINT", 0x04, OP2_MASK_NONE, OP2_ARG_NONE},
  {"IEN",  0x05, OP2_MASK_NONE, OP2_ARG_NONE},
  {"CSA",  0x06, OP2_MASK_NONE, OP2_ARG_NONE},
  {"CAS",  0x07, OP2_MASK_NONE, OP2_ARG_NONE},
  {"NOP",  0x08, OP2_MASK_NONE, OP2_ARG_NONE},
  // Shift and rotate instructions ...
  {"SIO",  0x19, OP2_MASK_NONE, OP2_ARG_NONE},
  {"SR",   0x1c, OP2_MASK_NONE, OP2_ARG_NONE},
  {"SRL",  0x1d, OP2_MASK_NONE, OP2_ARG_NONE},
  {"RR",   0x1e, OP2_MASK_NONE, OP2_ARG_NONE},
  {"RRL",  0x1f, OP2_MASK_NONE, OP2_ARG_NONE},
  // Extension register instructions ...
  {"LDE",  0x40, OP2_MASK_NONE, OP2_ARG_NONE},
  {"ANE",  0x50, OP2_MASK_NONE, OP2_ARG_NONE},
  {"ORE",  0x58, OP2_MASK_NONE, OP2_ARG_NONE},
  {"XRE",  0x60, OP2_MASK_NONE, OP2_ARG_NONE},
  {"DAE",  0x68, OP2_MASK_NONE, OP2_ARG_NONE},
  {"ADE",  0x70, OP2_MASK_NONE, OP2_ARG_NONE},
  {"CAE",  0x78, OP2_MASK_NONE, OP2_ARG_NONE},
  // Immediate instructions ...
  {"DLY",  0x8f, OP2_MASK_IMM, OP2_ARG_IMM},
  {"LDI",  0xc4, OP2_MASK_IMM, OP2_ARG_IMM},
  {"illegal", 0xcc, OP2_MASK_IMM, OP2_ARG_IMM},
  {"ANI",  0xd4, OP2_MASK_IMM, OP2_ARG_IMM},
  {"ORI",  0xdc, OP2_MASK_IMM, OP2_ARG_IMM},
  {"XRI",  0xe4, OP2_MASK_IMM, OP2_ARG_IMM},
  {"DAI",  0xec, OP2_MASK_IMM, OP2_ARG_IMM},
  {"ADI",  0xf4, OP2_MASK_IMM, OP2_ARG_IMM},
  {"CAI",  0xfc, OP2_MASK_IMM, OP2_ARG_IMM},
  // Pointer register instructions ...
  {"XPAL", 0x30, OP2_MASK_REG, OP2_ARG_REG},
  {"XPAH", 0x34, OP2_MASK_REG, OP2_ARG_REG},
  {"XPPC", 0x3c, OP2_MASK_REG, OP2_ARG_REG},
  // Transfer instructions ...
  {"JMP",  0x90, OP2_MASK_JMP, OP2_ARG_JMP},
  {"JP",   0x94, OP2_MASK_JMP, OP2_ARG_JMP},
  {"JZ",   0x98, OP2_MASK_JMP, OP2_ARG_JMP},
  {"JNZ",  0x9c, OP2_MASK_JMP, OP2_ARG_JMP},
  // Memory increment/decrement instructions ...
  {"ILD",  0xa8, OP2_MASK_JMP, OP2_ARG_JMP},
  {"DLD",  0xb8, OP2_MASK_JMP, OP2_ARG_JMP},
  // Memory Reference instructions - LOAD ...
  {"LD",   0xc0, OP2_MASK_MRI, OP2_ARG_MRI},
  {"ST",   0xc8, OP2_MASK_MRI, OP2_ARG_MRI},
  {"AND",  0xd0, OP2_MASK_MRI, OP2_ARG_MRI},
  {"OR",   0xd8, OP2_MASK_MRI, OP2_ARG_MRI},
  {"XOR",  0xe0, OP2_MASK_MRI, OP2_ARG_MRI},
  {"DAD",  0xe8, OP2_MASK_MRI, OP2_ARG_MRI},
  {"ADD",  0xf0, OP2_MASK_MRI, OP2_ARG_MRI},
  {"CAD",  0xf8, OP2_MASK_MRI, OP2_ARG_MRI},
};
#define OP2_COUNT (sizeof(g_aOpcodes)/sizeof(OP2_CODE))

// SC/MP pointer register names ...
PRIVATE const char *g_apszRegisters2[] = {"PC", "P1", "P2", "P3"};

PRIVATE string DisassembleMRI (address_t wAddress, uint8_t bOpcode, const OP2_CODE *pOpcode, int8_t iDisplacement)
{
  //++
  //   This routine will disassemble MRI and transfer (JMP/ILD/DLD) format
  // instructions.  As far as disassembling goes, the only difference between
  // the two is that MRIs have the autoindex (M) bit and the others do not.
  // Otherwise the pointer and displacement format and calculations are the
  // same.  
  //
  //   A couple of things make these instructions unique. For one thing, the
  // displacements are signed values and are always printed in decimal.  For
  // another, if the index register is the PC then we actually calculate the
  // absolute target address based on the location of this instruction and the
  // displacement, and then print that.
  //
  //   Oh, and a third special case is that if the displacement is exactly -128
  // (0x80) then the E register is used as the displacement instead!
  //--
  uint8_t nP = OP2_GET_P(bOpcode);
  bool fPrintAt = (pOpcode->nType == OP2_ARG_MRI) && ((OP2_GET_M(bOpcode) != 0));

  if ((nP == 0) && (iDisplacement != -128)) {
    // PC relative - calculate the absolute target address ...
    //   Note that if P is zero and this is an MRI, then we don't have to worry
    // about the M bit - that's because P==0 and M==1 is immediate mode, and
    // those have already been separated out.  Aso notice the "+2" correction -
    // one "+1" is because the address is relative to the second byte of the
    // instruction, not the first.  Another +1 is because the SC/MP increments
    // the PC _before_ every fetch (not after!).  The assembler corrects for
    // this by subtracting 1 from every target address, so we need to add one
    // to get back the address the programmer originally entered.
    address_t wTarget = (wAddress & 0xF000) | ((wAddress+iDisplacement+2) & 0x0FFF);
    return FormatString("%s\t0x%04X", pOpcode->pszName, wTarget);
  } else if (iDisplacement == -128) {
    //   The displacement is the E register.  There's no official National
    // mnemonic for that AFAIK, so I just made up my own!
    return FormatString("%s\t%sEREG(%s)", pOpcode->pszName, fPrintAt ? "@" : "", g_apszRegisters2[OP2_GET_P(bOpcode)]);
  } else {
    // Otherwise print the offset as a signed decimal value ...
    return FormatString("%s\t%s%d(%s)", pOpcode->pszName, fPrintAt ? "@" : "", iDisplacement, g_apszRegisters2[OP2_GET_P(bOpcode)]);
  }
}

PUBLIC size_t Disassemble2 (const CMemory *pMemory, size_t nStart, string &sCode)
{
  //++
  //   Disassemble one instruction and return a string containg the result.
  // Since instructions are variable length, this can potentially require 1,
  // 2 or 3 bytes of data.  The memory address of the first byte should be
  // passed to the nStart parameter, and the return value is the number of
  // bytes actually used by the instruction.
  //--
  uint8_t bOpcode = pMemory->CPUread(ADDRESS(nStart));
  uint8_t bData;  size_t cbOpcode;

  // Search the opcode table for a match ...
  const OP2_CODE *pOpcode = NULL;
  for (size_t i = 0;  i < OP2_COUNT;  ++i) {
    if ((bOpcode & g_aOpcodes[i].bMask) == g_aOpcodes[i].bOpcode) {
      pOpcode = &g_aOpcodes[i];  break;
    }
  }

  // If there's no match then it's not a valid opcode ...
  if (pOpcode == NULL) {
    sCode = FormatString("invalid opcode");  return 1;
  }

  // If this opcode is two bytes long, fetch the second byte ...
  switch (pOpcode->nType) {
    case OP2_ARG_IMM:
    case OP2_ARG_JMP:
    case OP2_ARG_MRI:
      bData = pMemory->CPUread(CSCMP2::INC12((address_t) nStart));  cbOpcode = 2;  break;
    default:
      bData = 0;  cbOpcode = 1;  break;
  }

  // Decode the operand ...
  switch (pOpcode->nType) {
    case OP2_ARG_NONE:
      sCode = pOpcode->pszName;  break;
    case OP2_ARG_REG:
      sCode = FormatString("%s\t%s", pOpcode->pszName, g_apszRegisters2[OP2_GET_P(bOpcode)]);
      break;
    case OP2_ARG_IMM:
      sCode = FormatString("%s\t#0x%02X", pOpcode->pszName, bData);
      break;
    case OP2_ARG_JMP:
    case OP2_ARG_MRI:
      sCode = DisassembleMRI((address_t) nStart, bOpcode, pOpcode, bData);
      break;
    default:
      sCode = "opcode not implemented!";  break;
  }

  // Return the number of bytes (it's always either 1 or 2) and we're done ...
  return cbOpcode;
}

size_t Assemble2 (CMemory *pMemory, const string &sCode, size_t nStart)
{
  //++
  //--
  return 0;
}
