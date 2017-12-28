/*********************************************************************************************
* a simple hello world that present how easy the developpment of end user application become.*
**********************************************************************************************/

#include <stdio.h>
#include <unistd.h>

int main(int argc, char ** argv)
{
	int fd,delay=0,nr=1;
	if(argc != 4)
		printf("%s :\t[/dev/gpioF<x>] [delay <ms>] [repeatition <nr>].\n"\
		"\texample: %s /dev/gpioF6 1000 5 \n"\
		"\tthis will toggle the gpio 5 times with a period of 1 second.\n",argv[0],argv[0]);
	else
	{
		if(!isdigit(*argv[2]) || !isdigit(*argv[3]))
		{
			printf("parameters error!\n");
			return -1;
		}
		delay=atoi(argv[2])*1000;
		nr=(atoi(argv[3]));
		fd=open(argv[1],1);
		if(fd<0)
		{
			printf("can not open %s for writing.\n",argv[1]);
			return -2;
		}
		while(nr--!=0)
		{
			write(fd,"T",1);
			usleep(delay);
			write(fd,"T",1);
			usleep(delay);	
		}
		close(fd);
	}
return 0;
}
