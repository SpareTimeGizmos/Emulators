//++
// CommandParser.hpp -> CCmdParser (command line parser) class
//			CCmdVerb (command line verb) class
//                      CCmdArgument (CCmdArgKeyword, CCmdArgNumber, etc) class
//                      CCmdModifier class
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
//   This header contains a number of class definitions, all of which are used
// to implement a simplistic command line parser.  Many of the classes (e.g.
// CCmdArgument, CCmdModifier, CCmdVerb) define objects that are command line
// entities, and various tables of these objects are used to define the entire
// command language.  Lastly, the CCmdParser class defines an object which
// reads and parses entire command files and UI sessions.
//
// COMMAND PARSER OBJECT SUMMARY
//   CCmdParser       - parse an entire command line
//   CCmdVerb         - parse a command verb
//   CCmdModifier     - parse a modifier (e.g. "/name" or "/name=value")
//   CCmdAliases      - define aliases for commands
//
// Verb and Modifier arguments -
//   CCmdArgument           - base class for all arguments
//   CCmdArgName            - name or identifier (e.g. DISK)
//   CCmdArgNumber          - number in any radix (e.g. 1234)
//   CCmdArgKeyword         - name from a list of predefined keywords
//   CCmdArgString          - possibly quoted string (e.g. "this is a string")
//   CCmdArgFileName        - file name, which we can optionally open
//   CCmdArgPCIAddress      - PCI address in BDF (e.g. "bus:domain.function")
//   CCmdArgDiskAddress     - disk address in CHS (e.g. "(1,2,3)") 
//   CCmdArgNetworkAddress  - dotted IP address, with optional port (e.g. 172.16.1.213:80)
//   CCmdArgNumberRange     - address range (e.g. "0x0100-0x01FF")
//   CCmdArgRangeOrName     - address range or name
//   CCmdArgList            - any of the above arguments repeated in a list
//
// Bob Armstrong <bob@jfcl.com>   [20-MAY-2015]
//
// REVISION HISTORY:
// 20-MAY-15  RLA   New file.
//  8-JUN-15  RLA   Add a Reset() to CCmdArgPCIAddress that clears the valid flag.
// 17-JUN-15  RLA   Add command alias support.
// 12-OCT-15  RLA   Make HELP command definition global.
// 22-OCT-15  RLA   Allow script files to be nested.
// 13-SEP-16  RLA   Add CCmdArgNetworkAddress.
// 28-FEB-17  RLA   Make 64 bit clean.
//  1-JUN-17  RLA   Linux port.
// 12-JAN-20  RLA   Add CCmdArgList repeated argument type.
//                  Add ParseError() et al to get better error messages.
// 14-JAN-20  RLA   Add CCmdArgNumberRange and CCmdArgNameOrNumber
// 25-AUG-22  RLA   Be more careful about private copy and assignment constructors
//--
#pragma once
#include <string>               // C++ std::string class, et al ...
#include <iostream>             // C++ style output for LOGS() ...
#include <unordered_map>        // C++ std::unordered_map (aka hash table) template
#include <vector>               // C++ std::vector template
using std::string;              // ...
using std::ostream;             // ...
using std::unordered_map;       // ...
using std::vector;              // ...
#include "SafeCRT.h"		// needed for _itoa_s() on Linux!
class CCmdLine;                 // we need a pointer to this class
class CCmdParser;               //   ... and this ...
class CConsoleWindow;           //   ... and this ...


//++
//  The CCmdArgument object represents a single command line argument - in the
// simplest case this is any string of characters delimited by white space,
// EOS, or a modifier (e.g. "/") character.  There are derivatives of this
// class - e.g. CCmdArgNumeric, CCmdArgKeyword, CCmdArgName, etc - that have
// more complex syntax.  
//
//   Note that the usage of an "argument" is not limited to a positional
// command line operand.  The values associated with modifiers are "arguments"
// too - for example, the "RP04" in "/TYPE=RP04" is a CCmdArgKeyword, and the
// 12345 in "/SERIAL_NUMBER=12345" is a CCmdArgNumeric.  The actual argument
// objects themselves do not distingush these different usages.
//
//   Lastly, note that CCmdArgument objects store the argument value and other
// information about the parse (e.g. whether the argument is present) in member
// data.  As a result these objects can't be made "const" - that's unfortunate
// since command parsing tables are usually thought of as constant. However all
// the things that shouldn't change (e.g. the argument name) don't have methods
// that allow them to change, and are for practical purposes immutable.
//--
class CCmdArgument {
  //++
  //   A generic command argument is any string of characters up to the next
  // white space, EOS or modifier character (e.g. "/")....
  //--

  // Constructor and destructor ...
public:
  CCmdArgument(const char *pszName, bool fOptional=false)
    : m_pszName(pszName), m_fOptional(fOptional)
      {Reset();}
  virtual ~CCmdArgument() {};
protected:
  // Disallow assignments ...
  CCmdArgument& operator= (CCmdArgument const&) = delete;
  // And use the Clone() method to create copies ...
  CCmdArgument (const CCmdArgument &ref)
    : m_pszName(ref.m_pszName), m_fOptional(ref.m_fOptional),
      m_sValue(ref.m_sValue), m_pszError(ref.m_pszError)
      {/*std::cout << "CCmdArgument copy constructor!" << std::endl;*/}

  // Properties ...
public:
  const char *GetName() const {return m_pszName;}
  string GetValue() const {return m_sValue;}
  void ClearValue() {m_sValue.clear();}
  void SetValue(const char *psz) {m_sValue.assign(psz);}
  void SetValue(const string &s) {m_sValue.assign(s);}
  void SetValue(const char *pcStart, const char *pcEnd);
  void SetValue(const char *pcStart, size_t nLen);
  bool IsPresent() const {return !m_sValue.empty();}
  bool IsOptional() const {return m_fOptional;}
  const char *GetError() const {return m_pszError;}
  void SetError (const char *psz) {m_pszError = psz;}
  void ClearError() {SetError(NULL);}
//virtual bool IsValid() const {return true;}
  virtual void Reset() {m_sValue.clear();  ClearError();}

  // Parse functions ...
protected:
  static string ScanToken (const char *&pcNext);
  static string ScanQuoted (const char *&pcNext);
public:
  virtual bool Parse (const char *&pcNext);

  // Other methods ...
public:
  //   This creates a new instance which is an exact copy of this object,
  // including any argument which may have already been parsed.  Yes, the copy
  // constructor does more or less the same thing, however Clone() can be made
  // virtual and a copy constructor cannot.  This allows us to clone a generic
  // CCmdArgument pointer and faithfully create any derived object.
  virtual CCmdArgument *Clone() const {return DBGNEW CCmdArgument(*this);}
  //   This method allows us to return a more specific error message in the
  // event that Parse() fails.  It saves a pointer to the error string and
  // then always returns false itself.
  bool ParseError (const char *pszError);
  //   And this method validates the argument.  This is a semantic rather than
  // a syntactic one, and is performed after parsing is completed.  It will
  // range check numeric values; verify that a keyword is a known name, etc.
  virtual bool Validate() const {return true;}

  // Private member data ...
private:
  const char *m_pszName;      // argument name (e.g. "unit" or "file name")
  const bool  m_fOptional;    // TRUE if this argument is optional
  string      m_sValue;       // the actual value of this argument
  const char *m_pszError;     // last parse error message (if any!)
};

//   This inserter allows you to send a command argument (it prints the
// actual argument value) directly to an I/O stream for error messages ...
// Needless to say, it works for all the derived classes too!
inline ostream& operator << (ostream &os, const CCmdArgument &arg)
  {os << arg.GetValue();  return os;}


class CCmdArgName : public CCmdArgument {
  //++
  //   A "name" argument is any string of alphanumeric characters, including
  // "_" and "$" ...
  //--
  friend class CCmdArgRangeOrName;

  // Constructors ...
public:
  CCmdArgName(const char *pszName, bool fOptional=false)
    : CCmdArgument(pszName, fOptional)
      {};
  virtual ~CCmdArgName() {};
protected:
  // Disallow assignments ...
  CCmdArgName& operator= (const CCmdArgName&) = delete;
  // And use the Clone() method to create copies ...
  CCmdArgName(const CCmdArgName &ref)
    : CCmdArgument(ref)
      {/*std::cout << "CCmdArgName copy constructor!" << std::endl;*/}

  // Parse functions ...
public:
  static string ScanName (const char *&pcNext);
  virtual bool Parse (const char *&pcNext) override;
  virtual CCmdArgument *Clone() const override {return DBGNEW CCmdArgName(*this);}
};


class CCmdArgNumber : public CCmdArgument {
  //++
  //   A numeric argument is what you'd expect - any string of digits.  The
  // exact syntax is determined by the radix specified and whatever strtoul()
  // likes...
  //
  //   N.B.  This is always a 32 bit type, even on a 64 bit platform!!!!
  //--
  friend class CCmdArgNumberRange;

  // Constructors ...
public:
  CCmdArgNumber(const char *pszName, uint32_t nRadix=10, uint32_t nMin=0, uint32_t nMax=UINT32_MAX, bool fOptional=false)
    : CCmdArgument(pszName, fOptional), m_nRadix(nRadix), m_nMin(nMin), m_nMax(nMax)
      {};
  virtual ~CCmdArgNumber() {};
protected:
  // Disallow assignments ...
  CCmdArgNumber& operator= (const CCmdArgNumber &) = delete;
  // And use the Clone() method to create copies ...
  CCmdArgNumber(const CCmdArgNumber &ref)
    : CCmdArgument(ref), m_nRadix(ref.m_nRadix), m_nMin(ref.m_nMin), m_nMax(ref.m_nMax)
      {/*std::cout << "CCmdArgNumber copy constructor!" << std::endl;*/}

  // Properties ...
public:
  //   Note that SetValue() and GetValue() in the base class set or return
  // the argument as a string - these functions convert from/to binary...
  void SetNumber (uint32_t nValue)
    {char sz[64];  _itoa_s(nValue, sz, sizeof(sz), m_nRadix);  SetValue(string(sz));}
  uint32_t GetNumber() const
    {return strtoul(GetValue().c_str(), NULL, m_nRadix);}
  virtual CCmdArgument *Clone() const override {return DBGNEW CCmdArgNumber(*this);}

  // Parse functions ...
protected:
  static string ScanNumber (const char *&pcNext, uint32_t nRadix=0);
public:
  virtual bool Parse (const char *&pcNext) override;
  virtual bool Validate() const override;

  // Private member data ...
private:
  const uint32_t m_nRadix;    // default radix for the number
  const uint32_t m_nMin;      // minimum legal value
  const uint32_t m_nMax;      // maximum legal value
};


class CCmdArgKeyword : public CCmdArgument {
  //++
  //   A keyword argument is a name argument where the name is constrained
  // to be an element of a particular set.  For example, "/TYPE=RP04" is a
  // keyword argument and the list of keywords includes "RP06", "RP04", "RM80",
  // etc.  Only arguments that match this list will be allowed.
  //
  //  The keyword_t structure allows the caller to build a list of keyword
  // names and the associated value.  Each keyword is allowed exactly one
  // value - an intptr_t, which you can use as you please.
  //--
public:
  struct _KEYWORD {
    const char *m_pszName;
    intptr_t    m_pValue;
  };
  typedef struct _KEYWORD KEYWORD;

  // Constructors ...
public:
  CCmdArgKeyword(const char *pszName, const KEYWORD *paKeys, bool fOptional=false)
    : CCmdArgument(pszName, fOptional), m_paKeys(paKeys)
      {Reset();}
  virtual ~CCmdArgKeyword() {};
protected:
  // Disallow assignments ...
  CCmdArgKeyword& operator= (const CCmdArgKeyword&) = delete;
  // And use the Clone() method to create copies ...
  CCmdArgKeyword (const CCmdArgKeyword &ref)
    : CCmdArgument(ref), m_paKeys(ref.m_paKeys), m_nIndex(ref.m_nIndex)
      {/*std::cout << "CCmdArgKeyword copy constructor!" << std::endl;*/}

  // Properties ...
private:
  bool IsValid() const {return IsPresent() && (m_nIndex >= 0);}
public:
  //   Like the CCmdArgNumber class, SetValue() and GetValue() set or return
  // the argument as a string.  This property looks up the name in the table
  // and returns the corresponding value ...
  intptr_t GetKeyValue() const {return IsValid() ? m_paKeys[m_nIndex].m_pValue : 0;}
  int GetKeyIndex() const {return IsValid() ? m_nIndex : -1;}
  virtual CCmdArgument *Clone() const override {return DBGNEW CCmdArgKeyword(*this);}

  // Parse functions ...
public:
  //   There are no parse functions for this type - since it's lexically just
  // a name, we can use the CCmdArgName::GetName() method.  There is, however,
  // a validate method that checks the value against the list of acceptable
  // ones...
  static bool Match (const char *pszToken, const char *pszKeyword);
  static bool Match (string strToken, const char *pszKeyword)
    {return Match(strToken.c_str(), pszKeyword);}
  static int Search (const char *pszToken, const KEYWORD *paKeys);
  static int Search (string strToken, const KEYWORD *paKeys)
    {return Search(strToken.c_str(), paKeys);}
  virtual bool Parse (const char *&pcNext) override;
  virtual void Reset() override {m_nIndex = -1;  CCmdArgument::Reset();}
  virtual bool Validate() const override;

  // Private member data ...
private:
  const KEYWORD *m_paKeys;    // list of keyword_t keywords
  int            m_nIndex;    // index of our key
};


class CCmdArgString : public CCmdArgument {
  //++
  //   A string argument is treated more or less the same as a generic name
  // argument - any string of characters except white space, EOS or a modifier
  // - except that a string also accepts a quoted string.  This allows special
  // characters - e.g. space or slash - to apper in a value.
  //--

  // Constructors ...
public:
  CCmdArgString(const char *pszName, bool fOptional=false)
    : CCmdArgument(pszName, fOptional)
      {};
  virtual ~CCmdArgString() {};
protected:
  // Disallow assignments ...
  CCmdArgString& operator= (const CCmdArgString &) = delete;
  // And use the Clone() method to create copies ...
  CCmdArgString(const CCmdArgString &ref)
    : CCmdArgument(ref)
      {/*std::cout << "CCmdArgString copy constructor!" << std::endl;*/}

  // Parse functions ...
public:
  virtual bool Parse (const char *&pcNext) override;
  virtual CCmdArgument *Clone() const override {return DBGNEW CCmdArgString(*this);}
};


class CCmdArgFileName : public CCmdArgString {
  //++
  //   Syntactically, a file name argument is the same as a stsring argument,
  // however a file name has a few extra semantics layered on top of that.
  //--

  // Constructors ...
public:
  CCmdArgFileName (const char *pszName, bool fOptional=false)
    : CCmdArgString(pszName, fOptional)
      {};
  virtual ~CCmdArgFileName() {};
protected:
  // Disallow assignments ...
  CCmdArgFileName& operator= (const CCmdArgFileName&) = delete;
  // And use the Clone() method to create copies ...
  CCmdArgFileName(const CCmdArgFileName &ref)
    : CCmdArgString(ref)
      {/*std::cout << "CCmdArgFileName copy constructor!" << std::endl;*/}

  // Properties ...
public:
  // Return the fully qualified path and file name ...
  string GetFullPath() const {return ::FullPath(GetValue());}
  // Return TRUE if the file exists ...
  bool FileExists() const {return ::FileExists(GetFullPath());}
  virtual CCmdArgument *Clone() const override {return DBGNEW CCmdArgFileName(*this);}

  // Special file name functions ...
public:
  FILE *OpenWrite (const char *pszMode = "a+t");
  FILE *OpenRead (const char *pszMode = "rt");
  // Are there any special filename specific semantic checks that we want to add??
  //virtual bool Validate() const override;
};


class CCmdArgPCIAddress : public CCmdArgument {
  //++
  //   This class will parse a PCI bus address in "bus:domain.function" (aka
  // BDF) notation and return the bus and slot numbers found.  The function is
  // optional, and is ignored.  The parsing isn't super smart, but it's good
  // enough for what we need...
  //--
  // Constructors ...
public:
  CCmdArgPCIAddress(const char *pszName, bool fOptional=false)
    : CCmdArgument(pszName, fOptional), m_fValid(false), m_nBus(0), m_nSlot(0), m_nFunction(0)
      {};
  virtual ~CCmdArgPCIAddress() {};
protected:
  // Disallow assignments ...
  CCmdArgPCIAddress& operator= (const CCmdArgPCIAddress &) = delete;
  // And use the Clone() method to create copies ...
  CCmdArgPCIAddress(const CCmdArgPCIAddress &ref)
    : CCmdArgument(ref), m_fValid(ref.m_fValid), m_nBus(ref.m_nBus), m_nSlot(ref.m_nSlot), m_nFunction(ref.m_nFunction)
      {/*std::cout << "CCmdArgPCIAddress copy constructor!" << std::endl;*/}

  // Properties ...
public:
  uint8_t GetBus() const {return LOBYTE(m_nBus);}
  uint8_t GetSlot() const {return LOBYTE(m_nSlot);}
  uint8_t GetFunction() const {return LOBYTE(m_nFunction);}
  virtual CCmdArgument *Clone() const override {return DBGNEW CCmdArgPCIAddress(*this);}

  // Parse functions ...
  //   Note that the Parse() function parses the BDF argument and sets BOTH
  // the individual bus, slot and function members in this class AND the
  // "value" in the CCmdArgument base class.  The latter contains the entire
  // BDF value as a single string.
protected:
  bool ScanBDF (const char *&pcNext, uint32_t &nBus, uint32_t &nSlot, uint32_t &nFunction);
public:
  virtual bool Parse (const char *&pcNext) override;
  virtual void Reset() override
    {m_fValid=false;  m_nBus=m_nSlot=m_nFunction=0;  CCmdArgument::Reset();}
  virtual bool Validate() const override;

  // Private member data ...
private:
  bool     m_fValid;          // true if the syntax was good
  uint32_t m_nBus;            // bus number
  uint32_t m_nSlot;           // slot number
  uint32_t m_nFunction;       // card subfunction index
};


class CCmdArgDiskAddress : public CCmdArgument {
  //++
  //   This class will parse a disk address as either a single integer (assumed
  // to be a logical block number), or as a separate cylinder, head and sector
  // address using the syntax "(c,h,s)" (i.e. three decimal numbers separated
  // by commas and enclosed in parenthesis. Note that no range checking is done
  // on either format, so any positive integer values will be accepted!
  //--

  // Constructors ...
public:
  CCmdArgDiskAddress(const char *pszName, uint32_t nMaxCylinder=UINT32_MAX, uint32_t nMaxHead=UINT32_MAX, uint32_t nMaxSector=UINT32_MAX, bool fOptional=false)
    : CCmdArgument(pszName, fOptional),
      m_fUseLBN(false), m_nMaxCylinder(nMaxCylinder), m_nMaxHead(nMaxHead),
      m_nMaxSector(nMaxSector), m_fValid(false), m_nBlock(0),
      m_nCylinder(0), m_nHead(0), m_nSector(0)
      {};
  virtual ~CCmdArgDiskAddress() {};
protected:
  // Disallow assignments ...
  CCmdArgDiskAddress& operator= (const CCmdArgDiskAddress &) = delete;
  // And use the Clone() method to create copies ...
  CCmdArgDiskAddress(const CCmdArgDiskAddress &ref)
    : CCmdArgument(ref), m_fUseLBN(ref.m_fUseLBN),
      m_nMaxCylinder(ref.m_nMaxCylinder), m_nMaxHead(ref.m_nMaxHead),
      m_nMaxSector(ref.m_nMaxSector), m_fValid(ref.m_fValid),
      m_nBlock(ref.m_nBlock), m_nCylinder(ref.m_nCylinder),
      m_nHead(ref.m_nHead), m_nSector(ref.m_nSector)
      {/*std::cout << "CCmdArgDiskAddress copy constructor!" << std::endl;*/}

  // Properties ...
public:
  bool IsCHS() const {return IsPresent() && !m_fUseLBN;}
  bool IsLBN() const {return IsPresent() &&  m_fUseLBN;}
  uint32_t GetCylinder() {ToCHS();  return m_nCylinder;}
  uint32_t GetHead()     {ToCHS();  return m_nHead;}
  uint32_t GetSector()   {ToCHS();  return m_nSector;}
  uint32_t GetBlock()    {ToLBN();  return m_nBlock;}
  virtual CCmdArgument *Clone() const override {return DBGNEW CCmdArgDiskAddress(*this);}

  // Parse functions ...
protected:
  bool ScanCHS (const char *&pcNext, uint32_t &nCyldiner, uint32_t &nHead, uint32_t &nSector);
public:
  virtual bool Parse (const char *&pcNext) override;
  virtual void Reset() override {
    m_fValid = m_fUseLBN = false;
    m_nBlock = m_nCylinder = m_nHead = m_nSector = 0;
    CCmdArgument::Reset();
  }
  virtual bool Validate() const override;

  // Other useful methods...
public:
  uint32_t ToLBN (uint32_t nCylinder, uint32_t nHead, uint32_t nSector) const;
  void ToCHS (uint32_t nLBN, uint32_t &nCylinder, uint32_t &nHead, uint32_t &nSector) const;
private:
  void ToLBN() {if (IsCHS()) m_nBlock = ToLBN(m_nCylinder, m_nHead, m_nSector);}
  void ToCHS() {if (IsLBN()) ToCHS(m_nBlock, m_nCylinder, m_nHead, m_nSector);}

  // Private member data ...
private:
  bool           m_fUseLBN;         // true if LBN format was used
  const uint32_t m_nMaxCylinder;    // largest cylinder allowed
  const uint32_t m_nMaxHead;        //     "   head        "
  const uint32_t m_nMaxSector;      //     "   sector      '
  bool           m_fValid;          // true if the syntax was good
  uint32_t       m_nBlock;          // logical block number
  uint32_t       m_nCylinder;       // cylinder number
  uint32_t       m_nHead;           // head number
  uint32_t       m_nSector;         // and sector
};


class CCmdArgNetworkAddress : public CCmdArgument {
  //++
  //   This class will parse a network address with the format "a.b.c.d:p" and
  // extract the IP address and port number.  It accepts several variations on
  // the same basic syntax -
  //
  //   a.b.c.d:p -> specify the IP address and port number
  //   a.b.c.d   -> specify only the IP and use the default port
  //   p         -> specify only the port number and use the default IP
  //   :p        -> ditto
  //
  //   The default IP address may be specified in the constructor and defaults
  // to zero (i.e. INADDR_ANY).  There is no "default" for the default port
  // number, and a default port number must be specified to the constructor.
  //
  //   Note that, for the moment at least, it accepts only dotted IP addresses
  // and not actual nost names or domains.  You could always add that later
  // if you need to.
  //--

  // Constructors ...
public:
  CCmdArgNetworkAddress(const char *pszName, uint16_t nDefaultPort, uint32_t lDefaultIP=0, bool fOptional=false)
    : CCmdArgument(pszName, fOptional), m_fValid(false),
      m_nDefaultPort(nDefaultPort), m_nPort(nDefaultPort),
      m_lDefaultIP(lDefaultIP), m_lIP(lDefaultIP)
      {};
  virtual ~CCmdArgNetworkAddress() {};
protected:
  // Disallow assignments ...
  CCmdArgNetworkAddress& operator= (const CCmdArgNetworkAddress &) = delete;
  // And use the Clone() method to create copies ...
  CCmdArgNetworkAddress(const CCmdArgNetworkAddress &ref)
    : CCmdArgument(ref), m_fValid(ref.m_fValid), m_nDefaultPort(ref.m_nDefaultPort),
      m_nPort(ref.m_nPort), m_lDefaultIP(ref.m_lDefaultIP), m_lIP(ref.m_lIP)
      {/*std::cout << "CCmdArgNetworkAddress copy constructor!" << std::endl;*/}

  // Properties ...
public:
  uint32_t GetIP() const {return m_lIP;}
  uint16_t GetPort() const {return m_nPort;}
  virtual CCmdArgument *Clone() const override {return DBGNEW CCmdArgNetworkAddress(*this);}

  // Parse functions ...
protected:
  bool ScanNetworkAddress (const char *&pcNext, uint16_t &nPort, uint32_t &lIP);
public:
  virtual bool Parse (const char *&pcNext) override;
  virtual void Reset() override {
    m_fValid = false;  m_lIP = m_lDefaultIP;  m_nPort = m_nDefaultPort;  CCmdArgument::Reset();
  }

  // Private member data ...
private:
  bool     m_fValid;          // true if the syntax was good
  uint16_t m_nDefaultPort;    // default port from the constructor
  uint16_t m_nPort;           // actual port specified
  uint32_t m_lDefaultIP;      // default IP from the constructor
  uint32_t m_lIP;             // actual IP specified
};


class CCmdArgNumberRange : public CCmdArgument {
  //++
  //   This argument accepts either a single number (i.e. exactly the same
  // thing as CArgNumber) or a numeric range formatted as "start : end" or
  // "start - end".  If only one argument is given, then "start" and "end"
  // will be set to the same value.
  //
  //   Note that this object actually contains two CCmdArgNumber objects for
  // the starting and ending addresses.  This is a hassle because it means
  // we're also responsible for initializing them, and you might be tempted
  // to the caller allocate objects for these and then simply pass references
  // to our constructor.  That'd save us the problem of passing parameters
  // (radix, min, max, etc) their constructors, and it'd allow the caller to
  // access their values directly.
  //
  //   The problem with that plan is that it won't work with the CCmdArgList
  // object (i.e. you couldn't have a list of address ranges).  That's because
  // the list uses Clone() to create a copy of this object, and that wouldn't
  // copy the caller's objects.  By embedding the CCmdArgNumber objects for
  // start and end in this object, they're automatically copied by Clone() and
  // everybody's happy...
  //--
  friend class CCmdArgRangeOrName;

  // Constructors ...
public:
  CCmdArgNumberRange(const char *pszName, uint32_t nRadix=10, uint32_t nMin=0, uint32_t nMax=UINT32_MAX, bool fOptional=false)
    : CCmdArgument(pszName, fOptional),
      m_Start("start address", nRadix, nMin, nMax, fOptional),
      m_End("end address", nRadix, nMin, nMax, fOptional)
      {};
  virtual ~CCmdArgNumberRange() {};
protected:
  // Disallow assignments ...
  CCmdArgNumberRange& operator= (const CCmdArgNumberRange &) = delete;
  // And use the Clone() method to create copies ...
  CCmdArgNumberRange(const CCmdArgNumberRange &ref)
    : CCmdArgument(ref), m_Start(ref.m_Start), m_End(ref.m_End)
      {/*std::cout << "CCmdArgNumberRange copy constructor!" << std::endl;*/}

  // Properties ...
public:
  void SetStart(uint32_t nValue) {m_Start.SetNumber(nValue);}
  void SetEnd(uint32_t nValue) {m_End.SetNumber(nValue);}
  uint32_t GetStart() const {return m_Start.GetNumber();}
  CCmdArgNumber &GetStartNumber() {return m_Start;}
  uint32_t GetEnd() const {return m_End.GetNumber();}
  CCmdArgNumber &GetEndNumber() {return m_End;}
  virtual CCmdArgument *Clone() const override {return DBGNEW CCmdArgNumberRange(*this);}

  // Parse functions ...
public:
  virtual bool Parse(const char *&pcNext) override;
  virtual bool Validate() const override;
  virtual void Reset()  override 
    {m_Start.Reset(); m_End.Reset(); CCmdArgument::Reset();}

  // Private member data ...
private:
  CCmdArgNumber  m_Start;     // the first (starting or lower) numeric value
  CCmdArgNumber  m_End;       //  "  second  (ending or higher)  "       "
};


class CCmdArgRangeOrName : public CCmdArgument {
  //++
  //   This argument type accepts EITHER an alphanumeric name, OR a numeric
  // address range.  It's a special case for the usual EXAMINE and DEPOSIT
  // commands to allow the argument to be either a register name or a range
  // of memory addresses...
  //
  //   Read the comments in the CCmdArgNumberRange class defition to see why
  // m_Range and m_Name are actual objects encapsulated in this one, rather
  // than references!
  //--

  // Constructors ...
public:
  CCmdArgRangeOrName(const char *pszName, uint32_t nRadix=10, uint32_t nMin=0, uint32_t nMax=UINT32_MAX, bool fOptional=false)
    : CCmdArgument(pszName, fOptional),
      m_Range("address range", nRadix, nMin, nMax, fOptional),
      m_Name("register name", fOptional)
      {m_fIsName=false;}
  virtual ~CCmdArgRangeOrName() {};
protected:
  // Disallow assignments ...
  CCmdArgRangeOrName& operator= (const CCmdArgRangeOrName&) = delete;
  // And use the Clone() method to create copies ...
  CCmdArgRangeOrName(const CCmdArgRangeOrName &ref)
    : CCmdArgument(ref), m_Range(ref.m_Range), m_Name(ref.m_Name), m_fIsName(ref.m_fIsName)
      {/*std::cout << "CCmdArgRangeOrName copy constructor!" << std::endl;*/}

  // Properties ...
public:
  bool IsName() const {return IsPresent() && m_fIsName;}
  bool IsRange() const {return IsPresent() && !m_fIsName;}
  CCmdArgName &GetNameArg() {return m_Name;}
  const CCmdArgName &GetNameArg() const {return m_Name;}
  CCmdArgNumberRange &GetRangeArg() {return m_Range;}
  const CCmdArgNumberRange &GetRangeArg() const {return m_Range;}
  virtual CCmdArgument *Clone() const override {return DBGNEW CCmdArgRangeOrName(*this);}

  // Parse functions ...
public:
  virtual bool Parse (const char *&pcNext) override;
  virtual bool Validate() const override;
  virtual void Reset() override
    {m_fIsName = false;  m_Name.Reset(); m_Range.Reset(); CCmdArgument::Reset();}

  // Private member data ...
private:
  CCmdArgNumberRange  m_Range;  // the first (starting or lower) numeric value
  CCmdArgName         m_Name;   //  "  second  (ending or higher)  "       "
  bool m_fIsName;               // TRUE if a name was specified
};


class CCmdArgList : public CCmdArgument {
  //++
  //   This class parses a list of one or more occurrences of any other argument
  // object.  The list may be separated by either commas or just spaces, and it
  // can optionally be enclosed in parentheses.  How's that for neat?
  //
  //   And yes, just in case you're wondering, it IS possible to to create an
  // CCmdArgList of CCmdArgList (i.e. a two level list).  In fact, they can be
  // nested to any level, PROVIDED that all levels except the outermost have the
  // parentheses flag turned on to avoud ambiguity.  This is probably not all
  // that useful, unless you want to re-invent LISP...
  //--

  // Constructors ...
public:
  CCmdArgList(const char *pszName, CCmdArgument &BaseArg, bool fParentheses=false, bool fOptional=false)
    : CCmdArgument(pszName, fOptional), m_BaseArg(BaseArg), m_fParentheses(fParentheses)
      {};
  virtual ~CCmdArgList() {};
protected:
  // Disallow assignments ...
  CCmdArgList& operator= (const CCmdArgList &) = delete;
  // And use the Clone() method to create copies ...
  CCmdArgList(const CCmdArgList &ref)
    : CCmdArgument(ref), m_BaseArg(ref.m_BaseArg),
      m_ArgumentList(ref.m_ArgumentList), m_fParentheses(ref.m_fParentheses)
      {/*std::cout << "CCmdArgList copy constructor!" << std::endl;*/}

  // The argument list is a simple std::vector of cloned CCmdArguments ...
  // Note that we only expose const_iterators - this list is generated by
  // Parse() and can't be modified otherwise.
public:
  typedef vector<CCmdArgument *> ARGUMENT_LIST;
  typedef ARGUMENT_LIST::const_iterator ARGUMENT_ITERATOR;
  ARGUMENT_ITERATOR begin() const {return m_ArgumentList.begin();}
  ARGUMENT_ITERATOR end()   const {return m_ArgumentList.end();}

  // Properties ...
public:
  size_t Count() const {return m_ArgumentList.size();}
  CCmdArgument *GetArgument (size_t n) const {return m_ArgumentList[n];}
  CCmdArgument *operator[] (size_t i) const {return GetArgument(i);}
  virtual CCmdArgument *Clone() const override {return DBGNEW CCmdArgList(*this);}

  // Parse functions ...
public:
  virtual bool Parse (const char *&pcNext) override;
  void ClearList();
  virtual void Reset() override {ClearList(); CCmdArgument::Reset();}
  virtual bool Validate() const override;

  // Private member data ...
private:
  CCmdArgument &m_BaseArg;      // type for the repeated argument
  ARGUMENT_LIST m_ArgumentList; // list of cloned CCmdArguments
  bool          m_fParentheses; // true if parentheses are required
};


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

//++
//   A modifier is a keyword preceeded by a "/" character.  Modifiers may be
// stand alone (e.g. "/ONLINE") or they may take a value argument (e.g.
// "/BITS=18").  In the latter case, a CCmdArgument object is associated with
// each modifier to handle parsing the argument.  Note that each modifier
// actually allows for two names - a "yes" or true case (e.g. "/WRITE") and
// a "no" of false case (e.g. "/NOWRITE").  If the modifier has only one sense
// then the latter name may be omitted.
//--
class CCmdModifier {
  // These magic characters introduce modifiers and their values ...
public:
  static const char m_cModifier = '/';
  static const char m_cValue = '=';

  // Constructor and destructor ...
public:
  CCmdModifier(const char *pszName, const char *pszNoName=NULL, CCmdArgument * const pArg=NULL, bool fOptional=true)
    : m_pszName(pszName), m_pszNoName(pszNoName), m_fOptional(fOptional),
      m_fPresent(false), m_fNegated(false), m_pArg(pArg)
      {Reset();}
  virtual ~CCmdModifier() {};
private:
  // Disallow copies and assignments ...
  CCmdModifier& operator= (const CCmdModifier &) = delete;
  CCmdModifier(const CCmdModifier &ref) = delete;

  // Properties ...
public:
  const char *GetName() const {return m_pszName;}
  CCmdArgument *GetArg() const {return m_pArg;}
  bool IsOptional() const {return m_fOptional;}
  bool IsPresent() const {return m_fPresent;}
  bool IsNegated() const {return m_fNegated;}
  virtual void Reset()
    {m_fPresent = m_fNegated = false;  if (m_pArg != NULL) m_pArg->Reset();}

  // Parse functions ...
public:
  static CCmdModifier *Search(const char *pszMod, CCmdModifier * const aMods[]);
  static CCmdModifier *Search(string sMod, CCmdModifier * const aMods[])
    {return Search(sMod.c_str(), aMods);}
  bool ParseArgument(const char *&pcNext);
  void ShowHelp() const;

  // Private member data ...
private:
  const char    *m_pszName;   // name of the modifier, without the "/"
  const char    *m_pszNoName; // negated name (e.g. "NOWRITE") if used
  const bool     m_fOptional; // true if this modifer is optional
  bool           m_fPresent;  // true if this modifier is present
  bool           m_fNegated;  // true if this modifer is negated
  CCmdArgument * const m_pArg;// argument required by this modifier
};

//   This inserter allows you to send a modifier (it prints the modifier's
// name) directly to an I/O stream for error messages...
inline ostream& operator << (ostream &os, const CCmdModifier &mod)
  {os << CCmdModifier::m_cModifier << mod.GetName();  return os;}


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

//++
//   The CCmdVerb object ties together all the elements of a single command.
// Each command starts with a keyword (the verb, e.g. "disconnect", "attach",
// "exit", etc), is followed by zero or more arguments, and those may be
// followed by zero or more modifiers.  The arguments and modifiers accepted
// by this verb are specified by two NULL terminated arrays of pointers to the
// corresponding objects.  Note that for arguments the order of the array is
// important since arguments are positional, however for modifiers the order
// is unimportant.
//--
class CCmdVerb {
  //   This is the "type" of a verb action routine - that's what gets called
  // to actually execute the command after the command line is parsed.  Note
  // that the return type is bool - if the command "fails" the action should
  // return false.  That aborts any indirect command file in progress.  Also
  // note that the CCmdParser object is passed as a parameter - this allows
  // command routines to get input from or send output to the UI stream.
public:
  typedef bool (VerbAction_t) (CCmdParser &cmd);

  // Constructors and destructor ...
public:
  CCmdVerb (const char *pszVerb, VerbAction_t *pAction,
            CCmdArgument * const paArgs[]=NULL, CCmdModifier * const paMods[]=NULL,
            CCmdVerb * const paSubVerbs[]=NULL)
    : m_pszVerb(pszVerb), m_pAction(pAction), m_paArguments(paArgs),
      m_paModifiers(paMods), m_paSubVerbs(paSubVerbs)
      {}
  virtual ~CCmdVerb() {};
private:
  // Disallow copies and assignments ...
  CCmdVerb& operator= (const CCmdVerb&) = delete;
  CCmdVerb(const CCmdVerb &ref) = delete;

  // Properties ...
public:
  const char *GetName() const {return m_pszVerb;}

  // Parse functions ...
public:
  // Lookup command names in the parse tables ...
  static CCmdVerb *Search (const char *pszVerb, CCmdVerb * const aVerbs[], bool fError=true);
  static CCmdVerb *Search (string strVerb, CCmdVerb * const aVerbs[], bool fError=true)
    {return Search(strVerb.c_str(), aVerbs, fError);}
  // Handle lines that start with "@" (indirect commands) ...
  static bool ParseIndirect (CCmdParser &cmd, const char *&pcNext, CCmdVerb * const aVerbs[]);
  // Parse a command verb, look it up, and then parse the rest of the line ...
  static bool ParseVerb (CCmdParser &cmd, const char *&pcNext, CCmdVerb * const aVerbs[]);
  // Print out the command parse tables ...
  void ShowHelp() const;

  // Private methods ...
private:
  static void ResetArguments (CCmdArgument * const paArgs[]);
  static void ResetModifiers (CCmdModifier * const paMods[]);
  static bool ParseArgument (const char *&pcNext, int &nArgs, CCmdArgument * const paArgs[]);
  static bool ParseModifier (const char *&pcNext, CCmdModifier * const paMods[]);
  static bool ParseTail (const char *&pcNext, CCmdArgument * const paArgs[], CCmdModifier * const paMods[]);
  static bool ValidateArguments (CCmdArgument * const paArgs[]);
  static bool ValidateModifiers (CCmdModifier * const paMods[]);
  static void ShowModifiers (CCmdModifier * const paMods[]);
  static void ShowArguments (const char *pszVerb, CCmdArgument * const paArgs[], const char *pszPrefix=NULL);
  void ShowVerb (const char *pszPrefix=NULL) const;

  // Private member data ...
public:
  const char           *m_pszVerb;    // the actual name of this verb
  VerbAction_t * const  m_pAction;    // address of a routine to execute it
  CCmdArgument * const *m_paArguments;// argument list for this verb
  CCmdModifier * const *m_paModifiers;// modifier list for this verb
  CCmdVerb     * const *m_paSubVerbs; // alternate syntax "subverbs"
};


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

//++
//   A command "alias" is simply a shortened alias name for a longer command
// string.  For example,
//
//      xxx> DEFINE R0 "REWIND TAPE/UNIT=0"
//
// makes the string "R0" an alias for the command "REWIND TAPE/UNIT=0".  It's
// not complicated, and at the moment that's all there is to it.  Someday we
// might implement fancy features like parameter substitutions, conditional
// and looping constructs, etc, but not today.
//
//   This class is essentially just a collection that implements an unordered
// map (aka a hash table) mapping of one string into another string.  Although
// this is essentially a "is a" relationship (a CCmdAliases IS a hash table)
// we choose to hide the underlying implementation.  This allows us to add a
// few extra rules (e.g. all alias names are in upper case) and also allows
// for easy expansion in the future (e.g. to include parameters or multiline
// expansions).
//--
class CCmdAliases {

  // Constructor and destructor ...
public:
  CCmdAliases() {};
  virtual ~CCmdAliases() {};
private:
  // Disallow copies and assignments ...
  CCmdAliases& operator= (const CCmdAliases &) = delete;
  CCmdAliases(const CCmdAliases &ref) = delete;

  // Define the underlying hash table and iterators for this collection ...
private:
  typedef unordered_map<string, string> ALIAS_TABLE;
  typedef ALIAS_TABLE::iterator iterator;
  iterator begin() {return m_Aliases.begin();}
  iterator end() {return m_Aliases.end();}
  //   Notice that the const_iterators are exposed.  This is mainly so that
  // the "SHOW ALIASES" command can use them to print a list of aliases...
public:
  typedef ALIAS_TABLE::const_iterator const_iterator;
  const_iterator begin() const {return m_Aliases.begin();}
  const_iterator end() const {return m_Aliases.end();}

  // Public CCmdAliases collection properties ...
public:
  // Return true if the specified alias is defined ...
  bool IsDefined (string str) const
    {ToUpper(str);  return m_Aliases.find(str) != m_Aliases.end();}
  // Return the number of aliases defined ...
  size_t Count() const {return m_Aliases.size();}

  // Public CCmdAliases collection methods ...
public:
  // Fold an alias name to upper case ...
  static void ToUpper(string &str);
  // Define a new alias ...
  bool Define (string sAlias, string sSubstitution, bool fRedefine=false);
  // Undefine an existing alias ...
  bool Undefine(string sAlias);
  // Return the definition of a single alias ...
  string GetDefinition (string s) const
    {ToUpper(s);  return IsDefined(s) ? m_Aliases.at(s) : string("");}
  // Expand an alias string in a command line ...
  bool Expand(char *pszCommand, size_t cbCommand) const;

  // Private member data ...
private:
  ALIAS_TABLE     m_Aliases;    // collection of all aliases
};


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

//++
//   The CCmdParser class reads a command line from the console, parses it,
// executes it, and then repeats.  It basically provides an endless loop that
// can be called by the main program as the background task to read and execute
// commands forever.  A single level of indirect command file processing is
// also implemented.
//
//   FWIW, this is probably not the best way to handle things - it'd be better
// to define a generic CCmdSource class for obtaining command lines, and then
// derive CCmdConsole and CCmdScript classes from it to read from the console
// and indirect files, but for the moment we're going to leave that as an
// exercise for later.
//--
class CCmdParser {

  // Constants ...
public:
  enum {
    MAXCMD    = 256,            // longest command line ever allowed
    MAXDEPTH  =  10,            // maximum script file nesting depth
  };          

  // Special types ...
public:
  //   This type of routine is called when the operator tries to exit from the
  // command parser.  Since this will presumably exit the application, this
  // routine can be used to ask "Are you sure?" if there are unsaved files,
  // attached devices, etc...
  typedef bool (ConfirmExit_t) (CCmdParser &cmd);

  // Constructor and destructor ...
public:
  CCmdParser (const char *pszPrompt, CCmdVerb * const aVerbs[], ConfirmExit_t *pConfirm=NULL, CConsoleWindow *pConsole=NULL);
  virtual ~CCmdParser() {};
private:
  // Disallow copies and assignments ...
  CCmdParser& operator= (const CCmdParser &) = delete;
  CCmdParser(const CCmdParser &ref) = delete;

  // Properties ...
public:
  // Return the prompting string we're using ...
  string GetPrompt() const {return m_sPrompt+">";}
  // Return the current script file nesting level ...
  uint32_t GetScriptLevel() const {return m_nScriptLevel;}
  bool InScript() const {return m_nScriptLevel > 0;}
  // Return the current script name (if any) ...
  string GetScriptName() const 
    {return InScript() ? m_asScriptName[m_nScriptLevel-1] : string("");}
  // Return the current script file line number ...
  uint32_t GetScriptLine() const
    {return InScript() ? m_anScriptLine[m_nScriptLevel-1] : 0;}
  // Return the current script file handle ...
  FILE *GetScriptFile() const
    {return InScript() ? m_apScriptFile[m_nScriptLevel-1] : NULL;}
  // Return true if the UI wants this application to exit ...
  bool IsExitRequested() const {return m_fExitLoop;}
  void SetExitRequest(bool fExit=true) {m_fExitLoop = fExit;}
  // Return the handle of the console window (if any) ...
  CConsoleWindow *GetConsole() const
    {assert(m_pConsole != NULL);  return m_pConsole;}
  // Return true if a console window is attached ...
  bool IsConsoleAttached() const {return m_pConsole != NULL;}
  // Get the current list of command aliases (for reading only!) ...
  const CCmdAliases *GetAliases() const {return &m_Aliases;}
  // Set or get the last subverb scanned...
//void SetSubVerb (const string sVerb) {m_sSubVerb = sVerb;}
//string GetSubVerb() const {return m_sSubVerb;}

  // Methods ...
public:
  // Set the default extension on any file name ...
  static string SetDefaultExtension(string sFileName, const char *pszDefExt);
  // Open, close and read from script files ...
  bool OpenScript(string sFileName);
  void CloseScript();
  bool ReadScript(char *pszBuffer, size_t cbBuffer);
  void ScriptError(bool fAbort=true);
  // Read from the console ...
  bool ReadConsole(string sPrompt, char *pszBuffer, size_t cbBuffer);
  // Read a command, either from a script or the console ...
  bool ReadCommand();
  // Ask the user "Are you sure?" ...
  bool AreYouSure(string sPrompt = string(), bool fDefault=false);
  // Parse one command line ...
  bool ParseCommand(const char *&pcNext);
  // Read and parse commands forever (or at least until exit or EOF) ...
  void CommandLoop();
  // Call the ConfirmExit routine, if one exists ...
  bool ConfirmExit()
    {return (m_pConfirmExit != NULL) ? (*m_pConfirmExit)(*this) : true;}
  // Define or undefine an alias name ...
  bool DefineAlias (string sAlias, string sSubstitution);
  bool UndefineAlias (string sAlias);

  // Generic HELP command ...
public:
  static CCmdVerb g_cmdHelp;
public:
  static CCmdArgName g_argHelp;
  static CCmdArgument * const g_argsHelp[];
  static bool DoHelp (CCmdParser &cmd);
  static void ShowVerbs(CCmdParser &cmd);

  // Private member data ...
private:
  CConsoleWindow   *m_pConsole;     // pointer to console window object
  string            m_sPrompt;      // prompting string to use
  CCmdVerb * const *m_paVerbs;      // table of verbs to be processed
  bool              m_fExitLoop;    // TRUE to exit the command loop
  ConfirmExit_t    *m_pConfirmExit; // TRUE to confirm exit
  CCmdAliases       m_Aliases;      // hash table of alias names
  uint32_t          m_nScriptLevel; // current script file nesting level
//string            m_sSubVerb;     // last subverb scanned
  string   m_asScriptName[MAXDEPTH];// name of the current indrect file
  FILE    *m_apScriptFile[MAXDEPTH];// handle of the indirect file
  uint32_t m_anScriptLine[MAXDEPTH];// current line number being interpreted
  char     m_szCmdBuf[MAXCMD];      // the text of the actual command line
};

