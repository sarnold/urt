#! /bin/sh
#
# convert a dvi file to RLE
#

tflags=
flags=
q=
v=
title=

# eat arguments

while [ $# -gt 0 ]
do
	case "$1" in
	-m)
		shift
		flags="$flags -m $1";;
	-d)
		shift
		flags="$flags -d $1";;
	-[xy])
	    	tflags="$tflags $1 $2"
		shift;;
	-s)
	    	flags="$flags $1"
		tflags="$tflags $1";;
	-*)
		flags="$flags $1";;
	*)
		break
	esac
	shift
done

if [ $# != 1 ]; then
	echo "Usage: $0 [-h] [-s] [-m mag] [-d drift] filename" 1>&2
	exit 1
fi

dvifile=$1

if [ ! -r $dvifile ]; then
	dvifile=$1.dvi
	if [ ! -r $dvifile ]; then
		echo "$0: cannot find $1 or $1.dvi" 1>&2
		exit 1
	fi
fi

dvirle1 $flags $dvifile | dvirle2 $tflags

