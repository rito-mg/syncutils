/* main.c */

#ifdef WIN32
#	include <windows.h>
#	include <stdio.h>
#	include <string.h>
#	include <stdlib.h>
#	include <time.h>
#	include <ctype.h>
#	include <share.h>
#	include "getopt.h"
#	undef UNIX
#else
#	include <unistd.h>
#	include <sys/types.h>
#	include <sys/stat.h>
#	include <stdio.h>
#	include <string.h>
#	include <stdlib.h>
#	include <time.h>
#	include <ctype.h>
#	include <dirent.h>
#	include <errno.h>
#	define UNIX 1
#	define Perror perror
#	define _int64 long long
#	define _MAX_PATH 256
#	define _fsopen(a,b,c) fopen(a,b)
#endif

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
#include "md5.h"

#define VERSION_STR "1.2 (2011-12-16)"
#define DEFAULT_MD5DIR_LOG "md5dir.log"

typedef char md5_signature[32+1];

static int checkMode;
static _int64 gTotalBytes;
static time_t gLastProgReport;
static char gMD5DirLogFileName[_MAX_PATH] = DEFAULT_MD5DIR_LOG;
static int gOneLineMode = 0;
#ifdef WIN32
static char gDefaultSumFile[_MAX_PATH], gDefaultDirToDo[16];
#endif

void ProgressReport(size_t n, int lastReport);

void ProgressReport(size_t n, int lastReport)
{
	double fracMeg;
	_int64 meg;
	time_t now;

	gTotalBytes += (_int64) n;
	time(&now);
	if ((lastReport != 0) || (now > gLastProgReport)) {
		gLastProgReport = now;
		meg = gTotalBytes / (_int64) (1024 * 1024);
		fracMeg = (double) (gTotalBytes - (meg * 1024 * 1024)) / (1024.0 * 1024.0);
#ifdef WIN32
		printf("\rProcess%s %I64d.%02d MB ", (lastReport == 0) ? "ing:" : "ed: ", meg, (int) (fracMeg * 100.0));
#else
		printf("\rProcess%s %lld.%02d MB ", (lastReport == 0) ? "ing:" : "ed: ", meg, (int) (fracMeg * 100.0));
#endif
		fflush(stdout);
	}
}	/* ProgressReport */




static void GetSignature(FILE *fp, md5_signature sig)
{
	unsigned char buf[16];
	int i;

	memset(sig, 0, sizeof(md5_signature));
	if (md5_stream(fp, buf) >= 0) {
		for (i=0; i<16; i++) {
			sprintf(sig + (i * 2), "%02lx", (unsigned long) buf[i]);
		}
	}
}	/* GetSignature */




#ifdef WIN32
static void Perror(const char *const whatFailed)
{
	char errMsg[256];

	(void) FormatMessage(
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL,
		GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		errMsg,
		sizeof(errMsg),
		NULL
	);

	(void) fprintf(stderr, "%s: %s\n", whatFailed, errMsg);
}	/* Perror */




static void CreateMD5SigForFile(FILE *ofp, const char *const fPath, WIN32_FIND_DATA *ffd)
{
	unsigned _int64 fSize;
	SYSTEMTIME st;
	md5_signature fSig;
	FILE *fp;

	fSize = (((unsigned _int64) ffd->nFileSizeHigh) << 32) | ((unsigned _int64) ffd->nFileSizeLow);

//	if (! FileTimeToLocalFileTime(&ffd->ftLastWriteTime, &lt))
//		return;

	if (! FileTimeToSystemTime(&ffd->ftLastWriteTime, &st))
		return;
 
	fp = fopen(fPath, "rb");
	if (fp == NULL) {
		Perror(fPath);
		return;
	}

	GetSignature(fp, fSig);
	fclose(fp);


	if (gOneLineMode == 0) {
		fprintf(ofp, "%s\n%s  %4hd-%02hd-%02hd %02hd:%02hd:%02hd  %I64d\n\n",
				ffd->cFileName,		// Don't want pathname in log.
				fSig,
				st.wYear,
				st.wMonth,
				st.wDay,
				st.wHour,
				st.wMinute,
				st.wSecond,
				fSize
				);
	} else {
		fprintf(ofp, "%s|%4hd-%02hd-%02hd %02hd:%02hd:%02hd|%15I64d|%s\n",
				fSig,
				st.wYear,
				st.wMonth,
				st.wDay,
				st.wHour,
				st.wMinute,
				st.wSecond,
				fSize,
				ffd->cFileName		// Don't want pathname in log.
				);
	}
}	// CreateMD5SigForFile




static void CreateMD5SigsForDir(const char *const srcpath)
{
	char pattern[_MAX_PATH], path[_MAX_PATH];
	WIN32_FIND_DATA ffd;
	HANDLE searchHandle;
	DWORD dwErr;
	char *cp;
	FILE *ofp;

	STRNCPY(pattern, srcpath);

	/* Get rid of trailing slashes. */
	cp = pattern + strlen(pattern) - 1;
	while ((cp >= pattern) && ((*cp == '\\') || (*cp == '/')))
		*cp-- = '\0';

	// printf("Entering directory %s\\\n", pattern);


	STRNCAT(pattern, "\\*.*");
	memset(&ffd, 0, sizeof(ffd));

	/* "Open" the directory. */
	searchHandle = FindFirstFile(pattern, &ffd);
	if (searchHandle == INVALID_HANDLE_VALUE) {
		Perror(srcpath);
		return;
	}

	/* Get rid of that \*.* again. */
	cp = strrchr(pattern, '\\');
	*cp = '\0';
 
	_snprintf(path, sizeof(path), "%s\\%s", pattern, gMD5DirLogFileName);

	ofp = _fsopen(path, "wt", _SH_DENYWR);
	if (ofp == NULL)
		return;

	for (;;) {
		if ((ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
			/* Directory.
			 *
			 * First time through, skip it.
			 * This isn't very efficient since we could do this
			 * in one pass, but I want to do the entire contents
			 * before proceeding to subdirectories.
			 */
			goto next1;
		}

		if ((ffd.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_TEMPORARY)) != 0) {
			/* We don't want to handle these items. */
			goto next1;
		}
	
		if (stricmp(ffd.cFileName, gMD5DirLogFileName) == 0) {
			/* Skip our log files, and our own .exe */
			goto next1;
		}

		_snprintf(path, sizeof(path), "%s\\%s", pattern, ffd.cFileName);
		CreateMD5SigForFile(ofp, path, &ffd);

next1:
#if _DEBUG
		memset(&ffd, 0, sizeof(ffd));
#endif
		if (!FindNextFile(searchHandle, &ffd)) {
			dwErr = GetLastError();
			if (dwErr != ERROR_NO_MORE_FILES) {
				Perror(srcpath);
			}
			break;
		}
	}

	FindClose(searchHandle);
	(void) fclose(ofp);

	/* Second pass. */
	STRNCPY(pattern, srcpath);

	/* Get rid of trailing slashes. */
	cp = pattern + strlen(pattern) - 1;
	while ((cp >= pattern) && ((*cp == '\\') || (*cp == '/')))
		*cp-- = '\0';
	STRNCAT(pattern, "\\*.*");

	/* "Open" the directory for the second time. */
	searchHandle = FindFirstFile(pattern, &ffd);
	if (searchHandle == INVALID_HANDLE_VALUE) {
		Perror(srcpath);
		return;
	}

	/* Get rid of that \*.* again. */
	cp = strrchr(pattern, '\\');
	*cp = '\0';

	for (;;) {
		if ((ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
			/* Not a directory, skip it. */
			goto next;
		}

		if ((ffd.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_TEMPORARY)) != 0) {
			/* We don't want to handle these items. */
			goto next;
		}

		cp = ffd.cFileName;
		if ((*cp == '.') && ((cp[1] == '\0') || ((cp[1] == '.') && (cp[2] == '\0')))) {
			/* It was "." or "..", so skip it. */
			goto next;
		}
		
		STRNCPY(pattern, srcpath);
		
		/* Get rid of trailing slashes. */
		cp = pattern + strlen(pattern) - 1;
		while ((cp >= pattern) && ((*cp == '\\') || (*cp == '/')))
			*cp-- = '\0';
		*++cp = '\\';
		*++cp = '\0';
		STRNCAT(pattern, ffd.cFileName);
		CreateMD5SigsForDir(pattern);

next:
#if _DEBUG
		memset(&ffd, 0, sizeof(ffd));
#endif
		if (!FindNextFile(searchHandle, &ffd)) {
			dwErr = GetLastError();
			if (dwErr != ERROR_NO_MORE_FILES) {
				Perror(srcpath);
			}
			break;
		}
	}
	FindClose(searchHandle);

}	/* CreateMD5SigsForDir */




static int CheckMD5SigsAgainstLogFile(FILE *ifp, const char *const reldir)
{
	char line[_MAX_PATH + 8];
	char logged[128];
	char path[_MAX_PATH];
	char *cp;
	unsigned _int64 fSize;
	DWORD fSize1, fSize2;
	SYSTEMTIME st;
	md5_signature fSig;
	FILE *fp;
	FILETIME crTime, acTime, wrTime;
	int errors = 0;

	// Read each filename and data line, and check it against the source.
	//
	// Entries look like this:
	//
	//	DIRECTX5\DIRECTX\DRIVERS\USA\ATI.VXD
	//	c6db4097ef51f71754c671b7f6452012  1997-07-14 22:00:00  27427
	//
	while (fgets(line, sizeof(line) - 1, ifp) != NULL) {
		cp = line + strlen(line) - 1;
		while (isspace(*cp) && (cp >= line))
			--cp;
		cp[1] = '\0';
		if ((line[0] == '\0') || isspace(line[0]))
			continue;

		for (cp = line; *cp != '\0'; cp++)
			if (*cp == '/')
				*cp = '\\';

		_snprintf(path, sizeof(path), "%s\\%s", reldir, line);
		if (fgets(logged, sizeof(logged) - 1, ifp) != NULL) {
			HANDLE h;
			
			h = CreateFile(path,
				GENERIC_READ,
				FILE_SHARE_READ,
				NULL,
				OPEN_EXISTING,	// fails if it doesn't exist
				FILE_ATTRIBUTE_NORMAL,
				NULL
				);
			
			if (h == INVALID_HANDLE_VALUE) {
				errors++;
				if (GetLastError() == ERROR_SHARING_VIOLATION) {
					fprintf(stderr, "\rCannot check %s: sharing violation.\n", path);
				} else if ((GetLastError() == ERROR_PATH_NOT_FOUND) || (GetLastError() == ERROR_FILE_NOT_FOUND)) {
					fprintf(stdout, "\rMissing %s:              \n    Logged: %s\n", path, logged);
				} else {
					fprintf(stdout, "\n");
					Perror(path);
				}
				goto skip;
			}

			// Exists, and wasn't open.
			//

			if (! GetFileTime(h, &crTime, &acTime, &wrTime)) {
				errors++;
				CloseHandle(h);
				Perror(path);
				goto skip;
			}

			fSize = (_int64)0;
			fSize1 = GetFileSize(h, &fSize2);
			if ((fSize1 == 0xFFFFFFFF) && (GetLastError() != NO_ERROR)) {
				errors++;
				CloseHandle(h);
				Perror(path);
				goto skip;
			}
			fSize = ((_int64) fSize2 << 32) | (_int64) fSize1;

			if (! FileTimeToSystemTime(&wrTime, &st)) {
				errors++;
				CloseHandle(h);
				Perror(path);
				goto skip;
			}

			
			CloseHandle(h);
			fp = _fsopen(path, "rb", _SH_DENYWR);
			if (fp == NULL) {
				errors++;
				Perror(path);
				goto skip;
			}
			
			GetSignature(fp, fSig);
			fclose(fp);

			if (strnicmp(logged, fSig, sizeof(md5_signature) - 1) != 0) {
				// Signatures differ!
				//
				errors++;
				cp = logged + strlen(logged) - 1;
				while (isspace(*cp))
					--cp;
				cp[1] = '\0';
				fprintf(stdout, "\rDiscrepancy for %s:       \n    Logged: %s\n    Actual: %s  %4hd-%02hd-%02hd %02hd:%02hd:%02hd  %I64d\n\n",
					path, logged,
					fSig,
					st.wYear,
					st.wMonth,
					st.wDay,
					st.wHour,
					st.wMinute,
					st.wSecond,
					fSize
					);
			}
		}
skip:
		continue;
	}

	return (errors);
}	/* CheckMD5SigsAgainstLogFile */




static int CheckMD5SigsForDir(const char *const srcpath)
{
	char pattern[_MAX_PATH], path[_MAX_PATH];
	WIN32_FIND_DATA ffd;
	HANDLE searchHandle;
	DWORD dwErr;
	char *cp;
	FILE *ifp;
	int errs;

	STRNCPY(pattern, srcpath);

	/* Get rid of trailing slashes. */
	cp = pattern + strlen(pattern) - 1;
	while ((cp >= pattern) && ((*cp == '\\') || (*cp == '/')))
		*cp-- = '\0';

	// printf("Entering directory %s\\\n", pattern);


	STRNCAT(pattern, "\\*.*");
	memset(&ffd, 0, sizeof(ffd));

	/* "Open" the directory. */
	searchHandle = FindFirstFile(pattern, &ffd);
	if (searchHandle == INVALID_HANDLE_VALUE) {
		Perror(srcpath);
		return (1);
	}

	/* Get rid of that \*.* again. */
	cp = strrchr(pattern, '\\');
	*cp = '\0';
 
	errs = 0;

	_snprintf(path, sizeof(path), "%s\\%s", pattern, gMD5DirLogFileName);

	ifp = _fsopen(path, "rt", _SH_DENYWR);
	if (ifp != NULL) {
		// There doesn't need to be a log file in every directory.
		//
		errs += CheckMD5SigsAgainstLogFile(ifp, pattern);
		(void) fclose(ifp);
	}

	for (;;) {
		if ((ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
			/* Not a directory, skip it. */
			goto next;
		}

		if ((ffd.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_TEMPORARY)) != 0) {
			/* We don't want to handle these items. */
			goto next;
		}

		cp = ffd.cFileName;
		if ((*cp == '.') && ((cp[1] == '\0') || ((cp[1] == '.') && (cp[2] == '\0')))) {
			/* It was "." or "..", so skip it. */
			goto next;
		}
		
		STRNCPY(pattern, srcpath);
		
		/* Get rid of trailing slashes. */
		cp = pattern + strlen(pattern) - 1;
		while ((cp >= pattern) && ((*cp == '\\') || (*cp == '/')))
			*cp-- = '\0';
		*++cp = '\\';
		*++cp = '\0';
		STRNCAT(pattern, ffd.cFileName);
		errs += CheckMD5SigsForDir(pattern);

next:
#if _DEBUG
		memset(&ffd, 0, sizeof(ffd));
#endif
		if (!FindNextFile(searchHandle, &ffd)) {
			dwErr = GetLastError();
			if (dwErr != ERROR_NO_MORE_FILES) {
				Perror(srcpath);
			}
			break;
		}
	}
	FindClose(searchHandle);

	return (errs);
}	/* CheckMD5SigsForDir */



static int DoDir(const char *const srcpath, int checkmode)
{
	int errs = 0;
	if (checkmode == 0)
		CreateMD5SigsForDir(srcpath);
	else
		errs = CheckMD5SigsForDir(srcpath);

	return errs;
}	/* DoDir */




static BOOL ExistsFile(LPCSTR path)
{
	HANDLE h;
	
	h = CreateFile(path,
		GENERIC_READ,
		FILE_SHARE_READ,
		NULL,
		OPEN_EXISTING,	// fails if it doesn't exist
		FILE_ATTRIBUTE_NORMAL,
		NULL
		);
				
	if (h != INVALID_HANDLE_VALUE) {
		// Exists, and wasn't open.
		//
		CloseHandle(h);
		return TRUE;
	} else if (GetLastError() == ERROR_SHARING_VIOLATION) {
		// It existed, but it was already open.
		//
		return TRUE;
	}
	return FALSE;
}	// ExistsFile
#endif	/* WIN32 */




#ifdef UNIX

static int CheckMD5SigsAgainstLogFile(FILE *const logfp, const char *const reldir)
{
	char line[_MAX_PATH + 8];
	char logged[128];
	char path[_MAX_PATH];
	char lpath[_MAX_PATH];
	char *cp;
	unsigned _int64 fSize;
	md5_signature fSig;
	FILE *fp;
	int errors = 0;
	struct stat st;
	struct tm *utm;

	/* Read each filename and data line, and check it against the source.
	//
	// Entries look like this:
	//
	//	DIRECTX5\DIRECTX\DRIVERS\USA\ATI.VXD
	//	c6db4097ef51f71754c671b7f6452012  1997-07-14 22:00:00  27427
	*/
	while (fgets(line, sizeof(line) - 1, logfp) != NULL) {
		cp = line + strlen(line) - 1;
		while (isspace(*cp) && (cp >= line))
			--cp;
		cp[1] = '\0';
		if ((line[0] == '\0') || isspace(line[0]))
			continue;

		for (cp = line; *cp != '\0'; cp++) {
			if (*cp == '\\') {
				*cp = '/';
			}
		}
		(void) snprintf(path, sizeof(path), "%s/%s", reldir, line);

		/* When reading a file generated with the
		 * Win32 version, when we can't find the
		 * file requested try lowercasing the
		 * whole pathname.
		 */
		(void) snprintf(lpath, sizeof(lpath), "%s/%s", reldir, line);
		for (cp = lpath; *cp != '\0'; cp++)
			if (isupper(*cp))
				*cp = (char) tolower(*cp);

		if (fgets(logged, sizeof(logged) - 1, logfp) != NULL) {
			if ((lstat(path, &st) < 0) && (lstat(lpath, &st) < 0)) {
				if (errno == ENOENT) {
					fprintf(stdout, "\rMissing %s:              \n    Logged: %s\n", path, logged);
				} else {
					fprintf(stdout, "\n");
					Perror(path);
				}
			}

			fSize = (_int64) st.st_size;

			utm = gmtime(&st.st_mtime);
			if (utm == NULL)
				goto skip;

			fp = fopen(path, "r");
			if (fp == NULL) {
				fp = fopen(lpath, "r");
				if (fp == NULL) {
					errors++;
					Perror(path);
					goto skip;
				}
			}
			
			GetSignature(fp, fSig);
			fclose(fp);

			if (strncasecmp(logged, fSig, sizeof(md5_signature) - 1) != 0) {
				/* Signatures differ!
				 */
				errors++;
				cp = logged + strlen(logged) - 1;
				while (isspace(*cp))
					--cp;
				cp[1] = '\0';

				(void) fprintf(stdout, "\rDiscrepancy for %s:       \n    Logged: %s\n    Actual: %s  %4hd-%02hd-%02hd %02hd:%02hd:%02hd  %lld\n\n",
					path, logged,
					fSig,
					utm->tm_year + 1900,
					utm->tm_mon + 1,
					utm->tm_mday,
					utm->tm_hour,
					utm->tm_min,
					utm->tm_sec,
					fSize
					);
			}
		}
skip:
		continue;
	}

	return (errors);
}	/* CheckMD5SigsAgainstLogFile */





static void DoItem(FILE *const logfp, const char *const fPath, const char *const fName, const struct stat *const stp)
{
	unsigned _int64 fSize;
	md5_signature fSig;
	FILE *fp;
	struct tm *utm;

	if (strcasecmp(fName, gMD5DirLogFileName) == 0)
		return;		/* Skip ourself */

	utm = gmtime(&stp->st_mtime);
	if (utm == NULL)
		return;

	fSize = (_int64) stp->st_size;

	fp = fopen(fPath, "r");
	if (fp == NULL) {
		Perror(fPath);
		return;
	}

	GetSignature(fp, fSig);
	(void) fclose(fp);

	if (gOneLineMode == 0) {
		(void) fprintf(logfp, "%s\n%s  %4hd-%02hd-%02hd %02hd:%02hd:%02hd  %lld\n\n",
				fName,
				fSig,
				utm->tm_year + 1900,
				utm->tm_mon + 1,
				utm->tm_mday,
				utm->tm_hour,
				utm->tm_min,
				utm->tm_sec,
				fSize
				);
	} else {
		(void) fprintf(logfp, "%s|%4hd-%02hd-%02hd %02hd:%02hd:%02hd|%15lld|%s\n",
				fSig,
				utm->tm_year + 1900,
				utm->tm_mon + 1,
				utm->tm_mday,
				utm->tm_hour,
				utm->tm_min,
				utm->tm_sec,
				fSize,
				fName
				);
	}
}	/* DoItem */




static int Traverse(const char *const dirPath, size_t dirPathLen, int checkmode)
{
	DIR *DIRp;
	char *cp;
	char *fnp;
	size_t pathAllocLen, fnLen;
	struct dirent *direntp;
	struct stat st;
	char *path;
	mode_t m;
	int errs;
	FILE *logfp;

	if (stat(dirPath, &st) < 0) {
		Perror(dirPath);
		return (1);
	}

	if ((DIRp = opendir(dirPath)) == NULL) {
		Perror(dirPath);
		return (1);
	}

	pathAllocLen = dirPathLen + 64;
	path = calloc(pathAllocLen, sizeof(char));
	if (path == NULL) {
		Perror("malloc");
		return (1);
	}
	memcpy(path, dirPath, dirPathLen);
	fnp = path + dirPathLen;
	*fnp++ = '/';
	dirPathLen++;

	errs = 0;
	memcpy(fnp, gMD5DirLogFileName, strlen(gMD5DirLogFileName) + 1);
	if (checkmode == 0) {
		logfp = fopen(path, "w");
		*fnp = '\0';
		if (logfp == NULL) {
			Perror(path);
			return (1);
		}
	} else {
		logfp = fopen(path, "r");
		if (logfp != NULL) {
			fnp[-1] = '\0';
			errs += CheckMD5SigsAgainstLogFile(logfp, path);
			fnp[-1] = '/';
			fclose(logfp);
		}
		*fnp = '\0';
	}

	while (1) {
		direntp = readdir(DIRp);
		if (direntp == NULL)
			break;
		cp = direntp->d_name;
		if ((cp[0] == '.') && ((cp[1] == '\0') || ((cp[1] == '.') && (cp[2] == '\0'))))
			continue;	/* Skip . and .. */

		fnLen = strlen(cp) + 1	/* include \0 */;
		if ((fnLen + dirPathLen) > pathAllocLen) {
			pathAllocLen = fnLen + dirPathLen + 32;
			path = realloc(path, pathAllocLen);
			if (path == NULL) {
				/* possible memory leak */
				return (1);
			}
			fnp = path + dirPathLen;
		}
		memcpy(fnp, cp, fnLen);

		if (checkmode == 0) {
			if (lstat(path, &st) == 0) {
				m = st.st_mode;
				if (S_ISREG(m) != 0) {
					DoItem(logfp, path, cp, &st);
				}
			}
		} else {
			if (lstat(path, &st) == 0) {
				m = st.st_mode;
				if (S_ISDIR(m)) {
					/* directory */
					errs += Traverse(path, dirPathLen + fnLen - 1, checkmode);
				}
			}
		}
	}

	if (checkmode == 0) {
		/* done adding entries to log */
		fclose(logfp);

		/* Go through again, this time doing the directories. */
		rewinddir(DIRp);

		while (1) {
			direntp = readdir(DIRp);
			if (direntp == NULL)
				break;
			cp = direntp->d_name;
			if ((cp[0] == '.') && ((cp[1] == '\0') || ((cp[1] == '.') && (cp[2] == '\0'))))
				continue;	/* Skip . and .. */

			fnLen = strlen(cp) + 1	/* include \0 */;
			if ((fnLen + dirPathLen) > pathAllocLen) {
				pathAllocLen = fnLen + dirPathLen + 32;
				path = realloc(path, pathAllocLen);
				if (path == NULL) {
					/* possible memory leak */
					return (1);
				}
				fnp = path + dirPathLen;
			}
			memcpy(fnp, cp, fnLen);

			if (lstat(path, &st) == 0) {
				m = st.st_mode;
				if (S_ISDIR(m)) {
					/* directory */
					errs += Traverse(path, dirPathLen + fnLen - 1, checkmode);
				}
			}
		}
	}

	(void) closedir(DIRp);
	free(path);

	return (errs);
}	/* Traverse */




static int DoDir(const char *const srcpath, int checkmode)
{
	return Traverse(srcpath, strlen(srcpath), checkmode);
}	/* DoDir */

#endif /* UNIX */




int main(int argc, char **argv)
{
	int c;
	const char *dirToDo;
	int errors;

	checkMode = 1;
	gTotalBytes = 0;
	gLastProgReport = 0;

	GetoptReset();
	while ((c = Getopt(argc, argv, "mf:1")) > 0) switch (c) {
		case 'm':
			checkMode = 0;
			break;
		case '1':
			/* This is half-baked at the moment...
			 * It can write md5dir.log file in one line format
			 * but is not able to use this format in verify
			 * mode.
			 */
			gOneLineMode = 1;
			break;
		case 'f':
			STRNCPY(gMD5DirLogFileName, gOptArg);
			break;
		default:
			goto usage;
	}

	if (gOptInd != (argc - 1)) {
usage:
		fprintf(stderr, "Usage:  md5dirs [-m] directory\n");
		fprintf(stderr, "Version: %s\n\n", VERSION_STR);
		fprintf(stderr, "The purpose of this program is to be able to verify that the contents of a\n");
		fprintf(stderr, "directory are not corrupt.  To do this, you run this program against your\n");
		fprintf(stderr, "master source to create MD5 signatures of each file, and then you use this\n");
		fprintf(stderr, "signature log to check your copies against the original signatures.\n\n");
		fprintf(stderr, "Do \"md5dirs -m dir\" when you want to generate the MD5 log file for each,\n");
		fprintf(stderr, "subdirectory, and do just \"md5dirs dir\" when you want to verify the files\n");
		fprintf(stderr, "against each subdirectory's log file.\n\n");
		exit(1);
	} else {
		dirToDo = argv[gOptInd];
	}

	errors = DoDir(dirToDo, checkMode);
	ProgressReport(0, 1);
	printf("\n");

	if (checkMode != 0) {
		if (errors == 0)
			printf("Done.  All files verified successfully.\n");
		else
			printf("Done.  %d error%s detected.\n", errors, (errors == 1) ? "" : "s");
	}
	exit(0);
}	/* main */
