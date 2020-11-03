#include <ctype.h>
#include <assert.h>
#include <pvideo.h>
#include <duiwin.h>
#include <common.h>

static inline bool eq(char a, char b)
	{ return (modes & MODE_CASESENSITIVE)? a == b: toupper(a) == toupper(b); }

static Bob searchPat, replacePat;

#define REPLACE	0x01
#define QUERY	0x02
#define BACK	0x04
#define GLOBAL	0x08

/*--------------------------------------------------------------------------
 * General use search and replace.
 */
static int SearchReplace(int n, int sflags)
{
	Position tmp, cur = curvp->position[DOT];
	Buffer *startBuffer = curbp;
	unsigned long found = 0;
	unsigned char cRow = curvp->dotrow, maxRow = curvp->winp->high - 1;
	char c;
	const char *pp;
	tmp.line = 0;
	tmp.offset = 0;
	if (sflags & GLOBAL)
		curvp->position[SAVE] = cur;
	MLWrite("Searching ...");
	for (;;)
	{
		if (sflags & BACK)
		{
			while (cur.offset == 0)
			{
				if (cRow > 0)
					cRow--;
				cur.line = cur.line->back();
				if (cur.line == curbp->linep)
					goto SearchEnd;
				cur.offset = cur.line->length();
				assert(cur.offset);
			}
			cur.offset--;
		}
		else
		{
			cur.offset++;
			while (cur.offset >= cur.line->length())
			{
				if (cur.line == curbp->linep)
				{
					if (!(sflags & GLOBAL))
						goto SearchEnd;
					SetBuffer(curbp->bufp? curbp->bufp: bheadp);
					cur.line = curbp->linep;
					cRow = 0;
				}
				else if (cRow < maxRow)
					cRow++;
				cur.line = cur.line->forw();
				cur.offset = 0;
				assert(cur.line->length() || cur.line == curbp->linep);
			}
			
			if (sflags & GLOBAL && curbp == startBuffer
					&& cur.line == curvp->position[SAVE].line
					&& cur.offset == curvp->position[SAVE].offset)
			{
				curvp->position[DOT] = cur;
				curvp->dotrow = cRow;
				curvp->position[SAVE].line = 0;
				goto SearchEnd;
			}
		}
NoIncrement:
		tmp = cur;
		assert(tmp.offset < tmp.line->length());
		for (pp = searchPat.data(); pp - searchPat.data() < searchPat.length(); ++pp)
		{
			assert(tmp.offset < tmp.line->length());
			if (!eq(tmp.line->getc(tmp.offset), *pp))
				break;
			if (++tmp.offset >= tmp.line->length())
			{
				if (tmp.line == curbp->linep)
					break;
				tmp.line = tmp.line->forw();
				tmp.offset = 0;
				if (tmp.line == curbp->linep && tmp.line->length() == 0)
					break;
				assert(tmp.line->length() || tmp.line == curbp->linep);
				assert(tmp.offset < tmp.line->length());
			}
		}
		if (pp - searchPat.data() == searchPat.length()) /* got a match */
		{
			found++;
			curvp->position[DOT] = cur;
			curvp->dotrow = cRow;
			if ((sflags & (QUERY|REPLACE)) == (QUERY|REPLACE))
			{
				curvp->position[MARK] = tmp;
				DisplayViews();
				curvp->position[MARK].line = 0;
				c = MLKey("Replace? [Yes, No, All, yes-Stop] ", "", "");
				MLWrite("Searching ...");
				switch(c)
				{
					case 'A': sflags &= ~QUERY; n = -1;
					case 'Y':
					case ' ': goto DoReplace;
					case 'S': n = 1; goto DoReplace;
					case -1: MLWrite("Query replace cancelled"); return 1;
				}
			}
			else if (sflags & REPLACE)
			{
			DoReplace:
				if (!DeleteChars(searchPat.length())
						|| !InsertChars(replacePat.length(), replacePat.data()))
					return 0;
				cur = curvp->position[DOT];
				if (n != -1 && --n == 0)
					break;
				goto NoIncrement;
			}
			else if (n != -1 && --n == 0)
				break;
		}
	}
SearchEnd:
	if (!found)
		MLWrite("Not found");
	else
		MLWrite("Found %lu times", found);
	return found? 1: 0;
}

/*--------------------------------------------------------------------------
 * toggle case sensitivity flag used
 * by search and replace functions.
 */
int ToggleCaseSearchKey(int n)
{
	DisplayMode("Case Sensitive", ChangeMode(n, MODE_CASESENSITIVE));
	return 1;
}

/*--------------------------------------------------------------------------
 * Reverse search.
 * Get a search string from the user, and search, starting at "."
 * and proceeding toward the front of the buffer. If found "." is left
 * pointing at the first character of the pattern (the last character that
 * was matched).
 *
 * Forward/Global Search.
 * Get search string from the user. Call general routine with the
 * appropriate flags set to do all the work.
 *
 * Replace/GlobalReplace/QueryReplace/GlobalQueryReplace.
 * Get search and replace strings from the user. Call general routine with the
 * appropriate flags set to do all the work.
 */

static int PromptSR(const char *prompt, int sflags, int n)
{
	if (MLReply(prompt, searchPat) != 1
			|| (sflags & REPLACE) && MLReply("With: ", replacePat) == -1)
		return 0;
	return SearchReplace(n, sflags);
}

int BackSearchKey(int n)
	{ return PromptSR("Reverse search: ", BACK, n); }

int ForwSearchKey(int n)
	{ return PromptSR("Forward Search: ", 0, n); }

int GlobalSearchKey(int n)
	{ return PromptSR("Global Search: ", GLOBAL, n); }

int ReplaceKey(int n)
	{ return PromptSR("Replace: ", REPLACE, n); }

int GlobalReplaceKey(int n)
	{ return PromptSR("Global replace: ", REPLACE | GLOBAL, n); }

int QueryReplaceKey(int n)
	{ return PromptSR("Query replace: ", REPLACE | QUERY, n == 1? -1: n); }

int GlobalQueryReplaceKey(int n)
	{ return PromptSR("Global query replace: ", REPLACE | QUERY | GLOBAL, n == 1? -1: n); }

/*--------------------------------------------------------------------------
 * Find matching brace, bracket, or paren.
 */
int FindMatchKey(void)
{
	static char pairs[] = "{}[]()<>";
	Position cur;
	unsigned level, forward;
	unsigned char cRow, maxRow;
	char startChar, endChar, c, *pp;
	cRow = curvp->dotrow;
	maxRow = curvp->winp->high - 1;
	cur = curvp->position[DOT];
	startChar = cur.line->getc(cur.offset);
	for (pp = pairs; *pp != '\0' && *pp != startChar; pp++)
		;
	if (*pp == '\0')
	{
		MLWrite("Can only match these characters: %s", pairs);
		return 1;
	}
	forward = (((pp - pairs) % 2) == 0);
	endChar = *(forward? pp + 1: pp - 1);
	for (level = 1; level > 0; )
	{
		if (forward)
			for (cur.offset++; cur.offset >= cur.line->offsetofEOL();
					cur.line = cur.line->forw(), cur.offset = 0)
			{
				if (cur.line == curbp->linep)
					goto notfound;
				if (cRow < maxRow)
					cRow++;
			}
		else
			for (; cur.offset-- == 0; cur.line = cur.line->back(),
					cur.offset = cur.line->offsetofEOL())
			{
				if (cur.line->back() == curbp->linep)
					goto notfound;
				if (cRow > 0)
					cRow--;
			}
		if ((c = cur.line->getc(cur.offset)) == startChar)
			level++;
		else if (c == endChar)
			level--;
	}
	curvp->position[DOT] = cur;
	curvp->dotrow = cRow;
	return 1;

notfound:
	MLWrite("Not found");
	return 0;
}

/*--------------------------------------------------------------------------
 * set search or replace mask from region
 */
static int SetMask(const char *name, Bob &mask, int n, unsigned char seqCmd)
{
	bool newMask = true;
	if (!curvp->position[MARK].line)
	{
		curbp->thisSeqCmd = seqCmd;
		newMask = curbp->lastSeqCmd != seqCmd;
	}
	if (newMask)
		mask.clear();
	Region r;
	int retCode = GetRegion(&r, n);
	char *p = mask.insert(mask.length(), r.size);
	if (!p)
		return MLNoMem();
	r.get(p);
	curvp->position[MARK].line = 0;
	MLWrite("%s pattern set", name);
	return retCode || ForwCharKey(n);
}

int SetSearchMaskKey(int n) { return SetMask("Search", searchPat, n, SEQ_SMASK); }
int SetReplaceMaskKey(int n) { return SetMask("Replace", replacePat, n, SEQ_RMASK); }

/*======================================================================*/
