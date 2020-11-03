#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

#include <pvideo.h>
#include <duiwin.h>
#include <common.h>

#if defined(WC10_5_WIN32)
#define _IS_SP _SPACE
#define _IS_DIG _DIGIT
#define _IS_UPP _UPPER
#define _IS_LOW _LOWER
#define _IS_HEX _XDIGT
#define _IS_CTL _CNTRL
#define _IS_PUN _PUNCT
#define  _ctype _IsTable
#endif

/*--------------------------------------------------------------------------
 * Display available memory.
 */
int MemoryLeftKey(void)
{
	MLWrite("Available memory: unknown");
	return 1;
}

/*--------------------------------------------------------------------------
 * Display the current position of the cursor,
 * in origin 1 X-Y coordinates, the character that is
 * under the cursor, and the fraction of the
 * text that is before the cursor. The displayed column
 * is not the current column, but the column that would
 * be used on an infinite width display.
 */
int ShowPositionKey(void)
{
	Line *blp = curbp->linep, *lp = blp->forw(), *clp = curvp->position[DOT].line;
	unsigned clo = curvp->position[DOT].offset, eolSize = clp->sizeofEOL();
	unsigned long totalChars = 0;

	for (; lp != clp; lp = lp->forw())
		totalChars += lp->length();
	unsigned charsBeforeCursor = totalChars + clo;
	for (; lp != blp; lp = lp->forw())
		totalChars += lp->length();
	totalChars += lp->length();

	unsigned ratio = totalChars?
			(unsigned)((100L * charsBeforeCursor) / totalChars): 0;

	if (eolSize == 1 || clo + eolSize < clp->length())
	{
		unsigned cursorChar = clp->getc(clo) & 0xff;
		MLWrite("Char = %d (0x%2.2X), chars before cursor = %ld (%d%% of %ld)",
			cursorChar, cursorChar, charsBeforeCursor, ratio, totalChars);
	}
	else if (clp == blp && eolSize == 0)
		MLWrite("Cursor at end of file, %ld characters", totalChars);
	else if (eolSize == 2)
		MLWrite("Chars = 13,10 (0x0D,0x0A), before cursor = %ld (%d%% of %ld)",
			charsBeforeCursor, ratio, totalChars);
	else
 		MLWrite("Char = ?, before cursor = %ld (%d%% of %ld)",
  			charsBeforeCursor, ratio, totalChars);
	return 1;
}


/*--------------------------------------------------------------------------
 * Twiddle the two characters just behind
 * dot. Return with an error if dot
 * is at the beginning of line; it seems to be a bit
 * pointless to make this work. This fixes up a very
 * common typo with a single stroke. This always works
 * within a line.
 */
int TwiddleKey(void)
{
	Line *dotp;
	unsigned doto, cl, cr;
	doto = curvp->position[DOT].offset;
	if (doto < 2 || !SetBufferChanged())
		return 0;
	dotp = curvp->position[DOT].line;
	doto--;
	cr = dotp->getc(doto);
	doto--;
	cl = dotp->getc(doto);
	dotp->putc(doto, cr);
	dotp->putc(doto + 1, cl);
	return 1;
}

/*--------------------------------------------------------------------------
 * Get a literal character, and
 * insert it into the buffer. All the characters
 * are taken literally.
 */
int LiteralCharKey(int n)
{
	int	retCode;
	char c;
	if ((retCode = GetLiteral()) == -1)
		return 0;
	c = retCode;
	while (n--)
		if (InsertChars(1, &c) != 1)
			return 0;
	return 1;
}


/*--------------------------------------------------------------------------
 * Get a literal character in hex or decimal.
 */
int GetLiteral(void)
{
	static bool getLiteralActive = false;
	static Bob buf;
	int	c;
	if (getLiteralActive)
		return -1;			/* block re-entrance */
	getLiteralActive = true;
	if (MLReply("Literal Character [\\c, Xhh, nnn]: ", buf) != 1)
		c = -1;
	else if (buf[0] == '\\')
		switch(toupper(buf[1]))
		{
			case 'A': c = '\a'; break;
			case 'B': c = '\b'; break;
			case 'F': c = '\f'; break;
			case 'N': c = '\n'; break;
			case 'R': c = '\r'; break;
			case 'T': c = '\t'; break;
			default: c = buf[1];
		}
	else if (toupper(buf[0]) == 'X')
	{
		if (!isxdigit(buf[1]))
			return -1;
		c = (buf[1] >= 'A'? toupper(buf[1]) - 'A' + 0xA: buf[1] - '0');
		if (buf[2] && isxdigit(buf[2]))
			c = c * 0x10 + (buf[2] >= 'A'? toupper(buf[2]) - 'A' + 0xA:
			  buf[2] - '0');
	}
	else
		c = atoi(buf.data());
	getLiteralActive = false;
	return c;
}


/*--------------------------------------------------------------------------
 * Set case of all alpha chars in a region. Use the region code to set the
 * limits. Scan the buffer, doing the changes.
 */
static int CaseRegion(bool lower, int n)
{
	int retCode;
	Region region;
	char c;

	if (!SetBufferChanged())
		return 0;

	retCode = GetRegion(&region, n);
	while (region.size--)
		if (region.start.offset == region.start.line->length())
		{
			region.start.line = region.start.line->forw();
			region.start.offset = 0;
		}
		else
		{
			c = region.start.line->getc(region.start.offset);
			c = lower? tolower(c): toupper(c);
			region.start.line->putc(region.start.offset, c);
			region.start.offset++;
		}
	curvp->position[MARK].line = 0;
	return retCode || ForwCharKey(n);
}

int LowerRegionKey(int n) { return CaseRegion(true, n); }
int UpperRegionKey(int n) { return CaseRegion(false, n); }

/*--------------------------------------------------------------------------
 * save the position of DOT in
 * the USER area.
 */
int SavePositionKey(void)
{
	curvp->position[USER] = curvp->position[DOT];
	MLWrite("Saved the current cursor position");
	return 1;
}

/*--------------------------------------------------------------------------
 * restore the position of DOT
 * from the USER area.
 */
int SetPositionKey(void)
{
	if (curvp->position[USER].line != 0)
	{
		curvp->position[DOT] = curvp->position[USER];
		curvp->dotrow = curvp->winp->high/2;
	}
	return 1;
}

/*------------------------------------------------------------------------*/
int InsertTimeKey(int n)
{
	static char months[] = "JanFebMarAprMayJunJulAugSepOctNovDec";
	time_t tmpnow;
	struct tm *now;
	int i;
	char ampm, str[20];
	time(&tmpnow);
	now = localtime(&tmpnow);
	i = now->tm_hour % 24;
	ampm = (i < 12? 'a': 'p');
	if ((i %= 12) == 0)
		i = 12;
	sprintf(str, "%d %3.3s %u, %d:%02d%c",
		now->tm_mday, months + 3 * now->tm_mon, now->tm_year + 1900,
		i, now->tm_min, ampm);
	while (n--)
		if (!InsertChars(strlen(str), str))
			return 0;
	return 1;
}

/*======================================================================*/
