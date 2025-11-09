//++
//S2650opcodes.cpp -> 2650 assembler and disassembler
//
// DESCRIPTION:
//   This file contains a table of ASCII mnemonics for 2650 opcodes, and one
// line assembler and disassembler methods ...
//
// REVISION HISTORY:
// 24-FEB-20  RLA   New file.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include "EMULIB.hpp"           // emulator library definitions
#include "SBC50.hpp"            // global definitions for this project
#include "MemoryTypes.h"        // address_t and word_t data types
#include "Memory.hpp"           // main memory functions
#include "S2650.hpp"            // 2650 CPU emulation
#include "S2650opcodes.hpp"     // declarations for this module

// 2650 opcode definitions ...
PRIVATE const OP_CODE g_aOpcodes[] = {
  {"HALT", 0100,   0377, OP_ARG_1E},
  {"NOP",  0300,   0377, OP_ARG_1E},
  {"LPSU", 0222,   0377, OP_ARG_1E},
  {"LPSL", 0223,   0377, OP_ARG_1E},
  {"SPSU", 0022,   0377, OP_ARG_1E},
  {"SPSL", 0023,   0377, OP_ARG_1E},
  {"CPSU", 0164,   0377, OP_ARG_2EI},
  {"CPSL", 0165,   0377, OP_ARG_2EI},
  {"PPSU", 0166,   0377, OP_ARG_2EI},
  {"PPSL", 0167,   0377, OP_ARG_2EI},
  {"TPSU", 0264,   0377, OP_ARG_2EI},
  {"TPSL", 0265,   0377, OP_ARG_2EI},
  {"ZBRR", 0233,   0377, OP_ARG_2ER},
  {"BXA",  0237,   0377, OP_ARG_3EB},
  {"ZBSR", 0273,   0377, OP_ARG_2ER},
  {"BSXA", 0277,   0377, OP_ARG_3EB},
  {"ADDA", 043<<2, 0374, OP_ARG_3A},
  {"ADDI", 041<<2, 0374, OP_ARG_2I},
  {"ADDR", 042<<2, 0374, OP_ARG_2R},
  {"ADDZ", 040<<2, 0374, OP_ARG_1Z},
  {"ANDA", 023<<2, 0374, OP_ARG_3A},
  {"ANDI", 021<<2, 0374, OP_ARG_2I},
  {"ANDR", 022<<2, 0374, OP_ARG_2R},
  {"ANDZ", 020<<2, 0374, OP_ARG_1Z},
  {"BCFA", 047<<2, 0374, OP_ARG_3BCC},
  {"BCFR", 046<<2, 0374, OP_ARG_2RCC},
  {"BCTA", 007<<2, 0374, OP_ARG_3BCC},
  {"BCTR", 006<<2, 0374, OP_ARG_2RCC},
  {"BDRA", 077<<2, 0374, OP_ARG_3B},
  {"BDRR", 076<<2, 0374, OP_ARG_2R},
  {"BIRA", 067<<2, 0374, OP_ARG_3B},
  {"BIRR", 066<<2, 0374, OP_ARG_2R},
  {"BRNA", 027<<2, 0374, OP_ARG_3B},
  {"BRNR", 026<<2, 0374, OP_ARG_2R},
  {"BSFA", 057<<2, 0374, OP_ARG_3BCC},
  {"BSFR", 056<<2, 0374, OP_ARG_2RCC},
  {"BSNA", 037<<2, 0374, OP_ARG_3BCC},
  {"BSNR", 036<<2, 0374, OP_ARG_2RCC},
  {"BSTA", 017<<2, 0374, OP_ARG_3BCC},
  {"BSTR", 016<<2, 0374, OP_ARG_2RCC},
  {"COMA", 073<<2, 0374, OP_ARG_3A},
  {"COMI", 071<<2, 0374, OP_ARG_2I},
  {"COMR", 072<<2, 0374, OP_ARG_2R},
  {"COMZ", 070<<2, 0374, OP_ARG_1Z},
  {"DAR",  045<<2, 0374, OP_ARG_1Z},
  {"EORA", 013<<2, 0374, OP_ARG_3A},
  {"EORI", 011<<2, 0374, OP_ARG_2I},
  {"EORR", 012<<2, 0374, OP_ARG_2R},
  {"EORZ", 010<<2, 0374, OP_ARG_1Z},
  {"IORA", 033<<2, 0374, OP_ARG_3A},
  {"IORI", 031<<2, 0374, OP_ARG_2I},
  {"IORR", 032<<2, 0374, OP_ARG_2R},
  {"IORZ", 030<<2, 0374, OP_ARG_1Z},
  {"LODA", 003<<2, 0374, OP_ARG_3A},
  {"LODI", 001<<2, 0374, OP_ARG_2I},
  {"LODR", 002<<2, 0374, OP_ARG_2R},
  {"LODZ", 000<<2, 0374, OP_ARG_1Z},
  {"REDC", 014<<2, 0374, OP_ARG_1Z},
  {"REDD", 034<<2, 0374, OP_ARG_1Z},
  {"REDE", 025<<2, 0374, OP_ARG_2I},
  {"RETC", 005<<2, 0374, OP_ARG_1ZCC},
  {"RETE", 015<<2, 0374, OP_ARG_1ZCC},
  {"RRL",  064<<2, 0374, OP_ARG_1Z},
  {"RRR",  024<<2, 0374, OP_ARG_1Z},
  {"STRA", 063<<2, 0374, OP_ARG_3A},
  {"STRR", 062<<2, 0374, OP_ARG_2R},
  {"STRZ", 060<<2, 0374, OP_ARG_1Z},
  {"SUBA", 053<<2, 0374, OP_ARG_3A},
  {"SUBI", 051<<2, 0374, OP_ARG_2I},
  {"SUBR", 052<<2, 0374, OP_ARG_2R},
  {"SUBZ", 050<<2, 0374, OP_ARG_1Z},
  {"TMI",  075<<2, 0374, OP_ARG_2I},
  {"WRTC", 054<<2, 0374, OP_ARG_1Z},
  {"WRTD", 074<<2, 0374, OP_ARG_1Z},
  {"WRTE", 065<<2, 0374, OP_ARG_2I},
};
#define OP_COUNT (sizeof(g_aOpcodes)/sizeof(OP_CODE))

// 2650 register names, pairs and condition codes ...
PRIVATE const char *g_apszConditions[4] = {"EQ", "GT", "LT", "UN"};

PRIVATE string ShowRelativeAddress (address_t nStart, uint8_t bOffset)
{
  //++
  // Disassemble a relative address ...
  //--
  address_t wEA = C2650::ADD13(nStart, C2650::SXT16(bOffset)+2);
  if (ISSET(bOffset, 0x80))
    return FormatString("*0x%04X", wEA);
  else
    return FormatString("0x%04X", wEA);
}

PRIVATE string ShowBranchAddress (address_t wEA)
{
  //++
  // Disassemble an absolute branch address (indirection, but no indexing!) ...
  //--
  if (ISSET(wEA, 0x8000))
    return FormatString("*0x%04X", MASK15(wEA));
  else
    return FormatString("0x%04X", MASK15(wEA));
}

PRIVATE string ShowAbsoluteAddress (address_t nStart, address_t wAddress, uint2_t bReg)
{
  //++
  // Disassemble an absolute address, complete with indexing and everything!
  //--
  address_t wEA = (nStart & 0x6000) | (wAddress & 0x1FFF);
  string s = ISSET(wAddress, 0x8000) ? "*" : "";
  s += FormatString("0x%04X", wEA);
  if ((wAddress & 0x6000) != 0) {
    s += FormatString(",R%d", bReg);
    if ((wAddress & 0x6000) == 0x2000) s += ",+";
    if ((wAddress & 0x6000) == 0x4000) s += ",-";
  }
  return s;
}

PUBLIC uint16_t Disassemble (const CMemory *pMemory, address_t nStart, string &sCode)
{
  //++
  //   Disassemble one instruction and return a string containg the result.
  // Since instructions are variable length, this can potentially require 1,
  // 2 or 3 bytes of data.  The memory address of the first byte should be
  // passed to the nStart parameter, and the return value is the number of
  // bytes actually used by the instruction.
  //--
  uint8_t bOpcode = pMemory->CPUread(nStart);
  uint8_t bData1=0, bData2=0;
  uint16_t cbOpcode = 1;
  OP_ARG_TYPE nType;
  uint2_t bReg = bOpcode & 3;

  // Search the opcode table for a match ...
  const OP_CODE *pOpcode = NULL;
  for (size_t i = 0;  i < OP_COUNT;  ++i) {
    if ((bOpcode & g_aOpcodes[i].bMask) == g_aOpcodes[i].bOpcode) {
      pOpcode = &g_aOpcodes[i];  break;
    }
  }

  // If there's no match then it's not a valid opcode ...
  if (pOpcode == NULL) {
    sCode = FormatString("invalid opcode");  return 1;
  }

  // Fetch any operands ...
  nType = pOpcode->nType;
  if ((nType!=OP_ARG_1Z) && (nType!=OP_ARG_1E) && (nType!=OP_ARG_1ZCC)) {
    bData1 = pMemory->CPUread(C2650::ADD13(nStart, 1));  cbOpcode = 2;
    if ((nType==OP_ARG_3A) || (nType==OP_ARG_3B) || (nType==OP_ARG_3EB) || (nType==OP_ARG_3BCC)) {
      bData2 = pMemory->CPUread(C2650::ADD13(nStart, 2));  cbOpcode = 3;
    }
  }

  // Format the opcode neatly ...
  sCode = pOpcode->pszName;
  switch (nType) {
    case OP_ARG_1E:   break;
    case OP_ARG_2EI:   sCode += FormatString("\t0x%02X", bData1);                                                                   break;
    case OP_ARG_2ER:   sCode += FormatString("\t%s", ShowRelativeAddress(nStart, bData1).c_str());                                  break;
    case OP_ARG_3EB:   sCode += FormatString("\t%s", ShowBranchAddress(MKWORD(bData1, bData2)).c_str());                            break;
    case OP_ARG_1Z:    sCode += FormatString("\tR%d", bReg);                                                                         break;
    case OP_ARG_1ZCC:  sCode += FormatString(",%s", g_apszConditions[bReg]);                                                        break;
    case OP_ARG_2R:    sCode += FormatString(",R%d\t%s", bReg, ShowRelativeAddress(nStart, bData1).c_str());                        break;
    case OP_ARG_2RCC:  sCode += FormatString(",%s\t%s", g_apszConditions[bReg], ShowRelativeAddress(nStart, bData1).c_str());       break;
    case OP_ARG_2I:    sCode += FormatString(",R%d\t0x%02X", bReg, bData1);                                                         break;
    case OP_ARG_3A:    if ((bData1 & 0x60) != 0)
                         sCode += FormatString(",R0\t%s", ShowAbsoluteAddress(nStart, MKWORD(bData1, bData2), bReg).c_str());
                       else
                         sCode += FormatString(",R%d\t%s", bReg, ShowAbsoluteAddress(nStart, MKWORD(bData1, bData2), 0).c_str());
                       break;
    case OP_ARG_3B:    sCode += FormatString(",R%d\t%s", bReg, ShowBranchAddress(MKWORD(bData1, bData2)).c_str());                  break;
    case OP_ARG_3BCC:  sCode += FormatString(",%s\t%s", g_apszConditions[bReg], ShowBranchAddress(MKWORD(bData1, bData2)).c_str()); break;
    default: assert(false);   // what did I forget??
  }

  return cbOpcode;
}

size_t Assemble (CMemory *pMemory, const string &sCode, size_t nStart)
{
  //++
  //--
  return 0;
}
