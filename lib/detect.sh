#!/bin/bash


echo "split cache colors by Cache Level: "
read cachelvl

sets=`cat /sys/devices/system/cpu/cpu0/cache/index${cachelvl}/number_of_sets`
CACHE_OVERLAP=$(($sets * 64))
PAGE_COLORS=$(($CACHE_OVERLAP / 4096))

echo CACHE_OVERLAP=$CACHE_OVERLAP
echo PAGE_COLORS=$PAGE_COLORS


sed -i 's/\(^#define CACHE_OVERLAP\) .*$/\1 '$CACHE_OVERLAP'/g' mm_color.c
sed -i 's/\(^#define PAGE_COLORS\) .*$/\1 '$PAGE_COLORS'/g' mm_color.c
#sed -i 's/\(^#define MEMORY_SIZE\) .*$/\1 '$MEMORY_SIZE'/g' mm_color.c
