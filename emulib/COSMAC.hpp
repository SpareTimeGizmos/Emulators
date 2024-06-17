//++
// COSMAC.hpp -> definitions CCOSMAC microprocessor emulation class
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
//   The CCOSMAC class is derived from the CCPU class and implements the COSMAC
// CDP1802 specific microprocessor.
//
// REVISION HISTORY:
// 14-AUG-19  RLA   New file.
// 21-JUN-22  RLA   Add the EFx and Q constants
// 27-JUL-22  RLA   Add 1804/5/6 registers and Extended mode option
// 17-JUN-24  RLA   Add "override" to GetSenseName and GetFlagName
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ std::string class, et al ...
#include "CommandParser.hpp"    // needed for type KEYWORD
#include "MemoryTypes.h"        // address_t and word_t data types
#include "Memory.hpp"           // generic memory class
#include "Interrupt.hpp"        // interrupt simulation logic
#include "EventQueue.hpp"       // I/O device event queue simulation
#include "CPU.hpp"              // CPU definitions
using std::string;              // ...


class CCOSMAC : public CCPU {
  //++
  // RCA COSMAC CDP1802 CPU emulation...
  //--
 
  // CPU characteristics...
public:
  enum {
    CLOCK_FREQUENCY   = 2500000UL,// standard SBC1802 crystal is 2.5MHz
    CLOCKS_PER_CYCLE  = 8,        // COSMAC 8 clocks per major cycle
    MAXREGISTER       = 16,       // number of register file registers
    MAXDEVICE         = 7,        // number of I/O device addresses
    MAXSENSE          = 4,        // number of sense (EFx) inputs
    MAXFLAG           = 1,        // number of flag (i.e. Q) outputs
    TPAPRESCALE       = 32,       // timer decrements every 32 machine cycles
    //   Define mnemonics for the EFx and Q sense inputs and flag outputs here, for
    // convenience.  Note that GetSense() and SetFlag() both use zero based indices,
    // so EF1..4 are defined as 0..3!
    EF1         = 0,              // GetSense() input for B1/BN1 instructions
    EF2         = 1,              //  "   "       "    "  B2/BN2   "    "
    EF3         = 2,              //  "   "       "    "  B3/BN3   "    "
    EF4         = 3,              //  "   "       "    "  B4/BN4   "    "
    Q           = 0,              // SetFlag() output for SEQ/REQ instructions
  };
  enum _CTMODE {
    // Counter/timer modes ...
    CT_STOPPED,                   // counter/timer stopped
    CT_TIMER,                     // decrement once every 32 machine cycles
    CT_EVENT1,                    //   "   "   every falling EF1 edge
    CT_EVENT2,                    //   "   "     "    "   "  EF2   "
    CT_PULSE1,                    //   "   "   every machine cycle while EF1 iw true
    CT_PULSE2,                    //   "   "     "    "   "    "     "   EF2  "   "
  };
  typedef enum _CTMODE CTMODE;

  // Internal CPU registers ...
  //   These codes are passed to the GetRegister() and SetRegister() methods to
  // access internal CPU registers and state.  The 16 COSMAC address registers
  // must always be 0..15, but after that the order is arbitrary ...
public:
  enum _REGISTERS {
    REG_R0=0, REG_R1=1, REG_R2=2,  REG_R3=3,  REG_R4=4,  REG_R5=5,  REG_R6=6,  REG_R7=7,
    REG_R8=8, REG_R9=9, REG_RA=10, REG_RB=11, REG_RC=12, REG_RD=13, REG_RE=14, REG_RF=15,
    REG_D, REG_DF, REG_P, REG_X, REG_I, REG_N, REG_T, /*REG_B,*/ REG_IE,
    REG_Q, REG_EF1, REG_EF2, REG_EF3, REG_EF4, 
    // 1804/5/6 extended registers ...
    REG_XIE, REG_CIE, REG_CIL, REG_CNTR, REG_CH, REG_ETQ,
  };
  // This table is used to translate a name to a REGISTER ...
  static const CCmdArgKeyword::KEYWORD g_keysRegisters[];

  // Constructors and destructors...
public:
  CCOSMAC (CMemory *pMemory, CEventQueue *pEvents, CInterrupt *pInterrupt=NULL);
  virtual ~CCOSMAC() {};

  // CCOSMAC properties ...
public:
  // Get a constant string for the CPU name, type or options ...
  const char *GetDescription() const override {return "8 bit microprocessor";}
  const char *GetName() const  override {return "COSMAC";}
  // Get the CPU's simulated crystal frequency in MHz ...
  uint32_t GetCrystalFrequency() const override {return CLOCK_FREQUENCY;}
  // Get the address of the next instruction to be executed ...
  address_t GetPC() const override;
  // Get or set the extended (1804/5/6) instruction set support ...
  bool IsExtended() const {return m_fExtended;}
  void SetExtended (bool fExtended=true) {m_fExtended = fExtended;}
  // Return the current counter/timer mode ...
  CTMODE GetCounterMode() const {return m_CTmode;}
  static const char *CounterModeToString (CTMODE mode);

  // CCOSMAC public functions ...
public:
  // Reset the CPU ...
  virtual void ClearCPU() override;
  // Simulate one or more COSMAC instructions ...
  virtual STOP_CODE Run (uint32_t nCount=0) override;
  // Read or write CPU registers ... 
  const CCmdArgKeyword::KEYWORD *GetRegisterNames() const override {return g_keysRegisters;}
  unsigned GetRegisterSize (cpureg_t nReg) const override;
  uint16_t GetRegister (cpureg_t nReg) const override;
  void SetRegister (cpureg_t nReg, uint16_t nVal) override;

  // COSMAC basic 16 bit register file operations ...
private:
  // R[r] <= R[r] + 1 ...
  inline void IncReg (uint4_t r) {m_R[r] = MASK16(m_R[r]+1);}
  // R[r] <= R[r] - 1 ...
  inline void DecReg (uint4_t r) {m_R[r] = MASK16(m_R[r]-1);}
  // Get R[r].0 ...
  inline uint8_t GetRegLo (uint4_t r) const {return LOBYTE(m_R[r]);}
  // Get R[r].1 ...
  inline uint8_t GetRegHi (uint4_t r) const {return HIBYTE(m_R[r]);}
  // R[r].0 <= d ...
  inline void PutRegLo (uint4_t r, uint8_t d) {m_R[r] = MKWORD(HIBYTE(m_R[r]), d);}
  // R[r].1 <= d ...
  inline void PutRegHi (uint4_t r, uint8_t d) {m_R[r] = MKWORD(d, LOBYTE(m_R[r]));}

  //   These functions are shortcuts for memory operations.  The COSMAC always
  // addresses memory via a regster, so the memory address is specified as a
  // four bit register number (e.g. N, X, P or a small constant).
private:
  // Return M[R[r]] ...
  inline uint8_t MemRead (uint8_t r) const {return m_pMemory->CPUread(m_R[r]);}
  // Return M[R[r]], and then R[r] <= R[r] + 1 ...
  inline uint8_t MemReadInc (uint8_t r)
    {uint8_t d = MemRead(r);  IncReg(r);  return d;}
  // M[R[r]] <= data ...
  inline void MemWrite (uint4_t r, uint8_t d) {m_pMemory->CPUwrite(m_R[r], d);}
  // M[R[r]] <= data, and then R[r] <= R[r] - 1 ...
  inline void MemWriteDec (uint4_t r, uint8_t d) {MemWrite(r, d);  DecReg(r);}

  // COSMAC DMA emulation (for use by peripheral devices!) ...
public:
  //   DMA "input" is a transfer from the peripheral to memory.  The COSMAC
  // CPU does a memory write for M[R[0]] but doesn't drive the bus (that's
  // left for the peripheral device)...
  void DoDMAinput (uint8_t d)  {MemWrite(0, d);  IncReg(0);}
  //   Likewise, DMA "output" is a transfer from memory to the peripheral.
  // The COSMAC CPU does a memory read for M[R[0]] but doesn't do anything
  // with the data - the peripheral is expected to grab it off the bus.
  uint8_t DoDMAoutput()  {return MemReadInc(0);}

  // Sense (EFx) and flag (Q) support ...
private:
  void UpdateQ (uint1_t bNew);
  uint1_t UpdateEF (uint16_t nSense);
public:
  const char *GetFlagName (uint16_t nFlag=0) const override {return "Q";}
  const char *GetSenseName (uint16_t nSense=0) const override
    {assert(nSense<MAXSENSE);  return g_apszSenseNames[nSense];}
  // Set the default state for unconnected EF inputs ...
  void SetDefaultEF (uint16_t nEF, uint1_t nDefault)
    {assert(nEF<MAXSENSE);  m_EFdefault[nEF] = MASK1(nDefault);}

  // Other, more complex, internal COSMAC operations ...
private:
  // Emulate any ALU function (OR, AND, XOR, ADD, SUB, etc) ...
  void DoALU();
  void DoDecimal();
  // Handle the IDL instruction ...
  void DoIdle();
  // Emulate all the $7x and $Fx opcodes (ALU, shift and miscellaneous) ...
  void Do7xFx();
  void Do7xFxExtended();
  // Emulate input or output instructions ...
  void DoInOut();
  // Emulate short or long branch, or long skip instructions ...
  void DoShortBranch();
  void DoLongBranch();
  void DoInterruptBranch();
  // Emulate the 1804/5/6 counter/timer ...
  void DoCounter();
  void DecrementCounter();
  void UpdateCounter();
  void StartCounter (CTMODE ctmode)  {m_CTmode = ctmode;}
  void StopCounter() {m_CTmode = CT_STOPPED;  m_Prescaler = 0;}
  // Emulate the SCAL and SRET instructions ...
  void DoSCAL();
  void DoSRET();
  // Execute the current opcode in the I and N registers ...
  void DoExecute();
  // Emulate an interrupt acknowledge cycle ...
  void DoInterrupt();
  // Execute an extended 1804/5/6 instruction ...
  void DoExtended();

  // Other private COSMAC helper routines ...
private:
  void AddCycles (uint32_t lCycles);

  // CDP1802 COSMAC internal registers and state ...
private:
  bool      m_fExtended;      // true to emulate 1804/5/6 instructions
  uint16_t  m_R[MAXREGISTER]; // 16x16 general purpose register file
  uint8_t   m_D;	      // basic accumulator for all arithmetic/logic
  uint1_t   m_DF;	      // single bit carry flag
  uint4_t   m_P;	      // points to the program counter register
  uint4_t   m_X;	      // points to the stact/data pointer register
  uint4_t   m_I;	      // holds the high nibble of the current opcode
  uint4_t   m_N;	      // holds the low nibble of the current opcode
  uint8_t   m_T;	      // stores (X,P) during an interrupt
  uint8_t   m_B;	      // temporary register for ALU
  uint1_t   m_MIE;	      // (master) interrupt enable flag
  uint1_t   m_Q;	      // single bit serial output
  uint1_t   m_EF[MAXSENSE];       // current (last) state of the four EF inputs
  uint1_t   m_EFdefault[MAXSENSE];// default state for unconnected EF inputs
  // Extended 1804/5/6 registers ...
  uint8_t   m_CNTR;           // counter/timer count down register
  uint8_t   m_CH;             //  "   "    "   holding (jam) register
  uint8_t   m_Prescaler;      //  "   "    "   prescale counter
  uint1_t   m_LastEF;         // negative edge trigger for EF1/EF2
  CTMODE    m_CTmode;         // current counter/timer mode
  uint1_t   m_ETQ;            // toggle Q mode enabled
  uint1_t   m_CIE;            // counter interrupt enable
  uint1_t   m_CIR;            //   "  "    "    "  request
  uint1_t   m_XIE;            // external interrupt enable
  uint1_t   m_XIR;            //   "   "   "    "   request
  // Constant strings for sense (EFx) names ...
  static const char *g_apszSenseNames[MAXSENSE];
};


