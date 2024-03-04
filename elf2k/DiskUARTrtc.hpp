//++
// DiskUARTrtc.hpp -> ELF2K Disk/UART/RTC board emulation
//
//   COPYRIGHT (C) 2015-2020 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
//
// LICENSE:
//    This file is part of the ELF2K emulator project.  EMULIB is free
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
// REVISION HISTORY:
// 20-JAN-20  RLA   New file.
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ std::string class, et al ...
#include "MemoryTypes.h"        // address_t and word_t data types
#include "CPU.hpp"              // CPU definitions
#include "Device.hpp"           // generic device definitions
#include "INS8250.hpp"          // UART emulation device
#include "DS12887.hpp"          // NVR/RTC emulation
#include "IDE.hpp"              // IDE disk emulation
using std::string;              // ...
class CVirtualConsole;          // ...
class CEventQueue;              // ...
class CCPU;                     // ...

class CDiskUARTrtc : public CDevice {
  //++
  //--

  // Magic constants ...
public:
  enum {
    // Status register bits ...
    STS_CD1       = 0x20,     // CompactFlash card detect #1
    STS_CD2       = 0x10,     // CompactFlash card detect #2
    STS_DASP      = 0x08,     // IDE device active LED
    STS_UART_IRQ  = 0x04,     // UART IRQ active
    STS_RTC_IRQ   = 0x02,     // RTC IRQ active
    STS_DISK_IRQ  = 0x01,     // Disk IRQ active
  };

  // Constructor and destructor ...
public:
  CDiskUARTrtc  (address_t nPort, CEventQueue *pEvents);
  ~CDiskUARTrtc();

  // CDiskUARTrtc device methods ...
public:
  virtual void ClearDevice() override;
  virtual word_t DevRead (address_t nPort) override;
  virtual void DevWrite (address_t nPort, word_t bData) override;
  virtual void ShowDevice (ostringstream &ofs) const override;

  // Other public CDiskUARTrtc methods ...
public:
  // Install or remove the UART device ...
  bool InstallUART (CVirtualConsole *pConsole, CCPU *pCPU);
  void RemoveUART();
  // Install or remove the NVR/RTC device ...
  bool InstallNVR (const string &sFileName="");
  void RemoveNVR();
  // Install or remove the IDE device ...
  bool InstallIDE (const string &sFileName="");
  void RemoveIDE();
  // Return TRUE if the specified subdevice is attached ...
  bool IsUARTinstalled() const {return m_pUART != NULL;}
  bool IsNVRinstalled()  const {return m_pNVR  != NULL;}
  bool IsIDEinstalled()  const {return m_pIDE  != NULL;}
  // Return pointers to our child objects ...
  CINS8250  *GetUART() {assert(m_pUART != NULL);  return m_pUART;}
  C12887 *GetNVR()  {assert(m_pNVR  != NULL);  return m_pNVR;}
  CIDE   *GetIDE()  {assert(m_pIDE  != NULL);  return m_pIDE;}
  // Find a child device by name ...
  CDevice *FindDevice (string sName);
  // Return the current status register contents ...
  uint8_t GetStatus() const {return m_bStatus;}

  // Private member data...
protected:
  uint8_t    m_bStatus;         // current card status register contents
  uint8_t    m_bSelect;         // last value written to the select register
  CINS8250  *m_pUART;           // UART emulation device
  C12887    *m_pNVR;            // NVR/RTC   "       "
  CIDE      *m_pIDE;            // IDE disk  "       "
};
