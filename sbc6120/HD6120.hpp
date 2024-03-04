//++
// HD6120.hpp -> HD6120 (PDP-8 compatible) CPU definitions
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
//   The HD6120 is a general purpose CMOS 12 bit microprocessor designed to
// recognize the instruction set of Digital Equipment Corporation’s PDP-8/E
// minicomputer.
//
// REVISION HISTORY:
// 31-JUL-22  RLA   Copied from C2650.
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
#include "Interrupt.hpp"        // CInterrupt functions
#include "CPU.hpp"              // CPU definitions
using std::string;              // ...

// Increment/decrement any 12 bit quantity..
#define INC12(w)  MASK12(w+1)
#define DEC12(w)  MASK12(w-1)


class C6120 : public CCPU
{
  //++
  // HD6120 CPU emulation ...
  //--

  // Magic numbers and constants ...
public:
  enum {
    CLOCK_FREQUENCY = 8000000UL,  // standard SBC6120 crystal is 8MHz
  };

  // Internal CPU registers ...
  //   These codes are passed to the GetRegister() and SetRegister() methods to
  // access internal CPU registers and state.
public:
  enum _REGISTERS {
    REG_AC      =  0,         // accumulator
    REG_PC      =  1,         // program counter
    REG_MQ      =  2,         // multiplier/quotient register
    REG_DF      =  3,         // current data field
    REG_IF      =  4,         // current instruction field
    REG_IB      =  5,         // instruction field buffer,
    REG_PS      =  6,         // processor flags, including link
    REG_IR      =  7,         // instruction being executed now
    REG_MA      =  8,         // current memory address
    REG_SR      =  9,         // switch register for OSR/WSR
    REG_SP1     = 10,         // 6120 stack pointer #1
    REG_SP2     = 11,         // 6120 stack pointer #2
    REG_FLAGS   = 12,         // HD6120 specific flag bits
    MAXREG      = 13,         // number of registers
  };
  // This table is used to translate a name to a REGISTER ...
  static const CCmdArgKeyword::KEYWORD g_keysRegisters[];

  //   The HD6120 supports two "startup modes" - start execution in main
  // memory, or start with an immediate panel trap.
public:
  enum _STARTUP_MODES {
    STARTUP_MAIN,           // start execution at 07777 in main memory
    STARTUP_PANEL,          // set PWRON and trap to panel memory
  };
  typedef enum _STARTUP_MODES STARTUP_MODE;

  // Bits in the flag register (GTF/RTF instructions) ...
public:
  //   These are the bits kept in the processor status (PS) register.  These
  // bits are accessible to the program via instructions like GTF/RTF.  Don't
  // confuse them with the 6120 internal flip-flops kept in FLAGS!
  enum _PSBITS {
    PS_LINK     = 04000,      // the actual one and only LINK bit
    PS_GT       = 02000,      // GT flag for EAE (not used on the 6120)
    PS_IRQ      = 01000,      // set if an interrupt is being requested
    PS_PWRON    = 00400,      // 6120 specific power on flag
    PS_IEFF     = 00200,      // set if interrupts are enabled
                              // (this bit is the KM8/E user mode flag)
    PS_SIF      = 00070,      // last IF before interrupt
    PS_SDF      = 00007,      //   "  DF   "      "    "
    PS_SF       = 00077,      // both of the above
  };
  //   And these bits are HD6120 specific internal flags and flip flops.  Some
  // of these, specifically bits 0-4, correspond to the bits returned by the
  // PRS instruction.  The rest are assigned arbitrarily and are masked out
  // for PRS.  It's convenient to have them all together in one place!
  enum _FLIP_FLOPS {
    FF_BTSTRP   = 04000,      // set by external CPREQ request
    FF_PNLTRP   = 02000,      // set by any of the PRx instructions
    FF_IRQ      = 01000,      // bit 2 is a copy of PS_IRQ
    FF_PWRON    = 00400,      // set by RESET, cleared by PRS or PEX
    FF_HLTFLG   = 00200,      // set by HLT instruction
    FF_PRSMASK  = 07600,      // mask for bits returned by PRS
    FF_CTRL     = 00100,      // one if control panel mode is active
    FF_PEXIT    = 00040,      // panel exit pending
    FF_IIFF     = 00020,      // inhibit interrupts after CIF
    FF_FFETCH   = 00010,      // force fetch (inhibit interrupts after ION)
    FF_FZ       = 00004,      // force field zero after CP entry
    FF_PDF      = 00002,      // panel data flag
  };

  // Constructors and destructor ...
public:
  C6120(CMemory *pMainMemory, CMemory *pPanelMemory, CEventQueue *pEvent,
          CInterrupt *pMainInterrupt, CInterrupt *pPanelInterrupt);
  virtual ~C6120() {};
private:
  // Disallow copy and assignments!
  C6120 (const C6120 &) = delete;
  C6120& operator= (C6120 const &) = delete;

  // HD6120 properties ...
public:
  // Get a constant string for the CPU name, type or options ...
  virtual const char *GetDescription() const override {return "12 Bit Microprocessor";}
  virtual const char *GetName() const override {return "HD6120";}
  // Get the CPU's simulated crystal frequency in MHz ...
  virtual uint32_t GetCrystalFrequency() const override {return CLOCK_FREQUENCY;}
  // Get the address of the next instruction to be executed ...
  virtual address_t GetPC() const override {return m_IF|m_PC;}
  virtual void SetPC (address_t a) override {m_IF = m_IB = (a & 070000);  m_PC = (a & 07777);}
  // Return the instruction at the PC (used for tracing) ...
  inline word_t GetCurrentInstruction() const {return ReadDirect(m_PC);}
  // Get or set the startup mode ...
  void SetStartupMode (STARTUP_MODE mode) {m_StartupMode = mode;}
  STARTUP_MODE GetStartupMode() const {return m_StartupMode;}
  // Determine whether the HLT opcode halts or traps ...
  void SetStopOnHalt (bool fHalt) {m_fHLThalts = fHalt;}
  bool IsStopOnHalt() const {return m_fHLThalts;}
  // Decode the PS and FLAGS words to a human friendly format ...
  string DecodeStatus (word_t ps=0) const;
  string DecodeFlags (word_t ff=0) const;
  //   These routines are used by the SBC6120 memory mapping hardware to
  // change the memory space referred to for panel mode direct and indirect
  // accesses.  In the SBC6120 main memory always references RAM and never
  // changes, so there's no need to mess with that.
  void SetPanelDirect (CMemory *pMemory)
    {m_pPanelDirect = pMemory;  UpdateMemoryPointers();}
  void SetPanelIndirect (CMemory *pMemory)
    {m_pPanelIndirect = pMemory;  UpdateMemoryPointers();}
  // TRUE if an interrupt is current requested ...
  inline bool IsIRQ() const {return (m_pMainInterrupt != NULL) && m_pMainInterrupt->IsRequested();}
  inline bool IsCPREQ() const {return (m_pPanelInterrupt != NULL) && m_pPanelInterrupt->IsRequested();}

  // HD6120 public methods ...
public:
  // Reset the CPU ...
  virtual void ClearCPU() override;
  // Simulate one or more PDP8 instructions ...
  virtual STOP_CODE Run (uint32_t nCount=0) override;
  // Read or write CPU registers ... 
  virtual const CCmdArgKeyword::KEYWORD *GetRegisterNames() const override {return g_keysRegisters;}
  virtual unsigned GetRegisterSize (cpureg_t r) const override;
  virtual uint16_t GetRegister (cpureg_t nReg) const override;
  virtual void SetRegister (cpureg_t nReg, uint16_t nVal) override;

  // HD6120 private properties ...
private:
  // Test, set or clear status bits ...
  inline bool ISPS  (word_t f) const {return ISSET(m_PS, f);}
  inline void SETPS (word_t f) {SETBIT(m_PS, f);}
  inline void CLRPS (word_t f) {CLRBIT(m_PS, f);}
  // Test, set or clear flag bits ...
  inline bool ISFF (word_t f) const {return ISSET(m_Flags, f);}
  inline void SETFF (word_t f) {SETBIT(m_Flags, f);}
  inline void CLRFF (word_t f) {CLRBIT(m_Flags, f);}
  // TRUE if we're in control panel mode right now ...
  inline bool IsPanel() const {return ISFF(FF_CTRL);}
  // Increment the simulated CPU run time ...
  void AddCycles (uint32_t lCycles) {AddTime(lCycles * HZTONS(CLOCK_FREQUENCY));}

  // HD6120 memory access primitives ...
private:
  // Update m_pMemoryDirect and m_pMemoryIndirect whenever anything changes ...
  void UpdateMemoryPointers();
  // Return either the current instruction field or zero, depending on the FZ flag ...
  inline bool IsFZ() const {return (m_Flags & (FF_FZ|FF_CTRL)) == (FF_FZ|FF_CTRL);}
  inline address_t IForZ() const {return IsFZ() ? 0 : m_IF;}
  // Read/Write memory using the instruction field and the direct memory space ...
  inline word_t ReadDirect (word_t ea) const {return m_pMemoryDirect->CPUread(IForZ() | ea);}
  inline word_t ReadDirect() const {return ReadDirect(m_MA);}
  inline void WriteDirect (word_t ea, word_t md) {m_pMemoryDirect->CPUwrite((IForZ() | ea), md);}
  inline void WriteDirect (word_t md) {WriteDirect(m_MA, md);}
  // Read/Write memory using the data field and the indirect memory space ...
  inline word_t ReadIndirect (word_t eq) const {return m_pMemoryIndirect->CPUread(m_DF | eq);}
  inline word_t ReadIndirect() const {return ReadIndirect(m_MA);}
  inline void WriteIndirect (word_t ea, word_t md) {m_pMemoryIndirect->CPUwrite((m_DF | ea), md);}
  inline void WriteIndirect (word_t md) {WriteIndirect(m_MA, md);}

  // Basic, non-memory, PDP-8 operations ...
private:
  // AC and Link manipulations ... 
  inline void CLA() {m_AC  = 0;}                          // clear the AC
  inline void CMA() {m_AC ^= 07777;}                      // complement the AC
  inline void STA() {m_AC  = 07777;}                      // set the AC (to all ones)
  inline void CLL() {m_PS &= ~PS_LINK;}                   // clear the link
  inline void CML() {m_PS ^=  PS_LINK;}                   // complement the link
  inline void STL() {m_PS |=  PS_LINK;}                   // set the link
  inline void SKP() {m_PC  = INC12(m_PC);}                // skip always!
  inline void MQL() {m_MQ = m_AC;  m_AC = 0;}             // load MQ and clear AC
  inline void MQA() {m_AC |= m_MQ;}                       // OR transfer MQ to AC
  inline void SWP() {word_t t=m_MQ; m_MQ=m_AC; m_AC=t;}   // swap AC and MQ
  // Basic AC and Link tests ...
  inline bool SMA() const {return  ISSET(m_AC, 04000);}   // test for minus AC
  inline bool SPA() const {return !ISSET(m_AC, 04000);}   //   "   "  positive AC
  inline bool SNA() const {return  m_AC != 0;}            //   "   "  non-zero AC
  inline bool SZA() const {return  m_AC == 0;}            //   "   "  zero AC
  inline bool SNL() const {return  ISPS(PS_LINK);}        //   "   "  non-zero link
  inline bool SZL() const {return !ISPS(PS_LINK);}        //   "   "  zero link
  // Increment the AC and complement the link in case of overflow... 
  inline void IAC() {if ((++m_AC & 010000) != 0)  {CML();  m_AC = MASK12(m_AC);}}

  // Rotate and swap operations ...
private:
  // Byte swap (for 6 bit bytes!) the left and right bytes in the AC... 
  inline void BSW() {m_AC = ((m_AC >> 6) & 077) | ((m_AC & 077) << 6);}
  // Rotate the AC and LINK one bit left or right ... 
  void RAL();
  void RAR();
  // Rotate the AC (but not the LINK!) three bits to the left ...
  void R3L();

  // HD6120 stack operations ...
private:
  //   Note that the stacks are ALWAYS in field zero, regardless of the DF ...
  inline void PUSH (word_t &SP, word_t w) {m_pMemoryDirect->CPUwrite(SP, w);  SP = DEC12(SP);}
  inline word_t POP (word_t &SP) {SP = INC12(SP);  return m_pMemoryDirect->CPUread(SP);}

  //   Calculate the effective address (EA) for memory reference instructions.
  // In every case, the result is always left in the MA register ...
private:
  // Zero page addressing ... 
  inline void ZeroPage()    {m_MA =  m_IR & 0177;}
  // Current page addressing ... 
  inline void CurrentPage() {m_MA = (m_IR & 0177) | (m_MA & 07600);}
  // Indirect (DEC calls this "deferred") memory addressing ...
  void Deferred();

  // MRI instructions ...
private:
  // Logical AND the AC with memory ... 
  inline void AND (word_t w) {m_AC &= w;}
  // Two's complement ADD AC and memory, and complement LINK if overflow ... 
  void TAD (word_t w);
  // Deposit and clear accumulator ... 
  inline void DCAdirect()   {WriteDirect  (m_AC);  m_AC = 0;}
  inline void DCAindirect() {WriteIndirect(m_AC);  m_AC = 0;}
  // Increment memory location and skip if zero ... 
  inline void ISZdirect();
  inline void ISZindirect();
  // Jump to the effective memory address ... 
  inline void JMP() {m_PC = MASK12(m_MA);}
  // Jump to subroutine at the effective memory address ... 
  inline void JMS();
  // Update the instruction field during a JMP or JMS instruction ... 
  void IBtoIF();

  // HD6120 internal I/O instructions ...
private:
  // Clear all flags and I/O devices ...
  void CAF();
  // Return current panel status bits ...
  word_t PRS();
  // Return current flags and fields (used for both GTF and GCF) ...
  word_t GTF (bool fGTF=true);
  // Restore flags (after an interrupt) ...
  void RTF (word_t wFlags);
  // Execute standard PDP-8 CPU internal IOTs (opcodes 600x) ...
  void Do600x();
  // Execute extended memory IOTs (opcodes 62xx) ...
  void Do62xx();
  // Execute HD6120 stack instructions (opcodes 62x5 and 62x7) ...
  void DoStack();
  // Execute HD6120 special instructions (opcodes 62x6) ...
  void Do62x6();
  // Execute all I/O transfer instructions ...
  void DoIOT();

  // HD6120 CPU operations ...
private:
  // Execute all memory reference instructions (AND, TAD, DCA, ISZ, JMP and JMS!) ...
  void DoMRI();
  // Execute all group 1, 2 or 3 operate instructions ...
  void DoGroup1();
  void DoGroup2();
  void DoGroup3();
  // Simulate main memory or control panel interrupts ...
  void MainInterrupt();
  void PanelInterrupt();
  // Fetch and execute the next instruction ...
  void FetchAndExecute();

  // PDP8 internal registers and state ...
private:
  word_t      m_AC;             // accumulator
  word_t      m_PC;             // program counter
  word_t      m_MQ;             // multiplier/quotient register
  word_t      m_IR;             // instruction being executed now
  word_t      m_PS;             // processor flags, including link
  address_t   m_DF;             // current data field
  address_t   m_IF;             //   "  "  instruction field
  address_t   m_IB;             // instruction field buffer
  address_t   m_MA;             // current memory address
  word_t      m_SR;             // switch register for OSR/WSR
  word_t      m_SP1;            // 6120 stack pointer #1
  word_t      m_SP2;            // 6120 stack pointer #2
  word_t      m_Flags;          // HD6120 specific flag bits
  bool        m_fHLThalts;      // TRUE if the HLT opcode should reall halt
  CInterrupt *m_pMainInterrupt; // sources for main memory interrupts
  CInterrupt *m_pPanelInterrupt;//   "  "   "  control panel  "   "
  CMemory    *m_pMainMemory;    // memory object for 6120/PDP8 main memory
  CMemory    *m_pPanelDirect;   // 6120 control panel memory direct access
  CMemory    *m_pPanelIndirect; //  "      "      "     "    indirect  "
  CMemory    *m_pMemoryDirect;  // memory used for direct addressing
  CMemory    *m_pMemoryIndirect;//   "     "    "  indirect "    "
  STARTUP_MODE m_StartupMode;   // startup mode selected (main or panel memory)
};


