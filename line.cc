#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <pvideo.h>
#include <duiwin.h>
#include <common.h>

//---------------------------------------------------------------------------
// determines size of EOL for current modes and given line
size_t Line::sizeofEOL()
{
	if (len == 0)
		return 0;
	if (modes & MODE_SHOW_CTRL_CHARS)
		return dat[len - 1] == '\n'? 1: 0;
	if (dat[len - 1] != '\n')
		return 0;
	if (len == 1)
		return 1;
	return dat[len - 2] == '\r'? 2: 1;
}

/*--------------------------------------------------------------------------
 * This routine allocates a block of memory large enough to hold a Line
 * containing "used" characters. Return a pointer to the new block, or 0
 * if there isn't any memory left.
 */
Line *Line::alloc(size_t sz, const char *data)
{
	Line *lp = (Line *)malloc(sz + sizeof *lp - sizeof lp->dat);
	if (lp)
	{
		lp->max = sz;
		if (data)
			memcpy(lp->dat, data, lp->len = sz);
		else
			lp->len = 0;
		lp->fp = lp->bp = lp;
	}
	return lp;
}

/*-----------------------------------------------------------------------------
 */
void Line::link(Line *newNext)
{
	fp->bp = newNext;
	newNext->bp = this;
	newNext->fp = this->fp;
	fp = newNext;
}

/*-----------------------------------------------------------------------------
 * acts as a special destructor
 */
void Line::unlink()
{
	fp->bp = bp;
	bp->fp = fp;
	fp = bp = this;
	free(this);
}

/*--------------------------------------------------------------------------
 */
void Line::insert(size_t offset, size_t size, const char *data)
{
	assert(offset <= len && size + len <= max);
	char *base = dat + offset;
	memmove(base + size, base, len - offset);	// shift line to make room
	memcpy(base, data, size); // Add the data
	len += size;
}

/*--------------------------------------------------------------------------
 */
char *Bob::insert(size_t offset, size_t size)
{
	assert(offset <= len && len <= max);
	if (size + len > max)
	{
		char *p = new char[size + len + 1];
		if (!p)
			return 0;
		max = size + len;
		memcpy(p, dat, offset);
		memcpy(p + offset + size, dat + offset, len - offset);
		p[max] = 0;
		delete [] dat;
		dat = p;
	}
	else if (size)
		memmove(dat + offset + size, dat + offset, 1 + len - offset);
	len += size;
	return dat + offset;
}

/*--------------------------------------------------------------------------
 */
void Line::remove(size_t offset, size_t size)
{
	assert(size == 0 || offset < len && len <= max && size <= max && offset + size <= max);
	memmove(dat + offset, dat + offset + size, len - (offset + size));
	len -= size;
}

/*-----------------------------------------------------------------------------
 * reallocates line if it exceeds maxUnused and there is enough memory.
 * return Line* may be different than lp.
 */
static Line *MaybeShrinkLine(Line *lp)
{
	static const size_t maxUnused = 16;
	if (!lp || lp->unused() < maxUnused)
		return lp;
	Line *newLine = Line::alloc(lp->length(), lp->data());
	if (!newLine)
		return lp;
	lp->link(newLine);
	lp->unlink();
	return newLine;
}

/*--------------------------------------------------------------------------
 * Insert characters at the offset. In the easy case all that happens is
 * the text is stored in the line. In the hard case, the line has to be
 * reallocated. Returns line (may be different if reallocated) or 0 on error.
 * Don't actually change anything (e.g. truncate) until no errors can occur.
 */
static Line *WriteInLine(Line *line, size_t offset,
		size_t size, const char *data, bool truncate)
{
	if (size == 0)
		return line;
	assert(offset <= line->length());
	size_t effLen = truncate? offset: line->length();
	if (size >= SIZE_MAX - (effLen + sizeof *line))
	{
		MLWrite("line too long");
		return 0;
	}
	size_t newsize = effLen + size;
	Line *newLine = line; // default is in place
	if (newsize > line->maxLen())
	{
		if (!(newLine = Line::alloc(newsize)))
		{
			MLNoMem();
			return 0;
		}
		newLine->insert(0, effLen, line->data());
		line->link(newLine);
		line->unlink();
	}
	newLine->truncate(effLen);
	newLine->insert(offset, size, data);
	return MaybeShrinkLine(newLine);
}

/*---------------------------------------------------------------------------
 * fixup positions in all views that are within a range of a buffer that
 * is about to be deleted/inserted
 */
struct FixerBase
{
	Line *oldLine, *newLine;
	virtual void onePosition(Position &p) = 0;
	void allPositions()
	{
		if (curbp->linep == oldLine)
			curbp->linep = newLine;

		for (View *vp = vheadp; vp; vp = vp->next)
			if (vp->bufp == curbp)
				for (unsigned i = 0; i < POSITIONCOUNT; i++)
					if (vp->position[i].line == oldLine)
						onePosition(vp->position[i]);
	}
	FixerBase(Line *olp, Line *nlp): oldLine(olp), newLine(nlp) {}
	virtual ~FixerBase() {}
};

struct FixDelInLine: public FixerBase
{
	size_t offs, sz;
	virtual void onePosition(Position &p)
	{
		p.line = newLine;
		if (p.offset >= offs)
			p.offset = p.offset <= offs + sz? offs: p.offset - sz;
	}
	FixDelInLine(Line *olp, Line *nlp, size_t offset, size_t len):
		FixerBase(olp, nlp), offs(offset), sz(len) { allPositions(); }
	virtual ~FixDelInLine() {}
};

struct FixDelLine: public FixerBase
{
	size_t offs;
	virtual void onePosition(Position &p)
		{ p.line = newLine; p.offset = offs; }
	FixDelLine(Line *olp, Line *nlp, size_t offset):
		FixerBase(olp, nlp), offs(offset) { allPositions(); }
	virtual ~FixDelLine() {}
};

struct FixIns: public FixerBase
{
	size_t offs, sz;
	virtual void onePosition(Position &p)
	{
		p.line = newLine;
		if (p.offset >= offs)
			p.offset += sz;
	}
	FixIns(Line *olp, Line *nlp, size_t offset, size_t len):
		FixerBase(olp, nlp), offs(offset), sz(len) { allPositions(); }
	virtual ~FixIns() {}
};

struct FixSplit: public FixerBase
{
	size_t offs;
	Line *newLine1;
	virtual void onePosition(Position &p)
	{
		if (p.offset < offs)
			p.line = newLine1;
		else
		{
			p.offset = 0;
			p.line = newLine;
		}
	}
	FixSplit(Line *olp, Line *nlp, size_t offset, Line *nlp2):
		FixerBase(olp, nlp2), offs(offset), newLine1(nlp) { allPositions(); }
	virtual ~FixSplit() {}
};

struct FixMerge: public FixerBase
{
	size_t offs, newOffs;
	virtual void onePosition(Position &p)
	{
		p.line = newLine;
		if (p.offset < offs)
			p.offset = newOffs;
		else if (offs > newOffs)
			p.offset -= offs - newOffs;
		else
			p.offset += newOffs - offs;
	}
	FixMerge(Line *olp, Line *nlp, size_t offset, size_t newOffset):
		FixerBase(olp, nlp), offs(offset), newOffs(newOffset) { allPositions(); }
	virtual ~FixMerge() {}
};


/*--------------------------------------------------------------------------
 * Insert len characters of block at the current location of dot.
 * Block may contain carriage return and newline chars.
 */
int InsertChars(size_t len, const char *data)
{
	if (len == 0)
		return 1;
	if (!SetBufferChanged())
		return 0;

	const char *start, *end;
	for (start = data;; start = end, ++curvp->dotrow)
	{
		end = (const char *)memchr(start, '\n', len - (start - data));
		Line *lp = curvp->position[DOT].line;
		size_t offset = curvp->position[DOT].offset;
		if (!end)
		{
			if (!(len -= start - data))
				return 1;
			Line *nlp = WriteInLine(lp, offset, len, start, false);
			if (!nlp)
				return 0;
			FixIns(lp, nlp, offset, len);
			return 1;
		}

		size_t nllen = ++end - start;
		if (offset == 0)
		{	// add a whole new line, no position fixups necessary
			Line *nlp = Line::alloc(nllen, start);
			if (!nlp)
				return MLNoMem();
			lp->back()->link(nlp);
		}
		else
		{	// splitline
			Line *lp2 = Line::alloc(lp->length() - offset, lp->data(offset));
			if (!lp2) // New 2nd half
				return MLNoMem();
			Line *lp1 = WriteInLine(lp, offset, nllen, start, true);
			if (!lp1)
			{
				Line::free(lp2);
				return 0;
			}
			lp1->link(lp2);
			FixSplit(lp, lp1, offset, lp2);
		}
	}
}

/*--------------------------------------------------------------------------
 * This function deletes n characters starting at cursor.
 * It returns 1 if all of the characters were deleted, and 0 if
 * they were not because we ran into the end of the buffer. 
 */

int DeleteChars(fsize_t n)
{
	if (n == 0)
		return 1;
	if (!SetBufferChanged())
		return 0;

	while (n)
	{
		Line *lp = curvp->position[DOT].line;
		size_t offset = curvp->position[DOT].offset;
		size_t leftThisLine = lp->length() - offset;

		if (n < leftThisLine || lp == curbp->linep)
		{	// delete within current line
			size_t chunk = n > leftThisLine? leftThisLine: n;
			lp->remove(offset, chunk);
			FixDelInLine(lp, MaybeShrinkLine(lp), offset, chunk);
			return leftThisLine > n? 1: 0;
		}

		Line *nxtlp = lp->forw();
		if (n < leftThisLine + nxtlp->length())
		{	// merge current and next line, trim end of current & start of next
			size_t off2 = n - leftThisLine;
			Line *nlp = WriteInLine(lp, offset, nxtlp->length() - off2,
					nxtlp->data(off2), true);
			if (!nlp)
				return 0;
			nxtlp->unlink(); // delete the next line
			FixDelInLine(lp, nlp, offset, leftThisLine);
			FixMerge(nxtlp, nlp, off2, offset);
			return 1;
		}

		// delete the whole next line
		n -= nxtlp->length();
		nxtlp->unlink();
		FixDelLine(nxtlp, lp, offset);
	}
	return 1;
}

#if 0
/*---------------------------------------------------------------------------
 * fixup positions in all views that are within a range of a buffer that
 * is about to be deleted/inserted

if deleting && !lp2
  any positions on lp < offset: p.line = lp1, p.offset no change
  any positions on lp >= offset && <= offset + len: p.line = lp1, p.offset = offset
  any positions on lp > offset + len: p.line lp1, p.offset -= len
  if (lp == curbp->linep) curbp->linep = lp1;

if deleting && lp2
  any positions on lp: lp1, p.offset = offset
  if (lp == curbp->linep) curbp->linep = lp2;

if !deleting && !lp2
  any positions on lp < offset: p.line = lp1, p.offset no change
  any positions on lp >= offset: p.line = lp1, p.offset += len
  if (lp == curbp->linep) curbp->linep = lp1;

if !deleting && lp2
  any positions on lp < offset: p.line = lp1, p.offset no change
  any positions on lp >= offset: p.line = lp2, offset 0
  if (lp == curbp->linep) curbp->linep = lp2;

 */

static void FixupPositions(bool deleting, Line *lp, size_t offset, size_t len,
		Line *lp1, Line *lp2)
{
	if (lp == curbp->linep)
		curbp->linep = lp2? lp2: lp1;

	for (View *vp = vheadp; vp; vp = vp->next)
		if (vp->bufp == curbp)
			for (unsigned i = 0; i < POSITIONCOUNT; i++)
			{
				Position &p(vp->position[i]);
				if (p.line == lp)
				{
					if (deleting)
					{
						p.line = lp1;
						if (lp2)
							p.offset = offset;
						else if (p.offset >= offset)
							p.offset = p.offset <= offset + len?
									offset: p.offset - len;
					}
					else if (p.offset < offset)
						p.line = lp1;
					else if (lp2)
					{
						p.offset = 0;
						p.line = lp2;
					}
					else
					{
						p.offset += len;
						p.line = lp1;
					}
				}
			}
}
#endif

/*======================================================================*/


