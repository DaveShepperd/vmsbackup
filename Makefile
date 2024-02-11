#
#
CC= gcc
DEFS= -DHAVE_STRERROR #-DSWAP -DREMOTE
MACH= -m32
OPT= #-O2
DBG= -g #-DDEBUG
WARNS= -Wall #-ansi -pedantic
CFLAGS= $(DEFS) $(MACH) $(OPT) $(DBG) $(WARNS) 
LFLAGS= $(MACH)
LIBS= -lrmt   			# remote magtape library
OWNER=tar			# user for remote tape access
MODE=4755
BINDIR=/usr/local/bin
MANSEC=l
MANDIR=/usr/man/man$(MANSEC)

#
default: vmsbackup cp_tape dmp_tfile ext_tfile unpack_tap

vmsbackup: vmsbackup.o match.o
	cc $(LFLAGS) -o vmsbackup vmsbackup.o match.o 
cp_tape: cp_tape.o
	cc $(LFLAGS) -o $@ $<
dmp_tfile: dmp_tfile.o
	cc $(LFLAGS) -o $@ $<
ext_tfile: ext_tfile.o
	cc $(LFLAGS) -o $@ $<
unpack_tap: unpack_tap.o
	cc $(LFLAGS) -o $@ $<

install:
	install -m $(MODE) -o $(OWNER) -s vmsbackup $(BINDIR)	
	cp vmsbackup.1 $(MANDIR)/vmsbackup.$(MANSEC)
clean:
	rm -f vmsbackup vmsbackup.exe ext_tfile ext_tfile.exe cp_tape cp_tape.exe dmp_tfile dmp_tfile.exe unpack_tap unpack_tap.exe *.o core
shar:
	shar -a README vmsbackup.1 Makefile vmsbackup.c match.c \
	    > vmsbackup.shar
