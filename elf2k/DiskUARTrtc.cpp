//++
// DiskUARTrtc.cpp -> ELF2K Disk/UART/RTC board emulation
//
//   COPYRIGHT (C) 2015-2020 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
//
// LICENSE:
//    This file is part of the ELF2K emulator project.  ELF2K is free
// software; you may redistribute it and/or modify it under the terms of
// the GNU Affero General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any
// later version.
//
//    ELF2K is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License
// for more details.  You should have received a copy of the GNU Affero General
// Public License along with ELF2K.  If not, see http://www.gnu.org/licenses/.
//
// DESCRIPTION:
//   This module is an emulation of the real ELF2K Disk/UART/RTC card.  As you
// might guess, this card contains three distinct I/O subsystems - #1 a UART
// (any one of the 8250, 16450 or 16550), #2 an IDE disk or CompactFlash card
// interface, and #3 a non-volatile RAM and real time clock chip (a DS1287,
// DS12887 or DS12887A).  These three peripherals are all emulated by separate
// classes; this class just emulates the logic on the ELF2K card that handles
// the two level I/O necessary to access them.
//
// REVISION HISTORY:
// 20-JAN-20  RLA   New file.
// 19-FEB-24  RLA   Don't extend small IDE images to 32MB!
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include "EMULIB.hpp"           // emulator library definitions
#include "LogFile.hpp"          // emulator library message logging facility
#include "VirtualConsole.hpp"   // needed for CVirtualConsole
#include "ELF2K.hpp"            // global declarations for this project
#include "Interrupt.hpp"        // interrupt simulation logic
#include "EventQueue.hpp"       // I/O device event queue simulation
#include "MemoryTypes.h"        // address_t and word_t data types
#include "Memory.hpp"           // basic memory emulation declarations ...
#include "CPU.hpp"              // CCPU base class definitions
#include "Device.hpp"           // basic I/O device emulation declarations ...
#include "INS8250.hpp"          // UART emulation device
#include "DS12887.hpp"          // NVR/RTC emulation
#include "IDE.hpp"              // IDE/ATA disk emulation
#include "DiskUARTrtc.hpp"      // declarations for this module


CDiskUARTrtc::CDiskUARTrtc (address_t nPort, CEventQueue *pEvents)
  : CDevice("COMBO", "COMBO", "Disk/UART/RTC card", INOUT, nPort, 2, pEvents)
{
  //++
  //--
  m_pUART = NULL;  m_pNVR = NULL;  m_pIDE = NULL;
  ClearDevice();
}

CDiskUARTrtc::~CDiskUARTrtc()
{
  //++
  // Remove any and all devices before we go away ...
  //--
  if (IsUARTinstalled()) RemoveUART();
  if (IsNVRinstalled()) RemoveNVR();
  if (IsIDEinstalled()) RemoveIDE();
}

void CDiskUARTrtc::ClearDevice()
{
  //++
  // Clear the select and status registers, AND clear all child devices...
  //--
  m_bSelect = 0;
  m_bStatus = IsIDEinstalled() ? (STS_CD1|STS_CD2) : 0;
  if (IsUARTinstalled()) m_pUART->ClearDevice();
  if (IsNVRinstalled())  m_pNVR->ClearDevice();
  if (IsIDEinstalled())  m_pIDE->ClearDevice();
}

word_t CDiskUARTrtc::DevRead (address_t nPort)
{
  //++
  //   The Disk/UART/RTC card implements two ports - one reads the current
  // card status register, and the other port is passed on to one of the
  // subdevices to read a Disk, UART or RTC register.  The exact device is
  // selected by the last value written to the select register.
  //--
  assert(nPort >= GetBasePort());
  switch (nPort - GetBasePort()) {
    case 0:
      // Just read the status register and quit ...
      return m_bStatus;

    case 1:
      // Read from the subdevice selected by the select register!
      if ((m_bSelect & 0x90) == 0x00)
        return IsIDEinstalled() ? m_pIDE->DevRead(m_bSelect & 0x1F) : 0xFF;
      else if ((m_bSelect & 0x90) == 0x10)
        return IsUARTinstalled() ? m_pUART->DevRead(m_bSelect & 7) : 0xFF;
      else
        return IsNVRinstalled() ? m_pNVR->DevRead(m_bSelect & 0x7F) : 0xFF;

    default:
      // Should never get here!!
      assert(false);  return 0xFF;  // just to avoid C4715 errors!
  }
}

void CDiskUARTrtc::DevWrite (address_t nPort, word_t bData)
{
  //++
  //   The Disk/UART/RTC card implements two ports - one writes the current
  // selection register, and the other port is passed on to one of the
  // subdevices to write a Disk, UART or RTC register.  The exact device is
  // selected by the last value written to the select register.
  //--
  assert(nPort >= GetBasePort());
  switch (nPort - GetBasePort()) {
    case 0:
      // Just write the select register and quit ...
      m_bSelect = bData;  break;

    case 1:
      // Write to the subdevice selected by the select register!
      if ((m_bSelect & 0x90) == 0x00) {
        if (IsIDEinstalled()) m_pIDE->DevWrite((m_bSelect & 0x1F), bData);
      } else if ((m_bSelect & 0x90) == 0x10) {
        if (IsUARTinstalled()) m_pUART->DevWrite((m_bSelect & 7), bData);
      } else {
        if (IsNVRinstalled()) m_pNVR->DevWrite((m_bSelect & 0x7F), bData);
      }
      break;

    default:
      // Should never get here!!
      assert(false);
  }
}

bool CDiskUARTrtc::InstallUART (CVirtualConsole *pConsole, CCPU *pCPU)
{
  //++
  // Install the UART and connect it to the specified console window ...
  //--
  if (IsUARTinstalled()) return false;
  m_pUART = DBGNEW CINS8250("SLU", 0, GetEvents(), pConsole, pCPU);
  LOGS(DEBUG, m_pUART->GetDescription() << " attached to " << GetDescription());
  return true;
}

void CDiskUARTrtc::RemoveUART()
{
  //++
  // Delete the UART object and remove it from this card ...
  //--
  LOGS(DEBUG, "removing " << m_pUART->GetDescription() << " from " << GetDescription());
  if (IsUARTinstalled()) delete m_pUART;
  m_pUART = NULL;
}

bool CDiskUARTrtc::InstallNVR (const string &sFileName)
{
  //++
  //   Install the NVR/RTC chip.  Note that the initial NVR contents are zero -
  // you can call GetNVR()->Load() next to load it from a file ...
  //--
  if (IsNVRinstalled()) return false;
  m_pNVR = DBGNEW C12887("RTC", 0, GetEvents());
  LOGS(DEBUG, m_pNVR->GetDescription() << " attached to " << GetDescription());
  if (!sFileName.empty()) return m_pNVR->LoadNVR(sFileName)> 0;
  return true;
}

void CDiskUARTrtc::RemoveNVR()
{
  //++
  // Delete the NVR/RTC object and remove it from this card ...
  //--
  LOGS(DEBUG, "removing " << m_pNVR->GetDescription() << " from " << GetDescription());
  if (IsNVRinstalled()) delete m_pNVR;
  m_pNVR = NULL;
}

bool CDiskUARTrtc::InstallIDE (const string &sFileName)
{
  //++
  // Install the IDE disk drive and attach it to an image file ...
  //--
  if (IsIDEinstalled()) return false;
  m_pIDE = DBGNEW CIDE("DISK", 0, GetEvents());
  LOGS(DEBUG, m_pIDE->GetDescription() << " attached to " << GetDescription());
  SETBIT(m_bStatus, STS_CD1|STS_CD2);
  if (!sFileName.empty()) return m_pIDE->Attach(0, sFileName, 0);
  return true;
}

void CDiskUARTrtc::RemoveIDE()
{
  //++
  // Delete the IDE object and remove it from this card ...
  //--
  if (!IsIDEinstalled()) return;
  if (m_pIDE->IsAttached(0)) m_pIDE->Detach(0);
  LOGS(DEBUG, "removing " << m_pIDE->GetDescription() << " from " << GetDescription());
  CLRBIT(m_bStatus, STS_CD1|STS_CD2);
  delete m_pIDE;
  m_pIDE = NULL;
}

CDevice *CDiskUARTrtc::FindDevice (string sName)
{
  //++
  //  Search the child devices for one with a name matching the one given.
  // Return a pointer to that device, or NULL if we don't find one ...
  //--
  if (IsUARTinstalled() && (m_pUART->GetName() == sName)) return m_pUART;
  if (IsNVRinstalled()  && (m_pNVR->GetName()  == sName)) return m_pNVR;
  if (IsIDEinstalled()  && (m_pIDE->GetName()  == sName)) return m_pIDE;
  return NULL;
}

void CDiskUARTrtc::ShowDevice (ostringstream &ofs) const
{
  //++
  //   This routine will dump the state of the internal IDE registers.
  // This is used by the UI EXAMINE command ...
  //--
  ofs << FormatString("Select=0x%02X Status=0x%02X", m_bSelect, m_bStatus);
}