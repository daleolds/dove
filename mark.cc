#include <pvideo.h>
#include <duiwin.h>
#include <common.h>

/*--------------------------------------------------------------------------
 * This routine figures out the bounds of the region in the current view,
 * and fills in the fields of the Region.
 * If MARK is 0 the Region structure is initialized with DOT
 * and the number of chars covered by n cursor rights.
 * Because the DOT and MARK are usually very close together,
 * scan outward from DOT looking for MARK.
 * returns 0 if no mark, 1 if marking,
 *  -1 if marking but we couldn't find it (error).
 */
int GetRegion(Region *rp, unsigned n)
{
	Position dot = curvp->position[DOT], mark = curvp->position[MARK];
	Line *flp = dot.line, *blp = dot.line;
	fsize_t fNewlines = rp->newlines = rp->size = 0, bNewlines = 0;
	fsize_t fsize = flp->length() - dot.offset, bsize = dot.offset;

	if (mark.line == 0) // no mark, region is n right cursors worth of data
	{
		rp->start = dot;
		while (n)
		{
			size_t eolSize = dot.line->sizeofEOL();
			if (dot.offset >= dot.line->length() - eolSize)
			{
				if (dot.line == curbp->linep)
					break;
				assert(eolSize);
				rp->size += eolSize;
				rp->newlines++;
				dot.line = dot.line->forw();
				dot.offset = 0;
				--n;
			}
			else
			{
				size_t chunk = (dot.line->length() - eolSize) - dot.offset;
				if (chunk > n)
					chunk = n;
				rp->size += chunk;
				dot.offset += chunk;
				n -= chunk;
			}
		}
		return 0;
	}

	if (dot.line == mark.line)
	{
		rp->start.line = dot.line;
		if (dot.offset < mark.offset)
		{
			rp->start.offset = dot.offset;
			rp->size = mark.offset - dot.offset;
		}
		else
		{
			rp->start.offset = mark.offset;
			rp->size = dot.offset - mark.offset;
		}
		return 1;
	}

	while (flp != curbp->linep || blp->back() != curbp->linep)
	{
		if (flp != curbp->linep)
		{
			flp = flp->forw();
			fNewlines++;
			if (flp == mark.line)
			{
				rp->start = dot;
				rp->size = fsize + mark.offset;
				rp->newlines = fNewlines;
				return 1;
			}
			fsize += flp->length();
		}
		if (blp->back() != curbp->linep)
		{
			blp = blp->back();
			bNewlines++;
			bsize += blp->length();
			if (blp == mark.line)
			{
				rp->start = mark;
				rp->size = bsize - mark.offset;
				rp->newlines = bNewlines;
				return 1;
			}
		}
	}

	MLWrite("bug: lost mark");
	return -1;
}

/*--------------------------------------------------------------------------
 * Set the position of MARK in the current view
 * to the position of DOT. No errors are possible.
 */
int ToggleMarkKey(int n)
{
	curvp->position[MARK].line = n == 0 || n == 1 && curvp->position[MARK].line?
			0: curvp->position[DOT].line;
	curvp->position[MARK].offset = curvp->position[DOT].offset;
	curvp->positionFlags &= ~PF_TEMP_MARK;
	return 1;
}

/*--------------------------------------------------------------------------
 * Swap the positions of DOT and MARK in
 * the current view.
 */
int SwapMarkKey(void)
{
	Region region;
	if (!GetRegion(&region, 0))
		return 0;
	if (curvp->position[DOT].line != region.start.line)
		curvp->dotrow = (region.newlines > curvp->dotrow?
		  0: curvp->dotrow - (unsigned char)region.newlines);
	else
		curvp->dotrow = (region.newlines > curvp->winp->high - curvp->dotrow?
		  curvp->winp->high: curvp->dotrow + (unsigned char) region.newlines);
	region.start = curvp->position[DOT];
	curvp->position[DOT] = curvp->position[MARK];
	curvp->position[MARK] = region.start;
	return 1;
}

/*======================================================================*/
