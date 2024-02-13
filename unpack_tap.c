#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

#include	<stdio.h>
#include	<errno.h>
#include	<unistd.h>
#include	<fcntl.h>
#include	<ctype.h>
#include	<string.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<getopt.h>
#include	<time.h>
#include	<utime.h>
#include	<stdint.h>

/* File to unpack a .TAP file to raw binary'
 *
 * Usage: unpack_tap -o outfilename infilename
 *
 * As written, only works on a little endian machine.
 */

/* The S_xxx defines are apparently not available in Fedora 12 */
#define FILE_MODE (0660) /* S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP */

static unsigned char inBuff[65536];
static char *lclFileName;
static int fNameLen;
static const char *outFileName;

static void computeNewFilename(int fileNumber)
{
	const char *tarPtr;
	static const char LcTar[]=".tar", UcTar[]=".TAR";
	const char *whichTar;

#define FN_BUFF_LEN (32)
	if ( !fileNumber || !lclFileName )
	{
		fNameLen = strlen(outFileName) + FN_BUFF_LEN;
		lclFileName = (char *)malloc(fNameLen);
		if ( !lclFileName )
		{
			fprintf(stderr,"Out of memory allocating %d bytes for filename\n", fNameLen);
			exit(1);
		}
		strncpy(lclFileName, outFileName, fNameLen);
		return;
	}
	tarPtr = strrchr(outFileName,'.');
	lclFileName[0] = 0;
	if ( tarPtr )
	{
		whichTar = NULL;
		if ( !strcmp(tarPtr,LcTar) )
			whichTar = LcTar;
		else if ( !strcmp(tarPtr,UcTar) )
			whichTar = UcTar;
		if ( whichTar )
		{
			int cLen = tarPtr-outFileName;
			strncpy(lclFileName,outFileName,cLen);
			snprintf(lclFileName+cLen, FN_BUFF_LEN, "-%d%s", fileNumber, whichTar);
		}
	}
	if ( !lclFileName[0] )
		snprintf(lclFileName, fNameLen, "%s-%d", outFileName, fileNumber);
}

int main(int argc, char *argv[])
{
	int cc, sts, err=0, fd, ofd = -1, tapeMark=0, fileNumber=0;
	int lastRecLen=0, lastRecCnt=0;
	uint32_t reclen, tailRecLen;
	int help=0, rcdCnt=0, simh = 1, dontStop=1, bogusRecord;
#if 0
	int isLittle=0, intSizeGood=0, ptrSizeGood=0;
	union
	{
		int val;
		unsigned char chr[4];
	} endianTst;

	endianTst.val = 0;
	endianTst.chr[0] = 1;
	isLittle = endianTst.val == 1 ? 1 : 0;
	intSizeGood = (int)sizeof(int) == 4 ? 1 : 0;
	ptrSizeGood = (int)sizeof(char *) == 8 ? 1 : 0;
	if ( !isLittle || !intSizeGood || !ptrSizeGood )
	{
		fprintf(stderr, "Sorry. This program must be compiled on a system where:"
				"\tExpected endian is little. Currently is %s\n"
				"\tExpected sizeof(int) == 4. Currently is %ld\n"
				"\tExpected sizeof(char *) == 8. Currently is %ld\n",
				isLittle ? "little":"big",
				sizeof(int),
				sizeof(char *)
				);
		return 1;
	}
#endif
	while ( (cc = getopt(argc, argv, "o:")) != EOF )
	{
		switch (cc)
		{
		case 'o':
			outFileName = optarg;
			break;
		default:
			help = 1;
			break;
		}
		if ( help )
			break;
	}
	if ( help || optind >= argc )
	{
		fprintf(stderr,"Usage: unpack_tap [-o outout] filename.\n"
                "Where:\n"
			    "-o file - specify output file name\n"
                );
		return 1;
	}
	fd = open(argv[optind], O_RDONLY|O_LARGEFILE);
	if ( fd < 0 )
	{
		perror("Unable to open input.\n");
		return 2;
	}
#if 0
	if ( !outFileName )
	{
		printf("FYI: sizeof(int)=%ld, sizeof(long)=%ld, sizeof(char *)=%ld\n", sizeof(int), sizeof(long), sizeof(char *));
	}
#endif
	for ( ; 1; ++rcdCnt )
	{
		sts = read(fd, &reclen, sizeof(reclen));
		if ( sts == 0 )
		{
			if ( !outFileName && lastRecCnt )
				printf("Found %d %srecords of %d bytes\n", lastRecCnt, lastRecLen == 65535 ? "BOGUS ":"", lastRecLen);
			err = 1;
			fprintf(stderr,"Found EOF on input file.\n");
			break;
		}
		if ( sts < 0 )
		{
			perror("Error reading record count bytes.");
			err = 1;
			break;
		}
		if ( reclen > (int)sizeof(inBuff) )
		{
			fprintf(stderr,"Record %d has size of %d which is too big for input buffer of %d.\nThis file does not consist of a SIMH tape format image.\n",
					rcdCnt,
					reclen,
					(int)sizeof(inBuff));
			err = 1;
			break;
		}
		bogusRecord = reclen == 65535 ? 1 : 0;
		if ( !outFileName )
		{
			if ( lastRecLen != reclen )
			{
				if ( lastRecCnt )
				{
					printf("Found %d %srecords of %d bytes\n", lastRecCnt, lastRecLen == 65535 ? "BOGUS ":"", lastRecLen);
					lastRecCnt = 0;
				}
				else
					++lastRecCnt;
			}
			else
				++lastRecCnt;
			lastRecLen = reclen;
		}
		tapeMark <<= 1;
		if ( reclen == 0 )
		{
			tapeMark |= 1;
			if ( ofd >= 0 )
			{
				close(ofd);
				ofd = -1;
			}
			if ( (tapeMark & 3) == 3 )
			{
				if ( !outFileName )
					printf("Found EOT (double tape mark) at record %d.\n", rcdCnt);
				if ( !dontStop )
					break;
			}
			else if ( !outFileName )
			{
				printf("Found a tape mark at record %d\n", rcdCnt);
			}
			lastRecLen = 0;
			lastRecCnt = 0;
			continue;
		}
		if ( !bogusRecord )
		{
			sts = read(fd, inBuff, reclen);
			if ( sts != reclen )
			{
				fprintf(stderr,"Failed to read record %d of %d bytes. Got %d instead: %s\n", rcdCnt, reclen, sts, strerror(errno));
				err = 1;
				break;
			}
			if ( outFileName )
			{
				if ( ofd < 0 )
				{
					computeNewFilename(fileNumber);
					ofd = creat(lclFileName,FILE_MODE);
					if ( ofd < 0 )
					{
						fprintf(stderr,"Failed to open '%s' for output: %s\n", lclFileName, strerror(errno));
						close(fd);
						return 1;
					}
					++fileNumber;
				}
				sts = write(ofd, inBuff, reclen);
				if ( sts != (int)reclen )
				{
					fprintf(stderr,"Error writing %d byte record %d to output: %s\n", reclen, rcdCnt, strerror(errno));
					err = 1;
					break;
				}
			}
		}
		if ( simh )
		{
			sts = read(fd, &tailRecLen, sizeof(tailRecLen));
			if ( !sts )
			{
				err = 1;
				break;
			}
			if ( sts < 0 )
			{
				perror("Error reading tail of record count bytes.");
				err = 1;
				break;
			}
			if ( reclen != tailRecLen )
			{
				fprintf(stderr,"Trailing record length on record %d doesn't match. Expected %d, got %d\n", rcdCnt, reclen, tailRecLen);
				err = 1;
				break;
			}
		}
	}
	close(fd);
	if ( ofd >= 0 )
		close(ofd);
	return err;
}
