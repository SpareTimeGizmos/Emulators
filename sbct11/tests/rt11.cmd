; This script will boot RT11 using the SBCT11 emulator.
SET LOG/FILE/LEVEL=WARNING
SET LOG/CONSOLE/LEVEL=WARNING
LOAD boots11.hex/FORMAT=INTEL/ROM
ATTACH TAPE/UNIT=0 rt11/READ
ATTACH TAPE/UNIT=1 work/WRITE
RUN
