# vmsbackup
Tools to decode VAX/VMS backup savesets and others to handle tape image files.

## This is from the original README:

This progam reads a VMS backuptape.

The tape program is orginally written by John Douglas Carey and
the pattern matching routine by some unknown on the net. 

The remote tape option use the rmtlib from mod.sources.

A good way to archive remotetape access for users with only
a local account is to create a "netwide" user tar and let
the remote tape programs do suid to user tar.

The program is tested on vax and sun.

```
Sven-Ove Westberg
Lulea University of Technology
S-951 87  Lulea,  Sweden
UUCP:  sow@luthcad.UUCP
UUCP:  {decvax,philabs,seismo}!mcvax!enea!luthcad!sow
```

## This is clipped from the comments from vmsbackup.c and slightly edited to .md format
```
@author John Douglas CAREY.
@author Sven-Ove Westberg    (version 3.0 and up)
@author Dave Shepperd, (DMS) vmsbackup@dshepperd.com
 		(version Mar_2003 with snipits from Sven-Ove's
 		version 4-1-1)

[VMSBACKUP on github](https://github.com/DaveShepperd/vmsbackup)

Net-addess (as of 1986 or so; it is highly unlikely these still work):
john%monu1.oz@seismo.ARPA
luthcad!sow@enea.UUCP
vmsbackup@dshepperd.com
```
History:

**Version 1.0 - September 1984**
  * Can only read variable length records

**Version 1.1**
  * Cleaned up the program from the original hack
  *	Can now read stream files

**Version 1.2**
  * Now convert filename from VMS to UNIX and creates sub-directories.

**Version 1.3**
  * Works on the Pyramid if SWAP is defined

**Version 1.4**
  * Reads files spanning multiple tape blocks

**Version 1.5**
  * Always reset reclen = 0 on file open
  * Now output fixed length records

**Version 2.0 - July 1985**
  * VMS Version 4.0 causes a rethink !!
  * Now use mtio operations instead of opening and closing file
  * Blocksize now grabed from the label

**Version 2.1 - September 1985**
  * Handle variable length records of zero length.

**Version 2.2 - July 1986**
  * Handle FORTRAN records of zero length.
  * Inserted exit(0) at end of program.
  * Distributed program in aus.sources

**Version 2.3 - August 1986**
  * Handle FORTRAN records with record length fields at the end of a block
  * Put debug output to a file.
  * Distributed program in net.sources

**Version 3.0 - December 1986**
  * Handle multiple saveset 
  * Remote tape
  * Interactive mode
  * File name selection with meta-characters
  * Convert ; to : in VMS filenames
  * Flag for usage of VMS directory structure
  * Flag for "useless" files  eg. *.exe
  * Flag for use VMS version in file names
  * Flag for verbose mode
  * Flag to list the contents of the tape
  * Distributed to mod.sources

**Version 3.1 - March 2003 (DMS)**
  * ANSI'fy the source
  * Added option, -i, to read "tape file" from disk
  * Added option, -n, to read specific saveset from disk/tape
  * changed option, -v, to accept a bit mask to enable various squawks
  * Optimise the writing of data (avoid doing single char output where possible)
  * Added handling of redundancy groups and duplicate and missing blocks.
  * Fixed numerous crashes caused by various mis-decodes
  * Record and decode errors are no longer fatal.
  * Cut and pasted select features from the 4-1-1 release
  * Added vms2unix date function and utime()'d extracted files.
    * NOTE: There may be a problem with this since I found several savesets made by VMS versions 3.x and 4.x where the date seemed to be ~4 years too young. Newer savesets (> VMS 5.0?) seem to have dates set correctly; Was there a change in the VMS epoch at some point?.
  * Always walk the file data chain even when only doing a directory listing in order to identify those files which may present decode or other errors.
  * Due to bizzare data in the VAR records found in .mai and .dir files, force it never to extract them or even walk the chain.
  * Recover from BACKUP's occasional mistake of leaving off the record length bytes from record 0 of vfc files.
  * Added some 'doxygen'ated (but untested) comments.

**Version 3.2 - April 2020 (DMS)**
  * Added -I, to read SIMH format 'tape file' from disk

**Verson 3.3 - Janurary 2022 (DMS)**
  * Fixed comments. Added a -S to skip to HDR1 record.

**Version 3.4 - November 2023 (DMS)**
  * Added -R to make it not lowercase filenames and to strip off the file version number completely and only output the latest version of any file.

**Version 3.5 - January 2024 (DMS)**
  * Added -L to provide lowercase directory and filenames.

**Version 3.6 - January 2024 (DMS)**
  * Added -E, reworked the -e option a bit.
  * Fixed some comments.

**Version 3.7 - Feburary 2024 (DMS)**
  * Fixed problems with and added support for optional VFC carriage control characters in output.
  * Changed the way it handles record delimiters.

**Version 3.8 - March 2024 (DMS)**
  * Added support for fixed length input records.
  * Added support for getopt_long().
  * Added long options:
    * --delimiter
	 * --dvd (-i)
	 * --extract
	 * --file (-f)
	 * --hierarchy (-d)
	 * --hdr1 (s)
	 * --help (-h)
	 * --list (-t)
	 * --lowercase (-l)
	 * --noversions (-R)
	 * --prompt (-w)
	 * --binary 
	 * --record
	 * --setname (-n)
	 * --simh (-I)
	 * --verbose (-v)
	 * --vfc (-F)
  * Added rfm/size/att to directory listing.
  * Write binary output and filename if record errors detected.
  * Updated help message.

**Version 3.9 - April 2024 (DMS)**
  * Added support for building under MSYS2 on Windows

**Version 3.10 - April 2024 (DMS)**
  * Fixed MSYS2 builds and added ones for Linux, MinGW32 and PiOS (32 bit only).
  * Added a vmsbackup.html document. Mainly just rehash of this README.md.

**Versin 3.11 - April 2024 (DMS)**
  * Fixed MSYS2 and MinGW builds. Needed the O_BINARY flag.

**Some original author details**
```
Computer Centre
Monash University
Wellington Road
Clayton
Victoria	3168
AUSTRALIA
```

**Help message**

```
Usage:  vmsbackup -{tx}[cdeiIhw?][-n <name>][-s <num>][-v <num>] -f <file>
Where {} indicates one option is required, [] indicates optional and <> indicates parameter:
 -c               Convert VMS filename version delimiter ';' to ':'
 --delimiter[=x]  Convert VMS filename version delimiter from ';' to whatever 'x' is (x must be printable, defaults 'x' to ':')
 -d, --hierarchy  Maintain VMS directory structure during extraction.
 -x               Extract files from saveset. See -e, -E or --extract below. (Same as --extract=0).
 -e               Extract all files regardless of filetype (except .dir and .mai).
 -E               Extract all files regardless of filetype (including .dir and .mai).
 --extract[=n]    Extract all files according to value of n:
                     0  = All except .DIR,.EXE,.LIB,.MAI,.OBJ,.ODL,.OLB,.PMD,.SYS,.TLB,.TLO,.TSK,.UPD (default)
                     1  = All except .DIR,.MAI,.ODL,.OLB,.PMD,.SYS,.TLB,.TLO,.TSK,.UPD
                     2+ = All except .DIR,.MAI
 -f name          See --file below.
 --file=name      Name of image or device. Alternate to -f. Required parameter (no default)
 -F n             See --vfc below.
 --binary         Output records in binary while preserving record formats and attributes by including them in the filename.
                      The output files will be named x.x[;version][;format;size;att]
                      I.e. if the record format of file FOO.BAR;1 is FIXED with size 512 and no attributes its output will be
                      named FOO.BAR;1;FIX;512;NONE where the ';' is the delimiter specified with --delimiter or ';' by default.
                      If the format of file FOO.BAR;1 is VAR, then the size is that of the longest record.
                      I.e. if the longest record is 77 bytes and the attributes is CR, the name will be FOO.BAR;1;VAR;77;CR
                      The formats can be one of RAW, FIX, VAR, VFC, VFCn, STM, STMCR, STMLF where the 'n' in VFCn is the
                      number of VFC bytes in the record. The attributes can be one or more of NONE, FTN, CR, PRN and BLK
 --vfc[=n]        (Alternate to -F) Handle VFC records according to 'n' as:
                      0 - Discard the VFC bytes and output records with just a newline at the end of line.
                      1 - Decode the two VFC bytes into appropriate Fortran carriage control (Default).
                      2 - Insert the two VFC bytes at the head of each record unchanged.
 -h, --help       This message.
 -i, --dvd        Input is of type DVD disk image of tape (aka Atari format).
 -I, --simh       Input is of type SIMH format disk image of tape.
 -l, --lowercase  Lowercase all directory and filenames.
 -R, --noversions Strip off file version number and output only latest version.
 -n name          See --setname below.
 --setname=name   Select the name of the saveset in the tape image as found in a HDR1 record.
 -s n             See --hdr1 below.
 --hdr1=n         'n' is a decimal number indicating which file delimited by HDR1 records to unpack. (Starts at 1).
                      I.e. --hdr1number=3 means skip to the third HDR1 then unpack just that file.
 -t, --list       List file contents to stdout.
 -v n             See --verbose below.
 --verbose=n      'n' is a bitmask of items to enable verbose level:
                      0x01 - small announcements of progress.
                      0x02 - announcements about file read primitives.
                      0x04 - announcements about file write primitives.
                      0x08 - used to debug buffer queues.
                      0x10 - lots of other debugging info.
                      0x20 - block reads if -i or -I mode.
 -w, --prompt     Prompt before writing each output file.
```

**NOTE:**
If files are found with VAR or VFC formats but no record attribute set, the filename will
be output as x.x[;version];format;size;NONE; where ';' is the delimiter set in --delimiter (; by
default) and ';format' will be one of ;VAR or ;VFC and ;size will be size of the longest record
found in the file.

**NOTE 2:**
If an invalid length is discovered in a VAR or VFC record the file will be renamed
x.x[;version];format;size;att;may_be_corrupt_at_x where ;format will be one of ;VAR or ;VFC,
;size will be the length of the longest record, ;att will hold the attribute (CR, FTN, PRN, BLK)
and \_x is the byte offset in the file where the invalid record can be found. It is expected
a custom program to be used to attempt to extract as much of the files's content as possible using a custom
built application (not provided).

**NOTE 3:**
In case one does not know already, the binary format of VAR format records is:
```
Byte Offset Description
----------- -----------
     0      Least significant bits of 16 bit record count (n&0xFF)
     1      Most significatn bits of 16 bit record count (n>>8)
    ...     n bytes of record content.
    n+2     Optional byte. Only present if n is odd.

If n is zero, the record will consist of just the two byte count.
A record count greater than 0x7FFF (32767) is always illegal. Technically, in the case
of vmsbackup restoring a file, a legal record count will never be greater than
the maximimu record size listed in the file's meta data which will be reported in
the directory listing (via -t or --list) after the VAR format ID or in the filename
if it chose to restore the file in binary.

I.e. If the record is of the ASCII "HELLO", the bytes in the record will be (in hex):
05 00 48 45 4C 4C 4F xx

where 'xx' is a filler byte, probably 00. It is not included in the record count but
it is just to ensure the record count of the next record is on an even byte bounday.
I'd guess that would be due to the VAX's CPU needing suitably aligned addresses to
access entities greater than 8 bits.

A blank line will contain:
00 00
```

The format of the VFC files is nearly the same as VAR above. The only difference
is there are n bytes of fortran carriage control bytes following the 2 bytes of
record size. The VFC bytes are included in the record's size. In all the cases I've
seen with this, there have only ever been 2 bytes of VFC. The format will be listed in
the directory and filenames as VFCx with the 'x' being the number of VFC bytes (always
2 as far as I've seen as mentioned) but if there is a different number of bytes, the
'x' will be set accordingly.

In the case of a 2 byte VFC, the value of the first byte (vfc0) ID's the start of the
output according to:
```
  0  - (as in nul) no leading carriage control
 ' ' - (space) Normal: \n followed by text followed by \r
 '$' - Prompt: \n followed by text, no \r at end of line
 '+' - Overstrike: text followed by \r
 '0' - Double space: \n \n followed by text followed by \r
 '1' - Formfeed: \f followed by text followed by \r
 any - any other is same as Normal above 
```

Despite the comments above about the end-of-record carriage control, it is actually
determined by the second byte (vfc1) and handled separately as:
```
 Bit in byte
 ---------------
 7 6 5 4 3 2 1 0
 0 0 0 0 0 0 0 0 - no trailing carriage control
 0 x x x x x x x - bits 6-0 indicate how many nl's to output followed by a cr
 1 0 0 x x x x x - bits 4-0 describe the end-of-record char (normally a 0x0D)
 1 0 1 * * * * * - all other conditions = just one cr
 1 1 0 0 x x x x - bits 3-0 describe bits to send to VFU. If no VFU, just one cr 
 1 1 0 1 * * * * - all other conditions = just one cr
 1 1 1 * * * * * - all other conditions = just one cr

where 'x' can be 0 or 1 and means something and '*' means not used.
```
For example, if the VFC record is of a single line of ASCII "HELLO", the bytes in
the record will probably be (in hex):
```

07 00 01 8D 48 45 4C 4C 4F xx

As with VAR, the xx is a filler byte.
```

Where vfc0 (0x01) indicates the leading character is "normal" and to output a newline (0x0A) and
vfc1 (0x8D) indicates to output a 0x0D (carrage return) at the end of the record.


