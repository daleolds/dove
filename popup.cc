#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pvideo.h>
#include <duiwin.h>
#include <pkeybrd.h>
#include <common.h>

/*--------------------------------------------------------------------------
 */
static void DisplayMacros(Window *helpWindow)
{
	int mac, row, i, j;
	WriteBorder("Recorded Macros - Press Help to exit, move Backward by Pages",
			DOUBLE, attrib[HELPBORDER]);
	for (mac = 0, row = 1; row < helpWindow->high && mac < MACROCOUNT; mac++)
	{
		WSetCursor(row++, 1);
		for (i = 0; i < bindingCount; i++)
			if (bindingTable[i].type == MACROKEY
					&& bindingTable[i].data == mac)
			{
				WriteString(KeyName(bindingTable[i].key, 0));
				WriteString(": ");
				if (macroTable[mac])
					for (j = 0; macroTable[mac][j] != NONKEY; j++)
						WriteString(KeyName(macroTable[mac][j], 1));
				break;
			}
	}
}

/*--------------------------------------------------------------------------
 */
static void DisplayCopyright(void)
{
	static const char *helpOnHelpStrings[] =
	{
		"컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴컴",
		"Note on Key Names:",
		"In the following screens the key names are usually the same as the",
		"names printed on the keys of most keyboards. The most notable",
		"exception to this is that the keys on the numeric pad are designated",
		"with the letters NP. Thus \"NP0\" is the num pad 0 key, also labeled",
		"\"Ins\". The key named \"Insert\" refers to a key near the cursor key pad",
		"on the 101 key keyboard. The cursor or arrow keys are refered to as",
		"Up, Down, Left and Right. The Backspace key is BSP.",
		0
	};
	int i, j;
	WriteBorder("Press Help to exit, Page forward for key bindings and macros",
			DOUBLE, attrib[HELPBORDER]);
	for (i = 0; copyrightStrings[i] != 0; i++)
	{
		WSetCursor(i + 1, 2);
		WriteString(copyrightStrings[i]);
	}
	for (j = 0; helpOnHelpStrings[j] != 0; j++)
	{
		WSetAttribute(attrib[j < 2? HELPBORDER: HELPTEXT]);
		WSetCursor(i + j + 1, 2);
		WriteString(helpOnHelpStrings[j]);
	}
}

/*--------------------------------------------------------------------------
 */
static int DisplayHelpScreen(int screenNumber, Window *helpWindow)
{
	int j, func, centerCol, curCol, curRow, categoryIndex;
	unsigned char leftThisCategory;
	const char *title;
	centerCol = helpWindow->wide/2;
	SelectWindow(helpWindow);
	WSetAttribute(attrib[HELPTEXT]);
	ClearWindow();
	if (screenNumber == 0)
	{
		DisplayCopyright();
		return 0;
	}
	screenNumber--;
	title = "Key Bindings - Press Help to exit, move Forward or Backward by pages";
	for (func = leftThisCategory = 0, categoryIndex = -1, curCol = curRow = 1;;
			leftThisCategory--, func++)
	{
		while (leftThisCategory == 0)
		{
			if (curRow + 4 >= helpWindow->high)
			{
				if (curCol != centerCol)
					curCol = centerCol;
				else if (screenNumber > 0)
				{
					screenNumber--;
					curCol = 1;
				}
				else
					goto out;
				curRow = 1;
			}
			else if (curRow > 1)
				curRow++;
			if (++categoryIndex == CT_COUNT)
				goto out;
			leftThisCategory = categories[categoryIndex].entries;
			if (screenNumber == 0)
			{
				WSetCursor(curRow, curCol - 4 
				  + (centerCol - strlen(categories[categoryIndex].title))/2);
				WSetAttribute(attrib[HELPBORDER]);
				WriteString(categories[categoryIndex].title);
				WSetAttribute(attrib[HELPTEXT]);
			}
			curRow++;
			func = 0;
		}
		while (functionTable[func].category != categoryIndex)
			if (++func == functionCount)
				return -1;

		for (int foundOne = j = 0; j <= bindingCount; j++)
			if (j == bindingCount && !foundOne
					|| bindingTable[j].type == FUNCTIONKEY
					&& bindingTable[j].data == func)
			{
				foundOne = 1;
				if (screenNumber == 0)
				{
					WSetCursor(curRow, curCol);
					WriteString(functionTable[func].description);
					if (j != bindingCount)
					{
						WriteString(", ");
						WriteString(KeyName(bindingTable[j].key, 0));
					}
				}
				if (++curRow >= helpWindow->high)
				{
					if (curCol != centerCol)
						curCol = centerCol;
					else if (screenNumber > 0)
					{
						screenNumber--;
						curCol = 1;
					}
					else
						goto out;
					curRow = 1;
				}
			}
	}
out:
	if (screenNumber > 0)
		DisplayMacros(helpWindow);
	else
		WriteBorder(title, DOUBLE, attrib[HELPBORDER]);
	return screenNumber < 2;
}

/*--------------------------------------------------------------------------
 */
int HelpKey(void)
{
	static int helpScreenNumber = 0;
	int ret;
	BINDING cmd;
	Window *helpWindow;
	if (!(helpWindow = CreateWindow(1, 1, screen->high - 3,
			(screen->wide > 80? 80: screen->wide) - 2, DESTRUCTIVE)))
		goto error;
	SetCursorType(CURSOR_NONE);
	if ((ret = DisplayHelpScreen(helpScreenNumber, helpWindow)) == -1)
		goto error;
	for (;;)
	{
		cmd = BindKey(GetCommand());
		if (cmd.type == FUNCTIONKEY)
		{
			if (functionTable[cmd.data].nonr == CancelKey
					|| functionTable[cmd.data].nonr == HelpKey)
			{
				DeleteWindow(helpWindow);
				MLErase();
				return 1;
			}
			if (functionTable[cmd.data].rept == ForwPageKey)
				helpScreenNumber++;
			else if (functionTable[cmd.data].rept == BackPageKey
					&& helpScreenNumber > 0)
				helpScreenNumber--;
			else
				continue;
			if ((ret = DisplayHelpScreen(helpScreenNumber, helpWindow)) == -1)
				goto error;
			if (ret == 0 && helpScreenNumber > 0)
				helpScreenNumber--;
		}
	}
error:
	if (helpWindow)
		DeleteWindow(helpWindow);
	MLWrite("Could not create help window.");
	return 0;
}

/*===================================================================*/
