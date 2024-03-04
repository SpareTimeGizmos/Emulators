//++
// ImageFile.cpp -> CImageFile (image file I/O for emulatir library) methods
//                  CDiskImageFile (disk image file) methods
//                  CTapeImageFile (tape image file) methods
//                  CTextInputFile (unit record input file) methods
//                  CTextOutputFile (unit record output file) methods
//
//   COPYRIGHT (C) 2015-2020 BY SPARE TIME GIZMOS.  ALL RIGHTS RESERVED.
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
//   These class contains image file access methods.  A CImageFile object
// encapsulates any image file on the host file system.  A CDiskImage file is
// a derived class that represents a fixed length sector, random access, block
// rewritable (i.e. exactly what you'd expect from a disk drive!) file.  And
// conversely, a CTapeImageFile is a derived class that represents a variable
// record length, sequential access, non-rewritable (just what you'd want for
// a tape drive) file.  Lastly, CTextInputFile and CTextOutputFile
// are sequential text (i.e. translated ASCII) image file subclasses for unit
// record devices such as printers, card readers and card punches, etc.
//
//   The disk image file format is just a simple linear array of sectors stored
// verbatim, in ascending LBA order, and without any extra header words or
// other overhead.  It's the format used by simh and most other emulators. Note
// that the emulator library version here only deals with bytes. For disks that
// work with other word sizes the class that wraps this one must provide a
// conversion.  Lastly, notice that the disk sector size, ALWAYS in bytes, MUST
// be specified to the CDiskImage constructor - that's required by SeekSector()
// to calculate the correct offset.
//
//   The tape image format is identical to the simh TAP format, with a single
// 32 bit header record stored at the start and end of each logical record.
// Nine track tape images are always stored as eight bit bytes - the ninth bit
// is a parity check and is not stored in the TAP file.  Packing and unpacking
// these bytes into whatever wordsize the host supports is always the problem
// of the controller - most machines, like CDC and DEC, have various and creative
// ways of handling it.
//
//   Note that there isn't really a simh standard for seven track tape images,
// although one common usage is to always set the MSB to zero and then store
// seven track tape frames as eight bit bytes.  In that usage a seven track
// image and a nine track tape image are indistinguishable, which is probably
// OK for practical purposes.  There's no special code in this class to support
// seven track tapes, although if you handle them this way then you don't need
// any.
//
// IMPORTANT!
//   Even the casual observer will notice that the read and write functions
// are NOT ENDIAN INDEPENDENT.  Right now that's a moot point because the
// only platform we support is a PC, and PCs are all little endian.
//
// Bob Armstrong <bob@jfcl.com>   [8-OCT-2013]
//
// REVISION HISTORY:
// 20-MAY-15  RLA   New file.
//  8-DEC-15  RLA   Add CTextInputFile and CTextOutputFile.
// 22-FEB-16  RLA   Add fixed length record support to CText????ImageFile.
//  8-MAR-16  RLA   Fix the mixed read/write bug with CTapeImageFiles.
// 28-FEB-17  RLA   Make 64 bit clean.
//  1-JUN-17  RLA   Linux port.
// 28-Sep-17  RLA   In ReadForwardRecord() don't die if we try to read at EOT.
// 22-JUL-22  RLA   Add SetCapacity() for disk image files ...
// 26-AUG-22  RLA   Clean up Linux/WIN32 conditionals.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include <errno.h>              // ENOENT, EACCESS, etc ...
#include <string.h>             // strcpy(), strerror(), etc ...
#include <cstring>              // needed for memset()
#if defined(_WIN32)
#include <io.h>                 // _chsize(), _fileno(), etc...
#elif defined(__linux__)
#include <unistd.h>             // ftruncate(), etc ...
#include <sys/stat.h>           // needed for fstat() (what else??)
#include <sys/file.h>           // flock(), LOCK_EX, LOCK_SH, et al ...
#endif
#include "EMULIB.hpp"           // emulator library definitions
#include "SafeCRT.h"		// replacements for Microsoft "safe" CRT functions
#include "LogFile.hpp"          // message logging facility
#include "ImageFile.hpp"        // declarations for this module



///////////////////////////////////////////////////////////////////////////////
//  CImageFile members ...
///////////////////////////////////////////////////////////////////////////////

CImageFile::CImageFile()
{
  //++
  //   The constructor just initializes all the members - it DOES NOT open
  // any external disk file!  You'll have to call the Open() method to do that ...
  //--
  m_sFileName.clear();  m_pFile = NULL;  m_fReadOnly = false;
  m_nShareMode = SHARE_READ;
}

CImageFile::~CImageFile()
{
  //++
  // Close the disk file (if any) and destroy the CImage object ...
  //--
  Close();
}

bool CImageFile::Error (const char *pszMsg, int nError) const
{
  //++
  //   Print a file related error message and then always return false.  Why
  // always return false?  So we can say something like
  //
  //    if ... return Error("fail", errno);
  //--
  char sz[80];
  if (nError > 0) {
    LOGS(ERROR, "error (" << nError << ") " << pszMsg << " " << m_sFileName);
    strerror_s(sz, sizeof(sz), nError);
    LOGS(ERROR, sz);
  } else {
    LOGS(ERROR, pszMsg << " - " << m_sFileName);
  }
  return false;
}

bool CImageFile::TryOpenAndLock (const char *pszMode, int nShare)
{
  //++
  //   This routine will try to open the file using the specified mode and, if
  // it is successful it will lock the file as required.  On Windows this can be
  // done all in one step with _fsopen(), but under Linux it takes two steps.
  // First, open the file and, if that works, call flock() to lock the file.
  // On both platforms the file name and file sharing mode are assumed to be
  // already set up in m_FileName and m_nShareMode.
  //
  //   This method will use the Linux flock() call to apply the desired file
  // lock, as indicated by m_nShareMode, to this file.  Under Linux, flock()
  // really only gives us two options - lock the file for exclusive access, or
  // lock the file for shared access (which really just prevents anyone else
  // from opening it for exclusive access).  There is no concept of shared
  // read-only access - you either share the file or you don't!
  //--
#if defined(_WIN32)
  m_pFile = _fsopen(m_sFileName.c_str(), pszMode, nShare);
  return (m_pFile != NULL);
#elif defined(__linux__)
  m_pFile = fopen(m_sFileName.c_str(), pszMode);
  if (m_pFile == NULL) return false;
  int lock = (nShare == SHARE_NONE) ? LOCK_EX : LOCK_SH;
  return (flock(fileno(m_pFile), lock|LOCK_NB) == 0);
#endif
}

bool CImageFile::Open (const string &sFileName, bool fReadOnly, int nShareMode)
{
  //++
  //   This method opens an image file associated with this object.  Simple,
  // but it has a couple of special cases.  First, if the fReadOnly flag is
  // true then the file is opened for read access only.  if fReadOnly is
  // false we normally try to open the file as read/write, BUT if the actual
  // image file has the read only attribute in the host file system then
  // we'll force read only mode.  This particular situation doesn't report any
  // error, but the caller can find out if it happened thru the IsReadOnly()
  // attribute of this object.
  //
  //   Also, if the file doesn't exist then we'll attempt to create an empty
  // file UNLESS the fReadOnly flag is true - in that case a non-existent file
  // is an error.  If the file can't be opened or created one way or another,
  // then an error message is printed, false is returned and this object
  // remains unopened.
  //
  //   Lastly, nShareMode can optionally specify the desired file sharing mode.
  // This defaults to SHARE_READ (i.e. shared reading, but only a single writer)
  // if not specified.  Note that the CImageFile class currently doesn't contain
  // any code for synchronizing access to a shared write file, so use this at
  // your own risk!
  //--
  assert(!sFileName.empty());
  if (nShareMode == 0)  nShareMode = fReadOnly ? SHARE_READ : SHARE_NONE;
  m_sFileName = sFileName;  m_fReadOnly = fReadOnly;  m_nShareMode = nShareMode;

  if (m_fReadOnly) {
    //   Open the file as read only.  In this case if it doesn't already exist,
    // there's not much we can do about it ...
    if (TryOpenAndLock("rb", m_nShareMode)) return true;
    return Error("opening", errno);
  }

  // Try to open the file for R/W access.  If we succeed, then quit now...
  if (TryOpenAndLock("rb+", m_nShareMode)) return true;

  //   If we failed because the file is read only, then switch this unit to
  // read only and attempt to continue ...
  if ((errno == EROFS) || (errno == EACCES)) {
    if (TryOpenAndLock("rb", SHARE_READ)) {
      LOGS(DEBUG, "opening " << m_sFileName << " as read only");
      m_fReadOnly = true;  m_nShareMode = SHARE_READ;  return true;
    }
    return Error("opening", errno);
  }

  // If we failed because the file doesn't exist, then try to create it ...
  if (errno == ENOENT) {
    if (TryOpenAndLock("wb+", m_nShareMode)) {
      LOGS(DEBUG, "creating empty file for " << m_sFileName);
      return true;
    }
    return Error("creating", errno);
  }

  // Otherwise we don't know what's wrong, so give up ...
  return Error("opening", errno);
}

void CImageFile::Close ()
{
  //++
  //   This routine closes an image file.  It's not very complicated, but
  // we'll spell it out for completeness...
  //--
  if (IsOpen()) {
#if defined(__linux__)
    flock(fileno(m_pFile), LOCK_UN);
#endif
    fclose(m_pFile);
  }
  m_sFileName.clear();  m_pFile = NULL;
}

uint32_t CImageFile::GetFileLength() const
{
  //++
  // Get the current file size (in bytes!) ...
  //--
  assert(IsOpen());
#if defined(_WIN32)
  return _filelength(_fileno(m_pFile));
#elif defined(__linux__)
  struct stat st;
  if (fstat(fileno(m_pFile), &st) == 0) return st.st_size;
  Error("fstat", errno);  return 0;
#endif
}

uint32_t CImageFile::GetFilePosition() const
{
  //++
  // Get the current file position (in bytes!) ...
  //--
  assert(IsOpen());
  return ftell(m_pFile);
}

bool CImageFile::SetFileLength (uint32_t nNewLength)
{
  //++
  //   This method will change the length of this file.  If the new length
  // is longer than the current length, then the file is extended.  If the
  // new length is shorter (down to and including 0 bytes!), then the file
  // will be truncated ...
  //--
  if (IsReadOnly()) return false;
#if defined(_WIN32)
  if (_chsize(_fileno(m_pFile), nNewLength) == 0) return true;
#elif defined(__linux__)
  if (ftruncate(fileno(m_pFile), nNewLength) == 0) return true;
#endif
  return Error("change size", errno);
}

bool CImageFile::Truncate ()
{
  //++
  // Truncate the image file to the current position.
  //--
  assert(IsOpen());
  if (IsReadOnly()) return false;
  fseek(m_pFile, 0L, SEEK_CUR);
  return SetFileLength(ftell(m_pFile));
}



///////////////////////////////////////////////////////////////////////////////
// CDiskImageFile members ...
///////////////////////////////////////////////////////////////////////////////

CDiskImageFile::CDiskImageFile (uint32_t lSectorSize)
{
  //++
  //   Initialize any disk image specific flags.  Note that the sector size
  // parameter is REQUIRED - we need it to calculate the correct disk offset
  // in SeekSector()!
  //--
  assert(lSectorSize > 0);
  m_lSectorSize = lSectorSize;  m_lCapacity = 0;
}

bool CDiskImageFile::SeekSector (uint32_t lLBA)
{
  //++
  //   This method will do an fseek() on the image file to move to the correct
  // offset for the specified absolute sector.  The sector size is used to
  // calculate the correct byte offset - remember that disk images are always
  // stored uncompressed, so every PP word requires two bytes!
  //
  // Note that we use the 32 bit fseek() here rather than the 64 bit fseek64().
  // That's not really a problem at the moment, since the biggest disk we can
  // emulate only holds about 300Mb.
  //--
  assert(IsOpen());
  if (lLBA >= GetCapacity())
    return Error("bad LBA", 0);
  if (fseek(m_pFile, (lLBA * m_lSectorSize), SEEK_SET) != 0)
    return Error("seeking", errno);
  return true;
}

bool CDiskImageFile::ReadSector (uint32_t lLBA, void *pData)
{
  //++
  //   This method will read a single sector from a disk image file. Note that
  // the sector size, and therefore the number of bytes read, is fixed.  The
  // caller's buffer had better be big enough, or Bad Things will result!
  //
  //   Remember that new disk image files are NOT pre-allocated, so if the host
  // attempts to read a sector that's never been written before we'll end up
  // reading past the EOF.  That's not a problem, although it's not absolutely
  // clear what should happen in that case.  This routine always returns a
  // buffer of zeros for uninitialized disk data.
  //--
  assert(IsOpen());
  if (!SeekSector(lLBA)) return false;
  size_t count = fread(pData, 1, m_lSectorSize, m_pFile);
  if (count == 0) {
    // We're attempting to read past the EOF ...
    memset(pData, 0, m_lSectorSize);
  } else if (count != m_lSectorSize) {
    // Some kind of real file error occurred ...
    return Error("reading", errno);
  }
  return true;
}

bool CDiskImageFile::WriteSector (uint32_t lLBA, const void *pData)
{
  //++
  // Write a single sector to the image file ...
  //--
  assert(IsOpen());
  if (IsReadOnly()) return false;
  if (!SeekSector(lLBA)) return false;
  if (fwrite(pData, 1, m_lSectorSize, m_pFile) != m_lSectorSize)
    return Error("writing", errno);
  return true;
}

uint32_t CDiskImageFile::GetCapacity() const
{
  //++
  //   This routine will return the current capacity of the disk image, in
  // sectors or blocks.  It's one more than the maximum LBA that can be passed
  // to ReadSector() or WriteSector().  Note that this gets called fairly often
  // to range check the LBA, so we try to cache the result...
  //--
  if (m_lCapacity != 0) return m_lCapacity;
  const_cast<CDiskImageFile *>(this)->m_lCapacity = GetFileLength() / m_lSectorSize;
  return m_lCapacity;
}

bool CDiskImageFile::SetCapacity (uint32_t lCapacity, bool fTruncate)
{
  //++
  //   And this routine will attempt to set the size of the disk image.  If
  // the new size is larger than the current size then the image will be
  // extended, however if the new size is smaller then nothing will happen
  // unless fTruncate is also true.  In that case, and only then, will data
  // be truncated off the end of the image.
  //--
  if ((lCapacity < GetCapacity())  &&  !fTruncate) return false;
  if (!SetFileLength(lCapacity * m_lSectorSize)) return false;
  m_lCapacity = lCapacity;
  return true;
}


///////////////////////////////////////////////////////////////////////////////
// CTapeImageFile members ...
///////////////////////////////////////////////////////////////////////////////

CTapeImageFile::CTapeImageFile (bool f7Track)
{
  //++
  // Initialize any tape image specific flags ...
  //--
  m_nRecordCount = 0;  m_fWriteLast = false;
  m_nFileSize = 0;  m_f7Track = f7Track;
}

bool CTapeImageFile::Open (const string &sFileName, bool fReadOnly, int nShareMode)
{
  //++
  //   Open the associated image file and initialize the file length and
  // record count ...
  //--
  if (!CImageFile::Open(sFileName, fReadOnly, nShareMode)) return false;
  m_nFileSize = GetFileLength();  m_nRecordCount = 0;  m_fWriteLast = false;
  LOGF(TRACE, "  -> CTapeImageFile::Open, file length=%d", m_nFileSize);
  return true;
}

bool CTapeImageFile::IsBOT() const
{
  //++
  // Return TRUE if the current tape position is at the BOT ...
  //--
  assert(IsOpen());
  return ftell(m_pFile) == 0;
}

bool CTapeImageFile::IsEOT() const
{
  //++
  // Return TRUE if the current tape position is at the EOT ...
  //
  //   Unfortunately, and unlike IsBOT(), there's no elegant way to do this.
  // You're probably saying - "just return feof()!".  No dice - feof() won't
  // work unless you've first tried to read and then failed because of EOF.
  // feof() doesn't look ahead at what's next in the file.
  //
  //   Instead, we keep track of the total file length in the m_nFileSize
  // member and compare the current position (obtained from ftell()) to that.
  // If they're equal, then we're at the end.  This works great, and it's fast
  // too, as long as we keep m_nFileSize current.
  //
  //   One more comment before going - for a real tape, EOT means the end of
  // the physical media, but in the context of an image file it's a little
  // different.  For a read only file, the EOT logically means the end of the
  // data, because there's never going to be any more on that virtual tape.
  // But for a writable image, then EOT never occurs because the image file
  // can always be extended and more data can always be written.
  //
  //   Hence, this function only has meaning for a read only image.  For a
  // writable image, it always returns false!
  //--
  assert(IsOpen());
  if (!IsReadOnly()) return false;
  return ((uint32_t) ftell(m_pFile)) >= m_nFileSize;
}

bool CTapeImageFile::Rewind()
{
  //++
  // Rewind the logical tape to the BOT ...
  //--
  assert(IsOpen());
  if (fseek(m_pFile, 0L, SEEK_SET) != 0)
    return CImageFile::Error("seek rewind", errno);
  m_fWriteLast = false;  m_nRecordCount = 0;
  return true;
}

int32_t CTapeImageFile::ReadForwardRecord (uint8_t abData[], size_t cbMaxData)
{
  //++
  //   This routine will read, in the forward direction, the next record from
  // the tape image file.  The raw data from the record, which cannot exceed
  // cbMaxData in length, is returned in abData.  The actual record length, in
  // bytes, is the function return value.  This routine can also return either
  // EOTBOT, if the tape is at the end, or BADTAPE, if any TAP file format
  // errors are found.  Note that a record length of zero is also legal and is
  // not an error but rather indicates a tape mark.
  //
  //   After a successful read, the logical tape is left positioned at the
  // start (i.e. just before the header longword) of the next record.  In the
  // event of a BADTAPE, the tape position is lost and the caller must invoke
  // Rewind() before attempting to read again.
  //
  //   One final comment - In the tape image file the metadata is always stored
  // in little endian format and strictly speaking we should worry about that,
  // but since our only platfrom is a PC and that's little endian, we'll take
  // the Alfred E Neuman approach to programming...
  //--
  assert(IsOpen() && (cbMaxData > 0) && (cbMaxData <= MAXRECLEN));
  METADATA nRecLen1, nRecLen2;

  //   If the last operation we did on this image was a write, then first
  // sync the underlying file system buffers ...
  if (m_fWriteLast) {
    fseek(m_pFile, 0L, SEEK_CUR);  m_fWriteLast = false;
  }
  LOGF(TRACE, "  -> ReadForwardRecord, cbMaxData=%d, (before) pos=%d", cbMaxData, ftell(m_pFile));

  // Try to read the TAP record length header ...
  if (((uint32_t) ftell(m_pFile)) >= m_nFileSize) return EOTBOT;
  if (fread(&nRecLen1, sizeof(METADATA), 1, m_pFile) != 1) {
    CImageFile::Error("read forward header", errno);  return BADTAPE;
  }
//LOGF(TRACE, "  -> CTapeImageFile::ReadForwardRecord#1, nRecLen1=%d (0x%08X), pos=%d", nRecLen1, nRecLen1, ftell(m_pFile));

  //   In TAP format, the high order bits of the record length are reserved
  // for flagged tape errors.  We currently don't handle these...
  if ((nRecLen1 & ~RECLENMASK) != 0) {
    LOGF(ERROR, "forced error flag (0x%08X) on tape %s", nRecLen1, m_sFileName.c_str());
    return BADTAPE;
  }

  //   Sanity check the record length.  Also, if the record length is zero
  // then this is a tape mark and we can quit now ...
  ++m_nRecordCount;
  if (nRecLen1 == 0) return TAPEMARK;
  if (((size_t) nRecLen1) > cbMaxData) {
    LOGF(ERROR, "record length too long (%d bytes) on tape %s", nRecLen1, m_sFileName.c_str());
    return BADTAPE;
  }

  // Read the raw data ...
  if (fread(abData, 1, nRecLen1, m_pFile) != (size_t) nRecLen1) {
    CImageFile::Error("read forward data", errno);  return BADTAPE;
  }

  // Read the record length trailer and see if it matches the header ...
  if (fread(&nRecLen2, sizeof(METADATA), 1, m_pFile) != 1) {
    CImageFile::Error("read forward trailer 1", errno);  return BADTAPE;
  }
  if (nRecLen1 == nRecLen2) return nRecLen1;

  //   Some misguided TAP file writers round off the actual record to an even
  // number of bytes, even when the record length is odd.  There's no way to
  // tell in advance whether we have one of those, but if the record lengths
  // don't match, see if that's the problem.  Note that we just read 4 bytes,
  // so by skipping backward 3 bytes we effectively ignore the padding byte.
  fseek(m_pFile, -((int32_t) (sizeof(METADATA)-1)), SEEK_CUR);
  if (fread(&nRecLen2, sizeof(METADATA), 1, m_pFile) != 1) {
    CImageFile::Error("read forward trailer 2", errno);  return BADTAPE;
  }
//LOGF(TRACE, "  -> CTapeImageFile::ReadForwardRecord#2, nRecLen2=%d (0x%08X), pos=%d", nRecLen2, nRecLen2, ftell(m_pFile));
  if (nRecLen1 == nRecLen2) return nRecLen1;

  // Nope - it's a bad tape ...
  LOGF(ERROR, "header (0x%08X) and trailer (0x%08X) mismatch on tape %s", nRecLen1, nRecLen2, m_sFileName.c_str());
  return BADTAPE;
}

int32_t CTapeImageFile::ReadReverseRecord (uint8_t abData[], size_t cbMaxData)
{
  //++
  //   This routine is logically just like ReadForwardRecord(), except that
  // this time we read the _previous_ record.  In this case, the virtual tape
  // position is left at the beginning of the previous record.  The same error
  // return conditions apply here.
  //
  //   Note that calling ReadForwardRecord() and then immediately calling
  // ReadReverseRecord() should a) return exactly the same data,, and b) leave
  // the virtual tape in exactly the same position.
  //
  //   IMPORTANT!  The bytes returned by this routine will be ordered in the
  // "forward" direction - that is, they're in the same order as ReadForward
  // Record() would return when reading this same record.  That might seem
  // obvious, but remember that it's not what a real tape drive would do!
  // A real tape drive would have physically read the record backward, and all
  // the bytes would have therefore appeared in reverse order.  The host
  // software expects that, and somebody up above us in the chain needs to
  // compensate by reversing the order of the bytes in the record.
  //--
  assert(IsOpen() && (cbMaxData > 0) && (cbMaxData <= MAXRECLEN));
  METADATA nRecLen1, nRecLen2;
//LOGF(TRACE, "  -> CTapeImageFile::ReadReverseRecord, cbMaxData=%d, (before) pos=%d", cbMaxData, ftell(m_pFile));

  //  If we're already at the BOT, then fail.  Otherwise back up four bytes
  // and try to read the trailer from the previous record.  Note that since
  // the first thing we do here is always an fseek(), regardless, there is
  // never a need to worry about syncing the file system buffers ...
  if (IsBOT()) return EOTBOT;
  m_fWriteLast = false;
  fseek(m_pFile, -((int32_t) sizeof(METADATA)), SEEK_CUR);
  if (fread(&nRecLen2, sizeof(METADATA), 1, m_pFile) != 1) {
    CImageFile::Error("read reverse trailer", errno);  return BADTAPE;
  }
  fseek(m_pFile, -((int32_t) sizeof(METADATA)), SEEK_CUR);
//LOGF(TRACE, "  -> CTapeImageFile::ReadReverseRecord#1, nRecLen2=%d (0x%08X), pos=%d", nRecLen2, nRecLen2, ftell(m_pFile));

  // Check the record length, just as for read forward ...
  if ((nRecLen2 & ~RECLENMASK) != 0) {
    LOGF(ERROR, "forced error flag (0x%08X) on tape %s", nRecLen2, m_sFileName.c_str());
    return BADTAPE;
  }
  assert(m_nRecordCount > 0);  --m_nRecordCount;
  if (nRecLen2 == 0) return TAPEMARK;
  if (((size_t) nRecLen2) > cbMaxData) {
    LOGF(ERROR, "record length too long (%d bytes) on tape %s", nRecLen2, m_sFileName.c_str());
    return BADTAPE;
  }

  //   Instead of just skipping backwards and trying to read the data, we skip
  // backwards four extra bytes and first try to read the header.  We do this
  // just in case we have one of those funky, padded record length, TAP files.
  // If we have one of those, we'll have to skip backwards an extra byte to get
  // aligned properly with the header and data.
  fseek(m_pFile, -((int32_t) (nRecLen2+sizeof(METADATA))), SEEK_CUR);
  if (fread(&nRecLen1, sizeof(METADATA), 1, m_pFile) != 1) {
    CImageFile::Error("read reverse header 1", errno);  return BADTAPE;
  }
  if (nRecLen1 != nRecLen2) {
    // The header and trailer don't match - offset by one byte and try again.
    fseek(m_pFile, -((int32_t) (sizeof(METADATA)+1)), SEEK_CUR);
    if (fread(&nRecLen1, sizeof(METADATA), 1, m_pFile) != 1) {
      CImageFile::Error("read reverse header 2", errno);  return BADTAPE;
    }
    if (nRecLen1 != nRecLen2) {
      // Nope - it's a bad tape ...
      LOGF(ERROR, "header (0x%08X) and trailer (0x%08X) mismatch on tape %s", nRecLen1, nRecLen2, m_sFileName.c_str());
      return BADTAPE;
    }
  }

  // Now we're ready to read the actual data (forwards, of course) ...
  if (fread(abData, 1, nRecLen2, m_pFile) != (size_t) nRecLen2) {
    CImageFile::Error("read reverse data", errno);  return BADTAPE;
  }

  // Skip back over the data AND the header and we're done ...
  fseek(m_pFile, -((int32_t) (nRecLen2+sizeof(METADATA))), SEEK_CUR);
//LOGF(TRACE, "  -> CTapeImageFile::ReadReverseRecord#2, newpos=%d", ftell(m_pFile));
  return nRecLen2;
}

bool CTapeImageFile::Truncate()
{
  //++
  // Truncate the tape image to the current position.
  //--
  assert(IsOpen());
  if (IsReadOnly()) return false;
  fseek(m_pFile, 0L, SEEK_CUR);  m_fWriteLast = true;
  m_nFileSize = ftell(m_pFile);
  return SetFileLength(m_nFileSize);
}

bool CTapeImageFile::WriteRecord (uint8_t abData[], size_t cbData)
{
  //++
  //   This method writes a new data record at the current file position.
  // Unlike disks, tapes are not block rewritable and if this operation is
  // performed anywhere but the logical EOT it will result in the tape image
  // being truncated after the new record.  The current tape position should
  // be just after the end of a record (or just before the beginning of a
  // record, which amounts to the same thing!) otherwise the image will become
  // corrupted.
  //
  //   Note that there's something of an ambiguity in the TAP file format -
  // some misguided programs insist on rounding the record size up to an even
  // number of bytes.  These programs will write an extra, dummy, byte at the
  // end of odd length record.  We're not one of those and we write the record
  // just as it is.  The problem is that if we do this to an existing file it
  // could result in a file with half the records one way and half the other.
  // That won't prevent US from reading the file, since we're able to cope with
  // either format, but it might cause problems for other programs.
  //--
  assert(IsOpen() && (cbData > 0) && (cbData <= MAXRECLEN));
  METADATA nMeta = MKINT32(cbData);
  if (IsReadOnly()) return false;

  // If the last operation was a read, flush the file buffers first...
  if (!m_fWriteLast) {
    fseek(m_pFile, 0L, SEEK_CUR);  m_fWriteLast = true;
  }

  // Write the leading metadata, the data, and then the trailing metadata ...
  if (fwrite(&nMeta, sizeof(METADATA), 1, m_pFile) != 1)
    return CImageFile::Error("writing metadata (1)", errno);
  if (fwrite(abData, sizeof(uint8_t), cbData, m_pFile) != cbData)
    return CImageFile::Error("writing data", errno);
  if (fwrite(&nMeta, sizeof(METADATA), 1, m_pFile) != 1)
    return CImageFile::Error("writing metadata (2)", errno);

  // Truncate the file to the end of the new record and we're done!
  ++m_nRecordCount;
  if (!Truncate()) return false;
//LOGF(TRACE, "  -> CTapeImageFile::WriteRecord, cbData=%d, newpos=%d", cbData, ftell(m_pFile));
  return true;
}

bool CTapeImageFile::WriteMark ()
{
  //++
  //   This method will write a tape mark at the current file position.  The
  // tape image is then truncated after that point, EOT becomes true, and any
  // subsequent data in the original file is lost.  Note that the current tape
  // position should be just after the end of a record, otherwise the file
  // format will become corrupted!
  //
  //   Note that a tape mark is a special case of a record - there's no data
  // and only ONE metadata word ...
  //--
  assert(IsOpen());
  METADATA nMeta = TAPEMARK;
  if (IsReadOnly()) return false;
  if (!m_fWriteLast) {
    fseek(m_pFile, 0L, SEEK_CUR);  m_fWriteLast = true;
  }
  if (fwrite(&nMeta, sizeof(METADATA), 1, m_pFile) != 1)
    return CImageFile::Error("writing mark", errno);
  if (!Truncate()) return false;
  ++m_nRecordCount;
//LOGF(TRACE, "  -> CTapeImageFile::WriteMark, newpos=%d", ftell(m_pFile));
  return true;
}

int32_t CTapeImageFile::SpaceForwardRecord (int32_t nRecords)
{
  //++
  //   This routine will skip forward over nCount records.  It basically just
  // calls ReadForwardRecord() and discards the resulting data - it's no more
  // complicated than that.  It will stop when either the record count is
  // exhausted or a tape mark is read.  The real tape drive would also stop at
  // the physical EOT marker, but of course that doesn't apply to us.  The
  // number of records actually skipped, not counting the tape mark if any,
  // is returned.
  //--
  assert(IsOpen() && (nRecords > 0));
  uint8_t *pabData = DBGNEW uint8_t[MAXRECLEN];  METADATA ret, nCount;
  for (nCount = 0;  nCount < nRecords;  ++nCount) {
    ret = ReadForwardRecord(pabData, MAXRECLEN);
    if (ret <= 0) break;
  }
  delete []pabData;
//LOGF(TRACE, "  -> CTapeImageFile::SpaceForwardRecord, nRecords=%d, nCount=%d, ret=%d, newpos=%d", nRecords, nCount, ret, ftell(m_pFile));
  return (ret <= 0) ? ret : nCount;
}

int32_t CTapeImageFile::SpaceReverseRecord (int32_t nRecords)
{
  //++
  //   This routine will skip backward over nCount records.  It's just the
  // same as SpaceForwardRecord(), except that in this case we also stop
  // at the physical BOT.
  //--
  assert(IsOpen() && (nRecords > 0));
  uint8_t *pabData = DBGNEW uint8_t[MAXRECLEN];  METADATA ret, nCount;
  for (nCount = 0;  nCount < nRecords;  ++nCount) {
    ret = ReadReverseRecord(pabData, MAXRECLEN);
    if (ret <= 0) break;
  }
  delete []pabData;
//LOGF(TRACE, "  -> CTapeImageFile::SpaceReverseRecord, nRecords=%d, nCount=%d, ret=%d, newpos=%d", nRecords, nCount, ret, ftell(m_pFile));
  return (ret <= 0) ? ret : nCount;
}

int32_t CTapeImageFile::SpaceForwardFile (int32_t nFiles)
{
  //++
  //   This routine will space forward one or more files.  It works simply by
  // counting tape marks, so SpaceForwardFile(1) would leave the tape
  // positioned AFTER the next tape mark (i.e. at the start of the next file).
  // I assume that's the same place a real drive would have left the tape.
  // If it is successful it returns the number of files actually skipped, or
  // one of EOTBOT or BADTAPE if either of those conditions occur.
  //--
  assert(IsOpen() && (nFiles > 0));
  METADATA ret, nCount;
  for (nCount = 0; nCount < nFiles; ++nCount) {
    ret = SpaceForwardRecord(INT32_MAX);
    if (ret < 0) break;
  }
  return (ret <= 0) ? ret : nCount;
}

int32_t CTapeImageFile::SpaceReverseFile (int32_t nFiles)
{
  //++
  //   This is the same as SpaceForwardFile, except in the reverse direction.
  // In this case the tape is positioned just before the last tape mark read,
  // so (for example) a SpaceReverseFile(1) would leave the tape at the END
  // of the PREVIOUS file.  It's necessary to read forward over the tape mark
  // to get to the beginning of the current file.
  //--
  assert(IsOpen() && (nFiles > 0));
  METADATA ret, nCount;
  for (nCount = 0; nCount < nFiles; ++nCount) {
    ret = SpaceReverseRecord(INT32_MAX);
    if (ret < 0) break;
  }
  return (ret <= 0) ? ret : nCount;
}



///////////////////////////////////////////////////////////////////////////////
// CTextInputFile members ...
///////////////////////////////////////////////////////////////////////////////

bool CTextInputFile::Open (const string &sFileName, bool fReadOnly, int nShareMode)
{
  //++
  //   This method opens an input image file in translated ASCII text mode.
  // If the specified file does not exist, then this method always fails.
  // Note that the fReadOnly parameter is ignored - the file is always opened
  // for read only access, regardless.  The nShareMode parameter, however, is
  // implemented.
  //--
  assert(!sFileName.empty() && fReadOnly);
  if (nShareMode == 0)  nShareMode = SHARE_READ;
  m_sFileName = sFileName;  m_fReadOnly = true;  m_nShareMode = nShareMode;
  if (TryOpenAndLock("rt", m_nShareMode)) return true;
  return Error("opening", errno);
}

bool CTextInputFile::Read (char &ch)
{
  //++
  // Read one character from the file ...
  //--
  int ret = fgetc(m_pFile);
  if (ret != EOF) {ch = ret; return true;}
  if (!ferror(m_pFile)) return false;
  return Error("reading", errno);
}

bool CTextInputFile::FlushLine()
{
  //++
  // Flush the remainder of the current line, up to and including the "\n" ...
  //--
  char ch;
  //   Just call Read() until we find "\n" or we run out of data in the file.
  // It's not the fastest way, but it's easiest.
  do {
    if (!Read(ch)) return false;
  } while (ch != '\n');
  return true;
}

bool CTextInputFile::Read (char *pszBuffer, size_t cbBuffer)
{
  //++
  //   Read a string of cbBuffer-1 bytes from the file, or the reminder of the
  // line if it's shorter than that.  Note that this never reads more than one
  // line, and may read only a part of the current line depending on the buffer
  // length.  If the end of line is read, then the newline character will be
  // included in the buffer.
  //
  //    This is exactly the behavior of fgets() and there's nothing magic about
  // it - I'm just spelling out the details :-)
  //--
  assert(cbBuffer > 2);
  if (fgets(pszBuffer, MKINT32(cbBuffer), m_pFile) != NULL) return true;
  if (!ferror(m_pFile)) return false;
  return Error("reading", errno);
}

bool CTextInputFile::ReadLine (char *pszLine, size_t cbLine)
{
  //++
  // Read a line from the file, and strip any newline from the end!
  //
  //   Note that in this case cbLine refers to the actual length of the buffer.
  // As with fgets(), this routine will return at most cbLine-1 characters,
  // HOWEVER that needs to include a byte for the "\n" character.  This routine
  // will strip the newline before returning, but the buffer still needs space
  // to accomodate it.
  //
  //   This means that, say, if you want to read an 80 character card image
  // then caller's buffer MUST BE AT LEAST 82 BYTES!  That's one extra byte to
  // hold the newline and one extra for the null, EVEN THOUGH the string we
  // actually return will only use 81 bytes (80 characters plus the null).
  //
  //   Sorry - that's the way it is.  The only alternatives are to either use
  // fgetc() in place of fgets(), or to use a temporary buffer and then copy
  // the string.  Both are too expensive for the result.
  //--
  assert(cbLine > 2);
  if (!Read(pszLine, cbLine)) return false;
  size_t cb = strlen(pszLine);
  if ((cb > 0) && (pszLine[cb-1] == '\n')) pszLine[cb-1] = '\0';
  return true;
}

bool CTextInputFile::ReadRecord (char *pszLine, size_t cbLine, size_t cbRecLen, bool fPad)
{
  //++
  //   Read a fixed length line of exactly cbRecLen characters from this text
  // file.  If the actual line is shorter than cbLine AND fPad is true, then
  // pad the buffer with spaces.  If the actual line is longer, then issue a
  // warning and truncate the result.
  //
  //   As with ReadLine(), the cbLine parameter gives the actual size of the
  // caller's buffer, in bytes.  To work properly, cbLine must be at least 2
  // bytes more than cbRecLen - this allows for the trailing null character
  // and a temporary newline character (read the comments in ReadLine() to get
  // the whole story).  That means if you want to read an 80 character card
  // image, you need to pass an 82 character (or bigger) buffer.  Since this
  // is a bit awkward and error prone, that's why separate parameters are
  // required for the buffer size and record length!
  //--
  assert((cbRecLen > 0)  &&  (cbLine >= (cbRecLen+2)));

  //   Try to read as much as we can (stopping at a newline if necessary, of
  // course) and then see how long the line was by looking for the "\n".
  if (!Read(pszLine, cbLine)) return false;
  size_t cb = strlen(pszLine);

  if (cb > cbRecLen) {
    //   The actual line was longer than the record length.  Just terminate the
    // buffer at the record length, but also be sure to flush the rest of the
    // rest of the text in the file up to the newline.
    if (pszLine[cb-1] != '\n') {
      LOGF(WARNING, "record \"%10.10s...\" truncated on %s", pszLine, m_sFileName.c_str())
        if (!FlushLine()) return false;
    }
    pszLine[cb-1] = '\0';
  } else if (fPad) {
    //   The actual line from the file was shorter than the record length, so
    // pad the remainder of the buffer with spaces.  Note that the extra "-1"
    // adjustment overwrites the newline character, which also appears in the
    // buffer, with a space.
    memset(pszLine+cb-1, ' ', cbRecLen-cb+1);
    pszLine[cbRecLen] = '\0';
  } else {
    //   No padding is desired, so just remove the newline from the end of the
    // buffer and we're done...
    if (cb > 0) pszLine[cb-1] = '\0';
  }
  return true;
}



///////////////////////////////////////////////////////////////////////////////
// CTextOutputFile members ...
///////////////////////////////////////////////////////////////////////////////

bool CTextOutputFile::Open (const string &sFileName, bool fReadOnly, int nShareMode)
{
  //++
  //   This method opens an output image file in ASCII translated text mode.
  // Note that the fReadOnly parameter is ignored and the file is always opened
  // for writing (nShareMode, however, is implemented).  If a file with the
  // same name already exists, then the new data will be appended to it.
  //--
  assert(!sFileName.empty() && !fReadOnly);
  if (nShareMode == 0)  nShareMode = SHARE_NONE;
  m_sFileName = sFileName;  m_fReadOnly = false;  m_nShareMode = nShareMode;
  if (TryOpenAndLock("a+t", m_nShareMode)) return true;
  return Error("creating", errno);
}

bool CTextOutputFile::Write (char ch, size_t cb)
{
  //++
  //   Write a single character to the file.  If cb > 1, then write multiple
  // copies of the same character (useful for padding lines!).
  //--
  for (uint32_t i = 0;  i < cb;  ++i) {
    if (fputc(ch, m_pFile) == EOF) return Error("writing", errno);
  }
  return true;
}

bool CTextOutputFile::Write (const char *psz)
{
  //++
  // Write a string (without newline!) to the file ...
  //--
  if (fputs(psz, m_pFile) != EOF) return true;
  return Error("writing", errno);
}

bool CTextOutputFile::WriteFixed (char *pszLine, size_t cbLine)
{
  //++
  //   Write a fixed length line of exactly cbLine characters.  If the buffer
  // pointed to by pszLine is shorter than this, then the text written will be
  // padded with spaces.  If the actual buffer is longer, then the remainder
  // will be silently truncated.
  //
  //   Note that this routine DOES NOT add a newline - use WriteFixedLine()
  // for that.
  //--
  size_t cb = strlen(pszLine);
  if (cb <= cbLine) {
    //   In this case we get to write the whole buffer, so we can use the more
    // efficient Write() method instead ...
    if (!Write(pszLine)) return false;
    if (cb < cbLine) return Write(' ', cbLine-cb);
    return true;
  } else {
    //   We have to truncate the caller's buffer.  The easiest way to do this
    // is just to write it one character at a time - it's kind of slow, but
    // hopefully this doesn't happen often...
    for (uint32_t i = 0;  i < cbLine;  ++i) {
      if (!Write(pszLine[i])) return false;
    }
    return true;
  }
}



///////////////////////////////////////////////////////////////////////////////
// CCardInputImageFile members ...
///////////////////////////////////////////////////////////////////////////////

/*static*/ bool CCardInputImageFile::IsBinaryFile (const string &sFileName)
{
  //++
  //   This routine will attempt to open an image file, feel it up to see
  // if it's really a binary card image file or not, and then close it.
  // It returns TRUE if we're sure the file is a binary file, and FALSE if
  // it's not or if we just can't tell.
  //--
  assert(!sFileName.empty());
  FILE *pFile;  uint8_t abHeader[FILE_HEADER_LEN];

  // Try to open the file and just give up if we can't ...
  if (fopen_s(&pFile, sFileName.c_str(), "rb") != 0) return false;

  // Read and try to verify the file header ...
  bool fIsBinary = fread(abHeader, FILE_HEADER_LEN, 1, pFile) == 1;
  fclose(pFile);
  fIsBinary &= (abHeader[0]=='H') && (abHeader[1]=='8') && (abHeader[2]=='0');
  return fIsBinary;
}

CCardInputImageFile::CCardInputImageFile()
{
  //++
  // The constructor just initializes all class members ...
  //--
  memset(m_abFileHeader, 0, sizeof(m_abFileHeader));
  memset(m_abCardHeader, 0, sizeof(m_abCardHeader));
}

bool CCardInputImageFile::Open (const string &sFileName, bool fReadOnly, int nShareMode)
{
  //++
  //   This method will open the card image file and then read the file header
  // (the "magic number") at the beginning. At the moment the only file format
  // we support is "H80" (i.e. 80 column card images) and the header must
  // agree with that.  Anything else causes an error and failure!
  //
  //   Note that the fReadOnly parameter must always be true (this class is a
  // read only class, after all!).
  //--
  assert(!sFileName.empty() && fReadOnly);
  if (!CImageFile::Open(sFileName, true, nShareMode)) return false;

  // Read and verify the file header and we're done ...
  if (fread(m_abFileHeader, FILE_HEADER_LEN, 1, m_pFile) != 1) {
      CImageFile::Error("reading file header", errno);  goto badheader;
  }
  if ((m_abFileHeader[0]!='H') || (m_abFileHeader[1]!='8') || (m_abFileHeader[2]!='0')) {
    LOGF(DEBUG, "found card file header 0x%02X 0x%02X 0x%02X",
      m_abFileHeader[0], m_abFileHeader[1], m_abFileHeader[2]);
    CImageFile::Error("bad card file header", errno);  goto badheader;
  }
  return true;

  // Here if the file header is bad for any reason ...
badheader:
  Close();  return false;
}

void CCardInputImageFile::Pack (uint16_t *pawCard, size_t cwCard, const uint8_t *pabCard, size_t cbCard)
{
  //++
  //   This method will pack an array of 8 bit bytes into a array of 12 bit
  // card column images using Doug Jones' standard "3 for 2 big endian" packing
  // format.  Note that the size of the output card buffer must be at least
  // (cbCard/3*2) words.
  //
  // WARNING - this currently doesn't handle cards with an odd number of columns!
  //--
  assert((pawCard != NULL) && (pabCard != NULL));
  assert(((cbCard%3) == 0) && (cwCard >= (cbCard/3*2)));
  for (uint32_t i = 0;  i < cbCard;  i += 3, pabCard += 3) {
    *pawCard++ = (*pabCard << 4) | (*(pabCard+1) >> 4);
    *pawCard++ = ((*(pabCard+1) & 0xF) << 8) | *(pabCard+2);
  }
}

size_t CCardInputImageFile::Read (uint16_t awCard[], size_t cwCard)
{
  //++
  //   This routine will read and return the next card image from the file.
  // The function return value is the number of card columns read (which will
  // always be 80 in the current implementation) or 0 if we find EOF or any
  // I/O error.  It also reads, and mostly ignores, the card image header.
  // This gives metadata about the physical appearance of the card, but we
  // only care that the header is present and not what it contains.
  //--
  assert(cwCard == COLUMNS);
  uint8_t abCard[CARDBYTES+CARD_HEADER_LEN];

  //   Read the next card image.  Records in Doug's file format are always
  // fixed length, so it's all or nothing - if we can't read the entire record
  // then something is screwed up...
  if (fread(abCard, 1, sizeof(abCard), m_pFile) != sizeof(abCard)) {
    if (!IsEOF()) CImageFile::Error("reading card image", errno);
    return 0;
  }

  //   Check that the header bytes are valid.  All we really do is check that
  // the MSB of each byte is 1 - all the rest we don't care about.  Note that
  // we save the header bytes in m_abCardHeader just in case somebody wants
  // to use them in the future ...
  assert(CARD_HEADER_LEN == 3);
  if (!(ISSET(abCard[0],0x80) && ISSET(abCard[1],0x80) && ISSET(abCard[2],0x80))) {
    LOGF(DEBUG, "found card image header 0x%02X 0x%02X 0x%02X",
         abCard[0], abCard[1], abCard[2]);
    CImageFile::Error("bad card image header", errno);  return 0;
  }
  memcpy(m_abCardHeader, abCard, CARD_HEADER_LEN);

  // Pack the 8 bit bytes into 12 bit card columns and we're done ...
  Pack(awCard, cwCard, &abCard[CARD_HEADER_LEN], CARDBYTES);
  return COLUMNS;
}



///////////////////////////////////////////////////////////////////////////////
// CCardOutputImageFile members ...
///////////////////////////////////////////////////////////////////////////////

CCardOutputImageFile::CCardOutputImageFile (uint32_t nColumns)
{
  //++
  //   This constructor takes one parameter which gives the number of card
  // columns in the image file we are creating.  Right now only 80 column cards
  // are supported, and any other number will fail.  It's included here anyway
  // just in case we ever want to expand to allow other card lengths.
  //--
  assert(nColumns == COLUMNS);
}

bool CCardOutputImageFile::Open (const string &sFileName, bool fReadOnly, int nShareMode)
{
  //++
  //   This method will open the card image file and then write the file header
  // (the "magic number") to the file  Note "write only" only class, after all!).
  //
  //   WARNING - there is no random access or "append to end" mode for card
  // image files.  If you open a file that already exists, it will be silently
  // truncated before writing the header.
  //--
  assert(!sFileName.empty() && !fReadOnly);
  if (!CImageFile::Open(sFileName, false, nShareMode)) return false;

  // If the file exists, truncate it now ...
  if ((GetFileLength() > 0) && !Truncate()) return false;

  // Write the file header and we're done ...
  uint8_t abHeader[FILE_HEADER_LEN] = {'H', '8', '0'};
  if (fwrite(abHeader, FILE_HEADER_LEN, 1, m_pFile) == 1) return true;
  CImageFile::Error("writing file header", errno);
  Close();  return false;
}

void CCardOutputImageFile::Unpack (uint8_t *pabCard, size_t cbCard, const uint16_t *pawCard, size_t cwCard)
{
  //   This method will unpack an array of 12 bit column images into a array of
  // 8 bit bytes using Doug Jones' standard "3 for 2 big endian" packing format.
  // Note that the size of the output buffer must be at least (cwCard*3/2) bytes.
  //
  // WARNING - this currently doesn't handle cards with an odd number of columns!
  //--
  assert((pawCard != NULL) && (pabCard != NULL));
  assert(((cwCard%2) == 0) && (cbCard >= (cwCard*3/2)));
  for (uint32_t i = 0;  i < cwCard;  i += 2, pawCard += 2) {
    *pabCard++ = (*pawCard >> 4) & 0xFF;
    *pabCard++ = ((*pawCard & 0xF) << 4) | ((*(pawCard+1) >> 8) & 0xFF);
    *pabCard++ = *(pawCard+1) & 0xFF;
  }
}

bool CCardOutputImageFile::Write (const uint16_t awCard[], size_t cwCard)
{
  //++
  //   This routine will write a card image to the file.  It unpacks 80 twelve
  // bit card image columns into binary, adds the card header, and writes it
  // out.  If any I/O error occurs, FALSE is returned.
  //
  //   Note that the card header is currently set to all zeros (except for the
  // MSB of each header byte, which must always be 1).  I think this translates
  // to something like "cream colored cards, round corners, no cut, unknown
  // character set, no interpretation, and no logo".  It's maybe not the most
  // useful setup, but I don't think anybody uses this data.
  //--
  assert(cwCard == COLUMNS);
  uint8_t abCard[CARDBYTES+CARD_HEADER_LEN];

  // Fake up the header bytes ...
  assert(CARD_HEADER_LEN == 3);
  abCard[0] = abCard[1] = abCard[2] = 0x80;

  // Unpack the 12 bit columns into 8 bit bytes ...
  Unpack(&abCard[CARD_HEADER_LEN], COLUMNS, awCard, cwCard);

  // Write it out and we're done ...
  if (fwrite(abCard, 1, sizeof(abCard), m_pFile) != sizeof(abCard))
    return CImageFile::Error("writing card image", errno);
  return true;
}
