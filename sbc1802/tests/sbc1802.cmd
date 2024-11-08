set logging/console/level=warning
set cpu/extended
;attach disk ElfOSv5.dsk/unit=0
clear memory
load eprom.hex/rom
load/ram/base=fe00 nvr.bin
;run
;save/ram/base=fe00/count=224/format=binary/overwrite nvr.bin
