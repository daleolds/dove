#include <pvideo.h>
#include <duiwin.h>
#include <common.h>

static bool lastWasSpace = false;

/*--------------------------------------------------------------------------
 * Routine to check for what is considered
 * white space for indent purposes.
 */
static int IsIndentWhite(char c)
{
	return c == ' ' || c == '\t';
}

/*--------------------------------------------------------------------------
 * Figure out the indentation of the
 * current line.
 */
static size_t IndentationChars(void)
{
	size_t i = 0;
	Line &lp = *curvp->position[DOT].line;
	while (i < lp.length() && IsIndentWhite(lp.getc(i)))
		++i;
	return i;
}

/*--------------------------------------------------------------------------
 * Sets the cursor to where the line should
 * be split for word wrap, returns offset
 * from previous cursor position.
 */
static size_t GoToWordBreak(void)
{
	Line *dotp = curvp->position[DOT].line;
	size_t doto = curvp->position[DOT].offset, bdoto = doto;
	size_t fill = IndentationChars();
	while (DisplayCol(dotp, bdoto) > wordWrapCol)
		bdoto--;
	while (bdoto < doto && IsIndentWhite(dotp->getc(bdoto)))
		bdoto++;
	while (bdoto > fill && !IsIndentWhite(dotp->getc(bdoto)))
		bdoto--;
	if (bdoto == fill || bdoto == doto)
		return 0;
	curvp->position[DOT].offset = ++bdoto;
	return doto - bdoto;
}

/*--------------------------------------------------------------------------
 * Strips any white space from start of
 * line or the first non-white character
 * before DOT the first non-white character
 * under or after DOT or end of line.
 */
static void EatWhiteSpace(void)
{
	Line *dotp = curvp->position[DOT].line;
	unsigned fdoto = curvp->position[DOT].offset, bdoto = fdoto;
	while (bdoto > 0 && IsIndentWhite(dotp->getc(bdoto - 1)))
		bdoto--;
	while (fdoto < dotp->length() && IsIndentWhite(dotp->getc(fdoto)))
		fdoto++;
	curvp->position[DOT].offset = bdoto;
	DeleteChars(fdoto - bdoto);
}

/*--------------------------------------------------------------------------
 * Insert a newline, if in indent mode then add enough tabs and spaces to
 * duplicate the indentation of the previous line. Figure out the
 * indentation of the current line. Insert a newline by calling the
 * standard routine. Insert the indentation by inserting the right number
 * of tabs and spaces.
 */
static int IndentLine(void)
{
	static char newLineChars[] = {'\r', '\n'};
	unsigned fillCol, i;
	char space = ' ', tab = '\t', *nlChars = newLineChars;
	int nlCount  = sizeof newLineChars;
	if (!(curbp->flags & B_CR_LF))
	{
		nlChars++;
		nlCount--;
	}
	if (!(modes & MODE_INDENT))
		return InsertChars(nlCount, nlChars);
	fillCol = DisplayCol(curvp->position[DOT].line, IndentationChars());
	EatWhiteSpace();
	if (!InsertChars(nlCount, nlChars))
		return 0;
	if (modes & MODE_REALTABS)
	{
		for (i = fillCol/tabSize; i > 0; i--)
			if (!InsertChars(sizeof tab, &tab))
				return 0;
		i = fillCol % tabSize;
	}
	else
		i = fillCol;
	while (i--)
		if (!InsertChars(sizeof space, &space))
			return 0;
	return 1;
}

/*--------------------------------------------------------------------------
 * Used when word wrap is on to type a
 * space (instead of \r\n, \t or space) and
 * check for word wrap column exceeded.
 * Also inhibits more than one space in a  
 * row.
 */
static int TypeSpace(void)
{
	unsigned offsetFromWrap;
	char space = ' ';
	if (!lastWasSpace)
	{
		if (!InsertChars(sizeof space, &space))
			return 0;
		lastWasSpace = true;
		if (CursorCol() >= wordWrapCol)
		{
			offsetFromWrap = GoToWordBreak();
			if (!IndentLine())
				return 0;
			curvp->position[DOT].offset += offsetFromWrap;
		}
	}
	return 1;
}

/*--------------------------------------------------------------------------
 * Simple enter key.
 */
int EnterKey(int n)
{
	while (n--)
		if (!IndentLine())
			return 0;
	return 1;
} 

/*--------------------------------------------------------------------------
 * Set fill column to n for word wrap
 * functions. 
 */
int WordWrapModeKey(int n)
{
	wordWrapCol = (n != 1? n: CursorCol());
	if (wordWrapCol == 0)
		MLWrite("Word wrap is off");
	else
		MLWrite("Word wrap after column %d", wordWrapCol);
	return 1;
}

/*--------------------------------------------------------------------------
 * Set/clear/toggle a mode bit. Return state of mode as 1 or 0
 */
int ChangeMode(int n, int modeBit)
{
	int mode = (n == 1? (!(modes & modeBit)): n);
	if (mode)
		modes |= modeBit;
	else
		modes &= ~modeBit;
	return(mode == 0? 0 : 1);
}

/*--------------------------------------------------------------------------
 * Set/clear/toggle overwrite mode.
 */
int InsertModeKey(int n)
{
	ChangeMode(n, MODE_INSERT);
	return 1;
}

/*--------------------------------------------------------------------------
 * Toggle/set/clear indent mode.
 */
int IndentModeKey(int n)
{
	DisplayMode("Indent", ChangeMode(n, MODE_INDENT));
	return 1;
}

/*--------------------------------------------------------------------------
 * Toggle/set/clear show control chars mode.
 */
int CtrlCharsModeKey(int n)
{
	DisplayMode("Show Control Chars", ChangeMode(n, MODE_SHOW_CTRL_CHARS));
	return 1;
}

/*--------------------------------------------------------------------------
 * Toggle/set/clear CR-LF mode.
 */
int CRLFModeKey(int n)
{
	DisplayMode("Enter CR-LF", ChangeMode(n, MODE_CR_LF));
	return 1;
}

/*--------------------------------------------------------------------------
 * Set tab size, an argument of 0 toggles
 * real tabs/use spaces. 
 */
int TabModeKey(int n)
{
	if (n != 0)
		MLWrite("Tab size set to %d.", tabSize = n);
	else
		DisplayMode("Real Tabs", ChangeMode(1, MODE_REALTABS));
	return 1;
}

/*--------------------------------------------------------------------------
 * Puts tabs into the file.
 */
int TabKey(int n)
{
	unsigned fill;
	char tabChar;
	if (n > 0)
	{
		fill = (modes & MODE_REALTABS)? n: n * tabSize - CursorCol() % tabSize;
		tabChar = (modes & MODE_REALTABS)? '\t': ' ';
		while (fill-- > 0)
			if (!InsertChars(sizeof tabChar, &tabChar))
				return 0;
	}
	return 1;
}

/*--------------------------------------------------------------------------
 * This routine will insert "len" characters from "block" as if they
 * had been typed.
 */
int TypeChars(size_t len, const char *block)
{
	lastWasSpace = false;
	for (const char *bp = block; bp - block < len; )
		if (bp - block + 1 < len && bp[0] == '\r' && bp[1] == '\n')
		{
			if (wordWrapCol > 0)
			{
				if (!TypeSpace())
					return 0;
			}
			else if (!IndentLine())
				return 0;
			bp += 2;
		}
		else
		{
			if (wordWrapCol > 0  && IsIndentWhite(*bp))
			{
				if (!TypeSpace())
					return 0;
			}
			else
			{
				if (!InsertChars(sizeof *bp, bp))
					return 0;
				lastWasSpace = false;
			}
			bp++;
		}
	return 1;
}

/*======================================================================*/
