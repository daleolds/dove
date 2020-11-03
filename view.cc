#include <pvideo.h>
#include <duiwin.h>
#include <common.h>

/*--------------------------------------------------------------------------
 * The command makes the next view (down the screen) the current view.
 */
int NextViewKey(int n)
{
	while (n-- > 0)
		curvp = (curvp->next == 0? vheadp: curvp->next);
	curbp = curvp->bufp;
	return 1;
}

/*--------------------------------------------------------------------------
 * This command makes the previous view (up the screen) the current view.
 */
int PrevViewKey(int n)
{
	View *v;
	while (n-- > 0)
	{
		for (v = vheadp; v->next != curvp && v->next != 0; v = v->next)
			;
		curvp = v;
		curbp = v->bufp;
	}
	return 1;
}

/*-----------------------------------------------------------------------------
 * checks to see if current view can be resized or completely merged
 * with the given view.
 */
static int CanResizeThisView(View *v, int *increaseRows)
{
	if (v->winp->high == curvp->winp->high
			&& v->winp->row == curvp->winp->row)
	{
		*increaseRows = 0;
		return 1;
	}
	if (v->winp->wide == curvp->winp->wide
			&& v->winp->col == curvp->winp->col)
	{
		*increaseRows = 1;
		return 1;
	}
	return 0;
}

/*-----------------------------------------------------------------------------
 * checks to see if current view can be resized or completely merged
 * with a neighboring view.  Returns pointer to other view to be adjusted
 * or 0.
 */
static View *CanResizeView(int *increaseRows)
{
	View *v;
	
	v = curvp->next;
	if (v != 0 && CanResizeThisView(v, increaseRows))
			return(v);
	if (vheadp == curvp)
		return 0;
	for (v = vheadp; v->next != curvp; v = v->next)
			;
	return CanResizeThisView(v, increaseRows)? v: 0;
}

/*--------------------------------------------------------------------------
 * Deletes the current view if possible.
 */
int DeleteViewKey(void)
{
	View *v, *prev;
	int increaseRows;

	if ((v = CanResizeView(&increaseRows)) == 0)
	{
		MLWrite("Cannot delete this view");
		return 0;
	}

	if (increaseRows)
	{
		if (curvp->next == v)
		{
			v->winp->row = curvp->winp->row;
			v->dotrow += curvp->winp->high + 1;
		}
		v->winp->high += curvp->winp->high + 1;
	}
	else
	{
		if (curvp->next == v)
			v->winp->col = curvp->winp->col;
		v->winp->wide += curvp->winp->wide + 1;
	}

	DeleteWindow(curvp->winp);
	if (vheadp == curvp)
		vheadp = curvp->next;
	else
	{
		for (prev = vheadp; prev->next != curvp; prev = prev->next)
			;
		prev->next = curvp->next;
	}
	if (--curbp->viewCount == 0)
		curbp->viewState = *curvp;
	free(curvp);
	curvp = v;
	curbp = v->bufp;
	return 1;
}

/*--------------------------------------------------------------------------
 * Split the current view. A view smaller than 3 lines cannot be split.
 * The only other error that is possible is a "malloc" failure allocating
 * the structure for the new view.
 */
int CreateViewKey(void)
{
	View *v;
	unsigned char newUpperRows = 0, newLowerRows, newLeftCols = 0, newRightCols;
	int key;
	Window *w, *w1, *w2;
	key = MLKey("Split view? [Vertical, Horizontal]", "", "");
	if (key != 'H' && key != 'V')
	{
		MLErase();
		return 1;
	}
	if (key == 'H' && curvp->winp->high < 3 || curvp->winp->wide < 5)
	{
		MLWrite("View is too small to split");
		return 0;
	}
	if ((v = (View *)malloc(sizeof(*v))) == 0)
		goto Error0;
	*v = *curvp;
	w = v->winp;
	if (key == 'H')
	{
		newUpperRows = (w->high - 1) / 2;
		newLowerRows = (w->high - 1) - newUpperRows;
		if ((w1 = CreateWindow(w->row, w->col, newUpperRows, w->wide,
				TILEDBORDER|BOTTOMTITLE|DESTRUCTIVE|EDGEBORDER)) == 0 ||
				(w2 = CreateWindow(w->row + newUpperRows + 1, w->col,
				newLowerRows, w->wide, TILEDBORDER|BOTTOMTITLE|DESTRUCTIVE|EDGEBORDER)) == 0)
			goto Error1;
	}
	else
	{
		newLeftCols = (w->wide - 1) / 2;
		newRightCols = (w->wide - 1) - newLeftCols;
		if ((w1 = CreateWindow(w->row, w->col, w->high, newLeftCols,
				TILEDBORDER|BOTTOMTITLE|DESTRUCTIVE|EDGEBORDER)) == 0 ||
				(w2 = CreateWindow(w->row, w->col + newLeftCols + 1, w->high,
				newRightCols, TILEDBORDER|BOTTOMTITLE|DESTRUCTIVE|EDGEBORDER)) == 0)
			goto Error1;
	}
	DeleteWindow(w);
	v->next = curvp->next;
	curvp->next = v;
	curvp->winp = w1;
	v->winp = w2;
	if (key == 'H')
	{
		if (curvp->dotrow > newUpperRows)
		{
			curvp->dotrow -= newUpperRows + 1;
			curvp = v;
		}
	}
	else
	{
		v->leftcol += newLeftCols + 1;
		if (CursorCol() > newLeftCols + 1)
			curvp = v;
	}
	++curbp->viewCount;
	MLErase();
	return 1;
Error1:
	free(v);
Error0:
	MLWrite("Cannot allocate memory for a new view");
	return 0;
}

/*--------------------------------------------------------------------------
 * Adjust the size of the current view by "n" lines (positive or negative).
 */
static int ResizeView(int n)
{
	View *v, *shrinkingView, *growingView;
	int increaseRows;

	if ((v = CanResizeView(&increaseRows)) == 0)
	{
		MLWrite("Cannot resize this view");
		return 0;
	}

	if (n < 0)
	{
		shrinkingView = curvp;
		growingView = v;
		n = -n;
	}
	else
	{
		shrinkingView = v;
		growingView = curvp;
	}
	if (increaseRows)
	{
		if (shrinkingView->winp->high <= n)
		{
			MLWrite("Cannot change size by %d lines", n);
			return 0;
		}
		if (growingView->next == shrinkingView)
		{
			shrinkingView->winp->row += n;
			if (shrinkingView->dotrow >= n)
				shrinkingView->dotrow -= n;
			else 
				shrinkingView->dotrow = 0;
		}
		else
		{
			growingView->winp->row -= n;
			growingView->dotrow += n;
		}
		growingView->winp->high += n;
		shrinkingView->winp->high -= n;
	}
	else
	{
		if (shrinkingView->winp->wide <= n + 2)
		{
			MLWrite("Cannot change size by %d columns", n);
			return 0;
		}
		if (growingView->next == shrinkingView)
			shrinkingView->winp->col += n;
		else
			growingView->winp->col -= n;
		growingView->winp->wide += n;
		shrinkingView->winp->wide -= n;
	}
	return 1;
}

/*--------------------------------------------------------------------------
 * Enlarge/Shrink the current view.  Let the common routine do everything.
 */
int EnlargeViewKey(int n) { return ResizeView(n); }

int ShrinkViewKey(int n) { return ResizeView(-n); }

/*======================================================================*/
