#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <pvideo.h>
#include <duiwin.h>
#include <common.h>

#define	KBLOCK	16	// Kill buffer block size
#define MAXKILLS 100
static Bob *kills;

/*--------------------------------------------------------------------------
 * gets all data from region and copies it into buffer.
 * size checks must be previously done by caller.
 */
void Region::get(char *buffer)
{
	Line *lp = start.line;
	size_t chunkSize = lp->length() - start.offset;
	const char *chunkData = lp->data() + start.offset;
	fsize_t szLeft = size;
	while (szLeft > chunkSize)
	{
		memcpy(buffer, chunkData, chunkSize);
		buffer += chunkSize,
		szLeft -= chunkSize;
		lp = lp->forw();
		chunkSize = lp->length();
		chunkData = lp->data();
	}
	memcpy(buffer, chunkData, szLeft);
}

/*--------------------------------------------------------------------------
 * Copy all of the characters in the region to a kill buffer. Don't mess
 * with DOT or MARK.
 */
static int KillRegion(Region *r, bool prepend, bool newKill)
{
	Bob tmpKill, *thisKill = &tmpKill;

	if (r->size == 0)
		return 1;

	if (!newKill)
	{
		assert(kills);
		thisKill = &kills[0];
	}
	else if (!kills && !(kills = new Bob[MAXKILLS]))
		return MLNoMem();

	if (r->size + thisKill->length() >= SIZE_MAX - 2 * KBLOCK)
	{
		MLWrite("Marked block too big %sto kill", newKill? "add to ": "");
		return 0;
	}
	char *data = thisKill->insert(prepend? 0: thisKill->length(), r->size);
	if (!data)
		return MLNoMem();
	r->get(data);
	if (newKill)
	{
		for (unsigned i = MAXKILLS - 1; i > 0; --i)
			kills[i].take(kills[i - 1]);
		kills[0].take(tmpKill);
	}
	return 1;
}

/*--------------------------------------------------------------------------
 * Insert text from a kill buffer. All of the work is done by the
 * insert routines that is passed in as the charFunc arg. 
 */
static int UnKillRegion(const char *data, size_t size, int n,
		int (*charFunc)(size_t len, const char *data))
{
	unsigned char dotrow;
	int retCode = 0;

	if (data == 0)
	{
		MLWrite("Kill stack is empty");
		return 1;
	}

	curvp->position[SAVE] = curvp->position[DOT];
	--curvp->position[SAVE];
	dotrow = curvp->dotrow;
	while (n--)
		if ((retCode = charFunc(size, data)) == 0)
			break;
	curvp->position[DOT] = curvp->position[SAVE];
	++curvp->position[DOT];
	curvp->dotrow = dotrow;
	curvp->position[SAVE].line = 0;
	return retCode;
}

/*--------------------------------------------------------------------------
 * Local routine used to handle DOT and MARK etc when deleting a Region.
 */
static int DeleteRegion(Region *r)
{
	if (r->size == 0)
		return 1;
	if (curvp->position[DOT].line != r->start.line)
		curvp->dotrow = (r->newlines > curvp->dotrow? 0:
		  curvp->dotrow - (unsigned char)r->newlines);
	curvp->position[DOT] = r->start;
	curvp->position[MARK].line = 0;
	curvp->positionFlags &= ~PF_TEMP_MARK;
	return DeleteChars(r->size);
}

/*--------------------------------------------------------------------------
 * Delete backwards. This is quite easy because it's all done with other
 * functions. Just move the cursor back, and delete forwards.
 */
int DeleteBackKey(int n)
{
	Region region;
	curvp->position[MARK].line = 0;
	curvp->positionFlags &= ~PF_TEMP_MARK;
	curbp->thisSeqCmd = SEQ_BACKDEL;
	if (!BackCharKey(n) || GetRegion(&region, n) == -1)
		return 0;
	return KillRegion(&region, true, curbp->lastSeqCmd != SEQ_BACKDEL)?
			DeleteRegion(&region): 0;
}

/*--------------------------------------------------------------------------
 */
int InsertOverwriteChar(int ch)
{
	Region region;
	char c = (char)ch;
	if (curvp->position[MARK].line)
	{
		if (!DeleteKey(1))
			return 0;
	}
	else if (!(modes & MODE_INSERT)
		&& curvp->position[DOT].offset != curvp->position[DOT].line->length())
	{
		curbp->thisSeqCmd = SEQ_OVERWRITE;
		if (GetRegion(&region, 1) == -1
			|| !KillRegion(&region, false, curbp->lastSeqCmd != SEQ_OVERWRITE)
			|| !DeleteRegion(&region))
			return 0;
	}
	return TypeChars(1, &c);
}

/*--------------------------------------------------------------------------
 * retype text from the cut buffer.
 */
int RetypeKey(int n)
{
	return UnKillRegion(kills? kills[0].data(): 0, kills? kills[0].length(): 0, n, TypeChars);
}

/*--------------------------------------------------------------------------
 * Yank text back from the delete buffer.
 */
int UndeleteKey(int n)
{
	return UnKillRegion(kills? kills[0].data(): 0, kills? kills[0].length(): 0, n, InsertChars);
}


/*--------------------------------------------------------------------------
 * Select an entry in an array of strings
 * returns -1 for error, -2 for cancel
 */
int SelectString(const char *title, int stringCount, Bob *strings)
{
	unsigned row, col, high, wide;
	int baseString, i, selection;
	Window *w;
	BINDING cmd;

	GetCursor(&row, &col);
	high = stringCount;
	for (wide = strlen(title) + 3, i = 0; i < stringCount; i++)
	{
		if (wide < strings[i].length())
			wide = strings[i].length();
	}
	if (wide > videoCols - 2)
		wide = videoCols - 2;
	if ((w = CreateWindow(row, col, high, wide, 0)) == 0)
		return -1;
	SetCursorType(CURSOR_NONE);
	for (selection = baseString = 0;;)
	{
		if (!CommandAvailable())
		{
			SelectWindow(w);
			WriteBorder(title, DOUBLE, attrib[DELETEBORDER]);
			WSetAttribute(attrib[DELETETEXT]);
			for (i = baseString; i < baseString + w->high; i++)
			{
				WSetCursor(i - baseString, 0);
				if (i == selection)
					WSetAttribute(attrib[DELETEMARK]);
				WriteData(strings[i].length(), strings[i].data());
				WSetAttribute(attrib[DELETETEXT]);
				WriteLine("");
			}
			WSetAttribute(attrib[DELETETEXT]);
		}
		cmd = BindKey(GetCommand());
		if (cmd.type == FUNCTIONKEY)
		{
			if (functionTable[cmd.data].nonr == CancelKey
					|| functionTable[cmd.data].rept == EnterKey)
			{
				DeleteWindow(w);
				return functionTable[cmd.data].nonr == CancelKey? -2: selection;
			}
			if (functionTable[cmd.data].rept == ForwLineKey
					&& selection + 1 < stringCount)
				selection++;
			else if (functionTable[cmd.data].rept == BackLineKey && selection > 0)
				selection--;
			if (selection < baseString)
				baseString = selection;
			else if (selection - baseString >= w->high)
				baseString = selection - w->high + 1;
		}
	}
}

//---------------------------------------------------------------------------
// return 0 on error, 1 for okay, kill will be 0 if canceled or no kills

int SelectedKill(const char *&kill, size_t &size, bool topKill)
{
	size = 0;
	kill = 0;
	if (topKill)
	{
		if (kills)
		{
			kill = kills[0].data();
			size = kills[0].length();
		}
		return 1;
	}
	int i = 0;
	while (kills && kills[i].length() && i < MAXKILLS)
		++i;
	if (i == 0)
		return 1;
	if ((i = SelectString("select buffer to undelete", i, kills)) < 0)
		return i == -1? 0: 1;
	kill = kills[i].data();
	size = kills[i].length();
	return 1;
}


/*--------------------------------------------------------------------------
 * Manipulate the stack of kills.
 */
int StackOfKillsKey(int n)
{
	const char *data;
	size_t size;

	if (!SelectedKill(data, size, false))
		return 0;
	return data == 0? 1: UnKillRegion(data, size, n, InsertChars);
}

/*--------------------------------------------------------------------------
 * Delete the region. Ask "GetRegion" to figure out the bounds of the region,
 * then pass flags for common routine.
 */
int DeleteKey(int n)
{
	Region region;
	bool newKill = true;
	if (!curvp->position[MARK].line)
	{
		curbp->thisSeqCmd = SEQ_DELETE;
		newKill = curbp->lastSeqCmd != SEQ_DELETE;
	}
	if (GetRegion(&region, n) == -1)
		return 0;
	return KillRegion(&region, false, newKill)? DeleteRegion(&region): 0;
}

/*--------------------------------------------------------------------------
 * Copy all of the characters in the region to the cut buffer. Don't move dot
 * at all. This is a bit like a cut region followed by a Paste.
 */
int CopyKey(int n)
{
	Region region;
	bool newCopy = true;
	if (curvp->position[MARK].line == 0)
	{
		curbp->thisSeqCmd = SEQ_COPY;
		newCopy = curbp->lastSeqCmd != SEQ_COPY;
	}
	int retCode = GetRegion(&region, n);
	if (retCode == -1 || !KillRegion(&region, false, newCopy))
		return 0;
	curvp->position[MARK].line = 0;
	MLWrite("Block copied to buffer");
	return retCode || ForwCharKey(n);
}

/*======================================================================*/
