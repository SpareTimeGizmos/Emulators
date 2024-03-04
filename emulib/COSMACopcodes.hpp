//++
//Opcodes.hpp -> COSMAC opcodes, assembler and disassembler
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
//    This file contains COSMAC opcodes, mnemonics, and the function prototypes
// for the one line assembler and disassembler routines ...
//
// REVISION HISTORY:
// 14-Jan-20  RLA   New file.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ std::string class, et al ...
#include "MemoryTypes.h"        // address_t and word_t data types
using std::string;              // ...
class CMemory;                  // ...

// COSMAC opcode mnemonics ...
#define OP_IDL	  0x00		// WAIT FOR DMA OR INTERRUPT
#define OP_LDN	  0x00		// LOAD VIA N
#define OP_INC	  0x10		// INCREMENT REG N
#define OP_DEC	  0x20		// DECREMENT REG N
#define OP_BR	  0x30		// SHORT BRANCH
#define OP_BQ	  0x31		// SHORT BRANCH IF Q = 1
#define OP_BZ	  0x32		// SHORT BRANCH IF D = 0
#define OP_BDF	  0x33		// SHORT BRANCH IF DF = 1
#define OP_B1	  0x34		// SHORT BRANCH IF EF1 = 1
#define OP_B2	  0x35		// SHORT BRANCH IF EF2 = 1
#define OP_B3	  0x36		// SHORT BRANCH IF EF3 = 1
#define OP_B4	  0x37		// SHORT BRANCH IF EF4 = 1
#define OP_SKP	  0x38		// NO SHORT BRANCH
#define OP_BNQ	  0x39		// SHORT BRANCH IF Q = 0
#define OP_BNZ	  0x3A		// SHORT BRANCH IF D NOT 0
#define OP_BNF	  0x3B		// SHORT BRANCH IF DF = 0
#define OP_BN1	  0x3C		// SHORT BRANCH IF EF1 = 0
#define OP_BN2	  0x3D		// SHORT BRANCH IF EF2 = 0
#define OP_BN3	  0x3E		// SHORT BRANCH IF EF3 = 0
#define OP_BN4	  0x3F		// SHORT BRANCH IF EF4 = 0
#define OP_LDA	  0x40		// LOAD ADVANCE
#define OP_STR	  0x50		// STORE VIA N
#define OP_IRX	  0x60		// INCREMENT REG X
#define OP_OUT	  0x60		// OUTPUT
// $68 is the prefix for 1804/5/6 extended opcodes
#define OP_INP    0x68  	// INPUT 
#define OP_RET	  0x70		// RETURN
#define OP_DIS	  0x71		// DISABLE
#define OP_LDXA	  0x72		// LOAD VIA X AND ADVANCE
#define OP_STXD	  0x73		// STORE VIA X AND DECREMENT
#define OP_ADC	  0x74		// ADD WITH CARRY
#define OP_SDB	  0x75		// SUBTRACT D WITH BORROW
#define OP_SHRC	  0x76		// SHIFT RIGHT WITH CARRY
#define OP_SMB	  0x77		// SUBTRACT MEMORY WITH BORROW
#define OP_SAV	  0x78		// SAVE
#define OP_MARK	  0x79		// PUSH X, P TO STACK
#define OP_REQ	  0x7A		// RESET Q
#define OP_SEQ	  0x7B		// SET Q
#define OP_ADCI	  0x7C		// ADD WITH CARRY, IMMEDIATE
#define OP_SDBI	  0x7D		// SUBTRACT D WITH BORROW, IMMEDIATE
#define OP_SHLC	  0x7E		// SHIFT LEFT WITH CARRY
#define OP_SMBI	  0x7F		// SUBTRACT MEMORY WITH BORROW, IMMEDIATE
#define OP_GLO	  0x80		// GET LOW REG N
#define OP_GHI	  0x90		// GET HIGH REG N
#define OP_PLO	  0xA0		// PUT LOW REG N
#define OP_PHI	  0xB0		// PUT HIGH REG N
#define OP_LBR	  0xC0		// LONG BRANCH
#define OP_LBQ	  0xC1		// LONG BRANCH IF Q = 1
#define OP_LBZ	  0xC2		// LONG BRANCH IF D = 0
#define OP_LBDF	  0xC3		// LONG BRANCH IF DF = 1
#define OP_NOP	  0xC4		// NO OPERATION
#define OP_LSNQ	  0xC5		// LONG SKIP IF Q = 0
#define OP_LSNZ	  0xC6		// LONG SKIP IF D NOT 0
#define OP_LSNF	  0xC7		// LONG SKIP IF DF = 0
#define OP_LSKP	  0xC8		// NO LONG BRANCH
#define OP_LBNQ	  0xC9		// LONG BRANCH lF Q = 0
#define OP_LBNZ	  0xCA		// LONG BRANCH IF D NOT 0
#define OP_LBNF	  0xCB		// LONG BRANCH IF DF = 0
#define OP_LSIE	  0xCC		// LONG SKIP IF lE = 1
#define OP_LSQ	  0xCD		// LONG SKIP lF Q = 1
#define OP_LSZ	  0xCE		// LONG SKIP IF D = 0
#define OP_LSDF	  0xCF		// LONG SKIP IF DF = 1
#define OP_SEP	  0xD0		// SET P
#define OP_SEX	  0xE0		// SET X
#define OP_LDX	  0xF0		// LOAD VIA X
#define OP_OR	  0xF1		// OR
#define OP_AND	  0xF2		// AND
#define OP_XOR	  0xF3		// EXCLUSIVE OR
#define OP_ADD	  0xF4		// ADD
#define OP_SD	  0xF5		// SUBTRACT D
#define OP_SHR	  0xF6		// SHIFT RIGHT
#define OP_SM	  0xF7		// SUBTRACT MEMORY
#define OP_LDI	  0xF8		// LOAD IMMEDIATE
#define OP_ORI	  0xF9		// OR IMMEDIATE
#define OP_XRI	  0xFB		// EXCLUSIVE OR IMMEDIATE
#define OP_ANI	  0xFA		// AND IMMEDIATE
#define OP_ADI	  0xFC		// ADD IMMEDIATE
#define OP_SDI	  0xFD		// SUBTRACT D IMMEDIATE
#define OP_SHL	  0xFE		// SHIFT LEFT
#define OP_SMI	  0xFF		// SUBTRACT MEMORY IMMEDIATE

// CDP1804/5/6 Extended opcodes.
//   All these must be preceeded by a 0x68 byte!
#define OP_STPC   0x00          // STOP COUNTER
#define OP_DTC    0x01          // DECREMENT TIMER/COUNTER
#define OP_SPM2   0x02          // SET PULSE WIDTH MODE 2 AND START
#define OP_SCM2   0x03          // SET COUNTER MODE 2 AND START
#define OP_SPM1   0x04          // SET PULSE WIDTH MODE 1 AND START
#define OP_SCM1   0x05          // SET COUNTER MODE 1 AND START
#define OP_LDC    0x06          // LOAD COUNTER
#define OP_STM    0x07          // SET TIMER MODE AND START
#define OP_GEC    0x08          // GET COUNTER
#define OP_ETQ    0x09          // ENABLE TOGGLE Q
#define OP_XIE    0x0A          // EXTERNAL INTERRUPT ENABLE
#define OP_XID    0x0B          // EXTERNAL INTERRUPT DISABLE
#define OP_CIE    0x0C          // COUNTER INTERRUPT ENABLE
#define OP_CID    0x0D          // COUNTER INTERRUPT DISABLE
#define OP_DBNZ   0x20          // DECREMENT REG N AND LONG BRANCH IF NOT EQUAL ZERO
#define OP_BCI    0x3E          // SHORT BRANCH ON COUNTER INTERRUPT
#define OP_BXI    0x3F          // SHORT BRANCH ON EXTERNAL INTERRUPT
#define OP_RLXA   0x60          // REGISTER LOAD VIA X AND ADVANCE
#define OP_DADC   0x74          // DECIMAL ADD WITH CARRY
#define OP_DSAV   0x76          // SAVE T, D, DF
#define OP_DSMB   0x77          // DECIMAL SUBTRACT MEMORY WITH BORROW
#define OP_DACI   0x7C          // DECIMAL ADD WITH CARRY, IMMEDIATE
#define OP_DSBI   0x7F          // DECIMAL SUBTRACT MEMORY WITH BORROW, IMMEDIATE
#define OP_SCAL   0x80          // STANDARD CALL
#define OP_SRET   0x90          // STANDARD RETURN
#define OP_RSXD   0xA0          // REGISTER STORE VIA X AND DECREMENT
#define OP_RNX    0xB0          // REGISTER N TO REGISTER X COPY
#define OP_RLDI   0xC0          // REGISTER LOAD IMMEDIATE
#define OP_DADD   0xF4          // DECIMAL ADD
#define OP_DSM    0xF7          // DECIMAL SUBTRACT MEMORY
#define OP_DADI   0xFC          // DECIMAL ADD IMMEDIATE
#define OP_DSMI   0xFF          // DECIMAL SUBTRACT MEMORY, IMMEDIATE

// Opcode argument types ...
enum _OP_ARG_TYPES {
  OP_ARG_NONE     = 0,          // no argument at all
  OP_ARG_REG      = 1,          // 4 bit register number (part of the opcode)
  OP_ARG_1BYTE    = 2,          // one byte (e.g. branch, immediate, etc)
  OP_ARG_2BYTES   = 3,          // two bytes (long branch, etc)
  OP_ARG_IO       = 4,          // 3 bit I/O device address
  OP_ARG_EXTENDED = 5,          // extended (two byte) 1804/5/6 opcode
  OP_ARG_R2BYTES  = 6,          // register and 2 bytes (DBNZ, RLDI, etc)
};
typedef enum _OP_ARG_TYPES OP_ARG_TYPE;

// Opcode definitions for the assember and disassembler ...
struct _OPCODE {
  const char   *pszName;        // the mnemonic for the opcode
  uint8_t       bOpcode;        // the actual opcode
  uint8_t       bMask;          // mask of significant bits
  OP_ARG_TYPE   nType;          // argument/operand for this opcode
};
typedef struct _OPCODE OPCODE;


// Assemble or disassemble COSMAC instructions ...
extern size_t Disassemble (const CMemory *pMemory, address_t nStart, string &sCode);
extern size_t Assemble (CMemory *pMemory, const string &sCode, address_t nStart);
