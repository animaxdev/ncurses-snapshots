
/***************************************************************************
*                            COPYRIGHT NOTICE                              *
****************************************************************************
*                ncurses is copyright (C) 1992-1995                        *
*                          Zeyd M. Ben-Halim                               *
*                          zmbenhal@netcom.com                             *
*                          Eric S. Raymond                                 *
*                          esr@snark.thyrsus.com                           *
*                                                                          *
*        Permission is hereby granted to reproduce and distribute ncurses  *
*        by any means and for any fee, whether alone or as part of a       *
*        larger distribution, in source or in binary form, PROVIDED        *
*        this notice is included with any such distribution, and is not    *
*        removed from any of its header files. Mention of ncurses in any   *
*        applications linked with it is highly appreciated.                *
*                                                                          *
*        ncurses comes AS IS with no warranty, implied or expressed.       *
*                                                                          *
***************************************************************************/

#include "form.priv.h"

typedef struct {
  char **kwds;
  int  count;
  bool checkcase;
  bool checkunique;
} enumARG;

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  static void *Make_Enum_Type( va_list * ap )
|   
|   Description   :  Allocate structure for enumeration type argument.
|
|   Return Values :  Pointer to argument structure or NULL on error
+--------------------------------------------------------------------------*/
static void *Make_Enum_Type(va_list * ap)
{
  enumARG *argp = (enumARG *)malloc(sizeof(enumARG));
  char **kp;
  int cnt=0;

  if (argp)
    {
      int ccase, cunique;
      argp->kwds        = va_arg(*ap,char **);
      ccase             = va_arg(*ap,int);
      cunique           = va_arg(*ap,int);
      argp->checkcase   = ccase   ? TRUE : FALSE;
      argp->checkunique = cunique ? TRUE : FALSE;
    
      kp = argp->kwds;
      while( (*kp++) ) cnt++;
      argp->count = cnt;
    }
  return (void *)argp;
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  static void *Copy_Enum_Type( const void * argp )
|   
|   Description   :  Copy structure for enumeration type argument.  
|
|   Return Values :  Pointer to argument structure or NULL on error.
+--------------------------------------------------------------------------*/
static void *Copy_Enum_Type(const void * argp)
{
  enumARG *ap  = (enumARG *)argp;
  enumARG *new = (enumARG *)0;

  if (argp)
    {
      new = (enumARG *)malloc(sizeof(enumARG));
      if (new)
	*new = *ap;
    }
  return (void *)new;
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  static void Free_Enum_Type( void * argp )
|   
|   Description   :  Free structure for enumeration type argument.
|
|   Return Values :  -
+--------------------------------------------------------------------------*/
static void Free_Enum_Type(void * argp)
{
  if (argp) 
    free(argp);
}

#define SKIP_SPACE(x) while(((*(x))!='\0') && (is_blank(*(x)))) (x)++
#define NOMATCH 0
#define PARTIAL 1
#define EXACT   2

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  static int Compare(const unsigned char * s,  
|                                       const unsigned char * buf,
|                                       bool  ccase )
|   
|   Description   :  Check wether or not the text in 'buf' matches the
|                    text in 's', at least partial.
|
|   Return Values :  NOMATCH   - buffer doesn't match
|                    PARTIAL   - buffer matches partially
|                    EXACT     - buffer matches exactly
+--------------------------------------------------------------------------*/
static int Compare(const unsigned char *s, const unsigned char *buf, 
		   bool ccase)
{
  SKIP_SPACE(buf); /* Skip leading spaces in both texts */
  SKIP_SPACE(s);

  if (*buf=='\0')
    {
      return (((*s)!='\0') ? NOMATCH : EXACT);
    } 
  else 
    {
      if (ccase)
	{
	  while(*s++ == *buf)
	    {
	      if (*buf++=='\0') return EXACT;
	    } 
	} 
      else 
	{
	  while(toupper(*s)==toupper(*buf))
	    {
	      s++;
	      if (*buf++=='\0') return EXACT;
	    }
	}
    }
  /* At this location buf points to the first character where it no longer
     matches with s. So if only blanks are following, we have a partial
     match otherwise there is no match */
  SKIP_SPACE(buf);
  if (*buf) 
    return NOMATCH;

  /* If it happens that the reference buffer is at its end, the partial
     match is actually an exact match. */
  return ((s[-1]!='\0') ? PARTIAL : EXACT);
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  static bool Check_Enum_Field(
|                                      FIELD * field,
|                                      const void  * argp)
|   
|   Description   :  Validate buffer content to be a valid enumeration value
|
|   Return Values :  TRUE  - field is valid
|                    FALSE - field is invalid
+--------------------------------------------------------------------------*/
static bool Check_Enum_Field(FIELD * field, const void  * argp)
{
  char **kwds       = ((enumARG *)argp)->kwds;
  bool ccase        = ((enumARG *)argp)->checkcase;
  bool unique       = ((enumARG *)argp)->checkunique;
  unsigned char *bp = (unsigned char *)field_buffer(field,0);
  char *s, *t, *p;
  int res;
  
  while( (s=(*kwds++)) )
    {
      if ((res=Compare((unsigned char *)s,bp,ccase))!=NOMATCH)
	{
	  t=s;
	  if ((unique && res!=EXACT)) 
	    {
	      while( (p = *kwds++) )
		{
		  if ((res=Compare((unsigned char *)p,bp,ccase))!=NOMATCH)
		    {
		      if (res==EXACT)
			{
			  t = p;
			  break;
			}	
		      t = (char *)0;
		    }
		}
	    }
	  if (t)
	    {
	      set_field_buffer(field,0,t);
	      return TRUE;
	    }
	}
    }
  return FALSE;
}

static const char *dummy[] = { (char *)0 };

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  static bool Next_Enum(FIELD * field,
|                                          const void * argp)
|   
|   Description   :  Check for the next enumeration value
|
|   Return Values :  TRUE  - next value found and loaded
|                    FALSE - no next value loaded
+--------------------------------------------------------------------------*/
static bool Next_Enum(FIELD * field, const void * argp)
{
  enumARG *args     = (enumARG *)argp;
  char **kwds       = args->kwds;
  bool ccase        = args->checkcase;
  int cnt           = args->count;
  unsigned char *bp = (unsigned char *)field_buffer(field,0);

  while(cnt--)
    {
      if (Compare((unsigned char *)(*kwds++),bp,ccase)==EXACT) 
	break;
    }
  if (cnt<=0)
    kwds = args->kwds;
  if ((cnt>=0) || (Compare((unsigned char *)dummy,bp,ccase)==EXACT))
    {
      set_field_buffer(field,0,*kwds);
      return TRUE;
    }
  return FALSE;
}

/*---------------------------------------------------------------------------
|   Facility      :  libnform  
|   Function      :  static bool Previous_Enum(
|                                          FIELD * field,
|                                          const void * argp)
|   
|   Description   :  Check for the previous enumeration value
|
|   Return Values :  TRUE  - previous value found and loaded
|                    FALSE - no previous value loaded
+--------------------------------------------------------------------------*/
static bool Previous_Enum(FIELD * field, const void * argp)
{
  enumARG *args = (enumARG *)argp;
  int cnt       = args->count;
  char **kwds   = &args->kwds[cnt-1];
  bool ccase    = args->checkcase;
  unsigned char *bp = (unsigned char *)field_buffer(field,0);

  while(cnt--)
    {
      if (Compare((unsigned char *)(*kwds--),bp,ccase)==EXACT) 
	break;
    }

  if (cnt<=0)
    kwds  = &args->kwds[args->count-1];

  if ((cnt>=0) || (Compare((unsigned char *)dummy,bp,ccase)==EXACT))
    {
      set_field_buffer(field,0,*kwds);
      return TRUE;
    }
  return FALSE;
}


static FIELDTYPE typeENUM = {
  _HAS_ARGS | _HAS_CHOICE | _RESIDENT,
  1,
  (FIELDTYPE *)0,
  (FIELDTYPE *)0,
  Make_Enum_Type,
  Copy_Enum_Type,
  Free_Enum_Type,
  Check_Enum_Field,
  NULL,
  Next_Enum,
  Previous_Enum
};

FIELDTYPE* TYPE_ENUM = &typeENUM;

/* fty_enum.c ends here */
