typedef struct{
	unsigned long image;
	unsigned long start;
	unsigned long length;
	unsigned long adr1;
	unsigned long adr2;
	char 	 name[16];
	unsigned long sum_img;
	unsigned long magic;
	unsigned long flags;
	unsigned long sum_hdr;
} winbond_header;

extern winbond_header *find_winbond_part(int image);
