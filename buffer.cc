#include <ctype.h>
#include <pvideo.h>
#include <duiwin.h>
#include <common.h>

/*--------------------------------------------------------------------------
 * Attach the next buffer to a view
 */
int NextBufferKey(int n)
{
	Buffer *bp;
	bp = curbp;
	while (n--)
		bp = (bp->bufp == 0? bheadp: bp->bufp);
	if (bp != curbp)
		SetBuffer(bp);
	return 1;
}

/*--------------------------------------------------------------------------
 * Attach the previous buffer to a view.
 */
int PrevBufferKey(int n)
{
	Buffer *b;
	while (n--)
	{
		for (b = bheadp; b->bufp != curbp && b->bufp != 0; b = b->bufp)
			;
		SetBuffer(b);
	}
	return 1;
}

/*--------------------------------------------------------------------------
 * Change name of a buffer.
 */
int RenameBufferKey(void)
{
	int i;
	Buffer *next, *prev = 0, *oldPrev;
	char fullSpec[FIO_MAX_PATH + 1];
	Bob fname;

	if (!fname.copy(curbp->fname))
		return MLNoMem();

	if ((i = MLReply("new file name: ", fname)) != 1)
		return i;

	if (!CompletePath(fullSpec, fname.data()))
		return 0;
		
	for (next = bheadp; next && (i = strcmp(fullSpec, next->fname)) > 0;
			next = next->bufp)
		prev = next;

	if (i == 0)
	{
		MLWrite("duplicate file name ignored");
		return 0;
	}
		
	strcpy(curbp->fname, fullSpec);
	curbp->flags &= ~B_READONLY;
	curbp->flags |= B_CHANGED;
	if (prev != curbp && next != curbp)
	{
		if (bheadp == curbp)
			bheadp = curbp->bufp;
		else
			for (oldPrev = bheadp; oldPrev; oldPrev = oldPrev->bufp)
				if (oldPrev->bufp == curbp)
				{
					oldPrev->bufp = curbp->bufp;
					break;
				}
		curbp->bufp = next;
		if (prev)
			prev->bufp = curbp;
		else
			bheadp = curbp;
	}
	return 1;
}

/*--------------------------------------------------------------------------
 * Select a file for editing.
 * Look around to see if you can find the file in another buffer;
 * if you can find it just switch to the buffer. If you cannot find
 * the file, create a new buffer, read in the text, and switch to the
 * new buffer.
 */
static bool CreateBuffers(bool useSourcePath)
{
	static Bob fname;
	unsigned buffersCreated = 0;
	int ret;

	if ((ret = MLReply("file(s) to edit: ", fname, true)) != 1)
		return ret;

	Bob fn;
	const char *start = fname.data(), *cur = start + fname.length();
	do
	{
		while (cur > start && isspace(*(cur - 1)))
			--cur;
		const char *end = cur;
		while (cur > start && !isspace(*(cur - 1)))
			--cur;
		if (fn.copy(end - cur, cur))
			buffersCreated += useSourcePath?
					WildPathEdit(fn.cstr()): WildEdit(fn.cstr(), true);
	} while (cur > start);
	return buffersCreated > 0;
}

int CreateBufferKey(void) { return CreateBuffers(false); }
int CreateBufferFromPathKey(void) { return CreateBuffers(true); }

/*--------------------------------------------------------------------------
 * Dispose of the current buffer if possible.
 */
int DeleteBufferKey(void)
{
	Buffer *bp, *nextbp;
	bp = curbp;
	if (bp->viewCount > 1)
	{
		MLWrite("Cannot delete buffers displayed by multiple views");
		return 0;
	}
	nextbp = (bp->bufp == 0? bheadp: bp->bufp);
	if (bp == nextbp)
	{
		MLWrite("Cannot delete the only buffer");
		return 0;
	}
	if (bp->flags & B_CHANGED && !MLYesNo("Discard changes"))
	{
		MLErase();
		return 0;
	}
	SetBuffer(nextbp);
	DelBuffer(bp);
	MLErase();
	return 1;
}

/*--------------------------------------------------------------------------
 * Unconditionally delete the specified
 * buffer. Clear the buffer, then free the
 * header line and the buffer header.
 */
void DelBuffer(Buffer *bp)
{
	Buffer *bp1, *bp2;
	Line *lp1, *lp2;

	for (lp1 = bp->linep, lp2 = lp1->forw(); lp2 != bp->linep; lp1 = lp2, lp2 = lp2->forw())
		Line::free(lp1);
	Line::free(lp1);
	for (bp1 = 0, bp2 = bheadp; bp2 != bp; bp2 = bp2->bufp)
		bp1 = bp2;
	bp2 = bp2->bufp;						/* Next one in chain	*/
	if (!bp1)								/* Unlink it			*/
		bheadp = bp2;
	else
		bp1->bufp = bp2;
	free(bp);								/* Release buffer block	*/
}


/*--------------------------------------------------------------------------
 */
void CopyViewState(View *dest, View *src)
{
	View *tv = dest->next;
	Buffer *tb = dest->bufp;
	Window *tw = dest->winp;
	unsigned dotrow = dest->dotrow;
	*dest = *src;
	dest->next = tv;
	dest->bufp = tb;
	dest->winp = tw;
	dest->dotrow = dotrow;
}

/*--------------------------------------------------------------------------
 * Attach a buffer to a view. The values of DOT and MARK come from the buffer
 * if the use count is 0. Otherwise, they come from some other view.
 */
void SetBuffer(Buffer *bp)
{
	View *wp;
	if (curbp != 0 && --curbp->viewCount == 0)
		curbp->viewState = *curvp; // Last use
	curbp = curvp->bufp = bp;
	curvp->dotrow = curvp->winp->high / 2;
	if (!bp)
		return;
	if (bp->viewCount++ == 0)
		CopyViewState(curvp, &curbp->viewState); // First use
	else
		/* Look for old */
		for (wp = vheadp; wp != 0; wp = wp->next)
			if (wp != curvp && wp->bufp == bp)
			{
				CopyViewState(curvp, wp);
				break;
			}
}

//---------------------------------------------------------------------------
bool SetBufferChanged()
{
	if (curbp->flags & B_READONLY)
	{
		MLWrite("Buffer is read only");
		return false;
	}
	curbp->flags |= B_CHANGED;
	return true;
}

/*======================================================================*/
