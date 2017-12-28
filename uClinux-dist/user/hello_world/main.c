/*********************************************************************************************
* a simple hello world that present how easy the developpment of end user application become.*
**********************************************************************************************/

#include <stdio.h>

int main(int argc, char ** argv)
{
	if(argc != 1)
		printf("hello :\t%s.\n",argv[1]);
	else
		printf("hello world.\n");
return 0;
}
