#
#
# Set this to 1 if you want to include magtape device support
HAVE_MTIO = 0
# Set this to 1 if you are building under MSYS2 on Windows
UNDER_MSYS2 = 0

include Makefile.common

default: vmsbackup cp_tape dmp_tfile ext_tfile unpack_tap
