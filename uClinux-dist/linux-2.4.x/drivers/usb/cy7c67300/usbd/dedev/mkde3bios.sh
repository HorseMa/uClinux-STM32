# !/bin/sh

echo "Converting de3_bios.bin to de3_bios.h"

cy16-elf-as -ahlsn=de3_bios.lst -o de3_bios.o de3_bios.asm
cy16-elf-ld -e code_start -Ttext 0x4f0 -o de3_bios de3_bios.o
cy16-elf-objcopy -O binary de3_bios de3_bios.bin

../../file2struct de3_bios.bin de3_bios >de3_bios.h

rm -f ./de3_bios.o
rm -f ./de3_bios
rm -f ./de3_bios.bin

echo "Removing cy7c67200_300_hcd.o"

rm -f ../../cy7c67200_300_hcd.o
