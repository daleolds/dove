#include <ctype.h>
#include <pvideo.h>
#include <duiwin.h>
#include <common.h>

//---------------------------------------------------------------------------
static void ClearTempMark()
{
	if (!(curvp->positionFlags & PF_TEMP_MARK))
		return;
	curvp->positionFlags &= ~PF_TEMP_MARK;
	curvp->position[MARK].line = 0;
}

//---------------------------------------------------------------------------
static void SetTempMark()
{
	if (curvp->position[MARK].line)
		return;
	curvp->positionFlags |= PF_TEMP_MARK;
	curvp->position[MARK] = curvp->position[DOT];
}

/*--------------------------------------------------------------------------
 * Move the cursor to the beginning of the current line. No errors.
 */
int GoToBOLKey(void)
{
	ClearTempMark();
	curvp->position[DOT].offset = 0;
	return 1;
}

int MarkToBOLKey(void)
{
	SetTempMark();
	curvp->position[DOT].offset = 0;
	return 1;
}

/*--------------------------------------------------------------------------
 * Move the cursor backwards by n characters. Error if you try and move
 * out of the buffer.
 */
static int ComBackChar(int n)
{
	Line *lp;
	while (n--)
		if (curvp->position[DOT].offset == 0)
		{
			lp = curvp->position[DOT].line->back();
			if (lp == curbp->linep)
				return 0;
			if (curvp->dotrow > 0)
				curvp->dotrow--;
			curvp->position[DOT].line = lp;
			curvp->position[DOT].offset = lp->offsetofEOL();
		}
		else
			curvp->position[DOT].offset--;
	return 1;
}

int BackCharKey(int n) { ClearTempMark(); return ComBackChar(n); }
int MarkBackCharKey(int n) { SetTempMark(); return ComBackChar(n); }

/*--------------------------------------------------------------------------
 * Move the cursor to the end of the current line. No errors.
 */
int GoToEOLKey(void)
{
	ClearTempMark();
	curvp->position[DOT].offset = curvp->position[DOT].line->offsetofEOL();
	return 1;
}

int MarkToEOLKey(void)
{
	SetTempMark();
	curvp->position[DOT].offset = curvp->position[DOT].line->offsetofEOL();
	return 1;
}

/*--------------------------------------------------------------------------
 * Move the cursor forwards by n characters. Error if you try and move off
 * the end of the buffer.
 */

static int ComForwCharKey(int n)
{
	Line *lp;
	while (n--)
	{
		lp = curvp->position[DOT].line;
		if (curvp->position[DOT].offset >= lp->offsetofEOL())
		{
			if (lp == curbp->linep)
				return 0;
			if (curvp->dotrow < curvp->winp->high - 1)
				curvp->dotrow++;
			curvp->position[DOT].line = lp->forw();
			curvp->position[DOT].offset = 0;
		}
		else
			curvp->position[DOT].offset++;
	}
	return 1;
}
int ForwCharKey(int n) { ClearTempMark(); return ComForwCharKey(n); }
int MarkForwCharKey(int n) { SetTempMark(); return ComForwCharKey(n); }

/*--------------------------------------------------------------------------
 * Goto the beginning of the buffer.
 */
int GoToBOBKey(void)
{
	ClearTempMark(); 
	curvp->position[DOT].line = curbp->linep->forw();
	curvp->position[DOT].offset = curvp->dotrow = 0;
	return 1;
}
int MarkToBOBKey(void)
{
	SetTempMark(); 
	curvp->position[DOT].line = curbp->linep->forw();
	curvp->position[DOT].offset = curvp->dotrow = 0;
	return 1;
}

/*--------------------------------------------------------------------------
 * Put the cursor at the end of the file.
 */
int GoToEOBKey(void)
{
	ClearTempMark(); 
	curvp->position[DOT].line = curbp->linep;
	curvp->position[DOT].offset = curbp->linep->offsetofEOL();
	curvp->dotrow = curvp->winp->high;
	return 1;
}

int MarkToEOBKey(void)
{
	SetTempMark(); 
	curvp->position[DOT].line = curbp->linep;
	curvp->position[DOT].offset = curbp->linep->offsetofEOL();
	curvp->dotrow = curvp->winp->high;
	return 1;
}

/*--------------------------------------------------------------------------
 * Move to the upper line of the view.
 */
int GoToBOVKey(void) { return BackLineKey(curvp->dotrow); }
int MarkToBOVKey(void) { return MarkBackLineKey(curvp->dotrow); }

/*--------------------------------------------------------------------------
 * Move to the lower line of the view.
 */
int GoToEOVKey(void)
{
	return ForwLineKey(curvp->winp->high - curvp->dotrow - 1);
}

int MarkToEOVKey(void)
{
	return MarkForwLineKey(curvp->winp->high - curvp->dotrow - 1);
}

/*--------------------------------------------------------------------------
 * This routine, given a pointer to a Line, and the current cursor goal
 * column, sets the best choice for the cursor offset. 1 is returned.
 */
static int SetCol(Line *lp)
{
	static unsigned curgoal = 0;			/* Goal column for up, down */
	if (curbp->lastSeqCmd != SEQ_CURSOR_COL)
		curgoal = CursorCol();
	curvp->position[DOT].offset = DisplayOffset(lp, curgoal);
	curvp->position[DOT].line = lp;
	curbp->thisSeqCmd = SEQ_CURSOR_COL;
	return 1;
}

/*--------------------------------------------------------------------------
 * Move forward by full lines. The last command controls how the goal
 * column is set. No errors are possible.
 */
static int ComForwLineKey(int n)
{
	Line *lp = curvp->position[DOT].line;
	for (; n-- && lp != curbp->linep; lp = lp->forw())
		if (curvp->dotrow < curvp->winp->high - 1)
			curvp->dotrow++;
	return SetCol(lp);
}

int ForwLineKey(int n) { ClearTempMark(); return ComForwLineKey(n); }
int MarkForwLineKey(int n) { SetTempMark(); return ComForwLineKey(n); }

/*--------------------------------------------------------------------------
 * This function is like "ForwLineKey", but
 * goes backwards. The scheme is exactly the same.
 */
static int ComBackLineKey(int n)
{
	Line *lp = curvp->position[DOT].line;
	for (; n-- && lp->back() != curbp->linep; lp = lp->back())
		if (curvp->dotrow > 0)
			curvp->dotrow--;
	return SetCol(lp);
}
int BackLineKey(int n) { ClearTempMark(); return ComBackLineKey(n); }
int MarkBackLineKey(int n) { SetTempMark(); return ComBackLineKey(n); }

/*--------------------------------------------------------------------------
 * Scroll forward by a specified number of pages.
 */
int ForwPageKey(int n) { return ScrollDownKey(n * curvp->winp->high); }
int MarkForwPageKey(int n) { return MarkScrollDownKey(n * curvp->winp->high); }

/*--------------------------------------------------------------------------
 * This command is like "ForwPageKey", but it goes backwards.
 */
int BackPageKey(int n) { return ScrollUpKey(n * curvp->winp->high); }
int MarkBackPageKey(int n) { return MarkScrollUpKey(n * curvp->winp->high); }

/*--------------------------------------------------------------------------
 * Return true if the character under the cursor is considered to be
 * part of a word.
 */
static bool inword(void)
{
	unsigned o = curvp->position[DOT].offset;
	Line *lp = curvp->position[DOT].line;
	if (o >= lp->length())
	{
		assert(o == lp->length() && lp == curbp->linep);
		return false;
	}
	char c = lp->getc(o);
	return isalnum(c) || c == '_';
}

/*--------------------------------------------------------------------------
 * Move the cursor backward by "n" words. All of the details of motion
 * are performed by the "BackCharKey" and "ForwCharKey" routines.
 * Error if you try to move beyond the buffer.
 */
int BackWordKey(int n)
{
	if (!BackCharKey(1))
		return 0;
	ClearTempMark();
	while (n--)
		if (inword())
			while (inword())
			{
				if (!BackCharKey(1))
					return 0;
			}
		else
			while (!inword())
				if (!BackCharKey(1))
					return 0;
	return ForwCharKey(1);
}

int MarkBackWordKey(int n)
{
	if (!MarkBackCharKey(1))
		return 0;
	SetTempMark();
	while (n--)
		if (inword())
			while (inword())
			{
				if (!MarkBackCharKey(1))
					return 0;
			}
		else
			while (!inword())
				if (!MarkBackCharKey(1))
					return 0;
	return MarkForwCharKey(1);
}

/*--------------------------------------------------------------------------
 * Move the cursor forward by the specified number of words. All of the
 * motion is done by "ForwCharKey". Error if you try and move beyond
 * the buffer's end.
 */
int ForwWordKey(int n)
{
	ClearTempMark();
	while (n--)
		if (inword())
			while (inword())
			{
				if (!ForwCharKey(1))
					return 0;
			}
		else
			while (!inword())
				if (!ForwCharKey(1))
					return 0;
	return 1;
}

int MarkForwWordKey(int n)
{
	SetTempMark();
	while (n--)
		if (inword())
			while (inword())
			{
				if (!MarkForwCharKey(1))
					return 0;
			}
		else
			while (!inword())
				if (!MarkForwCharKey(1))
					return 0;
	return 1;
}

/*--------------------------------------------------------------------------
 * This command moves the current window down by n lines.
 */
static int ComScrollDownKey(int n)
{
	Line *lp = curvp->position[DOT].line;
	while (n-- && lp != curbp->linep)
		lp = lp->forw();
	return SetCol(lp);
}
int ScrollDownKey(int n) { ClearTempMark(); return ComScrollDownKey(n); }
int MarkScrollDownKey(int n) { SetTempMark(); return ComScrollDownKey(n); }

/*--------------------------------------------------------------------------
 * Move the current window up by n lines. 
 */
static int ComScrollUpKey(int n)
{
	Line *lp = curvp->position[DOT].line;
	while (n-- && lp->back() != curbp->linep)
		lp = lp->back();
	return SetCol(lp);
}
int ScrollUpKey(int n) { ClearTempMark(); return ComScrollUpKey(n); }
int MarkScrollUpKey(int n) { SetTempMark(); return ComScrollUpKey(n); }

/*--------------------------------------------------------------------------
 * Hold another column value across horizontal scroll functions so we can
 * scroll smoothly through tabs.
 */
static unsigned horizScrollCol = 0;
static void InitHorizScrollCol(void)
{
	if (curbp->lastSeqCmd != SEQ_HSCROLL)
		horizScrollCol = CursorCol();
	curbp->thisSeqCmd = SEQ_HSCROLL;
}
	
/*--------------------------------------------------------------------------
 * Move the current window left by n cols.
 */
int ScrollLeftKey(int n)
{
	InitHorizScrollCol();
	Position &dot(curvp->position[DOT]);
	while (n-- && dot.offset > 0)
	{
		if (curvp->leftcol > 0)
			curvp->leftcol--;
		horizScrollCol--;
		dot.offset--;
		if (CursorCol() < horizScrollCol)
			dot.offset++; // decrementing thru a TAB
	}
	return 1;
}

/*--------------------------------------------------------------------------
 * Move the current window right by "n" cols.
 */
int ScrollRightKey(int n)
{
	InitHorizScrollCol();
	Position &dot(curvp->position[DOT]);
	while (n-- && dot.offset < dot.line->offsetofEOL())
	{
		curvp->leftcol++;
		horizScrollCol++;
		dot.offset++;
		if (CursorCol() > horizScrollCol)
			dot.offset--; // incrementing thru a TAB
	}
	return 1;
}

/*======================================================================*/
