
AWK=${1-awk}
DATA=${2-../include/Caps.filtered}

cat <<'EOF'
/*
 *	comp_captab.c -- The names of the capabilities in a form ready for
 *		         the making of a hash table for the compiler.
 *
 */

#include "tic.h"
#include "term.h"
#include "hashsize.h"

static	struct name_table_entry	_nc_info_table[] =
{
EOF

$AWK <$DATA '
BEGIN		{
		    tablesize = 0;
		}

		# skip #-led comments
/^#/		{
		    next;
		}

$3 == "bool"	{
		    printf "\t{ 0,%15s,\tBOOLEAN,\t%3d },\n", $2, BoolCount++
		}


$3 == "num"	{
		    printf "\t{ 0,%15s,\tNUMBER,\t\t%3d },\n", $2, NumCount++
		}


$3 == "str"	{
		    printf "\t{ 0,%15s,\tSTRING,\t\t%3d },\n", $2, StrCount++
		}
'

cat <<'EOF'
};

const struct alias _nc_alias_table[] =
{
EOF

$AWK <$DATA '
$1 == "alias"	{
		    printf "\t{%s, %s},\t /* %s */\n", $2, $3, $4
		}
'

cat <<'EOF'
	{(char *)NULL, (char *)NULL}
};

static	struct name_table_entry	_nc_cap_table[] =
{
EOF

$AWK <$DATA '
BEGIN		{
		    tablesize = 0;
		    BoolCount = NumCount = StrCount = 0;
		}


		# skip #-led comments
/^#/		{
		    next;
		}

$3 == "bool"	{
		    printf "\t{ 0,%15s,\tBOOLEAN,\t%3d },\n", $4, BoolCount++
		    tablesize++;
		}


$3 == "num"	{
		    printf "\t{ 0,%15s,\tNUMBER,\t\t%3d },\n", $4, NumCount++
		    tablesize++;
		}


$3 == "str"	{
		    printf "\t{ 0,%15s,\tSTRING,\t\t%3d },\n", $4, StrCount++
		    tablesize++;
		}

END	{
	    print  "} /* " tablesize " entries */;"
	    print  ""
	    print  "struct name_table_entry *_nc_info_hash_table[HASHTABSIZE];"
	    print  "struct name_table_entry *_nc_cap_hash_table[HASHTABSIZE];"
	    print  ""
	    printf "#if (BOOLCOUNT!=%d)||(NUMCOUNT!=%d)||(STRCOUNT!=%d)\n",\
						BoolCount, NumCount, StrCount
	    print  "#error	--> term.h and comp_captab.c disagree about the <--"
	    print  "#error	--> numbers of booleans, numbers and/or strings <--"
	    print  "#endif"
	    print  ""
	    print  "struct name_table_entry *_nc_get_table(bool termcap)"
	    print  "{"
	    print  "	return termcap ? _nc_cap_table: _nc_info_table ;"
	    print  "}"
	}
'
