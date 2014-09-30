/* bigfilemaker main.c */

#if (defined(WIN32) || defined(_WINDOWS)) && !defined(__CYGWIN__)
#	define WIN 1
#	include <windows.h>
#	include <stdio.h>
#	include <string.h>
#	include <stdlib.h>
#	include <time.h>
#	include <ctype.h>
#	include <share.h>
#	include "getopt.h"
#	undef UNIX
#	define longest_int _int64
#	define sockopt_size_t int
#	define GETSOCKOPT_ARG4 (char *)
#	define PRINTF_LONG_LONG "%I64d"

/* Identical in declaration to struct _stati64.
 * Unfortunately, we don't get _stati64 unless we're on an IA-64
 */
struct WinStat64 {
	_dev_t st_dev;
	_ino_t st_ino;
	unsigned short st_mode;
	short st_nlink;
	short st_uid;
	short st_gid;
	_dev_t st_rdev;
	__int64 st_size;
	time_t st_atime;
	time_t st_mtime;
	time_t st_ctime;
};

int WinFStat64(const int h0, struct WinStat64 *const stp);
int WinStat64(const char *const path, struct WinStat64 *const stp);

#	ifndef Stat
#		define Stat WinStat64
#		define Lstat WinStat64
#		define Fstat WinFStat64
#	endif
#else
#	if ((!defined(_FILE_OFFSET_BITS)) || (_FILE_OFFSET_BITS < 64))
#		error "Missing definition of _FILE_OFFSET_BITS"
#	endif
#	include <unistd.h>
#	include <sys/types.h>
#	include <sys/stat.h>
#	include <sys/time.h>
#	include <fcntl.h>
#	include <stdio.h>
#	include <string.h>
#	include <stdlib.h>
#	include <time.h>
#	include <sys/socket.h>
#	include <netinet/in.h>
#	include <arpa/inet.h>
#	include <netdb.h>
#	include <ctype.h>
#	include <dirent.h>
#	include <errno.h>
#	include <signal.h>
#	define UNIX 1
#	define PRINTF_LONG_LONG "%lld"
#	define longest_int long long
#	define sockopt_size_t socklen_t
#	define GETSOCKOPT_ARG4
#endif

#ifndef INADDR_ANY
#	define INADDR_ANY              ((unsigned long int) 0x00000000)
#endif

#ifndef INADDR_NONE
#	define INADDR_NONE             ((unsigned long int) 0xffffffff)
#endif

/* Linux libc 5.3.x has a bug that causes isalnum() to not work! */
#define ISALNUM(c) ( (((c) >= 'A') && ((c) <= 'Z')) || (((c) >= 'a') && ((c) <= 'z')) || (((c) >= '0') && ((c) <= '9')) )

#define kAddrStrToAddrMiscErr (-4)
#define kAddrStrToAddrBadHost (-5)

#define write_return_t int
#define write_size_t unsigned int
#define forever for (;;)

#define kKilobyte 1024
#define kMegabyte (kKilobyte * 1024)
#define kGigabyte ((long) kMegabyte * 1024L)
#define kTerabyte ((double) kGigabyte * 1024.0)

#ifdef HAVE_GETOPT
	/* Use system getopt() rather than included version */
#	define Getopt getopt
#	define gOptInd optind
#	define gOptArg optarg
#	define GetoptReset()
	extern int optind;
	extern char *optarg;
#else
#	include "getopt.h"
#endif

#include "Strn.h"
#include "rand48.h"
#include "md5.h"

#define VERSION_STR "1.4 (2014-09-30)"


#define MINBLOCKSIZE 4096
#define MAXBLOCKSIZE (1L*1024*1024*1024)

#define MINBLOCKS 2

static size_t gBlockSize = 1048576;
static struct md5_ctx ctx;
static unsigned char digestRaw[16+16];
static char digestStr[32+16];
static longest_int gTotalBytes = 0;
static time_t gLastProgReport = 0;
static FILE *reportFp = NULL;
static struct timeval tv0, tv1;
static longest_int gFileSize = 0;
static double gdFileSize;
static int randbyte_i = 0;
static FILE *randlog = NULL;
static int gDebug = 0;

extern unsigned short BSD_rand48_seed[3];





#ifdef WIN
int
WinStat64(const char *const path, struct WinStat64 *const stp)
{
	HANDLE h;
	__int64 fSize;
	DWORD fSize1, fSize2;
	struct _stat st32;
	DWORD winErr;

	memset(stp, 0, sizeof(struct WinStat64));
	if (_stat(path, &st32) < 0)
		return (-1);
	stp->st_atime = st32.st_atime;
	stp->st_ctime = st32.st_ctime;
	stp->st_dev = st32.st_dev;
	stp->st_gid = st32.st_gid;
	stp->st_ino = st32.st_ino;
	stp->st_mode = st32.st_mode;
	stp->st_mtime = st32.st_mtime;
	stp->st_nlink = st32.st_nlink;
	stp->st_rdev = st32.st_rdev;
	stp->st_uid = st32.st_uid;

	if (S_ISREG(stp->st_mode)) {
		h = CreateFile(path,
			0,				/* Not GENERIC_READ; use 0 for "query attributes only" mode */
			0,
			NULL,
			OPEN_EXISTING,	/* fails if it doesn't exist */
			0,
			NULL
			);
		
		if (h == INVALID_HANDLE_VALUE)
			goto return_err;

		fSize = (__int64)0;
		fSize1 = GetFileSize(h, &fSize2);
		if ((fSize1 == 0xFFFFFFFF) && ((winErr = GetLastError()) != NO_ERROR))
			goto return_err;

		fSize = ((__int64) fSize2 << 32) | (__int64) fSize1;
		stp->st_size = fSize;
		CloseHandle(h);
	}
	return (0);

return_err:
	stp->st_size = (__int32) st32.st_size;
	if ((winErr = GetLastError()) == ERROR_SHARING_VIOLATION) {
		errno = EBUSY;
	} else if ((winErr == ERROR_PATH_NOT_FOUND) || (winErr == ERROR_FILE_NOT_FOUND)) {
		errno = ENOENT;
	} else if (winErr == ERROR_INVALID_PARAMETER) {
		errno = EINVAL;
	} else {
		errno = 100000 + winErr;
	}

	if (h != INVALID_HANDLE_VALUE)
		CloseHandle(h);
	return (-1);
}	/* WinStat64 */




static int
GetFileSize(const char *const path, longest_int *fsize)
{
	struct WinStat64 st;

	if (WinStat64(path, &st) == 0) {
		*fsize = (longest_int) st.st_size;
		return (0);
	}
	*fsize = (longest_int) 0;
	return (-1);
}	/* GetFileSize */
#elif defined(UNIX)





static int
GetFileSize(const char *const path, longest_int *fsize)
{
	struct stat st;

	memset(&st, 0, sizeof(st));
	if (sizeof(st.st_size) <= 4) {
		/* assertion failure */
		*fsize = (longest_int) 0;
		return (-2);
	}

	if (stat(path, &st) == 0) {
		*fsize = (longest_int) st.st_size;
		return (0);
	}
	*fsize = (longest_int) 0;
	return (-1);
}	/* GetFileSize */
#endif	/* UNIX */




#ifndef SO_RCVBUF
static int
GetSocketBufSize(const int UNUSED_sockfd, size_t *const rsize, size_t *const ssize)
{
	if (ssize != NULL)
		*ssize = 0;
	if (rsize != NULL)
		*rsize = 0;
	return (-1);
}	/* GetSocketBufSize */
#else
static int
GetSocketBufSize(const int sockfd, size_t *const rsize, size_t *const ssize)
{
	int rc = -1;
	int opt;
	sockopt_size_t optsize;

	if (ssize != NULL) {
		opt = 0;
		optsize = (sockopt_size_t) sizeof(opt);
		rc = getsockopt(sockfd, SOL_SOCKET, SO_SNDBUF, GETSOCKOPT_ARG4 &opt, &optsize);
		if (rc == 0)
			*ssize = (size_t) opt;
		else
			*ssize = 0;
	}
	if (rsize != NULL) {
		opt = 0;
		optsize = (sockopt_size_t) sizeof(opt);
		rc = getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, GETSOCKOPT_ARG4 &opt, &optsize);
		if (rc == 0)
			*rsize = (size_t) opt;
		else
			*rsize = 0;
	}
	return (rc);
}	/* GetSocketBufSize */
#endif




static int
AddrStrToAddr(const char * const s, struct sockaddr_in * const sa, const int defaultport)
{
	char portstr[128];
	unsigned int ipnum;
	unsigned int port;
	struct hostent *hp;
	char *hostcp, *atsign, *colon, *cp, *p2;

	memset(sa, 0, sizeof(struct sockaddr_in));
	strncpy(portstr, s, sizeof(portstr));
	portstr[sizeof(portstr) - 1] = '\0';

	if ((colon = strchr(portstr, ':')) != NULL) {
		/* Does it look like a URL?  http://host ? */
		if ((colon[1] == '/') && (colon[2] == '/')) {
			*colon = '\0';
			port = 0;
			hostcp = colon + 3;
			for (cp = hostcp; *cp != '\0'; cp++) {
				if ((!ISALNUM(*cp)) && (*cp != '.')) {
					/* http://host:port */
					if ((*cp == ':') && (isdigit((int) cp[1]))) {
						*cp++ = '\0';
						p2 = cp;
						while (isdigit((int) *cp))
							cp++;
						*cp = '\0';
						port = atoi(p2);
					}
					*cp = '\0';
					break;
				}
			}
			/*
			if (port == 0)
				port = ServiceNameToPortNumber(portstr, 0);
			*/
		} else {
			/* Look for host.name.domain:port */
			*colon = '\0';
			hostcp = portstr;
			port = (unsigned int) atoi(colon + 1);
		}
	} else if ((atsign = strchr(portstr, '@')) != NULL) {
		/* Look for port@host.name.domain */
		*atsign = '\0';
		hostcp = atsign + 1;
		port = (unsigned int) atoi(portstr);
	} else if (defaultport > 0) {
		/* Have just host.name.domain, use that w/ default port. */
		port = (unsigned int) defaultport;
		hostcp = portstr;
	} else {
		/* If defaultport <= 0, they must supply a port number
		 * in the host/port string.
		 */
		errno = EADDRNOTAVAIL;
		return (kAddrStrToAddrMiscErr);
	}

	sa->sin_port = htons((short) port);

	ipnum = inet_addr(hostcp);
	if (ipnum != INADDR_NONE) {
		sa->sin_family = AF_INET;
		sa->sin_addr.s_addr = ipnum;
	} else {
#ifdef DNSSEC_LOCAL_VALIDATION
		val_status_t val_status;
		errno = 0;
		hp = val_gethostbyname(NULL,hostcp,&val_status);
		if ((hp != NULL) && (!val_istrusted(val_status)))
			hp = NULL;
#else
		errno = 0;
		hp = gethostbyname(hostcp);
#endif
		if (hp == NULL) {
			if (errno == 0)
				errno = ENOENT;
			return (kAddrStrToAddrBadHost);
		}
		sa->sin_family = hp->h_addrtype;
		memcpy(&sa->sin_addr.s_addr, hp->h_addr_list[0],
			(size_t) hp->h_length);
	}
	return (0);
}	/* AddrStrToAddr */





static longest_int unit_atoll(const char *const str)
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




static void
Usage(const char *const prg)
{
	fprintf(stderr, "Usages:  %s <-z|-c|-r|-R> -S random-seed -s file-size [-o outputFile]\n", (prg == NULL) ? "bigfilemaker" : prg);
	fprintf(stderr, "         %s <-z|-c|-r|-R> -S random-seed -s file-size -o remote.host.name:tcp_port\n", (prg == NULL) ? "bigfilemaker" : prg);
	fprintf(stderr, "         %s <-z|-c|-r|-R> -S random-seed -m num-modifications -o preexistingOutputFile\n", (prg == NULL) ? "bigfilemaker" : prg);
	fprintf(stderr, "\nOptions:\n"); 
	fprintf(stderr, "  -s XX  Set the output file size to XX (can be specified with units, e.g., 100MB).\n");
	fprintf(stderr, "  -b XX  Set block size to XX (default %lu). Larger buffers generally increase performance.\n", (unsigned long) gBlockSize); 
	fprintf(stderr, "  -M     Do not calculate MD5 sum. File generation speed will increase slightly.\n");
	fprintf(stderr, "  -y 0   Do not sync (default).\n");
	fprintf(stderr, "  -y 1   Do fsync after last block.\n");
	fprintf(stderr, "  -y 2   Do fsync after each block.\n");
	fprintf(stderr, "  -z     Write zeroed data rather than random binary data. The resulting file will be highly compressible.\n");
	fprintf(stderr, "  -c X   Write constant stream of character X rather than random binary data. The resulting file will be highly compressible.\n");
	fprintf(stderr, "  -r     Write one randomized block repeatedly, rather than randomizing the entire file.\n");
	fprintf(stderr, "  -R     Randomize the contents of the entire output file at significant performance cost.\n");
	fprintf(stderr, "\nNotes:\n");
	fprintf(stderr, "    Use \"-m\" mode to simulate changes to a large file made previously without \"-m\", while efficiently recalculating MD5 sum.\n");
	fprintf(stderr, "\nExamples: %s -R -S 0x77c0ffee -s 5g -o 5gigfile.dat\n", (prg == NULL) ? "bigfilemaker" : prg);
	fprintf(stderr, "          %s -R -S 0x77c0ffee -m 10 -o 5gigfile.dat\n", (prg == NULL) ? "bigfilemaker" : prg);
	fprintf(stderr, "          %s -R -S 0x77c0ffee -s 200m -o test.example.com:51234\n", (prg == NULL) ? "bigfilemaker" : prg);
	fprintf(stderr, "\nVersion: %s\n\n", VERSION_STR);
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
		uStr = "TB";
		uMult = kTerabyte;
	} else if (size > (999.5 * kMegabyte)) {
		uStr = "GB";
		uMult = kGigabyte;
	} else if (size > (999.5 * kKilobyte)) {
		uStr = "MB";
		uMult = kMegabyte;
	} else if (size > 999.5) {
		uStr = "kB";
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




static double
FileSizeBits(const double sizeBits, const char **uStr0, double *const uMult0)
{
	double uMult, uTotal;
	const char *uStr;

	/* The comparisons below may look odd, but the reason
	 * for them is that we only want a maximum of 3 digits
	 * before the decimal point.  (I.e., we don't want to
	 * see "1017.2 kB", instead we want "0.99 MB".
	 */
	if (sizeBits > (999.5 * kGigabyte)) {
		uStr = "Tb";
		uMult = kTerabyte;
	} else if (sizeBits > (999.5 * kMegabyte)) {
		uStr = "Gb";
		uMult = kGigabyte;
	} else if (sizeBits > (999.5 * kKilobyte)) {
		uStr = "Mb";
		uMult = kMegabyte;
	} else if (sizeBits > 999.5) {
		uStr = "kb";
		uMult = 1024;
	} else {
		uStr = "b";
		uMult = 1;
	}
	if (uStr0 != NULL)
		*uStr0 = uStr;
	if (uMult0 != NULL)
		*uMult0 = uMult;
	uTotal = sizeBits / ((double) uMult);
	if (uTotal < 0.0)
		uTotal = 0.0;
	return (uTotal);
}	/* FileSizeBits */




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
Duration2(struct timeval *t0, struct timeval *t1)
{
	double sec;

	if (t0->tv_usec > t1->tv_usec) {
		t1->tv_usec += 1000000;
		t1->tv_sec--;
	}
	sec = ((double) (t1->tv_usec - t0->tv_usec) * 0.000001)
		+ (t1->tv_sec - t0->tv_sec);

	return (sec);
}	/* Duration2 */





static void
ProgressReport(longest_int n, int lastReport)
{
	double dura;
	double tbytes, tbits;
	double rateInUnits, sizeInUnits;
	double rateInUnitsB;
	double rateUnits, rateUnitsB;
	double secLeft;
	time_t now;
	const char *rStr, *sStr;
	const char *rStrB, *sStrB;
	int hr, min, sec;

	gTotalBytes += n;
	time(&now);
	if ((lastReport != 0) || (now > (gLastProgReport + 1))) {
		gLastProgReport = now;
		(void) gettimeofday(&tv1, NULL);
		dura = Duration2(&tv0, &tv1);
		if (dura <= 0.0)
			return;
		tbits = tbytes = (double) gTotalBytes;
		tbits *= 8;

		(void) FileSizeBits(tbits, &sStrB, NULL);
		rateInUnitsB = FileSizeBits(tbits / dura, &rStrB, &rateUnitsB);
		sizeInUnits = FileSize(tbytes, &sStr, NULL);
		rateInUnits = FileSize(tbytes / dura, &rStr, &rateUnits);
		if (rateInUnits > 0.0) {
			// pctDone = 100.0 * tbytes / gdFileSize;
			secLeft = ((gdFileSize - tbytes) / rateUnits) / rateInUnits;
			sec = (int) secLeft;
			if (sec <= 0) sec = 1;
			hr = sec / 3600;
			min = (sec / 60) % 60;
			sec = sec % 60;
			if (lastReport == 0) {
				fprintf(stderr, "\rProcess%s    %.2f %s   %6.2f %s/s   %6.2f %s/s   ETA %d:%02d:%02d    ", "ing:",  sizeInUnits, sStr, rateInUnits, rStr, rateInUnitsB, rStrB, hr, min, sec);
			} else {
				fprintf(stderr, "\rProcess%s    %.2f %s   %6.2f %s/s   %6.2f %s/s                       ", "ed: ", sizeInUnits, sStr, rateInUnits, rStr, rateInUnitsB, rStrB);
			}
		} else {
			fprintf(stderr, "\rProcessing    %.2f %s%-50s", sizeInUnits, sStr, "");
		}
	}
}	/* ProgressReport */




static int
BlockWrite(int sfd, const void *const buf0, size_t size)
{
	write_return_t nwrote;
	write_size_t nleft;
	const char *buf = buf0;
	
	if ((buf == NULL) || (size == 0)) {
		errno = EINVAL;
		return (-1);
	}
	
	nleft = (write_size_t) size;
	forever {
		nwrote = write(sfd, buf, nleft);
		if (nwrote < 0) {
			if (errno != EINTR) {
				nwrote = (write_return_t) size - (write_return_t) nleft;
				if (nwrote == 0)
					nwrote = (write_return_t) -1;
				goto done;
			} else {
				errno = 0;
				nwrote = 0;
				/* Try again. */
			}
		}
		nleft -= (write_size_t) nwrote;
		if (nleft == 0)
			break;
		buf += nwrote;
	}
	nwrote = (write_return_t) size - (write_return_t) nleft;

done:
	return ((int) nwrote);
}	/* BlockWrite */




static void randbyte_reset(void)
{
	randbyte_i = 0;
}	/* randbyte_reset */




static unsigned char randbyte(void)
{
	static union randbuf {
		double d;
		unsigned char b[6];
	} u;
	unsigned char b;

	if (randbyte_i == 0) {
		u.d = BSD_drand48();
	}

	b = u.b[randbyte_i];

	if (++randbyte_i == 6) {
		randbyte_i = 0;
	}

	if (randlog != NULL) {
		if (isgraph((int) b)) {
			fprintf(randlog, "> randbyte = '%c'\n", b);
		} else {
			fprintf(randlog, "> randbyte = 0x%02x\n", b);
		}
	}
	return (b);
}	/* randbyte */




static void BSD_srand48log(long seed)
{
	unsigned short oseed[3];

	oseed[0] = BSD_rand48_seed[0];
	oseed[1] = BSD_rand48_seed[1];
	oseed[2] = BSD_rand48_seed[2];
	BSD_srand48(seed);
	if (randlog != NULL)
		fprintf(randlog, "> srand48: oseed=%02x,%02x,%02x seed=%08lx nseed=%02x,%02x,%02x\n", oseed[0], oseed[1], oseed[2], seed, BSD_rand48_seed[0], BSD_rand48_seed[1], BSD_rand48_seed[2]);
}	/* srand48log */




static long BSD_lrand48log(void)
{
	long x = BSD_lrand48();
	if (randlog != NULL)
		fprintf(randlog, "> lrand48 = 0x%08lx\n", x);
	return (x);
}	/* lrand48log */





int main(int argc, const char **argv)
{
	int c, i;
	int nFullBlocks;
	longest_int nllFullBlocks;
	int nFullBlocksToDo;
	int iBlk = 0;
	int nBytesToDo;
	write_return_t partialBlockSize;
	unsigned char *theBlock = NULL, *ucp;
	char *modifications = NULL;
	int ofp = 1;
	int fillMode = -1;
	int iMod, nModifications = 0;
	time_t t0;
	long seed = 0L, defaultSeed = 0L;
	int *blocks = NULL;
	int eBlk;
	int eBlkOffset;
	size_t rbufsize = 0, sbufsize = 0;
	longest_int osize = 0;
	off_t o;
	char dstr[128];
	int sockfd = -1;
	size_t tcp_wmem[6];
	struct sockaddr_in serverAddr;
	int isRegularFile = 0;
	struct stat st;
	int md5mode = 1;
	int fillChar = 0;
	int randomizeOneBlockOnly = 0;
	int do_fsync = 0;

	(void) time(&t0);
	defaultSeed = (long) t0;
	reportFp = stderr;

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) >= 0) {
		if ((GetSocketBufSize(sockfd, &rbufsize, &sbufsize) == 0) && (sbufsize > gBlockSize) && ((int) sbufsize < MAXBLOCKSIZE)) {
			if (gDebug > 0) fprintf(stderr, "* Setting block size to socket buffer size (%lu).\n", (unsigned long) sbufsize);
			gBlockSize = (int) sbufsize;
		}
		close(sockfd);
		sockfd = -1;
	}

	memset(tcp_wmem, 0, sizeof(tcp_wmem));
	if ((sockfd = open("/proc/sys/net/ipv4/tcp_wmem", O_RDONLY)) >= 0) {
		memset(dstr, 0, sizeof(dstr));
		if ((read(sockfd, dstr, sizeof(dstr) - 1) > 0) && (sscanf(dstr, "%lu %lu %lu", &tcp_wmem[0], &tcp_wmem[1], &tcp_wmem[2]) == 3) && (tcp_wmem[2] > gBlockSize) && (tcp_wmem[2] < MAXBLOCKSIZE)) {
			if (gDebug > 0) fprintf(stderr, "* Setting block size to net.ipv4.tcp_wmem max (%lu).\n", (unsigned long) tcp_wmem[2]);
			gBlockSize = tcp_wmem[2];
		}
		close(sockfd);
		sockfd = -1;
	}

	GetoptReset();
	while ((c = Getopt(argc, argv, "b:B:c:d:L:Mm:o:rRs:S:y:z")) > 0) switch (c) {
		case 'b':
		case 'B':
			gBlockSize = (size_t) unit_atoll(gOptArg);
			if ((gBlockSize < MINBLOCKSIZE) || (gBlockSize > MAXBLOCKSIZE)) {
				fprintf(stderr, "Bad block size, %lu.\n", gBlockSize);
				Usage(argv[0]);
			}
			break;
		case 'd':
			gDebug = 1;
			break;
		case 'L':
			randlog = fopen(gOptArg, "wt");
			if (randlog == NULL) {
				perror(gOptArg);
				exit(1);
			}
			break;
		case 'M':
			md5mode = 0;
			break;
		case 'm':
			nModifications = atoi(gOptArg);
			if ((nModifications < 1) || (! isdigit(gOptArg[0]))) {
				fprintf(stderr, "Bad number of modifications, %d.\n", nModifications);
				Usage(argv[0]);
			}
			break;
		case 'o':
			if (nModifications > 0) {
				if (GetFileSize(gOptArg, &osize) < 0) {
					perror(gOptArg);
					exit(1);
				}
				ofp = open(gOptArg, O_WRONLY);
				if ((gFileSize > 0) && (osize != gFileSize)) {
					fprintf(stderr, "You said that %s has file size " PRINTF_LONG_LONG ", but it appears to have file size " PRINTF_LONG_LONG " instead.\n", gOptArg, gFileSize, osize);
					exit(1);
				}
				gFileSize = osize;
			} else if (strchr(gOptArg, ':') != NULL) {
				if (AddrStrToAddr(gOptArg, &serverAddr, 51234) < 0) {
					(void) fprintf(stderr, "Bad network host:port name, %s: %s\n", gOptArg, strerror(errno));
					exit(1);
				}

				ofp = socket(serverAddr.sin_family, SOCK_STREAM, 0);
				if (ofp < 0) {
					perror("socket");
					exit(1);
				}

				if (connect(ofp, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) < 0) {
					perror("connect");
					exit(1);
				}

				signal(SIGPIPE, SIG_IGN);
			} else {
				ofp = open(gOptArg, O_WRONLY|O_TRUNC|O_CREAT, 00666);
			}
			if (ofp < 0) {
				perror(gOptArg);
				exit(1);
			}
			reportFp = stdout;
			break;
		case 'S':
			seed = 0L;
			if ((1 != sscanf(gOptArg, "%li", &seed)) || (seed == 0L)) {
				fprintf(stderr, "Bad random seed, %08lx.\n", seed);
				Usage(argv[0]);
			}
			break;
		case 's':
			gFileSize = unit_atoll(gOptArg);
			if (gFileSize <= 0) {
				fprintf(stderr, "Bad filesize, " PRINTF_LONG_LONG ".\n", gFileSize);
				Usage(argv[0]);
			}
			break;
		case 'z':
			fillMode = 0;
			fillChar = 0;
			randomizeOneBlockOnly = 0;
			break;
		case 'c':
			fillMode = 0;
			fillChar = (int) gOptArg[0];
			if (strlen(gOptArg) > 1)
				(void) sscanf(gOptArg, "%i", &fillChar);
			randomizeOneBlockOnly = 0;
			break;
		case 'r':
			fillMode = 0;
			fillChar = -1;
			randomizeOneBlockOnly = 1;
			break;
		case 'R':
			fillMode = 1;
			fillChar = -1;
			randomizeOneBlockOnly = 0;
			break;
		case 'y':
			do_fsync = atoi(gOptArg);
			if ((do_fsync < 0) || (! isdigit(gOptArg[0]))) {
				fprintf(stderr, "Bad sync type, %s.\n", gOptArg);
				Usage(argv[0]);
			}
			break;
		default:
			Usage(argv[0]);
			/*NOTREACHED*/
	}

	if (fstat(ofp, &st) < 0) {
		perror("fstat");
		exit(1);
	}
	isRegularFile = (S_ISREG(st.st_mode)) ? 1 : 0;

#ifdef UNIX
	if (isatty(ofp)) {
		fprintf(stderr, "It doesn't seem wise to write binary data to your screen.\n\n");
		Usage(argv[0]);
	}
#endif
	if ((fillMode < 0) || (gFileSize <= 0)) {
		Usage(argv[0]);
	}

	randbyte_reset();
	if (nModifications > 0) {
		if (seed == 0L) {
			fprintf(stderr, "You must specify the random seed you used for the initial file creation when you perform modificaitons.\n");
			Usage(argv[0]);
		}
		BSD_srand48log(seed);

		modifications = calloc((size_t) nModifications + 1, sizeof(char));
		for (iMod=0; iMod<nModifications; iMod++) {
			do {
				c = (int) (randbyte());
			} while (! isalnum(c));
			modifications[iMod] = (char) c;
		}
		randbyte_reset();
		/* Important that seed gets re-set below */
	}

	if (seed == 0L)
		seed = defaultSeed;
	BSD_srand48log(seed);

	theBlock = malloc(gBlockSize);
	if (theBlock == NULL) {
		fprintf(stderr, "Could not allocate a %lu-byte block buffer: %s\n", (unsigned long) gBlockSize, strerror(errno));
		exit(1);
	}

	if (md5mode) md5_init_ctx(&ctx);
	memset(theBlock, fillChar, gBlockSize);
	memset(digestRaw, 0, sizeof(digestRaw));
	memset(digestStr, 0, sizeof(digestStr));
	memset(dstr, 0, sizeof(dstr));

	gdFileSize = (double) gFileSize;
	nllFullBlocks = (longest_int) gFileSize / gBlockSize;
	if (nllFullBlocks > (longest_int) 2*1024*1024*1024) {
		fprintf(stderr, "Too many blocks (" PRINTF_LONG_LONG "). Increase block size from %lu.\n", nllFullBlocks, (unsigned long) gBlockSize);
		exit(1);
	}
	nFullBlocks = (int) (gFileSize / gBlockSize);
	if (nFullBlocks < MINBLOCKS) {
		fprintf(stderr, "Too few blocks (%d; at least %d are required). At block size %lu, your file size should be at least %lu bytes.\n", nFullBlocks, MINBLOCKS, (unsigned long) gBlockSize, (unsigned long) (gBlockSize * MINBLOCKS));
		exit(1);
	}
	nFullBlocksToDo = nFullBlocks;
	partialBlockSize = (write_return_t) (gFileSize - (nllFullBlocks * gBlockSize));

	blocks = calloc((size_t) nFullBlocks + 2, sizeof(int));
	if (blocks == NULL) {
		fprintf(stderr, "Could not allocate a %lu * %lu-byte block buffer: %s\n", (unsigned long) gBlockSize, (unsigned long) sizeof(int), strerror(errno));
		exit(1);
	}

	if (nFullBlocks/2 < nModifications) {
		fprintf(stderr, "Too many modifications (%d) requested. With the block size currently set to %lu bytes, your bigfile has only %d blocks and we do one modification per block. A maximum of %d modifications is allowed in order for the randomization sequence to complete in a reasonable amount of time.\n", nModifications, gBlockSize, nFullBlocks, nFullBlocks/2); 
		exit(1);
	}

	/* fprintf(stdout, "Sizeof off_t = %lu\n", (unsigned long) sizeof(off_t)); */
	for (iMod=0; iMod<nModifications; iMod++) {
		eBlk = (int) (BSD_lrand48log() % (long) nFullBlocks);
		eBlkOffset = (int) (BSD_lrand48log() % (long) gBlockSize) + 1;
		if (blocks[eBlk != 0]) {
			iMod--;
			continue;
		}
		blocks[eBlk] = eBlkOffset;
	}

	BSD_srand48log(seed);
	(void) gettimeofday(&tv0, NULL);

	if (nModifications > 0) {
		iMod = 0;
		if (fillMode == 0) {
			if (randomizeOneBlockOnly != 0) {
				ucp = theBlock;
				nBytesToDo = (int) gBlockSize;
				while (--nBytesToDo >= 0) {
					*ucp++ = randbyte();
				}
			}
			for (iBlk = 0; iBlk < nFullBlocksToDo; iBlk++) {
				eBlkOffset = blocks[iBlk];
				ProgressReport((longest_int) gBlockSize, 0);
				if (eBlkOffset == 0) {
					if (md5mode) md5_process_bytes(theBlock, gBlockSize, &ctx);
				} else {
					o = (off_t) gBlockSize * (off_t) iBlk + (off_t) eBlkOffset - (off_t) 1;
					c = (int) theBlock[eBlkOffset - 1];
					if (isgraph(c)) {
						fprintf(stdout, "\n* Modification #%d at block number %d, block offset %d, total offset " PRINTF_LONG_LONG ", from '%c' to '%c'.\n", iMod + 1, iBlk, eBlkOffset - 1, (longest_int) o, c, modifications[iMod]);
					} else {
						fprintf(stdout, "\n* Modification #%d at block number %d, block offset %d, total offset " PRINTF_LONG_LONG ", from 0x%02x to '%c'.\n", iMod + 1, iBlk, eBlkOffset - 1, (longest_int) o, c, modifications[iMod]);
					}
					theBlock[eBlkOffset - 1] = modifications[iMod];
					if (md5mode) md5_process_bytes(theBlock, gBlockSize, &ctx);
					if (((off_t) -1) == lseek(ofp, o, SEEK_SET)) {
						perror("lseek failed");
						exit(1);
					}
					if (BlockWrite(ofp, (unsigned char *) theBlock + eBlkOffset - 1, 1) != (write_return_t) 1) {
						perror("write1 failed");
						exit(1);
					}
					theBlock[eBlkOffset - 1] = (char) c;
					iMod++;
				}
			}
			ProgressReport((longest_int) partialBlockSize, 0);
		} else {
			for (iBlk = 0; iBlk < nFullBlocksToDo; iBlk++) {
				ucp = theBlock;
				nBytesToDo = (int) gBlockSize;
				while (--nBytesToDo >= 0) {
					*ucp++ = randbyte();
				}

				eBlkOffset = blocks[iBlk];
				ProgressReport((longest_int) gBlockSize, 0);
				if (eBlkOffset == 0) {
					if (md5mode) md5_process_bytes(theBlock, gBlockSize, &ctx);
				} else {
					o = (off_t) gBlockSize * (off_t) iBlk + (off_t) eBlkOffset - (off_t) 1;
					c = (int) theBlock[eBlkOffset - 1];
					if (isgraph(c)) {
						fprintf(stdout, "\n* Modification #%d at block number %d, block offset %d, total offset " PRINTF_LONG_LONG ", from '%c' to '%c'.\n", iMod + 1, iBlk, eBlkOffset - 1, (longest_int) o, c, modifications[iMod]);
					} else {
						fprintf(stdout, "\n* Modification #%d at block number %d, block offset %d, total offset " PRINTF_LONG_LONG ", from 0x%02x to '%c'.\n", iMod + 1, iBlk, eBlkOffset - 1, (longest_int) o, c, modifications[iMod]);
					}
					theBlock[eBlkOffset - 1] = modifications[iMod];
					if (md5mode) md5_process_bytes(theBlock, gBlockSize, &ctx);
					if (((off_t) -1) == lseek(ofp, o, SEEK_SET)) {
						perror("lseek failed");
						exit(1);
					}
					if (BlockWrite(ofp, (unsigned char *) theBlock + eBlkOffset - 1, 1) != (write_return_t) 1) {
						perror("write1 failed");
						exit(1);
					}
					theBlock[eBlkOffset - 1] = (char) c;	/* Not needed for rand mode */
					iMod++;
				}
			}

			if (partialBlockSize > 0) {
				ucp = theBlock;
				nBytesToDo = (int) partialBlockSize;
				while (--nBytesToDo >= 0) {
					*ucp++ = randbyte();
				}
				if (md5mode) md5_process_bytes(theBlock, partialBlockSize, &ctx);
			}
			ProgressReport((longest_int) partialBlockSize, 0);
		}
	} else if ((fillMode == 0) && (md5mode == 0)) {
		/* Send block as-is, with no MD5 calculations. */
		if (randomizeOneBlockOnly != 0) {
			ucp = theBlock;
			nBytesToDo = (int) gBlockSize;
			while (--nBytesToDo >= 0) {
				*ucp++ = randbyte();
			}
		}
		while (--nFullBlocksToDo >= 0) {
			if ((BlockWrite(ofp, theBlock, gBlockSize) != (write_return_t) gBlockSize) || ((isRegularFile != 0) && (do_fsync & 002) && (fsync(ofp) < 0))) {
				perror("write failed");
				exit(1);
			}
			ProgressReport((longest_int) gBlockSize, 0);
		}

		if (partialBlockSize > 0) {
			if (BlockWrite(ofp, theBlock, partialBlockSize) != partialBlockSize) {
				perror("write last failed");
				exit(1);
			}
			ProgressReport((longest_int) partialBlockSize, 0);
		}
	} else if (fillMode == 0) {
		/* Send block as-is, *with* MD5 calculations. */
		if (randomizeOneBlockOnly != 0) {
			ucp = theBlock;
			nBytesToDo = (int) gBlockSize;
			while (--nBytesToDo >= 0) {
				*ucp++ = randbyte();
			}
		}
		while (--nFullBlocksToDo >= 0) {
			md5_process_bytes(theBlock, gBlockSize, &ctx);
			if ((BlockWrite(ofp, theBlock, gBlockSize) != (write_return_t) gBlockSize) || ((isRegularFile != 0) && (do_fsync & 002) && (fsync(ofp) < 0))) {
				perror("write failed");
				exit(1);
			}
			ProgressReport((longest_int) gBlockSize, 0);
		}

		if (partialBlockSize > 0) {
			md5_process_bytes(theBlock, partialBlockSize, &ctx);
			if (BlockWrite(ofp, theBlock, partialBlockSize) != partialBlockSize) {
				perror("write last failed");
				exit(1);
			}
			ProgressReport((longest_int) partialBlockSize, 0);
		}
	} else /* if (fillMode != 0) && (nModifications == 0) */ {
		/* Randomize each block *with* MD5 calculations. */
		while (--nFullBlocksToDo >= 0) {
			ucp = theBlock;
			nBytesToDo = (int) gBlockSize;
			while (--nBytesToDo >= 0) {
				*ucp++ = randbyte();
			}
			if (md5mode) md5_process_bytes(theBlock, gBlockSize, &ctx);
			if ((BlockWrite(ofp, theBlock, gBlockSize) != (write_return_t) gBlockSize) || ((isRegularFile != 0) && (do_fsync & 002) && (fsync(ofp) < 0))) {
				perror("write failed");
				exit(1);
			}
			ProgressReport((longest_int) gBlockSize, 0);
		}

		if (partialBlockSize > 0) {
			ucp = theBlock;
			nBytesToDo = (int) partialBlockSize;
			while (--nBytesToDo >= 0) {
				*ucp++ = randbyte();
			}
			if (md5mode) md5_process_bytes(theBlock, partialBlockSize, &ctx);
			if ((BlockWrite(ofp, theBlock, partialBlockSize) != partialBlockSize) || ((isRegularFile != 0) && (do_fsync & 002) && (fsync(ofp) < 0))) {
				perror("write last failed");
				exit(1);
			}
			ProgressReport((longest_int) partialBlockSize, 0);
		}
	}

	if ((do_fsync & 001) && (fsync(ofp) < 0)) {
		perror("fsync (write) last failed");
		exit(1);
	}

	if (ofp != 1) {
		if (close(ofp) != 0) {
			perror("close (write) failed");
			exit(1);
		}
	}
	ProgressReport(0, 1);

	if (md5mode) md5_finish_ctx(&ctx, digestRaw);
	for (i=0; i<16; i++) {
		sprintf(digestStr + (i * 2), "%02lx", (unsigned long) digestRaw[i]);
	}

	fprintf(reportFp, "\nFile size:     " PRINTF_LONG_LONG "\n", gFileSize);
	fprintf(reportFp, "Block size:    %lu\n", gBlockSize);
	(void) strftime(dstr, sizeof(dstr) - 1, "%Y-%m-%d %H:%M:%S %z", localtime(&tv0.tv_sec));
	fprintf(reportFp, "Start time:    %s\n", dstr);
	(void) strftime(dstr, sizeof(dstr) - 1, "%Y-%m-%d %H:%M:%S %z", localtime(&tv1.tv_sec));
	fprintf(reportFp, "End time:      %s\n", dstr);
	(void) ElapsedStr(Duration2(&tv0, &tv1), dstr, sizeof(dstr));
	fprintf(reportFp, "Elapsed time:  %s\n", dstr);
	fprintf(reportFp, "Random seed:   0x%08lx\n", seed);
	if (md5mode) fprintf(reportFp, "MD5 signature: %s\n", digestStr);
	exit(0);
}	/* main */
