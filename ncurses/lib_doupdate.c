
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


/*-----------------------------------------------------------------
 *
 *	lib_doupdate.c
 *
 *	The routine doupdate() and its dependents
 *
 *-----------------------------------------------------------------*/
 
#include "curses.priv.h"
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#if HAVE_SYS_SELECT_H
#include <sys/types.h>
#include <sys/select.h>
#endif
#include <string.h>
#include "term.h"

/*
 * Enable checking to see if doupdate and friends are tracking the true
 * cursor position correctly.  NOTE: this is a debugging hack which will
 * work ONLY on ANSI-compatible terminals!
 */
/* #define POSITION_DEBUG */

static void ClrUpdate( WINDOW *scr );
static void TransformLine( int lineno );
static void NoIDcTransformLine( int lineno );
static void IDcTransformLine( int lineno );
static void ClearScreen( void );
static void InsStr( chtype *line, int count );
static void DelChar( int count );

#ifdef POSITION_DEBUG
/****************************************************************************
 *
 * Debugging code.  Only works on ANSI-standard terminals.
 *
 ****************************************************************************/

void position_check(int expected_y, int expected_x, char *legend)
/* check to see if the real cursor position matches the virtual */
{
    static char  buf[9];
    int y, x;

    if (_tracing)
	return;

    memset(buf, '\0', sizeof(buf));
    (void) write(1, "\033[6n", 4);	/* only works on ANSI-compatibles */
    (void) read(0, (void *)buf, 8);
    _tracef("probe returned %s", visbuf(buf));

    /* try to interpret as a position report */
    if (sscanf(buf, "\033[%d;%dR", &y, &x) != 2)
	_tracef("position probe failed in %s", legend);
    else if (y - 1 != expected_y || x - 1 != expected_x)
	_tracef("position seen (%d, %d) doesn't match expected one (%d, %d) in %s",
		y-1, x-1, expected_y, expected_x, legend);
    else
	_tracef("position matches OK in %s", legend);
}
#endif /* POSITION_DEBUG */

/****************************************************************************
 *
 * Optimized update code
 *
 ****************************************************************************/

static inline void GoTo(int row, int col)
{
	chtype	oldattr = SP->_current_attr;

	TR(TRACE_MOVE, ("GoTo(%d, %d) from (%d, %d)",
			row, col, SP->_cursrow, SP->_curscol));

#ifdef POSITION_DEBUG
	position_check(SP->_cursrow, SP->_curscol, "GoTo");
#endif /* POSITION_DEBUG */

	/*
	 * Force restore even if msgr is on when we're in an alternate
	 * character set -- these have a strong tendency to screw up the
	 * CR & LF used for local character motions!
	 */
	if ((oldattr & A_ALTCHARSET)
#ifdef A_PCCHARSET
	    || (oldattr & A_PCCHARSET)
#endif /* A_PCCHARSET */
	    || (oldattr && !move_standout_mode))
	{
       		TR(TRACE_CHARPUT, ("turning off (%lx) %s before move",
		   oldattr, _traceattr(oldattr)));
		vidattr(A_NORMAL);
		curscr->_attrs = A_NORMAL;
	}

	mvcur(SP->_cursrow, SP->_curscol, row, col); 
	SP->_cursrow = row; 
	SP->_curscol = col; 
}

static inline void PutAttrChar(chtype ch)
{
	if (tilde_glitch && ((ch & A_CHARTEXT) == '~'))
		ch = ('`' | (ch & A_ATTRIBUTES));

	TR(TRACE_CHARPUT, ("PutAttrChar(%s, %s) at (%d, %d)",
			  _tracechar((ch & (chtype)A_CHARTEXT)),
			  _traceattr((ch & (chtype)A_ATTRIBUTES)),
			   SP->_cursrow, SP->_curscol));
	if (curscr->_attrs != (ch & (chtype)A_ATTRIBUTES)) {
		curscr->_attrs = ch & (chtype)A_ATTRIBUTES;
		vidputs(curscr->_attrs, _nc_outch);
	}
	putc(ch & A_CHARTEXT, SP->_ofp);
	if (char_padding) {
		TPUTS_TRACE("char_padding");
		putp(char_padding);
	}
}

static int LRCORNER = FALSE;

static inline void PutChar(chtype ch)
{
	if (LRCORNER == TRUE) {
		if (SP->_curscol == screen_columns-2) {
			PutAttrChar(newscr->_line[screen_lines-1].text[screen_columns-2]);
			SP->_curscol++;
#ifdef POSITION_DEBUG
			position_check(SP->_cursrow,SP->_curscol,"PutChar");
#endif /* POSITION_DEBUG */
			return;
		} else if (SP->_curscol == screen_columns-1) {
		int i = screen_lines;
		int j = screen_columns -1;
			GoTo(i-1, j);
			if (enter_insert_mode && exit_insert_mode) {
				TPUTS_TRACE("enter_insert_mode");
				putp(enter_insert_mode);
				PutAttrChar(newscr->_line[i-1].text[j]);
				if (insert_padding)
				{
					TPUTS_TRACE("insert_padding");
					putp(insert_padding);
				}
				TPUTS_TRACE("exit_insert_mode");
				putp(exit_insert_mode);
			} else if (insert_character) {
				TPUTS_TRACE("insert_character");
				putp(insert_character);
				PutAttrChar(newscr->_line[i-1].text[j]);
				if (insert_padding)
				{
					TPUTS_TRACE("insert_padding");
					putp(insert_padding);
				}
			}
			return;
		}
	}
	PutAttrChar(ch);
	SP->_curscol++; 
	if (SP->_curscol >= screen_columns) {
		if (eat_newline_glitch) {
 			/*
			 * xenl can manifest two different ways.  The vt100
			 * way is that, when you'd expect the cursor to wrap,
			 * it stays hung at the right margin (on top of the
			 * character just emitted) and doesn't wrap until the
			 * *next* graphic char is emitted.  The c100 way is
			 * to ignore LF received just after an am wrap.
			 *
			 * An aggressive way to handle this would be to 
			 * emit CR/LF after the char and then assume the wrap
			 * is done, you're on the first position of the next
			 * line, and the terminal out of its weird state.
			 * Here it's safe to just tell the code that the
			 * cursor is in hyperspace and let the next mvcur()
			 * call straighten things out.
			 */
			SP->_curscol = -1;	   
			SP->_cursrow = -1;
		} else if (auto_right_margin) { 
			SP->_curscol = 0;	   
			SP->_cursrow++;
		} else {
		 	SP->_curscol--;
		}
	}
#ifdef POSITION_DEBUG
	position_check(SP->_cursrow, SP->_curscol, "PutChar");
#endif /* POSITION_DEBUG */
}	

int _nc_outch(int ch)
{
	if (SP != NULL)
		putc(ch, SP->_ofp);
	else
		putc(ch, stdout);
	return OK;
}

int doupdate(void)
{
int	i;
	
	T(("doupdate() called"));

#ifdef TRACE
	if (_tracing & TRACE_UPDATE)
	{
	    if (curscr->_clear)
		_tracef("curscr is clear");
	    else
		_tracedump("curscr", curscr);
	    _tracedump("newscr", newscr);
	}
#endif /* TRACE */

	_nc_signal_handler(FALSE);

	if (SP->_endwin == TRUE) {
		T(("coming back from shell mode"));
		reset_prog_mode();
		if (enter_ca_mode)
		{
			TPUTS_TRACE("enter_ca_mode");
			putp(enter_ca_mode);
		}
		/*
		 * Undo the effects of terminal init strings that assume
		 * they know the screen size.  Useful when you're running
		 * a vt100 emulation through xterm.  Note: this may change
		 * the physical cursor location.
		 */
		if (change_scroll_region)
		{
			TPUTS_TRACE("change_scroll_region");
			putp(tparm(change_scroll_region, 0, screen_lines - 1));
		}
		newscr->_clear = TRUE;
		SP->_endwin = FALSE;
	}

	/* check for pending input */
	if (SP->_checkfd >= 0) {
	fd_set fdset;
	struct timeval timeout = {0,0};

		FD_ZERO(&fdset);
		FD_SET(SP->_checkfd, &fdset);
		if (select(SP->_checkfd+1, &fdset, NULL, NULL, &timeout) != 0) {
			fflush(SP->_ofp);
			return OK;
		}
	}

	if (curscr->_clear) {		/* force refresh ? */
		T(("clearing and updating curscr"));
		ClrUpdate(curscr);		/* yes, clear all & update */
		curscr->_clear = FALSE;	/* reset flag */
	} else {
		if (newscr->_clear) {
			T(("clearing and updating newscr"));
			ClrUpdate(newscr);
			newscr->_clear = FALSE;
		} else {
		        _nc_scroll_optimize();

			T(("Transforming lines"));
			for (i = 0; i < min(screen_lines, newscr->_maxy + 1); i++) {
				/*
				 * newscr->line[i].firstchar is normally set
				 * by wnoutrefresh.  curscr->line[i].firstchar
				 * is normally set by _nc_scroll_window in the
				 * vertical-movement optimization code,
				 */
				if (newscr->_line[i].firstchar != _NOCHANGE
				    || curscr->_line[i].firstchar != _NOCHANGE)
					TransformLine(i);
			}
		}
	}
	T(("marking screen as updated"));
	for (i = 0; i <= newscr->_maxy; i++) {
		newscr->_line[i].firstchar = _NOCHANGE;
		newscr->_line[i].lastchar = _NOCHANGE;
		newscr->_line[i].oldindex = i;
	}
	for (i = 0; i <= curscr->_maxy; i++) {
		curscr->_line[i].firstchar = _NOCHANGE;
		curscr->_line[i].lastchar = _NOCHANGE;
		curscr->_line[i].oldindex = i;
	}

	curscr->_curx = newscr->_curx;
	curscr->_cury = newscr->_cury;

	if (curscr->_attrs != A_NORMAL)
		vidattr(curscr->_attrs = A_NORMAL);

	GoTo(curscr->_cury, curscr->_curx);
	
	fflush(SP->_ofp);

	_nc_signal_handler(TRUE);

	return OK;
}

/*
**	ClrUpdate(scr)
**
**	Update by clearing and redrawing the entire screen.
**
*/

static void ClrUpdate(WINDOW *scr)
{
int	i = 0, j = 0;
int	lastNonBlank;

	T(("ClrUpdate(%p) called", scr));
	if (back_color_erase) {
		T(("back_color_erase, turning attributes off"));
		vidattr(A_NORMAL);
	}
	ClearScreen();

	if (scr != curscr) {
		for (i = 0; i < screen_lines ; i++)
			for (j = 0; j < screen_columns; j++)
				curscr->_line[i].text[j] = ' '; /* shouldn't this include the bkgd? */
	}

	T(("updating screen from scratch"));
	for (i = 0; i < min(screen_lines, scr->_maxy + 1); i++) {
		GoTo(i, 0);
		LRCORNER = FALSE;
		lastNonBlank = scr->_maxx;
		
		while (scr->_line[i].text[lastNonBlank] == BLANK && lastNonBlank > 0)
			lastNonBlank--;

		/* check if we are at the lr corner */
		if (i == screen_lines-1)
			if ((auto_right_margin) && !(eat_newline_glitch) &&
			    (lastNonBlank == screen_columns-1) && !(scr->_scroll)) 
			{
				T(("Lower-right corner needs special handling"));
			    LRCORNER = TRUE;
			}

		for (j = 0; j <= min(lastNonBlank, screen_columns); j++) {
			PutChar(scr->_line[i].text[j]);
		}
	}


	if (scr != curscr) {
		for (i = 0; i < screen_lines ; i++)
			for (j = 0; j < screen_columns; j++)
				curscr->_line[i].text[j] = scr->_line[i].text[j];
	}
}

/*
**	ClrToEOL()
**
**	Clear to EOL.  Deal with background color erase if terminal has this
**	glitch.  This code forces the current color and highlight to A_NORMAL
**	before emitting the erase sequence, then restores the current 
**	attribute.
*/

static void ClrToEOL(void)
{
int	j;

	if (back_color_erase) {
		TPUTS_TRACE("orig_pair");
		putp(orig_pair);
	}
	TPUTS_TRACE("clr_eol");
	putp(clr_eol);
	if (back_color_erase)
		vidattr(SP->_current_attr);

	for (j = SP->_curscol; j < screen_columns; j++)
	    curscr->_line[SP->_cursrow].text[j] = ' ';
}

static void ClrToBOL(void)
{
int j;

	if (back_color_erase) {
		TPUTS_TRACE("orig_pair");
		putp(orig_pair);
	}
	TPUTS_TRACE("clr_bol");
	putp(clr_bol);
	if (back_color_erase)
		vidattr(SP->_current_attr);

	for (j = 0; j <= SP->_curscol; j++)
	    curscr->_line[SP->_cursrow].text[j] = ' ';
}

/*
**	TransformLine(lineno)
**
**	Call either IDcTransformLine or NoIDcTransformLine to do the
**	update, depending upon availability of insert/delete character.
*/

static void TransformLine(int lineno)
{

	T(("TransformLine(%d) called",lineno));

	if ( (insert_character  ||  (enter_insert_mode  &&  exit_insert_mode))
		 &&  delete_character)
		IDcTransformLine(lineno);
	else
		NoIDcTransformLine(lineno);
}



/*
**	NoIDcTransformLine(lineno)
**
**	Transform the given line in curscr to the one in newscr, without
**	using Insert/Delete Character.
**
**		firstChar = position of first different character in line
**		lastChar = position of last different character in line
**
**		overwrite all characters between firstChar and lastChar.
**
*/

static void NoIDcTransformLine(int lineno)
{
int	firstChar, lastChar;
chtype	*newLine = newscr->_line[lineno].text;
chtype	*oldLine = curscr->_line[lineno].text;
int	k;
int	attrchanged = 0;
	
	T(("NoIDcTransformLine(%d) called", lineno));

	firstChar = 0;
	while (firstChar < screen_columns - 1 &&  newLine[firstChar] == oldLine[firstChar]) {
		if(ceol_standout_glitch) {
			if((newLine[firstChar] & (chtype)A_ATTRIBUTES) != (oldLine[firstChar] & (chtype)A_ATTRIBUTES))
			attrchanged = 1;
		}			
		firstChar++;
	}

	T(("first char at %d is %lx", firstChar, newLine[firstChar]));
	if (firstChar > screen_columns)
		return;

	if(ceol_standout_glitch && attrchanged) {
		firstChar = 0;
		lastChar = screen_columns - 1;
		GoTo(lineno, firstChar);
		if(clr_eol)
			ClrToEOL();
	} else {
		lastChar = screen_columns - 1;
		while (lastChar > firstChar  &&  newLine[lastChar] == oldLine[lastChar])
			lastChar--;
		GoTo(lineno, firstChar);
	}			

	/* check if we are at the lr corner */
	if (lineno == screen_lines-1)
		if ((auto_right_margin) && !(eat_newline_glitch) &&
		    (lastChar == screen_columns-1) && !(curscr->_scroll)) 
		{
			T(("Lower-right corner needs special handling"));
		    LRCORNER = TRUE;
		}

	T(("updating chars %d to %d", firstChar, lastChar));
	for (k = firstChar; k <= lastChar; k++) {
		PutChar(newLine[k]);
		oldLine[k] = newLine[k];
	}
}

/*
**	IDcTransformLine(lineno)
**
**	Transform the given line in curscr to the one in newscr, using
**	Insert/Delete Character.
**
**		firstChar = position of first different character in line
**		oLastChar = position of last different character in old line
**		nLastChar = position of last different character in new line
**
**		move to firstChar
**		overwrite chars up to min(oLastChar, nLastChar)
**		if oLastChar < nLastChar
**			insert newLine[oLastChar+1..nLastChar]
**		else
**			delete oLastChar - nLastChar spaces
*/

static void IDcTransformLine(int lineno)
{
int	firstChar, oLastChar, nLastChar;
chtype	*newLine = newscr->_line[lineno].text;
chtype	*oldLine = curscr->_line[lineno].text;
int	k, n;
int	attrchanged = 0;
	
	T(("IDcTransformLine(%d) called", lineno));

	if(ceol_standout_glitch && clr_eol) {
		firstChar = 0;
		while(firstChar < screen_columns) {
			if((newLine[firstChar] & (chtype)A_ATTRIBUTES) != (oldLine[firstChar] & (chtype)A_ATTRIBUTES))
				attrchanged = 1;
			firstChar++;			
		}
	}
	
	firstChar = 0;
	
	if (attrchanged) {
		GoTo(lineno, firstChar);
		ClrToEOL();
		for( k = 0 ; k <= (screen_columns-1) ; k++ )
			PutChar(newLine[k]);
	} else {
		while (firstChar < screen_columns  &&
				newLine[firstChar] == oldLine[firstChar])
			firstChar++;
		
		if (firstChar >= screen_columns)
			return;

		if (clr_bol)
		{
			int oFirstChar, nFirstChar;

			for (oFirstChar = 0; oFirstChar < screen_columns; oFirstChar++)
				if (oldLine[oFirstChar] != BLANK)
					break;
			for (nFirstChar = 0; nFirstChar < screen_columns; nFirstChar++)
				if (newLine[nFirstChar] != BLANK)
					break;

			if (nFirstChar > oFirstChar + strlen(clr_bol))
			{
			    GoTo(lineno, nFirstChar - 1);
			    ClrToBOL();

			    if (nFirstChar > firstChar)
				firstChar = nFirstChar;
			}
		}

		oLastChar = screen_columns - 1;
		while (oLastChar > firstChar  &&  oldLine[oLastChar] == BLANK)
			oLastChar--;
	
		nLastChar = screen_columns - 1;
		while (nLastChar > firstChar  &&  newLine[nLastChar] == BLANK)
			nLastChar--;

		if((nLastChar == firstChar) && clr_eol) {
			GoTo(lineno, firstChar);
			ClrToEOL();
			if(newLine[firstChar] != BLANK )
				PutChar(newLine[firstChar]);
		} else if( newLine[nLastChar] != oldLine[oLastChar] ) {
			n = max( nLastChar , oLastChar );

			GoTo(lineno, firstChar);

			for( k=firstChar ; k <= n ; k++ )
				PutChar(newLine[k]);
		} else {
			while (newLine[nLastChar] == oldLine[oLastChar]) {
				if (nLastChar != 0
				 && oLastChar != 0) {
					nLastChar--;
					oLastChar--;
				 } else {
					break;
				 }
			}
	
			n = min(oLastChar, nLastChar);

			GoTo(lineno, firstChar);
	
			for (k=firstChar; k <= n; k++)
				PutChar(newLine[k]);

			if (oLastChar < nLastChar)
				InsStr(&newLine[k], nLastChar - oLastChar);

			else if (oLastChar > nLastChar )
				DelChar(oLastChar - nLastChar);
		}
	}
	for (k = firstChar; k < screen_columns; k++)
		oldLine[k] = newLine[k];
}

/*
**	ClearScreen()
**
**	Clear the physical screen and put cursor at home
**
*/

static void ClearScreen()
{

	T(("ClearScreen() called"));

	if (clear_screen) {
		TPUTS_TRACE("clear_screen");
		putp(clear_screen);
		SP->_cursrow = SP->_curscol = 0;
#ifdef POSITION_DEBUG
		position_check(SP->_cursrow, SP->_curscol, "ClearScreen");
#endif /* POSITION_DEBUG */
	} else if (clr_eos) {
		SP->_cursrow = SP->_curscol = -1;
		GoTo(0,0);

		TPUTS_TRACE("clr_eos");
		putp(clr_eos);
	} else if (clr_eol) {
		SP->_cursrow = SP->_curscol = -1;

		while (SP->_cursrow < screen_lines) {
			GoTo(SP->_cursrow, 0);
			TPUTS_TRACE("clr_eol");
			putp(clr_eol);
		}
		GoTo(0,0);
	}
	T(("screen cleared"));
}


/*
**	InsStr(line, count)
**
**	Insert the count characters pointed to by line.
**
*/

static void InsStr(chtype *line, int count)
{
	T(("InsStr(%p,%d) called", line, count));

	if (enter_insert_mode  &&  exit_insert_mode) {
		TPUTS_TRACE("enter_insert_mode");
		putp(enter_insert_mode);
		while (count) {
			PutChar(*line);
			line++;
			count--;
		}
		TPUTS_TRACE("exit_insert_mode");
		putp(exit_insert_mode);
	} else if (parm_ich) {
		TPUTS_TRACE("parm_ich");
		tputs(tparm(parm_ich, count), count, _nc_outch);
		while (count) {
			PutChar(*line);
			line++;
			count--;
		}
	} else {
		while (count) {
			TPUTS_TRACE("insert_character");
			putp(insert_character);
			PutChar(*line);
			if (insert_padding)
			{
				TPUTS_TRACE("insert_padding");
				putp(insert_padding);
			}
			line++;
			count--;
		}
	}
}

/*
**	DelChar(count)
**
**	Delete count characters at current position
**
*/

static void DelChar(int count)
{
	T(("DelChar(%d) called", count));

	if (parm_dch) {
		TPUTS_TRACE("parm_dch");
		tputs(tparm(parm_dch, count), count, _nc_outch);
	} else {
		while (count--)
		{
			TPUTS_TRACE("delete_character");
			putp(delete_character);
		}
	}
}

/*
**	_nc_outstr(char *str)
**
**	Emit a string without waiting for update.
*/

void _nc_outstr(char *str)
{
    (void) fputs(str, stdout);
    (void) fflush(stdout);
}
