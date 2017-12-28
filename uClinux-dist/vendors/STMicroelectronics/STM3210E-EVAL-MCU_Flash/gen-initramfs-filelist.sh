#!/bin/sh
filelist=$ROOTDIR/$LINUXDIR/initramfs-filelist

rm -rf $filelist > /dev/null
for i in `ls -Rl $1 | grep ":$" | cut -d':' -f1 | cut -d'.' -f2`; 
do 
	echo "dir `echo $i |awk -F $ROMFSDIR '{print $2};'` 755 0 0 " >> $filelist
	for j in `ls -Q $i | cut -d'"' -f2`; 
	do 
		if [ -L $i/$j ] 
		then 
			h=`ls -l $i/$j | cut -f2 -d'>'`
			echo "slink `echo $i |awk -F $ROMFSDIR '{print $2};'`/$j `find $ROMFSDIR -name $h|awk -F $ROMFSDIR '{print $2};'` 777 0 0" >> $filelist;
		elif [ -f $i/$j ] 
		then
			echo "file `echo $i |awk -F $ROMFSDIR '{printf("/%s", $2)};'`/$j $i/$j 777 0 0" >> $filelist;
		fi; 
		
	done; 
done

awk 'BEGIN {i=0}; {if(i++!=0) printf("nod %s %d %d %d %c %d %d \n",$1 ,$3, $4 ,$5 ,$2, $6, $7);}' device.tab >> $filelist;