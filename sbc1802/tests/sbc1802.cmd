set logging/console/level=warning
set cpu/extended
attach disk sbc1802.dsk/unit=0/capacity=128128
clear memory
load eprom.hex/rom
load/ram/base=fe00 data.bin
;run
;save/ram/base=fe00/count=224/format=binary/overwrite data.bin
