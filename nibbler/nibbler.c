/* nibbler.c */

#if (defined(WIN32) || defined(_WINDOWS)) && !defined(__CYGWIN__)
#	define WIN 1
#	include <windows.h>
#	include <share.h>
#	define longest_int _int64
#	define PRINTF_LONG_LONG "%I64d"
#else
#	define longest_int long long int
#endif

#ifndef NO_DEFAULT_DEFS
#	ifndef HAVE_UNISTD_H 
#		define HAVE_UNISTD_H 1
#	endif
#	ifndef _FILE_OFFSET_BITS
#		define _FILE_OFFSET_BITS 64
#	endif
#	ifndef HAVE_SNPRINTF
#		define HAVE_SNPRINTF 1
#	endif
#endif

#ifdef HAVE_UNISTD_H
#	include <unistd.h>
#endif
#include <fcntl.h>
#include <sys/time.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "getopt.h"
#include "md5.h"

#ifndef INADDR_NONE
#	define INADDR_NONE ((unsigned long)(-1L))
#endif

#define Socklen_t socklen_t
#ifndef Socklen_t
#	define Socklen_t int
#endif

#ifndef sockopt_size_t
#	define sockopt_size_t socklen_t
#	define GETSOCKOPT_ARG4
#endif

#ifndef PRINTF_LONG_LONG
#	define PRINTF_LONG_LONG "%lld"
#endif

#define kKilobyte 1024
#define kMegabyte (kKilobyte * 1024)
#define kGigabyte ((long) kMegabyte * 1024L)
#define kTerabyte ((double) kGigabyte * 1024.0)

struct timeval t0, t1;
char *buf;
size_t bufsize = 8192;
int verbose = 1;
int md5mode = 0;
static struct md5_ctx ctx;
int used_stdin = 0;
int progress_increment = kMegabyte;
int progress_reports = 0;
double time_spent_waiting = 0.0;



static void
Usage(void)
{
	fprintf(stderr, "Usage: nibbler [-options] [files...] [tcp:port...] < stdin\n");
	fprintf(stderr, "\nOptions:\n");
	fprintf(stderr, "  -b XX : Set transfer buffer size to XX (default: %lu).\n", (unsigned long) bufsize);
	fprintf(stderr, "  -v    : Verbose messages.\n");
	fprintf(stderr, "  -vv   : Even more verbose messages.\n");
	fprintf(stderr, "  -q    : No messages.\n");
	fprintf(stderr, "  -i    : Consume stdin even if other items were specified.\n");
	fprintf(stderr, "  -m    : Print the MD5 digest for all data consumed.\n");
	fprintf(stderr, "  -mm   : Print the MD5 digest for each item consumed.\n");
	fprintf(stderr, "  -p    : Print a dot for every 1 MB eaten.\n");
	fprintf(stderr, "  -P XX : Print a dot for every XX eaten.\n");
	fprintf(stderr, "\nExamples:\n");
	fprintf(stderr, "  echo \"hello world\" | nibbler\n");
	fprintf(stderr, "  nibbler -mm /etc/services /etc/protocols\n");
	fprintf(stderr, "  nibbler -v tcp:51234 &; echo \"hello world\" | nc localhost 51234\n");
	fprintf(stderr, "  dd if=/dev/urandom bs=1048576 count=10 status=none | ssh -c blowfish -o Compression=no a.example.com /usr/local/bin/nibbler -p\n");
	fprintf(stderr, "\nVersion:\n  1.2 * 2014-06-18 * Gleason\n");
	exit(2);
}	/* Usage */




static double
FileSize(const double size, const char **uStr0, double *const uMult0)
{
	double uMult, uTotal;
	const char *uStr;

	/* The comparisons below may look odd, but the reason
	 * for them is that we only want a maximum of 3 digits
	 * before the decimal point.  (I.e., we don't want to
	 * see "1017.2 kB", instead we want "0.99 MB".
	 */
	if (size > (999.5 * kGigabyte)) {
		uStr = "TiB";
		uMult = kTerabyte;
	} else if (size > (999.5 * kMegabyte)) {
		uStr = "GiB";
		uMult = kGigabyte;
	} else if (size > (999.5 * kKilobyte)) {
		uStr = "MiB";
		uMult = kMegabyte;
	} else if (size > 999.5) {
		uStr = "kiB";
		uMult = 1024;
	} else {
		uStr = "B";
		uMult = 1;
	}
	if (uStr0 != NULL)
		*uStr0 = uStr;
	if (uMult0 != NULL)
		*uMult0 = uMult;
	uTotal = size / ((double) uMult);
	if (uTotal < 0.0)
		uTotal = 0.0;
	return (uTotal);
}	/* FileSize */





static longest_int
unit_atoll(const char *const str)
{
	longest_int x = 0;
	char unit[4];

	if (sscanf(str, PRINTF_LONG_LONG "%c", &x, unit) == 2) {
		switch ((int) unit[0]) {
			case 'k':
			case 'K':
				x *= 1024;
				break;
			case 'm':
			case 'M':
				x *= 1024;
				x *= 1024;
				break;
			case 'g':
			case 'G':
				x *= 1024;
				x *= 1024;
				x *= 1024;
				break;
			case 't':
			case 'T':
				x *= 1024;
				x *= 1024;
				x *= 1024;
				x *= 1024;
				break;
		}
	} else if (sscanf(str, PRINTF_LONG_LONG, &x) != 1) {
		x = 0;
	}
	return (x);
}	/* unit_atoll */




static char *
AddrToAddrStr(char *const dst, size_t dsize, struct sockaddr_in * const saddrp, int dns, const char *fmt)
{
	const char *addrNamePtr;
	struct hostent *hp;
	char str[128];
	char *dlim, *dp;
	const char *cp;
	struct servent *pp;

	if (dns == 0) {
		addrNamePtr = inet_ntoa(saddrp->sin_addr);
	} else {
		hp = gethostbyaddr((char *) &saddrp->sin_addr, (int) sizeof(struct in_addr), AF_INET);
		if ((hp != NULL) && (hp->h_name != NULL) && (hp->h_name[0] != '\0')) {
			addrNamePtr = hp->h_name;
		} else {
			addrNamePtr = inet_ntoa(saddrp->sin_addr);
		}
	}
	if (fmt == NULL)
		fmt = "%h:%p";
	for (dp = dst, dlim = dp + dsize - 1; ; fmt++) {
		if (*fmt == '\0') {
			break;	/* done */
		} else if (*fmt == '%') {
			fmt++;
			if (*fmt == '%') {
				if (dp < dlim)
					*dp++ = '%';
			} else if (*fmt == 'p') {
				sprintf(str, "%u", (unsigned int) ntohs(saddrp->sin_port));
				for (cp = str; *cp != '\0'; cp++)
					if (dp < dlim)
						*dp++ = *cp;
				*dp = '\0';
			} else if (*fmt == 'h') {
				if (addrNamePtr != NULL) {
					cp = addrNamePtr;
				} else {
					cp = "unknown";
				}
				for ( ; *cp != '\0'; cp++)
					if (dp < dlim)
						*dp++ = *cp;
				*dp = '\0';
			} else if (*fmt == 's') {
				pp = getservbyport((int) (saddrp->sin_port), "tcp");
				if (pp == NULL)
					pp = getservbyport((int) (saddrp->sin_port), "udp");
				if (pp == NULL) {
					sprintf(str, "%u", (unsigned int) ntohs(saddrp->sin_port));
					cp = str;
				} else {
					cp = pp->s_name;
				}
				for ( ; *cp != '\0'; cp++)
					if (dp < dlim)
						*dp++ = *cp;
				/* endservent(); */
				*dp = '\0';
			} else if ((*fmt == 't') || (*fmt == 'u')) {
				pp = getservbyport((int) (saddrp->sin_port), (*fmt == 'u') ? "udp" : "tcp");
				if (pp == NULL) {
					sprintf(str, "%u", (unsigned int) ntohs(saddrp->sin_port));
					cp = str;
				} else {
					cp = pp->s_name;
				}
				for ( ; *cp != '\0'; cp++)
					if (dp < dlim)
						*dp++ = *cp;
				/* endservent(); */
				*dp = '\0';
			} else if (*fmt == '\0') {
				break;
			} else {
				if (dp < dlim)
					*dp++ = *fmt;
			}
		} else if (dp < dlim) {
			*dp++ = *fmt;
		}
	}
	*dp = '\0';
	return (dst);
}	/* AddrToAddrStr */




static char *
ElapsedStr(double elap, char *const s, const size_t siz)
{
	int ielap, day, sec, min, hr;

	ielap = (int) (elap + 0.5);
	if (ielap < 0)
		ielap = 0;
	sec = ielap % 60;
	min = (ielap / 60) % 60;
	hr = (ielap / 3600) % 24;
	day = (ielap / 86400);

	if (day > 0) {
		snprintf(s, siz, "%d:%02d:%02d:%02d", day, hr, min, sec);
	} else if (hr > 0) {
		snprintf(s, siz, "%d:%02d:%02d", hr, min, sec);
	} else {
		snprintf(s, siz, "%d:%02d", min, sec);
	}

	return (s);
}	/* ElapsedStr */




static double
Duration2(struct timeval *xt0, struct timeval *xt1)
{
	double sec;

	if (xt0->tv_usec > xt1->tv_usec) {
		xt1->tv_usec += 1000000;
		xt1->tv_sec--;
	}
	sec = ((double) (xt1->tv_usec - xt0->tv_usec) * 0.000001)
		+ (xt1->tv_sec - xt0->tv_sec);

	return (sec);
}	/* Duration2 */




static char *
RateStr(char *dst, size_t dsiz, double tbytes, double dura)
{
	double rateInUnits;
	const char *rStr;

	if (dura <= 0) dura = 0.0;
	rateInUnits = FileSize(tbytes / dura, &rStr, NULL);
	snprintf(dst, dsiz, "%.2f %s/s", rateInUnits, rStr);
	return (dst);
}	/* RateStr */




static char *
md5_get_digest_hexstr(char *digestStr, size_t dsiz, struct md5_ctx *done_ctx)
{
	int i;
	unsigned char digestRaw[16+16];

	if (dsiz < 33) return (NULL);
	memset(digestStr, 0, dsiz);
	md5_finish_ctx(done_ctx, digestRaw);
	for (i=0; i<16; i++) {
		sprintf(digestStr + (i * 2), "%02lx", (unsigned long) digestRaw[i]);
	}
	return (digestStr);
}	/* md5_get_digest_hexstr */




static long long int
nibbler(const char *fname)
{
	int fd;
	int nread;
	long long int tread = 0;
	int proginc = progress_increment;
	int doprog = progress_reports;
	long long int nextprog = (long long int) proginc;

	struct timeval wt0, wt1;
	int servfd, connfd;
	Socklen_t caSize;
	struct sockaddr_in srvAddr, clientAddr, myAddr;
	char clientAddrStr[80], myAddrStr[80];
	int i, on;
	Socklen_t siz;
	int onsize;
	short port;
	int md5mode1 = md5mode;
	
	if ((fname == NULL) || (strcmp(fname, "stdin") == 0) || (strcmp(fname, "-") == 0) || (fname[0] == '\0')) {
		fd = 0;
		fname = "stdin";
		if (++used_stdin > 1)
			fd = -1;
	} else if (strncmp(fname, "tcp:", 4) == 0) {

		port = 0;
		if ((sscanf(fname + 4, "%hd", &port) != 1) || (port == 0)) {
			errno = EINVAL;
			perror("tcp port number");
			exit(1);
		}

		srvAddr.sin_family = AF_INET;
		srvAddr.sin_addr.s_addr = htonl(INADDR_ANY);
		srvAddr.sin_port = htons(port);

		servfd = socket(srvAddr.sin_family, SOCK_STREAM, 0);
		if (servfd < 0) {
			perror("socket");
			exit(1);
		}

		/* This is mostly so you can quit the server and re-run it
		 * again right away.  If you don't do this, the OS may complain
		 * that the address is still in use.
		 */
		on = 1;
		onsize = (int) sizeof(on);
		(void) setsockopt(servfd, SOL_SOCKET, SO_REUSEADDR,
			(char *) &on, onsize);

		for (i=1; ; i++) {
			/* Try binding a few times, in case we get Address in Use
			 * errors.
			 */
			if (bind(servfd, (struct sockaddr *) &srvAddr, sizeof(srvAddr)) == 0) {
				break;
			}
			if (i == 3) {
				perror("Can't bind local address, exiting");
				exit(1);
			} else {
				perror("Can't bind local address, waiting 7 seconds");
				/* Give the OS time to clean up the old socket,
				 * and then try again.
				 */
				sleep(7);
			}
		}

		if (listen(servfd, 1) < 0) {
			perror("listen");
			exit(1);
		}

		(void) gettimeofday(&wt0, NULL);
		if (verbose > 1) fprintf(stderr, "nibbler: waiting for connection from remote client to port %hu\n", port);
		caSize = (Socklen_t) sizeof(clientAddr);
		connfd = accept(servfd, (struct sockaddr *) &clientAddr, &caSize);
		if (connfd < 0) {
			perror("accept");
			exit(1);
		}

		(void) gettimeofday(&wt1, NULL);
		time_spent_waiting += Duration2(&wt0, &wt1);
		close(servfd);

		siz = (Socklen_t) sizeof(myAddr);
		if (getsockname(connfd, (struct sockaddr *) &myAddr, &siz) < 0) {
			perror("getpeername");
			exit(1);
		}
		(void) AddrToAddrStr(myAddrStr, sizeof(myAddrStr), &myAddr, 1, NULL);

		siz = (Socklen_t) sizeof(clientAddr);
		if (getpeername(connfd, (struct sockaddr *) &clientAddr, &siz) < 0) {
			perror("getsockname");
			exit(1);
		}
		(void) AddrToAddrStr(clientAddrStr, sizeof(clientAddrStr), &clientAddr, 1, NULL);

		if (verbose > 1) fprintf(stderr, "nibbler: connected from %s to %s\n", clientAddrStr, myAddrStr);
		fd = connfd;
	} else {
		fd = open(fname, O_RDONLY);
		if (fd < 0) {
			perror(fname);
			exit(1);
		}
	}

	memset(buf, 0, bufsize);
	for (tread = 0; ; ) {
		nread = (int) read(fd, buf, bufsize);
		if (nread == 0)
			break;
		if (nread < 0) {
			if (errno == EINTR)
				continue;
			(void) fprintf(stderr, "read error on fd=%d, %s: %s\n", fd, fname, strerror(errno));
			exit(3);
		}
		if (md5mode1) md5_process_bytes(buf, nread, &ctx);
		tread += nread;
		if (!doprog) continue;
		while (tread > nextprog) {
			(void) fprintf(stderr, ".");
			nextprog += proginc;
		}
	}

	close(fd);
	return (tread);
}	/* nibbler */




int
main(int argc, const char **argv)
{
	long long int tread = 0, tread1 = 0;
	int i = 0, c = 0;
	int force_stdin = 0;
	char dstr[64], rstr[64];
	char md5str[64];

	memset(&ctx, 0, sizeof(ctx));
	memset(md5str, 0, sizeof(md5str));
	memset(dstr, 0, sizeof(dstr));
	memset(rstr, 0, sizeof(rstr));
	memset(&t0, 0, sizeof(t0));
	memset(&t1, 0, sizeof(t1));

	while ((c = Getopt(argc, argv, "b:impP:vq")) > 0) switch(c) {
		case 'b':
			if (unit_atoll(gOptArg) >= 16)
				bufsize = (size_t) unit_atoll(gOptArg);
			break;
		case 'q':
			verbose = 0;
			break;
		case 'v':
			verbose++;
			break;
		case 'i':
			force_stdin++;
			break;
		case 'm':
			md5mode++;
			break;
		case 'p':
			progress_reports = 1;
			break;
		case 'P':
			progress_reports = 1;
			if (unit_atoll(gOptArg) >= 16)
				progress_increment = (int) unit_atoll(gOptArg);
			break;
		default:
			Usage();
	}

	buf = malloc(bufsize);
	if (buf == NULL) {
		(void) fprintf(stderr, "could not allocate %lu bytes of memory: %s\n", (unsigned long) bufsize, strerror(errno));
		exit(4);
	}

	(void) signal(SIGPIPE, SIG_IGN);
	if (md5mode) md5_init_ctx(&ctx);
	gettimeofday(&t0, NULL);
	for (i=gOptInd; i<argc; i++) {
		if (md5mode >= 2) md5_init_ctx(&ctx);
		if (verbose > 1) fprintf(stdout, "nibbling %s\n", argv[i]);
		if (progress_reports) fprintf(stderr, "%s: ", argv[i]);
		tread1 = (long long int) nibbler(argv[i]);
		if (progress_reports) fprintf(stderr, "\n");
		if (verbose > 2) fprintf(stdout, "  munched %lld from %s\n", tread1, argv[i]);
		tread += tread1;
		if (md5mode >= 2) fprintf(stdout, "%s %s\n", md5_get_digest_hexstr(md5str, sizeof(md5str), &ctx), argv[i]);

	}

	if ((!used_stdin) && (force_stdin || (gOptInd == argc))) {
		if (md5mode >= 2) md5_init_ctx(&ctx);
		if (verbose > 1) fprintf(stdout, "nibbling %s\n", "stdin");
		tread1 = (long long int) nibbler("stdin");
		if (progress_reports) fprintf(stderr, "\n");
		if (verbose > 2) fprintf(stdout, "  munched %lld from %s\n", tread1, "stdin");
		tread += tread1;
		if (md5mode >= 2) fprintf(stdout, "%s %s\n", md5_get_digest_hexstr(md5str, sizeof(md5str), &ctx), "stdin");
	}

	gettimeofday(&t1, NULL);

	if (tread == 0) Usage();

	if (md5mode == 1) fprintf(stdout, "%s\n", md5_get_digest_hexstr(md5str, sizeof(md5str), &ctx));
	if (verbose > 0) fprintf(stdout, "%lld bytes devoured in %s (%s).\n",
		tread,
		ElapsedStr(Duration2(&t0, &t1) - time_spent_waiting, dstr, sizeof(dstr)),
		RateStr(rstr, sizeof(rstr), (double) tread, Duration2(&t0, &t1) - time_spent_waiting)
	);

	exit(0);
}	/* main */
