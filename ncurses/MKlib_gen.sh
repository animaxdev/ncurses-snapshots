#!/bin/sh
#
# MKlib_gen.sh -- generate sources from curses.h macro definitions
#
# (@Id: MKlib_gen.sh,v 1.1 1995/11/13 22:31:45 esr Exp @)
#
# The XSI Curses standard requires all curses entry points to exist as
# functions, even though many definitions would normally be shadowed
# by macros.  Rather than hand-hack all that code, we actually
# generate functions from the macros.
#
# This script accepts a file of prototypes on standard input.  It discards
# any that don't have a `generated' comment attached. It then parses each
# prototype (relying on the fact that none of the macros take function 
# pointer or array arguments) and generates C source from it.
#
# Here is what the pipeline stages are doing:
#
# 1. sed: extract prototypes of generated functions
# 2. sed: decorate prototypes with generated arguments a1. a2,...z
# 3. awk: generate the calls with args matching the formals 
# 4. sed: prefix function names in prototypes so the preprocessor won't expand
#         them.
# 5. cpp: macro-expand the file so the macro calls turn into C calls
# 6. awk: strip the expansion junk off the front and add the new header
# 7. sed: squeeze spaces, strip off gen_ prefix, create needed #undef
#

preprocessor="$1 -I../include"
AWK="$2"
TMP=gen$$.c
trap "rm -f $TMP" 0 1 2 5 15

(cat <<EOF
#include "curses.h"

DECLARATIONS

EOF
sed -n -e "/^extern.*generated/s/^extern \([^;]*\);.*/\1/p" \
| sed \
	-e "/(/s// (/" \
	-e "/(void)/b nc" \
	-e "s/,/ a1% /" \
	-e "s/,/ a2% /" \
	-e "s/,/ a3% /" \
	-e "s/,/ a4% /" \
	-e "s/,/ a5% /" \
	-e "s/,/ a6% /" \
	-e "s/,/ a7% /" \
	-e "s/,/ a8% /" \
	-e "s/,/ a9% /" \
	-e "s/,/ a10% /" \
	-e "s/,/ a11% /" \
	-e "s/,/ a12% /" \
	-e "s/,/ a13% /" \
	-e "s/,/ a14% /" \
	-e "s/,/ a15% /" \
	-e "s/%/,/g" \
	-e "s/)/ z)/" \
	-e ":nc" \
| $AWK '{
	print "\n"
	print "M_" $2
	print $0;
	print "{";
	for (i = argcount = 0; i < length($0); i++)
		if (substr($0, i, 1) == ",")
			argcount++;
	++argcount;
	if (match($0, "^void"))
		call = "%%"
	else
		call = "%%return ";
	call = call $2 "(";
	for (i = 1; i < argcount; i++)
		call = call " a" i ", ";
	if (match($0, "\\(void\\)"))
		call = call ");";
	else
		call = call "z);";
	print call
	print "}";
}
' ) \
| sed \
	-e '/^\([a-z_][a-z_]*\) /s//\1 gen_/' >$TMP
  $preprocessor $TMP 2>/dev/null \
| $AWK '
BEGIN		{
	print "/*"
	print " * DO NOT EDIT THIS FILE BY HAND!"
	print " * It is generated by MKlib_gen.sh."
	print " *"
	print " * This is a file of trivial functions generated from macro"
	print " * definitions in curses.h in order to satisfy the XSI Curses"
	print " * requirement that every macro also exist as a callable"
	print " * function."
	print " *"
	print " * It will never be linked unless you call one of the entry"
	print " * points with its normal macro definition disabled.  In that"
	print " * case, if you have no shared libraries, it will indirectly"
	print " * pull most of the rest of the library into your link image."
	print " * Cope with it."
	print " */"
	print "#include \"curses.h\""
	print ""
		}
/^DECLARATIONS/	{start = 1; next;}
		{if (start) print $0;}
' \
| sed \
	-e 's/		*/ /g' \
	-e 's/  */ /g' \
	-e 's/ gen_/ /' \
	-e 's/^M_/#undef /' \
	-e '/^%%/s//	/'

