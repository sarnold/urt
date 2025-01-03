=========================
 The Utah Raster Toolkit
=========================

(and RLE graphics library)
==========================

|ci|

|tag| |license| |contributors|


This is the Utah Raster Toolkit distribution version 3.x plus build
"modernization" and code fixes.  Next release will be a bug-fix
and build update maintenance release.

* Changes in maintenance release candidate 3.2_rc1: https://github.com/sarnold/urt/releases

Other changes since version 3.0 include:

* Updates for compiler and make/build fixes
* Fixed type conflict with OS/X stack_t type
* Added PIC and shared library to the default build
* Includes all Gentoo patches applied to 3.1b-r1

Major changes from version 2.0 are detailed (as much as possible) in the
CHANGES file.  One change to be aware of, if you have toolkit programs
of your own is that many function and data structure names have
changed.  See CHANGES for details.

The source-only distribution contains the following subdirectories::

	cnv:		File conversion directory.  A collection of
				utilities for converting to and from the Utah
				RLE format.

	etc:		Contains some other sample programs that may
				or may not be useful, but may be interesting to
				look at if you are writing RLE programs.

	get:		Contains programs to display RLE images on various
				types of displays, and instructions on how to
				roll your own (see the README)

	include:	Necessary ".h" files for the library and toolkit.

	lib:		Sources for the RLE library.

	man:		Manual pages for the library and tools.

	tools:		Sources for the tools.

	tex:		Source for dvi to rle conversion program.
				Also has dviselect program.  Requires U of
				Maryland MC-TeX distribution (see README).

	img:		Contains a couple of uuencoded (use uudecode
				to get them back) RLE files.

The FTP distribution contains the following directories, as well::

	The following two directories are in urt-doc.tar.Z.

	doc:		Contains long narrative descriptions of the RLE
				file format and the Toolkit.  Documentation files
				are in Scribe ".mss" format, PostScript ".ps" form,
				and cleartext (".doc).  A formatted version of the
				toolkit paper appeared in the Proceedings of the
				USENIX 3rd Graphics Workshop.

	doc/pics:	Postscript form of the raster images for the
				toolkit paper.

	img:		Includes some interesting RLE image files.
				Distributed as urt-img.tar.Z.

To make and install the toolkit, you should::

	0. If you got the source-only distribution, remove all the
	   urt.kit?? and kit*isdone files:
	   rm urt.kit?? kit*isdone

	1. Edit config/urt (I recommend making a copy and editing that
	   instead) to reflect your site configuration.  The
	   definitions in this file should be relatively self-
	   explanatory.  See config/README for more details.  If you
	   want to understand what is going on, look at the comments
	   in the makedef.awk script.

	   You should also look at the other files in the config
	   directory, as one of them may already be correct for you.
	   All the configuration files compile as many of the
	   conversion tools as possible for the given machine.  If you
	   don't want some of these tools, comment out the corresponding
	   lines.

	2. Configure the makefiles by running
	   Configure config/urt
	   If you have made a different config file, substitute its
	   name for config/urt, of course.  Once you have done this
	   once, you can use 'make config' instead.

	3a. If you have set the "RL" path in the config file to
	    something besides "lib" (presumably the same as LIB_DEST),
	    you must first
			cd lib; make install
	    Then do 3b.

	3b. Run 'make' in this directory.  This will
	   compile the library and tools you have selected.

	4. Run 'make install' in this directory.  Alternatively, you
	   can combine 3a, 3b, and 4 by just running 'make install'
	   right away.

This version of the toolkit has been successfully compiled on the
following machines (with the corresponding configuration file
indicated in parentheses)::

	Sun 3 (SunOS 4) with cc (config/sun3) and gcc
	Sun 4 (SunOS 4) (config/sun4)
	DEC 3100 (Ultrix 3.1) (config/dec)
	IBM RT (AIX ??) (config/ibm-rt)
	SGI Iris 4D (IRIX 3.2) (config/iris4d)
	Apollo (SR 10.2) (config/apollo)
	Stardent GS1000 (config/stellar)
	HP 9000/3xx,8xx (HP-UX 7.03) (config/hpux300 config/hpux800)
	Macintosh (A/UX) [note - getmac program does not work on A/UX]
	Macintosh (MacOS) with MPW [at least library and getmac program]
	Cray 2 (UNICOS) (config/cray)

If you find bugs, make improvements, write new tools or conversions,
or have questions or suggestions, please send them to the address below.

If you want to write your own tool, we suggest that you start with
tools/rleskel.c.  This has all the right code it it for opening image
files for input and output, for processing multiple images per file,
error checking, etc.

Our thanks to some beta testers (and contributors)::

	Eric Haines, 3D Eye
	Gregg Townsend, CS Dept., U of Arizona
	John Peterson, Apple Computer

Also thanks to the new Github PR contributors::

	@ceamac
	@jopadan
	@moshekaplan

And to all of you who submitted new programs or ideas for new
features, especially Craig Kolb (Yale), whose 'rayshade' program provided
(indirectly) the incentive for this (old) release.

A special thanks to Martin Friedmann, MIT Media Lab, who almost
totally revamped the 'getx11' program, so that it now works correctly,
and is the most featurful of all the toolkit programs.

Original core authors::

	Spencer W. Thomas

	Rod G. Bogart

	James Painter

	John W. Peterson  (http://www.saccade.com/)


License
=======

This project is licensed under the GPL license - see the `COPYING file`_ for
details.

.. _COPYING file: https://github.com/sarnold/urt/blob/master/COPYING


.. |ci| image:: https://github.com/sarnold/urt/actions/workflows/ci.yml/badge.svg
    :target: https://github.com/sarnold/urt/actions/workflows/ci.yml
    :alt: CI Status

.. |license| image:: https://img.shields.io/github/license/sarnold/urt
    :target: https://github.com/sarnold/urt/blob/master/COPYING
    :alt: License

.. |tag| image:: https://img.shields.io/github/v/tag/sarnold/urt?color=green&include_prereleases&label=latest%20release
    :target: https://github.com/sarnold/urt/releases
    :alt: GitHub tag

.. |contributors| image:: https://img.shields.io/github/contributors/sarnold/urt
   :target: https://github.com/sarnold/urt/
   :alt: Contributors
