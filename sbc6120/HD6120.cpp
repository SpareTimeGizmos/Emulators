//++
// HD6120.hpp -> Harris HD6120 PDP8 CPU emulation
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
// minicomputer.  Many architectural, functional and processing enhancements
// have been designed into the 6120 in comparison to its predecessor, the
// Intersil IM6100.  The HD6120 features include -
//
//      * A completely PDP-8/E compatible instruction set
//      * Built in KM8/E compatible memory management
//      * A separate control panel memory for a bootstrap/monitor
//      * Two onchip stack pointers
//
// MEMORY ARCHITECTURE
//   The HD6120 supports two independent 32K word memory spaces - main memory
// and control panel memory.  Main memory is the traditional PDP8 memory and
// holds OS/8, FOCAL, or whatever PDP8 program you intend to run.  Control
// panel memory holds the firmware for booting, debugging, implementing a soft
// front panel, etc.  Normally instructions are fetched from main memory,
// however the HD6120 can switch to fetching instructions from panel memory
// by way of the Panel Interrupt.
// 
//   Programs executing from main memory can access panel memory only via one
// of the panel trap instructions, which force a Panel Interrupt.  Code that
// executes from panel memory however, can access either panel memory or main
// memory depending on whether the operand is directly or indirectly addressed,
// and on the setting of the panel data flag but.  The SBC6120 adds yet another
// mapping layer on top of this, and panel memory can be mapped to EPROM, RAM
// or RAMdisk depending on the memory mode selected.  
// 
//   It'd be pretty slow if every memory reference had to go thru a complicated
// series of tests to figure out which memory space to access, so instead we
// just keep two pointers - m_pMemoryDirect and m_pMemoryIndirect.  Each is a
// pointer to a CMemory object, and the first is used for all direct memory
// accesses and the second for indirect access.  Then whenever the panel mode,
// panel data flag, or the memory mapping mode is changed we just need to
// change these pointers to they refer to the correct memory space.  The basic
// memory read and write functions don't need to worry about it.
// 
//   The SBC6120 specific memory mapping hardware is implemented as a layer on
// top of this module, and calls the SetPanelDirect() and SetPanelIndrect()
// methods to change the memory space accessed in panel mode.  In the SBC6120
// and the HD6120 main memory always refers to main memory and that is never
// affected by any mapping mode or panel data flags, and there's no need to
// change the CMemory object used for main memory.
//
// DEVICES
//   PDP-8, and HD6120, peripheral devices are unique among microprocessors in
// that the device decides what operation to perform.  There are no explicit
// input or output instructions and no memory mapped peripherals.  There is
// just a generic set of I/O opcodes that the CPU puts out there on the bus,
// and each peripheral can decode its own opcodes and then tell the CPU what
// it wants to do.  The range of actions include loading a device register
// from the AC, loading the AC from a device register, and incrementing the
// PC for a "skip on flag" operation.
// 
//   It's worth mentioning that the HD6120 came with a companion I/O controller
// chip, the HD6121.  The latter is an attempt at implementing a "generic"
// PDP-8 peripheral, however unfortunately it's not really compatible with any
// existing devices.  DEC used the 6121 in all the DECmate family machines, and
// it caused some comatibility headaches for users.  The SBC6120 does not use
// the 6121 and there's no support here for emulating it.
// 
// INTERRUPTS
//   The HD6120 actually supports two distinct interrupts - there's a standard
// PDP-8 style interrupt (called the main memory interrupt), and there's a
// control panel interrupt.  The latter causes an interrupt to the control
// panel memory space and the firmware in ROM, and is transparent to the main
// memory code.  In the SBC6120 the control panel interrupt is used only in
// response to a BREAK received on the console SLU as a way to get back into
// the firmware.  Think of it as being like the console ODT on an LSI-11.
//
// HD6120 - PDP8 INCOMPATIBILITIES
//   On the traditional PDP8s, the GTF instruction returns the current, actual,
// state of the IEFF, but the RTF instruction ignores this bit and always
// enables interrupts regardless.  On the HD6120 this is flipped around, and
// GTF always returns a 1 in the IEFF position regardless of the actual state
// of that flip flop, but RTF actually enables or disables interrupts based on
// the IEFF bit in the AC.  This difference causes some of the DEC diagnostics
// to fail.  Normally we implement the HD6120 interpretation, but if you define
// GTF_RETURNS_IEFF (see below) then this code gives the traditional behavior.
// 
// REVISION HISTORY:
// 31-JUL-22  RLA  New file.
//  6-NOV-24  RLA  Add set default CPU clock to constructor.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>             // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include "EMULIB.hpp"           // emulator library definitions
#include "LogFile.hpp"          // emulator library message logging facility
#include "CommandParser.hpp"    // needed for type KEYWORD
#include "Interrupt.hpp"        // interrupt simulation logic
#include "EventQueue.hpp"       // I/O device event queue simulation
#include "MemoryTypes.h"        // address_t and word_t data types
#include "Memory.hpp"           // CMemory memory emulation object
#include "HD6120opcodes.hpp"    // HD6120 opcode definitions
#include "CPU.hpp"              // CCPU base class definitions
#include "Device.hpp"           // CDevice I/O device emulation objects
#include "HD6120.hpp"           // declarations for this module

// Conditional compilation...
//#define GTF_RETURNS_IEFF        // traditional interpretation of GTF/RTF

// Internal CPU register names for GetRegisterNames() ...
const CCmdArgKeyword::KEYWORD C6120::g_keysRegisters[] = {
  {"AC",  C6120::REG_AC},  {"PC",  C6120::REG_PC},
  {"MQ",  C6120::REG_MQ},  {"DF",  C6120::REG_DF},
  {"IF",  C6120::REG_IF},  {"IB",  C6120::REG_IB},
  {"PS",  C6120::REG_PS},  {"IR",  C6120::REG_IR},
  {"MA",  C6120::REG_MA},  {"SR",  C6120::REG_SR},
  {"SP1", C6120::REG_SP1}, {"SP2", C6120::REG_SP2},
  {"FF",C6120::REG_FLAGS}, {NULL, 0}
};


////////////////////////////////////////////////////////////////////////////////
///////////////////////////// CPU CONTROL METHODS //////////////////////////////
////////////////////////////////////////////////////////////////////////////////

C6120::C6120(CMemory *pMainMemory, CMemory *pPanelMemory, CEventQueue *pEvent,
                 CInterrupt *pMainInterrupt, CInterrupt *pPanelInterrupt)
  : CCPU(pMainMemory, pEvent, pMainInterrupt)
{
  //++
  //--
  m_pMainMemory = pMainMemory;  m_pPanelDirect = m_pPanelIndirect = pPanelMemory;
  m_pMainInterrupt = pMainInterrupt;  m_pPanelInterrupt = pPanelInterrupt;
  m_StartupMode = STARTUP_PANEL;  m_fHLThalts = false;
  SetCrystalFrequency(DEFAULT_CLOCK);
  ClearCPU();
}

void C6120::ClearCPU()
{
  //++
  //   This simulates the HD6120 RESET input.  The AC, MQ, LINK, IF, IB, DF,
  // and SF registers are all cleared.  All PS buts are cleared, including
  // interrupt enable and interrupt inhibit.  The PC is set to 07777 (no, not
  // zero!).  All flags are cleared except for PWRON.  If the startup mode is
  // set to PANEL, then the PWRON flag will be set, which causes an immediate
  // trap to panel memory.  If the startup mode is normal, then PWRON is not
  // set and execution will begin from main memory.
  // 
  //   Note that on a real HD6120 chip RESET does not alter the SP1 and SP2
  // stack pointers.  We always clear them though, just for consistency.
  //--
  CCPU::ClearCPU();
  // Clear all CPU registers ...
  m_AC = m_PC = m_MQ = m_IR = m_MA = m_SR = m_SP1 = m_SP2 = 0;
  // Clear all memory fields ...
  m_DF = m_IF = m_IB = 0;
  // Clear all flags and status bits ...
  m_PS = m_Flags = 0;
  //   We always start off accessing main memory for everything.  If the PWRON
  // flag is set then we'll immediately trap to panel memory, and that'll
  // switch the memory pointers...
  m_pMemoryDirect = m_pMemoryIndirect = m_pMainMemory;
  // Set the PC to 7777 ...
  m_PC = 07777;
  // If the startup mode is PANEL, then set PWRON ...
  if (m_StartupMode == STARTUP_PANEL) SETFF(FF_PWRON);
}

unsigned C6120::GetRegisterSize (cpureg_t nReg) const
{
  //++
  //   This method returns the size of a given register, IN BITS!
  // It's used only by the UI, to figure out how to print and mask register
  // values...
  //--
  switch (nReg) {
    case REG_AC:  case REG_MQ:  case REG_PC:  case REG_PS:
    case REG_IR:  case REG_SR:  case REG_SP1: case REG_SP2: 
    case REG_MA:  case REG_FLAGS:
      return 12;
    case REG_IF:  case REG_DF:  case REG_IB:
      return 3;
    default:
      return 0;
  }
}

uint16_t C6120::GetRegister (cpureg_t nReg) const
{
  //++
  // This method will return the contents of an internal CPU register ...
  //--
  switch (nReg) {
    case REG_AC:    return m_AC;
    case REG_PC:    return m_PC;
    case REG_MQ:    return m_MQ;
    case REG_PS:    return m_PS;
    case REG_IF:    return m_IF >> 12;
    case REG_DF:    return m_DF >> 12;
    case REG_IB:    return m_IB >> 12;
    case REG_IR:    return m_IR;
    case REG_MA:    return m_MA;
    case REG_SR:    return m_SR;
    case REG_SP1:   return m_SP1;
    case REG_SP2:   return m_SP2;
    case REG_FLAGS: return m_Flags;
    default:        return 0;
  }
}

void C6120::SetRegister (cpureg_t nReg, uint16_t wData)
{
  //++
  //   This method will set the contents of an internal CPU register.  Note
  // that some registers (e.g. the flags) cannot be set, and other registers
  // (e.g. the PS) only allow certain bits to be modified.
  //--
  switch (nReg) {
    case REG_AC:  m_AC  = MASK12(wData);                        break;
    case REG_PC:  m_PC  = MASK12(wData);                        break;
    case REG_MQ:  m_MQ  = MASK12(wData);                        break;
    case REG_IF:  m_IF  = MASK3(wData) << 12;  m_IB = m_IF;     break;
    case REG_DF:  m_DF  = MASK3(wData) << 12;                   break;
    case REG_SR:  m_SR  = MASK12(wData);                        break;
    case REG_SP1: m_SP1 = MASK12(wData);                        break;
    case REG_SP2: m_SP2 = MASK12(wData);                        break;
    case REG_PS:  m_PS = (wData & (PS_LINK|PS_GT|PS_SIF|PS_SDF))
                       | (m_PS & (PS_IRQ|PS_PWRON|PS_IEFF));    break;
    case REG_IR:
    case REG_MA:
    case REG_FLAGS: /* can't be modified! */                    break;
    default: break;
  }
}

string C6120::DecodeStatus (word_t ps) const
{
  //++
  //   Decode the processor status bits into a human friendly string.  This
  // is used for debugging purposes.  If the PS value passed is zero, then
  // use the current PS register contents.
  //--
  string s;
  if (ps == 0) ps = m_PS;
  if (ISSET(ps, PS_LINK )) s += "LINK ";
  if (ISSET(ps, PS_GT   )) s += "GT ";
  if (ISSET(ps, PS_IRQ  )) s += "IRQ ";
  if (ISSET(ps, PS_PWRON)) s += "PWRON ";
  if (ISSET(ps, PS_IEFF )) s += "IEFF ";
  s += FormatString("SIF=%d ", (ps & PS_SIF) >> 3);
  s += FormatString("SDF=%d ", (ps & PS_SDF));
  return s;
}

string C6120::DecodeFlags (word_t ff) const
{
  //++
  //   Decode the HD6120 flag flip flops into a human readable string for
  // debugging purposes.  If the flags passed are zero, then use the current
  // flag register instead.
  //--
  string s;
  if (ff == 0) ff = m_Flags;
  if (ISSET(ff, FF_BTSTRP)) s += "BTSTRP ";
  if (ISSET(ff, FF_PNLTRP)) s += "PNLTRP ";
  if (ISSET(ff, FF_IRQ   )) s += "IRQ ";
  if (ISSET(ff, FF_PWRON )) s += "PWRON ";
  if (ISSET(ff, FF_HLTFLG)) s += "HLTFLG ";
  if (ISSET(ff, FF_CTRL  )) s += "CTRLFF ";
  if (ISSET(ff, FF_PEXIT )) s += "PEXIT ";
  if (ISSET(ff, FF_IIFF  )) s += "IIFF ";
  if (ISSET(ff, FF_FFETCH)) s += "FFETCH ";
  if (ISSET(ff, FF_FZ    )) s += "FZ ";
  if (ISSET(ff, FF_PDF   )) s += "PDF ";
  return s;
}

void C6120::UpdateMemoryPointers()
{
  //++
  //   This routine should be called whenever the panel mode (FF_CTRL), the
  // panel data (FF_PDF), or the control panel pointers (m_pPanelDirect and
  // m_pPanelIndirect) are changed.  It will figure out exactly which memory
  // space the m_pMemoryDirect and m_pMemoryIndirect pointers should refer
  // to, and update them accordingly.
  //--
  if (!ISFF(FF_CTRL)) {
    // Not in panel mode, so always point to main memory!
    m_pMemoryDirect = m_pMemoryIndirect = m_pMainMemory;
  } else {
    //   In control panel mode, direct accesses always to to panel memory,
    // and indirect accesses go to either main or panel memory depending
    // on the panel data flag ...
    m_pMemoryDirect = m_pPanelDirect;
    m_pMemoryIndirect = ISFF(FF_PDF) ? m_pPanelIndirect : m_pMainMemory;
  }
}



////////////////////////////////////////////////////////////////////////////////
/////////////////////////// BASIC PDP8 INSTRUCTIONS ////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void C6120::RAL()
{
  //++
  // Rotate the AC and LINK one bit to the left ...
  //--
  m_AC = (m_AC << 1) | (ISPS(PS_LINK) ? 1 : 0);
  if (m_AC & 010000)  STL();  else CLL();
  m_AC = MASK12(m_AC);
}

void C6120::RAR()
{
  //++
  // Rotate the AC and LINK one bit to the right ... 
  //--
  if (ISPS(PS_LINK)) m_AC |= 010000;
  if (m_AC & 1)  STL();  else CLL();
  m_AC = MASK12(m_AC >> 1);
}

void C6120::R3L()
{
  //++
  // Rotate the AC (but not the LINK!) left three bits ...
  //--
  m_AC = m_AC << 3;
  m_AC |= (m_AC >> 12) & 7;
  m_AC = MASK12(m_AC);
}

void C6120::Deferred()
{
  //++
  // Indirect (DEC calls this "deferred") memory addressing ...
  //   This has the additional complication of auto index registers!
  // 
  //   Note that the extra time required for indirect addressing has alreaedy
  // been accounted for in the DoMRI() method, however auto-indexing requires
  // two extra clocks, which have to be added here.
  //--
  if ((m_MA & 07770) == 00010) {
    word_t t = INC12(ReadDirect());  WriteDirect(t);
    m_MA = t;  AddCycles(2);
  } else {
    m_MA = ReadDirect();
  }
}

void C6120::TAD (word_t w)
{
  //++
  // Two's complement ADD AC and memory, and complement LINK if overflow ... 
  //--
  m_AC += w;
  if ((m_AC & 010000) != 0) CML();
  m_AC = MASK12(m_AC);
}

void C6120::ISZdirect()
{
  //++
  // Increment memory (directly addressed) and skip if zero ...
  //--
  word_t t = INC12(ReadDirect());
  WriteDirect(t);
  if (t == 0)  SKP();
}

void C6120::ISZindirect()
{
  //++
  // Increment memory (indirectly addressed) and skip if zero ...
  //--
  word_t t = INC12(ReadIndirect());
  WriteIndirect(t);
  if (t == 0)  SKP();
}

void C6120::JMS()
{
  //++
  // Jump to subroutine (store PC at MEM(EA) and jump to EA+1) ...
  //--
  WriteDirect(m_PC);
  m_PC = INC12(m_MA);
}

void C6120::DoMRI()
{
  //++
  //   This routine decodes and executes all memory reference instructions.
  // There are six different MRI opcodes - AND, TAD,  DCA, ISZ, JMP and JMS -
  // and each of these has four addressing modes - page zero direct, current
  // page direct, page zero indirect and current page indirect.  We simply
  // spell out all 24 possible combinations and handle each one individually...                                  
  //--
  switch (m_IR & 07600) {
  
    // AND - bitwise logical AND of AC and memory... 
    case 00000:  ZeroPage();                AND(ReadDirect());   AddCycles( 7); break;
    case 00200:  CurrentPage();             AND(ReadDirect());   AddCycles( 7); break;
    case 00400:  ZeroPage();    Deferred(); AND(ReadIndirect()); AddCycles(10); break;
    case 00600:  CurrentPage(); Deferred(); AND(ReadIndirect()); AddCycles(10); break;

    // TAD - two's complement ADD memory to AC... 
    case 01000:  ZeroPage();                TAD(ReadDirect());   AddCycles( 7); break;
    case 01200:  CurrentPage();             TAD(ReadDirect());   AddCycles( 7); break;
    case 01400:  ZeroPage();    Deferred(); TAD(ReadIndirect()); AddCycles(10); break;
    case 01600:  CurrentPage(); Deferred(); TAD(ReadIndirect()); AddCycles(10); break;

    // ISZ - increment memory and skip if zero... 
    case 02000:  ZeroPage();                ISZdirect();         AddCycles( 9); break;
    case 02200:  CurrentPage();             ISZdirect();         AddCycles( 9); break;
    case 02400:  ZeroPage();    Deferred(); ISZindirect();       AddCycles(12); break;
    case 02600:  CurrentPage(); Deferred(); ISZindirect();       AddCycles(12); break;

    // DCA - deposit AC in memory and clear AC... 
    case 03000:  ZeroPage();                DCAdirect();         AddCycles( 7); break;
    case 03200:  CurrentPage();             DCAdirect();         AddCycles( 7); break;
    case 03400:  ZeroPage();    Deferred(); DCAindirect();       AddCycles(10); break;
    case 03600:  CurrentPage(); Deferred(); DCAindirect();       AddCycles(10); break;

    // JMS - jump and store PC (i.e. jump to subroutine)... 
    case 04000:  ZeroPage();                IBtoIF(); JMS();     AddCycles( 7); break;
    case 04200:  CurrentPage();             IBtoIF(); JMS();     AddCycles( 7); break;
    case 04400:  ZeroPage();    Deferred(); IBtoIF(); JMS();     AddCycles(10); break;
    case 04600:  CurrentPage(); Deferred(); IBtoIF(); JMS();     AddCycles(10); break;

    // JMP - simple jump... 
    case 05000:  ZeroPage();                IBtoIF(); JMP();     AddCycles( 4); break;
    case 05200:  CurrentPage();             IBtoIF(); JMP();     AddCycles( 4); break;
    case 05400:  ZeroPage();    Deferred(); IBtoIF(); JMP();     AddCycles( 7); break;
    case 05600:  CurrentPage(); Deferred(); IBtoIF(); JMP();     AddCycles( 7); break;

  }
}

void C6120::DoGroup1()
{
  //++
  //   This routine executes Group 1 PDP-8 microinstructions.  Group 1    
  // microinstructions have eight function bits and are handled in three  
  // separate cases. The first case handles bits 4-7 (CLA, CLL, CMA, CML) 
  // which happen at times 1 and 2.  The second case handles bit 11 (IAC) 
  // alone, which happens at time 3.  The final case handles the rotate   
  // bits, 8-10, which happen at time 4...                                
  //
  //   All group 1 microinstructions take 6 clock cycles to complete, EXCEPT
  // those that include a rotate (or byte swap) and those take 8 cycles.
  //--

  // Parts 1 and 2 - CLA, CLL, CMA, and CML... 
  // (Note that CLA CMA is STA and CLL CML is STL!) 
  switch (m_IR & 0360) {
    case 00000:                  break;
    case 00020:  CML();          break;     // ... ... ... CML 
    case 00040:  CMA();          break;     // ... ... CMA ... 
    case 00060:  CMA();  CML();  break;     // ... ... CMA CML 
    case 00100:  CLL();          break;     // ... CLL ... ... 
    case 00120:  STL();          break;     // ... CLL ... CML 
    case 00140:  CLL();  CMA();  break;     // ... CLL CMA ... 
    case 00160:  STL();  CMA();  break;     // ... CLL CMA CML 
    case 00200:  CLA();          break;     // CLA ... ... ... 
    case 00220:  CLA();  CML();  break;     // CLA ... ... CML 
    case 00240:  STA();          break;     // CLA ... CMA ... 
    case 00260:  STA();  CML();  break;     // CLA ... CMA CML 
    case 00300:  CLA();  CLL();  break;     // CLA CLL ... ... 
    case 00320:  CLA();  STL();  break;     // CLA CLL ... CML 
    case 00340:  STA();  CLL();  break;     // CLA CLL CMA ... 
    case 00360:  STA();  STL();  break;     // CLA CLL CMA CML 
  }

  // Part 3 - IAC... 
  if (ISSET(m_IR, 0001)) IAC();

  // Part 4 - RAR, RAL, RTR, and RTL ... 
  switch (m_IR & 0016) {
    case 0000:  /* NOP */       break;
    case 0002:  BSW();          break;
    case 0004:  RAL();          break;
    case 0006:  RAL();  RAL();  break;
    case 0010:  RAR();          break;
    case 0012:  RAR();  RAR();  break;
    case 0014:  R3L();          break;
    case 0016: /* undefined! */ break;
  }

  // Add in the required time ...
  if ((m_IR & 0016) != 0) AddCycles(2);
  AddCycles(6);

}

void C6120::DoGroup2()
{
  //++
  //   This routine interprets PDP-8 group 2 microinstructions.  These    
  // are all test and skip on condition instructions with the exception   
  // of HLT and OSR (which, I suppose, the PDP-8 designers could find no  
  // better place for!).  The skip instructions all happen at time 1, and 
  // then the CLA, OSR and HLT instructions happen in that order at times 
  // 2, 3 and 4...                                                        
  // 
  //   All group 2 instructions take 7 clocks, except for those including
  // OSR, which takes 8.
  //--

  // Time 1 - SMA/SPA, SZA/SNA, and SNL/SZL... 
  switch (m_IR & 0170) {
    case 0000:  /* skip never  */                     break;
    case 0010:  /* skip always */             SKP();  break;
    case 0020:  if (                  SNL())  SKP();  break;
    case 0030:  if (                  SZL())  SKP();  break;
    case 0040:  if (         SZA()         )  SKP();  break;
    case 0050:  if (         SNA()         )  SKP();  break;
    case 0060:  if (         SZA() || SNL())  SKP();  break;
    case 0070:  if (         SNA() && SZL())  SKP();  break;
    case 0100:  if (SMA()                  )  SKP();  break;
    case 0110:  if (SPA()                  )  SKP();  break;
    case 0120:  if (SMA()          || SNL())  SKP();  break;
    case 0130:  if (SPA()          && SZL())  SKP();  break;
    case 0140:  if (SMA() || SZA()         )  SKP();  break;
    case 0150:  if (SPA() && SNA()         )  SKP();  break;
    case 0160:  if (SMA() || SZA() || SNL())  SKP();  break;
    case 0170:  if (SPA() && SNA() && SZL())  SKP();  break;
  }

  // Time 2 - CLA ...
  if (ISSET(m_IR, 0200)) CLA();

  // Time 3 - OSR ...
  if (ISSET(m_IR, 0004)) {
    m_AC |= m_SR;  AddCycles(1);
  }

  //   Note that the HLT instruction just sets the HALT FLAG on the 6120.
  // This never actually halts anything, but will eventually trap to panel
  // memory when the time is right.  When executed from panel memory this
  // opcode does the exact same thing, but the trap to panel memory doesn't
  // happen until after we exit and fetch the next main memory opcode.
  //
  //   Also note that we have an option to allow the HLT opcode to actually
  // halt and return control to the UI.  This is not a HD6120 option at all,
  // but it's very handy for running the DEC diagnostics.
  if (ISSET(m_IR, 0002)) {
    if (IsStopOnHalt())
      m_nStopCode = STOP_HALT;
    else
      SETFF(FF_HLTFLG);
  }

  // All these take 7 clocks (except OSR, which takes one more) ...
  AddCycles(7);
}

void C6120::DoGroup3()
{
  //++
  //   This function emulates the PDP-8 group 3 microinstructions.  These 
  // all control the extended arithmetic element (aka EAE), which the HD6120
  // doesn't have.  The 6120 does, however, have an MQ register (as does the
  // plain PDP8/E without the EAE) and still implements the specific EAE   
  // instructions to load and store the MQ.
  // 
  //   All group 3 instructions take 6 clock cycles ...
  //--

  // Time 1 - CLA... 
  if (m_IR & 0200) CLA();

  // Time 2 - MQA and MQL... 
  switch (m_IR & 0120) {
    case 0000:          break;  // nop     
    case 0020:  MQL();  break;  // MQL     
    case 0100:  MQA();  break;  // MQA     
    case 0120:  SWP();  break;  // MQA MQL 
  }

  // All other EAE instructions are unimplemented!   
  if ((m_IR & 0056) != 0) IllegalOpcode();
  AddCycles(6);
}



////////////////////////////////////////////////////////////////////////////////
/////////////////////////// INTERNAL IOT INSTRUCTIONS //////////////////////////
////////////////////////////////////////////////////////////////////////////////

void C6120::CAF()
{
  //++
  //   The Clear All Flags (CAF) instruction clears the AC, LINK and GT bits,
  // and also the interrupt enable.  Most importantly though, it clears all
  // I/O devices!
  //--
  m_AC = 0;
  CLRPS(PS_LINK|PS_GT|PS_IEFF);
  ClearAllDevices();
}

word_t C6120::PRS()
{
  //++
  //   The 6120 PRS instruction returns the current panel flags - BTSTRP,
  // PNLTRP, HLTFLG and PWRON as well as the current interrupt request
  // status.  It also clears these panel request flags, EXCEPT for HLTFLG.
  // You have to use PGO to clear that one.
  // 
  //    Note that we've carefully assigned the bits in m_Flags so that
  // they correspond exactly to the four bits we need for PRS.  The other
  // bits we use in m_Flags have to be cleared out, however, and we have
  // to add the IRQ status.
  //--
  word_t s = m_Flags & (FF_BTSTRP|FF_PNLTRP|FF_HLTFLG|FF_PWRON);
  if (IsIRQ()) SETBIT(s, FF_IRQ);
  CLRFF(FF_BTSTRP|FF_PNLTRP|FF_PWRON);
  return s;
}

word_t C6120::GTF (bool fGTF)
{
  //++
  //   This routine implements both the GTF ("get flags", 6004) and the GCF
  // ("get current fields", 6256) instructions.  GTF is a standard PDP-8 opcode
  // and GCF is unique to the HD6120.  They're almost, but not quite, identical
  // the only difference being in bit 4.
  // 
  //   Both instruction returns the current state of the LINK, GT, and save
  // field flags.  They also return the current IRQ status and, oddly, on the
  // 6120 it returns the PWRON flag in bit 3.  AFAIK this bit is unused on a
  // real PDP-8.  Bit 5, which is user mode on a real PDP-8, is always zero.
  // 
  //   However for bit 4 the GCF instruction returns the current state of the
  // interrupt enable F-F (this is the one controlled by the ION/IOF opcodes),
  // but GTF always returns 1 in this bit.  This is important because GTF is
  // normally used in an interrupt handler, and the interrupt enable is always
  // cleared on entry into the ISR.  If GTF returned the actual state of the
  // interrupt enable here, the subsequent RTF would leave interrupts turned
  // off!  By always returning 1, GTF saves the ISR the trouble of ORing this
  // bit with 1 to ensure that a RTF will turn the interrupt system back on.
  // 
  //   GCF, on the other hand, allows the program to read the actual state of
  // the interrupt enable.  That's the only difference.
  //--
  word_t s = m_PS & (PS_LINK|PS_GT|PS_SIF|PS_SDF|PS_PWRON|PS_IEFF);
#ifndef GTF_RETURNS_IEFF
  if (fGTF) SETBIT(s, PS_IEFF);
#endif
  if (IsIRQ()) SETBIT(s, PS_IRQ);
  return s;
}

void C6120::RTF (word_t wFlags)
{
  //++
  //   RTF restores the LINK, GT and IEFF flags from the AC, as well as the
  // IF and DF.  As with CIF, the IF is not set directly but rather thru
  // changing the IB, which will update IF after the next JMP or JMS.  
  //--
  CLRPS(PS_LINK|PS_GT|PS_IEFF);
  if (ISSET(wFlags, PS_LINK))  SETPS(PS_LINK);
  if (ISSET(wFlags, PS_GT))    SETPS(PS_GT);
#ifndef GTF_RETURNS_IEFF
  if (ISSET(wFlags, PS_IEFF)) {
#endif
    SETPS(PS_IEFF);  SETFF(FF_IIFF);
#ifndef GTF_RETURNS_IEFF
  }
#endif
  m_IB = (wFlags & PS_SIF) <<  9;
  m_DF = (wFlags & PS_SDF) << 12;
}

void C6120::Do600x()
{
  //++
  //   This routine is called for all 600x opcodes.  These do things like
  // enable or disable interrupts, read status, save or restore the IF and
  // DF during interrupts, etc.  The tricky bit is that on the 6120 some of
  // these instructions behave differently, sometimes quite differently,
  // depending on whether they're being executed from main memory or panel
  // memory.  On the other hand, some are completely the same.  Gotta pay
  // attention!
  //--
  switch (m_IR) {
    case OP_SKON:
      //   If executed from main memory, this is SKON - skip if interrupts are
      // enabled and then turn then off.  However in panel memory this is PRS -
      // read panel status flags!  it's also worth noting that SKON takes 7
      // clock cycles where as PRS takes 8!  Go figure...
      if (IsPanel()) {
        m_AC = PRS();  AddCycles(8);
      } else {
        if (ISPS(PS_IEFF)) SKP();
        CLRPS(PS_IEFF);  AddCycles(7);
      }
      break;

    case OP_ION:
      //   ION enables interrupts, but not until after the next instruction has
      // been fetched.  This is handled with the FFETCH flag, which inhibits
      // interrupts for exactly one instruction and then is cleared by Run().
      // AFAIK, ION works the same way in panel memory, but panel mode will
      // inhibit any main memory interrupts until we exit panel mode.
      SETPS(PS_IEFF);  SETFF(FF_FFETCH);  AddCycles(6);
      break;

    case OP_IOF:
      // IOF always disables interrupts.  No funny stuff here!
      CLRPS(PS_IEFF);  AddCycles(6);
      break;

    case OP_SRQ:
      //   When executed from main memory, SRQ skips if an interrupt is 
      // requested. On the 6120 this actually tests the state of the INTREQ
      // pin and is independent of any of the internal enable or inhibit flip
      // flops.  When executed from panel memory, this is PGO, which simply
      // clears the HLTFLG.  AFAIK PGO has no other side effects.
      if (IsPanel()) {
        CLRFF(FF_HLTFLG);  AddCycles(6);
      } else {
        if (IsIRQ()) SKP();
        AddCycles(7);
      }
      break;

    case OP_GTF:
      //   When executed from main memory, GTF returns the current PS bits,
      // with a few odd exceptions.  From panel memory, this is the PEX
      // instruction which will force an exit from panel memory after the
      // next JMP or JMS instruction.  As a side effect, PEX also clears
      // the PNLTRP and PWRON flags.
      if (IsPanel()) {
        SETFF(FF_PEXIT);  CLRFF(FF_PNLTRP);  CLRPS(PS_PWRON);  AddCycles(6);
      } else {
        m_AC = GTF();  AddCycles(9);
      }
      break;

    case OP_RTF:
      //   RTF restores the LINK, GT and IEFF flags from the AC, as well as the
      // IF and DF.  AFAIK it works the same when executed from panel memory. 
      RTF(m_AC);  m_AC = 0;  AddCycles(8);
      break;

    case OP_SGT:
      // SGT skips if the GT flag is set.  That's all!
      if (ISPS(PS_GT)) SKP();
      AddCycles(7);
      break;

    case OP_CAF:
      //  CAF clears the AC and LINK as well as all I/O devices.  It works the
      // same from panel memory, and has no effect on any of the panel flags.
      CAF();  AddCycles(7);
      break;

    default:
      // Anything else is illegal!
      UnimplementedIO();  AddCycles(9);
      break;
  }
}

void C6120::DoStack()
{
  //++
  //   The Harris HD6120 had, of all things to find on a PDP-8, a stack.  And
  // better yet, it had not just one stack but _two_ separate and independent
  // stacks!  Unfortunately it wasn't all that useful and I don't think it was
  // ever really used, but we have to emulated it anyway.
  //
  //   NOTE: The 6120 has a "top down" stack (i.e. PUSHing decrements the stack
  // pointer and POPing increments it) which is decremented _after_ a PUSH
  // operation and _before_ a POP.  Also, the 6120 stacks are _always_ in field
  // zero, regardless of what the IF or DF may be...
  //--
  switch (m_IR) {
    // Push the AC onto the stack.  The AC is unchanged.
    case OP_PAC1:  PUSH(m_SP1, m_AC);  AddCycles(9);  break;
    case OP_PAC2:  PUSH(m_SP2, m_AC);  AddCycles(9);  break;

    // Pop the AC from the stack...
    case OP_POP1:  m_AC = POP(m_SP1);  AddCycles(9);  break;
    case OP_POP2:  m_AC = POP(m_SP2);  AddCycles(9);  break;
  
    //   The closest thing to a PUSHJ is the "PUSH PC" instruction, which
    // actually pushes the current PC (which has already been incremented
    // after fetching the PPC opcode) plus one.  The presumption is that
    // the PPC instruction will be folowed by a JMP to the actual subroutine,
    // and therefore the RTN instruction will return to the location after
    // the JMP.  It works, but it's crude.  In any case it's not our problem -
    // we just do what the 6120 does...
    case OP_PPC1:  PUSH(m_SP1, INC12(m_PC));  AddCycles(9);  break;
    case OP_PPC2:  PUSH(m_SP2, INC12(m_PC));  AddCycles(9);  break;

    //   Return (i.e. POPJ) is easier - it just pops the top of the stack and
    // puts it in the PC.  Note that the 6120 treats this just like a JMP or
    // JMS for the purposes of IB to IF transfers (and CP exits too!) ...
    //
    //   A subtle but critical point is that we must pop the return address
    // from the stack BEFORE we call IBtoIF().  We temporarily store that
    // address in MA and then jump to MA AFTER calling IBtoIF().  This ensures
    // that if an RTN instruction is used to exit panel mode, then the return
    // address will be fetched from the stack in panel memory!
    case OP_RTN1:  m_MA = POP(m_SP1);  IBtoIF();  JMP();  AddCycles(9);  break;
    case OP_RTN2:  m_MA = POP(m_SP2);  IBtoIF();  JMP();  AddCycles(9);  break;

    // Load the AC with the stack pointer...
    case OP_RSP1:  m_AC = m_SP1;  AddCycles(5);  break;
    case OP_RSP2:  m_AC = m_SP2;  AddCycles(5);  break;

    // Load the stack pointer from the AC and clear the AC...
    case OP_LSP1:  m_SP1 = m_AC;  m_AC = 0;  AddCycles(5);  break;
    case OP_LSP2:  m_SP1 = m_AC;  m_AC = 0;  AddCycles(5);  break;

    // everything else is a dud! 
    default: UnimplementedIO();  AddCycles(9);  break;
  }
}

void C6120::Do62x6()
{
  //++
  //   This method is called for all 62x6 instructions.  On the 6120 these are
  // all special instructions - control panel trap, write switch register, set
  // or clear the panel data flag, etc.  Note that some of these opcodes are
  // legal only in panel mode (e.g. SPD and CPD) and others are legal only when
  // executed from main memory (e.g. PR0..PR3).  Others, like WSR, can be used
  // anywhere...
  //--
  switch (m_IR) {
    case OP_PR0:
    case OP_PR1:
    case OP_PR2:
    case OP_PR3:
      //   Trap to control panel mode.  These opcodes are defined to be a NOP
      // if executed from panel mode!
      if (!IsPanel()) SETFF(FF_PNLTRP);
      AddCycles(6);
      break;

    case OP_WSR:
      //   Write to switch register.  This is actually external to the HD6120
      // chip and could be any kind of device, but we model it as a real
      // switch register (that can be read back by OSR).
      m_SR = m_AC;  m_AC = 0;  AddCycles(7);
      break;

    case OP_GCF:
      // Get current flags (similar to, but not the same as, GTF!) ...
      m_AC = GTF(false);  AddCycles(9);
      break;

    case OP_CPD:
      //  Clear panel data flag.  This is a panel memory only opcode and the
      // spec doesn't really say what it does if executed from main memory.
      if (IsPanel()) {
        CLRFF(FF_PDF);  UpdateMemoryPointers();
      } else
        UnimplementedIO();
      AddCycles(5);
      break;

    case OP_SPD:
      // Set panel data flag ...
      if (IsPanel()) {
        SETFF(FF_PDF);  UpdateMemoryPointers();
      } else
        UnimplementedIO();
      AddCycles(5);
      break;

    default:
      // Anything else is illegal!
      UnimplementedIO();  AddCycles(9);
      break;
  }
}

void C6120::Do62xx()
{
  //++
  //   This routine is called for all 62xx opcodes.  Most of these are standard
  // PDP-8 extended memory addressing instructions.  Although extended memory
  // is an option on many older PDP-8 models, it's built into the CPU on the
  // HD6120 chip.  The 62n1, 62n2 and 62n3 instructions change the data field,
  // instruction field or both to value of n in the opcode, and the 62n4 opcodes
  // are special extended memory functions selected by of n.  And lastly, the
  // HD6120 has a number of unique instructions including stacks (!!) and
  // special control panel functions, that are assigned to 62nx opcodes, where
  // x is 5, 6 or 7.
  //--
  switch (m_IR & 7) {
    case 1:   // CDF - change data field
    case 2:   // CIF - change instruction field
    case 3:   // CXF (aka CIF CDF) - channge both
      if (ISSET(m_IR, 1))  m_DF = (m_IR & 0070) << 9;
      if (ISSET(m_IR, 2)) {m_IB = (m_IR & 0070) << 9;  SETFF(FF_IIFF);}
      AddCycles(6);
      break;
  
    case 4:
      // "special" EMA functions ...
      switch (m_IR) {
        case OP_RDF:
          // read data field ...
          m_AC |= m_DF >> 9;  AddCycles(6);  break;
        case OP_RIF:
          // read instruction field ...
          m_AC |= m_IF >> 9;  AddCycles(6);  break;
        case OP_RIB:
          // read save field ...
          m_AC |= m_PS & PS_SF;  AddCycles(9);  break;
        case OP_RMF:
          // restore fields (after interrupt) ...
          m_DF = (m_PS & PS_SDF) << 12;  m_IB = (m_PS & PS_SIF) << 9;
          SETFF(FF_IIFF);  AddCycles(6);  break;
        default:
          // all others are illegal!
          UnimplementedIO();  AddCycles(6);  break;
      }
      break;

    case 5:
    case 7:
      // HD6120 stack instructions ...
      DoStack();
      break;

    case 6:
      // HD6120 control panel and other unique opcodes ...
      Do62x6();
      break;

    default:
      // All that remains are the 62x0 IOTs, which are unimplemented!
      UnimplementedIO();  AddCycles(6);
      break;
  }
}

void C6120::DoIOT()
{
  //++
  //--
  word_t wDevice = (m_IR & 0770) >> 3;
  if (wDevice == 0)
    Do600x();
  else if ((wDevice & 070) == 020)
    Do62xx();
  else {
    //   Find the CDevice object that corresponds to this IOT.  The CCPU base
    // class keeps separate device maps for input devices and output devices
    // (for CPUs with explicit INP and OUT instructions, for example) but that
    // doesn't really apply to the PDP8.  We assume all PDP8 devices are marked
    // as INOUT and thus appear on both maps, but just to be safe we'll search
    // them both.
    CDevice *pDevice = FindInputDevice(wDevice);
    if (pDevice == NULL) pDevice = FindOutputDevice(wDevice);
    if (pDevice != NULL) {
      if (!pDevice->DevIOT(m_IR, m_AC, m_PC)) UnimplementedIO();
    } else
      UnimplementedIO();
    AddCycles(6);
  }
}



////////////////////////////////////////////////////////////////////////////////
////////////////////////////////// INTERRUPTS //////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void C6120::IBtoIF()
{
  //++
  //   This routine transfers the instruction field buffer (IB) to the current
  // instruction field (IF) during the execution of a JMP, JMS, RTN1 or RTN2
  // opcode.  This also clears the EMA interrupt inhibit flip flop.
  // 
  //   When executed from panel mode however, this has a couple of additional
  // side effects.  In the interrupt inhibit is currently set, implying that a
  // CIF instruction has been executed recently, then it will clear the force
  // field zero (FZ) flag.
  // 
  //   Further, if an exit from panel memory is pending (as set by the PEX
  // instruction) then this routine clears the CTRLFF and the next instruction
  // will be fetched from main memory.  This is how the 6120 gets out of panel
  // mode.  Note when we exit from panel mode the FFETCH (force fetch) flag is
  // also set - this guarantees that the next instruction will be fetched and
  // executed before interrupts are recognized again.  This necessary so we
  // can exit from panel mode with the HLTFLG already set and have a single
  // instruction executed before we trap back to panel mode.
  //--
  if (IsPanel()) {
    if (ISFF(FF_IIFF)) CLRFF(FF_FZ);
    if (ISFF(FF_PEXIT)) {
      CLRFF(FF_CTRL|FF_PEXIT);  SETFF(FF_FFETCH);
      UpdateMemoryPointers();
    }
  }
  m_IF = m_IB;  CLRFF(FF_IIFF);
}

void C6120::MainInterrupt()
{
  //++
  //   This routine simulates a conventional PDP-8 style interrupt.  On the
  // HD6120, this can happen only when control panel mode is NOT active,
  // and is the simplest of the interrupts.  It saves the current IF and DF
  // in the PS Save Field bits; stores the current PC in main memory location
  // zero, and then jumps to location 1.
  // 
  //   Note that there are several conditions around when an interrupt can
  // occur, incuding testing the interrupt enable flip flop and the IB to
  // IF interrupt inhibit flag, but none of those are handled here.  Once
  // we're called, it's been decided that an interrupt should occur!
  //--
  assert(!IsPanel());
  // Save the current IF and DF in the save field bits of the PS... 
  CLRPS(PS_SF);  m_PS |= (m_IF >> 9) | (m_DF >> 12);
  // Cave the PC in main memory field 0 location 0 ...
  m_pMainMemory->CPUwrite(0, m_PC);
  // Disable interrupts until the program turns them back on again... 
  CLRPS(PS_IEFF);
  // Start executing at location 1, field 0... 
  m_IF = m_IB = m_DF = 0;  m_PC = 1;
  // All this takes 4 clocks on the HD6120 ...
  AddCycles(4);
}

void C6120::PanelInterrupt()
{
  //++
  //   This routine simuates an HD6120 control panel interrupt.  This is similar
  // a conventional interrupt, except the panel flag is set first and the current
  // PC is stored in location 0 of panel memory, not main memory.  The IF, IB and
  // DF are not changed, nor is the Save Field, but the FZ (force zero) flag is
  // set instead.  This forces all direct memory accesses to panel memory field
  // zero regardless of the IF.  Also, in the case of a panel interrupt the new
  // PC is 7777 rather than 1.
  //--
  assert(!IsPanel());
  // Set the panel (CTRLFF) and force zero (FZ) flags, and clear panel data ...
  SETFF(FF_CTRL|FF_FZ);  CLRFF(FF_PDF);  UpdateMemoryPointers();
  // Store the old PC in location zero of panel memory ...
  m_pPanelDirect->CPUwrite(0, m_PC);
  // And start executing at location 7777 ...
  m_PC = 07777;
  //   The datasheet doesn't seem to say how long this takes, but I'll assume
  // it's the same as a main memory interrupt ...
  AddCycles(4);
}



////////////////////////////////////////////////////////////////////////////////
//////////////////////////// INSTRUCTION DECODING //////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void C6120::FetchAndExecute()
{
  //++
  // Fetch and execute one instruction...
  //--
  m_MA = m_PC;  m_nLastPC = m_IF|m_PC;
  m_PC = INC12(m_PC);  m_IR = ReadDirect();
           if (m_IR < 06000) DoMRI();
      else if (m_IR < 07000) DoIOT();
      else if ((m_IR & 00400) == 0) DoGroup1();
      else if ((m_IR & 00001) == 0) DoGroup2();
      else                          DoGroup3();
}

CCPU::STOP_CODE C6120::Run (uint32_t nCount)
{
  //++
  //   This is the main "engine" of the PDP8 emulator.  The UI code is  
  // expected to call it whenever the user gives a START, GO, STEP, etc 
  // command and it will execute HD6120 instructions until it either a)  
  // executes the number of instructions specified by nCount, or b) some        
  // condition arises to interrupt the simulation such as a HLT opcode, 
  // an illegal opcode or I/O, the user entering the escape sequence on 
  // the console, etc.  If nCount is zero on entry, then we will run for-       
  // ever until one of the previously mentioned break conditions arises.        
  //--
  bool fFirst = true;
  m_nStopCode = STOP_NONE;

  while (m_nStopCode == STOP_NONE) {
    // If any device events need to happen, now is the time...
    DoEvents();

    //   If the interrupt inhibit (IIFF) is set then no interrupts, neither
    // panel nor main, are recognized.  If the force fetch (FFETCH) flag is
    // set, then all interrupts are ignored until after the next instruction
    // is fetched and executed.  This is used to guarantee that at least one
    // instruction will be executed after an ION or panel exit.
    if (!ISFF(FF_IIFF) && !ISFF(FF_FFETCH)) {
      //   Check for external control panel interrupt requests and set the
      // BTSTRP flag if we find one ...
      if (IsCPREQ()) {
        SETFF(FF_BTSTRP);  m_pPanelInterrupt->AcknowledgeRequest();
      }
      //   If any of the four flags, PWRON, PNLTRP, BTSTRP, or HLTFLG are set
      // then force a panel interrupt.  Don't even bother checking for main
      // memory interrupts in that case because those won't be acknowledged.
      //
      //   However, if no panel interrupt is pending and main interrupts are
      // enabled by the IEFF, then check for a main memory interrupt.
      //
      //   And it probably doesn't need mentioning, but there's no interrupt
      // (neither panel nor main) if we're already in panel memory!
      if (!IsPanel()) {
        if (ISFF(FF_PWRON|FF_PNLTRP|FF_BTSTRP|FF_HLTFLG)) {
          PanelInterrupt();
        } else if (ISPS(PS_IEFF) && IsIRQ()) {
          MainInterrupt();
        }
      }
    }
    CLRFF(FF_FFETCH);

    // Stop after we hit a breakpoint ...
    if (!fFirst && m_pMemoryDirect->IsBreak(IForZ()|m_PC)) {
      m_nStopCode = STOP_BREAKPOINT;  break;
    }
    fFirst = false;

    // Ok, we're ready to execute one instruction!
    FetchAndExecute();

    //   If the PC hasn't changed and interrupts are disabled, then we're stuck
    // in an infinite loop!
    if (((m_IF | m_PC) == m_nLastPC) && !ISPS(PS_IEFF))
      m_nStopCode = STOP_ENDLESS_LOOP;

    // Terminate if we've executed enough instructions ... 
    if (m_nStopCode == STOP_NONE) {
      if ((nCount > 0) && (--nCount == 0))  m_nStopCode = STOP_FINISHED;
    }
  }

  return m_nStopCode;
}
