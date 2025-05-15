//++
// IDE.hpp -> Simple IDE drive emulation
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
//   This class emulates the basic functions of an IDE disk drive (or Compact
// Flash card in IDE emulation mode!).  It's hardly a complete emulation, and
// only implements the basic functions as used on the ELF2K.
//
// REVISION HISTORY:
// 22-JAN-20  RLA   New file.
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ std::string class, et al ...
#include "EMULIB.hpp"           // emulator library definitions
#include "ImageFile.hpp"        // CDiskImageFile, et al ...
#include "MemoryTypes.h"        // address_t and word_t data types
#include "EventQueue.hpp"       // CEventQueue declarations
#include "Device.hpp"           // generic device definitions
using std::string;              // ...


class CIDE : public CDevice
{
  //++
  // Generic IDE disk emulation ...
  //--

  // ATA registers and bits ...
public:
  enum {
    //   This is the geometry for a 32Mb disk drive or CompactFlash card.  No,
    // 32Mb (yes, "Mb" not "Gb"!) is not very big by modern standards, but for
    // most microprocessor systems that's huge!
    SECTOR_SIZE     = 512,    // standard sector size
    DEFAULT_CAPACITY= 65536,  // default IDE disk capacity (in sectors!)
    NDRIVES         = 2,      // number of drives supported (MASTER and SLAVE)
    MODELLEN        = 40,     // length of the IDENTIFY DEVICE model number string
    // Standard IDE registers...
    CS1FX           = 0x0000, // selects the 1Fx register space
    CS3FX           = 0x0008, //   "	  "  3Fx  "   "     "
    REG_DATA        = CS1FX+0,// data (R/W)
    REG_ERROR       = CS1FX+1,// error (read only)
    REG_FEATURE     = CS1FX+1,// feature (write only)
    REG_COUNT       = CS1FX+2,// sector count(R/W)
    REG_LBA0        = CS1FX+3,// LBA byte 0 (or sector number) (R/W)
    REG_LBA1        = CS1FX+4,// LBA byte 1 (or cylinder low)  (R/W)
    REG_LBA2        = CS1FX+5,// LBA byte 2 (or cylinder high) (R/W)
    REG_LBA3        = CS1FX+6,// LBA byte 3 (or device/head)   (R/W)
    REG_STATUS      = CS1FX+7,// status (read only)
    REG_COMMAND     = CS1FX+7,// command (write only)
    REG_ALTSTS      = CS3FX+6,// alternate status register (R/O)
    REG_DEVCTL      = CS3FX+6,// device control register (W/O)
    REG_DRVADDR     = CS3FX+7,// drive address register (R/O)
    MAXREG          = 16,     // number of registers (CS1 and CS3 combined)
    // IDE error register bits...
    ERR_IDNF        = 0x10,   // sector not found
    ERR_ABORT       = 0x04,   // invalid command
    // IDE status register bits...
    STS_BUSY        = 0x80,   // busy
    STS_READY       = 0x40,   // device ready
    STS_FAULT       = 0x20,   // device fault
    STS_SEEK_DONE   = 0x10,   // device seek complete
    STS_DRQ         = 0x08,   // data request
    STS_COR         = 0x04,   // corrected data flag
    STS_ERROR       = 0x01,   // error detectedv
    // Device control register bits ...
    CTL_SRST        = 0x04,   // software reset
    CTL_nIEN        = 0x02,   // interrupt enable (active low!)
    // IDE command codes (or at least the ones we use!)...
    CMD_FEATURES    = 0xEF,   // set features
    CMD_IDENTIFY    = 0xEC,   // identify device
    CMD_DIAGNOSE    = 0x90,   // execute device diagnostic
    CMD_READ        = 0x20,   // read sectors with retry
    CMD_READ_BUFFER = 0xE4,   // read back the sector buffer
    CMD_WRITE       = 0x30,   // write sectors with retry
    CMD_WRITE_BUFFER= 0xE8,   // load the sector buffer
    CMD_SPIN_UP     = 0xE1,   // spin up
    CMD_SPIN_DOWN   = 0xE0,   // spin down
    // IDE feature register bits ...
    FEA_8BIT        = 0x01,   // enable 8 bit mode
    // Drive/Head select register magic bits ...
    DRV_SLAVE       = 0x10,   // select the slave drive
    DRV_LBA         = 0x40,   // use LBA addressing
    DRV_HEAD        = 0x0F,   // head select bits
    // Magic bits for the IDENTIFY DEVICE command
    IDD_FIXED_DEVICE  = 1<<6, // wGeneralConfiguration
    IDD_LBA_SUPPORTED = 1<<9, // wCapabilities
    // IDE events ...
    //   Note that we need separate events for the master and slave drives
    // so that we can update the status of the correct unit!
    EVENT_READY_0   = 10,     // clear busy, set drive ready (MASTER drive)
    EVENT_READY_1   = 11,     //   "     "    "    "     "   (SLAVE drive)
    EVENT_READ_0    = 20,     // transfer data drive -> CPU (MASTER)
    EVENT_READ_1    = 21,     //   "   "    "    "       "  (SLAVE)
  };

  // Data returned by the ATA IDENTIFY DEVICE command ...
public:
#pragma pack(push,1)
  struct _IDENTIFY_DEVICE_DATA {
    uint16_t wGeneralConfiguration;       //   1 - IDD_FIXED_DEVICE
    uint16_t wNumberOfCylinders;          //   2 -
    uint16_t wSpecificConfiguration;      //   3 -
    uint16_t wNumberOfHeads;              //   4 -
    uint16_t wUnformattedBytesPerTrack;   //   5 -
    uint16_t wUnformattedBytesPerSector;  //   6 -
    uint16_t wSectorsPerTrack;            //   7 -
    uint16_t awATAreserved7[3];           //   8 -
    uint8_t  abSerialNumber[20];          //  10 -
    uint16_t wBufferType;                 //  20 -
    uint16_t wBufferSize;                 //  21 -
    uint16_t wECCbytes;                   //  22 -
    uint8_t  abFirmwareRevision[8];       //  23 -
    uint8_t  abModelNumber[40];           //  27 -
    uint16_t wReadWriteMultiple;          //  47 -
    uint16_t wTrustedComputing;           //  48 -
    uint16_t wCapabilities;               //  49 - IDD_LBA_SUPPORTED
    uint16_t wATAreserved50;              //  50 -
    uint16_t wPIOtimingMode;              //  51 -
    uint16_t wDMAtimingMode;              //  52 -
    uint16_t wATAreserved53;              //  53 -
    uint16_t wNumberOfCurrentCylinders;   //  54 -
    uint16_t wNumberOfCurrentHeads;       //  55 -
    uint16_t wCurrentSectorsPerTrack;     //  56 -
    uint32_t lCurrentCapacity;            //  57 -
    uint16_t wATAreserved59;              //  59 -
    uint32_t lUserAddressableSectors;     //  60 -
    uint16_t wSingleWordDMA;              //  62 - 
    uint16_t wMultiWordDMA;               //  63 -
    uint16_t awATAreserved64[64];         //  64 -
    uint16_t awVendorReserved[32];        //  
    uint16_t awATAreserved160[96];        //  80 -
  };
#pragma pack(pop)
  typedef struct _IDENTIFY_DEVICE_DATA IDENTIFY_DEVICE_DATA;

  // Constructor and destructor ...
public:
  CIDE (const char *pszName, const char *pszType, const char *pszDescription, address_t nPort, address_t nPorts, CEventQueue *pEvents);
  CIDE (const char *pszName, address_t nPort, CEventQueue *pEvents) : CIDE(pszName, "IDE", "Generic IDE/ATA Disk", nPort, MAXREG, pEvents) {};
  virtual ~CIDE();
private:
  // Disallow copy and assignments!
  CIDE(const CIDE&) = delete;
  CIDE& operator= (CIDE const&) = delete;

  // CIDE device methods from CDevice ...
public:
  virtual void ClearDevice() override;
  virtual void EventCallback (intptr_t lParam) override;
  virtual word_t DevRead (address_t nRegister) override;
  virtual void DevWrite (address_t nRegister, word_t bData) override;
  virtual void ShowDevice (ostringstream &ofs) const override;

  // Other public CIDE methods ...
public:
  // Attach the emulated drive to a disk image file ...
  bool Attach (uint8_t nUnit, const string &sFileName, uint32_t lCapacity);
  void Detach (uint8_t nUnit);
  void DetachAll();
  // Return TRUE if the drive is attached (online) ...
  bool IsAttached (uint8_t nUnit=0) const
    {assert(nUnit < NDRIVES);  return m_apImages[nUnit]->IsOpen();}
  // Return TRUE if the drive/file is read only (not supported) ...
  bool IsReadOnly (uint8_t nUnit=0) const
    {assert(nUnit < NDRIVES);  return IsAttached(nUnit) && m_apImages[nUnit]->IsReadOnly(); }
  // Return the external file that we're attached to ...
  string GetFileName (uint8_t nUnit=0) const
    {assert(nUnit < NDRIVES);  return IsAttached(nUnit) ? m_apImages[nUnit]->GetFileName() : "";}
  // Return the capacity of the drive, in sectors ...
  uint32_t GetCapacity (uint8_t nUnit=0) const
    {assert(nUnit < NDRIVES);  return m_apImages[nUnit]->GetCapacity(); }
  // Get or set the delay factors ...
  void SetDelays (uint64_t llLongDelay, uint64_t llShortDelay)
    {m_llLongDelay = llLongDelay;  m_llShortDelay = llShortDelay;}
  void GetDelays (uint64_t &llLongDelay, uint64_t &llShortDelay) const
    {llLongDelay = m_llLongDelay;  llShortDelay = m_llShortDelay;}
  void SetShortDelay (uint64_t llDelay) {m_llShortDelay = llDelay;}
  void SetLongDelay (uint64_t llDelay) {m_llLongDelay = llDelay;}
  // Force 8 bit mode regardless of any SET FEATURES command ...
  void Set8BitMode (uint8_t nUnit=0, bool f8Bit=true)
    {assert(nUnit < NDRIVES);  m_afForce8Bit[nUnit] = f8Bit;}
  bool Is8Bit (uint8_t nUnit) const 
    {assert(nUnit < NDRIVES);  return m_af8BitMode[nUnit] || m_afForce8Bit[nUnit];}

  // Private methods ...
protected:
  // Update the unit selection ...
  uint8_t SelectUnit();
  // Make the drive look busy or ready ...
  void DriveBusy (uint8_t nUnit);
  void DriveReady (uint8_t nUnit);
  // Set or clear the error register and the error status bit ...
  void ClearError (uint8_t nUnit);
  void SetError (uint8_t nUnit, uint8_t bError);
  // Update our interrupt request ...
  void UpdateInterrupt (uint8_t nUnit, bool fRequest);
  // Execute an IDE command ...
  void DoControl (uint8_t bControl);
  void DoCommand (uint8_t bCommand);
  // Set drive "features" ...
  void SetFeatures();
  // Identify the drive ...
  void IdentifyDevice (uint8_t nUnit);
  // Get the current disk address (either LBA or C/H/S) ...
  bool GetLBA (uint32_t &lLBA) const;
  // Transfer data ...
  void StartTransfer (uint8_t nUnit, bool fRead);
  void AbortTransfer (uint8_t nUnit);
  uint8_t ReadData();
  void WriteData (uint8_t bData);
  bool IsTransferInProgress() const {return m_cbTransfer > 0;}
  // Handle the READ and WRITE commands ...
  void DoDiskRead();
  void DoDiskWrite();
  void WriteSector();
  // Handle the READ BUFFER and WRITE BUFFER commands ...
  void DoReadBuffer();
  void DoWriteBuffer();
  // Handle commands that do nothing ..
  void DoNothing();
  // Set the model name (for ATA IDENTIFY DEVICE) for this drive ...
  void SetModelName (uint8_t nUnit, const char *pszModel);

  // Private member data...
protected:
  // ATA registers ...
  uint8_t   m_bFeatures;      // features (W/O)
  uint8_t   m_bCount;         // sector count(R/W)
  uint8_t   m_abLBA[4];       // LBA address (R/W)
  uint8_t   m_bLastCommand;   // last command executed
  // Other members ...
  uint8_t   m_nSelectedUnit;  // currently selected drive
  uint64_t  m_llLongDelay;    // "long" delay for disk read or write
  uint64_t  m_llShortDelay;   // "short" delay for all other commands
  uint16_t  m_cbTransfer;     // count of bytes transferred
  bool      m_fReadTransfer;  // true for a transfer disk -> CPU
  bool      m_fBufferOnly;    // true for READ BUFFER/WRITE BUFFER commands
  // Drive specific parameters ...
  uint8_t         m_abStatus[NDRIVES];    // status (R/O)
  uint8_t         m_abError[NDRIVES];     // error (R/O)
  bool            m_afIEN[NDRIVES];       // true if interrupts are enabled
  bool            m_afIRQ[NDRIVES];       // internal interrupt request flag
  bool            m_af8BitMode[NDRIVES];  // true if 8 bit mode is selected by features
  bool            m_afForce8Bit[NDRIVES]; // true to force 8 bit mode, regardless
  CDiskImageFile *m_apImages[NDRIVES];    // disk image file(s)
  char m_apszModelName[NDRIVES][MODELLEN];// model name for this drive
  //   Strictly speaking I think there should be a separate sector buffer for
  // each drive, but in most cases it doesn't matter and we're not going to
  // bother ...
  uint8_t         m_abBuffer[SECTOR_SIZE];// sector buffer
};
