#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <getopt.h>

/* Program used to convert a .data format file to simh format. */

/* Usage: data2simh input output */

/*
 * The input file is expected to be have variable length records:
 * 4 bytes, little endian, record byte count
 * n bytes of data.
 *
 * The output file will contain variable length records:
 * 4 bytes, little endia, record byte count
 * n bytes of data.
 * 4 bytes, little endian, record byte count
 *
 * A byte count of 0 indicates a tape mark and there will be only
 * one instance of a tape mark.
 * Two tape marks in a row indicates an end of file.
 */

static char buff[128*1024];

static int help_em(const char *imageName)
{
	fprintf(stderr, "Usage: %s [-v][-l n] <input_path> <output_path>\n"
			"Where:\n"
			"<input_path>  - points to input file\n"
			"<output_path> - points to output file\n"
			"-l n          - set the number of records between report (default=256)\n"
			"-v            - set verbose mode\n"
			,imageName);
	return 1;
}

int main(int argc, char *argv[])
{
	int sts, infd, outfd;
	int verbose=0, opt, tape_marks = 0, records=0;
	int recordLimit=256;
	unsigned long total = 0;
	const char *src, *dst, *imgName;
	
	if ( sizeof(int) != 4 )
	{
		fprintf(stderr, "This program has to be compiled such that sizeof(int) == 4. Currently is %d\n", (int)sizeof(int));
		return 1;
	}
	while ( (opt = getopt(argc, argv, "l:v")) != -1 )
	{
		switch (opt)
		{
		case 'l':
			recordLimit = atoi(optarg);
			verbose = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		default: /* '?' */
			return help_em(argv[0]);
		}
	}
	imgName = strrchr(argv[0],'/');
	if ( !imgName )
		imgName = argv[0];
	else
		++imgName;
	printf("%s version 1.0\n", imgName);
	if ( optind >= argc-1  )
		return help_em(imgName);
	src = argv[optind];
	dst = argv[optind+1];
	infd = open(src, O_RDONLY);
	if ( infd < 0 )
	{
		fprintf(stderr, "Unable to open %s: %s\n", src, strerror(errno));
		return 1;
	}
	outfd = creat(dst, 0664);
	if ( outfd < 0 )
	{
		fprintf(stderr, "Unable to open %s: %s\n", dst, strerror(errno));
		return 5;
	}
	while ( 1 )
	{
		int bcnt;
		char hdr[50];
		sts = read(infd, &bcnt, sizeof(bcnt));
		if ( sts != (int)sizeof(bcnt) )
		{
			fprintf(stderr, "Error reading %s: %s\n", src, strerror(errno));
			return 2;
		}
		if ( bcnt > 32767 )
		{
			fprintf(stderr, "Error in %s: Record length of %d is too big.\n", src, bcnt);
			return 3;
		}
		sts = write(outfd, &bcnt, sizeof(bcnt));
		if ( (sts != (int)sizeof(bcnt)) )
		{
			fprintf(stderr, "Failed to write leading record byte count to output, sts=%d (s/b 4), bcnt=%d, errno: %s\n",
					sts, bcnt, strerror(errno));
			exit(1);
		}
		if ( bcnt )
		{
			sts = read(infd, buff, bcnt);
			if ( sts < 0 || sts != bcnt)
			{
				fprintf(stderr, "Error reading %s: %s\n", src, strerror(errno));
				return 2;
			}
			total += sts;
			if ( sts == 80 )
			{
				int ii;
				memcpy(hdr, buff, sizeof(hdr) - 1);
				for ( ii = 0; ii < sizeof(hdr) - 1; ++ii )
				{
					if ( !isprint(hdr[ii]) )
						hdr[ii] = '.';
				}
				hdr[(int)sizeof(hdr) - 1] = 0;
				printf("Read %6d bytes: \"%s\"\n", sts, hdr);
			}
			sts = write(outfd, buff, bcnt);
			if ( bcnt != sts )
			{
				fprintf(stderr, "Failed to write record data to output. sts=%d, bcnt=%d (they should match) errno: %s\n",
						sts, bcnt, strerror(errno));
				exit(1);
			}
			sts = write(outfd, &bcnt, sizeof(bcnt));
			if ( (sts != (int)sizeof(bcnt)) )
			{
				fprintf(stderr, "Failed to write trailing record byte count to output, sts=%d (s/b 4), bcnt=%d, errno: %s\n",
						sts, bcnt, strerror(errno));
				exit(1);
			}
			++records;
			if ( verbose && !(records % recordLimit) )
				printf("Record count so far: %d\n", records);
		}
		else
			printf("Found tape mark\n");
		tape_marks <<= 1;
		if ( !bcnt )
		{
			tape_marks |= 1;
			if ( (tape_marks & 3) == 3 )  /* two tape marks in a row is EOT */
			{
				break;
			}
		}
	}
	close(infd);
	close(outfd);
	printf("Read a total of %ld bytes, %d records\n", total, records);
	return 0;
}
