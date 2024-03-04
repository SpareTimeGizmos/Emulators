//++
// HD6120opcodes.hpp -> 6120/6100/PDP8 assembler and disassembler
//
//   Copyright (C) 1999-2022 by Spare Time Gizmos.  All rights reserved.
//
// LICENSE:
//    This file is part of the SBC6120 emulator project. SBC6120 is free
// software; you may redistribute it and/or modify it under the terms of
// the GNU Affero General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any
// later version.
//
//    SBC6120 is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License
// for more details.  You should have received a copy of the GNU Affero General
// Public License along with SBC6120.  If not, see http://www.gnu.org/licenses/.
//
// DESCRIPTION:
//   This file contains a table of ASCII mnemonics for HD6120, IM6100 and the
// traditional PDP-8 opcodes, and one line assembler and disassembler methods ...
//
// REVISION HISTORY:
// 30-JUL-22  RLA  New file.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include "EMULIB.hpp"           // emulator library definitions
#include "MemoryTypes.h"        // address_t and word_t data types
#include "Memory.hpp"           // main memory functions
#include "HD6120opcodes.hpp"    // declarations for this module

// PDP-8 opcode definitions ...
PRIVATE const OPCODE g_aOpcodes[] = {
  // Memory reference instructions ...
  {"AND",    OP_AND,  07000, OP_MRI },
  {"TAD",    OP_TAD,  07000, OP_MRI },
  {"ISZ",    OP_ISZ,  07000, OP_MRI },
  {"DCA",    OP_DCA,  07000, OP_MRI },
  {"JMS",    OP_JMS,  07000, OP_MRI },
  {"JMP",    OP_JMP,  07000, OP_MRI },
  // Group 1 microinstructions ...
  {"NOP",    OP_NOP1, 07777, OP_NONE},
  {"CLA",    OP_CLA1, 07777, OP_NONE},
  {"STL",    07120,   07777, OP_NONE},
  {"STA",    07240,   07777, OP_NONE},
  {"NL0000", 07300,   07777, OP_NONE},
  {"NL0001", 07301,   07777, OP_NONE},
  {"NL0002", 07305,   07777, OP_NONE},
  {"NL0003", 07325,   07777, OP_NONE},
  {"NL0004", 07307,   07777, OP_NONE},
  {"NL0006", 07327,   07777, OP_NONE},
  {"NL0100", 07303,   07777, OP_NONE},
  {"NL2000", 07332,   07777, OP_NONE},
  {"NL3777", 07350,   07777, OP_NONE},
  {"NL4000", 07330,   07777, OP_NONE},
  {"NL5777", 07352,   07777, OP_NONE},
  {"NL6000", 07333,   07777, OP_NONE},
  {"NL7775", 07346,   07777, OP_NONE},
  {"NL7776", 07344,   07777, OP_NONE},
  {"NL7777", 07340,   07777, OP_NONE},
  {NULL,     07000,   07400, OP_OPR1},
  // Group 2 microinstructions ...
  {"NOP",    OP_NOP2, 07777, OP_NONE},
  {"SKP",    OP_SKP,  07777, OP_NONE},
  {"LAS",    07604,   07777, OP_NONE},
  {NULL,     07400,   07401, OP_OPR2},
  // Group 3 microinstructions ...
  {"NOP",    OP_NOP3, 07777, OP_NONE},
  {"ACL",    07701,   07777, OP_NONE},
  {"CAM",    07621,   07777, OP_NONE},
  {"SWP",    07521,   07777, OP_NONE},
  {NULL,     07401,   07401, OP_OPR3},
  // CPU internal IOTs ...
  {"SKON",   OP_SKON, 07777, OP_NONE},
  {"ION",    OP_ION,  07777, OP_NONE},
  {"IOF",    OP_IOF,  07777, OP_NONE},
  {"SRQ",    OP_SRQ,  07777, OP_NONE},
  {"GTF",    OP_GTF,  07777, OP_NONE},
  {"RTF",    OP_RTF,  07777, OP_NONE},
  {"SGT",    OP_SGT,  07777, OP_NONE},
  {"CAF",    OP_CAF,  07777, OP_NONE},
  // Extended memory instructions ...
  {"CDF",    OP_CDF,  07707, OP_EMA},
  {"CIF",    OP_CIF,  07707, OP_EMA},
  {"CXF",    OP_CXF,  07707, OP_EMA},
  {"RDF",    OP_RDF,  07777, OP_NONE},
  {"RIF",    OP_RIF,  07777, OP_NONE},
  {"RIB",    OP_RIB,  07777, OP_NONE},
  {"RMF",    OP_RMF,  07777, OP_NONE},
//{"LIF",    OP_LIF,  07777, OP_NONE},
  // HD6120 stack instructions ...
  {"PPC1",   OP_PPC1, 07777, OP_NONE},
  {"PPC2",   OP_PPC2, 07777, OP_NONE},
  {"PAC1",   OP_PAC1, 07777, OP_NONE},
  {"PAC2",   OP_PAC2, 07777, OP_NONE},
  {"RTN1",   OP_RTN1, 07777, OP_NONE},
  {"RTN2",   OP_RTN2, 07777, OP_NONE},
  {"POP1",   OP_POP1, 07777, OP_NONE},
  {"POP2",   OP_POP2, 07777, OP_NONE},
  {"RSP1",   OP_RSP1, 07777, OP_NONE},
  {"RSP2",   OP_RSP2, 07777, OP_NONE},
  {"LSP1",   OP_LSP1, 07777, OP_NONE},
  {"LSP2",   OP_LSP2, 07777, OP_NONE},
  // Other HD6120 instructions ...
  {"PR0",    OP_PR0,  07777, OP_NONE},
  {"PR1",    OP_PR1,  07777, OP_NONE},
  {"PR2",    OP_PR2,  07777, OP_NONE},
  {"PR3",    OP_PR3,  07777, OP_NONE},
  {"WSR",    OP_WSR,  07777, OP_NONE},
  {"GCF",    OP_GCF,  07777, OP_NONE},
  {"CPD",    OP_CPD,  07777, OP_NONE},
  {"SPD",    OP_SPD,  07777, OP_NONE},
  // KL8/E console terminal IOTs ...
  {"KCF",    OP_KCF,  07777, OP_NONE},
  {"KSF",    OP_KSF,  07777, OP_NONE},
  {"KCC",    OP_KCC,  07777, OP_NONE},
  {"KRS",    OP_KRS,  07777, OP_NONE},
  {"KIE",    OP_KIE,  07777, OP_NONE},
  {"KRB",    OP_KRB,  07777, OP_NONE},
  {"TFL",    OP_TFL,  07777, OP_NONE},
  {"TSF",    OP_TSF,  07777, OP_NONE},
  {"TCF",    OP_TCF,  07777, OP_NONE},
  {"TPC",    OP_TPC,  07777, OP_NONE},
  {"TSK",    OP_TSK,  07777, OP_NONE},
  {"TLS",    OP_TLS,  07777, OP_NONE},
  // KM8/E Timeshare option IOTs ...
  {"CINT",   OP_CINT, 07777, OP_NONE},
  {"SINT",   OP_SINT, 07777, OP_NONE},
  {"CUF",    OP_CUF,  07777, OP_NONE},
  {"SUF",    OP_SUF,  07777, OP_NONE},
  // SBC6120 unique IOTs ...
  {"MMAP",   OP_MMAP, 07774, OP_PLUS},
  {"LDAR",   OP_LDAR, 07777, OP_NONE},
  {"SDASP",  OP_SDASP,07777, OP_NONE},
  {"PRISLU",OP_PRISLU,07777, OP_NONE},
  {"SECSLU",OP_SECSLU,07777, OP_NONE},
  {"SBBLO",  OP_SBBLO,07777, OP_NONE},
  {"CCPR",   OP_CCPR, 07777, OP_NONE},
  {"SHSW",   OP_SHSW, 07777, OP_NONE},
  {"SPLK",   OP_SPLK, 07777, OP_NONE},
  {"SCPT",   OP_SCPT, 07777, OP_NONE},
  {"RFNS",   OP_RFNS, 07777, OP_NONE},
  {"RLOF",   OP_RLOF, 07777, OP_NONE},
  {"RLON",   OP_RLON, 07777, OP_NONE},
  {"POST",   OP_POST, 07770, OP_PLUS},
  {"PRPA",   OP_PRPA, 07777, OP_NONE},
  {"PRPB",   OP_PRPB, 07777, OP_NONE},
  {"PRPC",   OP_PRPC, 07777, OP_NONE},
  {"PWPA",   OP_PWPA, 07777, OP_NONE},
  {"PWPB",   OP_PWPB, 07777, OP_NONE},
  {"PWPC",   OP_PWPC, 07777, OP_NONE},
  {"PWCR",   OP_PWCR, 07777, OP_NONE},
};
#define OPCOUNT (sizeof(g_aOpcodes)/sizeof(OPCODE))


PRIVATE string DecodeMRI (address_t wAddress, word_t wOpcode, const char *pszMnemonic)
{
  //++
  //   Decode a PDP-8 memory reference instruction.  The effective address is
  // all we have to figure out - the mnemonic has already been found...
  //--
  word_t wEA = wOpcode & 0177;
  if (ISSET(wOpcode, 00200)) wEA |= (wAddress & 07600);
  if (ISSET(wOpcode, 00400))
    return FormatString("%-3s\t@%04o", pszMnemonic, wEA);
  else
    return FormatString("%-3s\t%04o", pszMnemonic, wEA);
}

PRIVATE string DecodeOPR1 (word_t wOpcode)
{
  //++
  //   Decode a PDP-8 group 1 microinstruction.  This opcode has several
  // independent functions selected by individual bits in the opcode.  The
  // only exception is the 3 bit rotate field, which all work as a group.
  //--
  string sCode;
  if (ISSET(wOpcode, 00200)) sCode += "CLA ";
  if (ISSET(wOpcode, 00100)) sCode += "CLL ";
  if (ISSET(wOpcode, 00040)) sCode += "CMA ";
  if (ISSET(wOpcode, 00020)) sCode += "CML ";
  if (ISSET(wOpcode, 00001)) sCode += "IAC ";
  switch (wOpcode & 00016) {
    case 00000:                   break;
    case 00002:  sCode += "BSW "; break;
    case 00004:  sCode += "RAL "; break;
    case 00006:  sCode += "RTL "; break;
    case 00010:  sCode += "RAR "; break;
    case 00012:  sCode += "RTR "; break;
    case 00014:  sCode += "R3L "; break;
    case 00016:                   break;
  }
  return sCode;
}

PRIVATE string DecodeOPR2 (word_t wOpcode)
{
  //++
  //   Decode PDP-8 group 2 microinstructions.  These are mostly skip on
  // condition tests, and bit 8 flips the sense of the tests.  The PDP-8
  // designers also threw HLT and OSR in here too, for lack of a better
  // place!
  //--
  string sCode;
  if (ISSET(wOpcode, 00010)) {
    if (ISSET(wOpcode, 00100)) sCode += "SPA ";
    if (ISSET(wOpcode, 00040)) sCode += "SNA ";
    if (ISSET(wOpcode, 00020)) sCode += "SZL ";
  } else {
    if (ISSET(wOpcode, 00100)) sCode += "SMA ";
    if (ISSET(wOpcode, 00040)) sCode += "SZA ";
    if (ISSET(wOpcode, 00020)) sCode += "SNL ";
  }
  if (ISSET(wOpcode, 00200)) sCode += "CLA ";
  if (ISSET(wOpcode, 00004)) sCode += "OSR ";
  if (ISSET(wOpcode, 00002)) sCode += "HLT ";
  return sCode;
}

PRIVATE string DecodeOPR3(word_t wOpcode)
{
  //++
  //   Decode PDP-8 group 3 microinstructions.  These are all EAE (extended
  // arithmetic element) operations, which aren't implemented here.  The only
  // ones we recognize are those that load or read the MQ register.
  //--
  string sCode;
  if (ISSET(wOpcode, 00200)) sCode += "CLA ";
  if (ISSET(wOpcode, 00100)) sCode += "MQA ";
  if (ISSET(wOpcode, 00020)) sCode += "MQL ";
  return sCode;
}


PUBLIC string Disassemble (address_t wAddress, word_t wOpcode)
{
  //++
  //   Decode a single PDP-8 instruction and return a string for the result.
  // We need to know the address of the opcode for MRIs, so we can display
  // the effective address for current page addressing.
  //--

  // Search the opcode table for a match ...
  const OPCODE *pOpcode = NULL;
  for (size_t i = 0;  i < OPCOUNT;  ++i) {
    if ((wOpcode & g_aOpcodes[i].wMask) == g_aOpcodes[i].wOpcode) {
      pOpcode = &g_aOpcodes[i];  break;
    }
  }

  // If we didn't find a match, then return a null string ...
  if (pOpcode == NULL) return "";

  // Otherwise decode the opcode ...
  switch (pOpcode->nType) {
    case OP_NONE: return string(pOpcode->pszName);
    case OP_MRI:  return DecodeMRI(wAddress, wOpcode, pOpcode->pszName);
    case OP_OPR1: return DecodeOPR1(wOpcode);
    case OP_OPR2: return DecodeOPR2(wOpcode);
    case OP_OPR3: return DecodeOPR3(wOpcode);
    case OP_EMA:  return FormatString("%s\t%1o", pOpcode->pszName, (wOpcode>>3) & 7);
    case OP_PLUS: return FormatString("%s+%1o", pOpcode->pszName, wOpcode & 7);
  }

  // We should never get here, but ...
  return  FormatString("%04o", wOpcode);
}

size_t Assemble (CMemory *pMemory, const string &sCode, address_t nStart)
{
  //++
  //--
  return 0;
}
