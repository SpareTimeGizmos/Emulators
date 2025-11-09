//++
// INS8070.hpp -> definitions for CSCMP3 microprocessor emulation class
//
// DESCRIPTION:
//   The CSCMP3 class is derived from the CCPU class and implements the
// SC/MP-III specific microprocessor.  BTW, the official National part
// number for the SC/MP-III was the INS8070, hence the file name!  FWIW,
// the original SC/MP was the INS8050 and the SC/MP-II was INS8060.
//
// REVISION HISTORY:
// 30-OCT-25  RLA   New file.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ std::string class, et al ...
#include "CommandParser.hpp"    // needed for type KEYWORD
#include "MemoryTypes.h"        // address_t and word_t data types
#include "EventQueue.hpp"       // CEventQueue declarations
#include "Interrupt.hpp"        // interrupt control logic
#include "CPU.hpp"              // CPU definitions
using std::string;              // ...


class CSCMP3 : public CCPU {
  // SC/MP-III CPU characteristics...
public:
  enum {
    MAXMEMORY   = 65536UL,    // largest possible memory size (bytes)
    MAXSENSE    = 2,          // number of sense (A and B) inputs
    MAXFLAG     = 3,          // number of flag (F0, F1, F2) outputs
    // Special vectors in low memory ...
    INTA_VEC    = 0x0004,     // interrupt A vector
    INTB_VEC    = 0x0007,     // interrupt B vector
    CALL_VEC    = 0x0020,     // start of CALL vector table
    // All "DIRECT" addressing is in the range $FF00 .. $FFFF ...
    DIRECT_BASE = 0xFF00,     // ...
    //   For the INS8070 SC/MP-III the standard crystal frequency is
    // 4MHz and one microcycle is 1/4 the crystal frequency.
    DEFAULT_CLOCK   = 4000000,// 4MHz clock (1us per microcycle) by default
    CLOCKS_PER_MICROCYCLE = 4,// and there are four clocks per microcycle
  };

  // Internal CPU registers ...
  //   These codes are passed to the GetRegister() and SetRegister() methods
  // to access internal CPU registers and state.
public:
  enum _REGISTERS {
    REG_PC = 0,               // program counter (also P0)      
    REG_SP = 1,               // stack pointer (also P1)
    REG_P2 = 2,               // generic pointer register P2
    REG_P3 = 3,               //   "        "       "     #3
    REG_A  = 4,               // accumulator
    REG_E  = 5,               // extension register
    REG_S  = 6,               // status register
    REG_T  = 7,               // temporary register
  };
  // This table is used to translate a name to a REGISTER ...
  static const CCmdArgKeyword::KEYWORD g_keysRegisters[];

  // Status bits in the SR (status) register ...
public:
  enum _FLAGS {
    SR_CYL = 0x80,            // carry/link (unsigned 8 bit overflow)
    SR_OV  = 0x40,            // signed 8 bit overflow
    SR_SB  = 0x20,            // sense B external input
    SR_SA  = 0x10,            // sense A external input
    SR_F3  = 0x08,            // general purpose flag output #3
    SR_F2  = 0x04,            //    "       "      "     "   #2
    SR_F1  = 0x02,            //    "       "      "     "   #1
    SR_IE  = 0x01,            // interrupt enable
  };

  // Sense and flag mnemonics for UpdateFlag() and UpdateSense() ...
public:
  enum {
    SENSEA  = 0,              // SENSE A 
    SENSEB  = 1,              // SENSE B
    FLAG1   = 0,              // FLAG 1
    FLAG2   = 1,              // FLAG 2
    FLAG3   = 2,              // FLAG 3
  };

  // Constructors and destructors...
public:
  CSCMP3 (CMemory *pMemory, CEventQueue *pEvents, CInterrupt *pInterrupt=NULL);
  virtual ~CSCMP3() {};

  // CSCMP3 properties ...
public:
  // Get a constant string for the CPU name, type or options ...
  const char *GetDescription() const override {return "SC/MP-III microprocessor";}
  const char *GetName() const override {return "INS8070";}
  // Get the CPU's simulated crystal frequency in MHz ...
  uint32_t GetCrystalFrequency() const override
    {return NSTOHZ(m_qMicrocycleTime/CLOCKS_PER_MICROCYCLE);}
  void SetCrystalFrequency (uint32_t lFrequency) override
    {assert(lFrequency != 0);  m_qMicrocycleTime = 4000000000ULL/lFrequency;}

  // Get the address of the next instruction to be executed ...
  //   Remember that the weirdo SC/MP increments the PC BEFORE fetching the
  // next instruction, hence the +1 here!
  inline address_t GetPC() const override {return ADDRESS(m_PC+1);}

  // CSCMP3 public functions ...
public:
  // Reset the CPU ...
  virtual void ClearCPU() override;
  // Simulate one or more SC/MP instructions ...
  STOP_CODE Run (uint32_t nCount=0) override;
  // Read or write CPU registers ... 
  const CCmdArgKeyword::KEYWORD *GetRegisterNames() const override {return g_keysRegisters;}
  unsigned GetRegisterSize (cpureg_t nReg) const override;
  uint16_t GetRegister (cpureg_t nReg) const override;
  void SetRegister (cpureg_t nReg, uint16_t nVal) override;

  // Sense and flag support ...
private:
  void UpdateFlag (uint16_t nFlag, uint1_t bNew);
  uint1_t UpdateSense (uint16_t nSense);
public:
  const char *GetSenseName (uint16_t nSense=0) const override
    {assert(nSense<MAXSENSE);  return g_apszSenseNames[nSense];}
  const char *GetFlagName (uint16_t nFlag=0) const override
    {assert(nFlag<MAXFLAG);  return g_apszFlagNames[nFlag];}

  // Miscellaneous primitives ...
private:
  // Test the sign bit of 8 or 16 bit values ...
  static inline bool ISNEG8  (uint8_t  v) {return ISSET(v, 0x80);}
  static inline bool ISNEG16 (uint16_t v) {return ISSET(v, 0x8000);}
  // Sign extend 8 bits to 16 bits ...
  static inline uint16_t SEXT16 (uint8_t v) {return MKWORD((ISNEG8(v) ? 0xFF : 0), v);}
  // Increment or decrement a 16 bit value.
  static inline uint16_t ADD16 (uint16_t  v, uint16_t i=1) {return MASK16(v+i);}
  static inline uint16_t SUB16 (uint16_t  v, uint16_t d=1) {return MASK16(v-d);}
  static inline uint16_t INC16 (uint16_t &v, uint16_t i=1) {return v = MASK16(v+i);}
  static inline uint16_t DEC16 (uint16_t &v, uint16_t d=1) {return v = MASK16(v-d);}

  // Memory operations ...
  //   Note that we MUST use our own routines, MEMRxx and MEMWxx, to access
  // memory because the INS807x timing is different for internal RAM/ROM
  // vs external memory!
private:
  // Handle internal and external memory read and write operations ...
  uint8_t MEMR8 (address_t wAddr);
  void MEMW8 (address_t wAddr, uint8_t bData);
  inline uint16_t MEMR16 (address_t wAddr)
    {uint8_t l = MEMR8(wAddr);  uint8_t h = MEMR8(wAddr+1);  return MKWORD(h,l);}
  inline void MEMW16 (address_t wAddr, uint16_t wData)
    {MEMW8(wAddr, LOBYTE(wData));  MEMW8((wAddr+1), HIBYTE(wData));}
  // Stack operations ...
  inline void Push8 (uint8_t bData) {MEMW8(DEC16(m_SP), bData);}
  inline uint8_t Pop8()
    {uint8_t b = MEMR8(m_SP);  INC16(m_SP);  return b;}
  inline void Push16 (uint16_t wData)
    {Push8(HIBYTE(wData));  Push8(LOBYTE(wData));}
  inline uint16_t Pop16()
    {uint8_t l = Pop8();  uint8_t h = Pop8();  return MKWORD(h,l);}

  // Addressing mode calculations ...
private:
  // Immediate mode addressing, both 8 and 16 bit operands ...
  uint8_t IMM8() {return MEMR8(INC16(m_PC));}
  uint16_t IMM16()
    {uint8_t l = IMM8();  uint8_t h = IMM8();  return MKWORD(h,l);}
  // Register (including PC) relative addressing ...
  address_t REL8 (address_t wBase) {return ADD16(wBase, SEXT16(IMM8()));}
  // Page $FFxx (the INS8070 calls this "DIRECT") addressing ...
  inline address_t DIRECT (uint8_t bOffset) const {return DIRECT_BASE | bOffset;}
  // And auto increment/decrement register addressing ...
  address_t AUTO (uint16_t &wReg, uint8_t bOffset);

  // Special functions for the EA "register" ...
  //   Note that EA is not really a register, but is the concatenation of the
  // two 8 bit E and A registers to make a single 16 bit virtual register.
private:
  // Get, set and exchange the EA register ...
  inline uint16_t GetEA() const {return MKWORD(m_E, m_A);}
  inline void SetEA (uint16_t w) {m_E = HIBYTE(w);  m_A = LOBYTE(w);}
  void ExchangeEA (uint16_t &wReg)
    {uint16_t w = GetEA();  SetEA(wReg);  wReg = w;}
  // Shift EA left or right one bit ...
  //   Note that bits shifted out of the left or right end are lost, and zeros
  // are always shifted in on the other end ...
  void ShiftRightEA() {SetEA(GetEA() >> 1);}
  void ShiftLeftEA() {SetEA(GetEA() << 1);}
  // Double precision add or subtract from the EA register ...
  void AddEA (uint16_t wData);

  // Accumulator "A" functions ...
  //   The basic shift left and shift right are easy - the bit shifted out is
  // always lost, and zero is always shifted in ...
  inline void ShiftLeftA() {m_A <<= 1;}
  inline void ShiftRightA() {m_A >>= 1;}
  //   SRL shifts A right with the current carry bit shifted in on the left.
  // Note that there is NO SLL instruction!
  inline void ShiftRightAL()
    {ShiftRightA();  if (ISSET(m_S,SR_CYL)) m_A |= 0x80;}
  //   RR shifts A right, and the bit shifted out on the right is shifted in
  // on the left. The carry bit is not affected.  There is no RL instruction!
  inline void RotateRightA()
    {uint8_t b = m_A & 1;  ShiftRightA();  if (b) m_A |= 0x80;}
  //   RRL shifts A right.  The bit shifted out on the right is shifted into
  // the carry bit, and the previous carry bit is shifted in on the left.
  void RotateRightAL()
    {uint8_t b = ISSET(m_S,SR_CYL);  CLRBIT(m_S,SR_CYL);
     if (m_A & 1) SETBIT(m_S,SR_CYL);  ShiftRightA();  if (b) m_A |= 0x80;}
  // Add or subtract from the A register ...
  void AddA(uint8_t bData);

  // Other special instructions ...
private:
  // Multiply and divide ...
  void Multiply();
  void Divide();
  // Short form subroutine call ...
  void ShortCall (uint8_t bVector);
  // Branch if not digit ...
  uint64_t BranchNotDigit (address_t wAddr);
  // Search and skip if character match ...
  uint64_t SearchAndSkip (uint16_t &wRegister);
  // Increment/decrement memory and load ...
  void AddMemory (address_t wAddr, int8_t bAdd);

  // Other INS8070 CPU operations ...
private:
  // Add microcycles to the execution time ...
  inline void AddCycles (uint64_t qCycles) {AddTime(qCycles*m_qMicrocycleTime);}
  // Get or set the status register and handle flag/sense side effects ...
  void SetStatus (uint8_t bData);
  uint8_t GetStatus();
  // Trace instruction execution ...
  void TraceInstruction() const;
  // Emulate an interrupt acknowledge cycle ...
  void DoInterrupt();
  // Execute the opcode just fetched ...
  uint64_t DoExecute (uint8_t bOpcode);

  // INS8070 internal registers and state ...
private:
  address_t m_PC;             // program counter
  address_t m_SP;             // stack pointer
  address_t m_P2, m_P3;       // general purpose pointer registers
  uint8_t   m_A;	      // basic accumulator for all arithmetic/logic
  uint8_t   m_E;	      // extension register
  uint8_t   m_S;	      // status register
  uint16_t  m_T;              // temporary register
  uint1_t   m_Flag[MAXFLAG];  // three single bit outputs
  uint1_t   m_Sense[MAXSENSE];// two single bit inputs
  uint64_t  m_qMicrocycleTime;// length of one microcyle, in ns
  // Constant strings for sense and flag names ...
  static const char *g_apszSenseNames[MAXSENSE];
  static const char *g_apszFlagNames[MAXFLAG];
};


