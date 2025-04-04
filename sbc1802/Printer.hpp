//++
// Printer.hpp -> SBC1802 specific parallel port printer emulation
//
//   COPYRIGHT (C) 2015-2025 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
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
//
// REVISION HISTORY:
// 26-MAR-25  RLA   New file.
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ std::string class, et al ...
#include "EMULIB.hpp"           // emulator library definitions
#include "ImageFile.hpp"        // CTextOutputFile class
#include "MemoryTypes.h"        // address_t and word_t data types
#include "EventQueue.hpp"       // CEventQueue declarations
#include "Device.hpp"           // generic device definitions
#include "PPI.hpp"              // generic parallel interface definitions
#include "CDP1851.hpp"          // RCA CDP1851 specific PPI emulation
using std::string;              // ...


class CPrinter : public CCDP1851
{
  //++
  // Special SBC1802 emulation for a parallel port printer interface ...
  //--

public:
  enum {
    // Magic constants ...
    DEFAULT_SPEED       =  100, // 100 cps by default
    DEFAULT_WIDTH       =   80, // 80 columns by default
    ACK_PULSE_WIDTH     =  500, // ACK pulse width (nanoseconds)
    CR                  = 0x0D, // ASCII carriage return
    LF                  = 0x0A, // ASCII line feed
    //   Bits in m_bLastControl - these are outputs from the SBC1802 and
    // inputs to the printer.  All of these bits correspond to the bits
    // used for these signals in PPI port B.
    CTL_INIT            = 0x01, // initialize printer
    CTL_SELECT_OUT      = 0x04, // select the printer (SBC1802->PRINTER)
    CTL_AUTO_LF         = 0x40, // enable auto line feed after CR
    CTL_STROBE          = 0x80, // printer data strobe
    CTL_WRITE_MASK      = 0x05, // bits writable by output to port B
    //   Bits in m_bCurrentStatus - these are inputs to the SBC1802 and
    // outputs from the printer.  All of these, except for ACK and BUSY,
    // correspond to the bits used for these signals in PPI port B.
    STS_SELECT_IN       = 0x02, // printer selected (PRINTER->SBC1802)
    STS_ERROR           = 0x08, // any printer error condition
    STS_PAPER_OUT       = 0x10, // printer is out of paper
    STS_ACK             = 0x40, // printer ACK (on ASTB)
    STS_BUSY            = 0x80, // printer BUSY (on BSTB)
    STS_MASK_READ       = 0x1A, // bits readable by input from port B
    // Event callback event numbers ...
    EVENT_BUSY_DELAY    = 1025, // delay from STROBE -> BUSY
    EVENT_ACK_DELAY     = 1026, // ACK release callback
  };

  // Constructor and destructor ...
public:
  CPrinter (const char *pszName, address_t nPort, CEventQueue *pEvents);
  virtual ~CPrinter() {};
private:
  // Disallow copy and assignments!
  CPrinter(const CPrinter&) = delete;
  CPrinter& operator= (CPrinter const&) = delete;

  // Public properties ...
public:
  // Set the printer speed, in characters per second ...
  void SetSpeed (uint32_t nCPS)   {m_llBusyDelay = CPSTONS(nCPS);}
  uint32_t GetSpeed() const {return NSTOCPS(m_llBusyDelay);}
  // Set the printer line width, in columns ...
  void SetWidth (uint32_t nWidth) {m_lLineWidth = nWidth;}
  uint32_t GetWidth() const {return m_lLineWidth;}

  // CPrinter device methods from CCDP1851 and CPPI ...
public:
  // Initialize the printer (after a hardware RESET, for example) ...
  virtual void ClearDevice() override;
  // Handle scheduled printer events ...
  virtual void EventCallback (intptr_t lParam) override;
  // Show the printer status ...
  virtual void ShowDevice (ostringstream& ofs) const override;

  // Overrides from CCDP1851 and CPPI ...
protected:
  // Reading PPI port B returns the current printer status signals ...
  virtual uint8_t ReadB() override {return UpdateStatus();}
  // Writing PPI port B drives various printer control signals ...
  virtual void WriteB (uint8_t bData) override
    {UpdateControl(bData);   CCDP1851::WriteB(bData);}
  //   Writing PPI port A drives the data bits.  Note that we still need to
  // call the base class WriteA() method so that it can cache the data, just
  // in case the program tries to read back the port!
  virtual void WriteA (uint8_t bData) override
    {m_bDataBuffer = bData;  CCDP1851::WriteA(bData);}
  //   The READY A output controls the printer STROBE signal, which latches
  // the data and starts printing!
  virtual void OutputReadyA (uint1_t bNew) override {SetStrobe(bNew != 0);}
  // The READY B output just controls the AUTO LF signal ...
  virtual void OutputReadyB (uint1_t bNew) override {SetAutoLF(bNew == 0);}

  // Passthru methods for the printer text file ...
public:
  // Attach this printer emulator to a text file ...
  bool OpenFile (const string &sFileName) {return m_PrinterFile.Open(sFileName);}
  // Return the name of the currently attached file ...
  string GetFileName() const {return m_PrinterFile.GetFileName();}
  // Return TRUE if the printer is attached to a file ...
  bool IsAttached() const {return m_PrinterFile.IsOpen();}
  // Close the printer file ...
  void CloseFile() {m_PrinterFile.Close();}

  // Status and control bit functions ...
protected:
  // Set, clear or test the printer BUSY signal ...
  void SetBusy (bool fSet=true);
  inline bool IsBusy() const {return ISSET(m_bCurrentStatus, STS_BUSY);}
  // Set, clear or test the printer ACK signal ...
  void SetAcknowledge (bool fSet=true);
  inline bool IsAcknowledge() const {return ISSET(m_bCurrentStatus, STS_ACK);}
  //   Return TRUE if INIT is asserted.  Note that the INIT bit in the B
  // control register is inverted (aka active low!) ...
  inline bool IsInitialize() const {return !ISSET(m_bLastControl, CTL_INIT);}
  // Return TRUE if the printer should be selected (also inverted!).
  inline bool IsSelectOut() const {return !ISSET(m_bLastControl, CTL_SELECT_OUT);}
  // Assert or deassert the printer STROBE signal ...
  void SetStrobe (bool fSet);
  inline bool IsStrobe() const {return  ISSET(m_bLastControl, CTL_STROBE);}
  // Set or return the state of the AUTOLF signal ...
  inline bool IsAutoLF() const {return !ISSET(m_bLastControl, CTL_AUTO_LF);}
  inline void SetAutoLF (bool fAutoLF=true)
    {if (fAutoLF) CLRBIT(m_bLastControl, CTL_AUTO_LF); else SETBIT(m_bLastControl, CTL_AUTO_LF);}

  // Private methods ...
protected:
  // Send characters to the printer image file ...
  void Print (uint8_t ch);
  void NewLine (bool fLF=true);
  // Update the current printer status ...
  uint8_t UpdateStatus();
  // Update the current printer control signals ...
  void UpdateControl (uint8_t bData);

  // Private member data...
protected:
  uint8_t   m_bCurrentStatus;   // status byte to be read from port B
  uint8_t   m_bLastControl;     // last byte written to port B
  uint64_t  m_llBusyDelay;      // per character delay while printing
  uint32_t  m_lLineWidth;       // width of a printer line
  uint32_t  m_lCurrentColumn;   // width of the current line
  uint8_t   m_bDataBuffer;      // 8 bit data buffer
  CTextOutputFile m_PrinterFile;// printer text output file
};
