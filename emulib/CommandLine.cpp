//++
// CommandLine.cpp -> CCommandLine (siple argc/argv parser) methods
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
//   This class is a simple C++ version of the classic C library getopt()
// routine.  It parses the argc and argv[] command line passed to all C
// programs according to a simple set of syntax rules.
//
//   * All options consist of a single character, 'A'-'Z', 'a'-'z' or '0'-'9',
//     and are always preceeded by either a '-' or '/' character. 
//
//   * Some options may optionally take arguments.  If an option does have an
//     argument, it may either be part of the same argv element (for example,
//     "-ofile.txt" or possibly "-o=file.txt") or in the next element (e.g.
//     "-o file.txt"). 
//
//   * Options which do not take arguments may be combined, for example "-xvf"
//     is equivalent to "-X -v -f".
//
//   * Anything which is not an option is assumed to be a program argument.
//     A lone "-" without any letter or digit is treated as an argument (this
//     syntax is used to specify input from stdin, for example).
//
//   * New style long options (e.g. "--output=foo") are not accepted.
//
//   A getopt() style option string is passed to the class constructor, and
// this string specifies the legal options (e.g. "flx" says that only the "-f",
// "-l" and "-x" options are accepted).  As with getopt(), if an letter in the
// string is followed by a ":" then that option requires an argument, and if it
// is followed by a "+" then that option optionally accepts an argument.  Two
// other parameters to the constructor specify the minimum and maximum number
// of non-option command line arguments allowed.
//
//   The basic plan for using this class is to first create a CCommandLine
// object and give the constructor the list of legal options and the number of
// arguments expected. Next, call the Parse() method and pass it the argc and
// argv[] values from main().  It will return false if the command line is
// invalid, in which case the caller will usually print a help text and exit.
//
//   After that, this CComandLine object contains the command line and the
// caller can examine it as his convenience. There are iterators for traversing
// the list of options and arguments, or the caller may use IsOptionPresent()
// and GetOptionValue or GetArgument() to get individual values.  
//
// Bob Armstrong <bob@jfcl.com>   [24-OCT-2015]
//
// REVISION HISTORY:
// 24-OCT-15  RLA   New file.
//  1-JUN-17  RLA   Linux port.
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
#include "CommandLine.hpp"      // declarations for this module



CCommandLine::CCommandLine (const char *pszValidOptions, uint32_t nMinArgs,
                uint32_t nMaxArgs, bool fCaseSensitive, const char *pszOptionPrefix)
             : m_sValidOptions(pszValidOptions), m_nMaxArguments(nMaxArgs), m_nMinArguments(nMinArgs),
               m_sOptionPrefix(pszOptionPrefix), m_fCaseSensitive(fCaseSensitive)
{
  //++
  //   Initialize this object instance with a list of valid option characters.
  // Optionally, you can also specify the miniumum and maximum number of 
  // arguments allowed (the default is zero to unlimited).  Optionally, you
  // can specify whether option matching should be case sensitive (the default
  // is true), and optionally you can specify the set of option prefix char-
  // acters (by default it's "-/").  You might change the latter to "-" if
  // you wanted to allow only Un*x style options.
  //--
  ClearArguments();  ClearOptions();
}


CCommandLine::OPTION_TYPE CCommandLine::GetOptionType (char ch) const
{
  //++
  //   This method searches m_sValidOptions for a specific option and returns
  // it's option type.  Remember - if the character isn't in the list of valid
  // options then it's invalid.  If it's in there and followed by a ":" then
  // a value is required, and if it's followed by a "+" a value is optional.
  //--
  if (m_sValidOptions.length() == 0) return ILLEGAL;
  size_t npos = m_sValidOptions.find(FixCase(ch));
  if (npos == string::npos) return ILLEGAL;
  if (npos >= m_sValidOptions.length()-1) return NO_VALUE;
  if (m_sValidOptions[npos+1] == ':') return VALUE_REQUIRED;
  if (m_sValidOptions[npos+1] == '+') return VALUE_OPTIONAL;
  return NO_VALUE;
}


bool CCommandLine::ParseValue (const char *pszProgram, const char *pszArg, string &sValue)
{
  //++
  //   This routine will parse and return the value of an option, assuming
  // it's embedded with the option name.  For example, passing the option
  // "-o=foo.txt" would return "foo.txt".  Likewise, the syntax "-o:foo.txt"
  // is accepted, as is the more traditional "-ofoo.txt".  If no option value
  // is present (i.e. the option string passed is just two characters) then
  // false is returned.
  //
  // Note: the syntax "-o:" or "-o=" is accepted and returns a null string.
  //--
  if (strlen(pszArg) <= 2) return false;
  if ((pszArg[2] == ':') || (pszArg[2] == '='))
    sValue = &pszArg[3];
  else
    sValue = &pszArg[2];
  return true;
}


bool CCommandLine::ParseOption (const char *pszProgram, int &narg, int argc, const char * const argv[])
{
  //++
  //   This method is called by Parse() when it has determined that a particular
  // argv[] element is an option.  This code takes care of figuring out the
  // option's value, if any, and adding the pair to the m_OptionList map.  If
  // any syntax errors are found it returns false.
  //--
  char op = FixCase(argv[narg][1]);  string ov;
  switch (GetOptionType(op)) {

    case VALUE_OPTIONAL:
      //   This option has an optional value.  An optional value never uses the
      // next argv[] element, so check for the "-o=...", "-o:..." or "-o..."
      // syntax.  If none is found, just save the option with a null value.
      if (ParseValue(pszProgram, argv[narg], ov))
        AddOption(op, ov);
      else
        AddOption(op, "");
      return true;

    case VALUE_REQUIRED:
      //   This option has a required value.  It accepts the same "-o=...",
      // "-o:..." and "-o..." syntax as above, but in this case if nothing is
      // found we'll absorb the next argv[] element as this option's value.
      // It fails only if there's nothing more left on the command line.
      if (ParseValue(pszProgram, argv[narg], ov))
        AddOption(op, ov);
      else {
        if (++narg >= argc) {
          fprintf(stderr, "%s: value required for %s", pszProgram, argv[narg-1]);
          return false;
        } else
          AddOption(op, argv[narg]);
      }
      return true;

    case NO_VALUE:
      //   This option doesn't allow a value.  That's easy - this element must
      // be exactly two characters long (e.g. "-o") ...
      if (strlen(argv[narg]) == 2) {
        AddOption(op);  return true;
      } else {
        fprintf(stderr, "%s: junk after option \"%s\"", pszProgram, argv[narg]);
        return false;
      }

    case ILLEGAL:
    default:
      // This option isn't on the allowed option list at all ...
      fprintf(stderr, "%s: illegal option \"%2s\"", pszProgram, argv[narg]);
      return false;
  }
}


bool CCommandLine::Parse (const char *pszProgram, int argc, const char * const argv[])
{
  //++
  //   This method parses a traditional C style argv[] array and divides it
  // up into options, which go on the m_OptionList map, and arguments, which
  // get added to the m_ArgumentList vector.  It returns true if all is well,
  // and false if there is something wrong with the arguments (e.g. invalid
  // option, too many or too few arguments, etc).  In this case, it's assumed
  // that the caller will print some kind of "USAGE: ..." message and exit.
  //
  //   BTW, note that argv[0] is always ignored.  We have no need for the
  // program/command name, but the caller can always extract it if necessary.
  //--
  ClearArguments();  ClearOptions();;

  // Go thru the argv[] vector and find all the options and arguments ...
  for (int n = 1;  n < argc;  ++n) {
    if (IsOptionPrefix(argv[n][0]) && (strlen(argv[n]) > 1)) {
      // This looks like an option ...
      if (!ParseOption(pszProgram, n, argc, argv)) return false;
    } else
      // Not an option - it must be an argument ...
      AddArgument(argv[n]);
  }

  // Validate the argument count ...
  if (GetArgumentCount() < m_nMinArguments) {
    fprintf(stderr, "%s: more arguments required", pszProgram);  return false;
  } else if (GetArgumentCount() > m_nMaxArguments) {
    fprintf(stderr, "%s: too many arguments", pszProgram);  return false;
  }
  return true;
}


string CCommandLine::GetOptionValue (char ch) const
{
  //++
  //   Return the value string associated with the specified option.  If this
  // particular option was not present on the command line, return a null
  // string instead.  You can use IsOptionPresent() before calling this method
  // to determine whether the option was specified at all.
  //--
  option_iterator it = m_OptionList.find(FixCase(ch));
  return (it == m_OptionList.end()) ? "" : it->second;
}


string CCommandLine::GetArgument (uint32_t n) const
{
  //++
  //   Return the n'th command line argument, where arguments are numbred 
  // starting from zero.  If n exceeds the number of arguments actually present
  // on the command line, a null string is returned instead.  You can use the
  // GetArgumentCount() method to determine the actual number of arguments.
  //--
  return (n < GetArgumentCount()) ? m_ArgumentList[n] : "";
}


string CCommandLine::BuildCommand() const
{
  //++
  //   This method builds a facsimile of the original command line in canonical
  // form - all options are listed at the beginning and all arguments appear at
  // the end.  Options are listed in the order they appear in the ValidOptions,
  // regardless of the order specified on the original command line.  If the
  // CaseSensitive flag is NOT set, all options are translated to lower case.
  // Options with values always appear using the "-o=..." notation.
  //--
  string sCommand;

  // First list all the options ...
  for (string::const_iterator it=m_sValidOptions.begin();  it!=m_sValidOptions.end();  ++it) {
    char op = *it;
    if (IsOptionPresent(op)) {
      if (!sCommand.empty()) sCommand.append(1, ' ');
      sCommand.append(1, m_sOptionPrefix[0]);  sCommand.append(1, op);
      string sValue = GetOptionValue(op);
      if (!sValue.empty()) sCommand += "=" + sValue;
    }
  }

  // Then list all the arguments ...
  for (argument_iterator it = arguments_begin();  it != arguments_end();  ++it) {
    if (!sCommand.empty()) sCommand += " ";
    sCommand += *it;
  }

  // All done!
  return sCommand;
}


