//++
// S2650.hpp -> definitions for Signetics 2650 microprocessor emulation
//
// DESCRIPTION:
//   The C2650 class is derived from the CCPU class and implements the 2650
// specific microprocessor.
//
// REVISION HISTORY:
// 21-FEB-20  RLA   Copied from C2650.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <assert.h>             // assert() (what else??)
#include <string>               // C++ std::string class, et al ...
#include "CommandParser.hpp"    // needed for type KEYWORD
#include "MemoryTypes.h"        // address_t and word_t data types
#include "EventQueue.hpp"       // CEventQueue declarations
#include "Interrupt.hpp"        // interrupt simulation logic
#include "CPU.hpp"              // CPU definitions
using std::string;              // ...


class C2650 : public CCPU {
  // 2650 CPU characteristics...
public:
  enum {
    //   Notice that the 2650 supports at most 32K bytes of memory.  Signetics
    // squandered one of their address bits on chained indirect addressing!
    MAXMEMORY     = 32768UL,    // largest possible memory size (bytes)
    MAXSENSE      = 1,          // number of sense inputs (exactly 1 sense!)
    MAXFLAG       = 1,          // number of flag outputs (exactly 1 flag!)
    DEFAULT_CLOCK = 1000000UL,  // standard clock/crystal frequency (1MHz)
  };

  // Internal CPU registers ...
  //   These codes are passed to the GetRegister() and SetRegister() methods to
  // access internal CPU registers and state.  Note that the 2650 keeps the
  // subroutine call/return stack on chip, and we pretend that it's another
  // set of 8 registers!
public:
  enum _REGISTERS {
    REG_R0    =  0,           // shared between both register sets
    REG_R1    =  1,           // temporary/working register
    REG_R2    =  2,           // ...
    REG_R3    =  3,           // ...
    REG_R1P   =  4,           // alternate register set
    REG_R2P   =  5,           // ...
    REG_R3P   =  6,           // ...
    REG_PSU   =  7,           // program status (upper)
    REG_PSL   =  8,           //   "  "    "     lower
    REG_IAR   =  9,           // instruction address register
    REG_STACK = 10,           // the actual return stack!
    MAXSTACK  =  8,           //   ... size of the on-chip stack
  };
  // This table is used to translate a name to a REGISTER ...
  static const CCmdArgKeyword::KEYWORD g_keysRegisters[];

  // Status bits in the PSU and PSL registers ...
public:
  enum _PSU_BITS {
    PSU_S       = 0x80,       // sense input
    PSU_F       = 0x40,       // flag output
    PSU_II      = 0x20,       // interrupt inhibit
    PSU_SP      = 0x07,       // stack pointer (3 bits)
    PSU_MASK    = 0xE7,       // valid bits in the PSU
  };
  enum _PSL_BITS {
    PSL_CC      = 0xC0,       // condition code bits
    PSL_IDC     = 0x20,       // inter-digit (half) carry
    PSL_RS      = 0x10,       // register bank select
    PSL_WC      = 0x08,       // with carry flag
    PSL_OVF     = 0x04,       // overflow
    PSL_COM     = 0x02,       // compare logical/arithmetic
    PSL_CY      = 0x01,       // carry/borrow
  };
  enum _CC_BITS {
    CC_ZERO     = 0x00,       // result is zero 
    CC_POSITIVE = 0x40,       // result is .GE. zero
    CC_NEGATIVE = 0x80,       // result is .LT. zero
    CC_EQUAL    = CC_ZERO,    // other words for the same thing
    CC_GREATER  = CC_POSITIVE,// ...
    CC_LESS     = CC_NEGATIVE,// ...
  };


  // Constructors and destructors...
public:
  C2650 (CMemory *pMemory, CEventQueue *pEvents, CInterrupt *pInterrupt=NULL);
  virtual ~C2650() {};

  // C2650 properties ...
public:
  // Get a constant string for the CPU name, type or options ...
  const char *GetDescription() const override {return "Signetics microprocessor";}
  const char *GetName() const override {return "S2650";}
  // Get the address of the next instruction to be executed ...
  inline address_t GetPC() const override {return m_IAR;}
  inline void SetPC (address_t a) override {m_IAR = a;}
  //   The S2650 data sheet describes instruction execution time in terms of
  // "processor cycles".  Each processor cycle requires three cycles of the
  // crystal clock.
  uint64_t CycleTime() const {return 3ULL * HZTONS(m_lClockFrequency);}

  // C2650 public functions ...
public:
  // Reset the CPU ...
  virtual void ClearCPU() override;
  // Simulate one or more 2650 instructions ...
  STOP_CODE Run (uint32_t nCount=0) override;
  // Read or write CPU registers ... 
  const CCmdArgKeyword::KEYWORD *GetRegisterNames() const override {return g_keysRegisters;}
  unsigned GetRegisterSize (cpureg_t r) const override
    {return (r >= REG_IAR) ? 16 : 8;}
  uint16_t GetRegister (cpureg_t nReg) const override;
  void SetRegister (cpureg_t nReg, uint16_t nVal) override;

  // Sense and flag support ...
public:
  const char *GetSenseName (uint16_t nSense=0) const override
    {assert(nSense<MAXSENSE);  return g_apszSenseNames[nSense];}
  const char *GetFlagName (uint16_t nFlag=0) const override
    {assert(nFlag<MAXFLAG);  return g_apszFlagNames[nFlag];}

  // 2650 memory access primitives ...
public:
  // Add an offset to an address using 13 bit "page mode" arithmetic (!).
  //   The upper two, page select, bits aren't affected...
  static inline address_t ADD13 (address_t wAddr, address_t wOff)
    {return (wAddr & 0x6000) | ((wAddr+wOff) & 0x1FFF);}
  // Increment a pointer (probably the IAR) using 13 bit wrap around arithmetic
  static inline address_t INC13 (address_t &r) {r = ADD13(r, 1);  return r;}
  // Sign extend a 7 bit relative offset to a full 8 or 16 bits ...
  static inline uint8_t   SXT8  (uint8_t o) {return ISSET(o,0x40) ? (o|  0x80) : (o&  0x7F);}
  static inline address_t SXT16 (uint8_t o) {return ISSET(o,0x40) ? (o|0xFF80) : (o&0x007F);}
private:
  // Return a reference to the specified register ...
  uint8_t &REG (uint2_t r)
    {assert(r<4);  return ((r==0) || !ISSET(m_PSL,PSL_RS)) ? m_R[r] : m_RP[r-1];}
  // These two defintions save some typing for memory access...
  inline uint8_t MEMR (address_t a) {return m_pMemory->CPUread(MASK15(a));}
  inline void MEMW (address_t a, uint8_t b) {m_pMemory->CPUwrite(MASK15(a), b);}
  // Fetch an 8 bit value using the IAR, and increment the IAR ..
  inline uint8_t Fetch8() {uint8_t b=MEMR(m_IAR);  INC13(m_IAR);  return b;}
  // Fetch a 16 bit value.  Note that the 2650 is a big endian machine!
  inline address_t Fetch16()  {uint8_t h=Fetch8();  uint8_t l=Fetch8();  return MKWORD(h, l);}
  // Like above, except use the specified pointer istead of the IAR...
  inline address_t Fetch16 (address_t a)
    {uint8_t h=MEMR(a);  INC13(a);  uint8_t l=MEMR(a);  return MKWORD(h, l);}

  // Subroutine return address stack operations ...
private:
  inline uint8_t GetSP() const {return (m_PSU & PSU_SP);}
  inline void SetSP (uint8_t s) {m_PSU = (m_PSU & ~PSU_SP) | (s & PSU_SP);}
  inline void PUSH (address_t a) {SetSP(GetSP()+1);  m_RAS[GetSP()] = a;}
  inline address_t POP() {address_t a = m_RAS[GetSP()];  SetSP(GetSP()-1);  return a;}

  // 2650 PSW primitives ...
private:
  // Set the PSL condition code to an explicit value ...
  inline void SetCC (uint8_t cc) {m_PSL = (m_PSL & ~PSL_CC) | (cc & PSL_CC);}
  // Update the CC bits based on the value (positive, negative or zero) ...
  inline void UpdateCC (uint8_t bVal)
    {SetCC( (bVal==0) ? CC_ZERO : ISSET(bVal, 0x80) ? CC_NEGATIVE : CC_POSITIVE );}
  // Compare the CC value (note that the CC bits are right justified first!) ...
  //   Note that CC == 3 always matches!!!
  inline bool CompareCC (uint2_t bCC)
    {return (bCC == 3)  ||  (bCC == ((m_PSL & PSL_CC) >> 6));}
  // Set or clear the IDC, OVF and/or CY flags ...
  inline void SetIDC (bool f) {if (f) SETBIT(m_PSL, PSL_IDC);  else CLRBIT(m_PSL, PSL_IDC);}
  inline void SetOVF (bool f) {if (f) SETBIT(m_PSL, PSL_OVF);  else CLRBIT(m_PSL, PSL_OVF);}
  inline void SetCY  (bool f) {if (f) SETBIT(m_PSL, PSL_CY);   else CLRBIT(m_PSL, PSL_CY);}

  // Internal 2650 CPU operations ...
private:
  // Addressing mode handlers ...
  address_t GetEA2R(bool fAddTime=true);
  address_t GetEA3B(bool fAddTime = true);
  address_t GetEA2ER();
  address_t GetEA3EB();
  bool GetEA3A (uint2_t bReg, address_t &wEA);
  // Load/Store, arithmetic, logical and comparison instructions ...
  void ADD (uint8_t &bDST, uint8_t bSRC);
  void SUB (uint8_t &bDST, uint8_t bSRC);
  void COM (uint8_t bSRC1, uint8_t bSRC2);
  void TMI (uint8_t bSRC, uint8_t bMask);
  // Other instructions ...
  void DAR (uint8_t &bDST);
  void RRR (uint8_t &bDST);
  void RRL (uint8_t &bDST);
  // Basic branch and subroutine call operations ...
  void BRANCH (address_t wEA, bool fBranch=true) {if (fBranch) m_IAR = wEA;}
  void CALL (address_t wEA, bool fCall=true) {if (fCall) {PUSH(m_IAR);  m_IAR = wEA;}}
  bool RETURN (bool fReturn=true) {if (fReturn) m_IAR = POP();  return fReturn;}
  // Get or set the PSU and handle side effects on the sense and flag pins ...
  //   Note that the PSL has no side effects, so it can be changed at will!
  uint8_t GetPSU ();
  void SetPSU (uint8_t bPSU, bool fForce=false);
  // Simulate input and output instructions ...
  uint8_t DoInput (uint8_t bPort);
  void DoOutput (uint8_t bData, uint8_t bPort);
  // Trace instruction execution ...
  void TraceInstruction() const;
  // Execute the opcode just fetched ...
  uint64_t DoExecute (uint8_t bOpcode);

  // 2650 internal registers and state ...
private:
  uint8_t   m_R[4];           // primary register set
  uint8_t   m_RP[3];          // alternate register set
  uint8_t   m_PSU, m_PSL;     // program status (upper and lower)
  address_t m_IAR;            // instruction address register (aka PC)
  address_t m_RAS[MAXSTACK];  // subroutine call/return stack
  // Constant strings for sense and flag names ...
  static const char *g_apszSenseNames[MAXSENSE];
  static const char *g_apszFlagNames[MAXFLAG];
};


