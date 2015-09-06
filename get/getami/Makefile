#
# Makefile for use with Lattice C
#
STARTUP=LIB:c.o
DEBUG = -O
LINKDBG = ND
#
# Use these settings on a standard Amiga.
# If you have a version of Lattice C earlier than 5.10, replace the -ba
# switch with -b0, and link with lcnb.lib instead of lc.lib.
#
# CFLAGS = -ma -v -fi -cs -ba $(DEBUG)
# MATHLIB = LIB:lcmieee.lib
#
# use these settings on an Amiga with a 68020
#
CFLAGS = -m2 -v -f8 -cs -ba $(DEBUG)
MATHLIB = LIB:lcm881.lib
#
# if you have getopt and qsort in a library, declare it in SUPLIB
#
SUPLIB = LIB:unix.lib
SUPPORT = mktemp.o
#
# else declare SUPLIB as empty, and add the appropriate .o files in SUPPORT
# (alloca.o is only needed if you use the GNU getopt)
#
# SUPLIB =
# SUPPORT = mktemp.o qsort.o getopt.o alloca.o
#
# Override certain library functions with home-made, smaller ones. Their only
# purpose is to make executables smaller, so you don't really need them. If you
# have something equivalent, declare it here. GDEF is for getami, and SDEF is
# for show3.
#
GDEF = DEFINE __main=__nowbmain
SDEF = DEFINE __main=__nowbmain
#
# ".o" files and libraries required to build getami
#
GOBJ = getami.o AdjustScreen.o $(SUPPORT)
GLIBS = LIB:rle.lib $(MATHLIB) $(SUPLIB) LIB:lc.lib
#
# ".o" files and libraries required to build show3.
#
SOBJ = show3.o CenterScreen.o
SLIBS = $(SUPLIB) LIB:lc.lib

all: getami show3

getami: $(GOBJ)
	blink FROM $(STARTUP) $(GOBJ) TO getami SC SD $(LINKDBG) $(GDEF) LIB $(GLIBS)

show3: $(SOBJ)
	blink FROM $(STARTUP) $(SOBJ) TO show3 SC SD $(LINKDBG) $(SDEF) LIB $(SLIBS)

clean:
	delete \#?.o getami show3 QUIET
