# !/bin/sh

echo "Converting de1_bios.bin to de1_bios.h"

cy16-elf-as -ahlsn=de1_bios.lst -o de1_bios.o de1_bios.asm
cy16-elf-ld -e code_start -Ttext 0x4f0 -o de1_bios de1_bios.o
cy16-elf-objcopy -O binary de1_bios de1_bios.bin

../../file2struct de1_bios.bin de1_bios >de1_bios.h

rm -f ./de1_bios.o
rm -f ./de1_bios
rm -f ./de1_bios.bin

echo "Removing cy7c67200_300_hcd.o"

rm -f ../../cy7c67200_300_hcd.o
