#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mtio.h>

/* Program used to copy the images from tape to disk. */

/* Usage: cp_tape outfile */
/* Reads from /dev/st0, the first scsi tape drive found on the system. */

/*
 * The output file format is variable length records:
 * 4 bytes, little endian, containing the record length in bytes.
 * followed by n bytes of record data.
 * A record of length 0 represents a tape mark.
 * Two tape marks in a row indicates an end of tape condition.
 */

static char buff[128*1024];

int main( int argc, char *argv[] )
{
    int sts,fd,outfd;
    struct mtget mtsts;
    struct mtop ops;
    int tape_marks = 0;
    unsigned long total = 0;
    char *dst = "/tmp/tape";

    fd = open( "/dev/st0", O_RDONLY );
    if ( fd < 0 )
    {
        perror( "Unable to open /dev/st0" );
        return 1;
    }
    sts = ioctl( fd, MTIOCGET, &mtsts );
    if ( sts < 0 )
    {
        perror( "Unable to MTIOCGET to /dev/st0" );
        return 3;
    }
    printf( "Tape status:\nType: %08lX\n", mtsts.mt_type );
    printf( "resid: %08lX\n", mtsts.mt_resid );
    printf( "dsreg: %08lX\n", mtsts.mt_dsreg );
    printf( "   blksize: %ld, density: %ld\n", 
           (mtsts.mt_dsreg&MT_ST_BLKSIZE_MASK) >> MT_ST_BLKSIZE_SHIFT,
           (mtsts.mt_dsreg&MT_ST_DENSITY_MASK) >> MT_ST_DENSITY_SHIFT );
    printf( "gstat: %08lX\n", mtsts.mt_gstat );
    printf( "   EOF: %s\n", GMT_EOF( mtsts.mt_gstat ) ? "Yes" : "No " );
    printf( "   BOT: %s\n", GMT_BOT( mtsts.mt_gstat ) ? "Yes" : "No " );
    printf( "   EOT: %s\n", GMT_EOT( mtsts.mt_gstat ) ? "Yes" : "No " );
    printf( "   SM:  %s\n", GMT_SM(  mtsts.mt_gstat ) ? "Yes" : "No " );
    printf( "   EOD: %s\n", GMT_EOD( mtsts.mt_gstat ) ? "Yes" : "No " );
    printf( "   WPT: %s\n", GMT_WR_PROT( mtsts.mt_gstat ) ? "Yes" : "No " );
    printf( "erreg: %08lX\n", mtsts.mt_erreg );
    printf( "fileno: %d\n", mtsts.mt_fileno );
    printf( "blkno: %d\n", mtsts.mt_blkno );
    ops.mt_op = MTSETBLK;
    ops.mt_count = 0;
    sts = ioctl( fd, MTIOCTOP, &ops );
    if ( sts < 0 )
    {
        perror( "Unable to set to variable blocksize." );
        return 4;
    }
    if ( argc > 1 )
	dst = argv[1];
    outfd = creat( dst, 0664 );
    if ( outfd < 0 )
    {
	sprintf( buff, "Unable to open %s\n", dst );
        perror( buff );
        return 5;
    }
    while ( 1 )
    {
        unsigned long bcnt;
        char hdr[50];
        sts = read( fd, buff, sizeof(buff) );
        if ( sts < 0 )
        {
            perror( "Error reading /dev/st0" );
            return 2;
        }
	total += sts;
        if ( sts == 80 )
        {
            int ii;
            memcpy( hdr, buff, sizeof(hdr)-1 );
            for ( ii=0; ii < sizeof(hdr)-1; ++ii )
            {
                if ( !isprint( hdr[ii] ) )
                    hdr[ii] = '.';
            }
            hdr[(int)sizeof(hdr)-1] = 0;
            printf( "Read %6d bytes: \"%s\"\n", sts, hdr );
        }
        else
	{
#if 0
	    if ( !sts )
		printf( "Found tape mark.\n" );
//	    printf( "Read %6d bytes.\n", sts );
#endif
	}
        bcnt = sts;
        write( outfd, &bcnt, sizeof(bcnt) );
        if ( sts ) 
	    write( outfd, buff, sts );
	tape_marks <<= 1;
	if ( !sts )
	{
	    tape_marks |= 1;
	    if ( (tape_marks&3) == 3 )	// two tape marks in a row is EOT
	    {
		break;
	    }
	}
    }
    close( fd );
    close( outfd );
    printf( "Read a total of %ld bytes\n", total );
    return 0;
}
