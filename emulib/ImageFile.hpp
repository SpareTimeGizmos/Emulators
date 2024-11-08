//++
// ImageFile.hpp -> CImageFile (image file I/O for the emulator library) class
//                  CDiskImageFile (disk image file) subclass
//                  CTapeImageFile (tape image file) subclass
//                  CTextInputFile (unit record input file) subclass
//                  CTextOutputFile (unit record output file) subclass
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
//   A CImageFile object encapsulates a disk or tape image on the host file
// system.  A CDiskImage file is a derived class that represents a fixed
// length sector, random access, block rewritable (i.e. exactly what you'd
// expect from a disk drive!) file.  And conversely, a CTapeImageFile is a
// derived class that represents a variable record length, sequential access,
// non-rewritable (just what you'd want for a tape drive) file.
//
// Bob Armstrong <bob@jfcl.com>   [8-OCT-2013]
//
// REVISION HISTORY:
// 20-MAY-15  RLA   New file.
//  8-DEC-15  RLA   Add CTextInputFile and CTextOutputFile.
// 22-FEB-16  RLA   Add fixed length line support to CText?????ImageFile.
// 28-FEB-17  RLA   Make 64 bit clean.
//  1-JUN-17  RLA   Linux port.
// 26-AUG-22  RLA   Clean up Linux/WIN32 conditionals.
//  7-MAR-24  RLA   Add SetSectorSize for CDiskImageFile
//                  Add CHS addressing for CDiskImageFile
//--
#pragma once
#include <string>               // C++ std::string class, et al ...
using std::string;              // ...


class CImageFile {
  //++
  //   CImageFile is the base class for all disk and tape image files.  Notice
  // that this is an abstract class and it's not intended that anybody would
  // ever instantiate it directly.  There wouldn't be much point - it contains
  // no actual I/O methods!  All of those come from the derived disk or tape
  // classes.
  //--

  // Constants ...
public:
  //   These constants define platform independent names for shared file access.
  // On Windows we can just use the real constants for _fsopen(), but on Linux
  // we need to make up our own.  It's important that none of these values be
  // zero, even on Linux, so that we can distinguish the default argument value
  // in calls to Open().  Also notice that Linux has fewer options (at least if
  // you're using the flock() call) - we either lock the file or we don't.  In
  // that case the shared write and shared read options are the same!
  enum {
#if defined(_WIN32)
    SHARE_NONE  = _SH_DENYRW,    // no sharing (i.e. exclusive access)
    SHARE_READ  = _SH_DENYWR,    // shared reading, no writing
    SHARE_WRITE = _SH_DENYNO     // shared reading AND writing
#elif defined(__linux__)
    SHARE_NONE  = -1,           // no sharing (i.e. exclusive access)
    SHARE_WRITE =  1,           // or sharing (shared read and write!)
    SHARE_READ  = SHARE_WRITE   // there's no other option!
#elif defined(__APPLE__)
    SHARE_NONE  = 0,            // no sharing (i.e. exclusive access)
    SHARE_READ  = 1,            // shared reading
    SHARE_WRITE = 2             // shared writing
#elif defined(__unix__)
    SHARE_NONE  = 0,            // no sharing (i.e. exclusive access)
    SHARE_READ  = 1,            // shared reading
    SHARE_WRITE = 2             // shared writing
#endif
  };

public:
  // Constructor and destructor ...
  CImageFile();
  virtual ~CImageFile();
  // Disallow copy and assignment operations with CImageFile objects...
private:
  CImageFile(const CImageFile &f) = delete;
  CImageFile& operator= (const CImageFile &f) = delete;

  // Public methods ...
public:
  // Open or close the associated disk file ...
  virtual bool Open (const string &sFileName, bool fReadOnly=false, int nShareMode=0);
  virtual void Close();
  // Return various attributes (open, file name, read only, etc) ...
  bool IsOpen() const {return m_pFile != NULL;}
  bool IsReadOnly() const {return m_fReadOnly;}
  string GetFileName() const {return m_sFileName;}
  // Return true if we've hit the end of file ...
  bool IsEOF() const {return feof(m_pFile) != 0;}
  // Get the current file size or relative position (in bytes!) ...
  uint32_t GetFileLength() const;
  uint32_t GetFilePosition() const;
  // Set the file size, and truncate if necessary!
  bool SetFileLength (uint32_t nNewLength);
  // Truncate the file to the current position ...
  bool Truncate();

  // Local methods ...
protected:
  // Print a file related error message ...
  bool Error (const char *pszMsg, int nError) const;
  // Open the file and (on Linux) apply the file locking semantics ...
  bool TryOpenAndLock(const char *pszMode, int nShare);

  // Local members ...
protected:
  string   m_sFileName;   // name of the disk image file
  FILE    *m_pFile;       // handle of the image file
  bool     m_fReadOnly;   // TRUE if this image is R/O
  int      m_nShareMode;  // sharing mode for this file
};


class CDiskImageFile : public CImageFile {
  //++
  //   CDiskImageFile is the derived class for disk image files.  Disk images
  // have fixed length block/sector sizes, are block/sector rewritable, and
  // are random access.
  //--

public:
  // Constructor and destructor ...
  CDiskImageFile (uint32_t lSectorSize, uint16_t nCylinders=0, uint16_t nHeads=0, uint16_t nSectors=0);
  virtual ~CDiskImageFile() {};
  // Disallow copy and assignment operations with CDiskImageFile objects...
private:
  CDiskImageFile (const CDiskImageFile &f) = delete;
  CDiskImageFile& operator= (const CDiskImageFile &f) = delete;

  // Public methods ...
public:
  // Return the sector size.
  inline uint32_t GetSectorSize() const {return m_lSectorSize;}
  //   Set the sector size.  Note that we also reset the capacity to zero;
  // this will force it to be recalculated the next time GetCapacity()
  // is called...
  void SetSectorSize (uint32_t lSectorSize)
    {m_lSectorSize = lSectorSize;  m_lCapacity = 0;}
  // Set disk geometry ...
  void SetSectors   (uint16_t nSectors)   {m_nSectors = nSectors;}
  void SetHeads     (uint16_t nHeads)     {m_nHeads = nHeads;}
  void SetCylinders (uint16_t nCylinders) {m_nCylinders = nCylinders;}
  // Return geometry information ...
  inline uint16_t GetSectors()    const {return m_nSectors;   }
  inline uint16_t GetHeads()      const {return m_nHeads;     }
  inline uint16_t GetCylinders()  const {return m_nCylinders; }
  // Test C/H/S addresses for validity ...
  bool IsValidCylinder (uint16_t nCylinder) const {return nCylinder < GetCylinders();}
  bool IsValidHead (uint16_t nHead) const {return nHead < GetHeads();}
  bool IsValidSector (uint16_t nSector) const
    {return (nSector > 0) && (nSector <= GetSectors());}
  // Convert a C/H/S address to an absolute sector number ...
  enum {INVALID_SECTOR = 0xFFFFFFFFUL};
  bool IsValidCHS (uint16_t nCylinder, uint16_t nHead, uint16_t nSector) const;
  uint32_t CHStoLBA (uint16_t nCylinder, uint16_t nHead, uint16_t nSector) const;

  // Read or write sectors ...
  bool ReadSector (uint32_t lLBA, void *pData);
  bool ReadSector (uint16_t nCylinder, uint16_t nHead, uint16_t nSector, void *pData)
    {return ReadSector(CHStoLBA(nCylinder, nHead, nSector), pData);}
  bool WriteSector (uint32_t lLBA, const void *pData);
  bool WriteSector (uint16_t nCylinder, uint16_t nHead, uint16_t nSector, void *pData)
    {return WriteSector(CHStoLBA(nCylinder, nHead, nSector), pData);}
  // Get or change the disk capacity (in sectors) ...
  uint32_t GetCapacity() const;
  bool SetCapacity (uint32_t lCapacity, bool fTruncate=false);
  //   Return the capacity calculated from the CHS information only.  If the
  // CHS details are not set, this will return zero.
  uint32_t GetCHScapacity() const {return (uint32_t) m_nCylinders*m_nHeads*m_nSectors;}

  // Local methods ...
protected:
  // Seek to a particular sector ...
  bool SeekSector (uint32_t lLBA);

  // Local members ...
protected:
  uint32_t m_lSectorSize;       // disk sector/block size
  uint16_t m_nSectors;          // sectors per track
  uint16_t m_nHeads;            // surfaces (heads) per cylinder
  uint16_t m_nCylinders;        // cylinders per drive
  uint32_t m_lCapacity;         // disk capacity (in sectors!)
};


class CTapeImageFile : public CImageFile {
  //++
  //   CTapeImageFile is the derived class for tape image files.  Tape images
  // have variable length records, are sequential access only, and individual
  // blocks cannot be rewritten.  Well, OK - a record can be overwritten but
  // doing so truncates the tape at that point.
  //--

  // Public constants ...
public:
  //   These are magic constants that can be returned by ReadForwardRecord(),
  // et al.  Some of them correspond directly to values in the simh metadata
  // and some are invented values that we use internally.
  typedef int32_t METADATA;     // metadata longword used by simh format
  enum : METADATA {
    //   Note that this code, and also some of the higher level tape controller
    // code in the various emulators, allocate buffers based on the maximum
    // record length.  You should resist the temptation to set MAXRECLEN to a
    // huge value!
    MAXRECLEN   =     60000UL,  // longest possible tape record (bytes)
    RECLENMASK  = 0x00FFFFFFL,  // mask for TAP file record length field
    //   These values are all error conditions.  They're assigned zero or
    // negative values to avoid conflicts with valid record lengths...
    TAPEMARK    =  0L,          // tape mark found
    EOTBOT      = -1L,          // tape is at EOT or BOT
    BADTAPE     = -2L           // bad TAP file format
  };

public:
  //  Constructor and destructor ...
  CTapeImageFile (bool f7Track=false);
  virtual ~CTapeImageFile() {};
  // Disallow copy and assignment operations with CTapeImageFile objects...
private:
  CTapeImageFile(const CTapeImageFile &f) = delete;
  CTapeImageFile& operator= (const CTapeImageFile &f) = delete;

  // Public methods ...
public:
  // Open the associated disk file ...
  virtual bool Open (const string &sFileName, bool fReadOnly=false, int nShareMode=0) override;
  // Test current tape position for EOT/BOT ...
  bool IsBOT() const;
  bool IsEOT() const;
  // Return the current tape position ...
  uint32_t GetRecordCount() const {return m_nRecordCount;}
  // Return the 7 track flag for this image ...
  bool Is7Track() const {return m_f7Track;}
  // Read and write records ...
  int32_t ReadForwardRecord (uint8_t pabData[], size_t cbMaxData);
  int32_t ReadReverseRecord (uint8_t pabData[], size_t cbMaxData);
  bool Truncate();
  bool WriteMark();
  bool WriteRecord (uint8_t pabData[], size_t cbData);
  // Rewind the tape ...
  bool Rewind();
  // Skip records or files in either direction ...
  int32_t SpaceForwardRecord(int32_t nRecords=1);
  int32_t SpaceReverseRecord (int32_t nRecords=1);
  int32_t SpaceForwardFile (int32_t nFiles=1);
  int32_t SpaceReverseFile (int32_t nFiles=1);

  // Local methods ...
protected:

  // Local members ...
protected:
  //    This local keeps track of the current tape position by counting the
  // number of records from the start.  It's reset to zero at BOT, incremented
  // for every record read forward and decremented for every record read in
  // reverse.  For the purposes of this count, tape marks count as records.
  uint32_t  m_nRecordCount;     // current tape position in records ....
  //   The C standard contains this subtle but important footnote - "output
  // may not be directly followed by input without an intervening call to
  // a file positioning function (fseek, fsetpos, or rewind), and input may
  // not be directly followed by output without an intervening call to a
  // file positioning function ..."  Failure to follow this rule will result
  // in a screwed up file!
  bool      m_fWriteLast;       // TRUE if the last operation was a write
  //   And this local keeps track of the current file size.  It's updated every
  // time we write or truncate, and it's used to determine EOT when reading.
  uint32_t  m_nFileSize;        // total number of bytes in this file
  //   Seven track image files are treated EXACTLY the same as 9 track, except
  // that the upper bit of every byte is zeroed when reading or writing.  The
  // file format is exactly the same otherwise.
  //
  //   Actually to be honest at the moment this flag isn't even used, but it
  // is carried thru correctly in the rest of the CDC code (the only thing that
  // currently supports 7 track drives!) so it could be implemented someday
  // should we need it.
  bool      m_f7Track;          // TRUE for 7 track images
};


class CTextInputFile : public CImageFile {
  //++
  //   CTextInputFile is the derived class for input only unit record devices
  // (e.g. card readers) in translated ASCII text mode.  All I/O is sequential
  // and the only operations are to read characters and lines.
  //--

public:
  //  Constructor and destructor ...
  CTextInputFile() {};
  virtual ~CTextInputFile() {};
  // Disallow copy and assignment operations with CTextInputFile objects...
private:
  CTextInputFile (const CTextInputFile &f) = delete;
  CTextInputFile& operator= (const CTextInputFile &f) = delete;

  // Public methods ...
public:
  // Open an input file ...
  virtual bool Open (const string &sFileName, bool fReadOnly=true, int nShareMode=0);
  // Read characters or lines from the file ...
  bool Read (char &ch);
  bool Read (char *pszBuffer, size_t cbBuffer);
  bool ReadLine (char *pszLine, size_t cbLine);
  bool ReadRecord (char *pszLine, size_t cbLine, size_t cbRecLen, bool fPad=true);
  bool FlushLine();
};


class CTextOutputFile : public CImageFile {
  //++
  //   CTextOutputFile is the derived class for output only unit record devices
  // (e.g. card punches or line printers) in translated ASCII text mode.  All
  // I/O is sequential and the only operations are to write characters and lines.
  //--

public:
  //  Constructor and destructor ...
  CTextOutputFile() {};
  virtual ~CTextOutputFile() {};
  // Disallow copy and assignment operations with CTextOutputFile objects...
private:
  CTextOutputFile (const CTextOutputFile &f) = delete;
  CTextOutputFile& operator= (const CTextOutputFile &f) = delete;

  // Public methods ...
public:
  // Open an output file ...
  virtual bool Open (const string &sFileName, bool fReadOnly=false, int nShareMode=0) override;
  // Write characters, strings or lines to the file ...
  bool Write (char ch, size_t cb=1);
  bool WriteLine() {return Write('\n');}
  bool Write (const char *psz);
  bool WriteLine (const char *psz) {return Write(psz) && WriteLine();}
  bool WriteFixed (char *pszLine, size_t cbLine);
  bool WriteRecord (char *psz, size_t cb) {return WriteFixed(psz,cb) && WriteLine();}
};


class CCardInputImageFile : public CImageFile {
  //++
  //   CCardInputImageFile is the derived class for card image files using the
  // Doug Jones card image format. All I/O is sequential and the only supported
  // operation is to read the next card.
  //
  //   For more information on Doug Jones' card image file format, see
  //     http://homepage.cs.uiowa.edu/~jones/cards/format.html
  //
  //   Note that Doug's card format is quite elaborate and allows for card
  // images with other than 80 columns (e.g. 81, 82, 50, etc) and it also for
  // recording various metadata about the card color (yes, the color!), the
  // corner cut style, the corporate logo, any special markings on the card,
  // etc.  The current implementation of this class is compatible with that
  // format, however only 80 column cards are supported and any metadata about
  // the card's physical characteristics is ignored.
  //--

  // Public constants ...
public:
  enum {
    COLUMNS   =          80,  // number of card columms supported
    CARDBYTES = COLUMNS*3/2,  // number of bytes required for one card image
    CARD_HEADER_LEN =     3,  // number of bytes in each record header
    FILE_HEADER_LEN =     3,  // number of bytes in the file header
  };

public:
  //  Constructor and destructor ...
  CCardInputImageFile();
  virtual ~CCardInputImageFile() {};
  // Disallow copy and assignment operations with CCardInputImageFile objects...
private:
  CCardInputImageFile(const CCardInputImageFile &f) = delete;
  CCardInputImageFile& operator= (const CCardInputImageFile &f) = delete;

  // Public methods ...
public:
  // Test whether a file is really a binary file ...
  static bool IsBinaryFile (const string &sFileName);
  // Open an input file ...
  virtual bool Open (const string &sFileName, bool fReadOnly=true, int nShareMode=0) override;
  // Read a card image ...
  size_t Read (uint16_t awCard[], size_t cwCard);

  // Local methods ...
protected:
  // Pack 8 bit bytes into a 12 bit card image ...
  void Pack (uint16_t *pawCard, size_t cwCard, const uint8_t *pabCard, size_t cbCard);

  // Local members ...
protected:
  //   These members remember the file header bytes (the magic number 'H80')
  // and the header bytes from the last card record read.  The record header
  // bytes contain all kinds of information defined by Doug Jones about the
  // card color, the corner cut, the logo, whether the card is interpreted,
  // etc.  To be honest, I don't think anybody ever uses any of this data, but
  // it's captured here just in case you should want to.
  uint8_t m_abFileHeader[FILE_HEADER_LEN];
  uint8_t m_abCardHeader[CARD_HEADER_LEN];
};


class CCardOutputImageFile : public CImageFile {
  //++
  //   CCardInputImageFile is the derived class for card image files using the
  // Doug Jones card image format. All I/O is sequential and the only supported
  // operation is to write the next card.
  //
  //   Note that this class uses a lot of the common parameters (e.g. format
  // magic numbers, default number of columns, etc) from the CCardInputImageFile
  // class.
  //--

  // Public constants ...
public:
  enum {
    //   These constants are all identical to their CCardInputImageFile
    // counterparts.  They're duplicated here just so that we don't have
    // to type "CCardInputImageFile::" all the time!
    COLUMNS         = CCardInputImageFile::COLUMNS,
    CARDBYTES       = CCardInputImageFile::CARDBYTES,
    FILE_HEADER_LEN = CCardInputImageFile::FILE_HEADER_LEN,
    CARD_HEADER_LEN = CCardInputImageFile::CARD_HEADER_LEN,
  };

public:
  //  Constructor and destructor ...
  CCardOutputImageFile (uint32_t nColumns=COLUMNS);
  virtual ~CCardOutputImageFile() {};
  // Disallow copy and assignment operations with CCardOutputImageFile objects...
private:
  CCardOutputImageFile (const CCardOutputImageFile &f) = delete;
  CCardOutputImageFile& operator= (const CCardOutputImageFile &f) = delete;

  // Public methods ...
public:
  // Open an output file ...
  virtual bool Open (const string &sFileName, bool fReadOnly=true, int nShareMode=0) override;
  // Write a card image ...
  bool Write (const uint16_t awCard[], size_t cwCard);

  // Local methods ...
protected:
  // Unpack a 12 bit card image into 8 bit bytes ...
  void Unpack (uint8_t *pabCard, size_t cbCard, const uint16_t *pawCard, size_t cwCard);

  // Local members ...
protected:
};
