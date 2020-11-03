#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <pvideo.h>
#include <duiwin.h>

Window *selectedWindow;
VCHAR *windowTempLine;

static unsigned realRow, realCol, realHigh, realWide;

/*-----------------------------------------------------
 */
static void AdjustRealSizes(void)
{
	if (realRow > 0)
	{
		realRow--;
		realHigh++;
	}
	if (realCol > 0)
	{
		realCol--;
		realWide++;
	}
	if (realRow + realHigh < videoRows)
		realHigh++;
	if (realCol + realWide < videoCols)
		realWide++;
}

/*-----------------------------------------------------
 */
int InitWindows(void)
{
	if (!InitVideo())
		return false;
	if (!(windowTempLine = (VCHAR *)malloc(sizeof(VCHAR) * videoCols)))
	{
		ExitVideo();
		return false;
	}
	return true;
}

void ExitWindows(void)
{
	free(windowTempLine);
	windowTempLine = 0;
	ExitVideo();
}

/*-----------------------------------------------------
 * make a new window
 */
Window *CreateWindow(unsigned row, unsigned col, unsigned high, unsigned wide,
		unsigned flags)
{
	unsigned saveSize, edgeOffset, maxHigh, maxWide;
	Window *wnd;

	if (flags & (BORDERLESS|EDGEBORDER))
	{
		edgeOffset = 0;
		maxHigh = videoRows;
		maxWide = videoCols;
	}
	else
	{
		edgeOffset = 1;
		maxHigh = videoRows - 2;
		maxWide = videoCols - 2;
	}
	if (high > maxHigh)
		high = maxHigh;
	if (wide > maxWide)
		wide = maxWide;
	if (row + high > videoRows - edgeOffset)
		row = videoRows - high - edgeOffset;
	else if (row < edgeOffset)
		row = edgeOffset;
	if (col + wide > videoCols - edgeOffset)
		col = videoCols - wide - edgeOffset;
	else if (col < edgeOffset)
		col = edgeOffset;
	realRow = row;
	realCol = col;
	realHigh = high;
	realWide = wide;
	if (!(flags & BORDERLESS))
		AdjustRealSizes();
	saveSize = ((flags & DESTRUCTIVE)? 0:
	  realWide * realHigh * sizeof(wnd->save[0]));
	if ((wnd = (Window *)malloc(sizeof(*wnd) + saveSize - sizeof(wnd->save))) == 0)
		return 0;
	if (saveSize > 0)
		VideoGetZone(realRow, realCol, realHigh, realWide, wnd->save);
	wnd->flags = flags;
	wnd->high = high;
	wnd->wide = wide;
	wnd->col = col;
	wnd->row = row;
	wnd->curRow = wnd->curCol = 0;
	wnd->fillChar.c = ' ';
	wnd->fillChar.a = MakeAttrib(VWHITE, VBLACK);
	return (selectedWindow = wnd);
}

/*-----------------------------------------------------
 * delete a window
 */
void DeleteWindow(Window *wnd)
{
	if (wnd == 0)
		wnd = selectedWindow;
	selectedWindow = 0;
	if (!(wnd->flags & DESTRUCTIVE))
	{
		realRow = wnd->row;
		realCol = wnd->col;
		realHigh = wnd->high;
		realWide = wnd->wide;
		if (!(wnd->flags & BORDERLESS))
			AdjustRealSizes();
		VideoPutZone(realRow, realCol, realHigh, realWide, wnd->save);
	}
	free(wnd);
}

/*-----------------------------------------------------------------------------
 */
static void WriteTopBottom(VCHAR vc, unsigned leftCorner, unsigned rightCorner,
		const char *title, int top)
{
	Window *wnd = selectedWindow;
	unsigned i;
	for (i = 0; i < realWide; i++)
		windowTempLine[i] = vc;
	if (wnd->col > 0)
	{
		vc.c = leftCorner;
		windowTempLine[0] = vc;
	}
	if (wnd->col + wnd->wide < videoCols)
	{
		vc.c = rightCorner;
		if (realWide)
			windowTempLine[realWide - 1] = vc;
	}
	if (title != 0 && *title != 0)
	{
    assert(wnd->wide >= 2);
    if ((i = strlen(title) + 2) > wnd->wide)
      for (title += i - wnd->wide - 2, i = 0; *title != '\0'; ++i, ++title)
  			windowTempLine[i].c = *title;
    else
    {
      i = (wnd->wide - i) / 2;
  		windowTempLine[i++].c = ' ';
  		while (*title != '\0')
  			windowTempLine[i++].c = *title++;
  		windowTempLine[i].c = ' ';
    }
	}
	VideoPutZone(top || !realHigh? realRow: realRow + realHigh - 1,
			realCol, 1, realWide, windowTempLine);
}

/*-----------------------------------------------------
 * Write the border and title on a window
 */
void WriteBorder(const char *title, enum BORDERTYPE btype, VBYTE attrib)
{
	Window *wnd = selectedWindow;
	unsigned i;
	VCHAR vc;
	static struct { VBYTE ul, ur, lr, ll, side, line;} borderChar[] =
	{
		{218,191,217,192,179,196},
		{201,187,188,200,186,205},
		{214,183,189,211,186,196},
		{213,184,190,212,179,205},
		{194,194,217,192,179,196},
		{' ',' ',' ',' ',' ',' '}
	};
	
	if (wnd->flags & BORDERLESS)
		return;
	realRow = wnd->row;
	realCol = wnd->col;
	realHigh = wnd->high;
	realWide = wnd->wide;
	AdjustRealSizes();
	if (btype == CLEARBORDER)
		memset(&borderChar[btype], wnd->fillChar.c, sizeof(borderChar[btype]));
	vc.a = attrib;
	vc.c = borderChar[btype].line;

	if (wnd->flags & TILEDBORDER)
		i = (wnd->flags & BOTTOMTITLE) ? 0: 2;
	else
		i = 1;

	/* top */
	if (wnd->row > 0 && i > 0)
		WriteTopBottom(vc, borderChar[btype].ul, borderChar[btype].ur,
				((wnd->flags & BOTTOMTITLE)? 0: title), 1);

	/* bottom */
	if (wnd->row + wnd->high < videoRows && i < 2)
		WriteTopBottom(vc, borderChar[btype].ll, borderChar[btype].lr,
				((wnd->flags & BOTTOMTITLE)? title: 0), 0);

	/* sides */
	vc.c = borderChar[btype].side;
	if (wnd->col > 0 && !(wnd->flags & TILEDBORDER))
		for (i = 0; i < wnd->high; i++)
			VideoPutZone(wnd->row + i, realCol, 1, 1, &vc);
	if (wnd->col + wnd->wide < videoCols)
		for (i = 0; i < wnd->high; i++)
			VideoPutZone(wnd->row + i, wnd->col + wnd->wide, 1, 1, &vc);
}

/*-----------------------------------------------------
 * Write a character to a window
 */
void WriteChar(char c)
{
	if (selectedWindow->curCol >= selectedWindow->wide)
		return;
	windowTempLine[0].a = selectedWindow->fillChar.a;
	windowTempLine[0].c = c;
	_WriteTempLine(1);
	selectedWindow->curCol++;
}

/*-----------------------------------------------------
 *  Clear the selected window
 */
void ClearWindow(void)
{
	unsigned i;
	VCHAR vc;
	vc = selectedWindow->fillChar;
	for (i = 0; i < selectedWindow->wide; i++)
		windowTempLine[i] = vc;
	selectedWindow->curCol = 0;
	for (i = 0; i < selectedWindow->high; i++)
	{
		selectedWindow->curRow = i;
		_WriteTempLine(selectedWindow->wide);
	}
	selectedWindow->curRow = 0;
}

/*-----------------------------------------------------
 * Write data to the selected window
 */
void WriteData(size_t len, const char *data)
{
	unsigned i, maxLen;
	VCHAR vc;
	if ((maxLen = (int)selectedWindow->wide - selectedWindow->curCol) < 1)
		return;
	vc.a = selectedWindow->fillChar.a;
	if (len > maxLen)
		len = maxLen;
	for (i = 0; i < len; i++, data++)
	{
		vc.c = *data;
		windowTempLine[i] = vc;
	}
	_WriteTempLine(i);
	selectedWindow->curCol += i;
}

/*-----------------------------------------------------
 *  Clear to End of Line
 */
void ClearEOL(void)
{
	unsigned i, maxLen;
	VCHAR vc;
	if ((maxLen = (int)selectedWindow->wide - selectedWindow->curCol) < 1)
		return;
	vc = selectedWindow->fillChar;
	for (i = 0; i < maxLen; i++)
		windowTempLine[i] = vc;
	_WriteTempLine(i);
}

/*-----------------------------------------------------
 * Write a line to a window
 */
void WriteLine(const char *s)
{
	unsigned i, maxLen;
	VCHAR vc;
	if ((maxLen = selectedWindow->wide - selectedWindow->curCol) < 1)
		return;
	vc.a = selectedWindow->fillChar.a;
	for (i = 0; *s != 0 && i < maxLen; i++, s++)
	{
		vc.c = *s;
		windowTempLine[i] = vc;
	}
	vc.c = selectedWindow->fillChar.c;
	while (i < maxLen)
		windowTempLine[i++] = vc;
	_WriteTempLine(i);
	selectedWindow->curCol = 0;
	selectedWindow->curRow++;
}

/*-----------------------------------------------------
 * Write a string to a window
 */
void WriteString(const char *s)
{
	unsigned i, maxLen;
	VCHAR vc;
	if ((maxLen = (int)selectedWindow->wide - selectedWindow->curCol) < 1)
		return;
	vc.a = selectedWindow->fillChar.a;
	for (i = 0; *s != 0 && i < maxLen; i++, s++)
	{
		vc.c = *s;
		windowTempLine[i] = vc;
	}
	_WriteTempLine(i);
	selectedWindow->curCol += i;
}

/*-----------------------------------------------------
 * Write contents of windowTempLine to the selected window
 */
void _WriteTempLine(size_t len)
{
	Window *wnd = selectedWindow;
	VideoPutZone(wnd->row + wnd->curRow, wnd->col + wnd->curCol, 1, len,
			windowTempLine);
}

/*===========================================================================*/

