/**
 * @file vmsbackup.c
 */

/**
 *
 * @mainpage
 * @version Nov_2023
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
 *  Net-addess (as of 1986 or so; it is highly unlikely these still work):
 *	john%monu1.oz@seismo.ARPA
 *	luthcad!sow@enea.UUCP
 *	dshepperd@mindspring.com
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
#include	<stdlib.h>
#include	<unistd.h>
#include	<getopt.h>
#include	<time.h>
#include	<utime.h>

#include	<sys/ioctl.h>
#include	<sys/types.h>
#ifdef REMOTE
	#include	<local/rmt.h>
#endif
#include	<sys/stat.h>
#include	<sys/mtio.h>
#include	<sys/file.h>

extern int match ( const char *string, const char *pattern );
int typecmp ( const char *str );

/* Byte-swapping routines.  Note that these do not depend on the size
   of datatypes such as short, long, etc., nor do they require us to
   detect the endianness of the machine we are running on.  It is
   possible they should be macros for speed, but I'm not sure it is
   worth bothering.  We don't have signed versions although we could
   add them if needed.  They are, of course little-endian as that is
   the byteorder used by all integers in a BACKUP saveset.  */

static inline unsigned long getu32 ( unsigned char *addr )
{
	unsigned long ans;
	ans = addr[3];
	ans = (ans<<8) | addr[2];
	ans = (ans<<8) | addr[1];
	ans = (ans<<8) | addr[0];
	return ans;
}

static inline unsigned int getu16 ( unsigned char *addr )
{
	return(addr[1] << 8) | addr[0];
}

#define GETU16(x) getu16( (unsigned char *)&(x) )
#define GETU32(x) getu32( (unsigned char *)&(x) )

#define MAX_FILENAME_LEN (128)

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

struct file_details
{
	time_t ctime;
	time_t mtime;
	time_t atime;
	time_t btime;
	FILE *extf;
	int directory;
	int size;
	int nblk;
	int lnch;
	int allocation;
	int usr;
	int grp;
	int recfmt;
	int recatt;
	int count;
	int rec_count;
	int rec_padding;
	int vfcsize;
	char name[MAX_FILENAME_LEN+4];
	char ufname[MAX_FILENAME_LEN+4];
	unsigned short reclen;
	short fix;
	unsigned short recsize;
} file;

#ifndef DEF_TAPEFILE
	#define DEF_TAPEFILE "/dev/tape"
#endif
char *tapefile = DEF_TAPEFILE;

time_t secs_adj;
char last_char_written;

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
int cflag, dflag, eflag, iflag, Iflag, nflag, sflag, skipFlag, tflag, vflag, wflag, xflag, Rflag;
int setnr, selset, skipSet, numHdrs, ss_errors, file_errors, total_errors;
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

char **gargv;
int goptind, gargc;

#define	LABEL_SIZE	80
char label[32768 + LABEL_SIZE];

static int blocksize;
struct mtop op;

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

static char lastFileName[16];
static int lastVersionNumber;

static FILE *openfile ( char *ufn, char *fn, int dirfile )
{
	char ans[80];
	char *p, *q, s, *ext = NULL, *justFileName;
	int procf;

	procf = 1;
	/* copy whole fn to ufn and convert to lower case */
	p = fn;
	q = ufn;
	if ( *p == '[' )	/* strip off leading '[' */
		++p;
	while ( *p )
	{
		if ( isupper ( *p ) )
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
				mkdir ( p, 0777 );
			*q = '/';
			if ( s == ']' )
				break;
		}
		++q;
	}
	++q;	/* ufn points to path and q points to start of filename. */
	justFileName = q;
	if ( !dflag )
	{
		strcpy( ufn, q );	/* not keeping the directory structure */
		p = ufn;
	}
	/* strip off the version number and possibly fix the filename's case */
	while ( *q && *q != ';' )
	{
		if ( *q == '.' )
			ext = q;
		if ( Rflag && islower(*q) )
			*q = toupper(*q);	/* re-upcase the filename and filetype */
		q++;
	}
	if ( Rflag && *q == ';' )
	{
		char *endp = NULL;
		int curVersion;
		curVersion = strtol(q+1,&endp,10);
		if ( !endp || *endp )
			curVersion = 0;
		*q = 0;
		if ( curVersion && !strcmp(lastFileName,justFileName) )
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
			strncpy(lastFileName,justFileName,sizeof(lastFileName));
			lastVersionNumber = curVersion;
		}
		*q = ';';
	}
	if ( cflag && !Rflag )
	{
		*q = ':';
	}
	else
	{
		*q = '\0';
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
			if ( ext && !eflag && procf )
				procf = typecmp ( ++ext );
		}
	}
	if ( procf && wflag )
	{
		printf ( "extract %s [ny]", p );
		fflush ( stdout );
		fgets ( ans, sizeof ( ans ), stdin );
		if ( *ans != 'y' )
			procf = 0;
	}
	if ( procf )
		/* open the file for writing */
		return( fopen ( p, "w" ) );
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

int typecmp ( const char *str )
{
	static const char * const type[] = {
		"exe",			/* vms executable image */
		"lib",			/* vms object library */
		"obj",			/* rsx object file */
		"odl",			/* rsx overlay description file */
		"olb",			/* rsx object library */
		"pmd",			/* rsx post mortem dump */
		"stb",			/* rsx symbol table */
		"sys",			/* rsx bootable system image */
		"tsk",			/* rsx executable image */
		"dir",
		"upd",
		"tlo",
		"tlb",
		NULL			/* null string terminates list */
	};
	int ii;
	
	for (ii=0; type[ii]; ++ii )
		if ( strncasecmp ( str, type[ii], 3 ) == 0 )
			return( 0 );   /* found a match, file to be ignored */
	return( 1 );	   /* no match found */
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
	if ( !file.directory && !(file.recfmt&FAB_dol_M_MAIL) )
	{
		if ( (xflag || file.count) && file.count != file.size )
		{
			printf( "Snark: '%s' file size is not correct. Is %d, should be %d. May be corrupt.\n",
					file.name, file.count, file.size );
			++file_errors;
		}
		if ( (vflag & VERB_FILE_RDLVL) )
		{
			printf( "File size: %d, count: %d, padding: %d, rec_count: %d\n",
					file.size, file.count, file.rec_padding, file.rec_count );
		}
	}
	if ( file.extf != NULL )	/* if file previously opened */
	{
		struct utimbuf ut;

		fclose ( file.extf );	/* close it */
		ut.actime = file.atime;
		ut.modtime = file.mtime;
		utime( file.ufname, &ut );
		if ( file_errors )
		{
			char refilename[MAX_FILENAME_LEN+32];

			printf( "Snark: close_file(): %d error%s during extract of \"%s\"\n",
					file_errors, file_errors > 1 ? "s" : "",
					file.ufname );
			strcpy( refilename, file.ufname );
			strcat( refilename, ".may_be_corrupt" );
			rename( file.ufname, refilename );
		}
	}
	memset( &file, 0, sizeof( file ) );
	file_errors = 0;
	last_char_written = 0;
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

static char *vms2unixtime( time_t vtime )
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
		++ss_errors;
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
			++ss_errors;
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
			if ( clen > sizeof(file.name)-1 )
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
			++ss_errors;
			break;
		}
		++subf;
		cc += dsize + 4;
	}

	/* open the file */
	if ( strstr(file.name,".MAI") )
		file.recfmt |= FAB_dol_M_MAIL;
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
			printf ( " %-35s %8d%s\n", file.name, file.size, file.size < 0 ? " (IGNORED!!!)" : "" );
		if ( file.size < 0 )
		{
			if ( !tflag && xflag )
				printf ( "Snark: process_file(): %-35s not extracted due to filesize of %8d\n",
						 file.name, file.size );
			++file_errors;
			++ss_errors;
			skipping |= SKIP_TO_FILE;	/* this is bad */
			return;
		}
		if ( file.directory
			 || (file.recfmt&FAB_dol_M_MAIL)
			 || (   !file.recsize 
					&& (   file.recfmt == FAB_dol_C_VAR
						   || file.recfmt == FAB_dol_C_VFC
					   )
				)
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
			file.extf = openfile ( file.ufname, file.name, file.directory );
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
		++ss_errors;
		skipping |= SKIP_TO_BLOCK;	/* Skip to next block */
		return;
	}

	if ( tflag || (vflag & VERB_LVL) )
	{
		int clen, cc, subf=0;

		printf( "\nHeader processing. rsize=%d\n", rsize );
		cc = 2;
		buff_cnt = 0;
		while ( cc <= rsize-4 )
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
				if ( clen > sizeof(tbuff)-1 )
					clen = sizeof(tbuff)-1;
				memcpy( tbuff, text, clen );
				tbuff[clen] = 0;
				printf( "%02d: Saveset Name: \"%s\"\n", subf, tbuff );
				continue;
			case SUMM_CMDLINE:
				clen = dsize;
				if ( clen > sizeof(tbuff)-1 )
					clen = sizeof(tbuff)-1;
				memcpy( tbuff, text, clen );
				tbuff[clen] = 0;
				printf( "%02d: Command:      \"%s\"\n", subf, tbuff );
				continue;
			case SUMM_COMMENT:
				clen = dsize;
				if ( clen > sizeof(tbuff)-1 )
					clen = sizeof(tbuff)-1;
				memcpy( tbuff, text, clen );
				tbuff[clen] = 0;
				printf( "%02d: Comment:      \"%s\"\n", subf, tbuff );
				continue;
			case SUMM_USER:
				clen = dsize;
				if ( clen > sizeof(tbuff)-1 )
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
				if ( clen > sizeof(tbuff)-1 )
					clen = sizeof(tbuff)-1;
				memcpy( tbuff, text, clen );
				tbuff[clen] = 0;
				printf( "%02d: OS Version:   \"%s\"\n", subf, tbuff );
				continue;
			case SUMM_NODENAME:
				clen = dsize;
				if ( clen > sizeof(tbuff)-1 )
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
				if ( clen > sizeof(tbuff)-1 )
					clen = sizeof(tbuff)-1;
				memcpy( tbuff, text, clen );
				tbuff[clen] = 0;
				printf( "%02d: Device:       \"%s\"\n", subf, tbuff );
				continue;
			case SUMM_BCKVERSION:
				clen = dsize;
				if ( clen > sizeof(tbuff)-1 )
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
	int cc, ii, tlen;

	ii = 0;
	if ( (vflag & VERB_LVL) )
	{
		printf("process_vbn(): Entry rsize=%d. recFmt=%d, recCnt=%d, count=%d. buff: %02X %02X %02X %02X ...\n",
			   rsize, file.recfmt, file.rec_count, file.count,
			   buffer[0], buffer[1], buffer[2], buffer[3] );
	}
	if ( file.count >= file.size )
	{
		if ( !strlen( file.name ) )
		{
			skipping |= SKIP_TO_FILE;		/* What the hell are we doing here? */
			return;
		}
		else
			printf( "Snark: process_vbn(): Filesize of %s is too big. Is %d, expected %d\n",
					file.name, file.count, file.size );
	}
	while ( file.count + ii < file.size && ii < rsize )
	{
		switch ( (file.recfmt&0x1F) )
		{
		case FAB_dol_C_STM:
		case FAB_dol_C_STMLF:
		case FAB_dol_C_FIX:
		case FAB_dol_C_FIX11:
		case FAB_dol_C_RAW:
			file.reclen = rsize;		/* assume max */
			if ( file.count + file.reclen > file.size )
				file.reclen = file.size - file.count;
			if ( file.extf )
			{
				if ( (vflag & VERB_FILE_WRLVL) )
					printf( "Writing %4d bytes. recfmt=%d\n", file.reclen, file.recfmt );
				fwrite( buffer+ii, 1, file.reclen, file.extf );
			}
			ii += file.reclen;
			file.reclen = 0;
			break;

		case FAB_dol_C_VAR:
		case FAB_dol_C_VFC:
			if ( file.reclen == 0 )
			{
				file.reclen = getu16( (unsigned char *)buffer + ii );
				ii += 2;
				++file.rec_count;
				if ( (vflag & VERB_FILE_RDLVL) )
					printf ( "New record mark: reclen = %5d, ii = %5d, rsize = %5d\n",
							 file.reclen, ii-2, rsize );
				file.fix = file.reclen;
				if ( !file.reclen )		/* if blank line */
				{
					if ( file.extf )
					{
						if ( (vflag & VERB_FILE_WRLVL) )
							printf( "Writing blank line. recfmt=%d\n", file.recfmt );
						fputc( '\n', file.extf );
					}
					last_char_written = '\n';
				}
				if ( file.reclen == 0xFFFF )
				{
					if ( (vflag & VERB_FILE_RDLVL) )
						printf( "Reached EOF. recfmt=%d, reclen=%d. ii=%d, file.count=%d, count+ii=%d, file.padding=%d, file.size=%d. Skipping to next file.\n",
								file.recfmt, file.reclen, ii, file.count, file.count+ii, file.rec_padding, file.size );
					file.count += ii;
					if ( file.count > file.size )
						printf("Snark: '%s' file count, %d, exceeded file size, %d by %d bytes on '%s'\n", file.name, file.count, file.size, file.count-file.size, file.name );
					file.count = file.size;
					skipping |= SKIP_TO_FILE;
					return;
				}
				if ( file.reclen > file.recsize+file.vfcsize )
				{
					int curLen,lftOvr;
					
					printf( "Snark: '%s' (ii=%d) record length of %d (0x%04X;'%c','%c') is invalid. Must be %d >= x >= 0. Converting file type from %d to %d(RAW) to finish write.\n",
							file.name,
							ii,
							file.reclen, file.reclen, isprint(file.reclen&0xFF) ? (file.reclen&0xFF) : '.', isprint((file.reclen>>8)&0xFF) ? (file.reclen>>8)&0xFF : '.',
							file.recsize, file.recfmt, FAB_dol_C_RAW );
#if 1
					file.recfmt = FAB_dol_C_RAW;
					ii -= 2;					/* backup over the record length */
					curLen = ii;				/* how many bytes already written */
					lftOvr = rsize-curLen;		/* how many bytes left in buffer */
					if ( lftOvr > file.size-(file.count+curLen) )
						lftOvr = file.size-file.count-curLen; /* how many bytes left to write */
					file.reclen = lftOvr;
					if ( file.extf )
					{
						printf( "Snark: '%s' Writing %4d (remaining?) bytes in an effort to save as much data as possible. ii=%d, rsize=%d, file.count=%d, file.size=%d, curLen=%d, lftOvr=%d\n",
								file.name, file.reclen, ii, rsize, file.count, file.size, curLen, lftOvr );
						fwrite( buffer+ii, 1, file.reclen, file.extf );
					}
					else if ( (vflag&VERB_FILE_WRLVL) )
						printf( "Snark: '%s' Would have written %4d (remaining?) bytes in an effort to save as much data as possible. ii=%d, rsize=%d, file.count=%d, file.size=%d, curLen=%d, lftOvr=%d\n",
								file.name, file.reclen, ii, rsize, file.count, file.size, curLen, lftOvr );
					ii += file.reclen;
					file.reclen = 0;
					break;
#else
						if ( !file.count )	/* if this is the first record */
						{
							int jj;
							jj = ii;			/* save our starting position */
							ii -= 2;			/* backup over what should have been the record len */
							while ( jj < rsize && jj-ii < file.recsize+file.vfcsize )
							{
								file.reclen = getu16( (unsigned char *)buffer + jj );
								if ( file.reclen <= file.recsize+file.vfcsize )
									break;
								jj += 2;
							}                       
							if ( file.reclen <= file.recsize+file.vfcsize || jj >= rsize )
							{
								if ( file.extf )
								{
									if ( (vflag & VERB_FILE_WRLVL) )
										printf( "Snark: Writing %4d bytes (recover from backup error). recfmt=%d\n", jj-ii, file.recfmt );
									fwrite( buffer+ii, 1, jj-ii, file.extf ); /* write the first part of the record */
								}
								else if ( (vflag & VERB_FILE_WRLVL) )
									printf( "Snark: Would have written %4d bytes (recover from backup error). recfmt=%d\n", jj-ii, file.recfmt );
								ii = jj;			/* set next record starting position */
								file.reclen = 0;		/* force a start of next record */
								continue;			/* and keep looking */
							}
						}
						skipping |= SKIP_TO_FILE;
						++file_errors;
						++ss_errors;
						return;
#endif
				}
			}
			tlen = file.reclen;		/* assume we can write all of it */
			if ( tlen+ii > rsize )	/* if record overflows buffer */
				tlen = rsize-ii;	/* trim to remaining buffer size */
			if ( file.count + tlen > file.size )
			{
				printf( "Snark: '%s' process_vbn(): May be a problem with file.\n", file.name );
				printf( "Snark: '%s' process_vbn(): Writing %d bytes more than filesize says to.\n",
						file.name, file.count+tlen - file.size );
			}
/*
 * TODO:
 * Figure out the fortran carriage control options and
 * fixup the output appropriately. In the meantime, just
 * include the vfc data in the output record without
 * modification.
 */
			if ( file.extf )
			{
				if ( (vflag & VERB_FILE_WRLVL) )
					printf( "Writing %4d bytes. recfmt=%d, reclen=%d\n", tlen, file.recfmt, file.reclen );
				fwrite( buffer+ii, 1, tlen, file.extf ); /* write as much as we can at once */
			}
			file.reclen -= tlen;	/* take from total */
			ii += tlen;			/* advance index */
			if ( !file.reclen )
				last_char_written = buffer[ii-1];
			if ( file.extf && !file.reclen && last_char_written != '\f' )
			{
				fputc( '\n', file.extf );	/* follow with newline if appropriate */
				last_char_written = '\n';
			}
			if ( !file.reclen && (ii&1) )	/* if we've used all the data and index is odd */
			{
				++ii;				/* round it up (all records are padded to even length) */
				++file.rec_padding;		/* this doesn't get charged against file size */
			}
			break;

		case FAB_dol_C_STMCR:
			{
				unsigned char *src = buffer + ii;	/* point to record */
				tlen = rsize;		/* assume max size */
				while ( tlen )		/* substitute all \r with \n */
				{
					cc = *src;
					if ( cc == '\r' )
						*src = '\n';
					++src;
					--tlen;
				}
				tlen = rsize;
				if ( file.count + tlen > file.size )
					tlen = file.size - file.count;
				if ( file.extf )
				{
					if ( (vflag & VERB_FILE_WRLVL) )
						printf( "Writing %4d bytes. recfmt=%d\n", tlen, file.recfmt );
					fwrite( buffer + ii, 1, tlen, file.extf ); /* write the whole thing at once */
				}
				ii += tlen;
				file.reclen = 0;
				break;
			}

		default:
			++ss_errors;
			++file_errors;
			skipping |= SKIP_TO_FILE;
			printf ( "Snark: '%s' process_vbn(): Invalid record format = %d, file.count=%d, ii=%d, file.size=%d\n",
					 file.name, file.recfmt, file.count, ii, file.size );
			return;
		}
	}
	if ( file.count+ii > file.size )
	{
		printf("Snark: '%s' process_vbn(): Hey, we've got a problem: record format=%d, ii=%d, file.count=%d, file.size=%d\n",
			   file.name, file.recfmt, ii, file.count, file.size );
	}
	file.count += ii;
	if ( (vflag & VERB_FILE_RDLVL) )
	{
		if ( file.reclen )
			printf( "process_vbn(): '%s' Record straddled block. reclen=%d, recsize=%d\n",
					file.name, file.reclen, file.recsize );
		printf( "process_vbn(): '%s' file_count now %d, filesize: %d, padding: %d\n",
				file.name, file.count, file.size, file.rec_padding );
	}
	if ( file.count >= file.size )
	{
		if ( (vflag & VERB_FILE_RDLVL) )
			printf( "process_vbn(): '%s' Reached end of file. Skipping to next file.\n", file.name );
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
	if ( bsize != 0 && bsize != blocksize )
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
		++ss_errors;
		++file_errors;
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
		if ( rsize+ii > bsize )	// This is an invalid record
		{
			printf( "Snark: rsize of %d is wrong. Cannot be more than %ld\n",
					rsize, bsize-ii );
			skipping |= SKIP_TO_BLOCK;
			++ss_errors;
			++file_errors;
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
			++ss_errors;
			if ( file.extf )
			{
				printf( "Snark: Skipping rest of %s\n", file.name );
				++file_errors;
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
	int sts, reclen, Ireclen;
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
		printf( "Snark: WARNING: Record of %d bytes too long for user %d user buffer.\n", reclen, len );
		sts = read( fd, buff, len );		/* give 'em what he wants */
		lseek( fd, reclen-len, SEEK_CUR );	/* Skip remainder of record */
		if ( (vflag & VERB_DEBUG_LVL) )
			printf( "read_record: returns %d.\n", sts );
		return sts;
	}
	if ( reclen < len )
		len = reclen;				/* trim byte count to actual record length */
	sts = read( fd, buff, len );
	if ( (vflag & VERB_DEBUG_LVL) )
		printf( "read_record: returns %d.\n", sts );
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
		sts = reclen;
	}
	if ( (vflag&VERB_BLOCK_LVL ) && !(vflag&VERB_DEBUG_LVL) )
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
	total_errors += ss_errors;
	ss_errors = 0;
	file_errors = 0;
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
			if ( skipFlag )
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
	if ( vflag || tflag || ss_errors )
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
		if ( ss_errors )
			printf( "Snark: Found %d error%s in saveset \"%s\"\n",
					ss_errors, ss_errors > 1 ? "s" : "", name );
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
			if ( bptr->amt == blocksize )			/* block is ok so far */
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
	printf ("%s version 3.4, January 2023\n", progname );
	printf ( "Usage:  %s -{tx}[cdeiIhw?][-n <name>][-s <num>][-S <num>][-f <file>][-v <num>]\n",
			 progname );
	if ( full )
	{
		printf ( "Where {} indicates one option is required, [] indicates optional and <> indicates parameter:\n"
				 "    -c  Convert VMS filename version delimiter ';' to ':'\n"
				 "    -d  Maintain VMS directory structure during extraction.\n"
				 "    -e  Extract all files regardless of filetype.\n"
				 "    -f tapefile 'tapefile' is name of input. (default: " DEF_TAPEFILE ")\n" );
		printf(   "    -h or -? This message.\n"
				  "    -i  Input is of type DVD disk image of tape.\n"
				  "    -I  Input is of type SIMH format disk image of tape.\n"
				  "    -n setname 'setname' is the saveset name as provided in the HDR1 record.\n"
				  "    -R don't lowercase the filenames, strip off file version number and output only latest version.\n"
				  "    -s setnumber 'setnumber' is decimal saveset number as provided in a HDR1 record.\n"
				  "    -S setnumber 'setnumber' is decimal as the count of HDR1 blocks starting at 1.\n"
				  "    -t  List file contents to stdout.\n"
				  "    -v  bitmask of items to enable verbose level:\n"
				  "        0x01 - small announcements of progress.\n"
				  "        0x02 - announcements about file read primitives.\n"
				  "        0x04 - announcements about file write primitives.\n"
				  "        0x08 - used to debug buffer queues.\n"
				  "        0x10 - lots of other debugging info.\n"
				  "        0x20 - block reads if -i or -I mode.\n"
				  "    -w  Prompt before writing each output file.\n"
				  "    -x  Extract files from saveset.\n" );
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
	int c, i, eoffl;
	extern int optind;
	extern char *optarg;
	char *endp;
	struct tm tadj;

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
	cflag = dflag = eflag = sflag = tflag = vflag = wflag = xflag = iflag = Iflag = Rflag = 0;
	while ( ( c = getopt ( argc, argv, "cdef:hiIn:Rs:S:tv:wx" ) ) != EOF )
	{
		switch ( c )
		{
		case 'c':
			++cflag;
			break;
		case 'd':
			++dflag;
			break;
		case 'e':
			++eflag;
			break;
		case 'f':
			tapefile = optarg;
			break;
		default:
			printf( "Unrecognised option: '%c'.\n", c );
		case 'h':
		case '?':
			usage ( progname, 1 );
			return 0;
		case 'i':
			++iflag;
			break;
		case 'I':
			++Iflag;
			break;
		case 'n':
			++nflag;
			c = strlen(optarg);
			memset(selsetname,' ',sizeof(selsetname));
			memcpy(selsetname,optarg,c);
			break;
		case 'R':
			++Rflag;
			break;
		case 's':
			++sflag;
			sscanf ( optarg, "%d", &selset );
			break;
		case 'S':
			++skipFlag;
			sscanf ( optarg, "%d", &skipSet );
			break;
		case 't':
			++tflag;
			break;
		case 'v':
			endp = NULL;
			vflag = strtoul(optarg,&endp,0);
			if ( !endp || *endp )
			{
				printf("Snark: Bad -v parameter: '%s'. Must be a number\n", optarg);
				return 1;
			}
			break;
		case 'w':
			++wflag;
			break;
		case 'x':
			++xflag;
			break;
		}
	}
	if ( !tflag && !xflag )
	{
		printf( "You must provide either -x or -t.\n" );
		usage ( progname, 1 );
		exit ( 1 );
	}
	if ( sflag && nflag )
	{
		printf( "-s and -n are mutually exclusive.\n" );
		usage( progname, 1 );
		exit(1);
	}
	goptind = optind;

	/* open the tape file */
	fd = open ( tapefile, O_RDONLY );
	if ( fd < 0 )
	{
		perror ( tapefile );
		exit ( 1 );
	}

	if ( !iflag && !Iflag )
	{
		/* rewind the tape */
		op.mt_op = MTSETBLK;
		op.mt_count = 0;
		i = ioctl( fd, MTIOCTOP, &op );
		if ( i < 0 )
		{
			perror( "Unable to set to variable blocksize." );
			exit ( 1 );
		}
		op.mt_op = MTREW;
		op.mt_count = 1;
		i = ioctl ( fd, MTIOCTOP, &op );
		if ( i < 0 )
		{
			perror ( "Unable to rewind tape." );
			exit ( 1 );
		}
	}

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
			++ss_errors;
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
					++file_errors;
					++ss_errors;
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
