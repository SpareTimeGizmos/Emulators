; Script to boot the STG EPROM and ElfOS on the PicoElf emulator ...
SET LOGGING/CONSOLE/LEVEL=WARNING
CLEAR MEMORY
LOAD PicoElf-v120.hex/BASE=8000
ATTACH DISK/UNIT=0 ElfOSv5.dsk
RUN 8000
