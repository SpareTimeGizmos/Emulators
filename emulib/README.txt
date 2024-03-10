EMULATION MODELS

Microprocessors
---------------
CDP1802	- COSMAC 8 bit microprocessor
IM6100	- DEC PDP-8 12 bit microprocessor
HD6120	- DEC PDP-8 12 bit microprocessor
S2650	- Signetics 2650 8 bit microprocessor
i8085	- Intel 8 bit microprocessor (improved 8080)
INS8060	- Nat Semi SC/MP "Simple Cost-effective Micro Processor"
DCT11	- DEC PDP-11 Microprocessor

UARTs
-----
UART    - generic UART (basis for other UART classes!)
CDP1854	- RCA COSMAC family UART
DC319	- DEC PDP-11 KL11 compatible serial interface
INS8250	- Nat Semi UART (same as used in the PC!)
S2651	- Signetics PCI Programmable Communications Interface
SoftwareSerial	- Bit banged software serial emulation

PPIs
----
PPI	- generic PPI (basis for other PPI classes!)
i8255	- classic Intel PPI (unfinished, based on PPI class)
i8155	- Intel 8155/6 RAM-IO-TIMER (finished?  untested)
INS8154 - National SC/MP family PPI (unfinished)
NSC810  - National NSC800 family PPI (unfinished)
CDP1851 - RCA COSMAC family PPI (unfinished)
i8255-Original - original implementation (finished, NOT based on PPI class)

Other Peripherals
-----------------
CDP1877	- COSMAC Programmable Interrupt Controller
CDP1879	- Real Time Clock
DS12887	- Real Time Clock
IDE	- Generic IDE/ATA drive
uPD765	- NEC uPD765 floppy disk controller 
UART	- Generic UART base class
RTC	- Generic Real Time Clock base class
TU58	- DEC RSP serial disk/tape drive
