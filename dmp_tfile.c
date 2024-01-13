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
	int reclen, volCount=0, hdrCount=1, showHeaders = 1, simh = 0, interCount, interLen=0;

	while ( (cc = getopt(argc, argv, "nI")) != EOF )
	{
		switch (cc)
		{
		case 'n':
			showHeaders = 0;
			break;
        case 'I':
            simh = 1;
            break;
		default:
			printf("Unrecognised option: '%c'.\n", cc);
		}
	}
	if ( optind >= argc )
	{
		printf("Usage: dmp_tfile [-nI] filename.\n"
                "Where:\n"
                "-n  - Don't show just headers. Instead show everything\n"
                "-I  - file is SIMH format\n"
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
			perror("Error reading record count bytes.");
			break;
		}
		if ( showHeaders )
		{
			if ( interLen != 0 && reclen != interLen && interCount )
			{
				printf("%s\t<%5d records of %6d bytes>.\n", volCount ? "\t" : "", interCount, interLen);
				interCount = 0;
			}
			interLen = reclen;
			if ( reclen == 80 )
			{
				char *cp, ascBuf[82];
				sts = read(fd,ascBuf,reclen);
				if ( sts != reclen )
				{
					perror("Error reading HDR record.");
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
				continue;
			}
			if ( reclen == 0 )
			{
				printf("%s\tTape mark.\n", volCount?"\t":"");
				continue;
			}
            if ( simh )
            {
		        sts = read(fd, &reclen, sizeof(reclen));
		        if ( sts != reclen )
		        {
			        perror("Error reading trailing record count bytes.");
			        break;
		        }
            }
			++interCount;
/*			printf("%s\t%6d bytes.\n", volCount ? "\t" : "", reclen); */
		}
		else
		{
			if ( reclen == 0 )
				printf("Tape mark.\n");
			else
				printf("Record of %6d bytes.\n", reclen);
		}
        if ( reclen && simh )
            reclen += 4;
		lseek(fd, reclen, SEEK_CUR);
	}
	close(fd);
	return 0;
}
