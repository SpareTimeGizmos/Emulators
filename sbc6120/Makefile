#++
# Makefile - Makefile for the SBC6120 simulator...
#
#   Copyright (C) 1999-2022 by Spare Time Gizmos.  All rights reserved.
#
# LICENSE:
#    This file is part of the SBC6120 emulator project. SBC6120 is free
# software; you may redistribute it and/or modify it under the terms of
# the GNU Affero General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any
# later version.
#
#    SBC6120 is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Affero General Public License
# for more details.  You should have received a copy of the GNU Affero General
# Public License along with SBC6120.  If not, see http://www.gnu.org/licenses/.
#
# DESCRIPTION:
#   This is a fairly simple Makefile for building the Spare Time Gizmos SBC6120
# simulator on Debian Linux and using GNU Make and gcc (or rather, g++).
#
#
#                                   Bob Armstrong [27-AUG-22]
#
#TARGETS:
#  make SBC6120	- rebuild the SBC6120 simulator
#  make clean	- delete all generated files 
#
# REVISION HISTORY:
# dd-mmm-yy	who     description
# 27-AUG-22	RLA	New file.
#  4-MAR-24	RLA	Remove GENERIC and move everything to EMULIB.
#--

# Compiler preprocessor DEFINEs for the entire project ...
DEFINES = _DEBUG
EMULIB  = ../emulib


# Define the target (library) and source files required ...
CPPSRCS   = SBC6120.cpp HD6120.cpp MemoryMap.cpp HD6120opcodes.cpp \
	    RAMdisk.cpp SLU.cpp IDEdisk.cpp  MiscellaneousIOTs.cpp \
            POST.cpp  UserInterface.cpp \
            $(EMULIB)/LogFile.cpp $(EMULIB)/CommandParser.cpp \
	    $(EMULIB)/CommandLine.cpp $(EMULIB)/StandardUI.cpp \
	    $(EMULIB)/EMULIB.cpp $(EMULIB)/LinuxConsole.cpp \
            $(EMULIB)/ImageFile.cpp $(EMULIB)/SmartConsole.cpp \
	    $(EMULIB)/EventQueue.cpp $(EMULIB)/Interrupt.cpp \
	    $(EMULIB)/Memory.cpp $(EMULIB)/CPU.cpp $(EMULIB)/IDE.cpp \
	    $(EMULIB)/DeviceMap.cpp $(EMULIB)/Device.cpp \
	    $(EMULIB)/UART.cpp $(EMULIB)/DECfile8.cpp
CSRCS	  = $(EMULIB)/SafeCRT.c
INCLUDES  = $(EMULIB)/
LIBRARIES = -lstdc++ -lm -ldl
OBJDIR    = bin
OBJECTS   = $(addprefix $(OBJDIR)/, $(notdir $(CPPSRCS:.cpp=.o) $(CSRCS:.c=.o)))
TARGET    = $(OBJDIR)/sbc6120


# Define the standard tool paths and options.
#   Note that the CPPFLAGS are for C++, CCFLAGS are for C programs, and
# the CFLAGS variable (only one "C"!) are common to both.  Also, I'm not
# really clear what the difference is, if any, between the "g++" command
# and "gcc -x c++".  I'm assuming they're the same!
CC       = /usr/bin/gcc
CPP      = $(CC) -x c++
LD       = $(CC)
CPPFLAGS = -std=c++0x
CCFLAGS  = -std=c11 
CFLAGS   = -ggdb3 -O3 -pthread -Wall -Wno-deprecated-declarations \
            -funsigned-char -funsigned-bitfields -fshort-enums \
	    -DWORD_SIZE=12 -DADDRESS_SIZE=15 \
	    $(foreach inc,$(INCLUDES),-I$(inc)) \
	    $(foreach def,$(DEFINES),-D$(def))
LDFLAGS  = -pthread


# Rule to rebuild the executable ...
all:		$(TARGET)


$(TARGET):	$(OBJECTS)
	@echo Linking $(TARGET)
	@$(LD) $(LDFLAGS) -o $(TARGET) $(OBJECTS) $(LIBRARIES)


# Rules to compile C and C++ files ...
$(OBJDIR)/%.o: %.cpp
	@echo Compiling $<
	@$(CPP) -c -o $@ $(CPPFLAGS) $(CFLAGS) $<

$(OBJDIR)/%.o: $(EMULIB)/%.cpp
	@echo Compiling $<
	@$(CPP) -c -o $@ $(CPPFLAGS) $(CFLAGS) $<

$(OBJDIR)/%.o: %.c
	@echo Compiling $<
	@$(CC) -c -o $@ $(CCFLAGS) $(CFLAGS) $<

$(OBJDIR)/%.o: $(EMULIB)/%.c
	@echo Compiling $<
	@$(CC) -c -o $@ $(CCFLAGS) $(CFLAGS) $<


# A rule to clean up ...
clean:
	rm -f $(TARGET) $(OBJECTS) *~ *.core core Makefile.dep


# And a rule to rebuild the dependencies ...
Makefile.dep: $(CSRCS) $(CPPSRCS)
	@echo Building dependencies
	@$(CPP) -M $(CPPFLAGS) $(CFLAGS) $(CPPSRCS) >>Makefile.dep
##	@$(CC)  -M $(CCFLAGS) $(CFLAGS) $(CSRCS) >Makefile.dep

include Makefile.dep
