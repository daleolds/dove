#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <pkeybrd.h>
#include <pvideo.h>
#include <duiwin.h>
#include <duimenu.h>
#include <cfgcommo.h>

static const char *usageStrings[] =
{
"",
"    Usage: dove filespec ...",
"",
"Filespecs may contain wildcards, for example:",
"    dove *.c *.h *.txt",
"",
"Press Esc to exit, any other key enters configuration mode.",
0
};

const char *copyrightStrings[] =
{
"dove v" VERSION " -- a text editor",
"Copyright (c) 1989 - 2013 Dale Olds. All rights reserved.",
"",
"If you find dove useful, feel free to use it. Please send",
"comments and suggestions to:",
"",
"dove@oldtuba.com",
"",
0
};

/*-----------------------------------------------------------------------------
 */
int Confirm(int row, int col, const char *title, int selection)
{
	static MENUOPTION confirmOptions[] =
	{
		{"Yes", 1, 0, 0},
		{"No", 0, 0, 0},
	};
	static MENU confirmMenu =
	{
		VERTICAL, 0, 0, 0, NORMVID, REVVID, BOLDVID, DOUBLESIDES,
		sizeof(confirmOptions)/sizeof(confirmOptions[0]), confirmOptions,
		0, 0, 0
	};

	int retCode;
	Help("", "Use arrow keys and ENTER or first letter of option to select, ESC to cancel.");
	confirmMenu.selection = selection? 0: 1;
	confirmMenu.row = row;
	confirmMenu.col = col;
	confirmMenu.title = title;
	CreateMenu(&confirmMenu);
	retCode = ProcessMenu(&confirmMenu);
	DeleteMenu(&confirmMenu);
	return(retCode);
}

/*-----------------------------------------------------------------------------
 */
static const char *attribMessages[] =
{
	"text in a view window",
	"cursor over text in a view window",
	"marked text in a view window",
	"cursor over marked text",
	"a view window border",
	"the line overflow marker in a view window",
	"the message line",
	"cursor on message line",
	"the line and column display",
	"the time display",
	"the help window border",
	"text in the help window",
	"the delete stack window border",
	"text in the delete stack window",
	"the delete stack Selection",
};

/*-----------------------------------------------------------------------------
 * colors submenu
 */
static MENUOPTION colorOptions[] =
{
	{"View Text", 0, 0, Colors},
	{"View Text Cursor", 0, 0, Colors},
	{"View Marked Block", 0, 0, Colors},
	{"View Marked Cursor", 0, 0, Colors},
	{"View Border", 0, 0, Colors},
	{"View Line Overflow Marker", 0, 0, Colors},
	{"Message Line", 0, 0, Colors},
	{"Message Line Cursor", 0, 0, Colors},
	{"Line and Column Display", 0, 0, Colors},
	{"Time Display", 0, 0, Colors},
	{"Help Window Border", 0, 0, Colors},
	{"Help Window Display", 0, 0, Colors},
	{"Delete Stack Border", 0, 0, Colors},
	{"Delete Stack Text", 0, 0, Colors},
	{"Delete Stack Selection", 0, 0, Colors},
};

void ColorMessage(void)
{
	Message("Modify the color used for %s.",
			attribMessages[colorMenu.selection]);
}

MENU colorMenu =
{
	VERTICAL|AUTOLOCATE, 0, 0, 0, NORMVID, REVVID, BOLDVID, DOUBLESIDES,
	sizeof(colorOptions)/sizeof(colorOptions[0]), colorOptions, 0, 0,
	ColorMessage
};

/*-----------------------------------------------------------------------------
 * modes submenu
 */
static MENUOPTION modeOptions[] =
{
	{"Auto Indent", 0, 0, Modes},
	{"Case Sensitive Searchs", 0, 0, Modes},
	{"Create BAK File", 0, 0, Modes},
	{"Insert Characters", 0, 0, Modes},
	{"Real Tabs", 0, 0, Modes},
	{"Show Control Chars", 0, 0, Modes},
	{"Enter CR-LF", 0, 0, Modes},
	{"Enable Terminal Cursor", 0, 0, Modes},
	{"Tab Size", 0, 0, Numbers},
	{"Word Wrap Column", 0, 0, Numbers},
};

void ModeMessage(void)
{
	Help("", "Select one of these options to modify the default setting for that mode.");
}


MENU modeMenu =
{
	VERTICAL|AUTOLOCATE, 0, 0, 0, NORMVID, REVVID, BOLDVID, DOUBLESIDES,
	sizeof(modeOptions)/sizeof(modeOptions[0]), modeOptions, 0, 0, ModeMessage
};

/*-----------------------------------------------------------------------------
 * main menu
 */
static MENUOPTION mainOptions[] =
{
	{"Key Bindings", 0, 0, KeyBindings},
	{"Colors", 0, &colorMenu, 0},
	{"Mode Defaults", 0, &modeMenu, 0},
	{"Exit", 0, 0, 0},
};

static const char *mainMessages[] =
{
	"This option allows you to reconfigure the keyboard.",
	"This option allows you to choose the attributes for various types of text.",
	"This option allows you to set the default values of various modes.",
	"Exit this program.  You will be asked if you want to save any changes."
};

void MainMessage(void)
{
	Help(mainMessages[mainMenu.selection],
"Use arrow keys and ENTER or first letter of option to select, ESC to cancel."
		);
}

MENU mainMenu =
{
	DESTRUCTIVE, 0, 1, 1, NORMVID, REVVID, BOLDVID, DOUBLESIDES,
	sizeof(mainOptions)/sizeof(mainOptions[0]), mainOptions, 0, 0, MainMessage
};

/*-----------------------------------------------------------------------------
 */
static Window *msgLine;

void Message(const char *fmt, ...)
{
	char msg[128];
	va_list argptr;

	va_start(argptr, fmt);
	vsprintf(msg, fmt, argptr);
	va_end(argptr);
	SelectWindow(msgLine);
	WSetCursor(0, 0);
	WriteLine("");
	WriteLine(msg);
}

void Help(const char *line0, const char *line1)
{
	SelectWindow(msgLine);
	WSetCursor(0, 0);
	WriteLine(line0);
	WriteLine(line1);
}

/*-----------------------------------------------------------------------------
 */
static int ShowCopyright(void)
{
	Window *w;
	int i, j;
	if ((w = CreateWindow(2, 2, 20, 75, 0)) == 0)
		return 0;
	WriteBorder(0, DOUBLE, NORMVID);
	WSetAttribute(BOLDVID);
	for (i = 0, j = 2; copyrightStrings[i] != 0; i++, j++)
	{
		WSetCursor(j, 2);
		WriteString(copyrightStrings[i]);
	}
	for (i = 0; usageStrings[i] != 0; i++, j++)
	{
		WSetCursor(j, 2);
		WriteString(usageStrings[i]);
	}
	WSetCursor(w->high - 2, 2);
	i = GetKey();
	DeleteWindow(w);
	return i != ESC;
}

/*-----------------------------------------------------------------------------
 */
void ConfigMain()
{
	unsigned oldRow, oldCol;
	int retCode;
	Window *oldScreen;
	ReadConfig();
	SetActiveModes();
	if (!InitMenus())
	{
		puts("Failure to initialize menu system.");
		return;
	}
	oldScreen = CreateWindow(0, 0, videoRows, videoCols, BORDERLESS);
	GetCursor(&oldRow, &oldCol);
	SetCursorType(CURSOR_NONE);
	ClearWindow();
	if (ShowCopyright())
	{
		msgLine = CreateWindow(videoRows - 2, 0, 2, videoCols, BORDERLESS);
		WSetAttribute(HELPVID);
		CreateMenu(&mainMenu);
		do
		{
			ProcessMenu(&mainMenu);
			retCode = Confirm(8, 26, "Save configuration file?", 1);
		} while (retCode != 0 && retCode != 1);
		while (retCode && !WriteConfig())
			if (Confirm(8, 26, "Error writing configuration file, retry?", 1) == 0)
				break;
		DeleteMenu(&mainMenu);
		DeleteWindow(msgLine);
	}
	DeleteWindow(oldScreen);
	SetCursor(oldRow, oldCol);
	ExitConfig();
	ExitMenus();
}

/*===========================================================================*/
