#++
# Makefile - Makefile for the SCMP2 simulator...
#
#DESCRIPTION:
#   This is a fairly simple Makefile for building the Spare Time Gizmos
# SC/MP-II simulator on Debian Linux and using GNU Make, gcc and g++.
#
#
#                                   Bob Armstrong [20-AUG-22]
#
#TARGETS:
#  make SCMP	- rebuild the SCMP simulator
#  make clean	- delete all generated files 
#
# REVISION HISTORY:
# dd-mmm-yy	who     description
# 26-AUG-22	RLA	New file.
#  8-NOV-25	RLA	Change to SCMP2 (now that there's an SCMP3!)
#--

# Compiler preprocessor DEFINEs for the entire project ...
DEFINES = _DEBUG
EMULIB  = ../EMULIB
GENERIC = ../GENERIC


# Define the target (library) and source files required ...
CPPSRCS   = SCMP2.cpp INS8060.cpp INS8060opcodes.cpp UserInterface.cpp \
            $(EMULIB)/LogFile.cpp $(EMULIB)/CommandParser.cpp \
	    $(EMULIB)/CommandLine.cpp $(EMULIB)/EventFlag.cpp \
	    $(EMULIB)/MessageQueue.cpp $(EMULIB)/Mutex.cpp \
	    $(EMULIB)/StandardUI.cpp $(EMULIB)/Thread.cpp \
	    $(EMULIB)/CheckpointFiles.cpp $(EMULIB)/EMULIB.cpp \
	    $(GENERIC)/EventQueue.cpp $(GENERIC)/Interrupt.cpp \
	    $(GENERIC)/Memory.cpp $(GENERIC)/CPU.cpp \
	    $(GENERIC)/DeviceMap.cpp $(GENERIC)/Device.cpp \
	    $(GENERIC)/SoftwareSerial.cpp $(EMULIB)/LinuxConsole.cpp
CSRCS	  = $(EMULIB)/SafeCRT.c
INCLUDES  = $(EMULIB)/ $(GENERIC)/
LIBRARIES = -lstdc++ -lm -ldl
OBJDIR    = bin
OBJECTS   = $(addprefix $(OBJDIR)/, $(notdir $(CPPSRCS:.cpp=.o) $(CSRCS:.c=.o)))
TARGET    = $(OBJDIR)/scmp2


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

$(OBJDIR)/%.o: $(GENERIC)/%.cpp
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
