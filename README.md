# vmsbackup
Tools to decode VAX/VMS backup savesets and others to handle tape image files.

# This is from the old README:

This progam reads a VMS backuptape.

The tape program is orginally written by John Douglas Carey and
the pattern matching routine by some unknown on the net. 

The remote tape option use the rmtlib from mod.sources.

A good way to archive remotetape access for users with only
a local account is to create a "netwide" user tar and let
the remote tape programs do suid to user tar.

The program is tested on vax and sun.


Sven-Ove Westberg 
Lulea University of Technology
S-951 87  Lulea,  Sweden
UUCP:  sow@luthcad.UUCP
UUCP:  {decvax,philabs,seismo}!mcvax!enea!luthcad!sow

# This is clipped from the comments from vmsbackup.c

/*
*  @author John Douglas CAREY.
*  @author Sven-Ove Westberg    (version 3.0 and up)
*  @author Dave Shepperd, (DMS) vmsbackup@dshepperd.com
*  		(version Mar_2003 with snipits from Sven-Ove's
*  		version 4-1-1)
*
*  [VMSBACKUP on github](https://github.com/DaveShepperd/vmsbackup)
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

