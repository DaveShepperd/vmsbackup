/**
 * @file vmsbackup.c
 */

/**
 *
 * @mainpage
 * @version Feb_2024
 * @par Read Me
 *
 *  Title:
 *	vmsbackup
 *
 *  Decription:
 *	Program to read and decode VMS Backup images
 *
 *  @author John Douglas CAREY.
 *  @author Sven-Ove Westberg    (version 3.0 and up)
 *  @author Dave Shepperd, (DMS) vmsbackup@dshepperd.com
 *  		(version Mar_2003 with snipits from Sven-Ove's
 *  		version 4-1-1)
 *
 *  https://github.com/DaveShepperd/vmsbackup
 *
 *  Net-addess (as of 1986 or so; it is highly unlikely these still work):
 *	john%monu1.oz@seismo.ARPA
 *	luthcad!sow@enea.UUCP
 *  vmsbackup@dshepperd.com
 *
 *  History:
 *	Version 1.0 - September 1984
 *		Can only read variable length records
 *	Version 1.1
 *		Cleaned up the program from the original hack
 *		Can now read stream files
 *	Version 1.2
 *		Now convert filename from VMS to UNIX
 *			and creates sub-directories
 *	Version 1.3
 *		Works on the Pyramid if SWAP is defined
 *	Version 1.4
 *		Reads files spanning multiple tape blocks
 *	Version 1.5
 *		Always reset reclen = 0 on file open
 *		Now output fixed length records
 *
 *      Version 2.0 - July 1985
 *		VMS Version 4.0 causes a rethink !!
 *		Now use mtio operations instead of opening and closing file
 *		Blocksize now grabed from the label
 *
 *	Version 2.1 - September 1985
 *		Handle variable length records of zero length.
 *
 *	Version 2.2 - July 1986
 *		Handle FORTRAN records of zero length.
 *		Inserted exit(0) at end of program.
 *		Distributed program in aus.sources
 *
 *	Version 2.3 - August 1986
 *		Handle FORTRAN records with record length fields
 *		at the end of a block
 *		Put debug output to a file.
 *		Distributed program in net.sources
 *
 *	Version 3.0 - December 1986
 *		Handle multiple saveset 
 *		Remote tape
 *		Interactive mode
 *		File name selection with meta-characters
 *		Convert ; to : in VMS filenames
 *		Flag for usage of VMS directory structure
 *		Flag for "useless" files  eg. *.exe
 *		Flag for use VMS version in file names
 *		Flag for verbose mode
 *		Flag to list the contents of the tape
 *		Distributed to mod.sources
 *
 *	Version 3.1 - March 2003 (DMS)
 *		ANSI'fy the source
 *		added option, -i, to read "tape file" from disk
 *		added option, -n, to read specific saveset from disk/tape
 *		changed option, -v, to accept a bit mask to enable various squawks
 *		Optimise the writing of data (avoid doing single char output where possible)
 *		Added handling of redundancy groups and duplicate and missing blocks.
 *		Fixed numerous crashes caused by various mis-decodes
 *		Record and decode errors are no longer fatal.
 *		Cut and pasted select features from the 4-1-1 release
 *		Added vms2unix date function and utime()'d extracted files.
 *			(NOTE: There may be a problem with this since I
 *			found several savesets made by VMS versions 3.x and 4.x
 *			where the date seemed to be ~4 years too young. Newer
 *			savesets [> VMS 5.0?] seem to have dates set correctly;
 *			Was there a change in the VMS epoch at some point?).
 *		Always walk the file data chain even when only doing a directory
 *			listing in order to identify those files which may present
 *			decode or other errors.
 *		Due to bizzare data in the VAR records found in .mai and .dir
 *			files, force it never to extract them or even walk the chain.
 *		Recover from BACKUP's occasional mistake of leaving off the
 *			record length bytes from record 0 of vfc files.
 *		Added some 'doxygen'ated (but untested) comments.
 *
 *  Version 3.2 - April 2020 (DMS)
 *  	Added -I, to read SIMH format 'tape file' from disk
 *
 *  Verson 3.3 - Janurary 2022 (DMS)
 *      Fixed comments. Added a -S to skip to HDR1 record.
 *
 *  Version 3.4 - November 2023 (DMS)
 *  	Added -R to make it not lowercase filenames and to strip
 *  	off the file version number completely and only output
 *  	the latest version of any file.
 *
 *  Version 3.5 - January 2024 (DMS)
 *  	Added -L to provide lowercase directory and filenames.
 *
 *  Version 3.6 - January 2024 (DMS)
 *  	Added -E, reworked the -e option a bit.
 *  	Fixed some comments.
 *
 *  Version 3.7 - Feburary 2024 (DMS)
 *  	Added support for optional VFC decode in output.
 *  	Changed the way it handles record delimiters.
 *
 *  Version 3.8 - Feburary 2024 (DMS)
 *  	Added support for fixed length input records.
 *  	Added support for getopt_long().
 *  	Added a --record option.
 *  	Added rfm/size/att to directory listing.
 *  	Write binary output and filename if record errors
 *  	detected.
 *  	Updated help message.
 *  	See README.md or vmsbackup.html for further details.
 *
 *  Versin 3.9 - April 2024 (DMS)
 *      Added support for building under MSYS2 on Windows
 *
 *  Versin 3.10 - April 2024 (DMS)
 *  	Fixed MSYS2 builds and added ones for Linux, MinGW32 and
 *  	PiOS (32 bit only).
 *
 *  Versin 3.11 - April 2024 (DMS)
 *  	Fixed MSYS2 and MinGW builds. Needed the O_BINARY and
 *  	"b" flags.
 *
 *  Versin 3.12 - May 2024 (DMS)
 *  	Re-worked the method of exporting binary as a result of
 *  	errors in VAR/VFC record structures.
 *
 *  Installation:
 *
 *	Computer Centre
 *	Monash University
 *	Wellington Road
 *	Clayton
 *	Victoria	3168
 *	AUSTRALIA
 *
 */

#include	<stdio.h>
#include	<ctype.h>
#include	<string.h>
#include	<strings.h>
#include	<stdlib.h>
#include	<unistd.h>
#include	<getopt.h>
#include	<time.h>
#include	<utime.h>
#if HAVE_STRERROR
#include	<errno.h>
#endif
#include	<sys/types.h>
#ifdef REMOTE
	#include	<local/rmt.h>
#endif
#include	<sys/stat.h>
#if HAVE_MTIO
#include	<sys/ioctl.h>
#include	<sys/mtio.h>
#endif
#include	<sys/file.h>

#if MSYS2 || MINGW
#define MKDIR(a,b) mkdir(a)
#define OPEN_FLAGS O_RDONLY|O_BINARY
#else
#define MKDIR(a,b) mkdir(a,b)
#define OPEN_FLAGS O_RDONLY
#endif

#ifndef n_elts
#define n_elts(x) (int)(sizeof(x)/sizeof((x)[0]))
#endif

#define INT_SIZEOF(x) (int)(sizeof(x))

extern int match ( const char *string, const char *pattern );
static int typecmp ( const char *str, int which );

#define MAX_FILENAME_LEN (128)
#define MAX_FORMAT_LEN	 (16)

struct bbh
{
	short bbh_dol_w_size;
	short bbh_dol_w_opsys;
	short bbh_dol_w_subsys;
	short bbh_dol_w_applic;
	long bbh_dol_l_number;
	char bbh_dol_t_spare_1[20];
	short bbh_dol_w_struclev;
	short bbh_dol_w_volnum;
	long bbh_dol_l_crc;
	long bbh_dol_l_blocksize;
	long bbh_dol_l_flags;
	char bbh_dol_t_ssname[32];
	short bbh_dol_w_fid[3];
	short bbh_dol_w_did[3];
	char bbh_dol_t_filename[128];
	char bbh_dol_b_rtype;
	char bbh_dol_b_rattrib;
	short bbh_dol_w_rsize;
	char bbh_dol_b_bktsize;
	char bbh_dol_b_vfcsize;
	short bbh_dol_w_maxrec;
	long bbh_dol_l_filesize;
	char bbh_dol_t_spare_2[22];
	short bbh_dol_w_checksum;
};

static unsigned long last_block_number;

struct brh
{
	short brh_dol_w_rsize;
	short brh_dol_w_rtype;
	long brh_dol_l_flags;
	long brh_dol_l_address;
	long brh_dol_l_spare;
};

/* define record types */

#define	brh_dol_k_null	0
#define	brh_dol_k_summary	1
#define	brh_dol_k_volume	2
#define	brh_dol_k_file	3
#define	brh_dol_k_vbn	4
#define brh_dol_k_physvol	5
#define brh_dol_k_lbn	6
#define	brh_dol_k_fid	7

struct bsa
{
	short bsa_dol_w_size;
	short bsa_dol_w_type;
	char bsa_dol_t_text[1];
};

typedef enum
{
	GET_IDLE,
	GET_RCD_COUNT,
	GET_VFC,
	GET_DATA
} FileState_t;

struct file_details
{
	time_t ctime;
	time_t mtime;
	time_t atime;
	time_t btime;
	FILE *extf;
	FILE *altf;
	int directory;
	unsigned int size;
	int nblk;
	int lnch;
	int allocation;
	int usr;
	int grp;
	int recfmt;
	int savRecFmt;
	int recatt;
	unsigned int inboundIndex;		/* Index into specific byte in input file */
	unsigned int outboundIndex;		/* Index into specific byte in output file */
	unsigned int altboundIndex;
	int rec_count;
	int rec_padding;
	int vfcsize;					/* number of VFC bytes */
	char name[MAX_FILENAME_LEN+4];	/* Name from tape image */
	char ufname[MAX_FILENAME_LEN+MAX_FORMAT_LEN+4]; /* Name converted to Unix */
	char altUPfName[MAX_FILENAME_LEN+MAX_FORMAT_LEN+4]; /* Name converted to Unix */
	char *altUfNameOnly;			/* Place in altUPfName of '.' character at head of alternate filename */
	char *versionPtr;
	unsigned short reclen;			/* length of most recent record read */
	short fix;
	unsigned short recsize;			/* record length in FIXED and max record length in VAR and VFC formats */
	int do_vfc;
	unsigned char vfc0, vfc1;
	int do_rat;
	int do_binary;
	int errorIndex;
	int altErrorIndex;
	int file_record_error;
	int file_blk_error;
	int file_size_error;
	int file_format_error;
	FileState_t file_state;
} file;

char *tapefile;

time_t secs_adj;

#define	FAB_dol_C_RAW	0	/* undefined */
#define	FAB_dol_C_FIX	1	/* fixed-length record */
#define	FAB_dol_C_VAR	2	/* variable-length record */
#define	FAB_dol_C_VFC	3	/* variable-length with fixed-length control record */
#define FAB_dol_C_STM	4	/* RMS-11 stream record (valid only for sequential org) */
#define	FAB_dol_C_STMLF	5	/* stream record delimited by LF (sequential org only) */
#define FAB_dol_C_STMCR	6	/* stream record delimited by CR (sequential org only) */
#define FAB_dol_C_FIX11 11	/* alternate fixed-length */
#define	FAB_dol_C_MAXRFM 11	/* maximum rfm supported */
#define FAB_dol_M_MAIL (0x20)	/* this bit is set on .mai files for some reason */

#define	FAB_dol_V_FTN	0	/* FORTRAN carriage control character */
#define	FAB_dol_V_CR	1	/* line feed - record -carriage return */
#define	FAB_dol_V_PRN	2	/* print-file carriage control */
#define	FAB_dol_V_BLK	3	/* records don't cross block boundaries */

/* File record ID's */

#define FREC_END	(0x00)	/* End of list */
#define FREC_FNAME	(0x2a)	/* Filename */
#define FREC_UNK2b	(0x2b)	/* ? */
#define FREC_UNK2c	(0x2c)	/* ? */
#define FREC_UNK2d	(0x2d)	/* ? */
#define FREC_UNK2e	(0x2e)	/* ? */
#define FREC_UID	(0x2f)	/* UID */
#define FREC_UNK30	(0x30)	/* ? */
#define FREC_UNK31	(0x31)	/* ? */
#define FREC_UNK32	(0x32)	/* ? */
#define FREC_UNK33	(0x33)	/* ? */
#define FREC_FORMAT	(0x34)	/* Record format and stuff */
#define FREC_UNK35	(0x35)	/* ? */
#define FREC_CTIME	(0x36)	/* Creation time */
#define FREC_MTIME	(0x37)	/* Modification time */
#define FREC_ATIME	(0x38)	/* Access time */
#define FREC_BTIME	(0x39)	/* Backup time */
#define FREC_UNK47	(0x47)	/* ? */
#define FREC_UNK48	(0x48)	/* ? */
#define FREC_DIRECTORY	(0x49)	/* Directory file */
#define FREC_UNK4a	(0x4A)	/* ? */
#define FREC_UNK4b	(0x4B)	/* ? */
#define FREC_UNK4e	(0x4e)	/* ? */
#define FREC_UNK4f	(0x4F)	/* ? */
#define FREC_UNK50	(0x50)	/* ? */
#define FREC_UNK57	(0x57)	/* ? */

/* Summary record ID's */

#define SUMM_END	(0)	/* end of summary list */
#define SUMM_SSNAME	(1)	/* saveset name */
#define SUMM_CMDLINE	(2)	/* command line */
#define SUMM_COMMENT	(3)	/* comment */
#define SUMM_USER	(4)	/* username of creator of ss */
#define SUMM_UID	(5)	/* PID/GID of creator */
#define SUMM_CTIME	(6)	/* creation time */
#define SUMM_OSCODE	(7)	/* OS code */
#define SUMM_OSCODE_VAX (0x400) /* VAX */
#define SUMM_OSCODE_AXP (0x800) /* Alpha */
#define SUMM_OSVERSION	(8)	/* OS version */
#define SUMM_NODENAME	(9)	/* nodename */
#define SUMM_PID	(10)	/* Processor ID */
#define SUMM_DEVICE	(11)	/* device used to write ss */
#define SUMM_BCKVERSION	(12)	/* BACKUP version */
#define SUMM_BLOCKSIZE	(13)	/* /BLOCKSIZE */
#define SUMM_GROUPSIZE	(14)	/* /GROUP */
#define SUMM_BUFFCOUNT	(15)	/* /BUFFER */

int fd;				/* tape file descriptor */
int cDelim, dflag, eflag, iflag, Iflag, lcflag, nflag, binaryFlag, tflag, vflag, wflag, xflag, Rflag, vfcflag;
int setnr, selset, skipSet, numHdrs, saveSet_errors, total_errors;
char selsetname[14];

int skipping;			/*!< Bit mask of errors as described below */
#define SKIP_TO_FILE	(1)	/*!< Skip to next file */
#define SKIP_TO_BLOCK	(2)	/*!< Skip to next block */
#define SKIP_TO_SAVESET	(4)	/*!< Skip to next saveset */

/* Bit mask of verbosity levels set in vflag */
#define VERB_LVL	(1)	/* verbose */
#define VERB_FILE_RDLVL	(2)	/* verbose on record read processing */
#define VERB_FILE_WRLVL	(4)	/* verbose on record write processing */
#define VERB_QUEUE_LVL	(8)	/* squawk about queue handling */
#define VERB_DEBUG_LVL	(16)	/* generic debug statements */
#define VERB_BLOCK_LVL	(32) /* squawk during block processing */
#define VERB_DEBUG_U32	(64) /* squawk about what getu32() does */

char **gargv;
int goptind, gargc;

#define	LABEL_SIZE	80
char label[32768 + LABEL_SIZE];

static int blocksize;

/*
 * Someday, one might want to make MAX_BUFFCOUNT dynamic and get the actual
 * value from the /BUFF field of the backup command (as reported in the
 * summary record). For now, I just picked something that seemed reasonable.
 */
#ifndef MAX_BUFFCOUNT
	#define MAX_BUFFCOUNT (10)	/*!< Number of look ahead buffers */
#endif

/* A 'buffer' is actually a struct buff_ctl */

struct buff_ctl
{
	unsigned char *buffer;	/*!< pointer to buffer */
	int next;			/*!< index to next buffer (kept as index so we can realloc if necessary) */
	int amt;			/*!< amount of data in this buffer (0=tape mark) */
	unsigned long blknum;	/*!< block number (stored here for ease of use) */
};

static int buffalloc;		/*!< size of each buffer within buff_ctl */
static int num_buffers;		/*!< number of buffers currently allocated */
static int buff_cnt;		/*!< buffer count spec'd with /BUFF to VMS BACKUP (from saveset) */
static struct buff_ctl *buffers; /*!< pointer to array of buffers (0th entry is unused) */
static int freebuffs;		/*!< index to first item in freelist  */
static int busybuffs;		/*!< index to first item in busy list */
static int num_busys;		/*!< number of items currently on busy queue */

/* Byte-swapping routines.  Note that these do not depend on the size
   of datatypes such as short, long, etc., nor do they require us to
   detect the endianness of the machine we are running on.  It is
   possible they should be macros for speed, but I'm not sure it is
   worth bothering.  We don't have signed versions although we could
   add them if needed.  They are, of course little-endian as that is
   the byteorder used by all integers in a BACKUP saveset.  */

static unsigned long getu32 ( unsigned char *addr )
{
	unsigned long ans;
	ans = addr[3];
	ans = (ans<<8) | addr[2];
	ans = (ans<<8) | addr[1];
	ans = (ans<<8) | addr[0];
	if ( (vflag&VERB_DEBUG_U32) )
		printf("getu32(): %p=%02X %02X %02X %02x = 0x%lX (%ld)\n", (void *)addr, addr[0], addr[1], addr[2], addr[3], ans, ans);
	return ans;
}

static unsigned int getu16 ( unsigned char *addr )
{
	unsigned int ans;
	ans = (addr[1] << 8) | addr[0];
	if ( (vflag&VERB_DEBUG_U32) )
		printf("getu16(): %p=%02X %02X = 0x%X (%d)\n", (void *)addr, addr[0], addr[1], ans, ans);
	return ans;
}

#define GETU16(x) getu16( (unsigned char *)&(x) )
#define GETU32(x) getu32( (unsigned char *)&(x) )

/**
 * Dump the contents (indicies only) of the busy and free queues.
 *
 * @param which Bitmask of which queue to dump.
 * 	@arg 1 Dump the busy queue
 * 	@arg 2 Dump the free queue
 *
 * @return nothing
 */

static void dump_queues( int which )
{
	if ( (vflag&VERB_QUEUE_LVL) )
	{
		int idx;
		struct buff_ctl *bptr;
		if ( (which&1) )
		{
			printf( "\tBusy queue (%d): ", busybuffs );
			idx = busybuffs;
			while ( idx )
			{
				printf( "%d ", idx );
				bptr = buffers + idx;
				idx = bptr->next;
			}
			printf( "\n" );
		}
		if ( (which&2) )
		{
			printf( "\tFree queue (%d): ", freebuffs );
			idx = freebuffs;
			while ( idx )
			{
				printf( "%d ", idx );
				bptr = buffers + idx;
				idx = bptr->next;
			}
			printf( "\n" );
		}
	}
}

/**
 * Pop top item off busy queue
 *
 * @return Pointer to item or 0 if nothing available.
 */

static struct buff_ctl *popbusy_buff(void)
{
	struct buff_ctl *bptr;
	if ( !busybuffs )
	{
		if ( (vflag&VERB_QUEUE_LVL) )
		{
			printf( "popbusy_buff(): No items on queue.\n" );
			dump_queues( 3 );
		}
		return NULL;
	}
	bptr = buffers + busybuffs;
	busybuffs = bptr->next;
	bptr->next = 0;
	--num_busys;
	if ( (vflag&VERB_QUEUE_LVL) )
	{
		printf( "popbusy_buff(): popped %d off busy queue. num_busys now %d\n",
				bptr-buffers, num_busys );
		dump_queues( 3 );
	}
	return bptr;
}

/**
 * Add item to busy queue.
 *
 * @param bptr Pointer to item.
 * @param front Flag indicating where to add.
 *	@arg 0 Append to end of list.
 *	@arg 1 Prepend to front of list.
 *
 * @return nothing
 */

static void add_busybuff( struct buff_ctl *bptr, int front ) 
{
	int prev, ii;

	++num_busys;
	if ( (vflag&VERB_QUEUE_LVL) )
	{
		printf( "add_busybuff(): Added item %d to %s of busy queue. num_busys now %d\n",
				bptr - buffers, front ? "head" : "tail", num_busys );
	}
	if ( front )
	{
		bptr->next = busybuffs;
		busybuffs = bptr-buffers;
	}
	else
	{
		prev = 0;
		ii = busybuffs;
		bptr->next = 0;		/* make sure this is off */
		while ( ii )		/* walk the chain */
		{
			prev = ii;		/* remember this */
			ii = buffers[ii].next;	/* advance to next */
		}
		if ( prev )			/* if there was a chain */
			buffers[prev].next = bptr-buffers; /* link it */
		else
			busybuffs = bptr-buffers; /* else we are on top */
	}
	dump_queues( 3 );
}

/**
 * Get an item from free queue.
 *
 * @return Pointer to item or NULL if free queue empty.
 */

static struct buff_ctl *getfree_buff(void)
{
	struct buff_ctl *bptr;
	if ( !freebuffs )
	{
		if ( (vflag&VERB_QUEUE_LVL) )
		{
			printf( "getfree_buff(): Nothing on free list!!! num_busys now %d\n", num_busys );
			dump_queues( 3 );
		}
		return NULL;
	}
	bptr = buffers + freebuffs;
	freebuffs = bptr->next;
	bptr->next = 0;
	bptr->amt = 0;
	bptr->blknum = 0;
	if ( (vflag&VERB_QUEUE_LVL) )
	{
		printf( "getfree_buff(): Extracted %d from freelist. num_busys now %d\n", bptr-buffers, num_busys );
		dump_queues( 3 );
	}
	return bptr;
}

/**
 * Put item back on free queue.
 *
 * @param bptr Pointer to item.
 *
 * @return nothing.
 */

static void free_buff( struct buff_ctl *bptr )
{
	if ( bptr )
	{
		bptr->next = freebuffs;
		freebuffs = bptr - buffers;
		if ( (vflag&VERB_QUEUE_LVL) )
		{
			printf( "free_buff(): Put %d on freelist. num_busys now %d\n", bptr - buffers, num_busys );
			dump_queues( 3 );
		}
	}
}

/** 
 * Put all buffers back on free queue.
 *
 * @return nothing.
 */

static void freeall( void )
{
	if ( buffers )
	{
		struct buff_ctl *bp;
		int ii;
		bp = buffers+1;
		for ( ii=1; ii < num_buffers-1; ++ii, ++bp )
			bp->next = ii+1;
		bp->next = 0;
		freebuffs = 1;
		busybuffs = 0;
		if ( (vflag&VERB_QUEUE_LVL) )
		{
			printf( "freeall(): Free'd all buffers.\n" );
			dump_queues( 3 );
		}
	}
}

/**
 * Remove duplicate blocks from busy list.
 *
 * @return nothing.
 */

static void remove_dups( void )
{
	int ii, lim, jj;
	struct buff_ctl *bptr, *la;
	int buffs[MAX_BUFFCOUNT];		/* place to hold clone of buffer layout */

	if ( num_busys <= 1 )
		return;				/* nothing to do if there's only one item */
	memset( buffs, 0, sizeof(buffs) );	/* preclear local array */
	buffs[0] = busybuffs;
	bptr = buffers + busybuffs;
	lim = 1;
	while ( lim < MAX_BUFFCOUNT && bptr->next )	/* clone the busy list to our local array */
	{
		buffs[lim++] = bptr->next;
		la = buffers + bptr->next;
		bptr = la;
	}
	if ( lim >= MAX_BUFFCOUNT && bptr->next )
	{
		printf( "Snark: fatal internal error. Too many items on buffer list.\n" );
		skipping |= SKIP_TO_SAVESET;	/* toss the remainder of this saveset */
		return;
	}
	if ( lim != num_busys )
	{
		printf( "Snark: fatal internal error. busy list count (%d) != num_busys (%d).\n",
				lim, num_busys );
		skipping |= SKIP_TO_SAVESET;	/* toss the remainder of this saveset */
		return;
	}
	if ( lim == 2 && !bptr->amt )	/* only two items, but second is a tm */
		return;				/* so, nothing to do */
/* First, check each entry for a duplicate entry */
	if ( (vflag&VERB_QUEUE_LVL) )
	{
		printf( "Before checking for duplicates:\n\tBusy queue (?): " );
		for ( ii=0; ii < lim; ++ii )
			printf( "%d ", buffs[ii] );
		printf( "\n\tblknums: " );
		for ( ii=0; ii < lim; ++ii )
		{
			bptr = buffers + buffs[ii];
			printf( "%7ld ", bptr->blknum );
		}
		printf( "\n" );
		dump_queues( 2 );
	}
/*
 * If there are duplicate blocks, the later one must win.
 */
	for ( ii=0; ii < lim-1; ++ii )	 /* for each item in busy list */
	{
		bptr = buffers + buffs[ii];
		for ( jj=ii+1; jj < lim; ++jj )	 /* check to see if there's a like numbered one that came later */
		{
			la = buffers + buffs[jj];
			if ( la->amt && la->blknum == bptr->blknum )
			{
				if ( (vflag&VERB_FILE_RDLVL) )
					printf( "Found duplicate block numbered %ld. Discarded original.\n", bptr->blknum );
				buffs[ii] = buffs[jj];	/* Just place the duplicate in the original's location */
				if ( lim-jj > 1 )	/* and shift what's left (if any) up one spot */
					memmove( buffs+jj, buffs+jj+1, (lim-jj-1) * sizeof(int) );
				--ii;			/* conteract the outer for loop's ++ */
				--lim;			/* shrink the total by 1 */
				--num_busys;		/* reduce busy count too */
				if ( (vflag&VERB_QUEUE_LVL) )
				{
					printf( "Found duplicate block numbered %ld. Discarding buffer %d\n",
							bptr->blknum, bptr-buffers );
					busybuffs = buffs[0]; /* fixup the busy que so it'll display correctly */
					for ( jj=0; jj < lim-1; ++jj )
					{
						la = buffers + buffs[jj];
						la->next = buffs[jj+1];
					}
					la = buffers + buffs[jj];
					la->next = 0;
				}
				free_buff( bptr );	/* toss the original buffer */
				break;			/* look again from the beginning */
			}
		}
	}
	if ( (vflag&VERB_QUEUE_LVL) )
	{
		printf( "After removing duplicates:\n\tBusy queue (?): " );
		for ( ii=0; ii < lim; ++ii )
			printf( "%d ", buffs[ii] );
		printf( "\n" );
		dump_queues( 2 );
	}
/*
 * There may be missing blocks, so we bubble sort what's left
 * in case that happens. The decoder will check for and decide
 * what to do about missing blocks.
 */
	for ( ii=0; ii < num_busys-1; ++ii )
	{
		bptr = buffers + buffs[ii];
		for ( jj=ii+1; jj < num_busys; ++jj )
		{
			la = buffers + buffs[jj];
			if ( bptr->amt && bptr->blknum > la->blknum )
			{
				int sav = buffs[ii];
				buffs[ii] = buffs[jj];
				buffs[jj] = sav;
				bptr = la;
			}
		}
	}
/* Now rebuild the linked busy list */
	for ( ii=0; ii < num_busys-1; ++ii )
	{
		bptr = buffers + buffs[ii];
		bptr->next = buffs[ii+1];
	}
	bptr = buffers + buffs[ii];
	bptr->next = 0;
	busybuffs = buffs[0];
	if ( (vflag&VERB_QUEUE_LVL) )
	{
		printf( "After sorting:\n" );
		dump_queues( 3 );
	}
}

static int getRfmRatt(struct file_details *file, char *rcdFormat, int dstLen, char delim)
{
	int ii,rLen;
	static struct
	{
		int type;
		const char *name;
	} RcdFmts[] = {
#if 0
		"UNDEF"    // 0
	   ,"FIXED"    // 1
	   ,"VAR"      // 2
	   ,"VFC"      // 3
	   ,"STMCRLF"  // 4
	   ,"STMLF"	// 5
	   ,"STMCR"	// 6
#endif
		{ FAB_dol_C_RAW, "UNDEF" },
		{ FAB_dol_C_FIX, "FIXED" },
		{ FAB_dol_C_VAR, "VAR" },
		{ FAB_dol_C_VFC, "VFC" },
		{ FAB_dol_C_STM, "STMCRLF" },
		{ FAB_dol_C_STMLF, "STMLF" },
		{ FAB_dol_C_STMCR, "STMCR" },
		{ FAB_dol_C_FIX11, "FIXED" }
	};
	static struct 
	{
		int mask;
		const char *name;
	} RcdAtts[] = {
		{ 1<<FAB_dol_V_FTN, "FTN" },
		{ 1<<FAB_dol_V_CR, "CR" },
		{ 1<<FAB_dol_V_PRN, "PRN" },
		{ 1<<FAB_dol_V_BLK, "BLK" }
	};
	rLen = 0;
	for (ii=0; ii < n_elts(RcdFmts);++ii)
	{
		if ( file->savRecFmt == RcdFmts[ii].type )
		{
			if ( file->savRecFmt == FAB_dol_C_VFC )
				rLen += snprintf(rcdFormat + rLen, dstLen - rLen, "%c%s%d%c%d", delim, RcdFmts[ii].name, file->vfcsize, delim, file->recsize);
			else
				rLen += snprintf(rcdFormat + rLen, dstLen - rLen, "%c%s%c%d", delim, RcdFmts[ii].name, delim, file->recsize);
			break;
		}
	}
	if ( ii >= n_elts(RcdFmts) )
		rLen += snprintf(rcdFormat + rLen, dstLen - rLen, "%cUNDEF%c%d", delim, delim, file->recsize);
	if ( (file->recatt&((1<<n_elts(RcdAtts))-1)) )
	{
		for ( ii = 0; ii < n_elts(RcdAtts); ++ii )
		{
			if ( (file->recatt&RcdAtts[ii].mask) )
				rLen += snprintf(rcdFormat + rLen, dstLen - rLen, "%c%s", delim, RcdAtts[ii].name);
		}
	}
	else
		rLen += snprintf(rcdFormat + rLen, dstLen - rLen, "%cNONE", delim);
	rcdFormat[rLen] = 0;
	return rLen;
}

/**
 * Open a unix file.
 *
 * @param ufn Pointer to place to deposit unix filename.
 * @param fn  Pointer to VMS filename.
 * @param dirfile Flag indicating file is a directory.
 *	@arg 0 File is not a directory.
 *	@arg non-zero File is a directory.
 *
 * @return FILE * of open file or NULL if file not opened.
 *
 * @note
 * Convert VMS filename, 'fn', to unix filename, 'ufn'.
 * Check filename against the 'don't extract' list.
 * Opens an output file only if in extract mode.
 */

static char lastFileName[256];
static int lastVersionNumber;

static FILE *openfile ( struct file_details *file )
{
	char ans[80];
	char *p, *q, s, *ext = NULL; /*, *justFileName; */
	int procf;
	char *ufn = file->ufname;
	char *fn = file->name;
	int dirfile = file->directory;
	char rfm[MAX_FORMAT_LEN+4];

	procf = 1;
	/* copy whole fn to ufn and convert to lower case */
	p = fn;
	q = ufn;
	if ( *p == '[' )	/* strip off leading '[' */
		++p;
	while ( *p )
	{
		if ( lcflag && isupper ( *p ) )
			*q = tolower( *p );
		else
			*q = *p;
		++p;
		++q;
	}
	*q = '\0';

	/* convert the VMS to UNIX and make the directory path */
	p = ufn;
	q = p;
	while ( *q )
	{
		if ( *q == '.' || *q == ']' )
		{
			s = *q;
			*q = '\0';
			if ( procf && dflag )
				MKDIR( p, 0777 );
			*q = '/';
			if ( s == ']' )
				break;
		}
		++q;
	}
	++q;	/* both ufn and p point to path and q points to start of filename in the ufn string. */
	/* Make a copy of the directory */
/*	justFileName = q; */
	if ( !dflag )
	{
		strcpy( ufn, q );	/* not keeping the directory structure so toss the path */
		file->altUPfName[0] = '.'; /* alternate file starts with a '.' */
		strncpy(file->altUPfName+1, q, sizeof(file->altUPfName)-2);
	}
	else
	{
		int sLen = q-ufn;	/* Get length of path */
		memcpy(file->altUPfName, ufn, sLen); /* duplicate the path */
		file->altUPfName[sLen] = '.'; /* start alternate filename with a '.' */
		file->altUfNameOnly = file->altUPfName+sLen;
		strncpy(file->altUPfName + sLen + 1, q, sizeof(file->altUPfName)-sLen-2); /* copy rest of filename */
	}
	/* strip off the version number and possibly fix the filename's case */
	while ( *q && *q != ';' )
	{
		if ( *q == '.' )
			ext = q;
		q++;
	}
	file->do_binary = 0;
	file->do_rat = 0;
	if ( !binaryFlag )
	{
		const char *snarkMsg=NULL;
		file->do_rat = (file->recatt & ((1 << FAB_dol_V_FTN) | (1 << FAB_dol_V_CR) | (1 << FAB_dol_V_PRN)));
		if ( ((file->recfmt & 0x1F) == FAB_dol_C_FIX) || ((file->recfmt & 0x1F) == FAB_dol_C_FIX11) )
			snarkMsg = "Snark: process_file(): File %s is FIXED. Setting it to binary\n";
		if ( !snarkMsg && /* ((file->recfmt & 0x1F) == FAB_dol_C_VAR) && */ !file->do_rat )
			snarkMsg = "Snark: process_file(): File %s has no record attibutes. Setting it to binary\n";
		if ( snarkMsg )
		{
			printf(snarkMsg, file->ufname);
			file->savRecFmt = file->recfmt;
			file->recfmt = FAB_dol_C_RAW;
			file->do_binary = 1;
		}
	}
	else
	{
		file->recfmt = FAB_dol_C_RAW;
		file->do_binary = 1;
	}
	if ( *q == ';' )
	{
		file->versionPtr = q;
		if ( Rflag )
		{
			char *endp = NULL;
			int curVersion;
			curVersion = strtol(q+1,&endp,10);
			if ( !endp || *endp )
				curVersion = 0;
			*q = 0;
			if ( curVersion && !strcmp(lastFileName,ufn) )
			{
				if ( curVersion < lastVersionNumber )
				{
					printf( "Skipping extraction of \"%s;%d\" because it's an older version of %s;%d.\n", p, curVersion, lastFileName, lastVersionNumber );
					procf = 0;
				}
				else
					lastVersionNumber = curVersion;
			}
			else
			{
				strncpy(lastFileName,ufn,sizeof(lastFileName));
				lastVersionNumber = curVersion;
			}
			*q = ';';
		}
	}
	if ( Rflag )
		*q = '\0';
	else if ( cDelim )
		*q = cDelim;
	getRfmRatt(file,rfm,sizeof(rfm),cDelim);
	if ( file->do_binary )
	{
		strncat(ufn, rfm, sizeof(file->ufname)-1);
		file->savRecFmt = file->recfmt;
		file->recfmt = FAB_dol_C_RAW;
		file->do_binary = 1;
		file->altUPfName[0] = 0;
		file->altUfNameOnly = NULL;
	}
	else
	{
		strncat(file->altUPfName, rfm, sizeof(file->altUPfName) - 1);
	}
	if ( procf )
	{
		if ( dirfile )
		{
			procf = 0;			/* never explicitly extract directory files */
			if ( (vflag & VERB_DEBUG_LVL) )
			{
				printf( "Skipping explicit extraction of \"%s\" because it's a directory.\n", p );
			}
		}
		else
		{
			if ( ext && procf )
			{
				procf = typecmp(++ext, eflag);
			}
		}
	}
	if ( procf && wflag )
	{
		printf ( "extract %s [ny]", p );
		fflush ( stdout );
		if ( !fgets ( ans, sizeof ( ans ), stdin ) )
		{
			perror("Failed to read from stdin");
			exit(1);
		}
		if ( *ans != 'y' )
			procf = 0;
	}
	if ( procf )
	{
		FILE *fp;
		fp = fopen(p,"wb");
		if ( !fp )
		{
			printf("Snark: Failed to open '%s' for output: %s\n", file->ufname, strerror(errno));
		}
		else if ( !binaryFlag && file->altUPfName[0])
		{
			file->altf = fopen(file->altUPfName,"wb");
			if ( !file->altf )
			{
				fclose(fp);
				unlink(file->ufname);
				printf("Snark: Failed to open '%s' for output: %s\n", file->altUPfName, strerror(errno));
				fp = NULL;
			}
		}
		return fp;
	}
	else
		return( NULL );
}

/**
 * Check filetype against list of 'don't extract'.
 *
 * @param str Pointer to null terminated lowercased filetype.
 *
 * @return 
 *	@arg 0 if file is to be ignored.
 *	@arg 1 if file is not to be ignored.
 *
 * @note
 * Compares the filename type in pointed to by str
 * with our list of file types to be ignored.
 */
#ifndef n_elts
#define n_elts(x) (int)(sizeof(x)/sizeof((x)[0]))
#endif

int typecmp ( const char *str, int which )
{
	static const char * const Types0[] = {
		"exe",			/* executable image */
		"lib",			/* object library */
		"obj",			/* object file */
		NULL
	};
	static const char * const Types1[] = {
		"odl",			/* rsx overlay description file */
		"olb",			/* rsx object library */
		"pmd",			/* rsx post mortem dump */
		"sys",			/* rsx bootable system image */
		"tlb",			/* ? */
		"tlo",			/* ? */
		"tsk",			/* rsx executable image */
		"upd",			/* ? */
		NULL			/* null string terminates list */
	};
	static const char * const Types2[] = {
		"dir",			/* directory file */
		"mai",			/* mail file */
		NULL
	};
	int ii, jj;
	const char * const *list[4];
	const char * const *type;
	
	list[0] = Types2;
	if ( !which )
	{
		list[1] = Types1;
		list[2] = Types0;
		jj = 3;
	}
	else if ( which == 1 )
	{
		list[1] = Types1;
		jj = 2;
	}
	else 
	{
		jj = 1;
	}
	list[jj] = NULL;
	for (jj=0; (type=list[jj]); ++jj)
	{
		for (ii=0; *type; ++ii, ++type )
		{
			if ( strncasecmp ( str, *type, 3 ) == 0 )
				return( 0 );   /* found a match, file to be ignored */
		}
	}
	return 1;	   /* no match found keep file */
}

/**
 * Close file.
 *
 * @return Nothing.
 *
 * @note
 * Closes the file opened previously with openfile. Reports any straggling
 * errors if they can be detected at this point.
 */

static void close_file( void )
{
/*    int rfmt; */

	skipping &= ~SKIP_TO_FILE;
/*    rfmt = file.recfmt&0x1f; */
	if ( !file.directory && !(file.savRecFmt&FAB_dol_M_MAIL) )
	{
		if ( (xflag || file.inboundIndex) && file.inboundIndex != file.size )
		{
			printf( "Snark: '%s' file size is not correct. Is %d, should be %d. May be corrupt.\n",
					file.name, file.inboundIndex, file.size );
			++file.file_size_error;
		}
		if ( (vflag & VERB_FILE_RDLVL) )
		{
			printf( "File size: %d(0x%X), inboundIndex: %d(0x%X), outbountIndex: %d(0x%X), padding: %d, rec_count: %d\n",
					 file.size
					,file.size
					,file.inboundIndex
					,file.inboundIndex
					,file.outboundIndex
					,file.outboundIndex
					,file.rec_padding
					,file.rec_count
					);
		}
	}
	if ( file.extf != NULL )    /* if file previously opened */
	{
		struct utimbuf ut;

		fclose ( file.extf );	/* close it */
		file.extf = NULL;
		ut.actime = file.atime;
		ut.modtime = file.mtime;
		utime( file.ufname, &ut );
		if ( file.altf )
		{
			fclose(file.altf);
			file.altf = NULL;
			utime(file.altUPfName, &ut);
		}
		if ( (!binaryFlag && file.do_binary) || file.file_record_error || file.file_size_error || file.file_blk_error || file.file_format_error )
		{
			char refilename[MAX_FILENAME_LEN+MAX_FORMAT_LEN+32];
			int rLen, sLen, rName=0;

			if ( file.altUPfName[0] )
			{
				int pLen = file.altUfNameOnly-file.altUPfName;	/* length of path */
				int sLen = strlen(file.altUfNameOnly+1);
				memcpy(refilename,file.altUPfName,pLen);		/* copy path only */
				refilename[pLen] = 0;							/* terminate path */
				memcpy(refilename + pLen, file.altUfNameOnly+1, sLen); /* copy name sans leading '.' */
				refilename[pLen+sLen] = 0;						/* terminate filename */
			}
			else
			{
				strncpy(refilename, file.ufname, sizeof(refilename) - 1);
			}
			sLen = rLen = strlen(refilename);
			if ( file.file_record_error )
			{
				if ( file.altUPfName[0] )
					rLen += snprintf(refilename + rLen, sizeof(refilename) - rLen, "%cisCorruptAt%c%d", cDelim, cDelim, file.altErrorIndex);
				else
					rLen += snprintf(refilename + rLen, sizeof(refilename) - rLen, "%cisCorruptAt%c%d", cDelim, cDelim, file.errorIndex);
			}
			else if ( file.file_size_error )
				rLen += snprintf(refilename + rLen, sizeof(refilename) - rLen, "%cwrongSize", cDelim);
			else if ( file.file_blk_error )
				rLen += snprintf(refilename + rLen, sizeof(refilename) - rLen, "%cfailedBlkDecode", cDelim);
			else if ( file.file_format_error )
				rLen += snprintf(refilename + rLen, sizeof(refilename) - rLen, "%cundefinedFormat", cDelim);
			rName = rLen != sLen;	/* Check to see if anything changed */
			if ( file.altUPfName[0] )
			{
				/* We wrote a binary file with the altUfName */
				unlink(file.ufname);	/* toss the normal output file */
				rename(file.altUPfName, refilename); /* and make the binary file the one we want */
				rName = 1;
			}
			else
			{
				if ( file.altUPfName[0] )
					unlink(file.altUPfName); /* The ordinary output is okay, so toss the binary version */
				/* if a rename is required, do it. */
				if ( rName )
					rename(file.ufname, refilename);
			}
			if ( rName )
			{
				if ( file.file_record_error || file.file_blk_error || file.file_size_error )
					printf( "Snark: close_file(): Found file errors during copy. Renamed '%s' to '%s'\n",
							 file.ufname ,refilename );
				else
					printf( "Snark: close_file(): Forced binary mode. Renamed '%s' to '%s'\n",
						   file.ufname, refilename);
			}
			else
				printf( "Snark: close_file(): Forced binary mode. File renamed to '%s'\n",
					   file.ufname);
		}
		else
		{
			if ( file.altUPfName[0] )
				unlink(file.altUPfName);
		}
	}
	memset( &file, 0, sizeof( file ) );
}

/**
 * Convert the 64 bit VMS time to the 32 bit unix time.
 *
 * @param text Pointer to 8 byte VMS timestamp in VAX format.
 *
 * @return
 *	0 if no time specified.
 *	non-zero unix time in seconds since 1-jan-1970:00:00:00 GMT.
 */
static time_t vms2unixsecs( unsigned char *text )
{
	unsigned long long vmstime, vmsepoch;

	vmstime = getu32(text+4);
	vmstime = (vmstime<<32) | getu32( text );
	if ( !vmstime )
		return(time_t)0;   /* no time specified */
	vmstime /= 10000000LL;	/* Compute vms time in seconds (10^7 ticks per second) */
/* Get seconds between VMS epoch, 17-nov-1858:00:00:00 and unix epoch 1-jan-1970:00:00:00 */
	vmsepoch = 3506716800LL;
/*
 * Very curious anomoly here. I found numerous old savesets 
 * (created with VMS 3.x and 4.x) where this conversion produced
 * unix timestamps approx 4 years and some days too young.
 * I emperically determined the vmsepoch constant should be
 * 3646368000LL to make their dates work out correctly.
 * Was there a change to the VMS epoch with a later version
 * of VMS? What's going on here?
 *
 * TODO: Make this fix dynamic. I.e. look at the VMS version
 * if see if we need to apply this.
 */
#if 0
	vmsepoch += 139651200LL;
#endif
	vmstime -= vmsepoch;	/* convert to Unix epoch (1-jan-1970 00:00:00 GMT) */
	return(time_t)vmstime;
}

/**
 * Get unix time string.
 *
 * @param vtime Unix time.
 *
 * @return Pointer to null terminated ascii string containing unix time (sans \n)
 */

static const char *vms2unixtime( time_t vtime )
{
	char *ans, *cp;
	if ( !vtime )
		return "<none specified>";
	cp = ans = ctime( &vtime );
	while ( *cp )
	{
		if ( *cp == '\n' )	/* clip off trailing newline */
		{
			*cp = 0;
			break;
		}
		++cp;
	}
	return ans;
}

/**
 * Process a file block.
 *
 * @param buffer Pointer to file record.
 * @param rsize Number of bytes in record.
 *
 * @return nothing.
 *
 * @note
 * File record will be decoded and errors logged as appropriate.
 * If decode or other errors, bits in @e skipping will be set.
 * File will be opened if appropriate.
 */

void process_file ( unsigned char *buffer, int rsize )
{
	unsigned char *data;
	short dsize, dtype;
	int ii, cc, subf=0;
	int procf = 0;

	close_file();

	/* check the header word */
	if ( buffer[0] != 1 || buffer[1] != 1 )
	{
		printf ( "Snark: invalid file record header. Expected 01 01, found %02X %02X\n", buffer[0], buffer[1] );
		skipping |= SKIP_TO_FILE;	/* Skip to next file block */
		++saveSet_errors;
		return;
	}

	cc = 2;
	while ( cc <= rsize-4 )
	{
		struct bsa *bsa;
		int clen;

		bsa = (struct bsa *)( buffer + cc );
		dsize = GETU16( bsa->bsa_dol_w_size );
		dtype = GETU16( bsa->bsa_dol_w_type );
		data  = (unsigned char *)bsa->bsa_dol_t_text;

		if ( dsize < 0 || dsize+cc+4 > rsize )
		{
			printf( "Snark: process_file() subfield %d, type %d, found bad count of %d.\n", subf, dtype, dsize );
			++saveSet_errors;
			skipping |= SKIP_TO_FILE;	/* skip to next file block */
			return;
		}
		switch ( (int)dtype )
		{
		case FREC_END:
#if 0
			clen = cc+dsize+4;
			if ( (vflag & VERB_FILE_RDLVL) && clen < rsize-4 )
				printf( "Snark: process_file(): subfield %d, size %d, end of file list with %d bytes remaining. rsize=%d, cc=%d\n",
						subf, dsize, rsize - clen, rsize, cc );
			rsize = clen;
#else
			cc = rsize;
#endif
			break;
		case FREC_FNAME:
			clen = dsize;
			if ( clen > INT_SIZEOF(file.name)-1 )
				clen = sizeof(file.name)-1;
			memcpy( file.name, data, clen );
			file.name[ clen ] = 0;
			if ( (vflag & VERB_FILE_RDLVL) )
				printf( "File record field %2d, type FNAME, size %d. \"%s\"\n",
						subf, dsize, file.name );
			break;
		case FREC_UID:
			if ( dsize >= 4 )
			{
				file.usr = getu16(data);
				file.grp = getu16(data + 2);
			}
			if ( (vflag & VERB_FILE_RDLVL) )
				printf( "File record field %2d, type UID, size %d. usr %06o, grp %06o\n",
						subf, dsize, file.usr, file.grp );
			break;
		case FREC_FORMAT:
			file.recfmt = data[0];
			file.savRecFmt = file.recfmt;
			file.recatt = data[1];
			file.recsize = getu16( data+2 );
			/* bytes 4-7 unaccounted for.  */
			file.nblk = getu16( data+10 )
						/* Adding in the following amount is a
						   change that I brought over from
						   vmsbackup 3.1.  The comment there
						   said "subject to confirmation from
						   backup expert here" but I'll put it
						   in until someone complains.  */
						+ (64 * 1024) * getu16( data+8 );
			file.lnch = getu16( data+12 );
			if ( !file.nblk )
				file.size = 0;
			else
				file.size = (file.nblk-1) * 512 + file.lnch;
			/* byte 14 unaccounted for */
			file.vfcsize = data[15];
			if ( file.vfcsize == 0 )
				file.vfcsize = 2;
			/* bytes 16-31 unaccounted for */
			if ( (vflag & VERB_FILE_RDLVL) )
			{
				printf( "File record field %2d, type FORMAT, size %d. fmt %d, att %d, rsiz %d\n",
						subf, dsize, file.recfmt, file.recatt, file.recsize );
				printf( "                  nblk %d, lnch %d, vfcsize %d, filesize %d\n", 
						file.nblk, file.lnch, file.vfcsize, file.size );
			}
			break;
		case FREC_CTIME:
			if ( dsize >= 8 )
				file.ctime = vms2unixsecs( data );
			if ( (vflag & VERB_FILE_RDLVL) )
				printf( "File record field %2d, type CTIME, size %d. \"%s\"\n",
						subf, dsize, vms2unixtime( file.ctime ) );
			break;
		case FREC_MTIME:
			if ( dsize >= 8 )
				file.mtime = vms2unixsecs( data );
			if ( (vflag & VERB_FILE_RDLVL) )
				printf( "File record field %2d, type MTIME, size %d. \"%s\"\n",
						subf, dsize, vms2unixtime( file.mtime ) );
			break;
		case FREC_ATIME:
			if ( dsize >= 8 )
				file.atime = vms2unixsecs( data );
			if ( (vflag & VERB_FILE_RDLVL) )
				printf( "File record field %2d, type ATIME, size %d. \"%s\"\n",
						subf, dsize, vms2unixtime( file.atime ) );
			break;
		case FREC_BTIME:
			if ( dsize >= 8 )
				file.btime = vms2unixsecs( data );
			if ( (vflag & VERB_FILE_RDLVL) )
				printf( "File record field %2d, type BTIME, size %d. \"%s\"\n",
						subf, dsize, vms2unixtime( file.btime ) );
			break;
		case FREC_DIRECTORY:
			file.directory = data[0];
			if ( (vflag & VERB_FILE_RDLVL) )
				printf( "File record field %2d, type DIRECTORY, size %d. 0x%02X\n",
						subf, dsize, data[0] );
			break;
		case FREC_UNK2b:
			/* In my example, two bytes, 0x1 0x2.  */
		case FREC_UNK2c:
			/* In my example, 6 bytes,
			   0x7a 0x2 0x57 0x0 0x1 0x1.  */
		case FREC_UNK2d:
			/* In my example, 6 bytes.  hex 2b3c 2000 0000.  */
		case FREC_UNK2e:
			/* In my example, 4 bytes, 0x00000004.  Maybe
			   the allocation.  */
		case FREC_UNK30:
			/* In my example, 2 bytes.  0x44 0xee.  */
		case FREC_UNK31:
			/* In my example, 2 bytes.  hex 0000.  */
		case FREC_UNK32:
			/* In my example, 1 byte.  hex 00.  */
		case FREC_UNK33:
			/* In my example, 4 bytes.  hex 00 0000 00.  */
		case FREC_UNK35:
			/* In my example, 2 bytes.  04 00.  */
		case FREC_UNK47:
			/* In my example, 4 bytes.  01 00c6 00.  */
		case FREC_UNK48:
			/* In my example, 2 bytes.  88 aa.  */
		case FREC_UNK4a:
			/* In my example, 2 bytes.  01 00.  */
			/* then comes 0x0, offset 0x2b7.  */
		case FREC_UNK4b:
			/* In my example, 2 bytes.  Hex 01 00.  */
		case FREC_UNK4e:
			/* In my example, (usually) 24 bytes.  */
		case FREC_UNK4f:
			/* In my example, 4 bytes.  05 0000 00.  */
		case FREC_UNK50:
			/* In my example, 1 byte.  hex 00.  */
		case FREC_UNK57:
			/* In my example, 1 byte.  hex 00.  */
			if ( (vflag & VERB_FILE_RDLVL) )
			{
				int jj;
				printf( "File record field %2d (UNK) type 0x%02X, size %d: ",
						subf, dtype, dsize );
				ii = 8;
				if ( ii > dsize )
					ii = dsize;
				for ( jj=0; jj < ii; ++jj )
				{
					printf( "%02X ", data[jj] );
				}
				if ( dsize > 8 )
					printf( " (+%d bytes)\n", dsize-8 );
				else
					printf( "\n" );
			}
			break;
		default:
			printf( "Snark: process_file(): subfield %d, undefined record type: %d size %d\n",
					subf, dtype, dsize );
			++saveSet_errors;
			break;
		}
		++subf;
		cc += dsize + 4;
	}

	/* open the file */
	if ( strstr(file.name,".MAI") )
	{
		file.recfmt |= FAB_dol_M_MAIL;
		file.savRecFmt = file.recfmt;
	}
	procf = 0;
	if ( goptind < gargc )
	{
		for ( ii = goptind; ii < gargc; ii++ )
		{
			procf |= match ( file.name, gargv[ii] );
		}
	}
	else
		procf = 1;
	if ( procf )
	{
		if ( tflag )
		{
			char rfm[MAX_FORMAT_LEN];
			getRfmRatt(&file,rfm,sizeof(rfm), cDelim);
			printf ( " %-35s %8d (%s)%s\n", file.name, file.size, rfm, file.size < 0 ? " (IGNORED!!!)" : "" );
		}
		if ( file.size < 0 )
		{
			if ( !tflag && xflag )
				printf ( "Snark: process_file(): %-35s not extracted due to filesize of %8d\n",
						 file.name, file.size );
			++file.file_size_error;
			++saveSet_errors;
			skipping |= SKIP_TO_FILE;	/* this is bad */
			return;
		}
		if ( file.directory
			 || (file.recfmt&FAB_dol_M_MAIL)
		   )
		{
			skipping |= SKIP_TO_FILE;	/* ignore this file since the types are bogus */
			if ( (vflag&VERB_FILE_RDLVL) )
			{
				printf( "Skipping file due to it being a dir or mail file or recsize is 0.\n" );
			}
			return;
		}

		if ( xflag )
		{
			/* open file */
			file.extf = openfile ( &file );
			if ( file.extf != NULL && vflag )
				printf ( "extracting %s\n", file.name );
		}
	}
}

/**
 *  Process a summary block record.
 *
 * @param buffer Pointer to record.
 * @param rsize number of bytes in record.
 *
 * @return nothing.
 *
 * @note
 * Summary info will be sent to stdout if operating under -t or a verbose level.
 */

void process_summary ( unsigned char *buffer, unsigned short rsize )
{
/* TODO: Preclear and stash select info which may be used at some future date. */
	/* check the header word */
	if ( buffer[0] != 1 || buffer[1] != 1 )
	{
		printf ( "Snark: invalid summary record header. Expected 01 01, found %02X %02X\n",
				 buffer[0], buffer[1] );
		++saveSet_errors;
		skipping |= SKIP_TO_BLOCK;	/* Skip to next block */
		return;
	}

	if ( tflag || (vflag & VERB_LVL) )
	{
		int clen, cc, subf=0;

		printf( "\nHeader processing. rsize=%d\n", rsize );
		cc = 2;
		buff_cnt = 0;
		while ( cc <= (int)rsize-4 )
		{
			struct bsa *bsa;
			int dsize, dtype;
			unsigned char *text;
			char tbuff[256];

			bsa = ( struct bsa *)(buffer+cc);
			dsize = GETU16( bsa->bsa_dol_w_size );
			dtype = GETU16( bsa->bsa_dol_w_type );
			text = (unsigned char *)bsa->bsa_dol_t_text;
			cc += dsize+4;
			++subf;
			switch ( dtype )
			{
			case SUMM_END:
				printf("%02d: End header. cc=%d, dsize=%d, rsize=%d\n", subf, cc, dsize, rsize );
				break;
			case SUMM_SSNAME:
				clen = dsize;
				if ( clen > INT_SIZEOF(tbuff)-1 )
					clen = sizeof(tbuff)-1;
				memcpy( tbuff, text, clen );
				tbuff[clen] = 0;
				printf( "%02d: Saveset Name: \"%s\"\n", subf, tbuff );
				continue;
			case SUMM_CMDLINE:
				clen = dsize;
				if ( clen > INT_SIZEOF(tbuff)-1 )
					clen = sizeof(tbuff)-1;
				memcpy( tbuff, text, clen );
				tbuff[clen] = 0;
				printf( "%02d: Command:      \"%s\"\n", subf, tbuff );
				continue;
			case SUMM_COMMENT:
				clen = dsize;
				if ( clen > INT_SIZEOF(tbuff)-1 )
					clen = sizeof(tbuff)-1;
				memcpy( tbuff, text, clen );
				tbuff[clen] = 0;
				printf( "%02d: Comment:      \"%s\"\n", subf, tbuff );
				continue;
			case SUMM_USER:
				clen = dsize;
				if ( clen > INT_SIZEOF(tbuff)-1 )
					clen = sizeof(tbuff)-1;
				memcpy( tbuff, text, clen );
				tbuff[clen] = 0;
				printf( "%02d: Written by:   \"%s\"\n", subf, tbuff );
				continue;
			case SUMM_UID:
				if ( dsize >= 4 )
				{
					int uid, gid;
					uid = ((text[1]&0xFF)<<8) | (text[0]&0xFF);
					gid = ((text[3]&0xFF)<<8) | (text[2]&0xFF);
					printf( "%02d: UID:          [%06o,%06o]\n", subf, gid, uid );
				}
				continue;
			case SUMM_CTIME:
				if ( dsize == 8 )
				{
					printf( "%02d: Created:      \"%s\"\n", subf, vms2unixtime( vms2unixsecs( text ) ) );
				}
				continue;
			case SUMM_OSCODE:
				if ( dsize >= 2 )
				{
					int oscode;
					oscode = (((text[1]&0xFF)<<8)|(text[0]&0xFF));
					if ( oscode == SUMM_OSCODE_AXP )
						printf( "%02d: OS:           \"AXP/VMS\"\n", subf );
					else if ( oscode == SUMM_OSCODE_VAX )
						printf( "%02d: OS:           \"VAX/VMS\"\n", subf );
					else
						printf( "%02d: OS:           \"Unknown 0x%04X\"\n", subf, oscode );
				}
				continue;
			case SUMM_OSVERSION:
				clen = dsize;
				if ( clen > INT_SIZEOF(tbuff)-1 )
					clen = sizeof(tbuff)-1;
				memcpy( tbuff, text, clen );
				tbuff[clen] = 0;
				printf( "%02d: OS Version:   \"%s\"\n", subf, tbuff );
				continue;
			case SUMM_NODENAME:
				clen = dsize;
				if ( clen > INT_SIZEOF(tbuff)-1 )
					clen = sizeof(tbuff)-1;
				memcpy( tbuff, text, clen );
				tbuff[clen] = 0;
				printf( "%02d: Node:         \"%s\"\n", subf, tbuff );
				continue;
			case SUMM_PID:
				if ( dsize == 4 )
				{
					unsigned long id;
					id = GETU32( text );
					printf( "%02d: CPUPID:       0x%08lX\n", subf, id );
				}
				continue;
			case SUMM_DEVICE:
				clen = dsize;
				if ( clen > INT_SIZEOF(tbuff)-1 )
					clen = sizeof(tbuff)-1;
				memcpy( tbuff, text, clen );
				tbuff[clen] = 0;
				printf( "%02d: Device:       \"%s\"\n", subf, tbuff );
				continue;
			case SUMM_BCKVERSION:
				clen = dsize;
				if ( clen > INT_SIZEOF(tbuff)-1 )
					clen = sizeof(tbuff)-1;
				memcpy( tbuff, text, clen );
				tbuff[clen] = 0;
				printf( "%02d: Backup Ver:   \"%s\"\n", subf, tbuff );
				continue;
			case SUMM_BLOCKSIZE:
				if ( dsize == 4 )
				{
					unsigned long blk;
					blk = getu32( text );
					printf( "%02d: Blocksize:    %ld\n", subf, blk );
				}
				continue;
			case SUMM_GROUPSIZE:
				if ( dsize == 2 )
				{
					int grp;
					grp = getu16( text );
					printf( "%02d: Groupsize:    %d\n", subf, grp );
				}
				continue;
			case SUMM_BUFFCOUNT:
				if ( dsize == 2 )
				{
					buff_cnt = getu16( text );
					printf( "%02d: Buffcnt:      %d\n", subf, buff_cnt );
				}
				continue;
			default:
				printf( "Snark: %02d: Summary record type %d(0x%X) size %d undefined. cc=%d, rsize=%d\n", subf, dtype, dtype, dsize, cc, rsize );
				continue;
			}
			break;
		}       
#if 0
		if ( buff_cnt > MAX_BUFFCOUNT )
			printf( "Snark: Warning: /BUFFCOUNT=%d. MAX_BUFFCOUNT=%d.\n", buff_cnt, MAX_BUFFCOUNT );
#endif
		printf( "\n" );
	}
	return;
}

/**
 *  Process a virtual block record (file content record).
 *
 * @param buffer Pointer to data.
 * @param rsize Number of bytes in buffer.
 *
 * @return nothing.
 *
 * @note
 * Decode errors will be sent to stdout and @e skipping will be
 * set appropriately (the remainder of the file will be skipped).
 * If no errors detected, contents may be written to previously
 * opened file if appropriate.
 */

void process_vbn ( unsigned char *buffer, unsigned short rsize )
{
	int buffIndex, tlen;
	
	buffIndex = 0;
	if ( (vflag & (VERB_LVL|VERB_FILE_RDLVL|VERB_FILE_WRLVL)) )
	{
		printf("process_vbn(): Entry rsize=%d(0x%0X). recfmt=%d, recatt=0x%02X, rec_count=%d, do_binary=%d, do_rat=%d, file_state=%d\n",
			    rsize
			   ,rsize
			   ,file.recfmt
			   ,file.recatt
			   ,file.rec_count
			   ,file.do_binary
			   ,file.do_rat
			   ,file.file_state
			   );
		printf("\tinboundIndex=%d(0x%X), file_size=%d(0x%X), buff: %02X %02X %02X %02X %02X %02X %02X %02X ...\n",
			    file.inboundIndex
			   ,file.inboundIndex
			   ,file.size
			   ,file.size
			   ,buffer[0], buffer[1], buffer[2], buffer[3]
			   ,buffer[4], buffer[5], buffer[6], buffer[7]
			   );
	}
	if ( file.inboundIndex >= file.size )
	{
		if ( !strlen( file.name ) )
		{
			skipping |= SKIP_TO_FILE;		/* What the hell are we doing here? */
			return;
		}
		else
			printf( "Snark: process_vbn(): Filesize of %s is too big. Is %d, expected %d\n",
					file.name, file.inboundIndex, file.size );
	}
	while ( file.inboundIndex < file.size && buffIndex < rsize )
	{
		switch ( (file.recfmt&0x1F) )
		{
		case FAB_dol_C_FIX:
		case FAB_dol_C_FIX11:
		case FAB_dol_C_STM:
		case FAB_dol_C_STMLF:
		case FAB_dol_C_STMCR:
		case FAB_dol_C_RAW:
			file.reclen = rsize;		/* assume max */
			if ( file.inboundIndex + file.reclen > file.size )
				file.reclen = file.size - file.inboundIndex;
			if ( file.extf )
			{
				if ( (vflag & VERB_FILE_WRLVL) )
				{
					printf( "Writing %4d(0x%X) bytes. buffIndex=%d(0x%X), inboundIndex=%d(0x%X), outboundIndex=%d(0x%X), recfmt=%d, recatt=0x%02X\n",
							 file.reclen
							,file.reclen
							,buffIndex
							,buffIndex
						    ,file.inboundIndex
							,file.inboundIndex
							,file.outboundIndex
							,file.outboundIndex
							,file.recfmt
							,file.recatt
							);
				}
				if ( fwrite(buffer + buffIndex, 1, file.reclen, file.extf) != file.reclen )
				{
#if HAVE_STRERROR
					printf("snark: Failed to write (fixed length) %d bytes to '%s': %s\n", file.reclen, file.name, strerror(errno));
#else
					perror("snark: Failed to write record");
#endif
					file.inboundIndex = file.size;
					skipping |= SKIP_TO_FILE;
					file.file_state = GET_IDLE;
					return;
				}
				file.outboundIndex += file.reclen;
/* Not sure whether this is a good thing or not. Some FIXed files have a CR attribute which won't be right for example, .EXE, etc. */
/* So for, now, just don't do it. */
#if 0
				if ( (file.recatt & (1<<FAB_dol_V_CR)) && ((file.recfmt&0x1F) == FAB_dol_C_FIX || (file.recfmt&0x1F) == FAB_dol_C_FIX11) )
				{
					if ( fwrite("\n", 1, 1, file.extf) != 1 )
					{
	#if HAVE_STRERROR
						printf("snark: Failed to write (fixed length) %d bytes to '%s': %s\n", file.reclen, file.name, strerror(errno));
	#else
						perror("snark: Failed to write record");
	#endif
						file.inboundIndex = file.size;
						skipping |= SKIP_TO_FILE;
						file.file_state = GET_IDLE;
						return;
					}
					++file.outboundIndex;
				}
#endif
				if ( file.altf )
				{
					if ( fwrite(buffer + buffIndex, 1, file.reclen, file.altf) != file.reclen )
					{
	#if HAVE_STRERROR
						printf("snark: Failed to write (binary image) %d bytes to '%s': %s\n", 2, file.altUPfName, strerror(errno));
	#else
						perror("snark: Failed to write record");
	#endif
						file.inboundIndex = file.size;
						skipping |= SKIP_TO_FILE;
						file.file_state = GET_IDLE;
						return;
					}
					file.altboundIndex += file.reclen;
				}
			}
			buffIndex += file.reclen;
			file.inboundIndex += file.reclen;
			file.reclen = 0;
			continue;

		case FAB_dol_C_VAR:
		case FAB_dol_C_VFC:
			switch (file.file_state)
			{
			case GET_IDLE:
				if ( file.reclen != 0 )
				{
					file.file_state = GET_DATA;
					continue;
				}
				file.file_state = GET_RCD_COUNT;
/*				Fall through to GET_RCD_COUNT */
			case GET_RCD_COUNT:
				if ( file.altf )
				{
					if ( fwrite(buffer+buffIndex, 1, 2, file.altf) != 2 )
					{
	#if HAVE_STRERROR
						printf("snark: Failed to write (binary image) %d bytes to '%s': %s\n", 2, file.altUPfName, strerror(errno));
	#else
						perror("snark: Failed to write record");
	#endif
						file.inboundIndex = file.size;
						skipping |= SKIP_TO_FILE;
						file.file_state = GET_IDLE;
						return;
					}
					file.altboundIndex += 2;
				}
				file.reclen = getu16( (unsigned char *)buffer + buffIndex );
				buffIndex += 2;
				file.inboundIndex += 2;		/* This has to match all bytes found in file */
				file.file_state = ((file.recfmt&0x1F) == FAB_dol_C_VFC && file.vfcsize == 2) ? GET_VFC:GET_DATA;
				if ( (vflag & VERB_FILE_RDLVL) )
				{
					printf ( "New record mark: GET_RCD_COUNT: reclen = %5d(0x%04X), buffIndex = %5d(0x%04X), rsize = %5d(0x%04X), rec_count=%d, nextState=%d\n",
							  file.reclen
							 ,file.reclen
							 ,buffIndex-2
							 ,buffIndex-2
							 ,rsize
							 ,rsize
							 ,file.rec_count
							 ,file.file_state
							 );
				}
				++file.rec_count;
				file.fix = file.reclen;
				file.do_vfc = 0;
				if ( file.reclen == 0xFFFF )
				{
					if ( (vflag & VERB_FILE_RDLVL) )
					{
						printf("Reached EOF. inboundIndex=%d(0x%X), file.size=%d(0x%X), buffIndex=%d(0x%X), rsize=%d(0x%X), inboundIndex+(rsize-buffIndex)=%d(0x%X)",
							    file.inboundIndex
							   ,file.inboundIndex
							   ,file.size
							   ,file.size
							   ,buffIndex
							   ,buffIndex
							   ,rsize
							   ,rsize
							   ,file.inboundIndex+(rsize-buffIndex)
							   ,file.inboundIndex+(rsize-buffIndex)
							   );
						printf( "\trecfmt=%d, reclen=%d(0x%X), recatt=0x%02X, file.padding=%d. Skipping to next file.\n",
								 file.recfmt
								,file.reclen
								,file.reclen
								,file.recatt
								,file.rec_padding
								 );
					}
					if ( file.inboundIndex > file.size )
						printf("Snark: '%s' file count, %d, exceeded file size, %d by %d bytes on '%s'\n", file.name, file.inboundIndex, file.size, file.inboundIndex-file.size, file.name );
					file.inboundIndex = file.size;
					skipping |= SKIP_TO_FILE;
					file.file_state = GET_IDLE;
					return;
				}
				if ( file.reclen > file.recsize+file.vfcsize )
				{
					printf( "Snark: '%s' (buffIndex=%d(0x%X)) record length of %d (0x%04X;'%c','%c') is invalid. Must be %d >= x >= 0. Converting file type from %d to %d(RAW) to finish write.\n",
							file.name,
							buffIndex,
							buffIndex,
							file.reclen, file.reclen, isprint(file.reclen&0xFF) ? (file.reclen&0xFF) : '.', isprint((file.reclen>>8)&0xFF) ? (file.reclen>>8)&0xFF : '.',
							file.recsize, file.recfmt, FAB_dol_C_RAW );
					file.recfmt = FAB_dol_C_RAW;
					if ( !file.file_record_error )
					{
						file.errorIndex = file.inboundIndex-2;
						++file.file_record_error;
						if ( file.altf )
							file.altErrorIndex = file.altboundIndex-2;
					}
/*					buffIndex -= 2;  */               /* backup over the record length */
					continue;
				}
				if ( file.inboundIndex < file.size )
					continue;
				break;
			case GET_VFC:
				if ( file.altf )
				{
					if ( fwrite(buffer+buffIndex, 1, 2, file.altf) != 2 )
					{
	#if HAVE_STRERROR
						printf("snark: Failed to write (binary image) %d VFC bytes to '%s': %s\n", 2, file.altUPfName, strerror(errno));
	#else
						perror("snark: Failed to write record");
	#endif
						file.inboundIndex = file.size;
						skipping |= SKIP_TO_FILE;
						file.file_state = GET_IDLE;
						return;
					}
					file.altboundIndex += 2;
				}
				file.vfc0 = buffer[buffIndex];
				file.vfc1 = buffer[buffIndex+1];
				/* vfcflag == 0, eat the vfc bytes and do normal text processing if appropriate.
				 *            1, process the vfc characters and insert nl's, cr's and ff's as requested.
				 *            2, just put the 2 vfc bytes in the output record unchanged.
				 */
				if ( vfcflag == 1 )
				{
					file.do_vfc = 1;			/* do vfc handling */
					buffIndex += 2;
					file.reclen -= 2;
				}
				else if ( vfcflag != 2 )
				{
					buffIndex += 2;			/* eat the VFC bytes */
					file.reclen -= 2;
				}
				file.inboundIndex += 2;		/* VFC bytes get counted in the running index */
				if ( (vflag & VERB_FILE_RDLVL) )
					printf ( "New record mark: GET_VFC: reclen = %5d, buffIndex = %5d(0x%04X), rsize = %5d(0x%04X), vfc0=0x%02X, vfc1=0x%02X\n",
							 file.reclen, buffIndex-2, buffIndex-2, rsize, rsize, file.vfc0, file.vfc1 );
				if ( file.do_vfc )
				{
					static const char OneNl[]="\n";
					const char *preCode;
					int preNum;

					/* So here's an attempt at handling fortran carriage control */
					preCode = NULL;
					preNum = 0;
					/* vfc0 spec. Char has:
					 *  0  - (as in nul) no leading carriage control
					 * ' ' - (space) Normal: \n followed by text followed by \r
					 * '$' - Prompt: \n followed by text, no \r at end of line
					 * '+' - Overstrike: text followed by \r
					 * '0' - Double space: \n \n followed by text followed by \r
					 * '1' - Formfeed: \f followed by text followed by \r
					 * any - any other is same as Normal above 
					 * Despite the comments above about the end-of-record
					 * char, it is determined by vfc1 and handled separately below.
					 */
					switch (file.vfc0)
					{
					case 0:				/* No carriage control at all on this record */
						break;
					default:
					case ' ':			/* normal. \n text \cr */
						preCode = OneNl;
						preNum = 1;
						break;
					case '$':			/* Prompt: \n - buffer */
						preCode = OneNl;
						preNum = 1;
						break;
					case '+':			/* Overstrike: buffer - \r */
						break;
					case '0':			/* Double space: \n\n text \r */
						preCode = "\n\n";
						preNum = 2;
						break;
					case '1':
						preCode = "\f";	/* \f - buffer - \r */
						preNum = 1;
						break;
					}
					if ( file.extf && preNum && preCode )
					{
						if ( (vflag & VERB_FILE_WRLVL)  )
						{
							printf("Writing %d byte%s of leading VFC. vfc0=0x%02X, vfc1=0x%02X, preCode[0]=0x%02X\n",
									preNum,
									preNum == 1 ? "":"s",
									file.vfc0,
									file.vfc1,
									preCode[0] );
						}
						if ( fwrite(preCode, 1, preNum, file.extf) != (unsigned int)preNum ) /* write it */
						{
#if HAVE_STRERROR
							printf("snark: Failed to write (vfc header) %d byte(s) to '%s': %s\n", preNum, file.name, strerror(errno));
							file.inboundIndex = file.size;
							skipping |= SKIP_TO_FILE;
							file.file_state = GET_IDLE;
#else
							perror("snark: Failed to write vfc header");
#endif
							return;
						}
						file.outboundIndex += preNum;
					}
				}
				file.file_state = GET_DATA;
				if ( file.inboundIndex < file.size )
					continue;
				break;
			case GET_DATA:
				break;
			}
			/* End of switch(file_state) */
			tlen = file.reclen;		/* assume whole record is in buffer */
			if ( tlen+buffIndex > rsize )	/* if record length is longer than what's in the buffer */
				tlen = rsize-buffIndex;	/* trim to remaining buffer size */
			if ( tlen )
			{
				/* If there's something to write */
				if ( file.inboundIndex + tlen > file.size )
				{
					printf( "Snark: '%s' process_vbn(): May be a problem with file.\n", file.name );
					printf( "Snark: '%s' process_vbn(): Attempt to write %d bytes more than filesize says to. Trimming to %d\n",
							file.name, file.inboundIndex+tlen - file.size, file.size-file.inboundIndex );
					tlen = file.size-file.inboundIndex;
				}
				if ( file.extf )
				{
					if ( (vflag & VERB_FILE_WRLVL) )
					{
						printf( "Writing %4d byte%s. recfmt=%d, recatt=0x%02X, reclen=%d(0x%X)\n",
								tlen, tlen == 1 ? "":"s", file.recfmt, file.recatt, file.reclen, file.reclen );
					}
					if ( file.altf )
					{
						if ( (int)fwrite(buffer+buffIndex, 1, tlen, file.altf ) != tlen )
						{
		#if HAVE_STRERROR
							printf("snark: Failed to write (binary image) %d bytes to '%s': %s\n", 2, file.altUPfName, strerror(errno));
		#else
							perror("snark: Failed to write record");
		#endif
							file.inboundIndex = file.size;
							skipping |= SKIP_TO_FILE;
							file.file_state = GET_IDLE;
							return;
						}
						file.altboundIndex += tlen;
					}
					if ( (int)fwrite( buffer+buffIndex, 1, tlen, file.extf ) != tlen) /* write as much as we can at once */
					{
#if HAVE_STRERROR
						printf("snark: Failed to write (var/vfc record) %d bytes to '%s': %s\n", tlen, file.name, strerror(errno));
#else
						perror("snark: Failed to write var/vfc record");
#endif
						file.inboundIndex = file.size;
						skipping |= SKIP_TO_FILE;
						file.file_state = GET_IDLE;
						return;
					}
					file.outboundIndex += tlen;
				}
				buffIndex += tlen;				/* advance index */
				file.reclen -= tlen;	/* take from remaining record length */
				file.inboundIndex += tlen;
			}
			if ( !file.reclen )
			{
				/* We've reached a blank line */
				if ( file.extf )
				{
					/* If record attributes indicate to terminate line */
					if ( file.do_vfc )
					{
						/* vfc1 spec. Bits:
						 * 7 6 5 4 3 2 1 0
						 * 0 0 0 0 0 0 0 0 - no trailing carriage control
						 * 0 x x x x x x x - bits 6-0 indicate how many nl's to output followed by a cr (\r)
						 * 1 0 0 x x x x x - bits 4-0 describe the end-of-record char (normally a 0x0D: \r)
						 * 1 0 1 * * * * * - all other conditions = just one \r
						 * 1 1 0 0 x x x x - bits 3-0 describe bits to send to VFU. If no VFU, just one \r 
						 * 1 1 0 1 * * * * - all other conditions = just one \r
						 * 1 1 1 * * * * * - all other conditions = just one \r
						 *
						 * where 'x' can be 0 or 1 and means something and '*' means not used.
						 */
						if ( file.vfc1 )
						{
							char nls[128];
							int postNum=0;
							const char *postCode=NULL;
							int code = file.vfc1>>5;	/* get top 3 bits of vfc1 */
							switch ((code&7))
							{
							case 0:
							case 1:
							case 2:
							case 3:
								memset(nls,'\n',file.vfc1);	/* assume nl's */
								nls[file.vfc1] = '\r';		/* a cr always follows all the nl's */
								postCode = nls;
								postNum = file.vfc1+1;
								break;
							case 4:
								nls[0] = file.vfc1&0x1F;
								postCode = nls;
								postNum = 1;
								break;
							case 5: /* Not used*/
							case 6: /* special VFU stuff (not used) */
							case 7:	/* Not used */
								nls[0] = '\r';
								postCode = nls;
								postNum = 1;
								break;
							}
							if ( postNum && postCode )
							{
								if ( (vflag & VERB_FILE_WRLVL) )
									printf( "Writing %d byte%s of VFC tail. vfc1=0x%02X, postCode[0]=0x%02X\n",
											postNum, postNum == 1 ? "":"s", file.vfc1, postCode[0] );
								if ( (int)fwrite(postCode, 1, postNum, file.extf) != postNum ) /* write trailing character(s) */
								{
#if HAVE_STRERROR
									printf("snark: Failed to write (vfc trailer) %d bytes to '%s': %s\n", postNum, file.name, strerror(errno));
#else
									perror("snark: Failed to write (vfc trailer)");
#endif
									file.inboundIndex = file.size;
									skipping |= SKIP_TO_FILE;
									file.file_state = GET_IDLE;
									return;
								}
								file.outboundIndex += postNum;
							}
						}
					}
					else if ( file.do_rat )
					{
						if ( (vflag & VERB_FILE_WRLVL) )
							printf( "    Writing 1 byte 0x0A due to rat=0x%02X\n", file.do_rat );
						fputc( '\n', file.extf );	/* follow with newline if appropriate */
					}
				}
				if ( ((file.recfmt & 0x1F) == FAB_dol_C_VAR || (file.recfmt & 0x1F) == FAB_dol_C_VFC)  )
				{
					if ( (buffIndex & 1) )
					{
						if ( file.altf )
						{
							char chr[1];
							chr[0] = buffer[buffIndex];
							if ( fwrite(chr, 1, 1, file.altf ) != 1 )
							{
			#if HAVE_STRERROR
								printf("snark: Failed to write (binary image) %d byte filler to '%s': %s\n", 1, file.altUPfName, strerror(errno));
			#else
								perror("snark: Failed to write record");
			#endif
								file.inboundIndex = file.size;
								skipping |= SKIP_TO_FILE;
								file.file_state = GET_IDLE;
								return;
							}
							file.altboundIndex += 1;
						}
						++buffIndex;			/* round it up (all records are padded to even length) */
						++file.rec_padding;		/* this doesn't get charged against file size */
						++file.inboundIndex;	/* keep track of every byte found in input file */
					}
					file.file_state = GET_RCD_COUNT;	/* expect record count next */
				}
			}
			break;

		default:
			++saveSet_errors;
			++file.file_format_error;
			skipping |= SKIP_TO_FILE;
			printf ( "Snark: '%s' process_vbn(): Invalid record format = %d, file.inboundIndex=%d(0x%X), buffIndex=%d(0x%X), file.size=%d(0x%X)\n",
					 file.name, file.recfmt, file.inboundIndex, file.inboundIndex, buffIndex, buffIndex, file.size, file.size );
			return;
		}
	}
	if ( file.inboundIndex > file.size )
	{
		printf("Snark: '%s' process_vbn(): Hey, we've got a problem: record format=%d, buffIndex=%d, file.inboundIndex=%d(0x%X), file.size=%d(0x%X)\n",
			   file.name, file.recfmt, buffIndex, file.inboundIndex, file.inboundIndex, file.size, file.size );
	}
	if ( (vflag & VERB_FILE_RDLVL) )
	{
		if ( file.reclen )
			printf( "process_vbn(): '%s' Record straddled block. reclen=%d, recsize=%d, file_state=%d\n",
					file.name, file.reclen, file.recsize, file.file_state );
		printf( "process_vbn(): '%s' inboundIndex now %d(0x%X), filesize: %d(0x%X), padding: %d\n",
				file.name, file.inboundIndex, file.inboundIndex, file.size, file.size, file.rec_padding );
	}
	if ( file.inboundIndex >= file.size )
	{
		if ( (vflag & VERB_FILE_RDLVL) )
		{
			printf( "process_vbn(): '%s' Reached end of file. file.inboundIndex=%d(0x%X), file.size=%d(0x%X), file_state=%d. Skipping to next file.\n",
					file.name, file.inboundIndex, file.inboundIndex, file.size, file.size, file.file_state );
		}
		skipping |= SKIP_TO_FILE;
	}
}

/**
 * Get the VMS block number from the block.
 *
 * @param bptr Pointer to saveset block.
 *
 * @return
 *	@arg 0 if error in decoding the block.
 *	@arg non-zero blocknumber.
 *
 * @note
 * Will send error messages to stdout if decoding errors detected.
 */

static unsigned long get_block_number( unsigned char *bptr )
{
	unsigned long ans = 0, bsize;
	struct bbh *block_header;
	unsigned short bhsize;

	block_header = ( struct bbh * )bptr;

	bhsize = GETU16( block_header->bbh_dol_w_size );
	bsize = GETU32( block_header->bbh_dol_l_blocksize );

	/* check the validity of the header block */
	if ( bhsize != sizeof ( struct bbh ) )
	{
		printf ( "Snark: Invalid header block size. Expected %d, found %d\n", sizeof( struct bbh ), bhsize );
		return ans;
	}
	if ( bsize != 0 && bsize != (unsigned long)blocksize )
	{
		printf ( "Snark: Invalid block size. Expected %d, found %ld\n", blocksize, bsize );
		return ans;
	}
#if 0
/* TODO: Figure out how to compute the block checksum/crc and validate it here. */
#endif
	return GETU32( block_header->bbh_dol_l_number );
}

/**
 *  Process a backup block.
 *
 * @param blkptr Pointer to VMS saveset block.
 *
 * @return nothing.
 *
 * @note
 * Decodes the saveset block. If errors are detected by
 * this function or one it calls, @e skipping will have
 * bits set to indicate what should happen next.
 */

void process_block ( unsigned char *blkptr )
{
/*    unsigned short bhsize; */
	unsigned short rsize, rtype, applic;
	unsigned long bsize, ii, numb;
	struct bbh *block_header;

	skipping &= ~SKIP_TO_BLOCK;

	/* read the backup block header */
	block_header = ( struct bbh * )blkptr;
	ii = sizeof( struct bbh );

/*    bhsize = GETU16( block_header->bbh_dol_w_size ); */
	bsize = GETU32( block_header->bbh_dol_l_blocksize );

	numb = get_block_number( blkptr );
	if ( !numb )
	{
		skipping |= SKIP_TO_BLOCK;
		++saveSet_errors;
		++file.file_blk_error;
		return;
	}
	if ( numb != last_block_number+1 )
	{
		if ( numb == last_block_number )
			printf( "Snark: block %ld duplicated.\n", numb );
		else
			printf( "Snark: block %ld out of sequence. Expected %ld\n",
					numb, last_block_number+1 );
	}
	last_block_number = numb;
	applic = GETU16( block_header->bbh_dol_w_applic );
	if ( (vflag & VERB_DEBUG_LVL) )
	{
		printf ( "new block: ii = %ld, bsize = %ld, opsys=%d, subsys=%d, applic=%d, number=%ld\n",
				 ii, bsize,
				 GETU16( block_header->bbh_dol_w_opsys ),
				 GETU16( block_header->bbh_dol_w_subsys ),
				 applic,
				 numb );
	}
	if ( !bsize || applic > 1 )
	{
		if ( (vflag & VERB_DEBUG_LVL) )
		{
			if ( !bsize )
				printf( "Process_block(): Skipped block because bsize == 0\n" );
			else
				printf( "Process_block(): Skipped block because applic field is %d instead of 1.\n",
						applic );
		}
		skipping |= SKIP_TO_BLOCK;
		return;
	}
	/* read the records */
	while ( ii < bsize )
	{
		struct brh *record_header;
		/* read the backup record header */
		record_header = ( struct brh * ) (blkptr+ii);
		ii += sizeof ( struct brh );

		rtype = GETU16( record_header->brh_dol_w_rtype );
		rsize = GETU16( record_header->brh_dol_w_rsize );
		if ( (vflag & VERB_DEBUG_LVL) )
		{
			printf ( "ii=%ld, rtype=%d, rsize=%d, flags=0x%lX, addr=0x%lX\n",
					 ii, rtype, rsize,
					 GETU32( record_header->brh_dol_l_flags ),
					 GETU32( record_header->brh_dol_l_address ) );
		}
		if ( rsize+ii > bsize )	/* This is an invalid record */
		{
			printf( "Snark: rsize of %d is wrong. Cannot be more than %ld\n",
					rsize, bsize-ii );
			skipping |= SKIP_TO_BLOCK;
			++saveSet_errors;
			++file.file_record_error;
			break;
		}
		switch ( rtype )
		{
		
		case brh_dol_k_null:
			if ( (vflag & VERB_DEBUG_LVL) )
				printf ( "rtype = null\n" );
			break;

		case brh_dol_k_summary:
			if ( (vflag & VERB_DEBUG_LVL) )
				printf ( "rtype = summary\n" );
			process_summary( blkptr+ii, rsize );
			break;

		case brh_dol_k_file:
			if ( (vflag & VERB_DEBUG_LVL) )
				printf ( "rtype = file\n" );
			process_file ( blkptr+ii, rsize );
			break;

		case brh_dol_k_vbn:
			if ( (vflag & VERB_DEBUG_LVL) )
				printf ( "rtype = vbn\n" );
			if ( !(skipping&SKIP_TO_FILE) )
				process_vbn ( blkptr+ii, rsize );
			break;

		case brh_dol_k_physvol:
			if ( (vflag & VERB_DEBUG_LVL) )
				printf ( "rtype = physvol\n" );
			break;

		case brh_dol_k_lbn:
			if ( (vflag & VERB_DEBUG_LVL) )
				printf ( "rtype = lbn\n" );
			break;

		case brh_dol_k_fid:
			if ( (vflag & VERB_DEBUG_LVL) )
				printf ( "rtype = fid\n" );
			break;

		default:
			printf ( "Snark: process_block(): %d is an invalid record type.\n", rtype );
			++saveSet_errors;
			if ( file.extf )
			{
				printf( "Snark: Skipping rest of %s\n", file.name );
				++file.file_record_error;
			}
			skipping |= SKIP_TO_BLOCK|SKIP_TO_FILE;
			return;
/*	    exit ( 1 ); */
		}
		ii += rsize;
	}
}

static int tape_marks;		/*!< running bit mask of tape marks read */

/**
 * Get a record from tape or disk.
 *
 * @param buff Pointer to buffer into which to read record.
 * @param len Size of buffer.
 *
 * @return
 *	@arg 0 Indicates an TM read.
 *	@arg non-zero-positive Number of bytes read into @e buff.
 *	@arg negative Error code from OS.
 *
 * @note
 * Will not advance beyond two consequitive tape marks.
 */

static int read_record( unsigned char *buff, int len )
{
	unsigned char freclen[4], iFreclen[4];
	int sts, tmpLen, reclen, Ireclen;
#if  1 || WIN32
	int tmpRecCnt;
#endif
	if ( (tape_marks&3) == 3 )
	{
		if ( (vflag & VERB_DEBUG_LVL) )
			printf( "read_record: returns 0 cuz read 2 TMs in a row.\n" );
		return 0;				/* reached EOT, can't advance */
	}
	tape_marks <<= 1;
	if ( !iflag && !Iflag )
	{
		sts = read( fd, buff, len );		/* Read from the tape */
		if ( sts <= 0 )				/* A 0 is a tape mark, a -x is an error */
		{
			tape_marks |= 1;
		}
		if ( (vflag & VERB_DEBUG_LVL) )
			printf( "read_record: returns %d.\n", sts );
		return sts;
	}
/*
 * Format of our 'i' disk image of a tape is:
 *     4 byte record length in bytes, little endian, followed by 'n' bytes of data.
 * Format of simh 'I' disk image of a tape is the same except it also has the 4 byte count following the 'n' bytes of data. TM's excluded.
 */
	sts = read( fd, freclen, 4 );		/* Read the record length from disk */
	if ( sts <= 0 )				/* A 0 is EOF. a -x is an error */
	{
		tape_marks |= 1;			/* pretend we got a tape mark */
		if ( (vflag & VERB_DEBUG_LVL) )
			printf( "read_record: returns %d due to error or EOF.\n", sts );
		return sts;
	}
	reclen = getu32( freclen );			/* convert endianess as appropriate */
	if ( !reclen )				/* A 0 length record is a fake tape mark */
	{
		tape_marks |= 1;			/* Reached a fake tape mark */
		if ( (vflag & VERB_DEBUG_LVL) )
			printf( "read_record: returns 0 cuz found fake TM.\n" );
		return 0;
	}
	if ( reclen > len )
	{
		printf( "Snark: WARNING: Record of %d bytes too long for user %d buffer.\n", reclen, len );
		sts = read( fd, buff, len );		/* give 'em what he wants */
		lseek( fd, reclen-len, SEEK_CUR );	/* Skip remainder of record */
		if ( (vflag & VERB_DEBUG_LVL) )
			printf( "read_record: returns %d.\n", sts );
		return sts;
	}
	if ( reclen < len )
		len = reclen;				/* trim byte count to actual record length */
#if 1 || WIN32
	tmpRecCnt = 0;
	tmpLen = 0;
	while ( tmpLen < len )
	{
		sts = read(fd, buff+tmpLen, len-tmpLen);
		if ( sts <= 0 )
		{
			tape_marks |= 1;			/* pretend we got a tape mark */
			if ( (vflag & VERB_DEBUG_LVL) )
				printf( "read_record: read(%d) returns %d due to error or EOF on attempt %d. tmpLen=%d\n", len-tmpLen, sts, tmpRecCnt, tmpLen );
			return sts;
		}
		if ( (vflag & VERB_DEBUG_LVL) )
			printf( "read_record: attempt %d, read(%d) returns %d.\n", tmpRecCnt, len-tmpLen, sts );
		tmpLen += sts;
		++tmpRecCnt;
	}
#else
	tmpLen = read(fd, buff, len);
	if ( tmpLen <= 0 )
	{
		tape_marks |= 1;			/* pretend we got a tape mark */
		if ( (vflag & VERB_DEBUG_LVL) )
			printf( "read_record: read() returns %d due to error or EOF.\n", sts );
		return tmpLen;
	}
	if ( (vflag & VERB_DEBUG_LVL) )
		printf( "read_record: read() returns %d.\n", tmpLen );
#endif
	if ( Iflag )
	{
		sts = read( fd, iFreclen, 4 );		/* Read the end of block record length from disk */
		if ( sts <= 0 )				/* A 0 is EOF. a -x is an error */
		{
			if ( (vflag & VERB_DEBUG_LVL) )
				printf( "read_record: returns %d due to error reading SIMH record length.\n", sts );
			return sts;
		}
		Ireclen = getu32( iFreclen );			/* convert endianess as appropriate */
		if ( Ireclen != reclen )				/* it better match the starting one */
		{
			printf( "Snark: read_record: SIMH format record count mismatch. Expected %d read %d\n",  reclen, Ireclen);
			return -1;
		}
	}
	sts = reclen;
	if ( (vflag&VERB_DEBUG_U32) || ((vflag&VERB_BLOCK_LVL ) && !(vflag&VERB_DEBUG_LVL)) )
		printf("read_record: block returned %d(0x%X)\n", sts, sts );
	return sts;
}

/**
 * Skip to next tape mark.
 * Continues to call read_record until the next tape mark is reached.
 *
 * @return nothing.
 *
 */

static void skip_to_tm( void )
{
	while ( 1 )
	{
		if ( !read_record( (unsigned char *)label, sizeof( label ) ) )
			break;
	}
}

/**
 * Allocate buffers.
 *
 * @param nbuffs Number of buffers to allocate.
 * @param buffsize Size in bytes of each buffer.
 *
 * @return Nothing.
 *
 * @note
 * Program will print an error message and exit if malloc fails.
 */

static void alloc_buffers( int nbuffs, int buffsize )
{
	int ii;
	struct buff_ctl *bp;

	buffsize += 16;				/* make this a little bigger than he asked for */
/*
 * TODO: Allow this to be dynamic.
 * For now, we just ignore what he wants and always allocate a fixed number.
 */
	nbuffs = MAX_BUFFCOUNT;			/* always use a fixed number of buffers */
	if ( nbuffs+1 > num_buffers )		/* need to malloc (more) buffers */
	{
		int jj, newsz;
		ii = num_buffers;			/* old top */
		num_buffers = nbuffs+1;			/* new top */
		newsz = num_buffers*sizeof( struct buff_ctl );	/* figure out how much we will have */
		buffers = (struct buff_ctl *)realloc( buffers, newsz );	/* get new memory */
		if ( !buffers )
		{
			printf( "Snark: Failed to malloc %d bytes for buffer pointers.\n", newsz );
			exit (1);
		}
		bp = buffers + ii;			/* point to start of new area */
		jj = ii;
		for ( ; ii < num_buffers-1; ++ii , ++bp )
		{
			bp->buffer = NULL;			/* start with this empty */
			bp->next = ii+1;
			bp->amt = 0;
		}
		bp->buffer = NULL;
		bp->next = freebuffs;
		bp->amt = 0;
		freebuffs = jj;
	}
	bp = buffers+1;				/* now go through and make sure everybody has a buffptr */
	for ( ii=1; ii < num_buffers; ++ii, ++bp )
	{
		if ( buffalloc < buffsize || !bp->buffer )
		{
			bp->buffer = (unsigned char *)realloc( bp->buffer, buffsize );
			if ( !bp->buffer )
			{
				printf( "Snark: Failed to malloc %d bytes for block buffer # %d\n", 
						buffsize, ii );
				exit(1);
			}
		}
	}
	if ( buffalloc < buffsize )
		buffalloc = buffsize;			/* bump this up if appropriate */
}

/**
 * Read tape header record.
 * Search tape for next HDR1/2 records.
 *
 * @return
 *	@arg  0 Success
 *	@arg  1 Failure
 *	@arg -1 No more savesets to look for.
 */

int rdhead ( void )
{
	int marks=0, mstop, len, nfound, rptd=0, stm=0;
	char name[80];

	skipping = 0;
	total_errors += saveSet_errors;
	saveSet_errors = 0;
	nfound = 1;
	mstop = 3;				/* autostop when we get to 2 tm's */
	last_block_number = 0;		/* start all blocks at 0 */
	freeall();				/* free all the buffers */

	/* read the tape label - 4 records of 80 bytes */
	while ( 1 )
	{
		marks <<= 1;
		len = read_record( (unsigned char *)label, sizeof(label) );
		if ( !len )
		{
			marks |= 1;
			if ( (marks&mstop) == mstop ) /* stop after an appropriate # of tape marks */
				break;			/* and return found status */
			--stm;
			if ( stm < 0 )
				stm = 0;		/* don't let it go negative */
			continue;			/* otherwise eat tape marks */
		}
		if ( stm && len )
		{
			continue;			/* skipping to tape mark */
		}
		if ( len != LABEL_SIZE )
		{
			if ( !rptd )
			{
				printf ( "Snark: rdhead(): bad header record. Expected %d bytes, got %d.\n", LABEL_SIZE, len );
				++rptd;
			}
			continue;
		}
		if ( strncmp ( label, "VOL1", 4 ) == 0 )
		{
			memcpy( name, label+4, 14 );
			name[14] = 0;
			if ( vflag || tflag )
				printf ( "Volume: %s\n", name );
			continue;
		}
		if ( strncmp ( label, "HDR1", 4 ) == 0 )
		{
			memcpy( name, label+4, 14 );
			name[14] = 0;
			sscanf ( label + 31, "%4d", &setnr );
			++numHdrs;
			if ( (vflag&VERB_LVL) || tflag )
				printf( "HDR1: %3d: '%s'\n", numHdrs, name );
			continue;
		}
		/* get the block size */
		if ( strncmp ( label, "HDR2", 4 ) == 0 )
		{
			sscanf ( label + 5, "%5d", &blocksize );
			if ( (vflag & VERB_DEBUG_LVL) )
				printf ( "blocksize = %d\n", blocksize );
			if ( nflag )
			{
				if ( strncmp( name, selsetname, 14 ) )
				{
					if ( (vflag&VERB_LVL) || tflag )
					{
						printf( "Skipping '%s' due to -n option ('%s').\n", name, selsetname );
					}
					stm = 2;
					continue;
				}
			}
#if 0
			if ( sflag )
			{
				if ( setnr < selset )
				{
					if ( (vflag&VERB_LVL) || tflag )
					{
						printf( "SS number %d less than -s flag of %d. Skipping.\n", setnr, selset );
					}
					stm = 2;			/* skip to next volume or HDR marker */
					continue;
				}
				if ( setnr > selset )
				{
					if ( (vflag&VERB_LVL) || tflag )
					{
						printf( "SS number %d more than -s flag of %d. Done.\n", setnr, selset );
					}
					nfound = -1;
					break;
				}
			}
#endif
			if ( skipSet )
			{
				if ( numHdrs < skipSet )
				{
					if ( (vflag&VERB_LVL) || tflag )
					{
						printf( "Number of HDRs of %d is less than -S flag of %d. Skipping.\n", numHdrs, skipSet );
					}
					stm = 2;			/* skip to next volume or HDR marker */
					continue;
				}
				if ( numHdrs > skipSet )
				{
					if ( (vflag&VERB_LVL) || tflag )
					{
						printf( "Number of HDRs %d is more than -S flag of %d. Done.\n", numHdrs, skipSet );
					}
					nfound = -1;
					break;
				}
			}
			nfound = 0;
			mstop = 1;
			continue;
		}
	}
	if ( rptd > 1 )
		printf( "Snark: rdhead(): Skipped %d bad records looking for a HDR2.\n", rptd );
	if ( !tflag && (vflag & VERB_LVL) && !nfound )
		printf ( "Saveset name: %s   number: %d\n", name, setnr );
	if ( !nfound && blocksize && blocksize+16 > buffalloc )
	{
		alloc_buffers( MAX_BUFFCOUNT, blocksize );
		freeall();
	}
	return( nfound );
}

/**
 * Cleanup at end of saveset.
 * 
 * @param ssname Pointer to saveset name.
 *
 * @return nothing.
 */

static void end_of_saveset( char *ssname )
{
	if ( vflag || tflag || saveSet_errors )
	{
		char name[80];
		if ( ssname )
		{
			memcpy( name, ssname+4, 14 );
			name[14] = 0;
		}
		else
		{
			strcpy( name, "Unknown" );
		}
		if ( saveSet_errors )
			printf( "Snark: Found %d error%s in saveset \"%s\"\n",
					saveSet_errors, saveSet_errors > 1 ? "s" : "", name );
		if ( vflag || tflag )
			printf ( "End of saveset: %s\n\n\n", name );
	}
}

/**
 * Read tail end of saveset.
 * Close previously opened file if appropriate and look for and read the EOF tape records.
 *
 * @return nothing.
 */

void rdtail ( void )
{
	int len;

	close_file();
	/* read the tape label - 4 records of 80 bytes */
	while ( ( len = read_record( (unsigned char *)label, sizeof(label) ) ) != 0 )
	{
		if ( len != LABEL_SIZE )
		{
			printf ( "Snark: rdtail(): bad EOF label record. Expected %d bytes got %d.\n", LABEL_SIZE, len );
			skipping |= SKIP_TO_SAVESET;
			end_of_saveset( NULL );
			break;
		}
		if ( strncmp ( label, "EOF1", 4 ) == 0 )
		{
			end_of_saveset( label );
		}
	}
}

#define NXT_BLK_OK	(0)	/*!< block is ok to decode */
#define NXT_BLK_EOT	(1)	/*!< we're at EOT */
#define NXT_BLK_TM	(2)	/*!< we're at a TM */
#define NXT_BLK_NOLEAD	(3)	/*!< no leading block */
#define NXT_BLK_ERR	(4)	/*!< generic error */

/**
 * Read next tape block.
 *
 * @return
 *	@arg NXT_BLK_OK Buffer at top of busy list has next block to process.
 *	@arg NXT_BLK_EOT Reached end of tape. No buffer on busy queue.
 *	@arg NXT_BLK_TM Reached a tape mark. No buffer on busy queue.
 *	@arg NXT_BLK_NOLEAD No block 1 found in saveset. No buffer on busy queue.
 *	@arg NXT_BLK_ERR Generic read error. No buffer on busy queue.
 *
 * @note
 * This function will keep all buffers full with tape data at all times.
 */

static int read_next_block( )
{
	unsigned long numb0;
	struct buff_ctl *bptr;
	int ra, hittm=0;

	if ( !busybuffs )		/* if first time through, need to rdhead() then fill n buffers */
	{
		if ( rdhead (  ) )	/* read header */
			return NXT_BLK_EOT;	/* reached eot */
		bptr = getfree_buff();
		if ( !bptr )
		{
			printf( "Snark: Fatal internal error. No more free buffs.\n" );
			skipping |= SKIP_TO_SAVESET;
			return NXT_BLK_ERR;
		}
		while ( 1 )
		{
			bptr->amt = read_record( bptr->buffer, buffalloc );	/* fill first buffer */
			if ( !bptr->amt )
			{
				free_buff( bptr );				/* put this back */
				return NXT_BLK_TM;				/* found tm */
			}
			if ( bptr->amt == blocksize )           /* block is ok so far */
			{
				numb0 = get_block_number( bptr->buffer );	/* get the block number of leading block */
				if ( !numb0 )
					continue;					/* not a valid block, skip it */
				if ( numb0 != 1 )				/* it had better be a 1 */
				{
					free_buff( bptr );				/* put this back */
					return NXT_BLK_NOLEAD;			/* no leading block */
				}
				break;
			}
			printf ( "Snark: record size incorrect. read amt = %d, expected %d\n", bptr->amt, blocksize );
		}
		bptr->blknum = 1;					/* always starts with block 1 */
		add_busybuff( bptr, 0 );				/* put this on the busy queue */
	}
	bptr = buffers+busybuffs;		/* point to top item on queue */
	if ( !bptr->amt )			/* if top buffer is a TM */
	{
		free_buff( popbusy_buff() );	/* toss the top item */
		return NXT_BLK_TM;		/* return eof */
	}
	while ( bptr->next )		/* find last item on busy queue */
		bptr = buffers + bptr->next;
	if ( !bptr->amt )			/* if last item is a tm, nothing left to read */
		return NXT_BLK_OK;		/* just consume whatever is currently on the queue */
	while ( !hittm && num_busys < MAX_BUFFCOUNT )	/* keep busy queue as full as possible */
	{
		for ( ra=num_busys; !hittm && ra < MAX_BUFFCOUNT; ++ra )	/* may need to readahead n buffers */
		{
			bptr = getfree_buff();		/* get a free buffer */
			if ( !bptr )
			{
				printf( "Snark: Fatal internal error. Ran out of free buffs.\n" );
				skipping |= SKIP_TO_SAVESET;
				return NXT_BLK_ERR;
			}
			while ( !hittm )
			{
				bptr->amt = read_record( bptr->buffer, buffalloc );	/* fill it up */
				if ( !bptr->amt )		/* reached TM on readahead */
				{
					hittm = 1;			/* can't read anymore */
					break;
				}
				if ( bptr->amt == blocksize )
				{
					bptr->blknum = get_block_number( bptr->buffer );	/* get the block number */
					if ( !bptr->blknum )	/* not a valid block */
						continue;		/* get another one */
					break;			/* block is ok so far */
				}
				printf ( "Snark: record size on readahead is incorrect. read amt = %d, expected %d\n",
						 bptr->amt, blocksize );
			}
			if ( !hittm )
				add_busybuff( bptr, 0 );	/* append the buffer to busy queue */
			else
				free_buff( bptr );		/* toss this for now */
		}
		remove_dups();				/* account for missing & duplicates in busy queue */
	}
	if ( hittm )			/* if we've hit a TM */
	{
		bptr = getfree_buff();
		add_busybuff( bptr, 0 );	/* stick a TM at end of busy queue */
	}
	return NXT_BLK_OK;			/* we've got a good record */
}

typedef enum
{
	 OPT_UNDEFINED
	,OPT_DVD			/* -i */
	,OPT_EXTRACT		/* --extract */
	,OPT_FILE			/* -f */
	,OPT_HDR1_NUMBER	/* -s */
	,OPT_HELP			/* -h */
	,OPT_HIERARCHY		/* -d */
	,OPT_LIST			/* -t */
	,OPT_LOWERCASE		/* -l */
	,OPT_NO_VERSION		/* -R */
	,OPT_PROMPT			/* -w */
	,OPT_SET_NAME		/* -n */
	,OPT_SIMH			/* -I */
	,OPT_VERBOSE		/* -v */
	,OPT_VER_DELIMIT	/* -delimiter */
	,OPT_VFC
	,OPT_BINARY			/* write binary and preserve record formats */
} Options_t;

static struct option long_options[] = 
{
	 {"delimiter", optional_argument, NULL, OPT_VER_DELIMIT }
	,{"dvd",no_argument,NULL,'i'}
	,{"extract",optional_argument,NULL,OPT_EXTRACT}
	,{"file", required_argument, NULL, 'f' }
	,{"hierarchy", no_argument, NULL, 'd' }
	,{"hdr1",required_argument,NULL,'s'}
	,{"help", no_argument, NULL, 'h' }
	,{"list",no_argument,NULL,'t'}
	,{"lowercase", no_argument, NULL, 'l'}
	,{"noversions", no_argument, NULL, 'R'}
	,{"prompt",no_argument,NULL,'w'}
	,{"binary",no_argument,NULL,OPT_BINARY }
	,{"setname", required_argument, NULL, 'n'}
	,{"simh",no_argument,NULL,'I'}
	,{"verbose",required_argument,NULL,'v'}
	,{"vfc", required_argument, NULL, 'F' }
	,{NULL,0,NULL,0}
};

/**
 * Display program usage instructions.
 *
 * @param progname Pointer to null terminated program name.
 * @param full What to display.
 *	@arg 0 Brief format.
 *	@arg 1 Verbose format.
 *
 * @return nothing.
 */

void usage ( const char *progname, int full )
{
	printf ("%s version 3.12, May 2024\n", progname );
	printf ( "Usage:  %s -{tx}[cdeiIhw?][-n <name>][-s <num>][-v <num>] -f <file>\n",
			 progname );
	if ( full )
	{
		printf ( "Where {} indicates one option is required, [] indicates optional and <> indicates parameter:\n"
				 " -c               Convert VMS filename version delimiter ';' to ':'\n"
				 " --delimiter[=x]  Convert VMS filename version delimiter from ';' to whatever 'x' is (x must be printable, defaults 'x' to ':')\n"
				 " -d, --hierarchy  Maintain VMS directory structure during extraction.\n"
				 " -x               Extract files from saveset. See -e, -E or --extract below. (Same as --extract=0).\n"
				 " -e               Extract all files regardless of filetype (except .dir and .mai).\n"
				 " -E               Extract all files regardless of filetype (including .dir and .mai).\n"
				 " --extract[=n]    Extract all files according to value of n:\n"
				 "                     0  = All except .DIR,.EXE,.LIB,.MAI,.OBJ,.ODL,.OLB,.PMD,.SYS,.TLB,.TLO,.TSK,.UPD (default)\n"
				 "                     1  = All except .DIR,.MAI,.ODL,.OLB,.PMD,.SYS,.TLB,.TLO,.TSK,.UPD\n"
				 "                     2+ = All except .DIR,.MAI\n"
				 " -f name          See --file below.\n"
				 " --file=name      Name of image or device. Alternate to -f. Required parameter (no default)\n"
				 " -F n             See --vfc below.\n"
				 " --binary         Output records in binary while preserving record formats and attributes by including them in the filename.\n"
				 "                      The output files will be named x.x[;version][;format;size;att]\n"
				 "                      I.e. if the record format of file FOO.BAR;1 is FIXED with size 512 and no attributes its output will be\n"
				 "                      named FOO.BAR;1;FIX;512;NONE where the ';' is the delimiter specified with --delimiter or ';' by default.\n"
				 "                      If the format of file FOO.BAR;1 is VAR, then the size is that of the longest record.\n"
				 "                      I.e. if the longest record is 77 bytes and the attributes is CR, the name will be FOO.BAR;1;VAR;77;CR\n"
				 "                      The formats can be one of RAW, FIX, VAR, VFC, VFCn, STM, STMCR, STMLF where the 'n' in VFCn is the\n"
				 "                      number of VFC bytes in the record. The attributes can be one or more of NONE, FTN, CR, PRN and BLK\n"
				 " --vfc[=n]        (Alternate to -F) Handle VFC records according to 'n' as:\n"  
				 "                      0 - Discard the VFC bytes and output records with just a newline at the end of line.\n"
				 "                      1 - Decode the two VFC bytes into appropriate Fortran carriage control (Default).\n"
				 "                      2 - Insert the two VFC bytes at the head of each record unchanged.\n"
				  );
		printf(  " -h, --help       This message.\n"
				 " -i, --dvd        Input is of type DVD disk image of tape (aka Atari format).\n"
				 " -I, --simh       Input is of type SIMH format disk image of tape.\n"
				 " -l, --lowercase  Lowercase all directory and filenames.\n"
				 " -R, --noversions Strip off file version number and output only latest version.\n"
				 " -n name          See --setname below.\n"
				 " --setname=name   Select the name of the saveset in the tape image as found in a HDR1 record.\n"
				 " -s n             See --hdr1 below.\n"
				 " --hdr1=n         'n' is a decimal number indicating which file delimited by HDR1 records to unpack. (Starts at 1).\n"
				 "                      I.e. --hdr1number=3 means skip to the third HDR1 then unpack just that file.\n"
				 " -t, --list       List file contents to stdout.\n"
				 " -v n             See --verbose below.\n"
				 " --verbose=n      'n' is a bitmask of items to enable verbose level:\n"
				 "                      0x01 - small announcements of progress.\n"
				 "                      0x02 - announcements about file read primitives.\n"
				 "                      0x04 - announcements about file write primitives.\n"
				 "                      0x08 - used to debug buffer queues.\n"
				 "                      0x10 - lots of other debugging info.\n"
				 "                      0x20 - block reads if -i or -I mode.\n"
				 " -w, --prompt     Prompt before writing each output file.\n"
				 );
		printf( "\nNOTE: If files are found with VAR or VFC formats but no record attribute set, the filename will\n"
				"be output as x.x[;version];format;size;NONE; where ';' is the delimiter set in --delimiter (; by\n"
				"default) and ';format' will be one of ;VAR or ;VFC and ;size will be size of the longest record\n"
				"found in the file.\n"
				"\nNOTE 2: If an invalid length is discovered in a VAR or VFC record the file will be renamed\n"
				"x.x[;version];format;size;att;isCorruptAt;x where ;format will be one of ;VAR or ;VFC,\n"
				";size will be the length of the longest record, ;att will hold the attribute (CR, FTN, PRN, BLK)\n"
				"and _x is the byte offset in the file where the invalid record can be found. It is expected\n"
				"a custom program to be used to attempt to extract the records from the file as a separate step.\n"
				);
	}
}

/**
 * Program entry.
 *
 * @param argc Number of arguments passed to program.
 * @param argv Array of pointers to arguments.
 *
 * @return
 *	@arg 0 Success
 *	@arg non-zero Reason for failure.
 */

int main ( int argc, char *argv[] )
{
	const char *progname;
	int c, eoffl;
	extern int optind;
	extern char *optarg;
	char *endp;
	struct tm tadj;
	int option_index = 0;
	struct stat fileStat;
	
	memset( &tadj, 0, sizeof(tadj) );
	tadj.tm_sec = 0;
	tadj.tm_min = 0;
	tadj.tm_hour = 0;
	tadj.tm_mon = 5;
	tadj.tm_mday = 5;
	tadj.tm_year = 74;
	secs_adj = mktime( &tadj );

	progname = argv[0];
	if ( argc < 2 )
	{
		printf( "No arguments.\n" );
		usage ( progname, 0 );
		exit ( 1 );
	}
	gargv = argv;
	gargc = argc;
	dflag = eflag = tflag = vflag = wflag = xflag = iflag = Iflag = Rflag = 0;
	vfcflag = 1;	/* Default VFC mode */
	cDelim = ';';	/* Default version delimiter */
	while ( ( c = getopt_long ( argc, argv, "cdeEF:f:hiIln:Rs:tv:wx", long_options, &option_index ) ) != EOF )
	{
		switch ( c )
		{
		case OPT_VER_DELIMIT:	/* -delimiter */
			if ( !optarg )
			{
				printf("Defaulting version delimiter to ':'\n");
				cDelim = ':';
				break;
			}
			if ( !isprint(optarg[0]) )
			{
				printf("Argument to --delimiter must be printable. Is 0x%02X\n", optarg[0]);
				return 1;
			}
			cDelim = optarg[0];
			break;
		case OPT_EXTRACT:		/* --extract */
			endp = NULL;
			eflag = strtoul(optarg,&endp,0);
			if ( !endp || *endp || eflag < 0 || eflag > 2)
			{
				printf("Snark: Bad --extract parameter: '%s'. Must be a number 0 <= n <= 2\n", optarg);
				return 1;
			}
			++xflag;
			break;
		case 'c':
			cDelim = ':';
			break;
		case OPT_HIERARCHY:		/* -d */
		case 'd':
			++dflag;
			break;
		case 'e':
			eflag = 1;
			break;
		case 'E':
			eflag = 2;
			break;
		case OPT_FILE:			/* -f */
		case 'f':
			tapefile = optarg;
			break;
		case 'F':
			endp = NULL;
			vfcflag = strtoul(optarg,&endp,0);
			if ( !endp || *endp || vfcflag < 0 || vfcflag > 2)
			{
				printf("Snark: Bad -F parameter: '%s'. Must be a number 0 <= F <= 2\n", optarg);
				return 1;
			}
			break;
		case OPT_HELP:			/* -h */
		default:
			printf( "Unrecognised option: '%c'.\n", c );
		case 'h':
		case '?':
			usage ( progname, 1 );
			return 0;
		case OPT_DVD:			/* -i */
		case 'i':
			++iflag;
			break;
		case OPT_SIMH:			/* -I */
		case 'I':
			++Iflag;
			break;
		case OPT_BINARY:
			++binaryFlag;
			break;
		case OPT_LOWERCASE:		/* -l */
		case 'l':
			++lcflag;
			break;
		case OPT_SET_NAME:		/* -n */
		case 'n':
			++nflag;
			c = strlen(optarg);
			memset(selsetname,' ',sizeof(selsetname));
			memcpy(selsetname,optarg,c);
			break;
		case OPT_NO_VERSION:		/* -R */
		case 'R':
			++Rflag;
			break;
		case OPT_HDR1_NUMBER:	/* -s */
		case 's':
			endp = NULL;
			skipSet = strtol(optarg,&endp,0);
			if ( !endp || *endp || skipSet <= 0 )
			{
				printf("Snark: Bad -s parameter. '%s' Must be integer greater than 1\n", optarg);
				return 1;
			}
			break;
		case OPT_LIST:			/* -t */
		case 't':
			++tflag;
			break;
		case OPT_VERBOSE:		/* -v */
		case 'v':
			endp = NULL;
			vflag = strtoul(optarg,&endp,0);
			if ( !endp || *endp )
			{
				printf("Snark: Bad -v parameter: '%s'. Must be a number\n", optarg);
				return 1;
			}
			break;
		case OPT_PROMPT:			/* -w */
		case 'w':
			++wflag;
			break;
		case 'x':
			++xflag;
			break;
		}
	}
	if ( !tapefile )
	{
		printf("The -f (or --file) option is required.\n");
		return 1;
	}
	if ( !tflag && !xflag )
	{
		printf( "You must provide either -x or -t.\n" );
		usage ( progname, 1 );
		exit ( 1 );
	}
	if ( skipSet && nflag )
	{
		printf( "-s and -n are mutually exclusive.\n" );
		usage( progname, 1 );
		exit(1);
	}
	goptind = optind;

	/* open the tape file */
	fd = stat( tapefile, &fileStat);
	if ( fd < 0 )
	{
		perror("Failed to stat file");
		return 1;
	}
	fd = open(tapefile, OPEN_FLAGS);
	if ( fd < 0 )
	{
		perror ( tapefile );
		exit ( 1 );
	}

#if HAVE_MTIO
	if ( !S_ISREG(fileStat.st_mode) && !iflag && !Iflag )
	{
		/* rewind the tape */
		int ii;
		struct mtop op;
		op.mt_op = MTSETBLK;
		op.mt_count = 0;
		ii = ioctl( fd, MTIOCTOP, &op );
		if ( ii < 0 )
		{
			perror( "Unable to set to variable blocksize." );
			exit ( 1 );
		}
		op.mt_op = MTREW;
		op.mt_count = 1;
		ii = ioctl ( fd, MTIOCTOP, &op );
		if ( ii < 0 )
		{
			perror ( "Unable to rewind tape." );
			exit ( 1 );
		}
	}
#endif

	eoffl = 0;
	/* read the backup tape blocks until end of tape */
	while ( !eoffl )
	{
		struct buff_ctl *bptr;
		bptr = NULL;
		eoffl = read_next_block();
		switch ( eoffl )
		{
		case NXT_BLK_EOT:		/* reached EOT */
			eoffl = 1;		/* we're done */
			continue;
		case NXT_BLK_TM:		/* reached a TM */
			rdtail (  );		/* read EOF labels */
			freeall();		/* reset for next saveset */
			skipping = 0;		/* not skipping anything now */
			eoffl = 0;
			continue;		/* loop */
		default:
			printf( "Snark: Undefined return value from read_next_block(): %d\n", eoffl );
			/* FALL THROUGH TO NXT_BLK_NOLEAD */
		case NXT_BLK_ERR:		/* Generic internal error */
		case NXT_BLK_NOLEAD:	/* Failed to read leading block */
			++saveSet_errors;
			skipping |= SKIP_TO_SAVESET;
			skip_to_tm();
			freeall();		/* reset for next saveset */
			eoffl = 0;
			continue;
		case NXT_BLK_OK:
			{
				bptr = popbusy_buff();
				if ( bptr->blknum != last_block_number+1 )
				{
					printf( "Snark: block %ld out of sequence. Expected %ld\n",
							bptr->blknum, last_block_number+1 );
					++file.file_blk_error;
					++saveSet_errors;
					close_file();
					skipping |= SKIP_TO_FILE;	/* current file is probably corrupt */
				}
				eoffl = 0;
				break;
			}
		}
		if ( bptr )
		{
			process_block ( bptr->buffer );
			free_buff( bptr );
		}
	}
	close_file();

	if ( vflag || tflag )
		printf ( "End of tape\n" );

	/* close the tape */
	close ( fd );
	if ( total_errors )
		printf( "Snark: A total of %d error%s detected.\n",
				total_errors, total_errors > 1 ? "s" : "" );

	/* exit cleanly */
	return 0;
}
