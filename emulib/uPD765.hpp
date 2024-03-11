//++
// uPD765.hpp -> NEC Floppy Diskette Controller emulation
//
//   COPYRIGHT (C) 2024 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
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
//   This class is a (very!) rudimentary emulation of the NEC Upd765 FDC.  Note
// that this is NOT a CDevice implementation and it expects to be inherited by
// some implementation specific floppy disk controller emulation.
//
// REVISION HISTORY:
//  5-MAR-24  RLA   New file.
//--
#pragma once
#include <stdint.h>
#include <string>               // C++ std::string class, et al ...
#include "EMULIB.hpp"           // emulator library definitions
#include "ImageFile.hpp"        // CDiskImageFile, et al ...
#include "MemoryTypes.h"        // address_t and word_t data types
#include "EventQueue.hpp"       // CEventQueue declarations
using std::string;              // ...


class CUPD765 : public CEventHandler {
  //++
  // uPD765 floppy diskette controller emulation ...
  //--

  // uPD765 state machine states ...
public:
  enum _FDC_STATES {
    ST_IDLE,                  // waiting for command
    ST_READ_COMMAND,          // reading command packet
    ST_BUSY,                  // executing a command
    ST_SEND_RESULT,           // sending result packet
    ST_READ_DATA,             // reading data (via programmed I/O)
    ST_SEND_DATA,             // sending data (via programmed I/O)
  };
  typedef enum _FDC_STATES FDCSTATE;

  // uPD765 event callback codes ...
private:
  enum {
    //   These are the event codes passed to the EventCallback() method.
    // Notice that we start with 100 here to avoid conflicts with any events
    // used by the derived floppy interface class.
    //
    //   Also note that there is a separate SEEK_DONE event for each unit,
    // so EVENT_SEEK_DONE is for unit 0, EVENT_SEEK_DONE+1 is unit 1, etc.
    // Be sure you allow enough space if you assign new events!!!
    EVENT_READ_DATA     = 100,// delay before reading data from diskette
    EVENT_WRITE_DATA    = 101,// delay before writing data to diskette
    EVENT_FORMAT_NEXT   = 102,// delay before formatting the next sector
    EVENT_SEEK_DONE     = 110,// Seek complete events for each unit
  };

  // uPD765 magic constants ...
public:
  enum {
    MAXUNIT          =    4,        // number of floppy drives supported
    MAXCOMMAND       =    9,        // longest possible command packet        
    MAXRESULT        =    7,        // longest possible result packet
    MAXSTATUS        =    4,        // number of extended status bytes
    MAXSECTORSIZE    = 1024,        // longest possible sector size ever
    SECTORSIZE       =  512,        // initial sector size until changed
    TRANSFER_DELAY   = USTONS(13),  // 13us per byte in MFM mode
    ROTATIONAL_DELAY = MSTONS(100), // 300 RPM --> 200ms per revolution
  };

  // uPD765 commands and status bits ...
public:
  enum {
    // uPD765 main status register bits ...
    STS_FDD0_BUSY     = 0x01, // floppy drive 0 busy seeking
    STS_FDD1_BUSY     = 0x02, //   "      "   1  "     "  "
    STS_FDD2_BUSY     = 0x04, //   "      "   2  "     "  "
    STS_FDD3_BUSY     = 0x08, //   "      "   3  "     "  "
    STS_FDC_BUSY      = 0x10, // read or write command in progress
    STS_NONDMA        = 0x20, // uPD765 is using programmed I/O
    STS_DATA_OUT      = 0x40, // 1 -> data from FDC to CPU
    STS_DATA_REQUEST  = 0x80, // data transfer ready
    // uPD765 status register 0 bits ...
    ST0_IC_NORMAL     = 0x00, // normal termination
    ST0_IC_ABNORMAL   = 0x40, // abnormal termination
    ST0_IC_INVCMD     = 0x80, // invalid command
    ST0_IC_NOT_READY  = 0xC0, // drive not ready
    ST0_SEEK_END      = 0x20, // seek finished
    ST0_UNIT_CHECK    = 0x10, // drive error condition
    ST0_NOTREADY      = 0x08, // drive not ready
    ST0_HEAD_SELECT   = 0x04, // selected head
    ST0_UNIT_SELECT   = 0x03, // selected unit
    // uPD765 status register 1 bits ...
    ST1_END_OF_CYL    = 0x80, // end of cylinder
    ST1_DATA_ERROR    = 0x20, // data error in sector address or data
    ST1_TIMEOUT       = 0x10, // DMA time out (data overrun)
    ST1_NO_DATA       = 0x04, // selected sector ID cannot be found
    ST1_WRT_PROTECT   = 0x02, // selected drive is write protected
    ST1_NO_AM         = 0x01, // no address mark found
    // uPD765 status register 2 bits ...
    ST2_DDATA         = 0x40, // deleted address mark found
    ST2_CRC_ERROR     = 0x20, // CRC error in data field
    ST2_WRONG_CYL     = 0x10, // wrong cylinder found in address mark
    ST2_SEEK_EQUAL    = 0x08, // seek equal
    ST2_SEEK_ERROR    = 0x04, // seek error (sector not found)
    ST2_BAD_CYLINDER  = 0x02, // bad cylinder
    ST2_NOT_DATA      = 0x01, // cannot find address mark
    // uPD765 status register 3 bits ...
    ST3_ERR_SIGNAL    = 0x80, // drive error signal active
    ST3_WRT_PROTECT   = 0x40, // drive write protect signal active
    ST3_READY         = 0x20, // drive ready signal active
    ST3_TRACK_0       = 0x10, // drive track 0 signal active
    ST3_DOUBLE_SIDED  = 0x08, // drive indicates double sided
    ST3_HEAD_SELECT   = 0x04, // drive head select signal
    ST3_UNIT_SELECT   = 0x03, // drive unit select signals
    // uPD765 commands ...
    CMD_READ_TRACK    = 0x02, // read complete track
    CMD_READ_SECTOR   = 0x06, // read sector
    CMD_WRITE_SECTOR  = 0x05, // write sector
    CMD_READ_DELETED  = 0x0C, // read deleted sector
    CMD_WRITE_DELETED = 0x09, // write deleted sector
    CMD_READ_SECTOR_ID= 0x0A, // read sector address mark
    CMD_FORMAT_TRACK  = 0x0D, // format complete track
    CMD_RECALIBRATE   = 0x07, // recalibrate head position
    CMD_DRIVE_STATE   = 0x04, // get drive status
    CMD_SENSE_INT     = 0x08, // get interrupt status
    CMD_SEEK          = 0x0F, // seek to track
    CMD_SCAN_EQUAL    = 0x11, // scan (verify) equal
    CMD_SCAN_LE       = 0x19, // scan less or equal
    CMD_SCAN_GE       = 0x1D, // scan greater or equal
    CMD_SPECIFY       = 0x03, // specify parameters
    CMD_MASK          = 0x1F, // mask for commands
    // Command modifier bits ...
    CMD_MULTI_TRACK   = 0x80, // set to automatically switch heads
    CMD_MFM_MODE      = 0x40, // set to operate in MFM mode
    CMD_SKIP_DELETED  = 0x20, // set to skip deleted data
  };

  // uPD765 command packet formats ...
public:
#pragma pack(push,1)
  //   uPD765 "type 1" command packet (used by READ TRACK, WRITE SECTOR,
  // READ SECTOR, WRITE DELETED, READ DELETED, SCAN EQUAL, SCAN LESS OR EQUAL,
  // and SCAN GREATER OR EQUAL ...
  struct _CMD_TYPE1 {
    uint8_t bCommand;         // command byte and modifiers
    uint8_t bHeadUnit;        // head and drive select
    uint8_t bTrackNumber;     // desired track number
    uint8_t bHeadSelect;      // head select
    uint8_t bSectorNumber;    // sector number
    uint8_t bSizeCode;        // encoded sector length
    uint8_t bEndofTrack;      // track length
    uint8_t bGapLength;       // length of GAP3
    uint8_t bDataLength;      // data length if SizeCode == 0
  };
  typedef struct _CMD_TYPE1 CMD_TYPE1;
  // uPD765 "type 2" command packet (used by FORMAT TRACK) ...
  struct _CMD_TYPE2 {
    uint8_t bCommand;         // command byte and modifiers
    uint8_t bHeadUnit;        // head and drive select
    uint8_t bSizeCode  ;      // sector length
    uint8_t bSectorCount;     // number of sectors
    uint8_t bGapSize;         // length of GAP3
    uint8_t bFillByte;        // fill byte for sector data
  };
  typedef struct _CMD_TYPE2 CMD_TYPE2;
  //   uPD765 "type 3" command packet (used by READ ID, SENSE DRIVE STATE, and
  // RECALIBRATE) ...
  struct _CMD_TYPE3 {
    uint8_t bCommand;         // 0 command byte and modifiers
    uint8_t bHeadUnit;        // 1 head and drive select
  };
  typedef struct _CMD_TYPE3 CMD_TYPE3;
  // uPD765 "type 4" command packet (used by SEEK TRACK) ...
  struct _CMD_TYPE4 {
    uint8_t bCommand;         // 0 command byte and modifiers
    uint8_t bHeadUnit;        // 1 head and drive select
    uint8_t bTrackNumber;     // 3 physical track number
  };
  typedef struct _CMD_TYPE4 CMD_TYPE4;
  // uPD765 "type 5" command packet (used by SPECIFY) ...
  struct _CMD_TYPE5 {
    uint8_t bCommand;         // 0 command byte and modifiers
    uint8_t bStepRate;        // 1 step rate and head load delay
    uint8_t bHeadLoad;        // 2 DMA disable and head load delay
  };
  typedef struct _CMD_TYPE5 CMD_TYPE5;
#pragma pack(pop)

  // uPD765 result packet formats ...
public:
#pragma pack(push,1)
  //   uPD765 "type 1" result packet (used by almost everything, READ TRACK,
  // WRITE SECTOR, READ SECTOR, READ DELETED, WRITE DELETED< SCAN EQUAL, SCAN LESS
  // OR EQUAL, SCAN GREATER OR EQUAL, FORMAT TRACK and READ ID)...
  struct _RST_TYPE1 {
    uint8_t bST0;             // status register 0
    uint8_t bST1;             // status register 1
    uint8_t nST2;             // status register 2
    uint8_t bTrack;           // track ID
    uint8_t bHead;            // head ID
    uint8_t bLastSector;      // last sector (number of sectors for READ TRACK)
    uint8_t bSectorSize;      // sector size
  };
  typedef struct _RST_TYPE1 RST_TYPE1;
  // uPD765 "type 2" result (used by SENSE DRIVE STATE only) ...
  struct _RST_TYPE2 {
    uint8_t bST3;             // 0 status register 3
  };
  typedef struct _RST_TYPE2 RST_TYPE2;
  // uPD765 "type 3" result (used by SENSE INTERRUPT only) ...
  struct _RST_TYPE3 {
    uint8_t bST0;             // 0 status register 0
    uint8_t bTrackNumber;     // 1 physical track number
  };
  typedef struct _RST_TYPE3 RST_TYPE3;
#pragma pack(pop)

public:
  // Constructor and destructor...
  CUPD765(CEventQueue *pEvents);
  virtual ~CUPD765();
private:
  // Disallow copy and assignments!
  CUPD765(const CUPD765&) = delete;
  CUPD765& operator= (CUPD765 const &) = delete;

  // Public FDC properties ...
public:
  // Return TRUE if the drive is attached (online) ...
  bool IsAttached (uint8_t nUnit) const
    {assert(nUnit < MAXUNIT);  return m_apImages[nUnit]->IsOpen();}
  bool IsAttached() const {return IsAttached(m_bCurrentUnit);}
  // Return TRUE if the drive/file is read only (not supported) ...
  // Set or get the write protect flag for a diskette drive ...
  void SetWriteLock (uint8_t nUnit, bool fProtect=true);
  bool IsWriteLocked (uint8_t nUnit) const
    {assert(nUnit < MAXUNIT);  return m_afWriteLock[nUnit];}
  bool IsWriteLocked() const {return IsWriteLocked(m_bCurrentUnit);}
  // Return TRUE if the drive is busy seeking ...
  bool IsBusy(uint8_t nUnit) const
    {assert(nUnit < MAXUNIT);  return m_afBusySeeking[nUnit];}
  bool IsBusy() const {return IsBusy(m_bCurrentUnit);}
  // Return the external file that we're attached to ...
  string GetFileName (uint8_t nUnit=0) const
    {assert(nUnit < MAXUNIT);  return IsAttached(nUnit) ? m_apImages[nUnit]->GetFileName() : "";}
  // Return the capacity of the drive, in sectors ...
  uint32_t GetCapacity (uint8_t nUnit=0) const
    {assert(nUnit < MAXUNIT);  return m_apImages[nUnit]->GetCapacity(); }
  // Get or set the delay factors ...
  uint64_t GetStepDelay()       const {return m_llStepDelay;}
  uint64_t GetRotationalDelay() const {return m_llRotationalDelay;}
  uint64_t GetTransferDelay()   const {return m_llTransferDelay;}
  uint64_t GetLoadDelay()       const {return m_llHeadLoadDelay;}
  uint64_t GetUnloadDelay()     const {return m_llHeadUnloadDelay;}
  void SetStepDelay       (uint64_t llDelay) {m_llStepDelay       = llDelay;}
  void SetRotationalDelay (uint64_t llDelay) {m_llRotationalDelay = llDelay;}
  void SetTransferDelay   (uint64_t llDelay) {m_llTransferDelay   = llDelay;}
  void SetLoadDelay       (uint64_t llDelay) {m_llHeadLoadDelay   = llDelay;}
  void SetUnloadDelay     (uint64_t llDelay) {m_llHeadUnloadDelay = llDelay;}

  // Private FDC properties ...
private:
  //   There are many places where we reference the currently selected unit,
  // and these shortcuts save typing!
  inline CDiskImageFile *CurrentImage() {return m_apImages[m_bCurrentUnit];}
  inline const CDiskImageFile *CurrentImage() const {return m_apImages[m_bCurrentUnit];}
  inline uint16_t CurrentHeads() const {return CurrentImage()->GetHeads();}
  inline uint16_t CurrentSectors() const {return CurrentImage()->GetSectors();}
  inline uint16_t CurrentTracks() const {return CurrentImage()->GetCylinders();}
  inline uint16_t CurrentSectorSize() const {return CurrentImage()->GetSectorSize();}
  inline uint8_t CurrentTrack() const {return m_abCurrentTrack[m_bCurrentUnit];}
  // Test various bits and fields in the current command packet ...
  inline uint8_t CurrentCommand() const {return m_abCommand[0] & CMD_MASK;}
  inline bool IsDMAmode() const {return !m_fNoDMAmode;}
  inline bool IsMultiTrack() const {return ISSET(m_abCommand[0], CMD_MULTI_TRACK);}
  inline bool IsMFM() const {return ISSET(m_abCommand[0], CMD_MFM_MODE);}

  // Public FDC methods ...
public:
  // Set or get the geometry for a diskette drive ...
  void SetGeometry (uint8_t nUnit, uint16_t  nSectorSize, uint16_t  nSectors, uint16_t  nTracks, uint16_t  nHeads);
  void GetGeometry (uint8_t nUnit, uint16_t &nSectorSize, uint16_t &nSectors, uint16_t &nTracks, uint16_t &nHeads) const;
  // Attach the emulated drive to a disk image file ...
  virtual bool Attach (uint8_t nUnit, const string &sFileName, bool fWriteLock=false);
  virtual void Detach (uint8_t nUnit);
  virtual void DetachAll();
  // Access the uPD765 internal registers ...
  uint8_t ReadStatus();
  uint8_t ReadData();
  void WriteData (uint8_t bData);
  // Execute DMA transfers (derived classes should override!) ...
  virtual uint8_t DMAread() {return 0xFF;}
  virtual void DMAwrite (uint8_t bData) {};
  // Assert the uPD765 Terminal Count input ...
  void TerminalCount();
  // Request an uPD765 interrupt ...
  virtual void FDCinterrupt (bool fInterrupt=true) {};
  // Reset the FDC ...
  void ResetFDC();
  // Handle event queue callbacks ...
  void EventCallback (intptr_t lParam);
  // Show uPD765 status (for UI "SHOW DEVICE" command) ...
  void ShowFDC (ostringstream &ofs) const;

  // Private methods ...
private:
  //   Set the next state for the state machine.  Yes, this is a trivial
  // operation, but we handle it as a function for debugging!!
  inline void NextState (FDCSTATE Next) {m_State = Next;}
  // Convert the current state to a string ...
  string StateToString (FDCSTATE nState) const;
  // Return the number of bytes expected in this command ...
  uint8_t CommandLength (uint8_t bCommand);
  // Fill out the secondary status registers ..
  uint8_t UpdateST0 (uint8_t bInterruptCode);
  uint8_t UpdateST3();
  // Send a result packet back to the host ...
  void SendResult (uint8_t bLength);
  void SendResultType1();
  // Set up the parameters for common commands ...
  bool SetupCommandType1 (CMD_TYPE1 *pCommand, bool fWrite=false);
  // Execute the current command in the command buffer ...
  void DoCommand();
  // Send an INVALID COMMAND result ...
  void DoInvalid (uint8_t bCommand);
  // Handle various non-data transfer FDC commands ...
  void DoSpecify (CMD_TYPE5 *pCommand);
  void DoSenseInterrupt();
  void DoSenseDriveStatus (CMD_TYPE3* pCommand);
  // Handle seek/recalibrate commands ...
  void DoRecalibrate (CMD_TYPE3 *pCommand);
  void DoSeek (CMD_TYPE4 *pCommand);
  void SeekTrack (uint8_t bTrack);
  void SeekDone (uint8_t bUnit);
  // Handle diskette read or write operations ...
  void DoReadSector (CMD_TYPE1 *pCommand);
  void ReadTransfer();
  void DoWriteSector (CMD_TYPE1 *pCommand);
  void WriteTransfer();
  // Handle the FORMAT TRACK command ...
  bool GetFormatParameter (uint8_t &bData);
  void DoFormatTrack (CMD_TYPE2 *pCommand);
  void FormatNextSector();

  // Private member data...
protected:
  // Controller status ...
  FDCSTATE  m_State;                    // current FDC state
  uint8_t   m_bMainStatus;              // current status byte for ReadStatus()
  uint8_t   m_abST[MAXSTATUS];          // extended result status bytes
  bool      m_fNoDMAmode;               // no DMA (programmed I/O) mode selected
  uint8_t   m_bFillByte;                // filler byte used by FORMAT TRACK
  CEventQueue *m_pEvents;               // event queue handler

  // Command and result packets ...
  uint8_t   m_nCommandLength;           // expected length of command packet
  uint8_t   m_nResultLength;            // length of current result packet
  uint8_t   m_abCommand[MAXCOMMAND];    // current command packet
  uint8_t   m_abResult[MAXRESULT];      // current result packet

  // Sector transfer buffer ...
  uint16_t  m_nCurrentByte;             // current byte when sending or receiving
  uint16_t  m_nDataLength;              // length of data being transmitted
  uint8_t   m_abBuffer[MAXSECTORSIZE];  // temporary buffer for reading or writing

  // Drive status ...
  bool      m_afWriteLock[MAXUNIT];     // write lock flag for each unit
  uint8_t   m_bCurrentUnit;             // unit selected by last command
  uint8_t   m_bCurrentHead;             // head   "   "  "    "   "   "
  uint8_t   m_bCurrentSector;           // sector "   "  "    "   "   "
  uint8_t   m_bSizeCode;                // sector size code   "   "   "
  uint8_t   m_abCurrentTrack[MAXUNIT];  // current head position for all drives
  bool      m_afBusySeeking[MAXUNIT];   // true if the drive is busy seeking
  CDiskImageFile *m_apImages[MAXUNIT];  // diskette image file(s)

  // Delay and timing parameters ...
  uint64_t  m_llStepDelay;              // diskette head step delay
  uint64_t  m_llRotationalDelay;        // average diskette rotational delay
  uint64_t  m_llTransferDelay;          // delay between bytes when reading/writing
  uint64_t  m_llHeadLoadDelay;          // head load delay time
  uint64_t  m_llHeadUnloadDelay;        // head unload delay time
};
