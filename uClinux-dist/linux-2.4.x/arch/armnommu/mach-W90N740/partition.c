#include <asm/arch/bootheader.h>


winbond_header *find_winbond_part(int image)
{
        int i,j;
        winbond_header *p;
        unsigned long long sum;
        unsigned long *p_sum;

        p = (winbond_header*) (0x7f000000 + 0x20000 - sizeof(winbond_header));
        for (i=0; i<32; i++,p=(unsigned long)p + 0x10000) {
                if (p->image != image)
                        continue;
                if (p->magic != 0xa0ffff9f)
                        continue;
                p_sum = (unsigned long*) p;
                j = sizeof(winbond_header)>>2;
                sum=0;
                while (j--)
                        sum += *p_sum++;
                sum += sum>>32;
                if ((unsigned long)sum != 0xffffffff)
                        continue;

                return p;
        }
        return 0;
}

