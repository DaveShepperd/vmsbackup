/* ext_tfile.c - extract a saveset from a .data file */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>

static int verbose;
static int simhMode;
static char buff[65536];
#define MAX_SSNAME_LEN (17)
static char ssname[MAX_SSNAME_LEN+1];
static int ssnamelen;
/*                           00000000001111111111222222222233333333334444444444555555555566666666667777777777 */
/*                           01234567890123456789012345678901234567890123456789012345678901234567890123456789 */
static char vol_label[81];
static char hdr1[81], hdr2[81];

static void help_em( FILE *opf, const char *title )
{
	fprintf(opf, "Usage: %s [-sv] ss_name datafile\n"
			"Extracts saveset 'ss_name' from 'datafile' into <ssname>[.data|.simh]\n"
			"or if ss_name is a number 1<=num<=99, extracts num HDR1's entry in 'datafile' into <num>_<name>[.data|.simh]\n"
			"Where:\n"
			"-s       means make output simh format (selects output file extension as .simh)\n"
			"-v       set verbose\n"
			"ss_name  is the saveset name to extract or a number between 1 and 99\n"
			"datafile filename of .data file\n"
			,title);
}

static int last_reclen= -1, reccnt=0;

static int write_rcd( FILE *outp, char *bufp, int bc, const char *msg )
{
    int retv, outv;

    if ( verbose )
    {
        if ( bc == 0 || bc == 80 || bc != last_reclen )
        {
            if ( reccnt )
            {
                printf( "Info: Wrote %5d records of %5d bytes.\n", reccnt, last_reclen );
            }
            if ( !bc )
                printf( "Info: Wrote %s\n", msg ? msg : "tape mark" );
            if ( bc == 80 )
            {
                char tb[22];
                memcpy(tb,bufp,21);
                tb[21] = 0;
                printf( "Info: Wrote 80 byte record: '%s'\n", tb );
            }
            last_reclen = bc;
            reccnt = 0;
        }
        else
        {
            ++reccnt;
        }
    }
    retv = fwrite( &bc, 1, sizeof(bc), outp );
    if ( retv != (int)sizeof(bc) )
    {
        printf( "Error: Unable to write leading byte count. Wrote %d. Err=%s\n",
            retv, strerror(errno) );
        return -1;
    }
    if ( (outv=bc) )
    {
        if ( (bc&1) )
            ++outv;
        retv = fwrite( bufp, 1, outv, outp );
        if ( outv != retv )
        {
            printf( "Error: Unable to write %d byte record. Wrote %d. Err=%s\n",
                outv, retv, strerror(errno) );
            return -1;
        }
        if ( simhMode )
        {
            if ( (retv=fwrite( &bc, 1, sizeof(bc), outp)) != (int)sizeof(bc) )
            {
                printf( "Error: Unable to write trailing byte count. Wrote %d. Err=%s\n",
                    retv, strerror(errno) );
                return -1;
            }
        }
    }
    return bc;
}

static int write_ss( FILE *inp, int hdrIndx )
{
    FILE *outp;
    int outv, retv, bc, tmhist, wtmhist, state = 0, expectbc=0;
    int badcnt=0, badsize=0, skip;
    char ofname[sizeof(ssname)+8+4];

	skip = strlen(ssname);
	if ( skip && ssname[skip-1] == '.' )
	{
		--skip;
		ssname[skip] = 0;
	}
	if ( !hdrIndx )
		snprintf(ofname, sizeof(ofname), "%s%s", ssname, simhMode ? ".simh" : ".data");
	else
		snprintf(ofname, sizeof(ofname), "%02d_%s%s", hdrIndx, ssname, simhMode ? ".simh" : ".data");
    outp = fopen(ofname,"wb");
    if (!outp)
    {
        fprintf(stderr, "Error: Unable to open output '%s': %s\n",
            ofname, strerror(errno) );
        return -1;
    }
    if ( verbose )
    {
        printf( "Info: Opened '%s' for output...\n", ofname );
    }
    /* Write the VOL1 header */
    if ( write_rcd( outp, vol_label,80, "VOL1 header" ) != 80 )
        return -1;
    /* Write the HDR1 header */
    if ( write_rcd( outp, buff, 80, "HDR1 header" ) != 80 )
        return -1;
    memcpy(hdr1, buff, 80); /* Save a copy of our HDR1 record */
    wtmhist = tmhist = 0;             /* Clear TM history */
    while ( (retv=fread( &bc, 1, sizeof(bc), inp)) == (int)sizeof(bc) )
    {
/*        printf( "Record of %d bytes\n", bc ); */
        if ( bc > 65535 )
        {
            printf( "Warn: Record size %d too big. Probably out of sync\n", bc );
            continue;
        }
        skip = 0;       /* assume not to skip */
        if ( bc )
        {
            retv = fread( buff, 1, bc, inp );
            if ( retv != bc )
            {
                printf( "Error: Unable to read input. Found %d bytes, expected %d\n",
                    retv, bc );
                return -1;
            }
        }
        switch (state ) {
        case 0:
            if ( bc != 80 || strncmp(buff,"HDR2",4) )
            {
                printf( "Warn: No HDR2 record found after HDR1\n" );
            }
            else
            {
                int ii;
                memcpy(hdr2,buff,80);   /* Save a copy of our HDR2 record */
                ii = sscanf(buff+5,"%5d",&expectbc );
                if ( ii != 1 )
                {
                    char tchr[16];
                    memcpy(tchr, buff, 15);
                    tchr[15] = 0;
                    printf( "Warn: Error decoding byte count in '%s'. Found %d\n",
                        tchr, expectbc );
                }
            }
            ++state;
            break;
        case 1:
            if ( bc )
            {
                printf( "Warn: No tape mark found after HDR2 record\n" );
            }
            ++state;
            break;
        case 2:
            if ( !bc )
            {
                ++state;
                skip = 1;
                break;
            }
            if ( bc != expectbc )
            {
                if ( verbose && badcnt )
                {
                    if ( badsize != bc )
                    {
                        printf( "Info: Skipped %d records of size %d. Expected size %d\n",
                            badcnt, badsize, expectbc );
                        badcnt = 0;
                        badsize = bc;
                    }
                }
                ++badcnt;
                skip = 1;
                break;
            }
            if ( verbose && badcnt )
            {
                printf( "Info: Skipped %d records of %d bytes.\n", badcnt, badsize );
                badcnt = 0;
                badsize = 0;
            }
            break;
        case 3:
            if ( verbose && badcnt )
            {
                printf( "Info: Skipped %d records of %d bytes.\n", badcnt, badsize );
                badcnt = 0;
                badsize = 0;
            }
            if ( bc != 80 || strncmp(buff,"EOF1",4) )
            {
                printf( "Warn: Didn't find an EOF1 record after tape mark at end of data block.\n" );
                --state;
                if ( bc != expectbc )
                {
                    badsize = bc;
                    badcnt = 1;
                    skip = 1;
                }
                break;
            }
            ++state;
            break;
        default:
            skip = 1;
            break;
        }
        if ( bc == 80 )
        {
            if ( verbose && badcnt )
            {
                printf( "Info: Skipped %d records of %d bytes.\n", badcnt, badsize );
                badcnt = 0;
                badsize = 0;
            }
            /* Look for a VOL1, HDR1 or EOF1 record */
            if ( !strncmp(buff,"EOF1",4) )
            {
                if ( strncmp(buff+4,hdr1+4,14) )
                {
                    char hbuf[24], ebuf[24];
                    strncpy(ebuf,buff,23);
                    strncpy(hbuf,hdr1,23);
                    ebuf[23] = 0;
                    hbuf[23] = 0;
                    printf( "Warn: EOF1 record doesn't match HDR1 record:\n    %s\n    %s\n",
                        hbuf, ebuf );
                }
                break;
            }
            if ( !strncmp(buff,"VOL1",4) || !strncmp(buff,"HDR1",4) )
            {
                if ( verbose )
                {
                    char tbuf[24];
                    strncpy(tbuf,buff,23);
                    tbuf[23] = 0;
                    printf( "Warn: Premature end found with: %s\n", tbuf );
                }
                break;
            }
        }
        if ( !skip )
        {
            if ( write_rcd(outp, buff, bc, NULL) < 0 )
            {
                return -1;
            }
            if ( !bc )
            {
                wtmhist = (wtmhist<<1) | 1;
            }
        }
        if ( !bc )
        {
            tmhist = (tmhist<<1) | 1;
        }
    }
    if ( ferror(inp) )
    {
        printf( "Error: Error reading input. Expected %d bytes, got %d. Err=%s",
            sizeof(bc), retv, strerror(errno) );
        return -1;
    }
    if ( (wtmhist&3) != 3 )      /* Preceeded by two TM's? */
    {
        if ( write_rcd(outp,NULL,0, (tmhist&3) != 3 ? "missing tape mark" : "tape mark") != 0 ) /* Nope, write one */
            return -1;
        tmhist |= (tmhist<<1)|1;
        wtmhist |= (tmhist<<1)|1;
        if ( (wtmhist&3) != 3 )  /* Did that give us two TM's? */
        {
            if ( write_rcd(outp,NULL,0, (tmhist&3) != 3 ? "missing tape mark" : "tape mark") != 0 )  /* Nope, write another one */
                return -1;
        }
    }
    memcpy(hdr1,"EOF1",4);
    if ( write_rcd(outp,hdr1,80, "EOF1 header") != 80 )
    {
        return -1;
    }
    if ( !hdr2[0] )
    {
        printf("Warn: Didn't find a HDR2 record. VMS's backup may fail its tape label processing.\n" );
    }
    else
    {
        memcpy(hdr2,"EOF2",4);
        if ( write_rcd(outp,hdr2,80,"EOF2 header") != 80 )
        {
            return -1;
        }
    }
    for ( bc=0; bc < 3; ++bc )
    {
        if ( write_rcd(outp, NULL, 0, "trailing tape mark") != 0 )
            return -1;
    }
    if ( simhMode )
    {
        bc = -1;
        outv = fwrite( &bc, 1, sizeof(bc), outp );
        if ( outv != (int)sizeof(bc) )
        {
            printf( "Error: Error writing %d byte end of media header. Wrote %d. Err=%s\n",
                sizeof(bc), outv, strerror(errno) );
            return -1;
        }
    }
	return 0;
}

int main( int argc, char *argv[] )
{
	FILE *inp;
	static int bc;
	int opt, retv, hdr1Count, hdrIndx=0;
    char *cp, lssname[18];
	const char *title = argv[0], *dataFileName;
	
	while ( (opt = getopt(argc, argv, "sv")) != -1 )
	{
		switch (opt)
		{
		case 's':
			simhMode = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		default: /* '?' */
			help_em(stderr,title);
			return 1;
		}
	}
    if ( argc-optind < (hdrIndx ? 1 : 2) )
	{
        help_em(stdout,title);
        return 1;
	}
	strncpy(ssname, argv[optind], sizeof(ssname) - 1);
	dataFileName = argv[optind+1];
	cp = NULL;
	hdrIndx = strtol(ssname,&cp,0);
	if ( !cp || *cp || hdrIndx < 1 || hdrIndx >= 100 )
	{
		cp = strrchr( ssname, ' ' );
		if ( cp )
		{
			*cp = 0;
		}
		ssnamelen = strlen(ssname);
		memset(lssname,' ', sizeof(lssname)-1 );
		lssname[sizeof(lssname)-1] = 0;
		memcpy(lssname,ssname,ssnamelen);
		dataFileName = argv[optind+1];
		hdrIndx = 0;
	}
	inp = fopen( dataFileName, "rb" );
	if ( !inp )
	{
		perror( "Error opening datafile" );
		return 2;
	}
    if ( verbose )
    {
		if ( !hdrIndx )
			printf("Info: Looking for a saveset named '%s'\n", ssname);
		else 
			printf("Info: Looking for the saveset at HDR1 index %d\n", hdrIndx);
    }
	hdr1Count = 0;
	while ( (retv=fread( &bc, 1, sizeof(bc), inp)) == (int)sizeof(bc) )
	{
/*        printf( "Record of %d bytes\n", bc ); */
        if ( bc > 65535 )
        {
            printf( "Warn: Record size %d too big. Probably out of sync\n", bc );
            return 9;
        }
        if ( bc )
        {
            retv = fread( buff, 1, bc, inp );
            if ( retv != bc )
            {
                printf( "Error: Error reading input. Found %d bytes, expected %d\n",
                    retv, bc );
                return 5;
            }
        }
        if ( bc == 80 )
        {
            if ( !vol_label[0] && !strncmp(buff,"VOL1",4) )
            {
                strncpy(vol_label,buff,80);
				if ( !hdrIndx )
				{
					retv = ssnamelen;
					cp = strchr(ssname, '.');
					if ( cp )
					{
						retv = cp - ssname;
					}
					strncpy(vol_label+4,ssname,retv);
					vol_label[4+retv] = ' ';
					if ( verbose )
					{
						printf( "Info: Found '%s'\n", vol_label );
					}
				}
				else if ( verbose )
					printf( "Info: Found \"%s\"\n", vol_label );
            }
			/*                  00000000001111111111222222222233333333334444444444555555555566666666667777777777  */
			/*                  01234567890123456789012345678901234567890123456789012345678901234567890123456789  */
			/*                 "VOL186APRM                           D%C                                        " */
			/*                 "HDR1121589.SUTTLESDELxxx   00010081000100 89349 00000 000000DECVMSBACKUP        " */
            if ( !strncmp(buff,"HDR1",4) )
            {
				++hdr1Count;
				if ( !hdrIndx )
				{
					if ( !strncmp(buff + 4, lssname, MAX_SSNAME_LEN) )
					{
						if ( !vol_label[0] )
						{
							printf( "Error: No VOL1 label found before HDR1%s\n", ssname );
							return 1;
						}
						write_ss(inp, 0);
						fclose(inp);
						return 0;
					}
				}
				else if ( hdr1Count == hdrIndx )
				{
					char *cp;
					if ( !vol_label[0] )
					{
						printf("Error: No VOL1 label found before HDR1 index %d\n", hdr1Count);
						return 1;
					}
					retv = ssnamelen;
					cp = strchr(ssname, '.');
					if ( cp )
					{
						retv = cp - ssname;
					}
					memcpy(ssname, buff + 4, MAX_SSNAME_LEN);
					ssname[MAX_SSNAME_LEN] = 0;
					cp = ssname;
					while ( *cp && *cp != ' ' )
						++cp;
					*cp = 0;
					if ( write_ss(inp, hdrIndx) )
					{
						fclose(inp);
						return 1;
					}
					fclose(inp);
					return 0;
				}
            }
        }
	}
	if ( !feof(inp) )
	{
		printf( "Error: Unable to read input. Expected %d bytes, got %d. Err=%s",
            sizeof(bc), retv, strerror(errno) );
		return 8;
	}
	if ( !hdrIndx )
		printf("Error: Didn't find saveset named '%s'\n", ssname);
	else
		printf("Error: Last HDR1 was number %d. Didn't find HDR1 at index %d\n", hdr1Count, hdrIndx);
	fclose(inp);
	return 10;
}
