SET LOG/CONSOLE/LEVEL=WARN

;   The INS8073 with its internal ROM and RAM has a fairly complicated memory
; configuration.  Note that all memory internal to the INS8073 is one cycle
; faster than external memory, hence the "FAST" and "SLOW" designations here.
SET MEMORY 00000-009FF/ROM/FAST
SET MEMORY 00A00-00FFF/NORAM/NOROM
SET MEMORY 01000-02FFF/RAM/SLOW
SET MEMORY 03000-0FFFF/NORAM/NOROM
SET MEMORY 0FD00-0FD00/ROM/SLOW
SET MEMORY 0FFC0-0FFFF/RAM/FAST

; INS8073 BASIC uses location $FD00 to set the baud rate!
;	$F9 - 4800
;	$FB - 1200
;	$FD -  300
;	$FF -  110
DEPOSIT 0FD00 0F9

; Set the software (bit banged) serial port configuration ...
ATTACH SERIAL SENSEA FLAG1
SET SERIAL/BAUD=4800/INVERT=RX

; Set the CPU to stop on illegal opcodes, and the crystal frequency is 4MHz ...
SET CPU/OPCODE=STOP/CLOCK=4000000
CLEAR CPU

; Load the INS8073 internal ROM image of NIBL2 BASIC ...
LOAD BASIC3

; All done!
SHOW CONFIGURATION
SHOW MEMORY
;RUN
