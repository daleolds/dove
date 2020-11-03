#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <duiwin.h>
#include <common.h>

Window *screen, *messageWindow, *timeWindow;
VBYTE attrib[ATTRIBCOUNT];
static unsigned originalRow, originalCol;
unsigned leftEdge;
Line *bufferEnd;
VCHAR vfill;

/*--------------------------------------------------------------------------
 * Initialize screen required memory, etc.
 */
int InitScreen(void)
{
	if (!InitWindows())
		return 0;
	memcpy(attrib, cfg.attrib, sizeof(attrib));
	vfill.c = ' ';
	GetCursor(&originalRow, &originalCol);
	if (!(screen = CreateWindow(0, 0, videoRows, videoCols, BORDERLESS)))
	{
		ExitScreen();
		return 0;
	}
	WSetAttribute(vfill.a = attrib[VIEWTEXT]);

	messageWindow = CreateWindow(screen->high - 1, 0, 1,
			screen->wide - TIMELEN, BORDERLESS|DESTRUCTIVE);
	WSetAttribute(attrib[MSGLINE]);
	timeWindow = CreateWindow(screen->high - 1, screen->wide - TIMELEN, 1,
			TIMELEN, BORDERLESS|DESTRUCTIVE);
	WSetAttribute(attrib[TIME]);
	if (vheadp->winp)
	{
		DeleteWindow(vheadp->winp);
// #error resize and clear all views
	}
	vheadp->winp = CreateWindow(0, 0, screen->high - 2, screen->wide,
			TILEDBORDER|BOTTOMTITLE|DESTRUCTIVE|EDGEBORDER);
	if (messageWindow == 0 || timeWindow == 0 || vheadp->winp == 0)
	{
		ExitScreen();
		return 0;
	}
	MLErase();
	return 1;
}

/*--------------------------------------------------------------------------
 * Free screen memory, etc.
 */
void ExitScreen(void)
{
	if (screen)
		DeleteWindow(screen);
	if (messageWindow)
		DeleteWindow(messageWindow);
	if (timeWindow)
		DeleteWindow(timeWindow);
	screen = messageWindow = timeWindow = 0;
	SetCursor(originalRow, originalCol);
	SetCursorType(CURSOR_SMALL);
	ExitWindows();
}

/*--------------------------------------------------------------------------
 * Return closest line offset of specified line and display column.
 */
unsigned DisplayOffset(Line *lp, unsigned goalCol)
{
	unsigned lo, col;
	for (col = lo = 0; col < goalCol && lo < lp->offsetofEOL(); lo++)
		if (lp->getc(lo) == '\t' && !(modes & MODE_SHOW_CTRL_CHARS))
			col = (col/tabSize + 1) * tabSize;
		else
			col++;
	return col > goalCol? lo - 1: lo;
}

/*--------------------------------------------------------------------------
 * Return display column of specified line and offset.
 */
unsigned DisplayCol(Line *lp, unsigned lo)
{
	unsigned i, col;
	if (modes & MODE_SHOW_CTRL_CHARS)
		return lo;
	for (col = i = 0; i < lo; i++)
		if (lp->getc(i) == '\t')                
			col = (col/tabSize + 1) * tabSize;
		else
			col++;
	return col;
}

/*--------------------------------------------------------------------------
 * Return the current display column of the cursor.
 */
unsigned CursorCol(void)
{
	return DisplayCol(curvp->position[DOT].line, curvp->position[DOT].offset);
}

/*--------------------------------------------------------------------------
 * ensures that leftcol is valid, returns its value.
 */
static unsigned AdjustLeftCol(View *vp)
{
	unsigned i = DisplayCol(vp->position[DOT].line, vp->position[DOT].offset);
	if (i >= vp->leftcol + vp->winp->wide - 1)
		vp->leftcol = i - vp->winp->wide + 
		  (vp->position[DOT].offset == vp->position[DOT].line->offsetofEOL()
		  		&& !(modes & MODE_SHOW_CTRL_CHARS)? 1: 2);
	else if (i < vp->leftcol)
		vp->leftcol = i;
	return vp->leftcol;
}

/*--------------------------------------------------------------------------
 * ensures that dotrow is valid, returns pointer to top Line in the view.
 */
static Line *AdjustDotRow(View *vp)
{
	Line *lp;
	unsigned i;
	if (vp->dotrow > vp->winp->high - 1)
		vp->dotrow = vp->winp->high - 1;
	for (i = vp->winp->high - vp->dotrow - 1, lp = vp->position[DOT].line; i; i--)
		if (lp != vp->bufp->linep)
			lp = lp->forw();
		else
		{
			vp->dotrow += i;
			break;
		}
	for (i = vp->dotrow, lp = vp->position[DOT].line; i; i--)
		if (lp->back() != vp->bufp->linep)
			lp = lp->back();
		else
		{
			vp->dotrow -= i;
			break;
		}
	return lp;
}

/*--------------------------------------------------------------------------
 */
static void DisplayTitle(View *vp, VBYTE attrib)
{
	int i;
	i = strlen(vp->bufp->fname);
	if (vp->bufp->flags & B_CHANGED && i < sizeof(vp->bufp->fname) - 2)
	{
		vp->bufp->fname[i] = '*';
		vp->bufp->fname[i + 1] = 0;
	}
	SelectWindow(vp->winp);
	WriteBorder(vp->bufp->fname, DOUBLE, attrib);
	vp->bufp->fname[i] = 0;
}

/*--------------------------------------------------------------------------
 */
static void DisplayLine(Window *wp, Line *lp, unsigned startMark,
		unsigned endMark)
{
	VCHAR vc;
	unsigned j, txto, col;
	unsigned txtlen = (modes & MODE_SHOW_CTRL_CHARS)? lp->length(): lp->offsetofEOL();
	const char *txt = lp->data();
	vc.a = attrib[VIEWTEXT];
	for (txto = col = 0; col < leftEdge && txto < txtlen; txt++, txto++)
	{
		if (txto == startMark)
			vc.a = attrib[VIEWMARK];
		if (txto == endMark)
			vc.a = attrib[VIEWTEXT];
		if (*txt == '\t' && !(modes & MODE_SHOW_CTRL_CHARS) && tabSize > 1)
		{
			vc.c = ' ';
			col += tabSize - col % tabSize;
			if (col > leftEdge)
				for (j = 0; j < col - leftEdge; j++)
					windowTempLine[j] = vc;
		}
		else
			col++;
	}
	col = (col <= leftEdge? 0: col - leftEdge);
	for ( ; txto < txtlen && col < wp->wide; txt++, txto++)
	{
		if (txto == startMark)
			vc.a = attrib[VIEWMARK];
		if (txto == endMark)
			vc.a = attrib[VIEWTEXT];
		if (*txt == '\t' && !(modes & MODE_SHOW_CTRL_CHARS))
		{
			vc.c = ' ';
			j = col + tabSize - (col + leftEdge) % tabSize;
			if (j > wp->wide)
				j = wp->wide;
			while (col < j)
				windowTempLine[col++] = vc;
		}
		else
		{
			if (!(modes & MODE_SHOW_CTRL_CHARS)
					&& (*txt == '\r' || *txt == '\n')
					&& (txto == txtlen - 1 || txto == txtlen - 2))
				vc.c = ' ';
			else
				vc.c = *txt;
			windowTempLine[col++] = vc;
		}
	}
	if (col >= wp->wide)
	{
		vc.c = '\xAF';
		vc.a = attrib[VIEWLONGLINE];
		windowTempLine[col - 1] = vc;
	}
	else
	{
		if (!(modes & MODE_SHOW_CTRL_CHARS))
		{
			if (txto == startMark)
				vc.a = attrib[VIEWMARK];
			if (txto == endMark)
				vc.a = attrib[VIEWTEXT];
			vc.c = ' ';
			windowTempLine[col++] = vc;
		}
		while (col < wp->wide)
			windowTempLine[col++] = vfill;
	}
	_WriteTempLine(col);
	wp->curRow++;
}

/*--------------------------------------------------------------------------
 */
static void ClearLines(Window *wp, unsigned count)
{
	unsigned col;
	for (col = 0; col < wp->wide; col++)
		windowTempLine[col] = vfill;
	while (count-- > 0)
	{
		_WriteTempLine(col);
		wp->curRow++;
	}
}

/*--------------------------------------------------------------------------
 */
void ShowCursor(unsigned row, unsigned col, VBYTE attr)
{
	FakeCursor((modes & MODE_TTY_CURSOR)? CF_FAKE_AND_TTY: CF_FAKE_ONLY, attr);
	SetCursorType((modes & MODE_INSERT)? CURSOR_SMALL: CURSOR_BIG);
	SetCursor(row, col);
}

/*--------------------------------------------------------------------------
 */
void DisplayViews(void)
{
	Region r = {{0, 0}, 0, 0};
	Position endBlock = {0, 0};
	Line *lp;
	View *vp;
	Window *wp;
	unsigned i, inBlock;

	for (vp = vheadp; vp; vp = vp->next)
	{
		bufferEnd = vp->bufp->linep;
		leftEdge = AdjustLeftCol(vp);
		lp = AdjustDotRow(vp);
		wp = vp->winp;
		if (vp == curvp)
		{
			bool dotBlock = false;
			if (GetRegion(&r, 1) == 0)
			{
				if (r.size == 0)
					inBlock = 2;
				else
				{
					inBlock = 0;
					endBlock = vp->position[DOT];
					dotBlock = endBlock.line->getc(endBlock.offset) == '\t'
							&& !(modes & MODE_SHOW_CTRL_CHARS) && tabSize > 1;
					endBlock.offset++;
				}
			}
			else if (r.start.line == vp->position[DOT].line
			  && r.start.offset == vp->position[DOT].offset)
			{
				dotBlock = true;
				inBlock = 0;
				endBlock = vp->position[MARK];
			}
			else
			{
				inBlock = (r.newlines > vp->dotrow? 1: 0);
				endBlock = vp->position[DOT];
			}
			ShowCursor(wp->row + vp->dotrow, wp->col+CursorCol()-vp->leftcol,
					attrib[dotBlock? VIEWMARKCURSOR: VIEWTEXTCURSOR]);
		}
		else
			inBlock = 2;
		SelectWindow(wp);
		WSetCursor(0, 0);
		i = 0;
		do
		{
			switch (inBlock)
			{
				case 0:
					if (lp == r.start.line)
					{
						if (lp == endBlock.line)
						{
							DisplayLine(wp, lp, r.start.offset, endBlock.offset);
							inBlock = 2;
						}
						else
						{
							DisplayLine(wp, lp, r.start.offset, UINT_MAX);
							inBlock = 1;
						}
					}
					else
						DisplayLine(wp, lp, UINT_MAX, UINT_MAX);
					break;
				case 1:
					if (lp == endBlock.line)
					{
						DisplayLine(wp, lp, 0, endBlock.offset);
						inBlock = 2;
					}
					else
						DisplayLine(wp, lp, 0, UINT_MAX);
					break;
				case 2:
					DisplayLine(wp, lp, UINT_MAX, UINT_MAX);
			}
			i++;
			if (lp != bufferEnd)
				lp = lp->forw();
			else
			{	/* fast forward to the end */
				ClearLines(wp, wp->high - i);
				break;
			}
		} while (i < wp->high);
		DisplayTitle(vp, attrib[VIEWBORDER]);
	}
}

/*==========================================================================*/
