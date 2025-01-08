#include	<stdio.h>
#include	<unistd.h>
#include	<fcntl.h>
#include	<ctype.h>
#include	<string.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<getopt.h>
#include	<time.h>
#include	<utime.h>
#include	<errno.h>

/* File to dump the record lengths of a 'tape image file'
 * created with cp_tape.
 *
 * Usage: dmp_tfile infilename
 *
 * As written, only works on a little endian machine.
 */

int main(int argc, char *argv[])
{
	int cc, sts, fd;
	int reclen, volCount=0, hdrCount=1, showHeaders = 1;
	int simh = 0, interCount=0, interLen=0, verbose=0;

	while ( (cc = getopt(argc, argv, "nIv")) != EOF )
	{
		switch (cc)
		{
		case 'n':
			showHeaders = 0;
			break;
        case 'I':
            simh = 1;
            break;
		case 'v':
			verbose = 1;
			break;
		default:
			printf("Unrecognised option: '%c'.\n", cc);
			return 1;
		}
	}
	if ( optind >= argc )
	{
		printf("Usage: dmp_tfile [-nIv] filename.\n"
                "Where:\n"
                "-n  - Don't show just headers. Default is show everything.\n"
                "-I  - file is SIMH format\n"
			    "-v  - verbose mode\n"
                );
		return 1;
	}
	fd = open(argv[optind], O_RDONLY);
	if ( fd < 0 )
	{
		perror("Unable to open input.\n");
		return 2;
	}
	while ( 1 )
	{
		sts = read(fd, &reclen, sizeof(reclen));
		if ( !sts )
			break;
		if ( sts < 0 )
		{
			fprintf(stderr,"Error reading record count bytes. Expected %d, got %d: %s\n", sizeof(reclen), sts, strerror(errno));
			break;
		}
		if ( reclen > 65535 )
		{
			fprintf(stderr,"Fatal error decoding file. Record count of 0x%X is > 0xFFFF which is illegal. Corrupt? (sizeof(int)=%d)\n",
					reclen, sizeof(reclen));
			close(fd);
			return 1;
		}
		if ( showHeaders )
		{
			if ( interLen != 0 && reclen != interLen && interCount )
			{
				printf("%s\t<%5d record%s of %6d bytes>.\n", volCount ? "\t" : "", interCount, interCount == 1 ? " ":"s", interLen);
				interCount = 0;
			}
			interLen = reclen;
			if ( reclen == 0 )
			{
				printf("%s\tTape mark.\n", volCount?"\t":"");
				continue;
			}
			if ( reclen == 80 )
			{
				char *cp, ascBuf[82];
				sts = read(fd,ascBuf,reclen);
				if ( sts != reclen )
				{
					fprintf(stderr,"Error reading VOL/HDR/EOF record. Expected %d, got %d: %s\n", reclen, sts, strerror(errno));
					break;
				}
				cp = ascBuf;
				while ( sts > 0 )
				{
					if ( !isprint(*cp) )
						*cp = '.';
					++cp;
					--sts;
				}
				*cp = 0;
				if ( !strncmp(ascBuf,"VOL",3) )
				{
					printf("%4d: %s\n", volCount, ascBuf);
					++volCount;
				}
				else if ( !strncmp(ascBuf,"HDR1",4) )
				{
					printf("%s%4d: %s\n", volCount?"\t":"", hdrCount, ascBuf);
					++hdrCount;
				}
				else
					printf("%s      %s\n", volCount?"\t":"", ascBuf);
			}
			else
			{
				int odd = reclen&1;
				lseek(fd, reclen+odd, SEEK_CUR);
			}
            if ( simh )
            {
				int lastRecLen;
		        sts = read(fd, &lastRecLen, sizeof(lastRecLen));
		        if ( sts != sizeof(reclen) )
		        {
			        fprintf(stderr,"Error reading trailing record count bytes. Expected %d bytes, got %d: %s\n",
							sizeof(reclen), sts, strerror(errno));
			        break;
		        }
				if ( lastRecLen != reclen )
				{
					fprintf(stderr,"Error trailing record length does not match header. Expected 0x%X, found 0x%X\n",
							reclen, lastRecLen);
					break;
				}
            }
			++interCount;
			if ( verbose )
				printf("%s\t%6d bytes.\n", volCount ? "\t" : "", reclen);
			continue;
		}
		if ( reclen == 0 )
		{
			printf("Tape mark.\n");
		}
		else
		{
			printf("Record of %6d bytes.\n", reclen);
			if ( reclen && simh )
				reclen += 4;
			lseek(fd, reclen, SEEK_CUR);
		}
	}
	close(fd);
	return 0;
}
