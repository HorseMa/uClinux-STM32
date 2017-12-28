/*********************************************************************************************
* a simple Led_Show application 							     *
**********************************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

unsigned char _quit=0,_pause=0;

void Stop_Show()
{
	printf("Kill signal received\n");
	_quit = !_quit;
}
void pause_resume_Show()
{
	printf("%s signal received\n",(!_pause)? "Pause":"Resume");
	_pause = !_pause;
}

int main(int argc, char ** argv)
{
	struct timespec req;
	
	int LED[4],Delay = 0,cn = 0;
	
	char *LED_gpio[4] = {
		"/dev/gpioF6",
		"/dev/gpioF7",
		"/dev/gpioF8",
		"/dev/gpioF9"
		};
	if(argc != 1)
		printf("%s.\n"\
		"\tthis will toggle the leds on the STM3210E-EVAL. \n"\
		"\tto stop or resume the demo, send the signal 14 \"kill -14 <pidofDemo>\"\n",argv[0],argv[0]);
	else
	{

		for(cn = 0; cn < 4; cn++)
			LED[cn] = open(LED_gpio[cn],1);
			
		if(LED[0]<0||LED[1]<0||LED[2]<0||LED[3]<0)
		{
			printf("can not open gpio port for writing.\n");
			return -2;
		}
		
		for(cn = 0; cn < 4; cn++)
			write(LED[cn],"O0",2);	
		
		signal(SIGALRM,&pause_resume_Show);
		signal(SIGKILL,&Stop_Show);
		req.tv_nsec = 500000000;
		while(!_quit)
		{
			for (cn=0;cn<4 && !_pause;cn++)
			{ 
				write(LED[cn],"T",1);
				nanosleep(&req,NULL);	
			}
		}
		
		for(cn = 0; cn < 4; cn++)
			close(LED[cn]);
	}
return 0;
}
