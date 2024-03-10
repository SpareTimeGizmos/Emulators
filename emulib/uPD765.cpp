//++
// uPD765.cpp -> NEC Floppy Diskette Controller emulation
//
//   COPYRIGHT (C) 2024 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
//
// LICENSE:
//    This file is part of the SBC1802 emulator project.  SBC1802 is free
// software; you may redistribute it and/or modify it under the terms of
// the GNU Affero General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any
// later version.
//
//    SBC1802 is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License
// for more details.  You should have received a copy of the GNU Affero General
// Public License along with SBC1802.  If not, see http://www.gnu.org/licenses/.
//
// DESCRIPTION:
//    This module provides the basis for the emulation of a floppy diskette
// interface that's based around the NEC uPD765 FDC chip.  "Basis" because this
// does not actually implement the standard CDevice interface. and that's because
// there are number of functions that a floppy diskette interface requires that
// the uPD765 doesn't do.  Instead, it's intended that whatever implementation
// specific floppy diskette controller emulation you implement will inherit the
// CUPD765 class.
// 
// DRIVE AND HEAD SELECTION, AND MOTOR CONTROL
//   The uPD765 outputs two bits to select the floppy diskette unit, and thus
// allows up to four drives to be connected.  It also outputs a head select bit
// to allow for double sided drives.  The way these bits are used varies a little
// depending on the specific floppy controller implementation, but here we assume
// they're used exactly as intended.
// 
//   We implement a maximum of four drives, which means four diskette image files,
// and the image file for I/O is selected by the unit select bits.  If the derived
// class has told us that we have double sided drives (see GEOMETRY, below) then
// we respect the head select bit as well.  Otherwise, for single sided drives,
// head select is ignored.
// 
//   Most floppy diskette interfaces provide some way to turn the drive motor on
// or off, but this module simply doesn't deal with motor control at all.  The
// uPD765 knows nothing about that, and it's entirely up to whatever inherits this
// class.
//
// REGISTER ACCESS AND DMA
//   Since this isn't a CDevice we don't have the DevRead() and DevWrite()
// methods, and we don't know anything about port numbers on the host system.
// Instead we have the ReadStatus(), ReadData() and WriteData() methods to
// access the uPD765 registers.  The derived class is responsible for calling
// these as needed when the host accesses the FDC registers.
// 
//   DMA transfers are implemented by this class calling the DMAread() and
// DMAwrite() methods.  Note that DMAread means data transfer from memory to the
// FDC, and DMAwrite is FDC to memory.  These are both defined here as NOP
// virtual functions, and the derived class must override them to implement DMA
// transfers.  More over, the uPD765 doesn't keep track of the number of bytes
// transferred by DMA either, and it's up to the logic outside the chip to tell
// it when to stop.  In this implementation that's done by calling StopDMA().
// 
//   If the host controller doesn't implement DMA, and the uPD765 does support
// programmed I/O for diskette data transfer, then the DMAread() and DMAwrite()
// methods can be left as NOPs.
// 
// DISKETTE GEOMETRY
//    The uPD765 supports many different diskette formats with differing sector
// sizes, number of sectors per track, tracks per diskette, single or double
// sided, etc.  The sector size is specified in the read or write command, but
// other things the FDC just tries to find the selected track, sector or head,
// and if it can't then the operation fails.
// 
//    That's a bit of a problem for us, since we need to know the diskette
// geometry in order to calculate the correct sector position in the image file.
// In most systems (but not all, I admit) the geometry is fixed by the particular
// floppy disk drives that are attached, and it's known in advance.  We implement
// the SetGeometry() method to allow the diskette controller implementation to
// tell us the physical geometry of the drive.  
// 
//     SetGeometry() has to be called before any file is attached to the unit,
// otherwise the operation will fail.  And note that we do allow for each unit
// to have its own unique geometry; all drives do not have to be the same.
//
// COMMANDS AND RESPONSES
//    The uPD765 can execute 15 different commands.  Most commands require one
// or more parameters, and most commands generate some kind of response.  This
// table, which was stolen from https://www.cpcwiki.eu/index.php/765_FDC, nicely
// summarizes the commands and responses.
// 
// Command     Parameters              Exm Result               Description	Command	Response
//                                                                              Format  Format
// ----------- ----------------------- --- -------------------- --------------- ------- --------
// 02+MF+SK    HU TR HD ?? SZ NM GP SL <R> S0 S1 S2 TR HD NM SZ READ TRACK	Type 1	Type 1
// 05+MT+MF    HU TR HD SC SZ LS GP SL <W> S0 S1 S2 TR HD LS SZ WRITE SECTOR	Type 1	Type 1
// 06+MT+MF+SK HU TR HD SC SZ LS GP SL <R> S0 S1 S2 TR HD LS SZ READ SECTOR	Type 1	Type 1
// 09+MT+MF    HU TR HD SC SZ LS GP SL <W> S0 S1 S2 TR HD LS SZ WRITE DELETED	Type 1	Type 1
// 0C+MT+MF+SK HU TR HD SC SZ LS GP SL <R> S0 S1 S2 TR HD LS SZ READ DELETED	Type 1	Type 1
// 11+MT+MF+SK HU TR HD SC SZ LS GP SL <W> S0 S1 S2 TR HD LS SZ SCAN EQUAL	Type 1	Type 1
// 19+MT+MF+SK HU TR HD SC SZ LS GP SL <W> S0 S1 S2 TR HD LS SZ SCAN .LE.	Type 1	Type 1
// 1D+MT+MF+SK HU TR HD SC SZ LS GP SL <W> S0 S1 S2 TR HD LS SZ SCAN .GE.	Type 1	Type 1
// 0D+MF       HU SZ NM GP FB          <W> S0 S1 S2 TR HD LS SZ FORMAT TRACK	Type 2	Type 1
// 0A+MF       HU                       -  S0 S1 S2 TR HD LS SZ READ ID		Type 3	Type 1
// 04          HU                       -  S3                   SENSE DRV STAT  Type 3	Type 2
// 07          HU                       -                       RECALIBRATE	Type 3  none
// 0F          HU TP                    -                       SEEK TRACK	Type 4	none
// 03          XX YY                    -                       SPECIFY HLD/DMA	Type 5  none
// 08          -                        -  S0 TP                SENSE INTERRUPT	none	Type 3
// 
// Parameter bits that can be specified in some commands are:
// 
//  MT  Bit7  Multi Track (continue multi-sector-function on other head)
//  MF  Bit6  MFM-Mode-Bit (Default 1=Double Density)
//  SK  Bit5  Skip-Bit (set if secs with deleted DAM shall be skipped)
// 
// Parameter/Result bytes are:
// 
//  HU  b0,1=Unit/Drive Number, b2=Physical Head Number, other bits zero
//  TP  Physical Track Number
//  TR  Track-ID (usually same value as TP)
//  HD  Head-ID
//  SC  First Sector-ID (sector you want to read)
//  SZ  Sector Size (80h shl n) (default=02h for 200h bytes)
//  LS  Last Sector-ID (should be same as SC when reading a single sector)
//  GP  Gap (default=2Ah except command 0D: default=52h)
//  SL  Sectorlen if SZ=0 (default=FFh)
//  Sn  Status Register 0..3
//  FB  Fillbyte (for the sector data areas) (default=E5h)
//  NM  Number of Sectors (default=09h)
//  XX  b0..3=headunload n*32ms (8" only), b4..7=steprate (16-n)*2ms
//  YY  b0=DMA_disable, b1-7=headload n*4ms (8" only)
//
// DELAYS AND TIMING
//   We make some attempt to model accurate diskette timing, and the timing
// parameters used are
//
//   Step Delay        -> track to track head stepper delay for seeking
//   Rotational Delay  -> average delay for a sector to pass under the head
//   Transfer Delay    -> delay beetween bytes when reading or writing
//   Head Load Delay   -> delay when loading the heads (currently unimplemented!)
//   Head Unload Delay ->   "     "  unloading "   "      "    "    "       "  !
//
// Floppies are unique in that the timing varies a lot depending on the specific
// drive, and the uPD765 actually provides a SPECIFY command that lets the host
// firmware specify the step delay and head load/unload timings.  That gives us
// a bit of a delimma - do we let our user specify these delays with a UI command,
// or do we obey the command given to the controller?  What we do is initialize
// these three delays to zero in the constructor and then, if the user hasn't
// changed them to non-zero values before the simulation starts running, we'll
// use what's contained in the SPECIFY command.
//
//   For the rotational delay and the transfer delay this is a non-issue, since
// they can't be changed by a controller command.  Note that the transfer delay is
// the time between bytes in MFM mode; the code will double this value if single
// density mode is selected.  The rotational delay is the average time needed for
// a sector to rotate past the heads; this is normally 1/2 the rotational speed
// of the diskette.  5-1/4" diskettes nominally rotate at 300 RPM (though some
// were 360 RPM instead); that's 5 RPS, or 200ms per revolution.  That makes the
// rotational delay 100ms.
//
//   The real uPD765 controller has an automatic head load/unload function.  When
// a command is issued to a drive, the heads are loaded first (assuming they are
// currently unloaded) and that incurs the head load delay.  After 5 seconds of
// no activity, the heads are automatically unloaded, which incurs the head unload
// delay.  This function is transparent to the software, except for the additional
// delays.  We don't currently emulate head load or unload functions, although we
// could.
//
// LIMITATIONS AND TODO LIST
//   * Programmed I/O transfers are NOT implemented
//   * Head load/unload is NOT implemented
//   * These commands are not yet implemented
//     READ DELETED, WRITE DELETED, READ SECTOR ID
//     READ TRACK, FORMAT TRACK
//     SCAN EQUAL, SCAN LESS OR EQUAL, SCAN GREATER OR EQUAL
//
// REVISION HISTORY:
//  5-MAR-24  RLA   New file.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include <string>               // C++ std::string class, et al ...
#include <iostream>             // C++ style output for LOGS() ...
#include <sstream>              // C++ std::stringstream, et al ...
#include <map>                  // C++ std::map template
#include "EMULIB.hpp"           // emulator library definitions
#include "LogFile.hpp"          // emulator library message logging facility
#include "Interrupt.hpp"        // interrupt simulation logic
#include "EventQueue.hpp"       // I/O device event queue simulation
#include "Memory.hpp"           // basic memory emulation declarations ...
#include "Device.hpp"           // basic I/O device emulation declarations ...
#include "DeviceMap.hpp"        // CDeviceMap declaration
#include "CPU.hpp"              // CCPU base class definitions
#include "uPD765.hpp"           // declarations for this module


////////////////////////////////////////////////////////////////////////////////
////////////////////////   C O M M O N   M E T H O D S   ///////////////////////
////////////////////////////////////////////////////////////////////////////////

CUPD765::CUPD765 (CEventQueue *pEvents)
{
  //++
  //   The constructor initializes any member variables AND also creates the
  // default I/O group.  The default group always exists, even if there are
  // no devices in it or if UPD765 is disabled ...
  //--
  m_pEvents = pEvents;

  // Allocate the CDiskImageFile objects ...
  for (uint8_t i = 0; i < MAXUNIT; ++i)
    m_apImages[i] = DBGNEW CDiskImageFile(SECTORSIZE);

  //  Initialize the step delay, and head load/unload delays to zero (see the
  // discussion above under "DELAYS"...), but initialize the transfer and the
  // rotational delays to reasonable values.
  m_llStepDelay = m_llHeadLoadDelay = m_llHeadUnloadDelay = 0;
  m_llTransferDelay = TRANSFER_DELAY;  m_llRotationalDelay = ROTATIONAL_DELAY;

  // Initialize all the FDC internal registers and state ...
  ResetFDC();
}

CUPD765::~CUPD765()
{
  //++
  // Detach and delete all drives ... ...
  //--
  DetachAll();
  for (uint8_t i = 0; i < MAXUNIT; ++i)  delete m_apImages[i];
}

void CUPD765::ResetFDC()
{
  //++
  // Initialize all FDC registers ...
  //--
  m_State = ST_IDLE;
  memset(m_abST, 0, sizeof(m_abST));
  memset(m_abCommand, 0, sizeof(m_abCommand));
  memset(m_abResult, 0, sizeof(m_abResult));
  memset(m_afWriteLock, 0, sizeof(m_afWriteLock));
  memset(m_abCurrentTrack, 0, sizeof(m_abCurrentTrack));
  memset(m_afBusySeeking, 0, sizeof(m_afBusySeeking));
  memset(m_abBuffer, 0, sizeof(m_abBuffer));
  m_bMainStatus = 0;  m_nCurrentByte = 0;
  m_nCommandLength = m_nResultLength = 0;  m_nDataLength = 0;
  m_bCurrentUnit = m_bCurrentHead = m_bCurrentSector = 0;
  m_bSizeCode = 0; m_fNoDMAmode = false;
}

void CUPD765::EventCallback (intptr_t lParam)
{
  //++
  //   This routine is called by the CEventQueue object whenever it's time for
  // a scheduled event to occur.  In the CUPD765 class, we use these to simulate
  // seek delays, rotational delays (average access time) and maybe someday head
  // load and unload delays.  All we do here is to figure out which one of these
  // happened and then call the appropriate routine.
  //--
  if (lParam == EVENT_READ_DATA) {
    ReadTransfer();
  } else if (lParam == EVENT_WRITE_DATA) {
    WriteTransfer();
  } else if ((lParam >= EVENT_SEEK_DONE) && (lParam < EVENT_SEEK_DONE+MAXUNIT))
    SeekDone(lParam-EVENT_SEEK_DONE);
  else
    assert(false);
}

string CUPD765::StateToString (FDCSTATE nState) const
{
  //++
  // Convert a FDC state to a human readable string (for debugging) ...
  //--
  switch (nState) {
    case ST_IDLE:         return "IDLE";
    case ST_READ_COMMAND: return "READ COMMAND";
    case ST_BUSY:         return "BUSY";
    case ST_SEND_RESULT:  return "SEND RESULT";
    case ST_READ_DATA:    return "READ DATA";
    case ST_SEND_DATA:    return "SEND DATA";
    default:              return "UNKNOWN";
  }
}

void CUPD765::SetGeometry (uint8_t nUnit, uint16_t nSectorSize, uint16_t nSectors, uint16_t nTracks, uint16_t nHeads)
{
  //++
  // Set the geometry for the specified diskette drive ...
  //--
  assert((nUnit < MAXUNIT) && (m_apImages[nUnit] != NULL));
  m_apImages[nUnit]->SetHeads(nHeads);
  m_apImages[nUnit]->SetCylinders(nTracks);
  m_apImages[nUnit]->SetSectors(nSectors);
  m_apImages[nUnit]->SetSectorSize(nSectorSize);
}

void CUPD765::GetGeometry (uint8_t nUnit, uint16_t &nSectorSize, uint16_t &nSectors, uint16_t &nTracks, uint16_t &nHeads) const
{
  //++
  // Return the geometry set for a particular drive ...
  //--
  assert((nUnit < MAXUNIT) && (m_apImages[nUnit] != NULL));
  nHeads      = m_apImages[nUnit]->GetHeads();
  nTracks     = m_apImages[nUnit]->GetCylinders();
  nSectors    = m_apImages[nUnit]->GetSectors();
  nSectorSize = m_apImages[nUnit]->GetSectorSize();
}

uint8_t CUPD765::CommandLength (uint8_t bCommand)
{
  //++
  // Return the number of bytes expected to follow this command ...
  //--
  switch (bCommand & CMD_MASK) {
    case CMD_READ_TRACK:
    case CMD_READ_SECTOR:
    case CMD_WRITE_SECTOR:
    case CMD_READ_DELETED:
    case CMD_WRITE_DELETED:
    case CMD_SCAN_EQUAL:
    case CMD_SCAN_LE:
    case CMD_SCAN_GE:
      return sizeof(CMD_TYPE1);
    case CMD_FORMAT_TRACK:
      return sizeof(CMD_TYPE2);
    case CMD_READ_SECTOR_ID:
    case CMD_RECALIBRATE:
    case CMD_DRIVE_STATE:
      return sizeof(CMD_TYPE3);
    case CMD_SEEK:
      return sizeof(CMD_TYPE4);
    case CMD_SPECIFY:
      return sizeof(CMD_TYPE5);
    case CMD_SENSE_INT:
    default:
      return 1;
  }
}


////////////////////////////////////////////////////////////////////////////////
/////////////   S T A T U S   A N D   R E S U L T   M E T H O D S   ////////////
////////////////////////////////////////////////////////////////////////////////

uint8_t CUPD765::UpdateST0 (uint8_t bInterruptCode)
{
  //++
  //   This routine will update the secondary status register 0.  The main
  // thing here is the interrupt code, which gives the success or failure
  // status of the last command.  The other bits, like SEEK END, NOT READY,
  // etc we figure out here based on other status flags...
  //--
   uint8_t bStatus = bInterruptCode;

  // If the selected drive isn't attached, set the NOT READY bit ...
  if (!IsAttached()) SETBIT(bStatus, ST0_NOTREADY);

  // Set the last accessed head and unit ...
  if (m_bCurrentHead != 0) SETBIT(bStatus, ST0_HEAD_SELECT);
  SETBIT(bStatus, (m_bCurrentUnit & 3));

  // All done ...
  m_abST[0] = bStatus;  return bStatus;
}

uint8_t CUPD765::UpdateST3()
{
  //++
  //   This routine will update the secondary status register 3.  All these
  // bits are derived directly from state information that we already have.
  //--
  uint8_t bStatus = m_bCurrentUnit & 3;
  if (m_bCurrentHead != 0)  SETBIT(bStatus, ST3_HEAD_SELECT);
  if (CurrentHeads() > 1)   SETBIT(bStatus, ST3_DOUBLE_SIDED);
  if (CurrentTrack() == 0)  SETBIT(bStatus, ST3_TRACK_0);
  if (IsWriteLocked())      SETBIT(bStatus, ST3_WRT_PROTECT);
  if (IsAttached())         SETBIT(bStatus, ST3_READY);
  m_abST[3] = bStatus;  return bStatus;
}

uint8_t CUPD765::ReadStatus()
{
  //++
  //   This routine is called to read the main status register.  All these
  // bits are computed dynamically based on our current state, especially
  // the DATA OUT and DATA REQUEST bits.
  //--
  uint8_t bStatus = 0;

  // Set the busy bit for drive 0..3 as required ...
  if (m_afBusySeeking[0]) SETBIT(bStatus, STS_FDD0_BUSY);
  if (m_afBusySeeking[1]) SETBIT(bStatus, STS_FDD1_BUSY);
  if (m_afBusySeeking[2]) SETBIT(bStatus, STS_FDD2_BUSY);
  if (m_afBusySeeking[3]) SETBIT(bStatus, STS_FDD3_BUSY);

  // The FDC is busy if we're not idle...  At least I think so!
  if (m_State != ST_IDLE) SETBIT(bStatus, STS_FDC_BUSY);

  // SET NONDMA bit TBA NYI!!

  // The DATA_OUT and DATA_REQUEST bits depend on the current FDC state ...
  switch (m_State) {
    case ST_IDLE:
    case ST_READ_COMMAND:
      SETBIT(bStatus, STS_DATA_REQUEST);               break;
    case ST_SEND_RESULT:
      SETBIT(bStatus, STS_DATA_OUT|STS_DATA_REQUEST);  break;
    case ST_READ_DATA:
    case ST_SEND_DATA:
    default:
      break;
  }

  // That's it!
  m_bMainStatus = bStatus;  return m_bMainStatus;
}

void CUPD765::SendResult (uint8_t bLength)
{
  //++
  //   This method initiates sending a result packet back to the host.  The
  // caller is expected to have already stored the results in m_abResults[].
  // We set up the length and the current byte.  The next state is always
  // SEND RESULT and this always causes an interrupt (because we have data
  // to send to the host!).
  //--
  m_nResultLength = bLength;  m_nCurrentByte = 0;
  FDCinterrupt();  NextState(ST_SEND_RESULT);
}

void CUPD765::SendResultType1()
{
  //++
  //   This routine will fill in and send a "type 1" result packet.  This type
  // is used after most read or write commands such as READ SECTOR, WRITE
  // SECTOR, READ TRACK, etc.  Note that before calling this routine you should
  // call UpdateST0() to set the correct success or failure result code.  Also
  // ST1 and ST2 may need to be updated; in particular ST1 contains END OF
  // CYLINDER and OVERRUN bits that may be relevant, and ST2 contains the BAD
  // CYLINDER bit for seek errors.
  //--
  RST_TYPE1 *pResult = (RST_TYPE1 *) &m_abResult;
  pResult->bST0         = m_abST[0];
  pResult->bST1         = m_abST[1];
  pResult->nST2         = m_abST[2];
  pResult->bTrack       = CurrentTrack();
  pResult->bHead        = m_bCurrentHead;
  pResult->bLastSector  = m_bCurrentSector;
  pResult->bSectorSize  = m_bSizeCode;
  SendResult(sizeof(RST_TYPE1));
}


////////////////////////////////////////////////////////////////////////////////
////////////////////////   S I M P L E   C O M M A N D S  //////////////////////
////////////////////////////////////////////////////////////////////////////////

void CUPD765::DoInvalid (uint8_t bCommand)
{
  //++
  //   This method handles all invalid uPD765 commands.  This sends a single
  // byte result packet containing just ST0, which in turn contains the INVALID
  // COMMAND interrupt code.  This result DOES NOT generate an interrupt request!
  //--
  LOGF(DEBUG, "uPD765 INVALID COMMAND 0x%02X", bCommand);
  UpdateST0(ST0_IC_INVCMD);
  m_abResult[0] = m_abST[0];  m_nResultLength = 1;  NextState(ST_SEND_RESULT);
}

void CUPD765::DoSpecify (CMD_TYPE5 *pCommand)
{
  //++
  //   Handle the uPD765 SPECIFY command.  This specifies (what else) drive
  // parameters such as the head load time, head unload time, and track to
  // track step rate.  It also selects between DMA and programmed I/O (NODMA)
  // mode for future transfers
  // 
  //   If the our operator hasn't used the UI to specify valus for the step
  // delay or head load/unload time, then we'll actually use these values in
  // our future simulation.  But if the operator has specified values, then
  // we just ignore this.
  // 
  //   Note that this command sends no result packet and doesn't generate an
  // interrupt!
  //--
  uint64_t llStepDelay       = MSTONS(0x10 - ((pCommand->bStepRate >> 4) & 0x0F));
  uint64_t llHeadUnloadDelay = MSTONS((( pCommand->bStepRate       & 0x0F) + 1) * 16);
  uint64_t llHeadLoadDelay   = MSTONS((((pCommand->bHeadLoad >> 1) & 0x7F) + 1) *  2);
  m_fNoDMAmode = (pCommand->bHeadLoad & 0x01) != 0;
  LOGF(DEBUG, "uPD765 SPECIFY SRT=%lld ms, HUT=%lld ms, HLT=%lld ms, NO DMA=%d",
    NSTOMS(llStepDelay), NSTOMS(llHeadUnloadDelay), NSTOMS(llHeadLoadDelay), m_fNoDMAmode);
  if (m_llStepDelay       == 0) m_llStepDelay       = llStepDelay;
  if (m_llHeadLoadDelay   == 0) m_llHeadLoadDelay   = llHeadLoadDelay;
  if (m_llHeadUnloadDelay == 0) m_llHeadUnloadDelay = llHeadUnloadDelay;
  NextState(ST_IDLE);
}

void CUPD765::DoSenseInterrupt()
{
  //++
  //   The SENSE INTERRUPT command is used to ask the FDC why it interrupted.
  // Some commands, notable SEEK and RECALIBRATE, generate interrupts but don't
  // send result packets, and this is the official way to find out what happened.
  // It can also be used after other commands that do generate results, such as
  // READ or WRITE, but it's less useful then since you get the result packet
  // for free.
  //--
  RST_TYPE3 *pResult = (RST_TYPE3 *) & m_abResult;
  pResult->bST0         = m_abST[0];
  pResult->bTrackNumber = CurrentTrack();
  LOGF(DEBUG, "uPD765 SENSE INTERRUPT ST0=0x%02X, track=%d", pResult->bST0, pResult->bTrackNumber);
  SendResult(sizeof(RST_TYPE3));
}

void CUPD765::DoSenseDriveStatus (CMD_TYPE3 *pCommand)
{
  //++
  //   The SENSE DRIVE STATUS command returns the ST3 status word for the
  // selected drive.  This tells us things like, is the drive online, is it
  // write protected, is it double sided, etc.  FWIW, this is the ONLY
  // command that returns the ST3 status byte.
  //--
  RST_TYPE2 *pResult = (RST_TYPE2 *) &m_abResult;
  m_bCurrentUnit = pCommand->bHeadUnit & 0x3;
  m_bCurrentHead = (pCommand->bHeadUnit >> 2) & 0x1;
  pResult->bST3 = UpdateST3();
  LOGF(DEBUG, "uPD765 SENSE DRIVE STATUS unit=%d, ST3=0x%02X", m_bCurrentUnit, pResult->bST3);
  SendResult(sizeof(RST_TYPE2));
}


////////////////////////////////////////////////////////////////////////////////
//////////////////   S E E K   A N D   R E C A L I B R A T E   /////////////////
////////////////////////////////////////////////////////////////////////////////

void CUPD765::SeekTrack (uint8_t bTrack)
{
  //++
  //    This routine will start the current unit seeking to the specified track.
  // The uPD765 allows overlapped seeks, so this just starts the operation and
  // schedules an event callback for when we think the seek should be complete.
  //--
  if (!IsAttached()) {
    // If the unit isn't attached, then quit now with an ABNORMAL status ...
    UpdateST0(ST0_IC_ABNORMAL);
  } else {
    //   Update the current track (as if the drive were already finished!) and
    // set the drive status to be busy seeking.
    uint8_t bSteps = MAX(bTrack, CurrentTrack()) - MIN(bTrack, CurrentTrack());
    m_abCurrentTrack[m_bCurrentUnit] = bTrack;
    m_afBusySeeking[m_bCurrentUnit] = true;
    //   Now schedule a callback for when the drive should be finished.  This
    // requires us to calculate a) the number of steps required, either in or
    // out, and then b) multiply by the head step delay.
    uint64_t llDelay = bSteps * m_llStepDelay;
    m_pEvents->Schedule(this, EVENT_SEEK_DONE+m_bCurrentUnit, llDelay);
  }
  //   Since the uPD765 allows overlapped seek commands, so the next controller
  // state is IDLE so that we can accept a new command, BUT we don't interrupt
  // until the seek is completed in SeekDone()...
  NextState(ST_IDLE);
}

void CUPD765::SeekDone (uint8_t bUnit)
{
  //++
  //   This routine is invoked by the EventCallback() when a floppy unit has
  // finished seeking.  This causes an interrupt to let the host know we're
  // done, but it doesn't send any result packet.  The host is expected to send
  // us a SENSE INTERRUPT command to ask why we interrupted, and then he'll
  // see the SEEK DONE bit and the unit number.
  //--
  m_bCurrentUnit = bUnit;
  m_afBusySeeking[m_bCurrentUnit] = false;
  UpdateST0(ST0_IC_NORMAL | ST0_SEEK_END);
  FDCinterrupt();
}

void CUPD765::DoRecalibrate (CMD_TYPE3 *pCommand)
{
  //++
  //   This handles the RECALIBRATE command.  We treat this just as we would
  // a SEEK to track 0 ...
  //--
  m_bCurrentUnit = pCommand->bHeadUnit & 0x3;
  LOGF(DEBUG, "uPD765 RECALIBRATE unit=%d", m_bCurrentUnit);
  SeekTrack(0);
}

void CUPD765::DoSeek (CMD_TYPE4 *pCommand)
{
  //++
  //   And this handles the SEEK command which will (what else?) start the
  // selected unit seeking to the track specifed.  The seek operation will
  // finish later, after an appropriate amount of time has passed.
  //--
  m_bCurrentUnit =  pCommand->bHeadUnit      & 0x3;
  m_bCurrentHead = (pCommand->bHeadUnit >> 2) & 0x1;
  LOGF(DEBUG, "uPD765 SEEK unit=%d, new track=%d, head=%d",
    m_bCurrentUnit, pCommand->bTrackNumber, m_bCurrentHead);
  SeekTrack(pCommand->bTrackNumber);
}


////////////////////////////////////////////////////////////////////////////////
///////////////  R E A D   A N D   W R I T E   C O M M A N D S   ///////////////
////////////////////////////////////////////////////////////////////////////////

//++
//   The flow for all commands that transfer data is pretty complicated.  Here
// is a crude chart to help illustrate what happens.
//
//            READING DATA                              WRITING DATA
//    -----------------------------            -----------------------------
//          Command                                   Command
//           Setup                                     Setup
//             |                                         |
//             v                                         v
//         Head Load                                 Head Load
//           Delay                                     Delay
//             |                                         |
//             v                                         v
//         Rotational<----------<                    Rotational<----------<
//           Delay              |                      Delay              |
//             |                |                        |                |
//             v                |                        |<---------<     |
//            Read              |                        |          |     |
//           Sector             |                        v       Transfer |
//             |                |                     Transfer    Delay   |
//             |<---------<     |                       Byte -------^     |
//             |          |     |                        |    More?       |
//             v       Transfer |                        v                |
//          Transfer    Delay   |                      Write              |
//            Byte -------^     |                      Sector             |
//             |    More?       |                        |                |
//             v                |                        v                |
//           Sector             |                      Sector             |
//          Finished -----------^                     Finished -----------^
//             |       More?                             |       More?
//             |                                         |
//             v                                         v
//            Send                                      Send
//           Result                                    Result
//
//   Every "delay" in these flowcharts requires an event callback, and the
// code has to be split up to allow CPU emulation to continue while we're
// waiting for the delay to expire.  We can manage to get by with just two
// routines each, for reading and writing, in all -
//
//  #1 there's a command setup routine - DoReadSector(), DoWriteSector(), etc -
// and this cheats a bit by scheduling an EVENT_READ_DATA or EVENT_WRITE_DATA
// with the combined head load delay and rotational delay for the first time
// around.  
//
//  #2 there's a data transfer routine - DoReadData(), DoWriteData(), etc -
// that handles reading/writing a sector, and transferring one byte.  After
// each byte it schedules another EVENT_READ_DATA or EVENT_WRITE_DATA with
// the transfer delay.  When we finish transferring one sector it schedules
// the same, but with rotational delay instead.  And lastly, when we finish
// transferring all sectors, this routine sends the result packet and
// doesn't schedule any more events.
//--

bool CUPD765::SetupCommandType1 (CMD_TYPE1 *pCommand, bool fWrite)
{
  //++
  //    This routine will extract all the relevant information from a "type 1"
  // command packet and transfer it to member variables - m_bCurrentUnit,
  // m_bCurrentSector, etc.  It also does a lot of checking on the parameters
  // to verify they're what we expect and know how to deal with.  If there is
  // anything wrong with the command parameters it will set an appropriate 
  // code in the status register(s) and return false.
  //--

  //   ST1 and ST2 sometimes contain error bits, but their correct state,
  // assuming nothing goes wrong, is all zeros.
  m_abST[1] = m_abST[2] = 0;

  //   Extract the selected unit and head.  The command actually contains TWO
  // head select fields - a bit in the Head/Unit byte, and a separate byte for
  // the head alone.  I've no idea why there are two copies or what they mean,
  // but we'll warn if they differ.
  m_bCurrentUnit = pCommand->bHeadUnit & 3;
  m_bCurrentHead = (pCommand->bHeadUnit >> 2) & 1;
  if (m_bCurrentHead != pCommand->bHeadSelect)
    LOGS(WARNING, "uPD765 type 1 command Head/Unit and HeadSelect disagree");

  // If the selected drive isn't online, then quit now ...
  if (!IsAttached()) {
    UpdateST0(ST0_IC_ABNORMAL | ST0_NOTREADY);  return false;
  }

  //   If this is a disk write operation and the selected drive is write locked,
  // then return the NOT WRITABLE error and quit.
  if (fWrite && IsWriteLocked()) {
    m_abST[1] = ST1_WRT_PROTECT;  UpdateST0(ST0_IC_ABNORMAL);  return false;
  }

  //   If the track specified in the command doesn't agree with the current
  // head position for this drive, then set the BAD CYLINDER bit in ST2 and
  // give up ...
  if (CurrentTrack() != pCommand->bTrackNumber) {
    UpdateST0(ST0_IC_ABNORMAL);  m_abST[2] = ST2_BAD_CYLINDER;
    return false;
  }

  //   Verify that the starting sector number is legal, and that the EndofTrack
  // agrees with what we believe to be the diskette geometry ...  Note that we
  // don't currently generate error packets for these, although maybe we should.
  if ((pCommand->bSectorNumber == 0) || (pCommand->bSectorNumber > CurrentSectors()))
    LOGF(WARNING, "uPD765 type 1 command sector %d disagrees with geometry", pCommand->bSectorNumber);
  m_bCurrentSector = pCommand->bSectorNumber;
  if (pCommand->bEndofTrack != CurrentSectors())
    LOGF(WARNING, "uPD765 type 1 command track length %d disagrees with geometry", pCommand->bEndofTrack);

  //   We don't implement partial sector transfers, so the SizeCode can't be
  // zero and the DataLength is ignored.  Decode the SizeCode and verify that
  // it agrees with the geometry we expect.  This doesn't generate an error
  // result either, but maybe it should.
  if ((pCommand->bSizeCode == 0) || (pCommand->bSizeCode > 3))
    LOGF(WARNING, "uPD765 type 1 command size code %d invalid", pCommand->bSizeCode);
  m_bSizeCode = pCommand->bSizeCode;
  uint16_t wSize = 128 * (1 << m_bSizeCode);
  if (wSize != CurrentSectorSize())
    LOGF(WARNING, "uPD765 type 1 command sector size %d disagrees with geometry", wSize);

  // That's it - we're ready to go!
  return true;
}

void CUPD765::DoReadSector (CMD_TYPE1 *pCommand)
{
  //++
  //   The READ SECTOR command transfers one or more sectors of data from the
  // diskette to the host.  This routine is part 1 of that process - it sets
  // up the command parameters (selected unit, track, starting sector, etc)
  // and then schedules an event callback after an appropriate delay.
  // 
  //    Ideally the delay would be calculated as both the head load time plus
  // the average rotational delay, but at the moment we don't bother simulating
  // head loading or unloading, so we just do the rotational delay and wash
  // our hands of it.
  //--
  if (!SetupCommandType1(pCommand)) {
    //   There's some problem with the command parameters.  Just send the
    // result packet now and then quit.
    SendResultType1();
  } else {
    // All's well - schedule the data transfer next ...
    m_nDataLength = CurrentSectorSize();  m_nCurrentByte = m_nDataLength+1;
    m_pEvents->Schedule(this, EVENT_READ_DATA, m_llRotationalDelay);
  }
}

void CUPD765::ReadTransfer()
{
  //++
  //    This routine is called by EventCallback() while reading to transfer
  // one byte from the floppy diskette to the host.  Transfers may occur either
  // via DMA or by programmed I/O thru the data register.  A critical point
  // to remember is that we don't know how many bytes or even how many sectors
  // we're supposed to transfer.  We just keep readinguntil the host tells us
  // to stop by calling the TerminalCount() method.
  // 
  //   In the case of a DMA transfer, it's the DMAwrite() which should call
  // TerminalCount().  TerminalCount will, in turn, change our state to IDLE
  // which we'll detect here and then send the result packet signalling the
  // end of the operation.  In the case of programmed I/O, the host firmware
  // must assert TerminalCount() thru some I/O port on the floppy interface
  // board, and after that the result is the same.
  // 
  //   In either case, before it exits this routine always needs to schedule
  // another event callback for the next transfer.  If we're in the middle of a
  // sector then callback delay is the m_llTransferDelay, but if we need to
  // read another sector next time then the delay is the average rotational
  // delay.
  //--
  uint64_t llDelay = 0;

  //   If the host aborted the transfer while we were away, then quit (with a
  // success result) now ...
  if (m_State != ST_BUSY) goto success;

  // If the current sector buffer is empty, then read one from the disk.
  if (m_nCurrentByte >= m_nDataLength) {
    if (!CurrentImage()->ReadSector(CurrentTrack(), m_bCurrentHead, m_bCurrentSector, m_abBuffer)) {
      LOGF(WARNING, "uPD765 error reading %s", CurrentImage()->GetFileName().c_str());
      // Fake a disk read (CRC) error and abort this transfer ...
      m_abST[1] = ST1_DATA_ERROR;  goto abort;
    }
    LOGF(DEBUG, "uPD765 reading sector C/H/S = %d/%d/%d size %d",
      CurrentTrack(), m_bCurrentHead, m_bCurrentSector, CurrentSectorSize());
    m_nDataLength = CurrentSectorSize();  m_nCurrentByte = 0;
  }

  // Fetch the next byte and transfer it to the host ...
  if (IsDMAmode()) {
    // DMA transfer mode ...
    DMAwrite(m_abBuffer[m_nCurrentByte]);
  } else {
    // PROGRAMMED I/O MODE NOT IMPLEMENTED YET!!!
    LOGF(WARNING, "uPD765 programmed I/O mode not implemented");
    goto abort;
  }
  ++m_nCurrentByte;


  // If the host wants to stop, then send a success result packet and quit ...
  if (m_State != ST_BUSY) goto success;

  //   If we've reached the end of the data buffer, then reset the buffer
  // pointers to empty and advance to the next sector ...
  if (m_nCurrentByte >= m_nDataLength) {
    // Increment the sector number, and wrap around at the end ...
    if (++m_bCurrentSector > CurrentSectors()) {
      //   When we reach the last sector of the track we stop, regardless of
      // the TerminalCount() input, UNLESS the MultiTrack bit is set in the
      // command.  In that case we flip to the other head and continue reading
      // from sector 1, UNLESS of course we've already been here and flipped
      // heads once before.
      m_bCurrentSector = 1;
      if (!IsMultiTrack() || (CurrentHeads() == 1)) {
        m_abST[1] = ST1_END_OF_CYL;  goto success;
      }
      m_bCurrentHead ^= 1;
      if (m_bCurrentHead == ((CMD_TYPE1 *) (&m_abCommand))->bHeadSelect) {
        m_abST[1] = ST1_END_OF_CYL;  goto success;
      }
    }
  }

  //   Now we know we need to schedule another DataTransfer() event, but the
  // question is, when?  If we need to read the next sector (true if the current
  // sector buffer is empty) then it's a long delay, otherwise it's a short one.
  // Note that the TransferDelay is calculated for MFM drives.  If this is a
  // single density drive then that delay is doubled.
  llDelay = (m_nCurrentByte == 0) ? m_llRotationalDelay
          : IsMFM() ? m_llTransferDelay : m_llTransferDelay * 2;
  m_pEvents->Schedule(this, EVENT_READ_DATA, llDelay);
  return;

  // Here if the transfer is finished, and all is well!
success:
  UpdateST0(ST0_IC_NORMAL);  SendResultType1();  return;

  // Here if the transfer failed, and it's bad news ...
abort:
  UpdateST0(ST0_IC_ABNORMAL);  SendResultType1();  return;
}

void CUPD765::DoWriteSector (CMD_TYPE1 *pCommand)
{
  //++
  //   The write SECTOR command transfers one or more sectors of data from the
  // host to the diskette.  This routine is part 1 of that process and pretty
  // much parallels DoReadSector().
  //--
  if (!SetupCommandType1(pCommand, true)) {
    //   There's some problem with the command parameters.  Just send the
    // result packet now and then quit.
    SendResultType1();
  } else {
    // All's well - schedule the data transfer next ...
    m_nDataLength = CurrentSectorSize();  m_nCurrentByte = 0;
    m_pEvents->Schedule(this, EVENT_WRITE_DATA, m_llRotationalDelay);
  }
}

void CUPD765::WriteTransfer()
{
  //++
  //    This routine is called by EventCallback() while writing to transfer
  // one byte from the host to the floppy diskette.  Except for minor changes
  // in the order of operations (here we fill the buffer first, then write to
  // the disk) it parallels ReadTransfer().
  //--
  uint64_t llDelay = 0;

  // WHAT DO DO HERE IF THE HOST CALLS TerminalCOunt() WHILE WE WERE AWAY!?!?

  // Transfer one byte and add it to the sector buffer ...
  if (IsDMAmode()) {
    // DMA transfer mode ...
    m_abBuffer[m_nCurrentByte] = DMAread();
  } else {
    // PROGRAMMED I/O MODE NOT IMPLEMENTED YET!!!
    LOGF(WARNING, "uPD765 programmed I/O mode not implemented");
    UpdateST0(ST0_IC_ABNORMAL);  goto abort;
  }
  ++m_nCurrentByte;

  // If the current sector buffer is full, then write it to the disk.
  if (m_nCurrentByte >= m_nDataLength) {
    if (!CurrentImage()->WriteSector(CurrentTrack(), m_bCurrentHead, m_bCurrentSector, m_abBuffer)) {
      LOGF(WARNING, "uPD765 error writing %s", CurrentImage()->GetFileName().c_str());
      // Fake an equipment check error and abort this transfer ...
      UpdateST0(ST0_IC_ABNORMAL|ST0_UNIT_CHECK); goto abort;
    }
    LOGF(DEBUG, "uPD765 writing sector C/H/S = %d/%d/%d size %d",
      CurrentTrack(), m_bCurrentHead, m_bCurrentSector, CurrentSectorSize());

    // Now advance to the next sector ...
    m_nDataLength = CurrentSectorSize();  m_nCurrentByte = 0;
    if (++m_bCurrentSector > CurrentSectors()) {
      m_bCurrentSector = 1;
      if (!IsMultiTrack() || (CurrentHeads() == 1)) {
        m_abST[1] = ST1_END_OF_CYL;  goto success;
      }
      m_bCurrentHead ^= 1;
      if (m_bCurrentHead == ((CMD_TYPE1 *) (&m_abCommand))->bHeadSelect) {
        m_abST[1] = ST1_END_OF_CYL;  goto success;
      }
    }
  }

  // If the host wants to stop, then send a success result packet and quit ...
  if (m_State != ST_BUSY) goto success;

  // Schedule the next data transfer event ...
  llDelay = (m_nCurrentByte == 0) ? m_llRotationalDelay
          : IsMFM() ? m_llTransferDelay : m_llTransferDelay * 2;
  m_pEvents->Schedule(this, EVENT_WRITE_DATA, llDelay);
  return;

  // Here if the transfer is finished, and all is well!
success:
  UpdateST0(ST0_IC_NORMAL);  SendResultType1();  return;

  // Here if the transfer failed, and it's bad news ...
abort:
  SendResultType1();  return;
}


////////////////////////////////////////////////////////////////////////////////
///////////   F D C   R E G I S T E R   A C C E S S   M E T H O D S   //////////
////////////////////////////////////////////////////////////////////////////////

void CUPD765::DoCommand()
{
  //++
  //   Decode and execute the command currently in m_abCommand[] (or at least
  // the first byte of it!).
  //--
  NextState(ST_BUSY);
  switch (CurrentCommand()) {
    case CMD_SPECIFY:       DoSpecify         ((CMD_TYPE5 *) &m_abCommand);  break;
    case CMD_RECALIBRATE:   DoRecalibrate     ((CMD_TYPE3 *) &m_abCommand);  break;
    case CMD_SEEK:          DoSeek            ((CMD_TYPE4 *) &m_abCommand);  break;
    case CMD_SENSE_INT:     DoSenseInterrupt  (                          );  break;
    case CMD_READ_SECTOR:   DoReadSector      ((CMD_TYPE1 *) &m_abCommand);  break;
    case CMD_WRITE_SECTOR:  DoWriteSector     ((CMD_TYPE1 *) &m_abCommand);  break;
    case CMD_DRIVE_STATE:   DoSenseDriveStatus((CMD_TYPE3 *) &m_abCommand);  break;
    // Currently unimplemented commands ...
    case CMD_READ_TRACK:
    case CMD_READ_DELETED:
    case CMD_WRITE_DELETED:
    case CMD_READ_SECTOR_ID:
    case CMD_FORMAT_TRACK:
    case CMD_SCAN_EQUAL:
    case CMD_SCAN_LE:
    case CMD_SCAN_GE:
    default:                DoInvalid(m_abCommand[0]);  break;
  }
}

uint8_t CUPD765::ReadData()
{
  //++
  //   This routine is called when the host emulation wants to read from the
  // uPD765 data register.  If we're in the process of sending a result, then
  // continue that.  When we send the last byte in the result, change the state
  // to idle.  If we're in the process of sending data, then send the next byte
  // from the data buffer.
  // 
  // Note that reading the data register ALWAYS clears any interrupt request!
  //--
  uint8_t bData = 0xFF;
  if (m_State == ST_SEND_RESULT) {
    // Send the next byte from the response buffer ...
    bData = m_abResult[m_nCurrentByte++];
    if (m_nCurrentByte == m_nResultLength) NextState(ST_IDLE);
  } else if (m_State == ST_SEND_DATA) {
    // Send the next byte from the data buffer ..
    // PROGRAMMED I/O NOT YET IMPLEMENTED!.
  }
  FDCinterrupt(false);
  return bData;
}

void CUPD765::WriteData (uint8_t bData)
{
  //++
  //    This routine is called when the host emulation wants to write to the
  // uPD765 data register.  If we're currently idle, then this must be the
  // start of a command and we need to start storing that in the m_abCommand
  // buffer.  If we're in the middle of reading a command, then this is the
  // next byte in that packet.  If this is the last byte of the command, then
  // we can execute it now.  If we're reading data, then this byte gets stuffed
  // into the data buffer.
  // 
  //    If we're in any other state, then the datasheet says this byte will
  // be ignored.
  //--
  if (m_State == ST_IDLE) {
    // Start reading a new command ...
    m_abCommand[0] = bData;  m_nCurrentByte = 1;
    m_nCommandLength = CommandLength(bData);
    NextState(ST_READ_COMMAND);
    if (m_nCommandLength == 1) DoCommand();
  } else if (m_State == ST_READ_COMMAND) {
    // Add this byte to the command buffer ...
    m_abCommand[m_nCurrentByte++] = bData;
    if (m_nCurrentByte >= m_nCommandLength) DoCommand();
  } else if (m_State = ST_READ_DATA) {
    // Add this byte to the data buffer ...
    // PROGRAMMED I/O NOT YET IMPLEMENTED!.
  } else {
    LOGF(WARNING, "uPD7565 received 0x%02X when state is %s", bData, StateToString(m_State).c_str());
  }
}

void CUPD765::TerminalCount()
{
  //++
  //   Calling this routine corresponds to asserting the Terminal Count input
  // for the uPD765, and it tells the FDC to stop whatever its doing now.
  // The is the normal way to terminate a read or write operation because,
  // believe it or not, the uPD765 doesn't know how many bytes or even how
  // many sectors are to be read or written.  It just keeps transferring data
  // until TerminalCount() is called and tells it to stop.
  // 
  //   And this routine/input can be used at any time to abort whatever the
  // uPD765 is doing and return it to the idle, ready to read a command,
  // state.
  //--
  NextState(ST_IDLE); // for now, that's all it takes!
}


////////////////////////////////////////////////////////////////////////////////
////////////////   U S E R   I N T E R F A C E   M E T H O D S   ///////////////
////////////////////////////////////////////////////////////////////////////////

void CUPD765::SetWriteLock (uint8_t nUnit, bool fProtect)
{
  //++
  //  This will set or clear the write lock flag for the specified unit,  Easy
  // enough, BUT there's a catch - if the actual disk image file has a read
  // only protection, then we force the drive to be write locked, regardless.
  //--
  assert(nUnit < MAXUNIT);
  m_afWriteLock[nUnit] = fProtect;
  if (IsAttached(nUnit) && m_apImages[nUnit]->IsReadOnly()) m_afWriteLock[nUnit] = true;
}

bool CUPD765::Attach (uint8_t nUnit, const string &sFileName, bool fWriteLock)
{
  //++
  //   This method will attach one floppy drive to an image file.  If opening
  // the image file is successful, then the drive immediately becomes online
  // for the uPD765 and we return true.  If opening the image file fails for
  // any reason, then we return false and the uPD765 thinks the drive is not
  // ready.
  // 
  //   Unlike IDE images where the disk size is somewhat arbitrary, diskettes
  // have a fixed capacity based on their sector size and geometry (number of
  // heads, sectors and tracks).  It's assumed that SetGeometry() has been
  // called BEFORE attaching an image file, and we'll use the calculated disk
  // capacity based on the geometry to set the file size.  If the attached
  // file is too short, we'll extend it to the correct length.  If it's too
  // long then we'll leave it alone, but the extra data at the end can't be
  // accessed of course.
  // 
  //   And diskettes can be write protected too.  The fWriteLock parameter
  // allows the drive to be initially write locked, or the write lock status
  // can be changed at any time by calling the SetWriteProtect() method.  NOTE
  // that if the actual disk image file has a read only file protection then
  // the virtual diskette drive is ALWAYS write locked, regardless of the
  // fWriteLock parameter.
  //--
  assert(!sFileName.empty());
  uint32_t lCapacity = m_apImages[nUnit]->GetCHScapacity();
  assert((lCapacity != 0) && (m_apImages[nUnit]->GetSectorSize() != 0));

  // Try to open the image file ...
  if (IsAttached(nUnit)) Detach(nUnit);
  if (!m_apImages[nUnit]->Open(sFileName)) return false;

  //   If the actual disk file has a read only protection, then set the write
  // lock status for this drive ...
  if (m_apImages[nUnit]->IsReadOnly()) m_afWriteLock[nUnit] = true;

  // Set the drive capacity as necessary ...
  uint32_t lFileSize = m_apImages[nUnit]->GetFileLength() / m_apImages[nUnit]->GetSectorSize();
  if (lCapacity > lFileSize) m_apImages[nUnit]->SetCapacity(lCapacity);

  // Success!
  LOGF(DEBUG, "Floppy unit %d attached to %s size %ld sectors",
    nUnit, GetFileName(nUnit).c_str(), m_apImages[nUnit]->GetCapacity());
  return true;
}

void CUPD765::Detach (uint8_t nUnit)
{
  //++
  // Take the unit offline and close the image file associated with it.
  //--
  if (!IsAttached(nUnit)) return;
  LOGF(DEBUG, "Floppy disk unit %d detached from %s", nUnit, GetFileName(nUnit).c_str());
  m_apImages[nUnit]->Close();
}

void CUPD765::DetachAll()
{
  //++
  // Detach ALL drives ...
  //--
  for (uint8_t i = 0;  i < MAXUNIT;  ++i)  Detach(i);
}

void CUPD765::ShowFDC (ostringstream& ofs) const
{
  //++
  // Dump the device state for the UI command "SHOW DEVICE" ...
  //--
  ofs << FormatString("uPD765 Floppy Diskette Controller\n");
  for (uint8_t nUnit = 0;  nUnit < MAXUNIT;  ++nUnit) {
    CDiskImageFile* p = m_apImages[nUnit];
    ofs << FormatString("  Unit %d: %d bytes/sector, %d sectors, %d tracks, %d head(s), %ld bytes\n",
      nUnit, p->GetSectorSize(), p->GetSectors(), p->GetCylinders(), p->GetHeads(), p->GetCHScapacity()*p->GetSectorSize());
    ofs << "          ";
    if (p->IsOpen()) {
      ofs << p->GetFileName();
      if (IsWriteLocked(nUnit)) ofs << " WRITE LOCKED";
    } else
      ofs << "not attached";
    ofs << std::endl;
  }
  ofs << std::endl;
  
  ofs << "  Current State: " << StateToString(m_State) << std::endl;
  ofs << "  Last Command:";
  for (uint8_t i = 0; i < m_nCommandLength; ++i)  ofs << FormatString(" 0x%02X", m_abCommand[i]);
  ofs << std::endl << "  Last Result:";
  for (uint8_t i = 0; i < m_nResultLength; ++i)  ofs << FormatString(" 0x%02X", m_abResult[i]);
  ofs << std::endl;
  ofs << FormatString("  Status: main=0x%02X, ST0=0x%02X, ST1=0x%02X, ST2=0x%02X, ST3=0x%02X\n",
    m_bMainStatus, m_abST[0], m_abST[1], m_abST[2], m_abST[3]);
  ofs << FormatString("  Delays: SRT=%lldms, HUT=%lldms, HLT=%lldms, ROT=%lldms, TXFR=%lldus",
    NSTOMS(m_llStepDelay), NSTOMS(m_llHeadUnloadDelay), NSTOMS(m_llHeadLoadDelay), NSTOMS(m_llRotationalDelay), NSTOUS(m_llTransferDelay));

}

