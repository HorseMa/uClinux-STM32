/*
 *  Copyright (C) 2002 - 2006 Tomasz Kojm <tkojm@clamav.net>
 *  HTTP/1.1 compliance by Arkadiusz Miskiewicz <misiek@pld.org.pl>
 *  Proxy support by Nigel Horne <njh@bandsman.co.uk>
 *  Proxy authorization support by Gernot Tenchio <g.tenchio@telco-tech.de>
 *		     (uses fmt_base64() from libowfat (http://www.fefe.de))
 *  CDIFF code (C) 2006 Sensory Networks, Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301, USA.
 */
 
#ifdef	_MSC_VER
#include <winsock.h>	/* only needed in CL_EXPERIMENTAL */
#endif

#if HAVE_CONFIG_H
#include "clamav-config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#ifdef	HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>
#include <ctype.h>
#ifndef	C_WINDOWS
#include <netinet/in.h>
#include <netdb.h>
#endif
#include <sys/types.h>
#ifdef FILTER_RULES
#include <sys/wait.h>
#endif
#ifndef	C_WINDOWS
#include <sys/socket.h>
#include <sys/time.h>
#endif
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <zlib.h>

#include "manager.h"
#include "notify.h"
#include "dns.h"
#include "execute.h"
#include "nonblock.h"
#include "mirman.h"

#include "shared/options.h"
#include "shared/cfgparser.h"
#include "shared/output.h"
#include "shared/misc.h"
#include "shared/cdiff.h"
#include "shared/tar.h"

#include "libclamav/clamav.h"
#include "libclamav/others.h"
#include "libclamav/str.h"
#include "libclamav/cvd.h"

#ifndef	O_BINARY
#define	O_BINARY	0
#endif

#ifndef	C_WINDOWS
#define	closesocket(s)	close(s)
#endif

static int getclientsock(const char *localip)
{
	int socketfd = -1;

#ifdef PF_INET
    socketfd = socket(PF_INET, SOCK_STREAM, 0);
#else
    socketfd = socket(AF_INET, SOCK_STREAM, 0);
#endif

    if(socketfd < 0) {
	logg("!Can't create new socket\n");
	return -1;
    }

    if(localip) {
	struct hostent *he;

	if((he = gethostbyname(localip)) == NULL) {
	    const char *herr;
	    switch(h_errno) {
	        case HOST_NOT_FOUND:
		    herr = "Host not found";
		    break;

		case NO_DATA:
		    herr = "No IP address";
		    break;

		case NO_RECOVERY:
		    herr = "Unrecoverable DNS error";
		    break;

		case TRY_AGAIN:
		    herr = "Temporary DNS error";
		    break;

		default:
		    herr = "Unknown error";
		    break;
	    }
	    logg("!Could not resolve local ip address '%s': %s\n", localip, herr);
	    logg("^Using standard local ip address and port for fetching.\n");
	} else {
	    struct sockaddr_in client;
	    unsigned char *ia;
	    char ipaddr[16];

	    memset ((char *) &client, 0, sizeof(struct sockaddr_in));
	    client.sin_family = AF_INET;
	    client.sin_addr = *(struct in_addr *) he->h_addr_list[0];
	    if (bind(socketfd, (struct sockaddr *) &client, sizeof(struct sockaddr_in)) != 0) {
		logg("!Could not bind to local ip address '%s': %s\n", localip, strerror(errno));
		logg("^Using default client ip.\n");
	    } else {
		ia = (unsigned char *) he->h_addr_list[0];
		sprintf(ipaddr, "%u.%u.%u.%u", ia[0], ia[1], ia[2], ia[3]);
		logg("*Using ip '%s' for fetching.\n", ipaddr);
	    }
	}
    }

    return socketfd;
}

static int wwwconnect(const char *server, const char *proxy, int pport, char *ip, const char *localip, int ctimeout, struct mirdat *mdat, int logerr)
{
	int socketfd = -1, port, i, ret;
	struct sockaddr_in name;
	struct hostent *host;
	char ipaddr[16];
	unsigned char *ia;
	const char *hostpt;

    if(ip)
	strcpy(ip, "???");

    socketfd = getclientsock(localip);
    if(socketfd < 0)
	return -1;

    name.sin_family = AF_INET;

    if(proxy) {
	hostpt = proxy;

	if(!(port = pport)) {
#ifndef C_CYGWIN
		const struct servent *webcache = getservbyname("webcache", "TCP");

		if(webcache)
			port = ntohs(webcache->s_port);
		else
			port = 8080;

#ifndef	C_WINDOWS
		endservent();
#endif
#else
		port = 8080;
#endif
	}

    } else {
	hostpt = server;
	port = 80;
    }

    if((host = gethostbyname(hostpt)) == NULL) {
	const char *herr;
	switch(h_errno) {
	    case HOST_NOT_FOUND:
		herr = "Host not found";
		break;

	    case NO_DATA:
		herr = "No IP address";
		break;

	    case NO_RECOVERY:
		herr = "Unrecoverable DNS error";
		break;

	    case TRY_AGAIN:
		herr = "Temporary DNS error";
		break;

	    default:
		herr = "Unknown error";
		break;
	}
        logg("%cCan't get information about %s: %s\n", logerr ? '!' : '^', hostpt, herr);
	close(socketfd);
	return -1;
    }

    for(i = 0; host->h_addr_list[i] != 0; i++) {
	/* this dirty hack comes from pink - Nosuid TCP/IP ping 1.6 */
	ia = (unsigned char *) host->h_addr_list[i];
	sprintf(ipaddr, "%u.%u.%u.%u", ia[0], ia[1], ia[2], ia[3]);

	if((ret = mirman_check(((struct in_addr *) ia)->s_addr, mdat))) {
	    if(ret == 1)
		logg("Ignoring mirror %s (due to previous errors)\n", ipaddr);
	    else
		logg("Ignoring mirror %s (has connected too many times with an outdated version)\n", ipaddr);
	    continue;
	}

	if(ip)
	    strcpy(ip, ipaddr);

	if(i > 0)
	    logg("Trying host %s (%s)...\n", hostpt, ipaddr);

	name.sin_addr = *((struct in_addr *) host->h_addr_list[i]);
	name.sin_port = htons(port);

#ifdef SO_ERROR
	if(wait_connect(socketfd, (struct sockaddr *) &name, sizeof(struct sockaddr_in), ctimeout) == -1) {
#else
	if(connect(socketfd, (struct sockaddr *) &name, sizeof(struct sockaddr_in)) == -1) {
#endif
	    logg("Can't connect to port %d of host %s (IP: %s)\n", port, hostpt, ipaddr);
	    close(socketfd);
	    if((socketfd = getclientsock(localip)) == -1)
		return -1;

	    continue;
	} else {
	    mdat->currip = ((struct in_addr *) ia)->s_addr;
	    return socketfd;
	}
    }

    close(socketfd);
    return -2;
}

static int Rfc2822DateTime(char *buf, time_t mtime)
{
	struct tm *gmt;

    gmt = gmtime(&mtime);
    return strftime(buf, 36, "%a, %d %b %Y %X GMT", gmt);
}

static unsigned int fmt_base64(char *dest, const char *src, unsigned int len)
{
	unsigned short bits = 0,temp = 0;
	unsigned long written = 0;
	unsigned int i;
	const char base64[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";


    for(i = 0; i < len; i++) {
	temp <<= 8;
	temp += src[i];
	bits += 8;
	while(bits > 6) {
	    dest[written] = base64[((temp >> (bits - 6)) & 63)];
	    written++;
	    bits -= 6;
	}
    }

    if(bits) {
	temp <<= (6 - bits);
	dest[written] = base64[temp & 63];
	written++;
    }

    while(written & 3) {
	dest[written] = '=';
	written++;
    }

    return written;
}

static char *proxyauth(const char *user, const char *pass)
{
	int len;
	char *buf, *userpass, *auth;


    userpass = malloc(strlen(user) + strlen(pass) + 2);
    if(!userpass) {
	logg("!proxyauth: Can't allocate memory for 'userpass'\n");
	return NULL;
    }
    sprintf(userpass, "%s:%s", user, pass);

    buf = malloc((strlen(pass) + strlen(user)) * 2 + 4);
    if(!buf) {
	logg("!proxyauth: Can't allocate memory for 'buf'\n");
	free(userpass);
	return NULL;
    }

    len = fmt_base64(buf, userpass, strlen(userpass));
    free(userpass);
    buf[len] = '\0';
    auth = malloc(strlen(buf) + 30);
    if(!auth) {
	logg("!proxyauth: Can't allocate memory for 'authorization'\n");
	return NULL;
    }

    sprintf(auth, "Proxy-Authorization: Basic %s\r\n", buf);
    free(buf);

    return auth;
}

static struct cl_cvd *remote_cvdhead(const char *file, const char *hostname, char *ip, const char *localip, const char *proxy, int port, const char *user, const char *pass, const char *uas, int *ims, int ctimeout, int rtimeout, struct mirdat *mdat, int logerr)
{
	char cmd[512], head[513], buffer[FILEBUFF], ipaddr[16], *ch, *tmp;
	int bread, cnt, sd;
	unsigned int i, j;
	char *remotename = NULL, *authorization = NULL;
	const char *agent;
	struct cl_cvd *cvd;
	char last_modified[36];
	struct stat sb;


    if(proxy) {
        remotename = malloc(strlen(hostname) + 8);
	if(!remotename) {
	    logg("!remote_cvdhead: Can't allocate memory for 'remotename'\n");
	    return NULL;
	}
        sprintf(remotename, "http://%s", hostname);

	if(user) {
	    authorization = proxyauth(user, pass);
	    if(!authorization)
		return NULL;
	}
    }

    if(stat(file, &sb) != -1 && sb.st_mtime < time(NULL)) {
	Rfc2822DateTime(last_modified, sb.st_mtime);
    } else {
	    time_t mtime = 1104119530;

	Rfc2822DateTime(last_modified, mtime);
	logg("*Assuming modification time in the past\n");
    }

    logg("*If-Modified-Since: %s\n", last_modified);

    logg("Reading CVD header (%s): ", file);

    if(uas)
	agent = uas;
    else
#ifdef CL_EXPERIMENTAL
	agent = PACKAGE"/"VERSION"-exp";
#else
	agent = PACKAGE"/"VERSION;
#endif

    snprintf(cmd, sizeof(cmd),
	"GET %s/%s HTTP/1.0\r\n"
	"Host: %s\r\n%s"
	"User-Agent: %s\r\n"
	"Connection: close\r\n"
	"Range: bytes=0-511\r\n"
        "If-Modified-Since: %s\r\n"
        "\r\n", (remotename != NULL) ? remotename : "", file, hostname, (authorization != NULL) ? authorization : "", agent, last_modified);

    free(remotename);
    free(authorization);

    memset(ipaddr, 0, sizeof(ipaddr));

    if(ip[0]) /* use ip to connect */
	sd = wwwconnect(ip, proxy, port, ipaddr, localip, ctimeout, mdat, logerr);
    else
	sd = wwwconnect(hostname, proxy, port, ipaddr, localip, ctimeout, mdat, logerr);

    if(sd < 0) {
	return NULL;
    } else {
	logg("*Connected to %s (IP: %s).\n", hostname, ipaddr);
	logg("*Trying to retrieve CVD header of http://%s/%s\n", hostname, file);
    }

    if(!ip[0])
	strcpy(ip, ipaddr);

    if(send(sd, cmd, strlen(cmd), 0) < 0) {
	logg("%cremote_cvdhead: write failed\n", logerr ? '!' : '^');
	closesocket(sd);
	return NULL;
    }

    tmp = buffer;
    cnt = FILEBUFF;
#ifdef SO_ERROR
    while((bread = wait_recv(sd, tmp, cnt, 0, rtimeout)) > 0) {
#else
    while((bread = recv(sd, tmp, cnt, 0)) > 0) {
#endif
	tmp += bread;
	cnt -= bread;
	if(cnt <= 0)
	    break;
    }
    closesocket(sd);

    if(bread == -1) {
	logg("%cremote_cvdhead: Error while reading CVD header from %s\n", logerr ? '!' : '^', hostname);
	mirman_update(mdat->currip, mdat, 1);
	return NULL;
    }

    if((strstr(buffer, "HTTP/1.1 404")) != NULL || (strstr(buffer, "HTTP/1.0 404")) != NULL) { 
	logg("%cCVD file not found on remote server\n", logerr ? '!' : '^');
	/* mirman_update(mdat->currip, mdat, 1); */
	return NULL;
    }

    /* check whether the resource is up-to-date */
    if((strstr(buffer, "HTTP/1.1 304")) != NULL || (strstr(buffer, "HTTP/1.0 304")) != NULL) { 
	*ims = 0;
	logg("OK (IMS)\n");
	mirman_update(mdat->currip, mdat, 0);
	return NULL;
    } else {
	*ims = 1;
    }

    if(!strstr(buffer, "HTTP/1.1 200") && !strstr(buffer, "HTTP/1.0 200") &&
       !strstr(buffer, "HTTP/1.1 206") && !strstr(buffer, "HTTP/1.0 206")) {
	logg("%cUnknown response from remote server\n", logerr ? '!' : '^');
	mirman_update(mdat->currip, mdat, 1);
	return NULL;
    }

    i = 3;
    ch = buffer + i;
    while(i < sizeof(buffer)) {
	if (*ch == '\n' && *(ch - 1) == '\r' && *(ch - 2) == '\n' && *(ch - 3) == '\r') {
	    ch++;
	    i++;
	    break;
	}
	ch++;
	i++;
    }

    if(sizeof(buffer) - i < 512) {
	logg("%cremote_cvdhead: Malformed CVD header (too short)\n", logerr ? '!' : '^');
	mirman_update(mdat->currip, mdat, 1);
	return NULL;
    }

    memset(head, 0, sizeof(head));

    for(j = 0; j < 512; j++) {
	if(!ch || (ch && !*ch) || (ch && !isprint(ch[j]))) {
	    logg("%cremote_cvdhead: Malformed CVD header (bad chars)\n", logerr ? '!' : '^');
	    mirman_update(mdat->currip, mdat, 1);
	    return NULL;
	}
	head[j] = ch[j];
    }

    if(!(cvd = cl_cvdparse(head))) {
	logg("%cremote_cvdhead: Malformed CVD header (can't parse)\n", logerr ? '!' : '^');
	mirman_update(mdat->currip, mdat, 1);
    } else {
	logg("OK\n");
	mirman_update(mdat->currip, mdat, 0);
    }

    return cvd;
}

static int getfile(const char *srcfile, const char *destfile, const char *hostname, char *ip, const char *localip, const char *proxy, int port, const char *user, const char *pass, const char *uas, int ctimeout, int rtimeout, struct mirdat *mdat, int logerr)
{
	char cmd[512], buffer[FILEBUFF], *ch;
	int bread, fd, totalsize = 0,  rot = 0, totaldownloaded = 0,
	    percentage = 0, sd;
	unsigned int i;
	char *remotename = NULL, *authorization = NULL, *headerline, ipaddr[16];
	const char *rotation = "|/-\\", *agent;
#ifdef FILTER_RULES
	pid_t pid = -1;
#endif


    if(proxy) {
        remotename = malloc(strlen(hostname) + 8);
	if(!remotename) {
	    logg("!getfile: Can't allocate memory for 'remotename'\n");
	    return 75; /* FIXME */
	}
        sprintf(remotename, "http://%s", hostname);

	if(user) {
	    authorization = proxyauth(user, pass);
	    if(!authorization)
		return 75; /* FIXME */
	}
    }

    if(uas)
	agent = uas;
    else
#ifdef CL_EXPERIMENTAL
	agent = PACKAGE"/"VERSION"-exp";
#else
	agent = PACKAGE"/"VERSION;
#endif

    snprintf(cmd, sizeof(cmd),
	"GET %s/%s HTTP/1.0\r\n"
	"Host: %s\r\n%s"
	"User-Agent: %s\r\n"
#ifdef FRESHCLAM_NO_CACHE
	"Cache-Control: no-cache\r\n"
#endif
	"Connection: close\r\n"
	"\r\n", (remotename != NULL) ? remotename : "", srcfile, hostname, (authorization != NULL) ? authorization : "", agent);

    memset(ipaddr, 0, sizeof(ipaddr));

    if(ip[0]) /* use ip to connect */
	sd = wwwconnect(ip, proxy, port, ipaddr, localip, ctimeout, mdat, logerr);
    else
	sd = wwwconnect(hostname, proxy, port, ipaddr, localip, ctimeout, mdat, logerr);

    if(sd < 0) {
	return 52;
    } else {
	logg("*Trying to download http://%s/%s (IP: %s)\n", hostname, srcfile, ipaddr);
    }

    if(!ip[0])
	strcpy(ip, ipaddr);

    if(send(sd, cmd, strlen(cmd), 0) < 0) {
	logg("%cgetfile: Can't write to socket\n", logerr ? '!' : '^');
	return 52;
    }

    if(remotename)
	free(remotename);

    if(authorization)
	free(authorization);

    /* read http headers */
    ch = buffer;
    i = 0;
    while(1) {
	/* recv one byte at a time, until we reach \r\n\r\n */
#ifdef SO_ERROR
	if((i >= sizeof(buffer) - 1) || wait_recv(sd, buffer + i, 1, 0, rtimeout) == -1) {
#else
	if((i >= sizeof(buffer) - 1) || recv(sd, buffer + i, 1, 0) == -1) {
#endif
	    logg("%cgetfile: Error while reading database from %s (IP: %s)\n", logerr ? '!' : '^', hostname, ipaddr);
	    mirman_update(mdat->currip, mdat, 1);
	    return 52;
	}

	if(i > 2 && *ch == '\n' && *(ch - 1) == '\r' && *(ch - 2) == '\n' && *(ch - 3) == '\r') {
	    i++;
	    break;
	}
	ch++;
	i++;
    }

    buffer[i] = 0;

    /* check whether the resource actually existed or not */
    if((strstr(buffer, "HTTP/1.1 404")) != NULL || (strstr(buffer, "HTTP/1.0 404")) != NULL) { 
	logg("^getfile: %s not found on remote server (IP: %s)\n", srcfile, ipaddr);
	/* mirman_update(mdat->currip, mdat, 1); */
	closesocket(sd);
	return 58;
    }

    if(!strstr(buffer, "HTTP/1.1 200") && !strstr(buffer, "HTTP/1.0 200") &&
       !strstr(buffer, "HTTP/1.1 206") && !strstr(buffer, "HTTP/1.0 206")) {
	logg("%cgetfile: Unknown response from remote server (IP: %s)\n", logerr ? '!' : '^', ipaddr);
	mirman_update(mdat->currip, mdat, 1);
	closesocket(sd);
	return 58;
    }

    /* get size of resource */
    for(i = 0; (headerline = cli_strtok(buffer, i, "\n")); i++){
        if(strstr(headerline, "Content-Length:")) { 
	    if((ch = cli_strtok(headerline, 1, ": "))) {
		totalsize = atoi(ch);
		free(ch);
	    } else {
		totalsize = 0;
	    }
        }
	free(headerline);
    }

#ifdef FILTER_RULES
    {
        int pfd[2];

    	if (pipe(pfd) == -1) {
	    logg("!getfile: Can't create pipe to filter\n");
	    closesocket(sd);
	    return 57;
	}
	pid = fork();
	if (pid == -1) {
	    logg("!getfile: Can't filter fork failed\n");
	    closesocket(sd);
	    return 57;
	}
	if (pid == 0) {
	    /* Child */
	    close(pfd[1]);
	    dup2(pfd[0], 0);
	    close(pfd[0]);
	    execl("/bin/clamfilter", "clamfilter", destfile, NULL);
	    _exit(1);
	}
	/* Parent */
	close(pfd[0]);
	fd = pfd[1];
	destfile = "clam-filter";
    }
#else
    if((fd = open(destfile, O_WRONLY|O_CREAT|O_EXCL|O_BINARY, 0644)) == -1) {
	    char currdir[512];

	getcwd(currdir, sizeof(currdir));
	logg("!getfile: Can't create new file %s in %s\n", destfile, currdir);
	logg("Hint: The database directory must be writable for UID %d or GID %d\n", getuid(), getgid());
	closesocket(sd);
	return 57;
    }
#endif

#ifdef SO_ERROR
    while((bread = wait_recv(sd, buffer, FILEBUFF, 0, rtimeout)) > 0) {
#else
    while((bread = recv(sd, buffer, FILEBUFF, 0)) > 0) {
#endif
        if(write(fd, buffer, bread) != bread) {
	    logg("getfile: Can't write %d bytes to %s\n", bread, destfile);
#ifdef FILTER_RULES
//	    if (pid == -1)
//	        unlink(destfile);
#else
	    unlink(destfile);
#endif
	    close(fd);
	    closesocket(sd);
	    return 57; /* FIXME */
	}

        if(totalsize > 0) {
	    totaldownloaded += bread;
            percentage = (int) (100 * (float) totaldownloaded / totalsize);
	}

        if(!mprintf_quiet) {
            if(totalsize > 0) {
                mprintf("Downloading %s [%3i%%]\r", srcfile, percentage);
            } else {
                mprintf("Downloading %s [%c]\r", srcfile, rotation[rot]);
                rot++;
                rot %= 4;
            }
            fflush(stdout);
        }
    }
    closesocket(sd);
    close(fd);

#ifdef FILTER_RULES
    /*if (pid != -1)*/ {
	    int status;

	waitpid(pid, &status, 0);
	if (! WIFEXITED(status) || WEXITSTATUS(status) != 0) {
	    logg("!getfile: Filter failed\n");
	    return 57;
	}
    }
#endif

    if(totalsize > 0)
        logg("Downloading %s [%i%%]\n", srcfile, percentage);
    else
        logg("Downloading %s [*]\n", srcfile);

    mirman_update(mdat->currip, mdat, 0);
    return 0;
}

static int getcvd(const char *cvdfile, const char *newfile, const char *hostname, char *ip, const char *localip, const char *proxy, int port, const char *user, const char *pass, const char *uas, int nodb, unsigned int newver, int ctimeout, int rtimeout, struct mirdat *mdat, int logerr)
{
	struct cl_cvd *cvd;
	int ret;


    logg("*Retrieving http://%s/%s\n", hostname, cvdfile);
    if((ret = getfile(cvdfile, newfile, hostname, ip, localip, proxy, port, user, pass, uas, ctimeout, rtimeout, mdat, logerr))) {
        logg("%cCan't download %s from %s\n", logerr ? '!' : '^', cvdfile, hostname);
        unlink(newfile);
        return ret;
    }

    if((ret = cl_cvdverify(newfile))) {
        logg("!Verification: %s\n", cl_strerror(ret));
        unlink(newfile);
        return 54;
    }

    if(!(cvd = cl_cvdhead(newfile))) {
	logg("!Can't read CVD header of new %s database.\n", cvdfile);
	unlink(newfile);
	return 54;
    }

    if(cvd->version < newver) {
	logg("^Mirror %s is not synchronized.\n", ip);
    	cl_cvdfree(cvd);
	unlink(newfile);
	return 59;
    }

    cl_cvdfree(cvd);
    return 0;
}

static int chdir_tmp(const char *dbname, const char *tmpdir)
{
	char cvdfile[32];


    if(access(tmpdir, R_OK|W_OK) == -1) {
	sprintf(cvdfile, "%s.cvd", dbname);
	if(access(cvdfile, R_OK) == -1) {
	    sprintf(cvdfile, "%s.cld", dbname);
	    if(access(cvdfile, R_OK) == -1) {
		logg("!chdir_tmp: Can't access local %s database\n", dbname);
		return -1;
	    }
	}

	if(mkdir(tmpdir, 0755) == -1) {
	    logg("!chdir_tmp: Can't create directory %s\n", tmpdir);
	    return -1;
	}

	if(cvd_unpack(cvdfile, tmpdir) == -1) {
	    logg("!chdir_tmp: Can't unpack %s into %s\n", cvdfile, tmpdir);
	    cli_rmdirs(tmpdir);
	    return -1;
	}
    }

    if(chdir(tmpdir) == -1) {
	logg("!chdir_tmp: Can't change directory to %s\n", tmpdir);
	return -1;
    }

    return 0;
}

static int getpatch(const char *dbname, const char *tmpdir, int version, const char *hostname, char *ip, const char *localip, const char *proxy, int port, const char *user, const char *pass, const char *uas, int ctimeout, int rtimeout, struct mirdat *mdat, int logerr)
{
	char *tempname, patch[32], olddir[512];
	int ret, fd;


    if(!getcwd(olddir, sizeof(olddir))) {
	logg("!getpatch: Can't get path of current working directory\n");
	return 50; /* FIXME */
    }

    if(chdir_tmp(dbname, tmpdir) == -1)
	return 50;

    tempname = cli_gentemp(".");
    snprintf(patch, sizeof(patch), "%s-%d.cdiff", dbname, version);

    logg("*Retrieving http://%s/%s\n", hostname, patch);
    if((ret = getfile(patch, tempname, hostname, ip, localip, proxy, port, user, pass, uas, ctimeout, rtimeout, mdat, logerr))) {
        logg("%cgetpatch: Can't download %s from %s\n", logerr ? '!' : '^', patch, hostname);
        unlink(tempname);
        free(tempname);
	chdir(olddir);
        return ret;
    }

    if((fd = open(tempname, O_RDONLY|O_BINARY)) == -1) {
	logg("!getpatch: Can't open %s for reading\n", tempname);
        unlink(tempname);
        free(tempname);
	chdir(olddir);
	return 55;
    }

    if(cdiff_apply(fd, 1) == -1) {
	logg("!getpatch: Can't apply patch\n");
	close(fd);
        unlink(tempname);
        free(tempname);
	chdir(olddir);
	return 70; /* FIXME */
    }

    close(fd);
    unlink(tempname);
    free(tempname);
    chdir(olddir);
    return 0;
}

static struct cl_cvd *currentdb(const char *dbname, char *localname)
{
	char db[32];
	struct cl_cvd *cvd = NULL;


    snprintf(db, sizeof(db), "%s.cvd", dbname);
    if(localname)
	strcpy(localname, db);

    if(access(db, R_OK) == -1) {
	snprintf(db, sizeof(db), "%s.cld", dbname);
	if(localname)
	    strcpy(localname, db);
    }

    if(!access(db, R_OK))
	cvd = cl_cvdhead(db);

    return cvd;
}

static int buildcld(const char *tmpdir, const char *dbname, const char *newfile, unsigned int compr)
{
	DIR *dir;
	char cwd[512], info[32], buff[512], *pt;
	struct dirent *dent;
	int fd, err = 0;
	gzFile *gzs = NULL;


    getcwd(cwd, sizeof(cwd));
    if(chdir(tmpdir) == -1) {
	logg("!buildcld: Can't access directory %s\n", tmpdir);
	return -1;
    }

    snprintf(info, sizeof(info), "%s.info", dbname);
    if((fd = open(info, O_RDONLY|O_BINARY)) == -1) {
	logg("!buildcld: Can't open %s\n", info);
	chdir(cwd);
	return -1;
    }

    if(read(fd, buff, 512) == -1) {
	logg("!buildcld: Can't read %s\n", info);
	chdir(cwd);
	close(fd);
	return -1;
    }
    close(fd);

    if(!(pt = strchr(buff, '\n'))) {
	logg("!buildcld: Bad format of %s\n", info);
	chdir(cwd);
	return -1;
    }
    memset(pt, ' ', 512 + buff - pt);

    if((fd = open(newfile, O_WRONLY|O_CREAT|O_EXCL|O_BINARY, 0644)) == -1) {
	logg("!buildcld: Can't open %s for writing\n", newfile);
	chdir(cwd);
	return -1;
    }
    if(write(fd, buff, 512) != 512) {
	logg("!buildcld: Can't write to %s\n", newfile);
	chdir(cwd);
	unlink(newfile);
	close(fd);
	return -1;
    }

    if((dir = opendir(".")) == NULL) {
	logg("!buildcld: Can't open directory %s\n", tmpdir);
	chdir(cwd);
	unlink(newfile);
	close(fd);
	return -1;
    }

    if(compr) {
	close(fd);
	if(!(gzs = gzopen(newfile, "ab"))) {
	    logg("!buildcld: gzopen() failed for %s\n", newfile);
	    chdir(cwd);
	    unlink(newfile);
	    closedir(dir);
	    return -1;
	}
    }

    if(access("COPYING", R_OK)) {
	logg("!buildcld: COPYING file not found\n");
	err = 1;
    } else {
	if(tar_addfile(fd, gzs, "COPYING") == -1) {
	    logg("!buildcld: Can't add COPYING to .cld file\n");
	    err = 1;
	}
    }

    if(!err && !access("daily.cfg", R_OK)) {
	if(tar_addfile(fd, gzs, "daily.cfg") == -1) {
	    logg("!buildcld: Can't add daily.cfg to .cld file\n");
	    err = 1;
	}
    }

    if(err) {
	chdir(cwd);
	if(gzs)
	    gzclose(gzs);
	else
	    close(fd);
	unlink(newfile);
	return -1;
    }

    while((dent = readdir(dir))) {
#ifndef C_INTERIX
	if(dent->d_ino)
#endif
	{
	    if(!strcmp(dent->d_name, ".") || !strcmp(dent->d_name, "..") || !strcmp(dent->d_name, "COPYING") || !strcmp(dent->d_name, "daily.cfg"))
		continue;

	    if(tar_addfile(fd, gzs, dent->d_name) == -1) {
		logg("!buildcld: Can't add %s to .cld file\n", dent->d_name);
		chdir(cwd);
		if(gzs)
		    gzclose(gzs);
		else
		    close(fd);
		unlink(newfile);
		closedir(dir);
		return -1;
	    }
	}
    }
    closedir(dir);

    if(gzs) {
	if(gzclose(gzs)) {
	    logg("!buildcld: gzclose() failed for %s\n", newfile);
	    unlink(newfile);
	    return -1;
	}
    } else {
	if(close(fd) == -1) {
	    logg("!buildcld: close() failed for %s\n", newfile);
	    unlink(newfile);
	    return -1;
	}
    }

    if(chdir(cwd) == -1) {
	logg("!buildcld: Can't return to previous directory %s\n", cwd);
	return -1;
    }

    return 0;
}

static int updatedb(const char *dbname, const char *hostname, char *ip, int *signo, const struct cfgstruct *copt, const char *dnsreply, char *localip, int outdated, struct mirdat *mdat, int logerr)
{
	struct cl_cvd *current, *remote;
	const struct cfgstruct *cpt;
	unsigned int nodb = 0, currver = 0, newver = 0, port = 0, i, j;
	int ret, ims = -1;
	char *pt, cvdfile[32], localname[32], *tmpdir = NULL, *newfile, newdb[32], cwd[512];
	const char *proxy = NULL, *user = NULL, *pass = NULL, *uas = NULL;
	unsigned int flevel = cl_retflevel(), maxattempts;
	int ctimeout, rtimeout;


    snprintf(cvdfile, sizeof(cvdfile), "%s.cvd", dbname);

    if(!(current = currentdb(dbname, localname))) {
	nodb = 1;
    } else {
	mdat->dbflevel = current->fl;
    }

    if(!nodb && dnsreply) {
	    int field = 0;

	if(!strcmp(dbname, "main")) {
	    field = 1;
	} else if(!strcmp(dbname, "daily")) {
	    field = 2;
	} else {
	    logg("!updatedb: Unknown database name (%s) passed.\n", dbname);
	    cl_cvdfree(current);
	    return 70;
	}

	if(field && (pt = cli_strtok(dnsreply, field, ":"))) {
	    if(!cli_isnumber(pt)) {
		logg("^Broken database version in TXT record.\n");
	    } else {
		newver = atoi(pt);
		logg("*%s version from DNS: %d\n", cvdfile, newver);
	    }
	    free(pt);
	} else {
	    logg("^Invalid DNS reply. Falling back to HTTP mode.\n");
	}
    }

    /* Initialize proxy settings */
    if((cpt = cfgopt(copt, "HTTPProxyServer"))->enabled) {
	proxy = cpt->strarg;
	if(strncasecmp(proxy, "http://", 7) == 0)
	    proxy += 7;

	if((cpt = cfgopt(copt, "HTTPProxyUsername"))->enabled) {
	    user = cpt->strarg;
	    if((cpt = cfgopt(copt, "HTTPProxyPassword"))->enabled) {
		pass = cpt->strarg;
	    } else {
		logg("HTTPProxyUsername requires HTTPProxyPassword\n");
		if(current)
		    cl_cvdfree(current);
		return 56;
	    }
	}

	if((cpt = cfgopt(copt, "HTTPProxyPort"))->enabled)
	    port = cpt->numarg;

	logg("Connecting via %s\n", proxy);
    }

    if((cpt = cfgopt(copt, "HTTPUserAgent"))->enabled)
	uas = cpt->strarg;

    ctimeout = cfgopt(copt, "ConnectTimeout")->numarg;
    rtimeout = cfgopt(copt, "ReceiveTimeout")->numarg;

    if(!nodb && !newver) {

	remote = remote_cvdhead(cvdfile, hostname, ip, localip, proxy, port, user, pass, uas, &ims, ctimeout, rtimeout, mdat, logerr);

	if(!nodb && !ims) {
	    logg("%s is up to date (version: %d, sigs: %d, f-level: %d, builder: %s)\n", localname, current->version, current->sigs, current->fl, current->builder);
	    *signo += current->sigs;
	    cl_cvdfree(current);
	    return 1;
	}

	if(!remote) {
	    logg("^Can't read %s header from %s (IP: %s)\n", cvdfile, hostname, ip);
	    cl_cvdfree(current);
	    return 58;
	}

	newver = remote->version;
	cl_cvdfree(remote);
    }

    if(!nodb && (current->version >= newver)) {
	logg("%s is up to date (version: %d, sigs: %d, f-level: %d, builder: %s)\n", localname, current->version, current->sigs, current->fl, current->builder);

	if(!outdated && flevel < current->fl) {
	    /* display warning even for already installed database */
	    logg("^Current functionality level = %d, recommended = %d\n", flevel, current->fl);
	    logg("Please check if ClamAV tools are linked against the proper version of libclamav\n");
	    logg("DON'T PANIC! Read http://www.clamav.net/support/faq\n");
	}

	*signo += current->sigs;
	cl_cvdfree(current);
	return 1;
    }


    if(current) {
	currver = current->version;
	cl_cvdfree(current);
    }

    /*
    if(ipaddr[0]) {
	hostfd = wwwconnect(ipaddr, proxy, port, NULL, localip);
    } else {
	hostfd = wwwconnect(hostname, proxy, port, ipaddr, localip);
	if(!ip[0])
	    strcpy(ip, ipaddr);
    }

    if(hostfd < 0) {
	if(ipaddr[0])
	    logg("Connection with %s (IP: %s) failed.\n", hostname, ipaddr);
	else
	    logg("Connection with %s failed.\n", hostname);
	return 52;
    };
    */

    if(!cfgopt(copt, "ScriptedUpdates")->enabled)
	nodb = 1;

    getcwd(cwd, sizeof(cwd));
    newfile = cli_gentemp(cwd);

    if(nodb) {
	ret = getcvd(cvdfile, newfile, hostname, ip, localip, proxy, port, user, pass, uas, nodb, newver, ctimeout, rtimeout, mdat, logerr);
	if(ret) {
	    memset(ip, 0, 16);
	    free(newfile);
	    return ret;
	}
	snprintf(newdb, sizeof(newdb), "%s.cvd", dbname);

    } else {
	ret = 0;

	tmpdir = cli_gentemp(".");
	maxattempts = cfgopt(copt, "MaxAttempts")->numarg;
	for(i = currver + 1; i <= newver; i++) {
	    for(j = 0; j < maxattempts; j++) {
		    int llogerr = logerr;
		if(logerr)
		    llogerr = (j == maxattempts - 1);
		ret = getpatch(dbname, tmpdir, i, hostname, ip, localip, proxy, port, user, pass, uas, ctimeout, rtimeout, mdat, llogerr);
		if(ret == 52 || ret == 58) {
		    memset(ip, 0, 16);
		    continue;
		} else {
		    break;
		}
	    }
	    if(ret)
		break;
	}

	if(ret) {
	    cli_rmdirs(tmpdir);
	    free(tmpdir);
	    logg("^Incremental update failed, trying to download %s\n", cvdfile);
	    ret = getcvd(cvdfile, newfile, hostname, ip, localip, proxy, port, user, pass, uas, 1, newver, ctimeout, rtimeout, mdat, logerr);
	    if(ret) {
		free(newfile);
		return ret;
	    }
	    snprintf(newdb, sizeof(newdb), "%s.cvd", dbname);
	} else {
	    if(buildcld(tmpdir, dbname, newfile, cfgopt(copt, "CompressLocalDatabase")->enabled) == -1) {
		logg("!Can't create local database\n");
		cli_rmdirs(tmpdir);
		free(tmpdir);
		free(newfile);
		return 70; /* FIXME */
	    }
	    snprintf(newdb, sizeof(newdb), "%s.cld", dbname);
	    cli_rmdirs(tmpdir);
	    free(tmpdir);
	}
    }

    if(!(current = cl_cvdhead(newfile))) {
	logg("!Can't parse new database %s\n", newfile);
	unlink(newfile);
	free(newfile);
	return 55; /* FIXME */
    }

    if(!nodb && !access(localname, R_OK) && unlink(localname)) {
	logg("!Can't unlink %s. Please fix it and try again.\n", localname);
	unlink(newfile);
	free(newfile);
	return 53;
    }

    if(rename(newfile, newdb) == -1) {
	logg("!Can't rename %s to %s\n", newfile, newdb);
	unlink(newfile);
	free(newfile);
	return 57;
    }
    free(newfile);

    logg("%s updated (version: %d, sigs: %d, f-level: %d, builder: %s)\n", newdb, current->version, current->sigs, current->fl, current->builder);

    if(flevel < current->fl) {
	logg("^Your ClamAV installation is OUTDATED!\n");
	logg("^Current functionality level = %d, recommended = %d\n", flevel, current->fl);
	logg("DON'T PANIC! Read http://www.clamav.net/support/faq\n");
    }

    *signo += current->sigs;
    cl_cvdfree(current);
    return 0;
}

int downloadmanager(const struct cfgstruct *copt, const struct optstruct *opt, const char *hostname, const char *dbdir, int logerr)
{
	time_t currtime;
	int ret, updated = 0, outdated = 0, signo = 0;
	unsigned int ttl;
	char ipaddr[16], *dnsreply = NULL, *pt, *localip = NULL, *newver = NULL;
	const char *arg = NULL;
	const struct cfgstruct *cpt;
	struct mirdat mdat;
#ifdef HAVE_RESOLV_H
	const char *dnsdbinfo;
#endif

    time(&currtime);
    logg("ClamAV update process started at %s", ctime(&currtime));

#ifndef HAVE_LIBGMP
    logg("SECURITY WARNING: NO SUPPORT FOR DIGITAL SIGNATURES\n");
    logg("See the FAQ at http://www.clamav.net/support/faq for an explanation.\n");
#endif

#ifdef HAVE_RESOLV_H
    dnsdbinfo = cfgopt(copt, "DNSDatabaseInfo")->strarg;

    if(opt_check(opt, "no-dns")) {
	dnsreply = NULL;
    } else {
	if((dnsreply = txtquery(dnsdbinfo, &ttl))) {
	    logg("*TTL: %d\n", ttl);

	    if((pt = cli_strtok(dnsreply, 3, ":"))) {
		    int rt;
		    time_t ct;

		rt = atoi(pt);
		free(pt);
		time(&ct);
		if((int) ct - rt > 10800) {
		    logg("^DNS record is older than 3 hours.\n");
		    free(dnsreply);
		    dnsreply = NULL;
		}

	    } else {
		free(dnsreply);
		dnsreply = NULL;
	    }

	    if(dnsreply) {
		    int vwarning = 1;

		if((pt = cli_strtok(dnsreply, 4, ":"))) {
		    if(*pt == '0')
			vwarning = 0;

		    free(pt);
		}

		if((newver = cli_strtok(dnsreply, 0, ":"))) {

		    logg("*Software version from DNS: %s\n", newver);

		    if(vwarning && !strstr(cl_retver(), "devel") && !strstr(cl_retver(), "rc")) {
			if(strcmp(cl_retver(), newver)) {
			    logg("^Your ClamAV installation is OUTDATED!\n");
			    logg("^Local version: %s Recommended version: %s\n", cl_retver(), newver);
			    logg("DON'T PANIC! Read http://www.clamav.net/support/faq\n");
			    outdated = 1;
			}
		    }
		}

	    } else {
		if(dnsreply) {
		    free(dnsreply);
		    dnsreply = NULL;
		}
	    }
	}

	if(!dnsreply) {
	    logg("^Invalid DNS reply. Falling back to HTTP mode.\n");
	}
    }
#endif /* HAVE_RESOLV_H */

    if(opt_check(opt, "local-address")) {
        localip = opt_arg(opt, "local-address");
    } else if((cpt = cfgopt(copt, "LocalIPAddress"))->enabled) {
	localip = cpt->strarg;
    }

    if(cfgopt(copt, "HTTPProxyServer")->enabled)
	mirman_read("mirrors.dat", &mdat, 0);
    else
	mirman_read("mirrors.dat", &mdat, 1);

    memset(ipaddr, 0, sizeof(ipaddr));

    if((ret = updatedb("main", hostname, ipaddr, &signo, copt, dnsreply, localip, outdated, &mdat, logerr)) > 50) {
	if(dnsreply)
	    free(dnsreply);

	if(newver)
	    free(newver);

	mirman_write("mirrors.dat", &mdat);
	return ret;

    } else if(ret == 0)
	updated = 1;

    /* if ipaddr[0] != 0 it will use it to connect to the web host */
    if((ret = updatedb("daily", hostname, ipaddr, &signo, copt, dnsreply, localip, outdated, &mdat, logerr)) > 50) {
	if(dnsreply)
	    free(dnsreply);

	if(newver)
	    free(newver);

	mirman_write("mirrors.dat", &mdat);
	return ret;

    } else if(ret == 0)
	updated = 1;

    if(dnsreply)
	free(dnsreply);

    mirman_write("mirrors.dat", &mdat);

    if(updated) {
	if(cfgopt(copt, "HTTPProxyServer")->enabled) {
	    logg("Database updated (%d signatures) from %s\n", signo, hostname);
	} else {
	    logg("Database updated (%d signatures) from %s (IP: %s)\n", signo, hostname, ipaddr);
	}

#ifdef BUILD_CLAMD
	if(opt_check(opt, "daemon-notify")) {
		const char *clamav_conf = opt_arg(opt, "daemon-notify");
	    if(!clamav_conf)
		clamav_conf = CONFDIR"/clamd.conf";

	    notify(clamav_conf);
	} else if((cpt = cfgopt(copt, "NotifyClamd"))->enabled) {
	    notify(cpt->strarg);
	}
#endif

	if(opt_check(opt, "on-update-execute"))
	    arg = opt_arg(opt, "on-update-execute");
	else if((cpt = cfgopt(copt, "OnUpdateExecute"))->enabled)
	    arg = cpt->strarg;

	if(arg) {
	    if(opt_check(opt, "daemon"))
		execute("OnUpdateExecute", arg);
            else if(system(arg) == -1)
		logg("!system(%s) failed\n", arg);
	}
    }

    if(outdated) {
	if(opt_check(opt, "on-outdated-execute"))
	    arg = opt_arg(opt, "on-outdated-execute");
	else if((cpt = cfgopt(copt, "OnOutdatedExecute"))->enabled)
	    arg = cpt->strarg;

	if(arg) {
		char *cmd = strdup(arg);

	    if((pt = newver)) {
		while(*pt) {
		    if(!strchr("0123456789.", *pt)) {
			logg("!downloadmanager: OnOutdatedExecute: Incorrect version number string\n");
			free(newver);
			newver = NULL;
			break;
		    }
		    pt++;
		}
	    }

	    if(newver && (pt = strstr(cmd, "%v"))) {
		    char *buffer = (char *) malloc(strlen(cmd) + strlen(newver) + 10);

		if(!buffer) {
		    logg("!downloadmanager: Can't allocate memory for buffer\n");
		    free(cmd);
		    if(newver)
			free(newver);
		    return 75;
		}

		*pt = 0; pt += 2;
		strcpy(buffer, cmd);
		strcat(buffer, newver);
		strcat(buffer, pt);
		free(cmd);
		cmd = strdup(buffer);
		free(buffer);
	    }

	    if(newver) {
		if(opt_check(opt, "daemon"))
		    execute("OnOutdatedExecute", cmd);
		else if(system(cmd) == -1)
		logg("!system(%s) failed\n", cmd);
	    }
	    free(cmd);
	}
    }

    if(newver)
	free(newver);

    return updated ? 0 : 1;
}

