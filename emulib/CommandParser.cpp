//++
// CommandParser.cpp -> CCmdArgument (command line argument) methods
//                      CCmdModifier (command line modifier) methods
//                      CCmdVerb     (command line verb) methods
//                      CCmdParser   (command line parser) methods
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
//   These methods implement a simple command line parser for the emulator 
// library.  The caller is expected to define tables of command verbs,
// arguments and modifier objects and we'll parse a line of text according to
// those definitions.  Part of each verb definition is the address of a routine
// to actually execute the command, and we'll call that as the last step after
// the line is successfully parsed.
//
//   The results of the parse are stored implicitly in the CCmdArgument and
// CCmdModifer objects defined by the caller, and that's how the results of
// the parse are communicated to the verb action routine.  The only parameter
// passed to the verb action routine is a reference to the CCmdParser object.
// This allows command routines to get additional input from or send output
// to the UI stream.
//
//   And lastly, notice that the command parser is able to generate help text
// for the user solely by deciphering the parser tables.  This help text lists
// the names of all verbs, arguments, modifiers and values.
//
// Bob Armstrong <bob@jfcl.com>   [20-MAY-2015]
//
// REVISION HISTORY:
// 20-May-15  RLA   New file.
// 11-JUN-15  RLA   Add CConsoleWindow support.
// 17-JUN-15  RLA   Add command alias support.
//                  Accept "@file" to call a script file.
//                  Accept "!" and "#" in addition to ";" for comments.
// 19-JUN-15  RLA   Make paMods and paArgs explicit parameters in most cases.
//                  This is in preparation for the alternate syntax option.
// 12-OCT-15  RLA   Make HELP command definition global.
// 13-SEP-16  RLA   Add CCmdArgNetworkAddress.
// 28-FEB-17  RLA   Make 64 bit clean.
//  1-JUN-17  RLA   Linux port.
// 12-JAN-20  RLA   Add CCmdArgList repeated argument type.
//                  Add ParseError() et al to get better error messages.
// 14-JAN-20  RLA   Add CCmdArgNumberRange and CCmdArgNameOrNumber
// 17-JUL-22  RLA   Allow alias expansions to have arguments - for example,
//                    DEFINE SBR "SET BREAKPOINT/ROM"
//                    SBR 1234
// 26-AUG-22  RLA   Clean up Linux/WIN32 conditionals.
// 15-FEB-24  RLA   Allow comments at the end of commands.
//--
//000000001111111111222222222233333333334444444444555555555566666666667777777777
//234567890123456789012345678901234567890123456789012345678901234567890123456789
#include <stdlib.h>             // exit(), system(), etc ...
#include <stdint.h>	        // uint8_t, uint32_t, etc ...
#include <assert.h>             // assert() (what else??)
#include <string.h>             // strcpy(), strerror(), etc ...
#include <cstring>              // needed for memset()
#include "EMULIB.hpp"           // emulator library definitions
#include "SafeCRT.h"		// replacements for Microsoft "safe" CRT functions
#include "ConsoleWindow.hpp"    // console window methods
#include "LogFile.hpp"          // message logging facility
#include "CommandParser.hpp"    // declarations for this module

//++
// MORE
//   A more detailed tutorial on how to use the command parser -
//
// OVERVIEW
//   The basic command line syntax that we expect to parse looks something
// like this -
//
// <verb> [<argument>] [<argument>] ... [/<modifier>[=<value>]] [/<modifier>] ...
//
// Arguments, modifiers, and modifier values may be required or optional, as
// determined by the verb.  Arguments are positional (i.e. the first argument
// is the first atom after the verb, etc) and any optional arguments must
// appear at the end.  Modifiers are not positional and may occur in any order.
// In practice modifiers usually appear at the end of the command line, however
// the parser will accept them anywhere.  Modifiers may have a value, either
// optional or required.  A single modifier may also define two complementary
// forms (e.g. "/WRITE" and "/NOWRITE").
//
//   The CommandParser.hpp header defines a number of classes representing
// various command line entities - verbs, arguments, modifiers, etc - and the
// application creates trees and arrays of these objects to define the verbs
// recognized and the syntax of each one. Usually these would be static objects
// and the entire command language would be defined at compile time.
//
//   To execute commands, an application simply creates a CCmdParser object,
// specifying a handle for the console window, the prompting string to use
// (e.g. "ELF>"), and a pointer to an array of CCmdVerb objects.  After that,
// the application calls the CCmdParser::CommandLoop() method.  This method
// will loop, reading, parsing and executing commands from the console, until
// end of file is found (e.g. the console window is closed, or the user enters
// ^Z). It's also possible for one of the application's command action routines
// to force the CommandLoop() routine to exit; this is done by the EXIT and/or
// QUIT ocmmands, for example.
//
// ARGUMENTS
//   The CCmdArgument class defines the basic "atoms" used in parsing commands.
// It is used to define verb arguments, and (in spite of the name) is also used
// to define the value for any modifier that requires one.  There are a number
// of subclasses of CCmdArgument, each definiting a different atom type-
//
//  CCmdArgument        - any characters not including white space, "/" or EOL
//  CCmdArgString       - any string, possibly quoted (to allow spaces or "/")
//  CCmdArgNumber       - a decimal, binary, octal or hexadecimal integer
//  CCmdArgName         - any string of alphanumeric characters
//  CCmdArgKeyword      - a name from a predefined list (e.g. RP04, RP06, ...)
//  CCmdArgFileName     - the name of a file
//  CCmdArgPCIAddress   - a PCI address in BDF ("xx:xx.x") notation
//  CCmdArgDiskAddress  - a disk address in C/H/S or LBA notation
//
//   Each argument class contains methods to parse the argument from the
// command and and store the value in the object.  Some types also contain
// methods to to semantically validated it where appropriate (e.g. range
// checking for CCmdArgNumber values).  Each CCmdArgument object also defines
// a method to reset the argument to its initial (defualt) value.  And lastly,
// each CCmdArgument contains a flag to say whether this argument is optional
// or required.
//
//   IMPORTANT - the CCmdArguments objects are usually created statically at
// compile time by the application. HOWEVER, since since the results of parsing
// that argument are stored in the CCmdArgument object, THEY CAN'T BE CONST!
// Sorry - it would have been better to separate the argument definition from
// the argument value, but in the interest of simplicity I did not.
//
// MODIFIERS
//   The argument list defined in the CCmdVerb object is nothing more than an
// array of pointers to CCmdArgument objects.  Modifiers, however, require an
// additional object for their definition.  The CCmdModifier object defines two
// (yes, two!) names for the modifier, an argument (in the form of a CCmdArgument
// object) for the modifier if it requires one, and a flag to say whether this
// modifier is optional or required.
//
//   Modifiers with two names are said to be "negatable" and are used for
// YES/NO or TRUE/FALSE type options (e.g. "/LOG" and "/NOLOG").  The two names
// are distinct and don't need to be similar, for example "/ONLINE" and
// "/OFFLINE".  Modifiers with only one name are "non-negatable" and can only
// appear in the "true" form.  Modifiers which do not have the optional flag
// set are required and must be specified on the command line.  If they are
// not, the parser prints an error and the action routine is never invoked.
// Modifiers with the optional flag set are, of course, optional.  NOTE that
// the optional flag in the modifier refers to the modifier itself, NOT any
// associated value!
//
//   Modifiers with a non-NULL pointer to a CCmdArgument will accept a value,
// the type and syntax of which is determined by the CCmdArgument object (e.g.
// a number, a keyword, a file name, etc).  If the CCmdArgument object has the
// optional flag set, then the modifier value is optional, but otherwise it
// will be required.  Note that it's completely possible for a negatable
// modifier to have a value, optional or not.  For example, it's possible to
// define a single modifier for "/LOG", "/LOG=filename", and "/NOLOG" BUT
// unfortunately the parser handles both the true and false options identically
// when parsing any value.  That means in this example that "/NOLOG=filename"
// would also be accepted.  The application's action routine would need to deal
// with that case.
//
//   Lastly, remember that parsing updates the CCmdModifier object with two
// flags - whether the modifier was present, and whether it was negated.  This
// means that, like CCmdArgument, any CCmdModifier objects CANNOT BE const.
// The modifier value, if defined, will appear in the associated CCmdArgument
// object and not the modifier itself.
//
// VERBS
//   An array of pointers to CCmdVerb objects must be passed to the CCmdParser
// constructor.  Each CCmdVerb object contains the name of the verb, an array
// of pointers to CCmdArgument objects, an array of pointers to CCmdModifier
// objects, and the address of a callback routine (called an "action routine")
// that is invoked to execute this command.  The command parser completely
// reads and parses the entire command line, including checking for mandatory
// arguments and validating/range checking any values, before invoking the
// action routine.  Thus by the time the action routine is invoked all the
// required data from the command line is available and valid.
//
//   Unlike the CCmdArgument and CCmdModifier objects, the CCmdVerb object is
// not modified by the parser.  These objects can be declared as const if
// desired.
//
// SUBVERBS
//   There are two subclasses of CCmdVerb that allow us to handle special
// cases that we'd also like to be able to parse.  The first special case
// occurs when the application has a set of related commands, e.g.
//
//        SET LOG [/FILE=...] [/LEVEL=...] ...
//        SET WINDOW [/FONT=...] [/FOREGROUND=...] [/BACKGROUND=...]
//        SET UNIT <unit> [/ONLINE] [/OFFLINE]...
//
// We have a group of commands that all start with the same verb ("SET") and
// then a keyword ("LOG", "WINDOW", "UNIT", etc).  That's easy enough to handle,
// but the problem is that the options and arguments expected for the remainder
// of the command depend on the keyboard.  The "SET LOG ..." command doesn't
// have the same argument and modifier list that "SET WINDOW ..." does, for
// example.
//
//   This situation is handled by the CCmdSubVerb class.  A CCmdSubVerb object
// is similar to CCmdVerb, except that it has no associated argument list,
// modifier list or action routine.  Instead a CCmdSubVerb object contains a
// pointer to another table of CCmdVerb objects.  The CCmdSubVerb class simply
// looks up the next atom on the command line in the table of associated "sub"
// verbs, and then parses the remainder of the command according to the one
// found.
//
//   Notice that the nomenclature ends up being a little backwards.  In this
// example, "SET" would be a CCmdSubVerb object and it would point to a table
// of CCmdVerbs defining the "LOG", "WINDOW", and "UNIT" "sub" verbs.  A pointer
// to the "SET" CCmdSubVerb would be stored in the application's main table
// of verbs (as given to the CCmdParser object) and pointers to the "LOG",
// "WINDOW" and "UNIT" CCmdVerbs would stored in a separate table and referenced
// only by the CCmdSubVerb object.
//
// ALTERNATE SYNTAX
//   A related but slightly different problem comes up when we want to have
// commands like this -
//
//        SET CONSOLE [/FONT=...] [/COLOR=...] ...
//        SET TAPE1 ...
//        SET TAPE2 ...
//
// In this case, "CONSOLE", "TAPE1" and "TAPE2" are user defined aliases for
// controllers and equipment that has been defined by the user at runtime.
// These names are not constant and are not known at compile time.  In fact,
// in this case not even the number of potential names is known at compile
// time. This is beyond anything that can be handled with a subverb structure.
//
//   For this situation we have the CCmdSplitVerb object.  At least that's
// the idea - right now none of that has been implemented yet!  Sorry.
// TBA NYI FIX ME!!
//--


// Parsing primitives ...
inline bool IsEOS (char c)  {return c == '\000';}
inline bool IsModifier (char c)  {return c == CCmdModifier::m_cModifier;}
inline bool IsComment (char c)  {return (c == ';') || (c == '!') || (c == '#');}
inline bool IsIndirect (char c)  {return c == '@';}
inline bool IsQuote (char c)  {return c == '"';}
inline char SpanWhite (const char *&pc)  {while (isspace(*pc)) ++pc;  return *pc;}
inline bool strieql (const char *psz1, const char *psz2)  {return _stricmp(psz1, psz2) == 0;}
inline bool isodigit (char c) {return ((c >= '0') && (c <= '7'));}


////////////////////////////////////////////////////////////////////////////////
//////////////   C O M M A N D   A R G U M E N T   M E T H O D S   /////////////
////////////////////////////////////////////////////////////////////////////////

//CCmdArgument - base class for all arguments
string CCmdArgument::ScanToken(const char *&pcNext)
{
  //++
  //   This method parses a generic argument type - that's a string of any
  // characters up to the next white space or modifier character (i.e. "/").
  // Any leading white space characters before the argument will be skipped.
  // If the argument ends up being null (i.e. the next non-blank character is
  // eos or a modifier) then a null string is returned.
  //--
  SpanWhite(pcNext);
  for (const char *pcStart = pcNext;  ;  ++pcNext) {
    if (isspace(*pcNext) || IsEOS(*pcNext) || IsModifier(*pcNext)) {
      size_t len = pcNext - pcStart;
      return string(pcStart, len);
    }
  }
}

string CCmdArgument::ScanQuoted (const char *&pcNext)
{
  //++
  //   This method is identical to ScanToken(), except that this time we also
  // allow for a quoted string.  A quoted string may contain white space or
  // modifier ("/") characters, but not EOS!
  //--
  SpanWhite(pcNext);
  if (!IsQuote(*pcNext)) return ScanToken(pcNext);
  for (const char *pcStart = ++pcNext;  ;  ++pcNext) {
    if (IsQuote(*pcNext) || IsEOS(*pcNext)) {
      size_t len = pcNext - pcStart;
      if (IsQuote(*pcNext)) ++pcNext;
      return string(pcStart, len);
    }
  }
}

void CCmdArgument::SetValue (const char *pcStart, const char *pcEnd)
{
  //++
  // Set the argument value to the substring between *pcStart and *pcEnd.
  //--
  assert((pcStart != NULL) && (pcEnd != NULL));
  size_t len = pcEnd - pcStart;
  if (len == 0) {
    ClearValue();
  } else {
    string str(pcStart, len);  SetValue(str);
  }
}

void CCmdArgument::SetValue (const char *pcStart, size_t nLen)
{
  //++
  // Set the argument value to the substring specified by pcStart and nLen ...
  //--
  if (nLen == 0) {
    ClearValue();
  } else {
    assert((pcStart != NULL) && (nLen <= strlen(pcStart)));
    string str(pcStart, nLen);  SetValue(str);
  }
}

bool CCmdArgument::ParseError (const char *pszError)
{
  //++
  //   This method may be called by the Parse() method, or any similar method
  // in a derived class, to set a more specific error message in the event of
  // any failure.  It simply stores the pointer to the string and always
  // returns false.
  //
  //   It's assumed that the string is some compile time constant - e.g.
  //
  //          if (bad stuff...) return ParseError("bad stuff found");
  //
  // If you want to make a run time string then you'll have to allocate a
  // static buffer for it.  Since the error message is usually printed 
  // immediately after the Parse() method returns false, you probably don't
  // have to worry too much about another error occuring before the current
  // one is printed.
  //--
  m_pszError = pszError;  return false;
}

bool CCmdArgument::Parse (const char *&pcNext)
{
  //++
  //   A generic argument is any sequence of characters which doesn't contain
  // a white space or modifier ("/") character ...
  //--
  string sToken = ScanToken(pcNext);
  if (!sToken.empty()) {
    SetValue(sToken);  return true;
  } else
    return ParseError("token expected");
}


//CCmdArgName - name or identifier (e.g. DISK)
string CCmdArgName::ScanName(const char *&pcNext)
{
  //++
  //   This method parses a "name" argument type - that's any string of
  // alphanumeric characters including "$" or "_".  Note that the result
  // of GetName() is always folded to upper case!
  //--
  string sResult;
  SpanWhite(pcNext);
  for (/*const char *pcStart = pcNext*/;  ;  ++pcNext) {
    if (isalnum(*pcNext) || (*pcNext=='$') || (*pcNext=='_'))
      sResult += toupper(*pcNext);
    else
      return sResult;
  }
}

bool CCmdArgName::Parse (const char *&pcNext)
{
  //++
  // Parse any name (e.g. a verb, modifier, keyword, etc) ...
  //--
  string sName = ScanName(pcNext);
  if (!sName.empty()) {
    SetValue(sName);  return true;
  } else
    return ParseError("name expected");
}


//CCmdArgNumber - number in any radix (e.g. 1234)
string CCmdArgNumber::ScanNumber (const char *&pcNext, uint32_t nRadix)
{
  //++
  //   A numeric argument is what you'd expect - any string of digits.  The
  // exact syntax is determined by the radix specified and whatever strtoul()
  // likes.  If we can't find at least one digit next, then a null string will
  // be returned - this is almost always bad...
  //
  //   Notice that this routine actually returns a string containing the
  // numeric value.  That's because we actually store the result, as a string,
  // in the CCmdArgument() base class.  That's a little ugly here, but allows
  // us to share most of the base class functions (e.g. IsPresent(), etc)
  // without modification.
  //
  //   Because of that, this routine uses strtoul() to parse the argument, but
  // ironically enough we don't actually care what the result is!  Also remember
  // that strtoul() will happily take a leading '-' sign, however since the
  // return value is unsigned this leads to a very large result!  We don't
  // want that and explicity check for that case...
  //--
  SpanWhite(pcNext);
  const char *pcStart = pcNext;  char *pcEnd;
  if (*pcStart == '-') return "";
  strtoul (pcStart, &pcEnd, nRadix);
  size_t len = pcEnd - pcStart;
  if (len > 0) pcNext = pcEnd;
  return string(pcStart, len);
}

bool CCmdArgNumber::Parse (const char *&pcNext)
{
  //++
  // Parse a number ...
  //--
  string sNumber = ScanNumber(pcNext, m_nRadix);
  if (!sNumber.empty()) {
    SetValue(sNumber);  return true;
  } else
    return ParseError("number expected");
}

bool CCmdArgNumber::Validate() const
{
  //++
  // A numeric value must be within the range defined in the constructor ...
  //--
  if (IsPresent()) {
    uint32_t nVal = GetNumber();
    if ((nVal < m_nMin) || (nVal > m_nMax)) {
      // Special hack for our hexadecimal friends...
      if (m_nRadix == 16) 
        CMDERRF("%s must be between 0x%X and 0x%X", GetName(), m_nMin, m_nMax);
      else
        CMDERRF("%s must be between %d and %d", GetName(), m_nMin, m_nMax);
      return false;
    }
  }
  return true;
}


//CCmdArgKeyword - name from a list of predefined keywords
bool CCmdArgKeyword::Match(const char *pszToken, const char *pszKey)
{
  //++
  //   This routine will attempt to match a token (presumably scanned from the
  // command line) against a specific keyword. The matching is case insensitive
  // anda also allows for abbreviations.  The keyword string may contain a "*"
  // character at any point, and any characters after that are optional.  If
  // additional characters are present in the token, however, they must match
  // the keyword.  For example -
  //
  //    pszToken  pszKey     match?
  //    -------   ---------  -------
  //    E         ex*amine   NO
  //    EX        ex*amine   YES
  //    EXAM      ex*amine   YES
  //    EXIT      ex*amine   NO
  //
  //--
  bool match = false;
  if ((pszToken == NULL) || (pszKey == NULL)) return false;
  while (!IsEOS(*pszKey)) {
    if (*pszKey == '*') {
      // Just skip over the "*" in the keyword and keep going ...
      match = true;  ++pszKey;
    } else if (tolower(*pszToken) == tolower(*pszKey)) {
      // This pair of characters match - keep going!
      ++pszKey;  ++pszToken;
    } else {
      //   The token doesn't match the keyword.  If we've reached the end of
      // the token AND we've already found enough characters to force a match,
      // then success!
      return match && IsEOS(*pszToken);
    }
  }
  //   We've reached the end of the keyword - if we've also reached the end
  // of the token at the same time, then success.  Otherwise fail...
  return IsEOS(*pszToken);
}

int CCmdArgKeyword::Search (const char *pszToken, const KEYWORD *paKeys)
{
  //++
  //   This method searches a list of keywords to find the matching one.  The
  // keyword list is not assumed to be in any particular order, however it's
  // up to you to ensure that abbreviations don't create any ambiguities.  If
  // the token would match more than one keyword, then the index of the first
  // match is always returned.
  //--
  for (int i = 0; paKeys[i].m_pszName != NULL; ++i)
    if (Match(pszToken, paKeys[i].m_pszName)) return i;
  return -1;
}

bool CCmdArgKeyword::Parse(const char *&pcNext)
{
  //++
  //   The Parse() for keywords is a bit unusual.  The parse simply tries
  // to recognize a keyword - any keyword - and Parse() succeeds if that
  // does.  After that, we try to lookup the keyword and set the m_nIndex
  // member to the index of this token in the keyword table.  If the token
  // isn't in the table, then the IsValid() property becomes false, but the
  // Parse() still succeeds.  It's the difference between syntax and
  // semantics :-)
  //--
  SetValue(CCmdArgName::ScanName(pcNext));
  if (!IsPresent()) return ParseError("keyword expected");
  m_nIndex = Search(GetValue(), m_paKeys);
  return true;
}

bool CCmdArgKeyword::Validate() const
{
  //++
  // A keyword must be one of the defined choices ...
  //--
  if (IsPresent() && !IsValid()) {
    CMDERRF("%s is not a valid choice for %s", GetValue().c_str(), GetName());
    return false;
  }
  return true;
}


//CCmdArgString - possibly quoted string (e.g. "this is a string")
bool CCmdArgString::Parse(const char *&pcNext)
{
  //++
  // Parse a quoted string ...
  //--
  string sString = ScanQuoted(pcNext);
  if (!sString.empty()) {
    SetValue(sString);  return true;
  } else
   return ParseError("string expected");
}


//CCmdArgPCIAddress - PCI address in BDF (e.g. "bus:domain.function")
bool CCmdArgPCIAddress::ScanBDF (const char *&pcNext, uint32_t &nBus, uint32_t &nSlot, uint32_t &nFunction)
{
  //++
  //   This routine will parse a PCI bus address in "bus:domain.function" (aka
  // BDF) notation and return the individual components.  If anything about the
  // BDF string is illegal, it returns FALSE (and unpredictable results for the
  // bus, slot and function).  The parsing isn't super smart, but it's good
  // enough for what we need.
  //
  //   Notice that in the case of a malformed argument we still scan as much of
  // it as we can AND the pcNext pointer will be advanced over the part we scan.
  // This substring that we actually recognized will get stored as the argument
  // value, BUT this routine will return FALSE and the m_fValid flag will be
  // cleared for this argument.
  //--
  SpanWhite(pcNext);  nBus = nSlot = nFunction = 0;
  if (!isxdigit(*pcNext)) return ParseError("hex number expected");
  nBus = strtoul(pcNext, (char **) &pcNext, 16);
  if (*pcNext != ':') return ParseError("':' expected");
  if (!isxdigit(*++pcNext)) return ParseError("hex number expected");
  nSlot = strtoul(pcNext, (char **) &pcNext, 16);
  if (*pcNext == '.') {
    if (!isxdigit(*++pcNext)) return ParseError("hex number expected");
    nFunction = strtoul(pcNext, (char **) &pcNext, 16);
  }
  return true;
}

bool CCmdArgPCIAddress::Parse (const char *&pcNext)
{
  //++
  //   This routine parses the command line BDF argument and extracts both
  // the individual fields AND the entire argument as a string.  The latter
  // is stored in the CCmdArgument base class as the "value".
  //--
  SpanWhite(pcNext);  const char *pcStart = pcNext;
  m_fValid = ScanBDF(pcNext, m_nBus, m_nSlot, m_nFunction);
  SetValue(pcStart, pcNext);
  return m_fValid;
}

bool CCmdArgPCIAddress::Validate() const
{
  //++
  //   PCI allows up to 256 buses, each with up to 32 devices, each supporting
  // eight functions.  Addresses outside of that range are invalid. 
  //
  // Note that PCI-X enlarges the address space, but we don't support that.
  //++
  if (IsPresent()) {
    if (m_nBus > 255) {
      CMDERRF("PCI bus must be between 0 and 255");  return false;
    }
    if (m_nSlot > 31) {
      CMDERRF("PCI slot must be between 0 and 31");  return false;
    }
    if (m_nFunction > 7) {
      CMDERRF("PCI function must be between 0 and 7");  return false;
    }
  }
  return true;
}


//CCmdArgDiskAddress - disk address in CHS (e.g. "(1,2,3)") 
bool CCmdArgDiskAddress::ScanCHS(const char *&pcNext, uint32_t &nCylinder, uint32_t &nHead, uint32_t &nSector)
{
  //++
  //   This routine will parse a cylinder, head and sector disk address in
  // "(c,h,s)" notation.  If anything about the CHS string is illegal it returns
  // FALSE; otherwise the individual components are parsed and returned.
  //--
  SpanWhite(pcNext);  nCylinder = nHead = nSector = 0;
  if (*pcNext != '(') return ParseError("'(' expected");
  ++pcNext;
  if (!isdigit(SpanWhite(pcNext))) return ParseError("cylinder number expected");
  nCylinder = strtoul(pcNext, (char **) &pcNext, 10);
  if (SpanWhite(pcNext) != ',') return ParseError("',' expected");
  ++pcNext;
  if (!isdigit(SpanWhite(pcNext))) return ParseError("head number expected");
  nHead = strtoul(pcNext, (char **) &pcNext, 10);
  if (SpanWhite(pcNext) != ',') return ParseError("',' expected");
  ++pcNext;
  if (!isdigit(SpanWhite(pcNext))) return ParseError("sector number expected");
  nSector = strtoul(pcNext, (char **) &pcNext, 10);
  if (SpanWhite(pcNext) != ')') return ParseError("')' expected");
  ++pcNext;
  return true;
}

bool CCmdArgDiskAddress::Parse (const char *&pcNext)
{
  //++
  //   This routine parses either a logical block number OR a cylinder, head
  // and sector disk address from the command line. It sets the m_fUseLBN flag
  // as appropriate and extracts BOTH the individual fields AND the entire
  // argument as a string.  The latter is stored in the CCmdArgument base class
  // as the "value".
  //--
  SpanWhite(pcNext);  const char *pcStart = pcNext;
  m_nBlock = m_nCylinder = m_nHead = m_nSector = 0;  m_fValid = m_fUseLBN = false;
  if (isdigit(*pcNext)) {
    m_nBlock = strtoul(pcNext, (char **) &pcNext, 10);
    m_fValid = m_fUseLBN = true;
  } else
    m_fValid = ScanCHS(pcNext, m_nCylinder, m_nHead, m_nSector);
  SetValue(pcStart, pcNext);
  return m_fValid;
}

bool CCmdArgDiskAddress::Validate() const
{
  //++
  //   A (C,H,S) address must be less than the maximum cylinder, head and
  // sector specified.  LBN addresses are range checked against the product
  // of MaxCylinder*MaxHead*MaxSector ...
  //--
  if (IsPresent()) {
    if (m_nCylinder > m_nMaxCylinder) {
      CMDERRF("cylinder must be between 0 and %d", m_nMaxCylinder);  return false;
    }
    if (m_nHead > m_nMaxHead) {
      CMDERRF("head must be between 0 and %d", m_nMaxHead);  return false;
    }
    if (m_nSector > m_nMaxSector) {
      CMDERRF("sector must be between 0 and %d", m_nMaxHead);  return false;
    }
    if (m_nBlock > (m_nMaxCylinder*m_nMaxHead*m_nMaxSector)) {
      CMDERRF("LBN must be between 0 and %ld", (m_nMaxCylinder*m_nMaxHead*m_nMaxSector));  return false;
    }
  }
  return true;
}

uint32_t CCmdArgDiskAddress::ToLBN (uint32_t nCylinder, uint32_t nHead, uint32_t nSector) const
{
  //++
  //   Convert a (C,H,S) address to an LBN, assuming the geometry information
  // we've been given is correct.
  //--
  nSector   = nSector % m_nMaxSector;
  nHead     = nHead   % m_nMaxHead;
  nCylinder = nCylinder % m_nMaxCylinder;
  return ((nCylinder*m_nMaxHead) + nHead)*m_nMaxSector + nSector;
}

void CCmdArgDiskAddress::ToCHS (uint32_t nLBN, uint32_t &nCylinder, uint32_t &nHead, uint32_t &nSector) const
{
  //++
  //   Convert an LBN address to (C,H,S), assuming the disk geometry we were
  // given...
  //--
  nSector = nLBN % m_nMaxSector;  nLBN /= m_nMaxSector;
  nHead = nLBN % m_nMaxHead;
  nCylinder = (nLBN / m_nMaxHead) % m_nMaxCylinder;
}


//CCmdArgNetworkAddress - dotted IP address, with optional port (e.g. 172.16.1.213:80)
bool CCmdArgNetworkAddress::ScanNetworkAddress(const char *&pcNext, uint16_t &nPort, uint32_t &lIP)
{
  //++
  //   This method will try to parse a command line argument with the format
  // "a.b.c.d:p" and extract the IP address and port number. It accepts several
  // variations on the same basic syntax -
  //
  //   a.b.c.d:p -> set the IP address and port number
  //   a.b.c.d   -> set only the IP and leave nPort unchanged
  //   p         -> set only the port number and leave lIP unchanged
  //   :p        -> ditto
  //
  //   Any other syntax returns FALSE and leaves both lIP and nPort arguments
  // unchanged.  Note that only dotted IP addresses (e.g. 127.0.0.1) are
  // currently accepted and host names (e.g. localhost) are not.  Feel free to
  // change that if you're motivated!
  //--
  SpanWhite(pcNext);

  // Handle the case where the first character is a ":" ...
  if (*pcNext == ':') {
    if (!isdigit(*++pcNext)) return ParseError("port number expected");
    nPort = strtoul(pcNext, (char **) &pcNext, 10) & 0xFFFF;
    return true;
  }

  //   Otherwise we need to find a number of some kind - it could be just the
  // port number alone, or it could be the first byte of a dotted IP address.
  if (!isdigit(*pcNext)) return ParseError("IP address expected");
  uint32_t a = strtoul(pcNext, (char **) &pcNext, 10);
  if (*pcNext != '.') return true;

  // We found a "." so scan three more bytes of the dotted IP address ...
  if (!isdigit(*++pcNext)) return ParseError("IP address expected");
  uint32_t b = strtoul(pcNext, (char **) &pcNext, 10);
  if (*pcNext != '.') return ParseError("'.' expected");
  if (!isdigit(*++pcNext)) return ParseError("IP address expected");
  uint32_t c = strtoul(pcNext, (char **) &pcNext, 10);
  if (*pcNext != '.') return ParseError("'.' expected");
  if (!isdigit(*++pcNext)) return ParseError("IP address expected");
  uint32_t d = strtoul(pcNext, (char **) &pcNext, 10);
  lIP = ((a & 0xFF) << 24) | ((b & 0xFF) << 16)
      | ((c & 0xFF) <<  8) |  (d & 0xFF);

  // And lastly, check again for a ":" and a port number ...
  if (*pcNext != ':') return true;
  if (!isdigit(*++pcNext)) return ParseError("port number expected");
  nPort = strtoul(pcNext, (char **) &pcNext, 10) & 0xFFFF;
  return true;
}

bool CCmdArgNetworkAddress::Parse(const char *&pcNext)
{
  //++
  //   This routine parses the command line network address and extracts both
  // the individual fields AND the entire argument as a string.  The latter
  // is stored in the CCmdArgument base class as the "value".
  //--
  SpanWhite(pcNext);  const char *pcStart = pcNext;
  m_fValid = ScanNetworkAddress(pcNext, m_nPort, m_lIP);
  SetValue(pcStart, pcNext);
  return m_fValid;
}


//CCmdArgFileName - file name, which we can optionally open
FILE *CCmdArgFileName::OpenWrite(const char *pszMode)
{
  //++
  //   Open (for writing) the file specified by this argument.  If anything
  // goes wrong, then print an error message and return NULL instead.
  //--
  if (!IsPresent()) {
    CMDERRS("No name specified for " << GetName());  return NULL;
  }
  FILE *f;  int err = fopen_s(&f, GetValue().c_str(), pszMode);
  if (err != 0) {
    CMDERRS("unable (" << err << ") to write " << GetValue());
    return NULL;
  }
  return f;
}

FILE *CCmdArgFileName::OpenRead (const char *pszMode)
{
  //++
  //   Open (for reading) the file specified by this argument.  If the file
  // can't be found (or if anything else goes wrong, for that matter), print
  // an error message and return NULL...
  //--
  if (!IsPresent()) {
    CMDERRS("No name specified for " << GetName());  return NULL;
  }
  FILE *f;  int err = fopen_s(&f, GetValue().c_str(), pszMode);
  if (err != 0) {
    CMDERRS("unable (" << err << ") to read " << GetValue());
    return NULL;
  }
  return f;
}


//CCmdArgNumberRange - address range (e.g. "0x0100-0x01FF")
bool CCmdArgNumberRange::Parse (const char *&pcNext)
{
  //++
  //   This parses either a single number (i.e. exactly the same thing as a
  // CArgNumber) or a numeric range formatted as "start:end" or "start-end".
  // If only one argument is given, then "start" and "end" will be the same.
  //--
  SpanWhite(pcNext);  Reset();
  const char *pcStart = pcNext;
  if (!m_Start.Parse(pcNext)) return ParseError(m_Start.GetError());
  char ch = SpanWhite(pcNext);
  if ((ch == ':') || (ch == '-')) {
    ++pcNext;
    if (!m_End.Parse(pcNext)) return ParseError(m_End.GetError());
  } else {
    //   This is tricky - if there's no ':' or '-' then it's not an error;
    // it just means that the start and end addresses are the same!
    m_End.SetNumber(m_Start.GetNumber());
  }
  SetValue(pcStart, pcNext);  return true;
}

bool CCmdArgNumberRange::Validate() const
{
  //++
  //   A number range is valid if a) the start and ending numbers are valid,
  // and b) the start is .LE. the end ...
  //--
  if (!m_Start.Validate()) return false;
  if (!m_End.Validate()) return false;
  if (m_Start.GetNumber() > m_End.GetNumber()) {
    CMDERRF("%s start must be .LE. end", GetName());  return false;
  }
  return true;
}


//CCmdArgRangeOrName - address range or name
bool CCmdArgRangeOrName::Parse (const char *&pcNext)
{
  //++
  //   Parse EITHER an alphanumeric name OR a numeric address range.  This is
  // a special case argument for the usual EXAMINE and DEPOSIT commands to
  // allow the argument to be either a register name or a range of memory
  // addresses.
  //
  //   The actual implementation is trivial - we just look at the next non-
  // space character, if if it's alphabetic then it's a name and if it's a
  // digit then it's a number range.  Anything else is an error.  If you
  // allow for hexadecimal address ranges (and who doesn't?) then this has
  // and obvious problem, but remember that you can always prefix your hex
  // numbers with "0x...".
  //--
  char ch = SpanWhite(pcNext);
  if (isalpha(ch)) {
    if (!m_Name.Parse(pcNext)) return ParseError(m_Name.GetError());
    SetValue(m_Name.GetValue());  m_fIsName = true;  return true;
  } else if (isdigit(ch)) {
    if (!m_Range.Parse(pcNext)) return ParseError(m_Range.GetError());
    SetValue(m_Range.GetValue());  m_fIsName = false;  return true;
  } else
    return ParseError("expecting name or number");
}

bool CCmdArgRangeOrName::Validate() const
{
  //++
  // Just call Validate() for the range or the name, which ever was used...
  //--
  if (IsRange()) return m_Range.Validate();
  if (IsName()) return m_Name.Validate();
  return true;
}


//CCmdArgList - any of the above arguments repeated in a list
void CCmdArgList::ClearList()
{
  //++
  // Clear the argument list and dispose of all the cloned arguments ...
  //--
  for (ARGUMENT_ITERATOR it = begin(); it != end(); ++it) delete *it;
  m_ArgumentList.clear();
}

bool CCmdArgList::Parse (const char *&pcNext)
{
  //++
  //   This method parses an argument "list", which is one or more arguments,
  // of any type, in a row.  The argument list may be separated by spaces or
  // commas, and it may be enclosed in parentheses.  For every argument we
  // find, we create a clone of that argument object and add it to our own
  // m_ArgumentList vector.  The minimum number of arguments is 1, and there
  // is no maximum.
  //
  //   Note that the syntax of an argument list varies slightly depending on
  // whether the list is an argument to a verb or an argument to a modifier.
  // For verbs, the individual arguments in the list may be separated with
  // spaces or commas, and no special decorations are required.  For example,
  //
  //    DEPOSIT 0x100 0x01 0x02 0x03 0x04 0x05 0x06 0x07
  //
  // The "0x01 ... 0x07" is (or could be!) an argument list of numbers.
  //
  //   For modifiers the entire argument list must be enclosed in parentheses.
  // Commas are typical in this usage, but not required.  For example -
  //
  //    SET SOMETHING/DATA=(0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07)
  //
  // These extra requirements for modifiers avoid any ambiguities about whether
  // an argument belongs to the modifier list or any potential verb argument
  // list.
  //
  //   Note that this argument creates multiple copies of the base argument
  // type on the heap.  It's critically important to call the ClearList()
  // method of this class when you're finished processing the list.  Remember
  // that this CCmdArgList object itself is static and the destructor is never
  // called (well, not until the entire program exits!).  If you don't
  // explicitly call ClearList() to delete the created list after every parse,
  // these objects will hang around forever!
  //--
  SpanWhite(pcNext);  ClearList();  m_BaseArg.Reset();
  const char *pcStart = pcNext;

  // If parentheses are required, then the left one must be next!
  if (m_fParentheses) {
    if (*pcNext != '(') {
      //   Something of a kludge here - if parentheses are required BUT there
      // is exactly one argument, then it's OK.  This is really convenient!
      if (m_BaseArg.Parse(pcNext)) {
        m_ArgumentList.push_back(m_BaseArg.Clone());
        SetValue(pcStart, pcNext);  return true;
      } else
        return ParseError("'(' expected");
    }
    ++pcNext;
  }

  // Parse arguments 'till we can't parse any more!
  while (m_BaseArg.Parse(pcNext)) {
    m_ArgumentList.push_back(m_BaseArg.Clone());
    // If we stopped on a "," then skip the comma and keep going ...
    if (SpanWhite(pcNext) == ',') ++pcNext;
    m_BaseArg.Reset();
  }

  // If parentheses are required, then make sure we have a balanced set!
  if (m_fParentheses) {
    if (SpanWhite(pcNext) != ')') return ParseError("')' expected");
    ++pcNext;
  }

  // All done!
  SetValue(pcStart, pcNext);
  return true;
}

bool CCmdArgList::Validate() const
{
  //++
  //   Validating an argument list simply validates all of its members.  This
  // Validate() method returns true if all the member Validate() methods also
  // return true.  Currently we quit as soon as we find any member that fails
  // to validate - that's open for discussion as to whether we should do them
  // all, regardless, but this seems like the kinder approach...
  //--
  for (ARGUMENT_ITERATOR it = begin(); it != end(); ++it) {
    CCmdArgument *pArg = *it;  assert(pArg != NULL);
    if (!pArg->Validate()) return false;
  }
  return true;
}


////////////////////////////////////////////////////////////////////////////////
//////////////   C O M M A N D   M O D I F I E R   M E T H O D S   /////////////
////////////////////////////////////////////////////////////////////////////////

CCmdModifier *CCmdModifier::Search(const char *pszMod, CCmdModifier * const *paMods)
{
  //++
  //  This method will search thru the table of modifiers and try to find one
  // which matches either the normal sense or the negated sense.  If it finds
  // a match it will set the m_fPresent and (if required) m_fNegated flags for
  // that modifier.  The address of the corresponding CCmdModifier object is
  // then returned, which can then be used by the caller to parse the argument,
  // if any.  If no match can be found, NULL is returned instead.
  //--
  if (paMods == NULL) {
    CMDERRS("unknown modifier " << m_cModifier << pszMod);  return NULL;
  }
  for (int i = 0;  paMods[i] != NULL;  ++i) {
    if (CCmdArgKeyword::Match(pszMod, paMods[i]->m_pszName)) {
      if (paMods[i]->m_fPresent) {
        CMDERRS(*paMods[i] << " already specified");  return NULL;
      }
      paMods[i]->m_fPresent = true;  paMods[i]->m_fNegated = false;  return paMods[i];
    } else if (CCmdArgKeyword::Match(pszMod, paMods[i]->m_pszNoName)) {
      if (paMods[i]->m_fPresent) {
        CMDERRS(*paMods[i] << " already specified");  return NULL;
      }
      paMods[i]->m_fPresent = paMods[i]->m_fNegated = true;  return paMods[i];
    }
  }
  CMDERRS("unknown modifier " << m_cModifier << pszMod);
  return NULL;
}

bool CCmdModifier::ParseArgument (const char *&pcNext)
{
  //++
  //   This method is called to parse the argument for this modifier.  By the
  // time we get here, the name of the modifier has already been parsed (it
  // has to be - otherwise how would we know which modifier object we want the
  // argument for?).  All that's left for us to do is to parse the "=" and the
  // argument (if any)...
  //--
  //   If the modifier doesn't need an argument, then we can quit now.
  if (m_pArg == NULL) return true;

  //  Otherwise, look for a "=" and then try to parse the argument...
  // Note that for some modifiers the argument is optional, in which
  // case it's OK to omit the "=" and value.  Also note that no spaces
  // are allowed between the modifier name, the "=" and the actual argument!
  if (*pcNext != m_cValue) {
    if (m_pArg->IsOptional()) return true;
    CMDERRS("'" << m_cValue << "' expected after " << m_cModifier << m_pszName);
    return false;
  }
  ++pcNext;
  if (!m_pArg->Parse(pcNext)) {
    const char *pszError = m_pArg->GetError();
    if (pszError != NULL) {
      CMDERRS(string(pszError) << " after " << *this << m_cValue);
    } else  if (m_pArg->GetValue().empty()) {
      CMDERRS("argument expected after " << *this << m_cValue);
    } else {
      CMDERRF("extra junk \"%.10s\" after \"%c%s%c%s\"", pcNext, m_cModifier, GetName(), m_cValue, m_pArg->GetValue().c_str());
    }
    return false;
  }
  return true;
}

void CCmdModifier::ShowHelp() const
{
  //++
  // Show the help text for this modifier, including any argument ...
  //--
  string str(string("\t") + m_cModifier + m_pszName);
  if (m_pszNoName != NULL)
    str.append(string(" or ") + m_cModifier + m_pszNoName);
  if (m_pArg != NULL) {
    if (m_pArg->IsOptional()) str.append("[");
    str.append(string("=<") + m_pArg->GetName() + ">");
    if (m_pArg->IsOptional()) str.append("]");
    if (!m_fOptional) str.append(" (required)");
  }
  CMDOUTS(str);
}


////////////////////////////////////////////////////////////////////////////////
//////////////////   C O M M A N D   V E R B   M E T H O D S   /////////////////
////////////////////////////////////////////////////////////////////////////////

CCmdVerb *CCmdVerb::Search(const char *pszVerb, CCmdVerb * const aVerbs[], bool fError)
{
  //++
  //   This method will search thru the table of verbs and attempt to locate
  // one that matches the name given.  If it finds a match then it will return
  // the address of the corresponding CCmdVerb object.  If no match is found,
  // then NULL is returned.  Note that this just does a simple linear search
  // of the verb names - it's up to you to ensure that abbreviations cause no
  // ambiguities in the names.
  //--
  for (int i = 0;  aVerbs[i] != NULL;  ++i) {
    if (CCmdArgKeyword::Match(pszVerb, aVerbs[i]->m_pszVerb)) return aVerbs[i];
  }
  if (fError) CMDERRS("unknown command \"" << pszVerb << "\"");
  return NULL;
}

bool CCmdVerb::ParseArgument (const char *&pcNext, int &nArgs, CCmdArgument * const paArgs[])
{
  //++
  //   This method is called when the verb parser finds an argument on the
  // command line.  It first makes sure that this verb actually needs another
  // argument and then, assuming it does, parses the argument...
  //--
  if (paArgs == NULL) {
    CMDERRF("too many arguments \"%.10s\"", pcNext);  return false;
  }
  CCmdArgument *pArg = paArgs[nArgs];
  if (pArg == NULL) {
    CMDERRF("too many arguments \"%.10s\"", pcNext);  return false;
  }
  if (!pArg->Parse(pcNext)) {
    const char *pszError = pArg->GetError();
    if (pszError != NULL) {
      CMDERRF("%s before \"%.10s\"", pszError, pcNext);
    } else if (pArg->GetValue().empty()) {
      CMDERRF("missing argument before \"%.10s\"", pcNext);
    } else {
      CMDERRF("extra junk \"%.10s\" after argument \"%s\"", pcNext, pArg->GetValue().c_str());
    }
    return false;
  }
  ++nArgs;  return true;
}

bool CCmdVerb::ParseModifier (const char *&pcNext, CCmdModifier * const paMods[])
{
  //++
  //   This method is called when a modifier character (i.e. "/") is found
  // on the command line.  It will first parse the modifier name, then look
  // it up, and finally parse the modifier argument (if any) ...
  //--
  if (!isalnum(*++pcNext)) {
    CMDERRF("found \"%.10s\" after %c (modifier expected)", pcNext, CCmdModifier::m_cModifier);
    return false;
  }
  string sMod(CCmdArgName::ScanName(pcNext));
  CCmdModifier *pMod = CCmdModifier::Search(sMod.c_str(), paMods);
  if (pMod == NULL) return false;
  return pMod->ParseArgument(pcNext);
}

bool CCmdVerb::ParseTail (const char *&pcNext, CCmdArgument * const paArgs[], CCmdModifier * const paMods[])
{
  //++
  //   This method parses the entire argument and modifier list for this verb
  // (that is to say, the rest of the command line after the actual verb name).
  //--
  int nArgs = 0;  ResetArguments(paArgs);  ResetModifiers(paMods);
  while (!IsEOS(SpanWhite(pcNext)) && !IsComment(*pcNext)) {
    //   Now we need to decide whether this is an argument or a modifier.
    // The process is amazingly simple - if it starts with a "/" then it's
    // a modifier, and anything else is an argument...
    if (IsModifier(*pcNext)) {
      if (!ParseModifier(pcNext, paMods)) return false;
    } else {
      if (!ParseArgument(pcNext, nArgs, paArgs)) return false;
    }
  }
  return true;
}

bool CCmdVerb::ParseVerb (CCmdParser &cmd, const char *&pcNext, CCmdVerb * const aVerbs[])
{
  //++
  //   This method parses an entire command line.  It scans the verb and looks
  // it up in the verb table given.  If that verb has a table of subverbs, then
  // we recurse and do the whole thing over again using the subverb table.
  // When we get to a "simple" verb (one without a subverb) then we parse the
  // arguments and modifiers for that verb.  Assuming all that parsing is
  // successful, we finish by calling the action routine associated with the
  // last (sub)verb found.
  //--

  // Scan the verb and find it in the verb table ..
  string sVerb(CCmdArgName::ScanName(pcNext));
  if (sVerb.empty()) {
    CMDERRF("found \"%.10s\" (command expected)", pcNext);  return false;
  }
  CCmdVerb *pVerb = CCmdVerb::Search(sVerb.c_str(), aVerbs);
  if (pVerb == NULL) return false;

  //   If this verb has subverbs, then just recurse ...  You've probably
  // noticed that this also means any arguments or modifiers for this verb
  // (the parent one, that is, the one with the subverbs) are ignored.  Only
  // the argument list and modifiers for the subverb matter.  That's the
  // easiest thing to do and makes the most sense for the simple subverbs we
  // have (e.g. "SET ..." and "SHOW ...") but it isn't the only option.  We
  // could, for example, treat any modifiers for the parent verb as "global"
  // modifiers that are valid for all subverbs.
  if (pVerb->m_paSubVerbs != NULL)
    return ParseVerb(cmd, pcNext, pVerb->m_paSubVerbs);

  // It's a simple verb - parse the rest of the command line ...
  if (!ParseTail(pcNext, pVerb->m_paArguments, pVerb->m_paModifiers)) return false;

  // Validate all the arguments and modifiers ...
  if (!ValidateArguments(pVerb->m_paArguments)) return false;
  if (!ValidateModifiers(pVerb->m_paModifiers)) return false;

  // And call the action routine ...
  assert(pVerb->m_pAction != NULL);
  return (*pVerb->m_pAction) (cmd);
}

bool CCmdVerb::ParseIndirect (CCmdParser &cmd, const char *&pcNext, CCmdVerb * const aVerbs[])
{
  //++
  //   This method gets called when we find a command line that starts with an
  // "@" character, and that is taken to be a shorthand for running a script.
  // It's a little bit of a hack, but what we do is to check this application's
  // parse tables to see if a command named "DO" is defined.  If one exists,
  // then use that definition to parse and execute the rest of the command line.
  // Otherwise it's an error ...
  //
  //   Note that by the time we get here the "@" has already been parsed, so
  // pcNext points to the file name.
  //--
  CCmdVerb *pVerb = CCmdVerb::Search("DO", aVerbs);
  if (pVerb == NULL) {
    CMDERRS("script files not supported");  return false;
  }
  if (!(   ParseTail(pcNext, pVerb->m_paArguments, pVerb->m_paModifiers)
        && ValidateArguments(pVerb->m_paArguments)
        && ValidateModifiers(pVerb->m_paModifiers))) return false;
  assert(pVerb->m_pAction != NULL);
  return (*pVerb->m_pAction) (cmd);
}

void CCmdVerb::ResetArguments (CCmdArgument * const paArgs[])
{
  //++
  // Reset all arguments to their default state ...
  //--
  if (paArgs == NULL) return;
  for (int i = 0; paArgs[i] != NULL; ++i)  paArgs[i]->Reset();
}

void CCmdVerb::ResetModifiers (CCmdModifier * const paMods[])
{
  //++
  // Reset all modifiers to their default state ...
  //==
  if (paMods == NULL) return;
  for (int i = 0; paMods[i] != NULL; ++i)  paMods[i]->Reset();
}

bool CCmdVerb::ValidateArguments (CCmdArgument * const paArgs[])
{
  //++
  //   This method will verify that all required arguments are present, and
  // that all arguments present are valid.  The latter is a semantic test and
  // checks for valid keywords, numeric values within range, etc.
  //
  //   Note that because arguments are positional, once an optional argument
  // is found all arguments after that must be optional too.  We don't bother
  // to verify this, though, and it's up to the guy who defines the command
  // tables to do it correctly.
  //--
  if (paArgs == NULL) return true;
  for (int i = 0;  paArgs[i] != NULL;  ++i) {
    if (!paArgs[i]->IsPresent()) {
      if (!paArgs[i]->IsOptional()) {
        CMDERRS("expected argument for " << paArgs[i]->GetName());  return false;
      }
    } else if (!paArgs[i]->Validate()) {
      //CMDERRS("invalid value \"" << *paArgs[i] << "\" for  " << paArgs[i]->GetName());
      return false;
    }
  }
  return true;
}

bool CCmdVerb::ValidateModifiers (CCmdModifier * const paMods[])
{
  //++
  //   This method will verify that any required modifiers (they're rare, but
  // they do exist) are present and that any modifiers with values are valid.
  //--
  // Make sure any required modifiers are present ...
  if (paMods == NULL) return true;
  for (int i = 0;  paMods[i] != NULL;  ++i) {
    if (!paMods[i]->IsPresent() && !paMods[i]->IsOptional()) {
      CMDERRS("modifier " << *paMods[i] << " is required");
      return false;
    }
    if (!paMods[i]->IsPresent()) continue;
    CCmdArgument *pArg = paMods[i]->GetArg();
    if (pArg == NULL) continue;
    if (!pArg->IsPresent()) {
      if (!pArg->IsOptional()) {
        CMDERRS("expected argument for " << *paMods[i]);  return false;
      }
    } else if (!pArg->Validate()) {
      //CMDERRS("invalid value \"" << *pArg << "\" for  " << *paMods[i]);
      return false;
    }
  }
  return true;
}

void CCmdVerb::ShowModifiers (CCmdModifier * const paMods[])
{
  //++
  // Show the modifiers for this verb (used by the help command) ...
  //--
  if (paMods != NULL) {
    CMDOUTS("\nModifiers:");
    for (int i = 0;  paMods[i] != NULL;  ++i)
      paMods[i]->ShowHelp();
  } else {
    CMDOUTS("\nThis command has no modifiers.");
  }
}

void CCmdVerb::ShowArguments (const char *pszVerb, CCmdArgument * const paArgs[], const char *pszPrefix)
{
  //++
  // Show the argument list for this verb (used by the help command) ...
  //--
  string str("\t");
  if (pszPrefix != NULL) {
    str.append(pszPrefix);  str.append(" ");
  }
  str.append(pszVerb);
  if (paArgs != NULL) {
    for (int i = 0;  paArgs[i] != NULL;  ++i) {
      CCmdArgument *pArg = paArgs[i];
      if (pArg->IsOptional())
        str.append(string(" [<") + pArg->GetName() + string(">]"));
      else
        str.append(string(" <") + pArg->GetName() + string(">"));
    }
  }
  CMDOUTS(str);
}

void CCmdVerb::ShowVerb (const char *pszPrefix) const
{
  //++
  //   Show the help text for this verb only, including both the argument
  // list and all modifiers.  This function is used ONLY when this verb
  // does not have any sub verbs!
  //--
  CMDOUTS("\nFormat:");
  ShowArguments(m_pszVerb, m_paArguments, pszPrefix);
  ShowModifiers(m_paModifiers);
  CMDOUTS("");
}

void CCmdVerb::ShowHelp() const
{
  //++
  //   Show the help text for this complete verb definition.  If this verb has
  // subverbs, then we iterate showing the help for each subverb (each of which
  // may have completely different arguments and/or modifiers).  If this is a
  // simple verb with no subverbs, then just show the help for this one.
  //
  //   Notice that this means "parent" verbs (e.g. the "SET" verb in the list
  // of all "SET xyz" commands) never display any of the parent's modifiers or
  // arguments.  That's consistent with the ParseVerb() method, which also
  // ignores modifiers and arguments for the parent.
  //--
  if (m_paSubVerbs != NULL) {
    for (uint32_t i = 0;  m_paSubVerbs[i] != NULL; ++i)
      m_paSubVerbs[i]->ShowVerb(m_pszVerb);
  } else
    ShowVerb();
}


////////////////////////////////////////////////////////////////////////////////
/////////////////   C O M M A N D   A L I A S   M E T H O D S   ////////////////
////////////////////////////////////////////////////////////////////////////////

void CCmdAliases::ToUpper (string &str)
{
  //++
  //   Fold a string to upper case.  Why there isn't a version of this built
  // into the std::string class, I can't understand!
  //--
  for (string::iterator it = str.begin();  it != str.end();  ++it)
    *it = toupper(*it);
}

bool CCmdAliases::Define (string sAlias, string sSubstitution, bool fRedefine)
{
  //++
  //   Add a new alias definition to this object.  If the alias is already
  // defined, then return false and leave the current definition unchanged
  // UNLESS fRedefine is true.  In that case, silently redefine any existing
  // alias and always return true.
  //
  //   Note that alias names are ALWAYS stored in upper case!
  //--
  ToUpper(sAlias);  iterator it = m_Aliases.find(sAlias);
  if (it != m_Aliases.end()) {
    if (!fRedefine) return false;
    m_Aliases.erase(it);
  }
  m_Aliases[sAlias] = sSubstitution;
  return true;
}

bool CCmdAliases::Undefine (string sAlias)
{
  //++
  //   Delete an alias from our hash table.  If the alias is not currently
  // defined, then return false and no nothing.
  //--
  ToUpper(sAlias);  iterator it = m_Aliases.find(sAlias);
  if (it == m_Aliases.end()) return false;
  m_Aliases.erase(it);
  return true;
}

bool CCmdAliases::Expand (char *pszCommand, size_t cbCommand) const
{
  //++
  //   Attempt to expand any alias name in the command line and, in that case,
  // return a completely new command line. Notice that this routine may (will!)
  // completely overwrite the caller's buffer in the event an alias replacement
  // occurs.  It returns true if an alias substitution is made, and false if
  // no aliases can be found.
  //
  //   The current version is pretty simple and only allows for a single alias
  // at the beginning of the command line.  That alias name must be the first
  // non-blank atom on the line, however it may be followed by additional text
  // that will be retained (provided the command buffer is big enough).
  //
  //   The cbCommand parameter specifies the maximum size of the caller's
  // buffer.  If the alias expansion would exceed this, the result is silently
  // truncated.
  //--

  //   Look for a single alias name followed by EOL.  BTW, note that ScanName()
  // always folds the result to upper case, and the alias names are always
  // stored in upper case, so this is a case insensitive match!
  const char *pc = pszCommand;
  string sAlias(CCmdArgName::ScanName(pc));
  if (sAlias.empty() || !IsDefined(sAlias)) return false;

  //   Make a temporary copy of whatever text remains on the original command
  // line; overwrite the caller's buffer with the alias expansion, and then
  // append the additional text (if any) ...
  size_t cbTail = strlen(pc) + 1;  char *pszTail = DBGNEW char[cbTail];
  memset(pszTail, 0, cbTail);  strcpy_s(pszTail, cbTail, pc);
  strcpy_s(pszCommand, cbCommand, m_Aliases.at(sAlias).c_str());
  strcat_s(pszCommand, cbCommand, pszTail);
  delete []pszTail;

  // All done!
  return true;
}


////////////////////////////////////////////////////////////////////////////////
////////////////   C O M M A N D   P A R S E R   M E T H O D S   ///////////////
////////////////////////////////////////////////////////////////////////////////

CCmdParser::CCmdParser (const char *pszPrompt, CCmdVerb * const aVerbs[], ConfirmExit_t *pConfirm, CConsoleWindow *pConsole)
{
  //++
  // Just initialize everything...
  //--
  m_sPrompt = pszPrompt;  m_paVerbs = aVerbs;  //m_sSubVerb.clear();
  m_pConfirmExit = pConfirm;  m_pConsole = pConsole;  m_nScriptLevel = 0;
  for (uint32_t i = 0;  i < MAXDEPTH;  ++i) {
    m_asScriptName[i].clear();  m_anScriptLine[i] = 0;  m_apScriptFile[i] = NULL;
  }
}

bool CCmdParser::ReadConsole (string sPrompt, char *pszBuffer, size_t cbBuffer)
{
  //++
  //   This routine reads the next line from stdin and leaves it in the command
  // buffer. It returns false only if we find EOF on standard input, which should
  // be taken as a signal that we should exit ASAP ...
  //--
  assert((pszBuffer != NULL) && (cbBuffer > 0));
  if (m_pConsole != NULL) {
    if (!m_pConsole->ReadLine(sPrompt.c_str(), pszBuffer, cbBuffer)) return false;
  } else {
    fprintf(stdout, "%s", sPrompt.c_str());  fflush(stdout);
    if (fgets(pszBuffer, (int) cbBuffer, stdin) == NULL) return false;
    char *psz = strrchr(pszBuffer, '\n');
    if (psz != NULL) *psz = '\0';
  }
  CLog::GetLog()->LogOperator(sPrompt, pszBuffer);
  return true;
}

bool CCmdParser::AreYouSure (string sPrompt, bool fDefault)
{
  //++
  //   As you might guess, this routine asks the user "Are you sure?" and
  // then waits for a "yes" or "no" response.  It returns true if the user
  // says "yes" and false for "no".  Any other response just loops ...
  //
  char szAnswer[MAXCMD];
  while (true) {
    string str;
    if (!sPrompt.empty()) str = sPrompt + " - ";
    if (!ReadConsole(str+"Are you sure?", szAnswer, sizeof(szAnswer))) return fDefault;
    if (strieql(szAnswer, "y") || strieql(szAnswer, "yes")) return true;
    if (strieql(szAnswer, "n") || strieql(szAnswer, "no")) return false;
    CMDERRS("please answer \"yes\" or \"no\"");
  }
}

string CCmdParser::SetDefaultExtension (string sFileName, const char *pszDefExt)
{
  //++
  // Set the extension, if it doesn't already have one, of the file name ...
  //--

  //   This has been rewritten to use the portable SplitPath()/MakePath()
  // functions in EMULIB.cpp.  The old code is left below, commented out, 
  // just in case there's an issue with the new version!
//char szDrive[_MAX_DRIVE], szPath[_MAX_DIR], szName[_MAX_FNAME], szExt[_MAX_EXT];
//char szResult[_MAX_PATH];  errno_t ret;
//ret = _splitpath_s(sFileName.c_str(),
//  szDrive, _MAX_DRIVE, szPath, _MAX_DIR, szName, _MAX_FNAME, szExt, _MAX_EXT);
//if (ret != 0) return sFileName;
//if (strlen(szExt) == 0) strcpy_s(szExt, sizeof(szExt), pszDefExt);
//ret = _makepath_s(szResult, _MAX_PATH, szDrive, szPath, szName, szExt);
//if (ret != 0) return sFileName;
//return string(szResult);

  // And the new version ...
  string sDrive, sDir, sName, sExt;
  if (!SplitPath(sFileName, sDrive, sDir, sName, sExt)) return sFileName;
  if (sExt.empty()) sExt = pszDefExt;
  return MakePath(sDrive, sDir, sName, sExt);
}

bool CCmdParser::OpenScript (string sFileName)
{
  //++
  //   This method opens an indirect command file (aka a script).  Commands
  // are read from this file one line at a time and executed as though they
  // were typed on the console.  Any error aborts the current script file.
  // At the moment, script files cannot be nested.
  //
  //   Note that script file commands are NOT echoed on the console.
  //--
  if ((m_nScriptLevel+1) >= MAXDEPTH) {
    CMDERRS("script files nested too deeply");  return false;
  }
  uint32_t nLvl = ++m_nScriptLevel;
  sFileName = SetDefaultExtension(sFileName, ".cmd");
  int err = fopen_s(&m_apScriptFile[nLvl-1], sFileName.c_str(), "rt");
  if (err != 0) {
    CMDERRS("unable (" << err << ") to open script " << sFileName);
    --m_nScriptLevel;  return false;
  }
  LOGS(DEBUG, "script " << sFileName << " opened");
  m_asScriptName[nLvl-1] = sFileName;  m_anScriptLine[nLvl-1] = 0;
  return true;
}

void CCmdParser::CloseScript()
{
  //++
  // Close the current script file.
  //--
  if (m_nScriptLevel == 0) return;
  uint32_t nLvl = m_nScriptLevel--;
  LOGS(DEBUG, "script " << m_asScriptName[nLvl-1] << " closed");
  fclose(m_apScriptFile[nLvl-1]);  m_apScriptFile[nLvl-1] = NULL;
  m_asScriptName[nLvl-1].clear();  m_anScriptLine[nLvl-1] = 0;
}

bool CCmdParser::ReadScript (char *pszBuffer, size_t cbBuffer)
{
  //++
  //   Read the next line from the current script file.  If any errors or EOF
  // are found while reading, return false (although the script isn't closed
  // automatically - the caller is expected to do that).
  //--
  if (!InScript()) return false;
  if (fgets(pszBuffer, MKINT32(cbBuffer), GetScriptFile()) == NULL) return false;
  size_t len = strlen(pszBuffer);
  assert(len > 0);
  if (pszBuffer[len-1] == '\n') pszBuffer[--len] = '\0';
  if (pszBuffer[len-1] == '\r') pszBuffer[--len] = '\0';
  ++m_anScriptLine[m_nScriptLevel-1];
  CLog::GetLog()->LogScript(GetScriptName(), pszBuffer);
  return true;
}

void CCmdParser::ScriptError (bool fAbort)
{
  //++
  //   This method is invoked whenever any error occurs while executing a
  // script file.  We add a supplemental error message adding the name of
  // the script and the line number, and then the script is closed.
  //
  //   Note that if scripts are nested, then an error closes ALL currently
  // open scripts starting from the bottom and working up!
  //--
  if (!InScript()) return;
  CMDERRS("error in script " << GetScriptName() << " line " << GetScriptLine());
  CloseScript();
  while (InScript()) {
    CMDERRS("called from script " << GetScriptName() << " line " << GetScriptLine());
    CloseScript();
  }
}

bool CCmdParser::DefineAlias (string sAlias, string sSubstitution)
{
  //++
  //   Define a new command alias.  Most of the actual work is done by the
  // CCmdAliases object, but we have to check for conflicts with built in
  // command names first.  No alias name is allowed to overlap a built in
  // name or any legal abbreviation there of!  However, notice that we do
  // allow existing aliases to be redefined w/o error.
  //--
  CCmdVerb *pVerb = CCmdVerb::Search(sAlias, m_paVerbs, false);
  if (pVerb != NULL) {
    CMDERRS("alias " << sAlias << " conflicts with " << pVerb->GetName() << " command");
    return false;
  }
  return m_Aliases.Define(sAlias, sSubstitution, true);
}

bool CCmdParser::UndefineAlias (string sAlias)
{
  //++
  // Undefine an alias name, and print an error message if it's not defined.
  //--
  if (!m_Aliases.Undefine(sAlias)) {
    CMDERRS("alias " << sAlias << " is not defined");  return false;
  }
  return true;
}

bool CCmdParser::ReadCommand ()
{
  //++
  //   Read a command line from either the console or the indirect file,
  // as required.  The result always goes in m_szCmdBuf and, if reading
  // from the console, we always use the prompting string.
  //--
  while (InScript()) {
    if (ReadScript(m_szCmdBuf, sizeof(m_szCmdBuf))) return true;
    CloseScript();
  }
  return ReadConsole(GetPrompt(), m_szCmdBuf, sizeof(m_szCmdBuf));
}

bool CCmdParser::ParseCommand (const char *&pcNext)
{
  //++
  //   Parse this entire command line and dispatch to the correct verb action
  // routine.  This is pretty trivial since the CCmdVerb class does all the
  // real work.  All we have to worry about is ignoring blank and/or comment
  // lines, and handling special characters like "@".
  //--
  if (IsEOS(SpanWhite(pcNext))) return true;
  if (IsComment(*pcNext)) return true;
  if (IsIndirect(*pcNext))
    return CCmdVerb::ParseIndirect(*this, ++pcNext, m_paVerbs);
  else
    return CCmdVerb::ParseVerb(*this, pcNext, m_paVerbs);
}

void CCmdParser::CommandLoop()
{
  //++
  //   This method is the main "command loop" - it simply reads and executes
  // command lines more or less forever.  It's intended to be called as the
  // background task for the main program and it only returns under either of
  // two circumstances - 1) if we find EOF on the console (e.g. the operator
  // enters ^Z), or 2) if the ExitLoop() method is called by any verb.  The
  // latter is official way for "quit" or "exit" commands to terminate the
  // program.
  //--
  while (true) {

    // Initialize all the various flags we use for exiting!
    SetExitRequest(false);
    if (IsConsoleAttached()) GetConsole()->SetForcedExit(false);

    // Process commands until EXIT or EOF in the console ...
    while (!IsExitRequested() && ReadCommand()) {
      if (m_Aliases.Expand(m_szCmdBuf, MAXCMD)) {
        LOGS(DEBUG, "expanded to \"" << m_szCmdBuf << "\"");
      }
      const char *pcNext = m_szCmdBuf;
      if (!ParseCommand(pcNext)) {
        if (InScript()) ScriptError();
      }
    }

    //   If the operator has typed Control-Z or Control-BREAK on the console
    // then we want to exit, but we first allow the application to ask for
    // confirmation just in case there are unsaved files or attached devices.
    // If the operator changes his mind, then we clear the console EOF flag
    // and restart the command scanner.
    //
    //   BUT, there are two cases in which we don't ask for confirmation.  The
    // first is if m_fExitLoop is set - in that case, it's assumed that this
    // application's EXIT or QUIT command has already taken care of that.  The
    // other time we don't ask is if console window is being closed or if this
    // PC is being shut down - in those cases there is no choice!
    //
    //   Either way, appropriate clean up is left to the application!
    if (IsExitRequested()) return;
#if defined(_WIN32)
    if (IsConsoleAttached() && GetConsole()->IsSystemShutdown()) return;
#endif
    //   A little bit of a kludge here - the ConfirmExit procedure is allowed
    // to, and in fact it almost certainly will, call AreYouSure() and that
    // will try to read from the console.  Can't do that if the console's
    // forced EOF flag is still set, so we'll have to clear it first.
    if (IsConsoleAttached()) GetConsole()->SetForcedExit(false);
    if (ConfirmExit()) return;

    // If we get all the way thru to here, don't exit!
  }
}


////////////////////////////////////////////////////////////////////////////////
//////////////////////////   H E L P   C O M M A N D   /////////////////////////
////////////////////////////////////////////////////////////////////////////////

// Verb and argument definitions for the huilt in "HELP" command ...
CCmdArgName CCmdParser::g_argHelp("verb", true);
CCmdArgument * const CCmdParser::g_argsHelp[] = {&g_argHelp, NULL};
CCmdVerb CCmdParser::g_cmdHelp("H*ELP", CCmdParser::DoHelp, g_argsHelp, NULL);

void CCmdParser::ShowVerbs (CCmdParser &cmd)
{
  //++
  // Show a list of all known verbs ...
  //--
  CMDOUTS("\nValid commands are:\n");
  for (int i = 0;  cmd.m_paVerbs[i] != NULL;  ++i)
    CMDOUTS("\t" << cmd.m_paVerbs[i]->GetName());
  CMDOUTS("\nFor more information type \"HELP <verb>\"\n");
}

bool CCmdParser::DoHelp (CCmdParser &cmd)
{
  //++
  //   This HELP command prints out a simple description of the verbs, arguments
  // and modifiers simply by decoding the internal parser objects.  It's part
  // of the CCmdParser class because this version is generic and works with any
  // UI, regardless of the actual command set...
  //
  // Format:
  //    HELP
  //    HELP <verb>
  //--
  if (!g_argHelp.IsPresent()) {
    ShowVerbs(cmd);  return true;
  } else {
    CCmdVerb *pVerb = CCmdVerb::Search(g_argHelp.GetValue(), cmd.m_paVerbs);
    if (pVerb == NULL) return false;
    pVerb->ShowHelp();  return true;
  }
}
