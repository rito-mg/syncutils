/* waitfor.c */

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
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <signal.h>

#include "getopt.h"

#ifndef PRINTF_LONG_LONG
#	define PRINTF_LONG_LONG "%lld"
#endif

#define kKilobyte 1024
#define kMegabyte (kKilobyte * 1024)
#define kGigabyte ((long) kMegabyte * 1024L)
#define kTerabyte ((double) kGigabyte * 1024.0)

typedef struct FInfo {
	int fd;
	off_t curSize;
	off_t prevSize;
	off_t expectedSize;
	struct stat st;
	char pathname[512];
} FInfo;

int numFiles = 0;
FInfo *finfo = 0;

struct timeval t0, t1;
int sleep_amt = 2000000;
int createMode = 1;
int verbose = 1;



static void
Usage(void)
{
	fprintf(stderr, "Usage: waitfor [-c|-d] [-options] 'file1[,size1]' [files...]\n");
	fprintf(stderr, "\nOptions:\n");
	fprintf(stderr, "  -d     : Wait for files to be deleted rather than created.\n");
	fprintf(stderr, "  -c     : Wait for files to be created [to size] (default mode).\n");
	fprintf(stderr, "  -a XX  : Abort after XX seconds.\n");
	fprintf(stderr, "  -w X.Y : Wait X.Y seconds between checks (default: %.2f sec).\n", (double) sleep_amt * 0.000001);
	fprintf(stderr, "  -v     : Verbose messages.\n");
	fprintf(stderr, "  -q     : No messages.\n");
	fprintf(stderr, "  -l XX  : Append to log file XX with results.\n");
	fprintf(stderr, "  -D X   : Delimit pathnames and expected sizes by character X rather than a comma.\n");
	fprintf(stderr, "\nExamples:\n");
	fprintf(stderr, "  waitfor -c -w 0.5 -a 120 /tmp/bigfile,100M /tmp/file2,32768\n");
	fprintf(stderr, "  waitfor -d -w 0.5 -a 120 /tmp/bigfile /tmp/file2\n");
	fprintf(stderr, "\nVersion:\n  1.0 * 2014-04-02 * Gleason\n");
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




int
main(int argc, const char **argv)
{
	int i = 0, c = 0, polls = 0;
	int ndone = 0;
	double fm;
	const char *fu;
	char dstr[64], tstr[64], *cp = NULL;
	int delim = ',';
	FInfo *fip = NULL;
	int bored = 0;
	unsigned int alarm_amt = 0;
	FILE *logfp = NULL;

	setvbuf(stdout, NULL, _IOLBF, 0);
	memset(dstr, 0, sizeof(dstr));
	memset(tstr, 0, sizeof(tstr));
	memset(&t0, 0, sizeof(t0));
	memset(&t1, 0, sizeof(t1));

	GetoptReset();
	while ((c = Getopt(argc, argv, "a:cdD:l:w:vq")) > 0) switch(c) {
		case 'a':
			if (atoi(gOptArg) >= 2) {
				alarm_amt = atoi(gOptArg);
			}
			break;
		case 'q':
			verbose = 0;
			break;
		case 'v':
			verbose++;
			break;
		case 'c':
			createMode = 1;
			break;
		case 'd':
			createMode = 0;
			break;
		case 'D':
			delim = (int) gOptArg[0];
			break;
		case 'l':
			logfp = fopen(gOptArg, "a");
			if (logfp == NULL) {
				perror(gOptArg);
				exit(1);
			}
			setvbuf(logfp, NULL, _IOLBF, 0);
			break;
		case 'w':
			if (atof(gOptArg) > 0) {
				sleep_amt = (unsigned int) (atof(gOptArg) * 1000000.0);
			}
			break;
		default:
			Usage();
	}

	numFiles = (int) (argc - gOptInd);
	if (numFiles == 0)
		Usage();

	finfo = (FInfo *) calloc((size_t) (numFiles + 1), sizeof(FInfo));
	if (finfo == NULL) {
		(void) fprintf(stderr, "could not allocate %lu bytes of memory: %s\n", (unsigned long) sizeof(FInfo) * (size_t) (numFiles + 1), strerror(errno));
		exit(4);
	}

	for (i=0, fip = finfo; i<numFiles; i++, fip++) {
		fip->fd = 0;
		strncpy(fip->pathname, argv[gOptInd+i], sizeof(fip->pathname) - 1);
		cp = strchr(fip->pathname, delim);
		fip->prevSize = -1LL;
		fip->curSize = -1LL;
		if (cp != NULL) {
			*cp++ = '\0';
			fip->expectedSize = unit_atoll(cp);
		}
	}

	if (alarm_amt != 0) {
		alarm(alarm_amt);
	}

	(void) signal(SIGPIPE, SIG_IGN);
	gettimeofday(&t0, NULL);

	for (polls = 0; ndone < numFiles; polls++) {
		if (polls > 0) {
			if ((++bored == 1) || (verbose >= 2))
				printf("Waiting for %d file%s...\n", (numFiles - ndone), (numFiles - ndone) == 1 ? "" : "s");
			usleep(sleep_amt);
		}

		ndone = 0;
		for (i=0, fip = finfo; i<numFiles; i++, fip++) {
			if (createMode) {
				if (fip->fd <= 0) {
					/* File hadn't been created yet */
					fip->fd = open(fip->pathname, O_RDONLY);
				}
				if (fip->fd <= 0) {
					/* File hasn't been created yet */
				} else if (fstat(fip->fd, &fip->st) < 0) {
					fprintf(stderr, "Could not fstat fd %d for %s: %s\n", fip->fd, fip->pathname, strerror(errno));
					exit(1);
				} else {
					fip->curSize = (off_t) fip->st.st_size;
				}
				if (fip->prevSize < fip->curSize) {
					/* File size has increased since last check */
					fip->prevSize = fip->curSize;
					fm = FileSize((double) fip->curSize, &fu, NULL);
					if (fip->curSize >= fip->expectedSize) {
						ndone++;
						if (verbose) printf("%c %s: %.2f %s\n", '=', fip->pathname, fm, fu);
						gettimeofday(&t1, NULL);
						(void) strftime(tstr, sizeof(tstr) - 1, "%Y-%m-%d %H:%M:%S", localtime(&t1.tv_sec));
						if (logfp) fprintf(logfp, "%s + %7.2f " PRINTF_LONG_LONG " %s\n", tstr, Duration2(&t0, &t1), (long long) fip->curSize, fip->pathname);
					} else {
						if (verbose) printf("%c %s: %.2f %s\n", '+', fip->pathname, fm, fu);
					}
				} else if (fip->curSize >= fip->expectedSize) {
					if (fip->fd > 0)
						ndone++;
				} else {
					/* File size has not changed */	
				}
			} else {
				if (stat(fip->pathname, &fip->st) == 0) {
					/* File exists */
					fip->curSize = (off_t) fip->st.st_size;
				} else if (errno == ENOENT) {
					/* File does not exist */
					ndone++;
					if (fip->prevSize == -1LL) {
						/* File does not exist now, but did the last time */
						fm = FileSize((double) fip->curSize, &fu, NULL);
						if (verbose) printf("%c %s: %.2f %s\n", '-', fip->pathname, fm, fu);
						fip->prevSize = fip->curSize;
						gettimeofday(&t1, NULL);
						(void) strftime(tstr, sizeof(tstr) - 1, "%Y-%m-%d %H:%M:%S", localtime(&t1.tv_sec));
						if (logfp) fprintf(logfp, "%s - %7.2f " PRINTF_LONG_LONG " %s\n", tstr, Duration2(&t0, &t1), (long long) fip->curSize, fip->pathname);
					} else {
						/* File still does not exist */
					}
				} else {
					fprintf(stderr, "Could not stat %s: %s\n", fip->pathname, strerror(errno));
					exit(1);
				}
			}
		}
	}

	alarm(0);
	gettimeofday(&t1, NULL);
	(void) strftime(tstr, sizeof(tstr) - 1, "%Y-%m-%d %H:%M:%S", localtime(&t1.tv_sec));
	if (logfp) fprintf(logfp, "%s %c %s %s\n", tstr, createMode ? '+' : '-', ElapsedStr(Duration2(&t0, &t1), dstr, sizeof(dstr)), "*");

	printf("Done at %s. Elapsed Time = %s.\n", tstr, ElapsedStr(Duration2(&t0, &t1), dstr, sizeof(dstr)));

	if (logfp) fclose(logfp);
	exit(0);
}	/* main */
