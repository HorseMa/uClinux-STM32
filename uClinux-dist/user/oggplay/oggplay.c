/****************************************************************************/

/*
 *	oggplay.c -- Play OGG VORBIS data files
 *
 *	(C) Copyright 2007-2008, Paul Dale (Paul_Dale@au.securecomputing.com)
 *	(C) Copyright 1999-2002, Greg Ungerer (gerg@snapgear.com)
 *      (C) Copyright 1997-1997, Stéphane TAVENARD
 *          All Rights Reserved
 *
 *	This code is a derivitive of Stephane Tavenard's mpegdev_demo.c.
 *
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 * 
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 * 
 *	You should have received a copy of the GNU General Public License
 *	along with this program; if not, write to the Free Software
 *	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/****************************************************************************/
#define _BSD_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/file.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <ivorbiscodec.h>
#include <ivorbisfile.h>
#include <linux/soundcard.h>
#include <sys/resource.h>
#include <config/autoconf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/ioctl.h>
#include <sys/wait.h>

#ifdef CONFIG_USER_SETKEY_SETKEY
#include <key/key.h>
#endif

/****************************************************************************/

static int	verbose;
static int	quiet;
static int	lcd_line, lcd_time;
static int	lcdfd = -1;
static int	gotsigusr1;
static int	printtime;
static int	onlytags;
static int	crypto_keylen = 0;
static char	*crypto_key = NULL;
static int	buffer_size = 8192;


/****************************************************************************/

/*
 *	OV data stream support.
 */
static const char	 *trk_filename;
static OggVorbis_File	  vf;
static int		  dspfd, dsphw;


/****************************************************************************/

/*
 *	Trivial signal handler, processing is done from the main loop.
 */

static void usr1_handler(int ignore)
{
	gotsigusr1 = 1;
}

/****************************************************************************/

/*
 *	Print out the name on a display device if present.
 */

static void lcdtitle(char **user_comments)
{
	const char	*name;
	int	ivp;
	struct  iovec iv[4];
	char	prebuf[10];
	char	*p;
	int	namelen;

	/* Install a signal handler to allow updates to be forced */
	signal(SIGUSR1, usr1_handler);

	/* Determine the name to display.  We use the tag if it is
	 * present and the basename of the file if not.
	 */
	if (user_comments != NULL && *user_comments != NULL) {
		name = *user_comments;
		if (strncasecmp(name, "title=", sizeof("title=")-1) == 0)
			name += sizeof("title=")-1;
		namelen = strlen(name);
	} else {
		name = strrchr(trk_filename, '/');
		if (name == NULL)
			name = trk_filename;
		else
			name++;
		p = strchr(name, '.');
		if (p == NULL)
			namelen = strlen(name);
		else
			namelen = p - name;
	}

	if (lcd_line) {
		/* Lock the file so we can access it... */
		if (flock(lcdfd, LOCK_SH | LOCK_NB) == -1)
			return;
		if (lcd_line == 0) {
			prebuf[0] = '\f';
			prebuf[1] = '\0';
		} else if (lcd_line == 1) {
			strcpy(prebuf, "\003\005");
		} else if (lcd_line == 2) {
			strcpy(prebuf, "\003\v\005");
		}

		/*
		 * Now we'll write the title out.  We'll do this atomically
		 * just in case two players decide to coexecute...
		 */
		ivp = 0;
		iv[ivp].iov_len = strlen(prebuf) * sizeof(char);
		iv[ivp++].iov_base = prebuf;
		
		iv[ivp].iov_len = namelen * sizeof(char);
		iv[ivp++].iov_base = (void *)name;
		
		//postbuf = '\n';
		//iv[ivp].iov_len = sizeof(char);
		//iv[ivp++].iov_base = &postbuf;
		writev(lcdfd, iv, ivp);

		/* Finally, unlock it since we've finished. */
		flock(lcdfd, LOCK_UN);
	}
}

/****************************************************************************/

/*
 *	Output time info to display device.
 */

static void lcdtime(time_t sttime)
{
	static time_t	lasttime;
	time_t		t;
	char		buf[15], *p;
	int		m, s;

	t = time(NULL) - sttime;
	if (t != lasttime && flock(lcdfd, LOCK_SH | LOCK_NB) == 0) {
		p = buf;
		*p++ = '\003';
		if (lcd_time == 2)
			*p++ = '\v';
		*p++ = '\005';
		m = t / 60;
		s = t % 60;
		if (s < 0) s += 60;
		sprintf(p, "%02d:%02d", m, s);
		write(lcdfd, buf, strlen(buf));
		flock(lcdfd, LOCK_UN);
	}
	lasttime = t;
}

/****************************************************************************/

/*
 *	Configure DSP engine settings for playing this track.
 */
 
static void setdsp(int fd)
{
	int v;

	v = 44100;
	if (ioctl(fd, SNDCTL_DSP_SPEED, &v) < 0) {
		fprintf(stderr, "ioctl(SNDCTL_DSP_SPEED)->%d\n", errno);
		exit(1);
	}

	v = 1;
	if (ioctl(fd, SNDCTL_DSP_STEREO, &v) < 0) {
		fprintf(stderr, "ioctl(SNDCTL_DSP_STEREO)->%d\n", errno);
		exit(1);
	}

#if BYTE_ORDER == LITTLE_ENDIAN
	v = AFMT_S16_LE;
#else
	v = AFMT_S16_BE;
#endif
	if (ioctl(fd, SNDCTL_DSP_SAMPLESIZE, &v) < 0) {
		fprintf(stderr, "ioctl(SNDCTL_DSP_SAMPLESIZE)->%d\n", errno);
		exit(1);
	}
}


/****************************************************************************/

static void usage(int rc)
{
	printf("usage: oggplay [-hviqzRPZ] [-s <time>] "
		"[-d <device>] [-w <filename>] [-c <key>]"
		"[-l <line> [-t]] [-p <pause>]ogg-files...\n\n"
		"\t\t-h            this help\n"
		"\t\t-v            verbose stdout output\n"
		"\t\t-b <size>     set the input file buffer size\n"
		"\t\t-i            display file tags and exit\n"
		"\t\t-q            quiet (don't print title)\n"
		"\t\t-R            repeat tracks forever\n"
		"\t\t-z            shuffle tracks\n"
		"\t\t-Z            psuedo-random tracks (implicit -R)\n"
		"\t\t-P            print time to decode/play\n"
		"\t\t-s <time>     sleep between playing tracks\n"
		"\t\t-d <device>   audio device for playback\n"
		"\t\t-D            configure audio device as per a DSP device\n"
		"\t\t-l <line>     display title on LCD line (0,1,2) (0 = no title)\n"
		"\t\t-t <line>     display time on LCD line (1,2)\n"
		"\t\t-c <key>      decrypt using key\n"
		"\t\t-0 <bytes>    emit <bytes> zero bytes after playing a track\n"
		);
	exit(rc);
}

/****************************************************************************/
/* define custom OV decode call backs so that we can handle encrypted content.
 */
#include <sys/stat.h>
#include <sys/mman.h>

static long filelen;
static int fd;
static char *fmem;
static long fps;
static int aggressive = 0;

#if 0
static int fread_wrap(unsigned char *ptr, size_t sz, size_t n, FILE *f) {
	if (f == NULL)
		return -1;
	const size_t r = fread(ptr, sz, n, f);
	if (crypto_keylen > 0 && r > 0) {
		long pos = (ftell(f) - r) % crypto_keylen;
		int i;

		for (i=0; i<r; i++) {
			ptr[i] ^= crypto_key[pos++];
			if (pos >= crypto_keylen)
				pos = 0;
		}
	}
	return r;
}

static int fseek_wrap(FILE *f, ogg_int64_t off, int whence) {
	if (f == NULL)
		return -1;
	return fseek(f, (int)off, whence);
}
#endif
static int fread_wrap(unsigned char *ptr, size_t sz, size_t n, FILE *f) {
	int bytes = sz * n;

	if (fps + bytes >= filelen) {
		n = (filelen - fps) / sz;
		bytes = sz * n;
	}
	if (n > 0) {
		memcpy(ptr, fmem + fps, bytes);
		if (crypto_keylen > 0) {
			long pos = fps % crypto_keylen;
			int i;

			for (i=0; i<bytes; i++) {
				ptr[i] ^= crypto_key[pos++];
				if (pos >= crypto_keylen)
					pos = 0;
			}
		}
		fps += bytes;
	}
	return n;
}

static int fseek_wrap(FILE *f, ogg_int64_t off, int whence) {
	switch(whence) {
	case SEEK_SET:
		fps = off;
		if (fps == 0) {
			aggressive = 1;
			madvise(fmem, filelen, MADV_SEQUENTIAL);
		}
		break;
	case SEEK_CUR:
		fps += off;
		break;
	case SEEK_END:
		fps = filelen + off;
		break;
	default:
		errno = EINVAL;
		return -1;
	}
	if (fps < 0)
		fps = 0;
	else if (fps > filelen)
		fps = filelen;
	return 0;
}

static long ftell_wrap(FILE *f) {
	return fps;
}
static int fclose_wrap(FILE *f) {
	munmap(fmem, filelen);
	return close(fd);
}

static FILE *fopen_wrap(const char *fname, const char *mode) {
	struct stat st;

	fd = open(fname, O_RDONLY);
	if (fd == -1)
		return NULL;
	fstat(fd, &st);
	filelen = st.st_size;
	fmem = mmap(NULL, filelen, PROT_READ, MAP_SHARED, fd, 0);
	fps = 0;
	return (FILE *) fmem;
}

static ov_callbacks ovcb = {
	(size_t (*)(void *, size_t, size_t, void *))	&fread_wrap,
	(int (*)(void *, ogg_int64_t, int))		&fseek_wrap,
	(int (*)(void *))				&fclose_wrap,
	(long (*)(void *))				&ftell_wrap
};

/****************************************************************************/

static int play_one(const char *file) {
	char		pcmout[65536];
	int		current_section;
	time_t		sttime;
	unsigned long	us;
	struct timeval	tvstart, tvend;
	char		**user_comments = NULL;
	FILE		 *trk_fd;

	trk_filename = file;

	trk_fd = fopen_wrap(trk_filename, "r");

	if (trk_fd == NULL) {
		fprintf(stderr, "ERROR: Unable to open '%s', errno=%d\n",
			trk_filename, errno);
		return 1;
	}
	//setvbuf(trk_fd, NULL, _IOFBF, buffer_size);
retry:	aggressive = 0;
	if (ov_open_callbacks(trk_fd, &vf, NULL, 0, ovcb) < 0) {
		if (crypto_keylen > 0) {
			crypto_keylen = 0;
			fseek_wrap(trk_fd, 0L, SEEK_SET);
			goto retry;
		}
		fclose_wrap(trk_fd);
		fprintf(stderr, "ERROR: Unable to ov_open '%s', errno=%d\n",
			trk_filename, errno);
		return 1;
	}
	user_comments = ov_comment(&vf, -1)->user_comments;

	if (onlytags) {
		char **ptr = user_comments;
		while (ptr && *ptr) {
			puts(*ptr);
			ptr++;
		}
		return 0;
	}

	if (dsphw)
		setdsp(dspfd);
	if (lcd_line)
		lcdtitle(user_comments);

	gettimeofday(&tvstart, NULL);
	sttime = time(NULL);

	/* We are all set, decode the file and play it */
	for (;;) {
		const long ret = ov_read(&vf, pcmout, sizeof(pcmout), &current_section);
		if (ret == 0)
			break;
		else if (ret < 0) {
			ov_clear(&vf);
			return 1;
		}
		write(dspfd, pcmout, ret);
		if (gotsigusr1) {
			gotsigusr1 = 0;
			if (lcd_line)
				lcdtitle(user_comments);
		}
		if (lcd_time)
			lcdtime(sttime);
	}
	ov_clear(&vf);

	if (printtime) {
		gettimeofday(&tvend, NULL);
		us = ((tvend.tv_sec - tvstart.tv_sec) * 1000000) +
		    (tvend.tv_usec - tvstart.tv_usec);
		printf("Total time = %d.%06d seconds\n",
			(us / 1000000), (us % 1000000));
	}
	return 0;
}

static void paddy(int fd, int len) {
	char buf[2000];
	int n;

	bzero(buf, sizeof(buf));

	while (len > 0) {
		n = sizeof(buf);
		if (len < n)
			n = len;
		n = write(fd, buf, n);
		if (n == 0)
			break;
		if (n == -1) {
			if (errno != EINTR)
				break;
		} else
			len -= n;
	}
}

int main(int argc, char *argv[])
{
	int		c, i, j, slptime;
	int		repeat;
 	int		argnr, startargnr, rand, shuffle;
	char		*device, *argvtmp;
	pid_t		pid;
	int		zerobytes;

	signal(SIGUSR1, SIG_IGN);

	verbose = 0;
	quiet = 0;
	shuffle = 0;
	rand = 0;
	repeat = 0;
	printtime = 0;
	slptime = 0;
	device = "/dev/dsp";
	dsphw = 0;
	onlytags = 0;
	zerobytes = 0;

	while ((c = getopt(argc, argv, "?himvqzt:RZPs:d:Dl:Vc:b:0:")) >= 0) {
		switch (c) {
		case 'b':
			buffer_size = atoi(optarg);
			if (buffer_size < 1)
				buffer_size = 1;
			break;
		case 'V':
			printf("%s version 1.0\n", argv[0]);
			return 0;
		case 'v':
			verbose++;
			break;
		case 'q':
			verbose = 0;
			quiet++;
			break;
		case 'R':
			repeat++;
			break;
		case 'z':
			shuffle++;
			break;
		case 'Z':
			rand++;
			repeat++;
			break;
		case 'P':
			printtime++;
			break;
		case 's':
			slptime = atoi(optarg);
			break;
		case 'd':
			device = optarg;
			break;
		case 'D':
			dsphw = 1;
			break;
		case 'l':
			lcd_line = atoi(optarg);
			break;
		case 't':
			lcd_time = atoi(optarg);
			break;
		case 'i':
			onlytags = 1;
			break;
		case 'c':
			crypto_key = strdup(optarg);
			crypto_keylen = strlen(crypto_key);
			{	char *p = optarg;
				while (*p != '\0')
					*p++ = '\0';
			}
			break;
		case '0':
			zerobytes = (atoi(optarg) + 1) & ~1;
			if (zerobytes < 0)
				zerobytes = 0;
			break;
		case 'h':
		case '?':
			usage(0);
			break;
		}
	}

	argnr = optind;
	if (argnr >= argc)
		usage(1);
	startargnr = argnr;

#ifdef CONFIG_USER_SETKEY_SETKEY
	/* If we've got the crypto key driver installed and the user hasn't
	 * specified a crypto key already, we load it from the driver.
	 */
	if (crypto_key == NULL) {
		static unsigned char key[128];

		if ((i = getdriverkey(key, sizeof(key))) > 0) {
			crypto_key = (char *) key;
			crypto_keylen = i;
		}
	}
#endif

	/* Make ourselves the top priority process! */
	setpriority(PRIO_PROCESS, 0, -10);
	srandom(time(NULL) ^ getpid());

	/* Open the audio playback device */
	if ((dspfd = open(device, (O_WRONLY | O_CREAT | O_TRUNC), 0660)) < 0) {
		fprintf(stderr, "ERROR: Can't open output device '%s', "
			"errno=%d\n", device, errno);
		exit(0);
	}

	/* Open LCD device if we are going to use it */
	if ((lcd_line > 0) || (lcd_time > 0)) {
		lcdfd = open("/dev/lcdtxt", O_WRONLY);
		if (lcdfd < 0) {
			lcd_time = 0;
			lcd_line = 0;
		}
	}

nextall:
	/* Shuffle tracks if slected */
	if (shuffle) {
		for (c = 0; (c < 10000); c++) {
			i = (((unsigned int) random()) % (argc - startargnr)) +
				startargnr;
			j = (((unsigned int) random()) % (argc - startargnr)) +
				startargnr;
			argvtmp = argv[i];
			argv[i] = argv[j];
			argv[j] = argvtmp;
		}
	}

nextfile:
	if (rand) {
		argnr = (((unsigned int) random()) % (argc - startargnr)) +
			startargnr;
	}

	pid = fork();
	if (pid == 0) {
		exit(play_one(argv[argnr]));
	} else if (pid > 0) {
		int status;

		for (;;) {
			if (waitpid(pid, &status, 0) == -1)
				if (errno != EINTR)
					break;
		}
	}

	if (slptime)
		sleep(slptime);

	if (++argnr < argc)
		goto nextfile;

	if (repeat) {
		argnr = startargnr;
		goto nextall;
	}

	paddy(dspfd, zerobytes);
	close(dspfd);
	if (lcdfd >= 0)
		close(lcdfd);
	return 0;
}

/****************************************************************************/
