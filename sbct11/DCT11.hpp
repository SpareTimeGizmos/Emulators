//++
// DCT11.hpp -> definitions for DEC T11 microprocessor emulation
//
//   COPYRIGHT (C) 2015-2024 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
// 
// LICENSE:
//    This file is part of the SBCT11 emulator project.  SBCT11 is free
// software; you may redistribute it and/or modify it under the terms of
// the GNU Affero General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any
// later version.
//
//    SBCT11 is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License
// for more details.  You should have received a copy of the GNU Affero General
// Public License along with SBCT11.  If not, see http://www.gnu.org/licenses/.
//
// DESCRIPTION:
//   This file contains class definitions for the DEC DCT11 PDP11 microprocessor
// emulation.
//
// REVISION HISTORY:
// 27-FEB-20  RLA   Copied from C2650.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <assert.h>             // assert() (what else??)
#include <string>               // C++ std::string class, et al ...
#include "MemoryTypes.h"        // address_t and word_t data types
#include "Memory.hpp"           // generic memory class
#include "EventQueue.hpp"       // CEventQueue functions
#include "MemoryMap.hpp"        // Memory mapping for the SBCT11/DCT11
#include "CPU.hpp"              // CPU definitions
#include "PIC11.hpp"            // DCT11 priority interrupt control
using std::string;              // ...


class CDCT11 : public CCPU {
  //++
  // DEC DCT11 CPU emulation ...
  //--

  // Magic numbers and constants ...
public:
  enum {
    CLOCK_FREQUENCY = 4915200UL,  // standard SBCT11 crystal is 4.9152MHz
    PROCESSOR_TYPE  = 4,          // value returned by MFPT for the T11
    MAXIRQ          = 16,         // number of external interrupts supported
  };

  // Internal CPU registers ...
  //   These codes are passed to the GetRegister() and SetRegister() methods to
  // access internal CPU registers and state.  Note that the indices 0..7 MUST
  // correspond to the actual register number!
public:
  enum _REGISTERS {
    REG_R0    =  0,           // general purpose register
    REG_R1    =  1,           // ...
    REG_R2    =  2,           // ...
    REG_R3    =  3,           // ...
    REG_R4    =  4,           // ...
    REG_R5    =  5,           // ...
    REG_SP    =  6,           // R6 is the stack pointer
    REG_PC    =  7,           // R7 is the program counter
    MAXREG    =  8,           // number of regular registers
    REG_PSW   =  8,           // program status word
  };
  // This table is used to translate a name to a REGISTER ...
  static const CCmdArgKeyword::KEYWORD g_keysRegisters[];

  // Bits in the program status registers ...
public:
  enum _PSW_BITS {
    PSW_C     = 0000001,      // carry 
    PSW_V     = 0000002,      // two's complement overflow
    PSW_Z     = 0000004,      // zero
    PSW_N     = 0000010,      // negative
    PSW_T     = 0000020,      // trace trap
    PSW_PRIO  = 0000340,      // processor priority
      PSW_PRI7= 0000340,      //  ... priority 7
      PSW_PRI6= 0000300,      //  ... priority 6
      PSW_PRI5= 0000240,      //  ... priority 5
      PSW_PRI4= 0000200,      //  ... priority 4
      // The DCT11 doesn't implement priorities 3..1!
      PSW_PRI0= 0000000,      //  ... priority 0
    PSW_BITS  = 0000377,      // PSW bits that are actually implemented
  };

  // DCT11 trap bits and vectors ...
public:
  //   The member m_bRequests keeps a bitmap of pending trap and interrupt
  // requests.  It's perfectly possible to have multiple requests active at
  // once (say, an interrupt occurs while executing an EMT instruction, or
  // somebody with a debugger tries to trace thru a reserved opcode).  Note
  // that the T11 only has five distinct conditions, so a byte sized bit
  // vector is plenty!
  enum {
    REQ_HALT        = 020,    // HALT request (internal or external)
    REQ_TRACE       = 010,    // T bit (trace) trap request
    REQ_POWERFAIL   = 004,    // power fail request
    REQ_EXTERNAL    = 002,    // any external interrupt request
    REQ_INSTRUCTION = 001,    // any instruction trap
  };
  enum {
    VEC_UNDEF       = 000,    // undefined or default trap vector
    VEC_RESERVED    = 004,    // JMP or JSR to a register
    VEC_ILLEGAL     = 010,    // illegal instruction
    VEC_BPT         = 014,    // breakpoint (BPT) instruction
    VEC_IOT         = 020,    // IOT instruction
    VEC_POWERFAIL   = 024,    // power fail trap
    VEC_EMT         = 030,    // EMT instruction
    VEC_TRAP        = 034,    // TRAP instruction
  };

  // DCT11 MODE register bits ...
public:
  enum {
    //   Note that most of the mode register bits affect the hardware bus interface
    // (e.g. 8/16 bit mode, normal/delayed mode, static/dynamic mode, etc) and
    // aren't mentioned here.
    MODE_START    = 0160000,  // mask of all start/restart address bits
    MODE_172000   = 0160000,  // start address 172000, restart address 172004
    MODE_173000   = 0140000,  //   "    "   "  173000,  "   "    "  "  173004
    MODE_000000   = 0120000,  //   "    "   "  000000,  "   "    "  "  000004
    MODE_010000   = 0100000,  //   "    "   "  010000,  "   "    "  "  010004
    MODE_020000   = 0060000,  //   "    "   "  020000,  "   "    "  "  020004
    MODE_040000   = 0040000,  //   "    "   "  040000,  "   "    "  "  040004
    MODE_100000   = 0020000,  //   "    "   "  100000,  "   "    "  "  100004
    MODE_140000   = 0000000,  //   "    "   "  140000,  "   "    "  "  140004
    MODE_LMC      = 0000002,  // long microcycle mode (used to calculate times)
  };

  // Constructors and destructor ...
public:
  CDCT11 (uint16_t wMode, CMemoryMap *pMemory, CEventQueue *pEvent, CPIC11 *pInterrupt);
  virtual ~CDCT11() {};

  // DCT11 properties ...
public:
  // Get a constant string for the CPU name, type or options ...
  const char *GetDescription() const override {return "DEC T11 Microprocessor";}
  const char *GetName() const override {return "DCT11";}
  // Get the CPU's simulated crystal frequency in MHz ...
  uint32_t GetCrystalFrequency() const override {return CLOCK_FREQUENCY;}
  // Get the address of the next instruction to be executed ...
  inline address_t GetPC() const override {return m_wR[REG_PC];}
  inline void SetPC (address_t a) override {m_wR[REG_PC] = a;}
  // Decode the PSW and return it as a string ...
  string GetPSW() const;
  // Get or set the T11 mode register ...
  uint16_t GetMode() const {return m_wMode;}
  void SetMode (uint16_t wMode)
    {m_wMode = wMode & (MODE_START|MODE_LMC);}
  // Return the start or restart address selected by the mode register ...
  uint16_t GetStartAddress() const;
  uint16_t GetRestartAddress() const {return GetStartAddress()+4;}
  // Return TRUE if a long microcycle is selected ...
  bool IsLMC() const {return ISSET(m_wMode, MODE_LMC);}
  // Return a pointer to our own special interrupt handler class ...
  inline CPIC11 *GetPIC() const {return dynamic_cast<CPIC11 *>(m_pInterrupt);}
  // Return a pointer to our own special memory class ...
  inline CMemoryMap *GetMemory() const {return dynamic_cast<CMemoryMap *>(m_pMemory);}

  // DCT11 public methods ...
public:
  // Reset the CPU ...
  void MasterClear() override;
  // Simulate one or more DCT11 instructions ...
  STOP_CODE Run (uint32_t nCount=0) override;
  // Read or write CPU registers ... 
  const CCmdArgKeyword::KEYWORD *GetRegisterNames() const override {return g_keysRegisters;}
  unsigned GetRegisterSize (cpureg_t r) const override {return (r == REG_PSW) ? 8 : 16;}
  uint16_t GetRegister (cpureg_t nReg) const override;
  void SetRegister (cpureg_t nReg, uint16_t nVal) override;
  // Request the CPU halt (for NXM trap or console BREAK) ...
  void HaltRequest() {SETBIT(m_bRequests, REQ_HALT);}
  // Request the CPU take the power fail trap vector ...
  void PowerFailRequest() {SETBIT(m_bRequests, REQ_POWERFAIL);}

  // DCT11 memory access primitives ...
private:
  // Increment or decrement a 16 bit value.
  static inline uint16_t ADD16 (uint16_t  v, uint16_t i=1) {return MASK16(v+i);}
  static inline uint16_t SUB16 (uint16_t  v, uint16_t d=1) {return MASK16(v-d);}
  static inline uint16_t INC16 (uint16_t &v, uint16_t i=1) {return v = MASK16(v+i);}
  static inline uint16_t DEC16 (uint16_t &v, uint16_t d=1) {return v = MASK16(v-d);}
  // Read and write bytes and words from memory ...
  //   There are a couple of things worthy of note here - first, the PDP11 famously
  // uses memory mapped I/O, however we don't worry about that here.  The I/O vs
  // memory, and RAM vs EPROM, distinction is handled in the CMemoryMap module.
  // That's consistent with the way the real T11 chip works, anyway.
  //
  //   The other thing is that the SBCT11 actually uses the T11 in 16 bit bus mode.
  // This is transparent as far as RAM/ROM goes, but it could conceiveably have some
  // effect on I/O devices, because it's the difference between one word access vs
  // two byte accesses.  HOWEVER, the only actual 16 bit I/O device in the SBCT11 is
  // the IDE data register, and we just handle that one as a special case. 
  inline uint8_t READB (address_t a) const {return m_pMemory->CPUread(a);}
  inline void WRITEB (address_t a, uint8_t b) {m_pMemory->CPUwrite(a, b);}
  // Word versions of the above ...
  //   Note that the T11 has no concept of odd address traps - a word access
  // simply drops the LSB, so a word access to an odd address actually just
  // accesses the next lower even address...
  //
  //   BTW, remember that the PDP-11 is a little endian machine!
  inline uint16_t READW (address_t a) const
    {uint8_t l = READB(a & (~1));  uint8_t h = READB(a|1);  return MKWORD(h,l);}
  inline void WRITEW (address_t a, uint16_t w)
    {WRITEB((a & (~1)), LOBYTE(w));  WRITEB((a|1), HIBYTE(w));}
  //   Fetch the word (presumably an opcode) pointed to by the PC and increment
  // the PC by 2.  We want to be careful here in the case that the PC contains
  // an odd address.  This shouldn't normally happen, but if it does we need the
  // same behavior as READW()!
  inline uint16_t FETCHW()
    {uint16_t w = READW(m_wR[REG_PC]);  INC16(m_wR[REG_PC], 2);  return w;}

  // DCT11 PSW primitives ...
private:
  // Test whether a particular PSW bit is set or cleared ...
  inline bool ISPSW (uint16_t f) const {return ISSET(m_bPSW, f);}
  inline uint8_t ISN() const {return ISPSW(PSW_N) ? 1 : 0;}
  inline uint8_t ISZ() const {return ISPSW(PSW_Z) ? 1 : 0;}
  inline uint8_t ISV() const {return ISPSW(PSW_V) ? 1 : 0;}
  inline uint8_t ISC() const {return ISPSW(PSW_C) ? 1 : 0;}
  // Set or clear the N, Z, V, and/or C flags ...
  inline void SetN (bool f) {if (f) SETBIT(m_bPSW, PSW_N);  else CLRBIT(m_bPSW, PSW_N);}
  inline void SetZ (bool f) {if (f) SETBIT(m_bPSW, PSW_Z);  else CLRBIT(m_bPSW, PSW_Z);}
  inline void SetV (bool f) {if (f) SETBIT(m_bPSW, PSW_V);  else CLRBIT(m_bPSW, PSW_V);}
  inline void SetC (bool f) {if (f) SETBIT(m_bPSW, PSW_C);  else CLRBIT(m_bPSW, PSW_C);}
  // Test the 8 or 16 bit sign bit ...
  static inline bool ISNEGW (uint16_t w) {return ISSET(w, 0100000);}
  static inline bool ISNEGB (uint8_t  b) {return ISSET(b, 0200);}
  // Set the N and Z flags based on one 8 or 16 bit value ...
  inline void SetZNB (uint8_t  b) {SetZ(b==0);  SetN(ISNEGB(b));}
  inline void SetZNW (uint16_t w) {SetZ(w==0);  SetN(ISNEGW(w));}

  // DCT11 Two operand instructions ...
private:
  uint32_t MOVW (uint8_t bSRCmode, uint8_t bSRCreg, uint8_t bDSTmode, uint8_t bDSTreg);
  uint32_t MOVB (uint8_t bSRCmode, uint8_t bSRCreg, uint8_t bDSTmode, uint8_t bDSTreg);
  uint32_t CMPW (uint8_t bSRCmode, uint8_t bSRCreg, uint8_t bDSTmode, uint8_t bDSTreg);
  uint32_t CMPB (uint8_t bSRCmode, uint8_t bSRCreg, uint8_t bDSTmode, uint8_t bDSTreg);
  uint32_t BITW (uint8_t bSRCmode, uint8_t bSRCreg, uint8_t bDSTmode, uint8_t bDSTreg);
  uint32_t BITB (uint8_t bSRCmode, uint8_t bSRCreg, uint8_t bDSTmode, uint8_t bDSTreg);
  uint32_t BICW (uint8_t bSRCmode, uint8_t bSRCreg, uint8_t bDSTmode, uint8_t bDSTreg);
  uint32_t BICB (uint8_t bSRCmode, uint8_t bSRCreg, uint8_t bDSTmode, uint8_t bDSTreg);
  uint32_t BISW (uint8_t bSRCmode, uint8_t bSRCreg, uint8_t bDSTmode, uint8_t bDSTreg);
  uint32_t BISB (uint8_t bSRCmode, uint8_t bSRCreg, uint8_t bDSTmode, uint8_t bDSTreg);
  uint32_t ADDW (uint8_t bSRCmode, uint8_t bSRCreg, uint8_t bDSTmode, uint8_t bDSTreg);
  uint32_t SUBW (uint8_t bSRCmode, uint8_t bSRCreg, uint8_t bDSTmode, uint8_t bDSTreg);
  uint32_t XORW (uint8_t bSRCreg, uint8_t bDSTmode, uint8_t bDSTreg);

  // DCT11 Single operand instructions ...
private:
  uint32_t CLRW (uint8_t bDSTmode, uint8_t bDSTreg);
  uint32_t CLRB (uint8_t bDSTmode, uint8_t bDSTreg);
  uint32_t COMW (uint8_t bDSTmode, uint8_t bDSTreg);
  uint32_t COMB (uint8_t bDSTmode, uint8_t bDSTreg);
  uint32_t INCW (uint8_t bDSTmode, uint8_t bDSTreg);
  uint32_t INCB (uint8_t bDSTmode, uint8_t bDSTreg);
  uint32_t DECW (uint8_t bDSTmode, uint8_t bDSTreg);
  uint32_t DECB (uint8_t bDSTmode, uint8_t bDSTreg);
  uint32_t NEGW (uint8_t bDSTmode, uint8_t bDSTreg);
  uint32_t NEGB (uint8_t bDSTmode, uint8_t bDSTreg);
  uint32_t ADCW (uint8_t bDSTmode, uint8_t bDSTreg);
  uint32_t ADCB (uint8_t bDSTmode, uint8_t bDSTreg);
  uint32_t SBCW (uint8_t bDSTmode, uint8_t bDSTreg);
  uint32_t SBCB (uint8_t bDSTmode, uint8_t bDSTreg);
  uint32_t TSTW (uint8_t bDSTmode, uint8_t bDSTreg);
  uint32_t TSTB (uint8_t bDSTmode, uint8_t bDSTreg);
  uint32_t RORW (uint8_t bDSTmode, uint8_t bDSTreg);
  uint32_t RORB (uint8_t bDSTmode, uint8_t bDSTreg);
  uint32_t ROLW (uint8_t bDSTmode, uint8_t bDSTreg);
  uint32_t ROLB (uint8_t bDSTmode, uint8_t bDSTreg);
  uint32_t ASRW (uint8_t bDSTmode, uint8_t bDSTreg);
  uint32_t ASRB (uint8_t bDSTmode, uint8_t bDSTreg);
  uint32_t ASLW (uint8_t bDSTmode, uint8_t bDSTreg);
  uint32_t ASLB (uint8_t bDSTmode, uint8_t bDSTreg);
  uint32_t SXTW (uint8_t bDSTmode, uint8_t bDSTreg);
  uint32_t MTPS (uint8_t bDSTmode, uint8_t bDSTreg);
  uint32_t MFPS (uint8_t bDSTmode, uint8_t bDSTreg);
  uint32_t SWAB (uint8_t bDSTmode, uint8_t bDSTreg);

  // DCT11 Branch, jump and trap instructions ...
private:
  uint32_t DoBranch (bool fByte, uint8_t bOpcode, uint8_t bOffset);
  uint32_t TrapNow (uint16_t wVector);
  uint32_t TrapNow (uint16_t wNewPC, uint16_t wNewPSW);
  uint32_t SOB (uint8_t bReg, uint8_t bOffset);
  uint32_t JMP (uint8_t bMode, uint8_t bReg);
  uint32_t JSR (uint8_t bReg, uint8_t bDSTmode, uint8_t bDSTreg);
  uint32_t RTS (uint8_t bReg);
  uint32_t RTI (bool fInhibit=false);
  uint32_t RTT() {return RTI(true);}

  // DCT11 Miscellaneous instructions ...
private:
  uint32_t SETCLRCC (uint16_t wOpcode);
  uint32_t RESET();
  uint32_t HALT();
  uint32_t WAIT();

  // DCT11 CPU operations ...
private:
  // Sign extend an 8 bit value to 16 bits ..
  static inline uint16_t SXT8 (uint8_t b) {return ISSET(b,0200) ? (b|0177400) : b;}
  // Calculate the effective address given the mode and register ...
  uint32_t CALCEA (bool fByte, uint8_t bMode, uint8_t bReg, uint16_t &wEA);
  uint32_t CALCEAB (uint8_t bMode, uint8_t bReg, uint16_t& wEA)
    {return CALCEA(true, bMode, bReg, wEA);}
  uint32_t CALCEAW (uint8_t bMode, uint8_t bReg, uint16_t& wEA)
    {return CALCEA(false, bMode, bReg, wEA);}
  // Decode and execute the opcode just fetched ...
  uint32_t DoOpcode00 (uint16_t wOpcode);
  uint32_t DoSingleOperand (bool fByte, uint8_t bOpcode, uint8_t bDSTmode, uint8_t bDSTreg);
  uint32_t DoExecute (uint16_t wIR);
  // Process halt, trap and interrupt requests ...
  uint32_t BreakpointRequest() {SETBIT(m_bRequests, REQ_TRACE);  return 6;};
  uint32_t InstructionTrap (address_t wVector);
  uint32_t DoRequests (CPIC11::IRQ_t nIRQ);
  // Accumulate simulated time ...
  void AddCycles (uint32_t lCycles);

  // DCT11 internal registers and state ...
private:
  uint16_t   m_wR[MAXREG];    // primary register set
  uint8_t    m_bPSW;          // program status word
  uint16_t   m_wMode;         // T11 mode register
  uint8_t    m_bRequests;     // trap and interrupt requests pending
  address_t  m_wItrapVector;  // vector for pending instruction trap
};


