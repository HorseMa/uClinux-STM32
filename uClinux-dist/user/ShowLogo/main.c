#include "lcd.h"
#include <stdio.h>

void BlitImage(int fd)
{
  u32 index = 0, size = 0;
  vu16 buffer;

  /* Read bitmap size */
  lseek(fd,2,SEEK_SET);
  read(fd,&size,4);

  /* Get bitmap data address offset */
  lseek(fd,10,SEEK_SET);
  read(fd,&index,4);

  size = (size - index)/2;

  lseek(fd,index,SEEK_SET);

  /* Set GRAM write direction and BGR = 1 */
  /* I/D=00 (Horizontal : decrement, Vertical : decrement) */
  /* AM=1 (address is updated in vertical writing direction) */
  LCD_WriteReg(R3, 0x1008);
 
  LCD_WriteRAM_Prepare();
 
  while(read(fd,&buffer,sizeof(buffer))>0)
  	LCD_WriteRAM(buffer);
 
  /* Set GRAM write direction and BGR = 1 */
  /* I/D = 01 (Horizontal : increment, Vertical : decrement) */
  /* AM = 1 (address is updated in vertical writing direction) */
  LCD_WriteReg(R3, 0x1018);
}

int main(int argc, char ** argv)
{
	int fd;
	if(!strcmp(argv[1],"clear"))
	{
		LCD_Clear(White);
		return 0;
	}
	fd = open(argv[1],0);
	if(fd<0)
	{
		printf("Enable to open file\n");
		return -1;
	}
	BlitImage(fd);
	close(fd);
	return 0;
}
