#! /bin/awk -f
#
# Massage the makefile to include or remove specific device programs.
#
# This program can be run in a "replacement" mode on a makefile, or can 
# be used to process a makefile.src into a makefile.  If QUIET is not defined,
# the file is processed in a manner that leaves all the original
# information in it.  Values of #defined symbols can be changed, and
# the process repeated to get a new configuration.  If QUIET is defined, all
# preprocessor lines, comment lines, and lines that are #ifdef-ed out
# will not appear in the output.  QUIET mode is suitable only for processing
# a source file into a makefile.
#
# Special input forms are:
#
# #quiet on
# #quiet off
# 	Turn QUIET mode on or off
#
# DEPTH = string
# 	A string that will be prepended to all relative path variables.  It
#	is usually of the form "../" (repeated some number of times).  The
#	Configure script automatically defines this variable.
#
# #define SYMBOL comments ...
#	Define a SYMBOL that can be used for conditional inclusion (see
#	#ifdef and #ifadd below
#
# #defpath VARIABLE VALUE comments ...
# 	Define VARIABLE to have a path VALUE.  If the VALUE does not start
#	with a "/", then the value of DEPTH will be prepended.  This value
#	should be output with #emit or #set.  VALUE may contain other
#	make variables (i.e., "$(VAR)"), but these variables will not
#	be expanded when deciding whether VALUE starts with a "/".
#
# #ifdef SYMBOL1|SYMBOL2|...
# ...
# #else
# ...
# #endif
# 	If any of the the SYMBOLs are defined, then
#		Any line beginning #@ has the #@ removed.  
#		Other lines are unchanged.
# 	If none of the symbols are defined, then
# 		Any line not beginning with a # has a #@ inserted.
#		Other lines are unchanged.
#	#ifdefs may NOT be nested.
#	There should be no spaces in the symbol string.
# 
# #ifndef SYMBOL1|SYMBOL2|...
#	Like #ifdef, but includes lines if any of the SYMBOLs are not defined.
#	I.e., lines are not included only if all the SYMBOLs are defined.
#	Thus, you can use #ifndef a|b / #else / ... / #endif to include
#	text only if both a and b are defined.
# 
# #ifadd SYMBOL VARIABLE value ...
# 	Adds the "value ..." string to the end of the make VARIABLE if 
#	the SYMBOL is defined.
#	#emit or #set is required to tell make about the value.
#
# #ifnadd SYMBOL VARIABLE value ...
# 	Adds the "value ..." to the end of the make VARIABLE if the
#	SYMBOL is not defined.
#
# #add	VARIABLE value ...
#	Adds the "value ..." to the end of the make VARIABLE always.
# 
# #emit VARIABLE
# VARIABLE = STUFF
# 	These two lines cause the built-up value of VARIABLE (as set by
#	#defpath or #ifadd lines) to be added to the makefile.  Any STUFF
#	following the = will be replaced with the VARIABLE's current value.
#
# #set VARIABLE1 VARIABLE2
# VARIABLE1 = STUFF
#	#set is similar to #emit, except that if VARIABLE2 has a value,
#	it will be output instead of VARIABLE1's value.  This is usually
#	used to override a generic variable with a makefile-specific
#	one.  For example,
# 
# # In generic configuration file
# #defpath DEST 	bin	Default destination directory
# #defpath SPECIAL_DEST progs	Destination dir for special programs.
#
# # In special progs makefile.  If SPECIAL_DEST is defined, use it,
# # otherwise use default DEST value.
# #set	DEST	SPECIAL_DEST
# DEST = ../progs
#
# #alldefs VARIABLE
# VARIABLE = STUFF
# 	#alldefs sets the VARIABLE to a space-separated sequence of all
#	the configuration symbols that have been defined so far (by
#	#define).


BEGIN		{ 
			processing = 0; 
			emitting = 0; 
			emit = ""; 
			quiet = 0;
			blank_lines = 0;
			print "#%#### makefile AUTOMATICALLY GENERATED ######";
			print "#%############### DO NOT EDIT ################";
			print "#%### EDIT makefile.src AND make config ######";
		}

/^#%/		{ next; }

/^DEPTH/	{ depth = $3; }

/^#quiet/	{
		    wasquiet = quiet;
		    if ( $2 == "on" ) quiet = 1; else quiet = 0;
		    if ( wasquiet == 1 ) next;
		}

/^#define/	{ 
		    if ( processing == 0 || including == 1 ) {
			syms[$2] = 1;
			alldefs = alldefs " " $2;
		    }
		}

/^#defpath/	{
		    if ( processing == 0 || including == 1 ) {
			syms[$2] = 1;
			if ( substr($3,0,1) == "/" ) {
			    vars[$2] = $3; 
			} else {
			    vars[$2] = depth $3; 
			} 
		    }
		}

/^#ifdef/	{ 
		    processing = 1; 
		    including = 0;
		    n = split( $2, defs, "|" );
		    for ( i = 1; i <= n; i++ )
			if ( syms[defs[i]] == 1 ) 
			    including = 1; 
		    if ( quiet == 0 ) print; 
		    next; 
		}

/^#ifndef/	{ 
		    processing = 1; 
		    including = 0;
		    n = split( $2, defs, "|" );
		    for ( i = 1; i <= n; i++ )
			if ( syms[defs[i]] != 1 ) 
			    including = 1; 
		    if ( quiet == 0 ) print; 
		    next; 
		}

/^#else/	{ 
		    if ( processing == 1 ) {
			if ( including == 1 ) {
			    including = 0; 
			} else  including = 1;
		    }
		    if ( quiet == 0 ) print; 
		    next; 
		}

/^#endif/	{ processing = 0; }

/^#ifadd/	{ 
		    if ( syms[$2] == 1 ) {
			for ( i = 4; i <= NF; i++ ) {
			    vars[$3] = vars[$3] " " $i;
			}
		    }
		}

/^#ifnadd/	{ 
		    if ( syms[$2] != 1 ) {
			for ( i = 4; i <= NF; i++ ) {
			    vars[$3] = vars[$3] " " $i;
			}
		    }
		}

/^#add/	{ 
		for ( i = 3; i <= NF; i++ ) vars[$2] = vars[$2] " " $i;
	}

# If this line (and ones like it) cause problems (some versions of awk
# complain, apparently), change it to something like
# /^#@/ { if ( processing == 1 && including == 1 ) {
# 	...
# 	} }

/^#@/ && processing == 1 && including == 1 {
		    printf( "%s\n", substr( $0, 3 ) );
		    next;
		}

processing == 1 && including != 1 && /^[^#]/ {
		    if ( quiet == 0 ) printf( "#@%s\n", $0 );
		    next;
		}

emitting == 1 && $1 == emit {
		    printf( "%s = %s\n", $1, vars[$1] );
		    emitting = 0;
		    next;
		}

/^#emit/	{ emitting = 1; emit = $2; }

/^#set/		{ 
		    if ( syms[$3] == 1 ) vars[$2] = vars[$3];
		    emitting = 1;
		    emit = $2;
		}

/^#alldefs/	{ emitting = 1; vars[$2] = alldefs; emit = $2; }


quiet == 1 && /^#/	{ next; }

/^$/		{ blank_lines++; if ( quiet == 1 && blank_lines > 1 ) next; }

/./		{ blank_lines = 0; }

		{ print; }
