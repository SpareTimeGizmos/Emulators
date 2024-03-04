//IMPLEMENT READ ONLY ERRORS
//++
// IDE.cpp -> Simple IDE drive emulation
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
//   This module implements a simple emulation of an IDE/ATA disk drive (or
// CompactFlash card).  The implementation is pretty minimal, but it's enough
// to run most simple OSes.  In particular, we implement the ATA commands
//
//    SET FEATURES  - set features (via the FEATURES register)
//    IDENTIFY      - return the characteristics of this drive
//    READ          - read a sector
//    WRITE         - write a sector
//
// are all implemented.  The READ and WRITE commands are limited to one sector
// at a time, but both LBA and C/H/S addressing is allowed.  The only "feature"
// supported is 8 bit mode, which is essential to drive operation (see below).
// Additionally, these ATA commands
//
//    DIAGNOSE        - execute device diagnostic
//    SPIN UP         - spin up
//    SPIN DOWN       - spin down
//
// are implemented as NOPs.  They will return to a READY status but do nothing.
// On the plus side, interrupts are implemented at some level.  The only error
// statuses possible are ABORT (for invalid commands) or IDNF (for invalid disk
// addresses).
//
//   Two delays are used by this emulation.  There's a "short" delay, which
// is used for any command that doesn't actually access the media (e.g. for
// SET FEATURES or IDENTIFY DEVICE), and a "long" delay that's used for every
// command that requires a read or write.  That's as sophisticated as we can
// get; there's no attempt to calculate a realistic access time for rotating
// media.
//
// MASTER and SLAVE DRIVES
//   This module makes a stab at implemenenting both master and slave drives.
// The way this works with IDE is a bit odd, as both drives have their own
// independent controllers and internal logic.  As far as I know, all IDE
// writes update the associated register in BOTH drives, but only the selected
// drive executes a command.  Also only the selected drive enables its output
// bus drivers, so when a register is read only the selected drive can drive
// the bus.
// 
//   We model this in the more traditional sense as one controller with one
// set of registers but two physical drives (or image files, in this case).
// There are undoubtedly some differences between this and the way two real
// IDE drives would work, for example the status and error registers would
// be expected to differ, but so far none of the software we want to emulate
// seems to care.  
// 
//   One caveat though is that both drives have separate interrupt enable bits.
// That we do care about, and we make some effort to emulate that here.  Also
// in real IDE the reset bit, SRST, affects both drives.  That works naturally
// for us, but we do have to remember to clear BOTH interrupt enables.  FWIW
// the DIAG command is also executed by both drives simultaneously, but we
// don't emulate that one anyway.
// 
// 8 BIT VS 16 BIT
//   All IDE/ATA registers are 8 bits wide, EXCEPT for the data register which
// is nominally 16 bits.  Let me say that again - all reads or writes to the
// DATA register transfer 16 bits, not 8.  That's a problem for 8 bit micros,
// and it's a problem for this emulator too.  Some 8 bit systems use latches
// to temporarily store the upper 8 bit, and then have separate I/O ports to
// access the MSB and LSB of the IDE data.
//
//   BUT, IDE drives also theoretically have an 8 bit mode, which can be
// enabled by writing 0x01 to the FEATURE register.  In 8 bit mode the upper
// bits of the data register are not used and the drive transfers 512 8 bit
// bytes per sector instead of 256 16 bit words.  The reality is, however,
// that only very old IDE drives (mostly those made around the time of the
// PC/XT) support 8 bit mode.  No modern drive does.
//
//   BUT, all CompactFlash cards in ATA emulation mode support the 8 bit
// feature.  This is required by the CompactFlash Association specification.
// So if you have a CompactFlash card in ATA mode, then it plays nicely with
// an 8 bit system.
//
//   Since this emulator is designed around 8 bit systems, the CDevice ptoto-
// type doesn't allow for 16 bit transfers.  This module requires that the
// emulated firmware enable the 8 bit feature, and it'll log a warning message
// in any 16 bit data transfer is attempted.
//    
// REVISION HISTORY:
// 22-JAN-20  RLA   New file.
// 23-JUN-22  RLA   Remove references to GetBasePort() in Read() and Write().
//                  Generic IDE ports are always absolute.  This device is usually
//                  wrapped by some other class, and it will handle the base port.
// 23-JUN-22  RLA   If we get a command for the slave drive, then set an error bit!
//                  Don't just ignore it - that makes the BIOS1802 hang!
// 12-JUL-22  RLA   Add interrupt handling (!) for the SBCT11
//                  Add Set8BitMode() to force 8 bit mode regardless of features
//                  Add READ BUFFER and WRITE BUFFER commands
// 22-JUL-22  RLA   Add Capacity parameter to Attach()
//                  Make IDENTIFY return the actual size of the image file
//                  C/H/S addressing is no longer allowed and will cause an error
// 16-DEC-23  RLA   Set the model name (for IDENTIFY DEVICE) to the file name
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include <ctype.h>              // toupper() ...
#include <string>               // C++ std::string class, et al ...
#include <cstring>              // needed for memset()
#include <iostream>             // C++ style output for LOGS() ...
#include <sstream>              // C++ std::stringstream, et al ...
#include "EMULIB.hpp"           // emulator library definitions
#include "LogFile.hpp"          // emulator library message logging facility
#include "MemoryTypes.h"        // address_t and word_t data types
#include "EventQueue.hpp"       // CEventQueue declarations
#include "CPU.hpp"              // CPU definitions
#include "Device.hpp"           // generic device definitions
#include "IDE.hpp"              // declarations for this module


CIDE::CIDE (const char *pszName, const char *pszType, const char *pszDescription, address_t nPort, address_t nPorts, CEventQueue *pEvents)
  : CDevice(pszName, pszType, pszDescription, INOUT, nPort, nPorts, pEvents)
{
  //++
  //--
  m_llShortDelay = USTONS(10);    // 10 microseconds
  m_llLongDelay  = MSTONS( 1);    //  1 milliseconds
  // Allocate the CDiskImageFile objects ...
  for (uint8_t i = 0;  i < NDRIVES;  ++i) {
    m_apImages[i] = DBGNEW CDiskImageFile(SECTOR_SIZE);
    m_abError[i] = 0;  m_abStatus[i] = STS_READY;
    m_afIEN[i] = m_afIRQ[i] = false;
    m_af8BitMode[i] = m_afForce8Bit[i] = false;
    SetModelName(i, "EMULATOR");
  }
  //   You might be tempted to call Clear() here, but don't do it!  Clear()
  // tries to schedule an event to set the READY bit, and at this point we
  // don't have a CPU attached yet.  It'll fail!
  m_bCount = m_abLBA[0] = 1;
  m_cbTransfer = 0;  m_abLBA[3] = DRV_LBA;;
  m_abLBA[1] = m_abLBA[2] = 0;
  m_bFeatures = m_bLastCommand = 0;
  m_nSelectedUnit = 0;
  m_fReadTransfer = m_fBufferOnly = false;
  memset(m_abBuffer, 0, sizeof(m_abBuffer));

}

CIDE::~CIDE()
{
  //++
  // The destructor closes all image files and deletes the objects ...
  //--
  DetachAll();
  for (uint8_t i = 0;  i < NDRIVES;  ++i)  delete m_apImages[i];
}

void CIDE::ClearDevice()
{
  //++
  //   Reset the ATA registers as would the RESET signal...  Note that this
  // makes the drive busy for a short interval, and then it becomes ready.
  //--
  m_bCount = m_abLBA[0] = 1;  m_cbTransfer = 0;
  m_abLBA[1] = m_abLBA[2] = 0;  m_abLBA[3] = DRV_LBA;
  m_bFeatures = m_bLastCommand = 0;  m_nSelectedUnit = 0;
  m_fReadTransfer = m_fBufferOnly = false;
  memset(m_abBuffer, 0, sizeof(m_abBuffer));
  for (uint8_t i = 0;  i < NDRIVES;  ++i) {
    m_abError[i] = 0;  m_abStatus[i] = STS_READY;
    m_afIEN[i] = m_afIRQ[i] = m_af8BitMode[i] = false;
    ClearError(i);  DriveBusy(i);  UpdateInterrupt(i, false);
  }
  CancelEvent(EVENT_READY_0);  CancelEvent(EVENT_READ_0);
  CancelEvent(EVENT_READY_1);  CancelEvent(EVENT_READ_1);
  ScheduleEvent(EVENT_READY_0, m_llShortDelay);
  ScheduleEvent(EVENT_READY_1, m_llShortDelay);
}

void CIDE::SetModelName (uint8_t nUnit, const char *pszModel)
{
  //++
  //   This routine will set the "model name" for the given unit.  This string
  // is returned as part of the ATA IDENTIFY DEVICE command but otherwise has
  // no significance.
  // 
  //   Now for a puzzler - the ELF2K BIOS wants to swap pairs of bytes when it
  // reads the IDENTIFY DEVICE data.  Why?  Darn good question - I've no idea.
  // Did a real ATA drive, in 8 bit mode, send the high byte first?  Seems
  // unlikely (remember that ATA is a PC standard, and PCs are little endian),
  // but maybe.  More puzzling is that when the ELF2K BIOS reads the User
  // Addressable Sectors longword, it DOES NOT seem to swap bytes.  What the
  // heck?  If the bytes are swapped in the strings why are they not swapped
  // there?  I have no idea...
  //
  //   Note that the strings are always space filled and NOT zero terminated
  // (as you might expect from a C string).  The maximum space available for
  // the model name is 40 characters, and if the string passed is longer then
  // it will be truncated.
  //--
  size_t cbModel = strlen(pszModel);
  memset(m_apszModelName[nUnit], ' ', MODELLEN);
  memcpy(m_apszModelName[nUnit], pszModel, MIN(cbModel, MODELLEN));
  for (uint8_t i = 0; i < MODELLEN; i += 2) {
    char ch = m_apszModelName[nUnit][i];
    m_apszModelName[nUnit][i]   = toupper(m_apszModelName[nUnit][i+1]);
    m_apszModelName[nUnit][i+1] = toupper(ch);
  }
}

void CIDE::UpdateInterrupt (uint8_t nUnit, bool fRequest)
{
  //++
  //   Update the interrupt request for the specified drive.  If fRequest is
  // true AND the interrupt enable is set in the control register AND this
  // is the selected drive, then request an interrupt now.  In any other
  // case, clear the interrupt request.
  // 
  //   Note that m_fIRQ remembers the last interrupt state we wanted, but not
  // necessarily what was sent to the CPU because of the interrupt enable. I
  // believe that in a real ATA drive it keeps an interrupt request flip flop
  // internally, and the nIEN bit in the control register simply enables the
  // bus driver for that signal.  
  //--
  m_afIRQ[nUnit] = fRequest;
  if (nUnit == m_nSelectedUnit)
    RequestInterrupt(fRequest && m_afIEN[nUnit]);
}

void CIDE::ClearError (uint8_t nUnit)
{
  //++
  // Clear the error register and the error bit in the status ...
  //--
  CLRBIT(m_abStatus[nUnit], STS_ERROR);
  m_abError[nUnit] = 0;
}

void CIDE::SetError (uint8_t nUnit, uint8_t bError)
{
  //++
  // Set the error register and the error bit in the status ...
  //--
  SETBIT(m_abStatus[nUnit], STS_ERROR);
  m_abError[nUnit] = bError;
}

void CIDE::DriveBusy (uint8_t nUnit)
{
  //++
  //   Make the drive look "busy" by clearing the drive ready bit and setting
  // the busy bit in the status.  No other status bits are changed!
  //--
  CLRBIT(m_abStatus[nUnit], STS_READY);
  SETBIT(m_abStatus[nUnit], STS_BUSY);
}

void CIDE::DriveReady (uint8_t nUnit)
{
  //++
  //   Make the drive look "ready" by clearing the busy bit and setting the
  // drive ready bit in the status.  If interrupts are enabled, then this
  // will cause an interrupt request too.
  //--
  SETBIT(m_abStatus[nUnit], STS_READY);
  CLRBIT(m_abStatus[nUnit], STS_BUSY);
  CLRBIT(m_abStatus[nUnit], STS_DRQ);
  UpdateInterrupt(nUnit, true);
}

uint8_t CIDE::SelectUnit()
{
  //++
  // Update m_nSelectedUnit based on the drive select bit in the LBA3 register.
  //--
  uint8_t nNewUnit = ISSET(m_abLBA[3], DRV_SLAVE) ? 1 : 0;
  if (nNewUnit != m_nSelectedUnit) {
    LOGS(DEBUG, "IDE unit " << nNewUnit << " selected");
    m_nSelectedUnit = nNewUnit;
  }
  return m_nSelectedUnit;
}

void CIDE::DoControl (uint8_t bControl)
{
  //++
  //   This routine handles writing to the DEVICE CONTROL register.  All this
  // register does is a software reset (when bit 2 is set), and to set or
  // clear the interrupt enable bit.  Note that the interrupt enable bit is
  // inverted - a "0" ENABLES interrupts and "1" disables them!
  //
  //   NOTE - according to the ATA spec, bits 7 thru 4 of bControl are ignored.
  // Bit 3 is supposed to always be 1 and bit 0 is always 0, however we don't
  // bother to check those two conditions.  The ELF2K BIOS doesn't seem to
  // set bit 3 when it issues this command.
  //--
  LOGF(DEBUG, "IDE write device control 0x%02X", bControl);
  m_afIEN[m_nSelectedUnit] = !ISSET(bControl, CTL_nIEN);
  if (ISSET(bControl, CTL_SRST)) ClearDevice();
  UpdateInterrupt(m_nSelectedUnit, m_afIRQ[m_nSelectedUnit]);
}

bool CIDE::GetLBA (uint32_t &lLBA) const
{
  //++
  //   This will return the selected disk address as a 32 bit logical block
  // (sector) number.  If LBA addressing was selected by the host then this
  // is easy; if C/H/S addressing was selected, then we'll have to convert.
  //--
  if (ISSET(m_abLBA[3], DRV_LBA)) {
    // Assemble a 32 bit LBA value ...
    lLBA = MKLONG( MKWORD( (m_abLBA[3]&DRV_HEAD), m_abLBA[2]),
                   MKWORD(  m_abLBA[1],           m_abLBA[0]) );
    if (lLBA >= m_apImages[m_nSelectedUnit]->GetCapacity()) {
      LOGF(WARNING, "IDE unit %d invalid LBA address %ld", m_nSelectedUnit, lLBA);
      
      return false;
    }
  } else {
    LOGF(WARNING, "IDE C/H/S mode not supported!");
    return false;
    // Assemble an LBA from a C/H/S address ...
    //uint32_t lHead     = m_abLBA[3] & DRV_HEAD;
    //uint32_t lCylinder = MKWORD(m_abLBA[2], m_abLBA[1]);
    //uint32_t lSector   = m_abLBA[0];
    //if ((lHead >= HEADS_PER_CYLINDER) || (lCylinder >= NUMBER_OF_CYLINDERS) || (lSector > SECTORS_PER_TRACK)) {
    //  LOGF(WARNING, "IDE invalid C/H/S address %ld/%ld/%ld", lCylinder, lHead, lSector);  return false;
    //}
    //lLBA = (((lCylinder * HEADS_PER_CYLINDER) + lHead) * SECTORS_PER_TRACK) + lSector - 1;
  }
  return true;
}

void CIDE::StartTransfer (uint8_t nUnit, bool fRead)
{
  //++
  //   This will start programmed I/O data transfer.  It sets the data request
  // flag in the status and clears the drive busy flag.  It's not clear to me
  // whether READY should be set or remain cleared during the transfer, so for
  // now we'll just leave it alone.  The byte count is initialized to 512 and
  // the transfer direction is set by the fRead parameter (true = disk -> CPU,
  // and false = CPU -> disk).  If interrupts are enabled, then this will also
  // request an interrupt when reading (but not when writing!).
  //--
  SETBIT(m_abStatus[nUnit], STS_DRQ);
  CLRBIT(m_abStatus[nUnit], STS_BUSY);
  m_cbTransfer = SECTOR_SIZE;
  if (!fRead) memset(m_abBuffer, 0, sizeof(m_abBuffer));
  m_fReadTransfer = fRead;
  if (fRead) UpdateInterrupt(nUnit, true);
}

void CIDE::AbortTransfer (uint8_t nUnit)
{
  //++
  //   This routine will abort any data transfer that might be in progress.
  // It's called in the event of an error, or if the host issues a new command
  // before the current transfer is completed.  If this was a read operation
  // then the remaining data is just discarded.  If it was a write, then the
  // data is never written.
  //--
  if (!IsTransferInProgress()) return;
  m_cbTransfer = 0;
  CLRBIT(m_abStatus[nUnit], STS_DRQ);
  SetError(nUnit, ERR_ABORT);
  DriveReady(nUnit);
}

uint8_t CIDE::ReadData()
{
  //++
  //   This method is called whenever the host reads the data register.  If no
  // data transfer is in progress, then it always returns zero (I've no idea
  // what a real drive would do here).  If a write transfer is in progress,
  // then we abort the transfer (again, I've no idea what a real drive would
  // do under these conditions) and return zero.
  //
  //   BUT, if a real read transfer is in progress then we'll return the next
  // byte from the buffer.  If this was the last byte in the buffer then we'll
  // clear the DRQ bit, clear BUSY and set READY, otherwise we'll leave DRQ
  // set and continue to transfer bytes on subsequent reads.
  //
  //  We don't really support 16 bit transfers, but if the host has not enabled
  // the 8 bit feature then we'll transfer only every even numbered byte.
  // That's what a real drive would do in 16 bit mode with only an 8 bit data
  // connection.  The disk read and write commands can actually be used this
  // way, as long as you don't mind wasting half the disk space.
  //--
  if (!IsTransferInProgress()) return 0;
  if (!m_fReadTransfer) {
    AbortTransfer(m_nSelectedUnit);  return 0;
  }
  assert(m_cbTransfer <= SECTOR_SIZE);
  uint8_t bData = m_abBuffer[SECTOR_SIZE-m_cbTransfer];
  bool f8Bit = m_af8BitMode[m_nSelectedUnit] || m_afForce8Bit[m_nSelectedUnit];
  m_cbTransfer = f8Bit ? (m_cbTransfer-1) : ((m_cbTransfer & ~1)-2);
  if (m_cbTransfer == 0) DriveReady(m_nSelectedUnit);
  return bData;
}

void CIDE::WriteData (uint8_t bData)
{
  //++
  //   This method is called whenever the host writes the data register;  this
  // is used to transfer 512 bytes of data from the host to our buffer so that
  // we can write it to the disk.  If no data transfer is in progress, then
  // this routine just does nothing.  If a read transfer is in progress, then
  // then we abort the transfer (don't know if that's what a real drive would
  // do, but it's what we do!).
  //
  //   BUT, if a real write transfer is in progress then we'll store this byte
  // in the buffer.  If this was the last byte then we'll clear the DRQ bit and
  // call the WriteSector() method to write this data out to our image file.
  // We don't clear BUSY nor set DONE here - that's done by WriteSector().
  //
  //   If 8 bit mode is not enabled, then it's handled pretty much as a read
  // transfer would.  We load only the even numbered bytes in the buffer for a
  // total of 256 transfers, and the odd numbered bytes are always zero.
  //--
  if (!IsTransferInProgress()) return;
  if (m_fReadTransfer) {
    AbortTransfer(m_nSelectedUnit);  return;
  }
  assert(m_cbTransfer <= SECTOR_SIZE);
  m_abBuffer[SECTOR_SIZE-m_cbTransfer] = bData;
  bool f8Bit = m_af8BitMode[m_nSelectedUnit] || m_afForce8Bit[m_nSelectedUnit];
  m_cbTransfer =  f8Bit ? (m_cbTransfer-1) : ((m_cbTransfer & ~1)-2);
  if (m_cbTransfer == 0) {
    CLRBIT(m_abStatus[m_nSelectedUnit], STS_DRQ);
    if (!m_fBufferOnly)
      WriteSector();
    else
      ScheduleEvent(EVENT_READY_0+m_nSelectedUnit, m_llShortDelay);
  }
}

void CIDE::WriteSector()
{
  //++
  //   This method is called during a disk write command after the host has
  // filled our buffer, and now we're ready to write that data out to the 
  // image file.  We set the BUSY bit (DRQ and READY should already be
  // cleared) and then schedule an event to set READY at some time in the
  // future.  If the disk address is invalid, then we set the INVALID SECTOR
  // (IDNF) error bit and quit immediately without writing anything.
  //--
  DriveBusy(m_nSelectedUnit);
  uint32_t lLBA;
  if (GetLBA(lLBA)) {
    LOGF(DEBUG, "IDE unit %d write sector %d", m_nSelectedUnit, lLBA);
    if (!m_apImages[m_nSelectedUnit]->WriteSector(lLBA, m_abBuffer)) {
      LOGF(ERROR, "IDE unit %d offline due to errors", m_nSelectedUnit);
      Detach(m_nSelectedUnit);
    }
    ScheduleEvent(EVENT_READY_0+m_nSelectedUnit, m_llLongDelay);
  } else {
    SetError(m_nSelectedUnit, ERR_IDNF);
    DriveReady(m_nSelectedUnit);
  }
}

void CIDE::DoDiskRead()
{
  //++
  //   This method is called when the host executes a disk READ command.  We
  // verify that the sector count is 1 (only single sector transfers are
  // implemented here!), and then verify that the sector address is valid.
  // If either of those tests fail, we set the error status and quit now.
  //
  //   Assuming all is well, we read the desired sector from our image file
  // and load it into the sector buffer.  We then schedule a read event to
  // happen some time in the future - that'll set the DRQ bit and tell the
  // host to start reading bytes.  The ReadData() method is called for every
  // byte read, and when the whole buffer has been transferred it will make
  // the drive ready again.
  //--
  if (m_bCount != 1) {
    SetError(m_nSelectedUnit, ERR_ABORT);  return;
  }
  uint32_t lLBA;
  if (GetLBA(lLBA)) {
    LOGF(DEBUG, "IDE unit %d read sector %d", m_nSelectedUnit, lLBA);
    if (!m_apImages[m_nSelectedUnit]->ReadSector(lLBA, m_abBuffer)) {
      LOGF(ERROR, "IDE unit %d offline due to errors", m_nSelectedUnit);
      Detach(m_nSelectedUnit);
    }
    ScheduleEvent(EVENT_READ_0+m_nSelectedUnit, m_llLongDelay);
  } else {
    SetError(m_nSelectedUnit, ERR_IDNF);
    DriveReady(m_nSelectedUnit);
  }
}

void CIDE::DoReadBuffer()
{
  //++
  //   The READ BUFFER command is identical to READ SECTOR, except that this
  // command transfers the data already in the buffer, whatever that might
  // be.  No actual sector is read and the current LBA and sector count
  // registers are ignored.
  //--
  LOGF(DEBUG, "IDE unit %d read buffer", m_nSelectedUnit);
  ScheduleEvent(EVENT_READ_0+m_nSelectedUnit, m_llShortDelay);
}

void CIDE::DoDiskWrite()
{
  //++
  //   This method is called when the host executes a disk WRITE command.
  // Unlike READ, in this case all the good stuff happens at the end, after the
  // data has been transferred from the CPU to our buffer.  We just verify that
  // the sector count is one (since we only support single sector transfers)
  // and, assuming that it is, we wait for the host to transfer data.  Each one
  // of those transfers will call the WriteData() method, which will eventually
  // call WriteSector() when the buffer is filled.
  //--
  if (m_bCount != 1) {
    SetError(m_nSelectedUnit, ERR_ABORT);
    return;
  }
  StartTransfer(m_nSelectedUnit, false);
  m_fBufferOnly = false;
}

void CIDE::DoWriteBuffer()
{
  //++
  //   The WRITE BUFFER command transfers data from the host to our buffer,
  // but doesn't actually write any sector.  The current LBA and sector
  // count registers are ignored.
  //--
  LOGF(DEBUG, "IDE unit %d write buffer", m_nSelectedUnit);
  StartTransfer(m_nSelectedUnit, false);
  m_fBufferOnly = true;
}

void CIDE::SetFeatures()
{
  //++
  //   This executes the ATA "SET FEATURES" command, which will attempt to
  // enable the feature(s) specified in the features register.  The only
  // feature we current implement is 8 bit mode - everything else causes
  // an error and is ignored.
  //--
  if (m_bFeatures == FEA_8BIT) {
    m_af8BitMode[m_nSelectedUnit] = true;
  } else {
    LOGF(DEBUG, "unimplemented IDE feature 0x%02X", m_bFeatures);
    SetError(m_nSelectedUnit, ERR_ABORT);
  }
  ScheduleEvent(EVENT_READY_0+m_nSelectedUnit, m_llShortDelay);
}

void CIDE::IdentifyDevice (uint8_t nUnit)
{
  //++
  //   The IDENTIFY DEVICE command will fill the buffer with a bunch of data
  // about the drive, only some of which we emulate.  The buffer is then
  // transferred to the host just as it would be for a normal disk read
  // command.
  //--
  memset(m_abBuffer, 0, sizeof(m_abBuffer));
  IDENTIFY_DEVICE_DATA *pIDD = (IDENTIFY_DEVICE_DATA *) &m_abBuffer;

  //   Note that the strings are always space filled and NOT zero terminated
  // (as you might expect from a C string).
  //
  //                                0000000000111111111122222222223333333333
  //                                0123456789012345678901234567890123456789
  memcpy(pIDD->abSerialNumber,     "            01242020", 20);
  memcpy(pIDD->abFirmwareRevision, "V0.0.0  ", 8);
  memcpy(pIDD->abModelNumber, m_apszModelName[nUnit], MODELLEN);

  // Other interesting data ...
  pIDD->wGeneralConfiguration = IDD_FIXED_DEVICE;
  //pIDD->wNumberOfCylinders = NUMBER_OF_CYLINDERS;
  //pIDD->wNumberOfHeads = HEADS_PER_CYLINDER;
  //pIDD->wSectorsPerTrack = SECTORS_PER_TRACK;
  pIDD->wBufferSize = 1;
  pIDD->wCapabilities = IDD_LBA_SUPPORTED;
  pIDD->lUserAddressableSectors = m_apImages[m_nSelectedUnit]->GetCapacity();

  // Schedule a DRQ soon and we're done here...
  ScheduleEvent(EVENT_READ_0+m_nSelectedUnit, m_llShortDelay);
}

void CIDE::DoNothing()
{
  //++
  //   This method handles commands that do nothing, at least not in emulation,
  // like SPIN UP or SPIN DOWN.  It just delays a short while and then sets the
  // READY status bit.
  //--
  ScheduleEvent(EVENT_READY_0+m_nSelectedUnit, m_llShortDelay);
}

void CIDE::DoCommand (uint8_t bCommand)
{
  //++
  //   We end up here whenever the host writes a byte to the command register.
  // First we need to make sure that drive 0 is select; we only emulate the
  // master drive, and any command for the slave drive is ignored.  Then we
  // need to check that the drive is not currently busy - if it is, then this
  // command is ignored (I'm not sure what a real drive would do here!).  If
  // a buffer transfer is currently in progress then we abort that operation
  // (again, not sure what a real drive would do).
  //
  //   Finally, after all that, we're ready to execute the command.  The drive
  // clears the ERROR and READY bits in the status register, clears the error
  // register, and then sets the BUSY bit.  The command is then executed and,
  // depending on what exactly the command does, at some time in the future the
  // READY bit will be set again.
  //--
  if (!IsAttached(m_nSelectedUnit)) return;
  if (IsTransferInProgress()) {
    AbortTransfer(m_nSelectedUnit);  return;
  }
  ClearError(m_nSelectedUnit);
  DriveBusy(m_nSelectedUnit);
  LOGF(TRACE, "IDE unit %d command 0x%02X", m_nSelectedUnit, bCommand);
  switch ((m_bLastCommand = bCommand)) {
    case CMD_FEATURES:      SetFeatures();                    break;
    case CMD_IDENTIFY:      IdentifyDevice(m_nSelectedUnit);  break;
    case CMD_READ:          DoDiskRead();                     break;
    case CMD_WRITE:         DoDiskWrite();                    break;
    case CMD_READ_BUFFER:   DoReadBuffer();                   break;
    case CMD_WRITE_BUFFER:  DoWriteBuffer();                  break;
    case CMD_DIAGNOSE:
    case CMD_SPIN_UP:
    case CMD_SPIN_DOWN:     DoNothing();                      break;
    default:
      LOGF(DEBUG, "unimplemented IDE command 0x%02X", bCommand);  break;
  }

}

void CIDE::EventCallback (intptr_t lParam)
{
  //++
  // Handle event callbacks for this device ...
  //--
  switch (lParam) {
    case EVENT_READY_0: DriveReady(0);           break;
    case EVENT_READY_1: DriveReady(1);           break;
    case EVENT_READ_0:  StartTransfer(0, true);  break;
    case EVENT_READ_1:  StartTransfer(1, true);  break;
    default: assert(false);
  }
}

word_t CIDE::DevRead (address_t nRegister)
{
  //++
  //   Read an IDE/ATA register.  Most things just return the current contents
  // of the register, but reading the DATA register has side effects (each
  // read iterates thru the data buffer!).
  //--
  assert(nRegister < MAXREG);
  if (!IsAttached(m_nSelectedUnit)) return 0;
  switch (nRegister) {
    case REG_DATA:    return ReadData();
    case REG_ERROR:   return m_abError[m_nSelectedUnit];
    case REG_COUNT:   return m_bCount;
    case REG_LBA0:    return m_abLBA[0];
    case REG_LBA1:    return m_abLBA[1];
    case REG_LBA2:    return m_abLBA[2];
    case REG_LBA3:    return m_abLBA[3];
    case REG_DRVADDR: return 0xFE;  // TBA TODO NYI!!
    //   Note that reading the status register always clears any interrupt
    // request, but reading the alternate status register does not!
    case REG_ALTSTS:  return m_abStatus[m_nSelectedUnit];
    case REG_STATUS:  UpdateInterrupt(m_nSelectedUnit, false);
                      return m_abStatus[m_nSelectedUnit];
    default: return 0;
  }
}

void CIDE::DevWrite (address_t nRegister, word_t bData)
{
  //++
  //   Write to an IDE/ATA register.  Many registers just rememeber the value
  // written, however writing the DATA register iterates thru the buffer (as
  // does reading the DATA register).  Writing the COMMAND or CONTROL register
  // executes the command/control function immediately.
  //--
  assert(nRegister < MAXREG);
  //if (!IsAttached(m_nSelectedUnit)) return;
  switch (nRegister) {
    case REG_DATA:    WriteData(LOBYTE(bData));                    break;
    case REG_FEATURE: m_bFeatures = LOBYTE(bData);                 break;
    case REG_COUNT:   m_bCount    = LOBYTE(bData);                 break;
    case REG_LBA0:    m_abLBA[0]  = LOBYTE(bData);                 break;
    case REG_LBA1:    m_abLBA[1]  = LOBYTE(bData);                 break;
    case REG_LBA2:    m_abLBA[2]  = LOBYTE(bData);                 break;
    case REG_LBA3:    m_abLBA[3]  = LOBYTE(bData);  SelectUnit();  break;
    case REG_COMMAND: DoCommand(LOBYTE(bData));                    break;
    case REG_DEVCTL:  DoControl(LOBYTE(bData));                    break;
    default:  /* Do nothing!! */                                   break;
  }
}

bool CIDE::Attach (uint8_t nUnit, const string &sFileName, uint32_t lCapacity)
{
  //++
  //   This method will attach one IDE drive to an image file.  The nUnit
  // parameter must be either 0 for the master drive or 1 for the slave.
  // If opening the image file is successful, then the drive immediately
  // becomes online for the host and we return true.  If opening the image
  // file fails for any reason, then we return false.
  // 
  //   It's also possible to set the drive capacity here.  If the lCapacity
  // parameter is zero thn we'll use the current file size to set the drive
  // capacity, UNLESS the file is being created in which case we'll set the
  // file size to the IDE default size (currently 32Mb).
  // 
  //   However if fCapacity is NOT zero then it specifies the minimum drive
  // size, IN BLOCKS (NOT bytes!).  If the file opened is already that big or
  // bigger, then we'll leave it alone.  But if the file is smaller than the
  // lCapacity specified, or if the file is being created now, then we'll
  // extend it to the size specified.
  // 
  //   Note that IDE drives do not have any concept of read only status, and
  // there's no fReadOnly parameter here.  If the disk file we're attaching has
  // a read only protection then CImageFile::Open() will mark the image as read
  // only anyway.
  //--
  assert(!sFileName.empty());

  // Try to open the image file ...
  if (IsAttached(nUnit)) Detach(nUnit);
  if (!m_apImages[nUnit]->Open(sFileName)) return false;

  //   IDE/ATA doesn't really support the concept of read only drives, and even
  // though we specified that the image file be opened for read/write access,
  // if the file is read only then the image will be read only.  Disallow that
  // case, at least for now ...
  if (IsReadOnly(nUnit)) {
    LOGS(ERROR, "read only access not supported");  return false;
  }

  // Set the drive capacity as necessary ...
  if (m_apImages[nUnit]->GetCapacity() == 0) {
    // This is an empty file, so it was probably just created ...
    m_apImages[nUnit]->SetCapacity((lCapacity != 0) ? lCapacity : DEFAULT_CAPACITY);
  } else if (lCapacity > m_apImages[nUnit]->GetCapacity()) {
    // The file exists, but it's too short - extend it ...
    m_apImages[nUnit]->SetCapacity(lCapacity);
  }

  // Set the drive's "model name" to the file name ...
  string sDrive, sPath, sName, sType;
  SplitPath(sFileName, sDrive, sPath, sName, sType);
  string sModel = sName + sType;
  SetModelName(nUnit, sModel.c_str());

  // Success!
  LOGF(DEBUG, "IDE unit %d attached to %s capacity %ld blocks",
    nUnit, GetFileName(nUnit).c_str(), m_apImages[nUnit]->GetCapacity());
  return true;
}

void CIDE::Detach (uint8_t nUnit)
{
  //++
  // Take the unit offline and close the image file associated with it.
  //--
  if (!IsAttached(nUnit)) return;
  LOGF(DEBUG, "IDE unit %d detached from %s", nUnit, GetFileName(nUnit).c_str());
  m_apImages[nUnit]->Close();
}

void CIDE::DetachAll()
{
  //++
  // Detach ALL drives ...
  //--
  for (uint8_t i = 0;  i < NDRIVES;  ++i)  Detach(i);
}

void CIDE::ShowDevice (ostringstream &ofs) const
{
  //++
  //   This routine will dump the state of the internal IDE registers.
  // This is used by the user interface SHOW DEVICE command ...
  //--
  for (uint8_t i = 0;  i < NDRIVES;  ++i) {
    ofs << "Unit " << i << ": ";
    if (IsAttached(i)) {
      ofs << GetFileName(i);
      ofs << ", " << GetCapacity(i) << " blocks";
    } else
      ofs << "not attached";
    ofs << std::endl;
    ofs << FormatString("       %d bit mode, IEN=%d, IRQ=%d, status=0x%02X, error=0x%02X\n",
      (Is8Bit(i) ? 8 : 16), m_afIEN[i], m_afIRQ[i], m_abStatus[i], m_abError[i]);
  }
  ofs << std::endl;

  ofs << FormatString("Last command=0x%02X, Short delay=%lldus, Long=%lldus\n",
    m_bLastCommand, NSTOUS(m_llShortDelay), NSTOUS(m_llLongDelay));
  //uint32_t lLBA;  GetLBA(lLBA);
  //ofs << FormatString("LBA=%ld, SCT=%d, BCT=%d\n", lLBA, m_bCount, m_cbTransfer);
  ofs << std::endl;
  ofs << DumpBuffer("SECTOR BUFFER", m_abBuffer, SECTOR_SIZE);
}
