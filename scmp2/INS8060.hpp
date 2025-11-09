//++
//INS8060.hpp -> definitions for SCMP-II microprocessor emulation class
//
// DESCRIPTION:
//   The CSCMP2 class is derived from the CCPU class and implements the SC/MP-II
// specific microprocessor.  BTW, the official National part number for the
// SC/MP-II was INS8060, hence the file name!  FWIW, the original SC/MP was the
// INS8050 and the SC/MP-III was INS8070.
//
// REVISION HISTORY:
// 13-FEB-20  RLA   Copied from CCOSMAC.
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


class CSCMP2 : public CCPU {
  // SC/MP CPU characteristics...
public:
  enum {
    MAXMEMORY   = 65536UL,    // largest possible memory size (bytes)
    MAXSENSE    = 3,          // number of sense (A, B and SIN) inputs
    MAXFLAG     = 4,          // number of flag (F0, F1, F2 & SOUT) outputs
    //   Note that for the original INS8050 SC/MP a microcycle is one half the
    // crystal frequency (or rather, 1/2 the crystal period).  For the newer
    // INS8060 SC/MP-II a microcycle is 1/4 the crystal frequency.  The default
    // crystal for the INS8060 is 4MHz, BUT NIBL BASIC assumes the original CPU
    // with a 1MHz crystal, so that's equivalent to a 2MHz clock on the SC/MP-II.
    DEFAULT_CRYSTAL = 2000000,// 2MHz clock (2us per microcycle) by default
    CLOCKS_PER_MICROCYCLE = 4,// and there are four clocks per microcycle
  };

  // Internal CPU registers ...
  //   These codes are passed to the GetRegister() and SetRegister() methods to
  // access internal CPU registers and state.  The 4 SC/MP pointer registers
  // must always be 0..3, but after that the order is arbitry ...
public:
  enum _REGISTERS {
    REG_PC = 0,               // program counter (also P0)      
    REG_P1 = 1,               // pointer register #1
    REG_P2 = 2,               //   "        "     #2
    REG_P3 = 3,               //   "        "     #3
    REG_AC = 4,               // accumulator
    REG_EX = 5,               // extension register
    REG_SR = 6,               // status register
  };
  // This table is used to translate a name to a REGISTER ...
  static const CCmdArgKeyword::KEYWORD g_keysRegisters[];

  // Status bits in the SR (status) register ...
public:
  enum _FLAGS {
    SR_CYL = 0x80,            // carry/link (unsigned 8 bit overflow)
    SR_OV  = 0x40,            // signed 8 bit overflow
    SR_SB  = 0x20,            // sense B external input (and IRQ!0
    SR_SA  = 0x10,            // sense A external input
    SR_IE  = 0x08,            // interrupt enable
    SR_F2  = 0x04,            // general purpose flag output #2
    SR_F1  = 0x02,            //    "       "      "     "   #1
    SR_F0  = 0x01,            //    "       "      "     "   #0
  };

  // Sense and flag mnemonics for UpdateFlag() and UpdateSense() ...
public:
  enum {
    SENSEA  = 0,              // SENSE A 
    SENSEB  = 1,              // SENSE B
    SIN     = 2,              // serial input
    FLAG0   = 0,              // FLAG 0
    FLAG1   = 1,              // FLAG 1
    FLAG2   = 2,              // FLAG 2
    SOUT    = 3,              // serial output
  };

  // Constructors and destructors...
public:
  CSCMP2 (CMemory *pMemory, CEventQueue *pEvents, CInterrupt *pInterrupt=NULL);
  virtual ~CSCMP2() {};

  // CSCMP2 properties ...
public:
  // Get a constant string for the CPU name, type or options ...
  const char *GetDescription() const override {return "SC/MP-II microprocessor";}
  const char *GetName() const override {return "INS8060";}
  // Get the CPU's simulated crystal frequency in MHz ...
  uint32_t GetCrystalFrequency() const override
    {return NSTOHZ(m_qMicrocycleTime/CLOCKS_PER_MICROCYCLE);}
  void SetCrystalFrequency (uint32_t lFrequency) override
    {assert(lFrequency != 0);  m_qMicrocycleTime = 4000000000ULL/lFrequency;}
  // Get the address of the next instruction to be executed ...
  //   Rememher that the weirdo SC/MP increments the PC BEFORE fetching the
  // next instruction, hence the +1 here!
  inline address_t GetPC() const override {return INC12(m_P[REG_PC]);}

  // CSCMP2 public functions ...
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

  // PC and EA calculations ...
  //   The SC/MP is a bit weird in that address calculations, including
  // incrementing the PC, are done only on the lower 12 bits of the address.
  // The upper 4 bits, called the page number, never change!
public:
  // Sign extend an 8 bit signed displacement to 16 bits ...
  static inline uint16_t SEXT (uint8_t v) {return MKWORD( (ISSET(v,0x80) ? 0xFF : 0), v );}
  // Increment or decrement just the lower 12 bits of a 16 bit value ...
  static inline address_t INC12 (address_t w) {return (w & 0xF000) | ((w+1) & 0x0FFF);}
  static inline address_t DEC12 (address_t w) {return (w & 0xF000) | ((w-1) & 0x0FFF);}
private:
  // Set just the low order 12 bits of a pointer register ...
  //inline void SETP (uint2_t p, address_t v) {m_P[p] = (m_P[p] & 0xF000) | (v & 0x0FFF);}
  // Increment the program counter ...
  inline address_t INCPC() {return m_P[REG_PC] = INC12(m_P[REG_PC]);}
  // Calculate the effective address for this instruction ...
  address_t CalculateEA (uint2_t p, bool fAuto=false);
  // Fetch an immediate operand from memory ...
  inline uint8_t LoadImmediate() {return m_pMemory->CPUread(INCPC());}
  // Fetch a directly addressed operand from memory ...
  uint8_t Load (uint2_t p, bool fAuto=false)
    {return m_pMemory->CPUread(CalculateEA(p, fAuto));}
  // Store a directly addressed operand in memory ...
  void Store (uint8_t bData, uint2_t p, bool fAuto=false)
    {m_pMemory->CPUwrite(CalculateEA(p, fAuto), bData);}

  // Other, more complex, internal SC/MP operations ...
private:
  // Exchange pointer register p with the PC ...
  inline void XPPC (uint2_t p) 
    {address_t t = m_P[REG_PC];  m_P[REG_PC] = m_P[p];  m_P[p] = t;}
  // Exchange the AC with pointer register p high or low byte ...
  inline void XPAL (uint2_t p)
    {uint8_t t = LOBYTE(m_P[p]);  m_P[p] = MKWORD(HIBYTE(m_P[p]), m_AC);  m_AC = t;}
  inline void XPAH (uint2_t p)
    {uint8_t t = HIBYTE(m_P[p]);  m_P[p] = MKWORD(m_AC, LOBYTE(m_P[p]));  m_AC = t;}
  // Exchange AC and EX ...
  inline void XAE() {uint8_t t = m_AC;  m_AC = m_EX;  m_EX = t;}
  // Shift and rotate instructions ...
  uint8_t SR (uint8_t b) {return (b>>1) & 0x7F;}
  uint8_t SRL(uint8_t b) {return ISSET(m_SR, SR_CYL) ? (SR(b)|0x80) : SR(b);}
  uint8_t RR (uint8_t b) {return ISSET(b, 1) ? (SR(b)|0x80) : SR(b);}
  uint8_t RRL(uint8_t b) {
    uint1_t cy = b & 1;  uint8_t t = SRL(b);
    if (cy != 0) SETBIT(m_SR, SR_CYL);  else CLRBIT(m_SR, SR_CYL);
    return t;
  }

  // Simulate various primitive instructions ...
private:
  // Serial I/O (a special case of shifting!) ...
  void SIO();
  // Addition, both binary and decimal ...
  uint8_t ADD (uint8_t op1, uint8_t op2);
  uint8_t DADD (uint8_t op1, uint8_t op2);
  // Jump (conditional and unconditional) instructions ...
  uint64_t JMP(bool fJump, address_t wTarget);
  // The DLY (delay) instruction...
  uint64_t Delay(uint8_t op);
  // Increment and decrement memory (ILD/DLD) instructions ...
  uint8_t AddMemory (address_t wEA, int8_t bData);
  // Get or set the status register and handle flag/sense side effects ...
  void SetStatus (uint8_t bData);
  uint8_t GetStatus();
  // Trace instruction execution ...
  void TraceInstruction() const;
  // Emulate an interrupt acknowledge cycle ...
  void DoInterrupt();
  // Execute the opcode just fetched ...
  uint64_t DoExecute (uint8_t bOpcode);

  // SC/MP internal registers and state ...
private:
  address_t m_P[4];           // 16 bit pointer registers, including the PC
  uint8_t   m_AC;	      // basic accumulator for all arithmetic/logic
  uint8_t   m_EX;	      // extension register
  uint8_t   m_SR;	      // status register
  uint1_t   m_Flag[MAXFLAG];  // three single bit outputs
  uint1_t   m_Sense[MAXSENSE];// two single bit inputs
  uint64_t  m_qMicrocycleTime;// length of one microcyle, in ns
  // Constant strings for sense and flag names ...
  static const char *g_apszSenseNames[MAXSENSE];
  static const char *g_apszFlagNames[MAXFLAG];
};


