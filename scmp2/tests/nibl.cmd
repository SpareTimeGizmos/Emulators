;set log/console/level=trace
set memory 0-0fff/rom
set memory 1000-ffff/ram
attach serial senseb flag0
set serial/baud=110/invert=rx
load nibl
show configuration
show memory
clear cpu
;run 0
