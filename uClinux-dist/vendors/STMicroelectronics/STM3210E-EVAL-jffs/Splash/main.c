#define LCD_WriteBMP	*(unsigned long*)0x2000ff00 
#define BMP_Image	(unsigned long)0x64160000

int main(int argc, char ** argv)
{
	typedef void (*pShow)(unsigned long) ;
	pShow ShowLogo;
	ShowLogo=(pShow)(LCD_WriteBMP | 0x01); // ( @ | 0x01 ) thumb mode.
	ShowLogo(BMP_Image);
	return 0;
}
