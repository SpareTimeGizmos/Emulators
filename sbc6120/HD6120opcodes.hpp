//++
// HD6120opcodes.hpp -> 6120/6100/PDP8 opcodes, assembler and disassembler
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
//    This file contains 6120, 6100 and PDP-8 opcodes, mnemonics, and the
// function prototypes for the one line assembler and disassembler routines ...
//
// REVISION HISTORY:
// 30-JUL-22  RLA   New file.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ std::string class, et al ...
#include "MemoryTypes.h"        // address_t and word_t data types
using std::string;              // ...
class CMemory;                  // ...


// Memory reference instructions ...
#define OP_AND     00000        // AC <= AC AND MEM(EA)
#define OP_TAD     01000        // AC <= AC + MEM(EA)
#define OP_ISZ     02000        // MEM(EA) <= MEM(EA) + 1, SKIP IF ZERO
#define OP_DCA     03000        // MEM(EA) <= AC
#define OP_JMS     04000        // MEM(EA) <= PC, PC <= EA+1
#define OP_JMP     05000        // PC <= EA
// Group 1 microinstructions ...
#define OP_NOP1    07000        // no operation
#define OP_CLA1    07200        // AC <= 0
#define OP_CLL     07100        // LINK <= 0
#define OP_CMA     07040        // AC <= ~AC
#define OP_CML     07020        // LINK <= ~LINK
#define OP_RAR     07010        // rotate AC,L right one bit
#define OP_RAL     07004        //    "    " " left   "   "
#define OP_BSW     07002        // swap AC bytes (six bits)
#define OP_IAC     07001        // AC <= AC + 1
// Group 2 microinstructions ...
#define OP_NOP2    07400        // nop operation
#define OP_CLA2    07600        // AC <= 0
#define OP_SMA     07500        // skip if AC .LT. 0
#define OP_SPA     07510        // skip if AC .GE. 0
#define OP_SZA     07440        // skip if AC .EQ. 0
#define OP_SNA     07450        // skip if AC .NE. 0
#define OP_SNL     07420        // skip if link is zero
#define OP_SZL     07430        // skip if link is one
#define OP_SKP     07410        // unconditional skip
#define OP_OSR     07404        // AC <= AC | SR
#define OP_HLT     07402        // halt
// Group 3 microinstructions ...
#define OP_NOP3    07401        // no operation
#define OP_CLA3    07601        // AC <= 0
#define OP_MQA     07501        // AC <= AC | MQ
#define OP_MQL     07421        // MQ <= AC
// Interrupt control IOTs...
#define OP_SKON    06000        // skip if interrupts are enabled
#define OP_ION     06001        // enable interrupts
#define OP_IOF     06002        // disable interrupts
#define OP_SRQ     06003        // skip if an interrupt is pending
#define OP_GTF     06004        // get the current flags
#define OP_RTF     06005        // restore flags - LINK, DF, and IF (IB)
#define OP_SGT     06006        // unimplemented EAE instruction
#define OP_CAF     06007        // clear all flags
// Extended memory instructions ...
#define OP_CDF     06201        // change data field
#define OP_CIF     06202        // change instruction field
#define OP_CXF     06203        // change both fields
#define OP_RDF     06214        // read current data field
#define OP_RIF     06224        // read current instruction field
#define OP_RIB     06234        // read instruction field buffer
#define OP_RMF     06244        // restore memory field
//#define OP_LIF     06254        // load instruction field
// HD6120 stack instructions ...
#define OP_PPC1    06205        // push PC on stack #1
#define OP_PPC2    06245        //   "   "  "    "  #2
#define OP_PAC1    06215        // push AC on stack #1
#define OP_PAC2    06255        //   "   "  "   "   #2
#define OP_RTN1    06225        // return using stack #1
#define OP_RTN2    06265        //    "     "     "   #2
#define OP_POP1    06235        // pop AC from stack #1
#define OP_POP2    06275        //  "   "   "    "   #2
#define OP_RSP1    06207        // read stack pointer #1
#define OP_RSP2    06227        //   "    "      "    #2
#define OP_LSP1    06217        // load stack pointer #1
#define OP_LSP2    06237        //   "    "      "    #2
// Other HD6120 instructions ...
#define OP_PR0     06206        // panel request #0
#define OP_PR1     06216        //   "      "    #1
#define OP_PR2     06226        //   "      "    #2
#define OP_PR3     06236        //   "      "    #3
#define OP_WSR     06246        // write switch register
#define OP_GCF     06256        // get current fields
#define OP_CPD     06266        // clear panel data flag
#define OP_SPD     06276        // set     "     "    "
// KL8/E console terminal IOTs ...
#define OP_KCF     06030        // clear keyboard flag, do not set reader run
#define OP_KSF     06031        // skip if keyboard flag is set
#define OP_KCC     06032        // clear keyboard flag and AC, set reader run
#define OP_KRS     06034        // read keyboard buffer
#define OP_KIE     06035        // load interrupt enable from AC bit 11
#define OP_KRB     06036        // combination of KRS and KCC
#define OP_TFL     06040        // set printer flag
#define OP_TSF     06041        // skip if printer flag is set
#define OP_TCF     06042        // clear printer flag
#define OP_TPC     06044        // load printer buffer
#define OP_TSK     06045        // skip if interrupt request
#define OP_TLS     06046        // combination of TPC and TCF
// KM8/E Timeshare option IOTs ...
#define OP_CINT    06204        // clear user interrupt flag
#define OP_SINT    06254        // skip on user interrupt flag
#define OP_CUF     06264        // clear user flag
#define OP_SUF     06274        // set user flag
// SBC6120 unique IOTs ...
#define OP_MMAP    06400        // select memory map
#define OP_LDAR    06410        // load disk address register
#define OP_SDASP   06411        // skip on disk activity (DASP)
#define OP_PRISLU  06412        // select SLU primary IOT codes
#define OP_SECSLU  06413        // select SLU secondary IOT codes
#define OP_SBBLO   06415        // skip on backup battery low
#define OP_CCPR    06430        // clear front panel request flags
#define OP_SHSW    06431        // skip on HALT switch
#define OP_SPLK    06432        // skip on panel lock
#define OP_SCPT    06433        // skip on panel timer flag
#define OP_RFNS    06434        // read function switches
#define OP_RLOF    06435        // RUN LED off
#define OP_RLON    06437        // RUN LED on
#define OP_POST    06440        // display POST code n
#define OP_PRPA    06470        // read PPI port A
#define OP_PRPB    06471        // read PPI port B
#define OP_PRPC    06472        // read PPI port C
#define OP_PWPA    06474        // write PPI port A
#define OP_PWPB    06475        // write PPI port B
#define OP_PWPC    06476        // write PPI port C
#define OP_PWCR    06477        // write PPI control register


// Opcode argument types ...
enum _OP_ARG_TYPES {
  OP_NONE,
  OP_MRI,
  OP_OPR1,
  OP_OPR2,
  OP_OPR3,
  OP_EMA,
  OP_PLUS,          // IOT+n, where n is the last octal digit
};
typedef enum _OP_ARG_TYPES OP_ARG_TYPE;

// Opcode definitions for the assember and disassembler ...
struct _OPCODE {
  const char   *pszName;        // the mnemonic for the opcode
  word_t        wOpcode;        // the actual opcode
  word_t        wMask;          // mask of significant bits
  OP_ARG_TYPE   nType;          // argument/operand for this opcode
};
typedef struct _OPCODE OPCODE;


// Assemble or disassemble COSMAC instructions ...
extern string Disassemble (address_t wAddress, word_t wOpcode);
extern size_t Assemble (CMemory *pMemory, const string &sCode, address_t nStart);
