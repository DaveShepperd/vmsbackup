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

/* Extract a tape save set from a 'tape image file'
 * created with cp_tape.
 *
 * Usage: ext_tfile [options] -o outfilename infilename
 *
 * As written, only works on a little endian machine.
 */

static unsigned char inpBuf[65536];

int main(int argc, char *argv[])
{
	int cc, sts, fd= -1, ofd= -1, found=0;
	int reclen, showHeaders=0, simhOut=0;
	const char *ssName=NULL, *outFname=NULL;
	char ascBuf[82];
	             /*                  00000000001111111111222222222233333333334444444444555555555566666666667777777777*/
	             /*                  01234567890123456789012345678901234567890123456789012345678901234567890123456789*/
	static const char volData[81] = "VOLEXTTFILE                                                                    3";
	
	while ( (cc = getopt(argc, argv, "Ihx:o:")) != EOF )
	{
		switch (cc)
		{
		case 'h':
			++showHeaders;
			break;
		case 'x':
			ssName = optarg;
			break;
		case 'o':
			outFname = optarg;
			break;
		case 'I':
			simhOut = 1;
			break;
		default:
			printf("Unrecognised option: '%c'.\n", cc);
		}
	}
	if ( optind >= argc || !ssName || !outFname )
	{
		printf("Usage: ext_tfile [-Ih] -x setName -o outFileName inpFilename\n");
		printf("Where:\n"
			   "-h = this message\n"
			   "-I = write output in simh tape format (.tap file format)\n"
			   "-x ssname = specify the saveset name\n"
			   "-o outfile = output file name\n"
			   "inpFilename = name of input file\n"
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
		/* First search for HDR1 records. Ignoring tape marks */
		sts = read(fd, &reclen, sizeof(reclen));
		if ( !sts )
			break;
		if ( sts < 0 )
		{
			perror("Error reading input.");
			break;
		}
		if ( reclen == 80 )
		{
			              /*                  00000000001111111111222222222233333333334444444444555555555566666666667777777777*/
			              /*                  01234567890123456789012345678901234567890123456789012345678901234567890123456789*/
/*			static const char SampleHdr1[] = "HDR1EMPIRE.          RIVERA00010001000100 85100 00000 000000DECVMSBACKUP"; */
			char *cp,tmpName[18];
			sts = read(fd,ascBuf,reclen);
			if ( sts != reclen )
			{
				perror("Error reading HDR record.");
				break;
			}
			memcpy(tmpName,ascBuf+4,17);
			cp = tmpName;
			while ( cp < tmpName+17 && *cp != ' ' )
				++cp;
			*cp = 0;
			if ( !strncmp(ascBuf,"HDR1",4) && !strncmp(tmpName,ssName,17) )
			{
				found = 1;
				break;
			}
			continue;
		}
		lseek(fd, reclen, SEEK_CUR);
	}
	if ( found )
	{
		ofd = creat(outFname,0666);
		if ( ofd < 0 )
		{
			perror("Failed to create output file");
			close(fd);
			return 1;
		}
		/* Write out a dummy VOL record */
		reclen = 80;
		write(ofd,&reclen,sizeof(reclen));	/* Length */
		write(ofd,volData,reclen);			/* data */
		if ( simhOut )
			write(ofd,&reclen,sizeof(reclen));	/* trailing length */
		/* Write the HDR1 record */
		write(ofd,&reclen,sizeof(reclen));	/* length */
		write(ofd,ascBuf,reclen);			/* data */
		if ( simhOut )
			write(ofd,&reclen,sizeof(reclen));	/* trailing length */
		while ( 1 )
		{
			int odd;
			/* Read and write until we find an EOF2 record */
			/* get record length */
			sts = read(fd, &reclen, sizeof(reclen));
			if ( !sts )
				break;
			if ( sts < 0 )
			{
				perror("Error reading input.");
				break;
			}
			/* write record length */
			sts = write(ofd, &reclen, 4);
			if ( sts != 4 )
			{
				perror("Error writing record length");
				break;
			}
			if ( !reclen )
			{
				/* Wrote a tape mark */
				continue;
			}
			/* read record */
			sts = read(fd, inpBuf, reclen);
			if ( sts != reclen )
			{
				perror("Error reading data");
				break;
			}
			/* write record */
			odd = reclen&1;
			if ( odd )
				inpBuf[reclen] = 0;
			sts = write(ofd, inpBuf, reclen + odd);
			if ( sts != reclen+odd )
			{
				perror("Error writing data");
				break;
			}
			if ( simhOut )
			{
				sts = write(ofd, &reclen, 4);	/* write trailing length */
				if ( sts != 4 )
				{
					perror("Error writing trailing record length");
					break;
				}
			}
			if ( reclen == 80 && !strncmp((const char *)inpBuf, "EOF2", 4) )
			{
				reclen = 0;
				/* Write 3 consequitive tape marks */
				sts = write(ofd, &reclen, 4);
				sts = write(ofd, &reclen, 4);
				sts = write(ofd, &reclen, 4);
				break;
			}
		}
	}
	if ( ofd >= 0 )
		close(ofd);
	if ( fd >= 0 )
		close(fd);
	return 0;
}
