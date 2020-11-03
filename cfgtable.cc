#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include <pkeybrd.h>
#include <cfgtable.h>
#include <common.h>

#define KF(func)	func

BINDING *bindingTable;
unsigned bindingCount;
unsigned short *macroTable[MACROCOUNT];

static int cfgFileMode = 0;

/*--------------------------------------------------------------------------
 * returns index into binding table, -1 if not found,
 * -2 if no memory to grow table
 */
int FindBinding(unsigned key, bool insert)
{
	BINDING *probe, *bmin = bindingTable;
	unsigned i, count = bindingCount;
	while (count > 0)
	{
		i = count >> 1;
		probe = &bmin[i];
		if (probe->key == key)
			return (int)(probe - bindingTable);
		if (KeyData(probe->key) > KeyData(key)
				|| KeyData(probe->key) == KeyData(key) && probe->key > key)
	      count = i;
		else
		{
	      bmin = &probe[1];
	      count = count - i - 1;
   	}
	}
	if (!insert)
		return -1;

	i = (unsigned)(bmin - bindingTable);	/* bmin points to insertion */
	count = bindingCount + 1;
	BINDING *newTable = (BINDING *)malloc(count * sizeof *bindingTable);
	if (!newTable)
		return -2;
	memcpy(newTable, bindingTable, i * sizeof *bindingTable);
	newTable[i].key = key;
	newTable[i].type = NOTBOUND;
	newTable[i].data = 0;
	memcpy(&newTable[i + 1], &bindingTable[i],
			(bindingCount - i) * sizeof *bindingTable);
	free(bindingTable);
	bindingTable = newTable;
	bindingCount = count;
	return i;
}
/*-----------------------------------------------------------------------------
 * default configuration initialization
 */
CONFIGINFO cfg =
{
	"DOVE Configuration File.\r\n\x1A",
	1,													/* configVersion */
	MODE_INDENT | MODE_CASESENSITIVE | MODE_INSERT | MODE_REALTABS
#if !defined(DOVE_FOR_GNU)
	| MODE_CR_LF
#endif
	, { 4 /* tabSize */, 0	/* wordWrapCol */	},

	/* default colors */
	{
		MakeAttrib(VWHITE, VBLACK),				/* viewTextAttrib */
		MakeAttrib(VWHITE, VGREEN) | VBRIGHT,	/* viewTextCursor */
		MakeAttrib(VWHITE, VBLUE),				/* viewMarkAttrib */
		MakeAttrib(VWHITE, VGREEN) | VBRIGHT,	/* viewMarkCursor */
		MakeAttrib(VCYAN, VBLACK),				/* viewBorderAttrib */
		MakeAttrib(VYELLOW, VBLACK) | VBRIGHT,	/* viewLongLineAttrib */
		MakeAttrib(VYELLOW, VBLACK) | VBRIGHT,	/* msgLineAttrib */
		MakeAttrib(VBLACK, VYELLOW),			/* msgLineCursor */
		MakeAttrib(VBLUE, VBLACK),				/* lineColAttrib */
		MakeAttrib(VMAGENTA, VBLACK),			/* timeAttrib */
		MakeAttrib(VWHITE, VCYAN) | VBRIGHT,	/* helpBorderAttrib */
		MakeAttrib(VBLACK, VCYAN),				/* helpTextAttrib */
		MakeAttrib(VBLUE, VWHITE),				/* deleteBorderAttrib */
		MakeAttrib(VBLACK, VWHITE),				/* deleteTextAttrib */
		MakeAttrib(VWHITE, VBLUE) | VBRIGHT,	/* deleteMarkAttrib */
	},
};

unsigned modes, tabSize, wordWrapCol;

/*-----------------------------------------------------------------------------
 */
CATEGORY categories[CT_COUNT] =
{
	{0, "File"}, {0, "Edit"}, {0, "Search"}, {0, "Cursor"}, {0, "Block"},
	{0, "Misc"}, {0, "Options"}, {0, "Window"}, {0, "Help"},
};

/*--------------------------------------------------------------------------
 */
void InitCategories()
{
	int i;
	for (i = 0; i < CT_COUNT; i++)
		categories[i].entries = 0;
	for (i = 0; i < functionCount; i++)
		categories[functionTable[i].category].entries++;
}

/*----------------------------------------------------------------------------
* function table
*/
#define NULLREPT 0
#define NULLNOREPT 0

KEYFUNCTION functionTable[] =
{
{KF(HelpKey), NULLREPT, CT_HELP, "Help", F1},
{KF(QuitKey), NULLREPT, CT_FILE, "Quit", DCTRL | 'Q'},
{KF(CancelKey), NULLREPT, CT_MISC, "Cancel", ESC},
{NULLNOREPT, KF(MenuKey), CT_HELP, "Menu System", F10},
{KF(ShellKey), NULLREPT, CT_MISC, "Shell", SHIFT | ESC},
{KF(CommandKey), NULLREPT, CT_MISC, "Run external program"},
{KF(SaveMacrosKey), NULLREPT, CT_MISC, "Save macros to file", DCTRL | F2},
{KF(ReadMacrosKey), NULLREPT, CT_MISC, "Read in macro file", ALT | F2},
{KF(RecordMacroKey), NULLREPT, CT_MISC, "Begin/end macro", F2},
{NULLNOREPT, KF(BlockMacroKey), CT_MISC, "Record block as macro", SHIFT | F2},
{NULLNOREPT, KF(InsertModeKey), CT_OPTION, "Toggle insert mode", INS},
{NULLNOREPT, KF(IndentModeKey), CT_OPTION, "Toggle indent mode", ALT | ENTER},
{NULLNOREPT, KF(TabModeKey), CT_OPTION, "Set size/tab mode", DCTRL | 'T'},
{NULLNOREPT, KF(WordWrapModeKey), CT_OPTION, "Set word wrap column", DCTRL | 'W'},
{NULLNOREPT, KF(TabKey), CT_EDIT, "Insert tab", TAB, DCTRL | 'I'},
{NULLNOREPT, KF(EnterKey), CT_EDIT, "Start new line", ENTER, DCTRL | 'M'},
{NULLNOREPT, KF(ForwCharKey), CT_CURSOR, "Forward by chars", RIGHT},
{NULLNOREPT, KF(BackCharKey), CT_CURSOR, "Backward by chars", LEFT},
{NULLNOREPT, KF(ForwLineKey), CT_CURSOR, "Forward by lines", DOWN},
{NULLNOREPT, KF(BackLineKey), CT_CURSOR, "Backward by lines", UP},
{NULLNOREPT, KF(ForwPageKey), CT_CURSOR, "Forward by pages", PGDN},
{NULLNOREPT, KF(BackPageKey), CT_CURSOR, "Backward by pages", PGUP},
{NULLNOREPT, KF(BackWordKey), CT_CURSOR, "Backward by words", DCTRL | LEFT},
{NULLNOREPT, KF(ForwWordKey), CT_CURSOR, "Forward by words", DCTRL | RIGHT},
{NULLNOREPT, KF(ScrollDownKey), CT_CURSOR, "Scroll down"},
{NULLNOREPT, KF(ScrollUpKey), CT_CURSOR, "Scroll up"},
{NULLNOREPT, KF(ScrollLeftKey), CT_CURSOR, "Scroll left"},
{NULLNOREPT, KF(ScrollRightKey), CT_CURSOR, "Scroll right"},
{KF(GoToBOLKey), NULLREPT, CT_CURSOR, "Start of line", HOME},
{KF(GoToEOLKey), NULLREPT, CT_CURSOR, "End of line", END},
{KF(GoToBOVKey), NULLREPT, CT_CURSOR, "Upper line", DCTRL | PGUP},
{KF(GoToEOVKey), NULLREPT, CT_CURSOR, "Lower line", DCTRL | PGDN},
{KF(GoToBOBKey), NULLREPT, CT_CURSOR, "Start of buffer", DCTRL | HOME},
{KF(GoToEOBKey), NULLREPT, CT_CURSOR, "End of buffer", DCTRL | END},
{NULLNOREPT, KF(DeleteBackKey), CT_EDIT, "Backup and delete", BSP, DCTRL | 'H'},
{NULLNOREPT, KF(ToggleMarkKey), CT_BLOCK, "Toggle marking block", DCTRL | 'A'},
{NULLNOREPT, KF(CopyKey), CT_EDIT, "Copy to kill stack", DCTRL | 'C'},
{NULLNOREPT, KF(DeleteKey), CT_EDIT, "Delete to kill stack", DEL, DCTRL | 'X'},
{NULLNOREPT, KF(StackOfKillsKey), CT_BLOCK, "Manipulate kill stack", DCTRL | 'Z'},
{NULLNOREPT, KF(UndeleteKey), CT_EDIT, "Insert last kill", DCTRL | 'V'},
{NULLNOREPT, KF(RetypeKey), CT_EDIT, "Reform last kill", DCTRL | 'F'},
{KF(SwapMarkKey), NULLREPT, CT_BLOCK, "Swap block begin/end", DCTRL | 'E'},
{KF(DeleteBufferKey), NULLREPT, CT_FILE, "Delete current buffer", F5},
{KF(CreateBufferKey), NULLREPT, CT_FILE, "Edit file/New buffer", ALT | 'O'},
{NULLNOREPT, KF(PrevBufferKey), CT_FILE, "Previous buffer", F7},
{NULLNOREPT, KF(NextBufferKey), CT_FILE, "Next buffer", F8},
{KF(RenameBufferKey), NULLREPT, CT_FILE, "Rename current buffer", F6},
{KF(FileReadKey), NULLREPT, CT_FILE, "Insert file in buffer"},
{KF(FileWriteKey), NULLREPT, CT_FILE, "Save buffer or block", DCTRL | 'S'},
{KF(DeleteViewKey), NULLREPT, CT_VIEW, "Delete current view", DCTRL | F11},
{KF(CreateViewKey), NULLREPT, CT_VIEW, "Create a view", DCTRL | F12},
{NULLNOREPT, KF(PrevViewKey), CT_VIEW, "Previous view", F11},
{NULLNOREPT, KF(NextViewKey), CT_VIEW, "Next view", F12},
{NULLNOREPT, KF(ShrinkViewKey), CT_VIEW, "Shrink current view", SHIFT | DCTRL | F11},
{NULLNOREPT, KF(EnlargeViewKey), CT_VIEW, "Enlarge current view", SHIFT | DCTRL | F12},
{NULLNOREPT, KF(ForwSearchKey), CT_SEARCH, "Search forward", DCTRL | 'F'},
{NULLNOREPT, KF(ToggleCaseSearchKey), CT_OPTION, "Toggle case sense", ALT | 'I'},
{NULLNOREPT, KF(BackSearchKey), CT_SEARCH, "Search backwards", ALT | 'F'},
{NULLNOREPT, KF(QueryReplaceKey), CT_SEARCH, "Query and replace", ALT | 'H'},
{NULLNOREPT, KF(ReplaceKey), CT_SEARCH, "Search and replace"},
{NULLNOREPT, KF(SetSearchMaskKey), CT_SEARCH, "Set search mask", ALT | 'M'},
{NULLNOREPT, KF(SetReplaceMaskKey), CT_SEARCH, "Set replace mask", ALT | 'R'},
{KF(MemoryLeftKey), NULLREPT, CT_MISC, "Display avail memory"},
{KF(TwiddleKey), NULLREPT, CT_MISC, "Dyslexic key", DCTRL | 'D'},
{KF(FindMatchKey), NULLREPT, CT_SEARCH, "Match brace/paren", DCTRL | ']'},
{KF(ShowPositionKey), NULLREPT, CT_MISC, "Show cursor position", DCTRL | 'P'},
{KF(SavePositionKey), NULLREPT, CT_MISC, "Set bookmark", DCTRL | F3},
{KF(SetPositionKey), NULLREPT, CT_MISC, "Go to bookmark", F3},
{NULLNOREPT, KF(LiteralCharKey), CT_MISC, "Insert literal char", ALT | '='},
{NULLNOREPT, KF(LowerRegionKey), CT_EDIT, "Lower case block", DCTRL | 'L'},
{NULLNOREPT, KF(UpperRegionKey), CT_EDIT, "Upper case block", DCTRL | 'U'},
{NULLNOREPT, KF(InsertTimeKey), CT_EDIT, "Insert time and date"},
{KF(MarkToBOLKey), NULLREPT, CT_BLOCK, "Mark to start of line", SHIFT | HOME},
{NULLNOREPT, KF(MarkBackCharKey), CT_BLOCK, "Mark back by chars", SHIFT | LEFT},
{KF(MarkToEOLKey), NULLREPT, CT_BLOCK, "Mark to end of line", SHIFT | END},
{NULLNOREPT, KF(MarkForwCharKey), CT_BLOCK, "Mark forward by chars", SHIFT | RIGHT},
{KF(MarkToBOBKey), NULLREPT, CT_BLOCK, "Mark to start of buffer", SHIFT | DCTRL | HOME},
{KF(MarkToEOBKey), NULLREPT, CT_BLOCK, "Mark to end of buffer", SHIFT | DCTRL | END},
{KF(MarkToBOVKey), NULLREPT, CT_BLOCK, "Mark to start of view", SHIFT | DCTRL | PGUP},
{KF(MarkToEOVKey), NULLREPT, CT_BLOCK, "Mark to end of view", SHIFT | DCTRL | PGDN},
{NULLNOREPT, KF(MarkForwLineKey), CT_BLOCK, "Mark forward by lines", SHIFT | DOWN},
{NULLNOREPT, KF(MarkBackLineKey), CT_BLOCK, "Mark back by lines", SHIFT | UP},
{NULLNOREPT, KF(MarkForwPageKey), CT_BLOCK, "Mark forward by pages", SHIFT | PGDN},
{NULLNOREPT, KF(MarkBackPageKey), CT_BLOCK, "Mark backward by pages", SHIFT | PGUP},
{NULLNOREPT, KF(MarkForwWordKey), CT_BLOCK, "Mark forward by word edges", SHIFT | DCTRL | RIGHT},
{NULLNOREPT, KF(MarkBackWordKey), CT_BLOCK, "Mark back by word edges", SHIFT | DCTRL | LEFT},
{NULLNOREPT, KF(MarkScrollDownKey), CT_BLOCK, "Mark scroll down"},
{NULLNOREPT, KF(MarkScrollUpKey), CT_BLOCK, "Mark scroll up"},
{NULLNOREPT, KF(GlobalReplaceKey), CT_SEARCH, "Global replace"},
{NULLNOREPT, KF(GlobalQueryReplaceKey), CT_SEARCH, "Global query/replace", ALT | SHIFT | 'H'},
{NULLNOREPT, KF(GlobalSearchKey), CT_SEARCH, "Global search", ALT | SHIFT | 'F'},
{KF(CreateBufferFromPathKey), NULLREPT, CT_FILE, "Edit files on source path", DCTRL | 'O'},
{NULLNOREPT, KF(CtrlCharsModeKey), CT_OPTION, "Toggle show ctrl chars mode" /*, DCTRL | 'Q'*/},
{NULLNOREPT, KF(CRLFModeKey), CT_OPTION, "Toggle CR-LF mode", DCTRL | '\\'},
};

unsigned functionCount = ELEMCOUNT(functionTable);

/*--------------------------------------------------------------------------
 */
char *KeyName(int key, int braceFlag)
{
	static const char *functionKeyNames[] =
	{
		"Esc", "PrtSc", "Break", "Tab", "F1", "F2", "F3", "F4", "F5", "F6",
		"F7", "F8", "F9", "F10", "F11", "F12", "NP.", "NP0", "NP1", "NP2",
		"NP3", "NP4", "NP6", "NP7", "NP8", "NP9", "NP5", "Delete", "Insert",
		"End", "Down", "PgDn", "Left", "Right", "Home", "Up", "PgUp", "BSP",
		"Enter", "NPEntr", "NP*", "NP-", "NP+", "NP/", "NonKey", "ErrorKey",
		"ScreenRefresh"
	};
	static char name[50];

	if (!(key & (FUNC|DCTRL|SHIFT|ALT|REPT)))
	{
		if (key == '{' && braceFlag)
			strcpy(name, "{{}");
		else
		{
			name[0] = key;
			name[1] = 0;
		}
		return name;
	}
	name[0] = 0;
	if (braceFlag)
		strcpy(name, "{");
	if (key & REPT)
		sprintf(name + strlen(name), "Repeat %u", key & ~REPT);
	else
	{
		if (key & ALT)
			strcat(name, "Alt+");
		if (key & DCTRL)
			strcat(name, "Ctrl+");
		if (key & SHIFT)
			strcat(name, "Shift+");
		if (key & FUNC)
			strcat(name, functionKeyNames[KeyData(key) - 1]);
		else
		{
			char dataKey[2];
			dataKey[0] = KeyData(key);
			dataKey[1] = 0;
			strcat(name, dataKey);
		}
	}
	if (braceFlag)
		strcat(name, "}");
	return name;
}

/*--------------------------------------------------------------------------
 */
unsigned MacroLen(unsigned short *m)
{
	unsigned len = 1;
	while (*m++ != NONKEY)
		len++;
	return len;
}

/*--------------------------------------------------------------------------
 */
void SetActiveModes()
{
	modes = cfg.modes;
	tabSize = cfg.number[TABSIZE];
	wordWrapCol = cfg.number[WORDWRAPCOL];
}

/*--------------------------------------------------------------------------
 */
bool ReadConfig()
{
	unsigned short indexAndLength[2], bcount, *mbuffer, *mtable[MACROCOUNT];
	size_t tsize;
	BINDING *btable = 0;
	int fh, i, j, k;
	unsigned key;
	CONFIGINFO tmpcfg;
	memset(mtable, 0, sizeof mtable);
	memset(macroTable, 0, sizeof macroTable);
	InitCategories();
	const char *fname = FileCfgName();
	if ((fh = FileOpen(fname, &cfgFileMode)) == FIO_INVALID_HANDLE)
		goto error;
	if (FileRead(fh, &tmpcfg, sizeof tmpcfg) != sizeof tmpcfg)
		goto error;

	if (strcmp(tmpcfg.message, cfg.message) != 0
			|| tmpcfg.version != cfg.version)
	{
		errno = EBADF;
		goto error; // bad version
	}
	if (FileRead(fh, &bcount, sizeof bcount) != sizeof bcount)
		goto error;
	tsize = bcount * sizeof *btable;
	if (!(btable = (BINDING *)malloc(tsize)))
	{
		errno = tsize? ENOMEM: EBADF;
		goto error;
	}
	if (FileRead(fh, btable, tsize) != tsize)
		goto error;
	while (FileRead(fh, indexAndLength, sizeof indexAndLength) == sizeof indexAndLength)
	{
		if (indexAndLength[0] >= ELEMCOUNT(mtable))
		{
			errno = EBADF;
			goto error;
		}
		tsize = indexAndLength[1] * sizeof *mbuffer;
		if (!(mbuffer = (unsigned short *)malloc(tsize)))
		{
			errno = ENOMEM;
			goto error;
		}
		if (FileRead(fh, mbuffer, tsize) != tsize)
		{
			free(mbuffer);
			goto error;
		}
		free(mtable[indexAndLength[0]]);
		mtable[indexAndLength[0]] = mbuffer;
	}
	FileClose(fh);
	cfg = tmpcfg;
	bindingTable = btable;
	bindingCount = bcount;
	for (i = 0; i < ELEMCOUNT(mtable); i++)
	{
		free(macroTable[i]);
		macroTable[i] = mtable[i];
	}
	for (i = 1; i < bindingCount; i++)
		assert(KeyData(bindingTable[i].key) > KeyData(bindingTable[i - 1].key) || KeyData(bindingTable[i].key) == KeyData(bindingTable[i - 1].key) && bindingTable[i].key > bindingTable[i - 1].key);

	return true;

error:
	FileClose(fh);
	free(btable);
	for (i = 0; i < ELEMCOUNT(mtable); i++)
		free(mtable[i]);

	for (i = 0; i < functionCount; i++)
		for (k = 0; k < 2; k++)
			if (key = k? functionTable[i].defaultKeyB: functionTable[i].defaultKey)
			{
				assert(!IsDataKey(key) && !(key >= (ALT|'0') && key <= (ALT|'9')));
				if ((j = FindBinding(key, true)) < 0)
				{
					errno = ENOMEM;
					break;
				}
				bindingTable[j].type = FUNCTIONKEY;
				bindingTable[j].data = i;
			}

	for (i = 0; i < ELEMCOUNT(mtable) && i < 10; i++)
	{
		key = DCTRL + '0' + i;
		if ((j = FindBinding(key, true)) < 0)
		{
			errno = ENOMEM;
			break;
		}
		bindingTable[j].type = MACROKEY;
		bindingTable[j].data = i;
	}
	if ((j = FindBinding(F4, true)) < 0)
		errno = ENOMEM;
	else
	{
		bindingTable[j].type = MACROKEY;
		bindingTable[j].data = 0;
	}

	for (i = 1; i < bindingCount; i++)
		assert(KeyData(bindingTable[i].key) > KeyData(bindingTable[i - 1].key)
			|| KeyData(bindingTable[i].key) == KeyData(bindingTable[i - 1].key)
			&& bindingTable[i].key > bindingTable[i - 1].key);

	return false;
}

/*--------------------------------------------------------------------------
 */
bool WriteConfig()
{
	unsigned short **mp, indexAndLength[2], bcount = (unsigned short)bindingCount;
	int fh;
	bool retcode = true;
	if ((fh = FileCreate(FileCfgName(), cfgFileMode)) == FIO_INVALID_HANDLE)
		return false;
	if (FileWrite(fh, &cfg, sizeof cfg) != sizeof cfg
			|| FileWrite(fh, &bcount, sizeof bcount) != sizeof bcount
			|| FileWrite(fh, bindingTable, bcount * sizeof *bindingTable)
					!= bcount * sizeof *bindingTable)
		retcode = false;
	else
		for (mp = macroTable; mp < macroTable + ELEMCOUNT(macroTable); mp++)
			if (*mp)
			{
				indexAndLength[0] = (unsigned short)(mp - macroTable);
				indexAndLength[1] = MacroLen(*mp);
				if (FileWrite(fh, indexAndLength, sizeof indexAndLength) != sizeof indexAndLength
						|| FileWrite(fh, *mp, indexAndLength[1] * sizeof **mp) != indexAndLength[1] * sizeof **mp)
				{
					retcode = false;
					break;
				}
			}
	FileClose(fh);
	return retcode;
}

/*--------------------------------------------------------------------------
 */
void ExitConfig()
{
	free(bindingTable);
	bindingTable = 0;
	bindingCount = 0;
	for (int i = 0; i < ELEMCOUNT(macroTable); i++)
	{
		free(macroTable[i]);
		macroTable[i] = 0;
	}
}

/*===================================================================*/
