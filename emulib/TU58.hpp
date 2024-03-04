//++
// TU58.hpp -> DEC TU58 RSP Mass Storage Drive emulation
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
//   This class emulates the basic functions of TU58 DECtape drive connected
// to any UART serial port. It's not a complete emulation, and only implements
// the basic functions needed by SBCT11 and SBC1802.
//
// REVISION HISTORY:
// 22-JAN-20  RLA   New file.
//--
#pragma once
#include <stdint.h>             // uint8_t, int32_t, and much more ...
#include <string>               // C++ std::string class, et al ...
#include <vector>               // C++ std::vector template
#include "EMULIB.hpp"           // emulator library definitions
#include "ImageFile.hpp"        // CDiskImageFile, et al ...
#include "ConsoleWindow.hpp"    // needed for CVirtualConsole ...
using std::string;              // ...
using std::iterator;            // ...
using std::vector;              // ...


class CTU58 : public CVirtualConsole {
  //++
  // DEC TU58 RSP Mass Storage Drive emulation
  //--

  // RSP magic numbers, bits and structures ...
public:
  enum {
    // TU58 General Parameters ...
    RSP_CAPACITY      =  512, // standard TU58 holds 512 blocks
    RSP_BLOCKSIZE     =  512, // and a standard block is 512 bytes
    RSP_RECORDSIZE    =  128, // but individual tape records are 128 bytes
    // TU58 Packet Types ...
    //   NOTE: in the TU58 documentation, these are referred to as "flags"!
    RSP_F_DATA        =   1,  // data packets for read/write
    RSP_F_CONTROL     =   2,  // control (command) packet
    RSP_F_INITIALIZE  =   4,  // forces controller initialization
    RSP_F_BOOTSTRAP   =   8,  // sends unit 0/block 0 w/o RSP
    RSP_F_CONTINUE    =  16,  // continue with the next data packet
    RSP_F_XOFF        =  19,  // pause sending to the host
    // TU58 Command OpCodes ...
    RSP_O_NOP         =   0,  // no operation (returns an END packet)
    RSP_O_INITIALIZE  =   1,  // (re)intialize the drive
    RSP_O_READ        =   2,  // reads data from the tape
    RSP_O_WRITE       =   3,  // writes data to the tape
    RSP_O_POSITION    =   5,  // positions the tape at a given block
    RSP_O_DIAGNOSE    =   7,  // executes built in self test
    RSP_O_GETSTATUS   =   8,  // (treated as a NOP by TU58)
    RSP_O_SETSTATUS   =   9,  //     "    "  "  "   "   "
    RSP_O_END         =  64,  // acknowledgement (end) packet type
    // TU58 Command Modifiers ...
    RSP_M_SAM         = 128,  // selects special addressing mode
    RSP_M_CHECK       =   1,  // decreased sensitivity (read command)
                              // read and verify (write command)
    // TU58 Switches Flags ...
    RSP_S_MRSP        =   8,  // MRSP mode ON
    RSP_S_MAINTENANCE =  16,  // maintenance (inhibit read retries) mode
    // TU58 Error Codes ...
    RSP_E_SUCCESS     =   0,  // operation completed without errors
    RSP_E_WRETRY      =   1,  // success, but retries were needed
    RSP_E_POSTFAIL    = 255,  // self test failed
    RSP_E_EOT         = 254,  // partial operation - end of tape
    RSP_E_BADUNIT     = 248,  // unit number not 0 or 1
    RSP_E_NOTAPE      = 247,  // no cartridge in drive
    RSP_E_WLOCK       = 245,  // tape is write protected
    RSP_E_DATACHECK   = 239,  // bad data (checksum failure)
    RSP_E_SEEKFAIL    = 224,  // block not found (bad tape format?)
    RSP_E_JAMMED      = 223,  // motor stopped (jammed cartridge?)
    RSP_E_BADOPCODE   = 208,  // bad opcode in command packet
    RSP_E_BADBLOCK    = 201,  // block number .GT. 511
  };
  // RSP Command packet structure...
  struct _RSP_COMMAND {
    uint8_t  bOpcode;         // operation code
    uint8_t  bModifier;       // modifier flags
    uint8_t  bUnit;           // drive unit number requested
    uint8_t  bSwitches;       // special mode selections
    uint16_t wSequence;       // sequence number
    uint16_t wCount;          // byte count
    uint16_t wBlock;          // logical block number
  };
  typedef struct _RSP_COMMAND RSP_COMMAND;
  // RSP Data packet structure ...
  struct _RSP_DATA {
    uint8_t bFlag;                  // what RSP calls the flag byte
    uint8_t bCount;                 // message length (NOT counting flag/count)
    uint8_t abData[RSP_RECORDSIZE]; // the actual data being sent
  };
  typedef struct _RSP_DATA RSP_DATA;
  // RSP State machine states ...
  enum _RSP_STATES {
    STA_POWERUP,              // initial power up state - send CONTINUE
    STA_IDLE,                 // waiting for a command fromt the host
    STA_BREAK,                // host is currently sending BREAK
    STA_INIT1,                // waiting for the first INIT command
    STA_INIT2,                //  "  "    "   "  second  "     "
    STA_RXCOMMAND,            // receiving a command packet
    STA_RXDATA,               //  "    "   " data      "
    STA_REQUESTDATA,          // send CONTINUE, ask for data packet
    STA_WAITDATA,             // CONTINUE sent, wait for data packet
    STA_TXEND1,               // start transmitting an END packet
    STA_TXEND2,               // finish  "     "    "   "     "
    STA_TXDATA1,              // start transmitting a data packet
    STA_TXDATA2,              // finish  "     "    "  "     "
    STA_RXBOOTSTRAP,          // BOOTSTRAP received, waiting for unit number
    STA_TXBOOTSTRAP,          // transmitting bootstrap packet
    STA_ERROR,                // protocol error - send INIT continuously
  };
  typedef enum _RSP_STATES RSP_STATE;

  // Constructor and destructor ...
public:
  CTU58 (uint8_t nUnits=4);
  virtual ~CTU58();
private:
  // Disallow copy and assignments!
  CTU58(const CTU58&) = delete;
  CTU58& operator= (CTU58 const &) = delete;

  // CTU58 device methods from CVirtualConsole ...
public:
  virtual int32_t RawRead (uint8_t *pabBuffer, size_t cbBuffer, uint32_t lTimeout=0) override;
  virtual void RawWrite (const char *pabBuffer, size_t cbBuffer) override;
  virtual void SendSerialBreak (bool fBreak) override;

  // Other public CTU58 methods ...
public:
  // Return the number of units supported ...
  uint8_t GetUnits() const {return m_nUnits;}
  // Attach the emulated drive to an image file ...
  bool Attach (uint8_t nUnit, const string &sFileName, bool fReadOnly=false, uint32_t lCapacity=0);
  void Detach (uint8_t nUnit);
  void DetachAll();
  // Display drive status for debugging ...
  void ShowDevice (ostringstream &ofs) const;
  // Return TRUE if the drive is attached (online) ...
  bool IsAttached (uint8_t nUnit) const
    {assert(nUnit < m_nUnits);  return m_Images[nUnit]->IsOpen();}
  // Return TRUE if the drive/file is read only (not supported) ...
  bool IsReadOnly (uint8_t nUnit) const
    {assert(nUnit < m_nUnits);  return m_Images[nUnit]->IsReadOnly();}
  // Return the external file that we're attached to ...
  string GetFileName (uint8_t nUnit) const
    {assert(nUnit < m_nUnits);  return m_Images[nUnit]->GetFileName();}
  // Return the capacity (in records!) of the specified drive ...
  uint32_t GetCapacity (uint8_t nUnit) const
    {assert(nUnit < m_nUnits);  return m_Images[nUnit]->GetCapacity();}

  // Private methods ...
protected:
  // Calculate a TU58 style checksum ...
  static uint16_t ComputeChecksum (const RSP_DATA *pPacket);
  // Convert an RSP state to a string ...
  static string StateToString (RSP_STATE nState);
  // Convert an RSP command code to a string ...
  static string CommandToString (uint8_t bCommand);
  // Receiver state machine ...
  void RxFromHost (uint8_t bData);
  void RxPacketStart (uint8_t bFlag);
  int32_t RxPacketData (uint8_t bData);
  // Transmitter state machine ...
  bool TxToHost (uint8_t &bData);
  void TxPacketStart (uint8_t bFlag, uint8_t bCount);
  bool TxPacketData (uint8_t &bData);
  void TxEndPacket (uint8_t bSuccess, uint16_t wCount=0);
  // Command and read/write packet processing ...
  void DoCommand();
  void WriteData();
  bool ReadData();
  bool CheckUnit (bool fWrite=false);

  // Private member data...
protected:
  uint8_t       m_nUnits;         // number of units on this drive
  RSP_STATE     m_nState;         // current state of the RSP protocol
  RSP_DATA      m_RSPbuffer;      // packet being sent or received
  RSP_COMMAND   m_RSPcommand;     // last command packet received
  uint8_t       m_cbRSPpacket;    // bytes sent or received in this packet
  uint8_t       m_bChecksumH;     // packet checksum, high order byte
  uint8_t       m_bChecksumL;     //   "      "   "   low    "    "
  uint16_t      m_cbTransfer;     // total bytes in this read or write
  uint16_t      m_wCurrentBlock;  // current TU58 logical block number
  uint16_t      m_cbSector;       // bytes used in current sector
  uint8_t m_abSector[RSP_BLOCKSIZE];    // image file sector buffer
  vector<CDiskImageFile *> m_Images;    // CDiskImageFiles one per unit
//vector<uint32_t>         m_Capacity;  // capacity of each drive, in sectors
};
