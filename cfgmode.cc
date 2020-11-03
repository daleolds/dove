#include <stdio.h>
#include <string.h>
#include <pkeybrd.h>
#include <pvideo.h>
#include <duiwin.h>
#include <duimenu.h>
#include <cfgcommo.h>

/*-----------------------------------------------------------------------------
 * toggle modes menu
 */
static MENUOPTION toggleOptions[] =
{
	{"On", 1, 0, 0},
	{"Off", 0, 0, 0},
};

void ToggleMessage(void)
{
	Help("", "This mode can be initially set to On or Off.");
}

static MENU toggleMenu =
{
	VERTICAL, 0, 5, 10, NORMVID, REVVID, BOLDVID, DOUBLESIDES,
	sizeof(toggleOptions)/sizeof(toggleOptions[0]), toggleOptions, 0, 0,
	ToggleMessage
};

/*-----------------------------------------------------------------------------
 *
 */
int Modes(unsigned row, unsigned col)
{
	int retCode, i, bit;
	for (i = 0, bit = 1; i < modeMenu.selection; i++)
		bit <<= 1;
	toggleMenu.selection = (cfg.modes & bit)? 0: 1;
	toggleMenu.row = row;
	toggleMenu.col = col + strlen(modeMenu.options[modeMenu.selection].name) - 1;
	CreateMenu(&toggleMenu);
	retCode = ProcessMenu(&toggleMenu);
	DeleteMenu(&toggleMenu);
	if (retCode == 1)
		cfg.modes |= bit;
	else if (retCode == 0)
		cfg.modes &= ~bit;
	return -1;
}

/*-----------------------------------------------------------------------------
 */
int Numbers(unsigned row, unsigned col)
{
	char str[10], firstKey;
	unsigned value, key;
	Window *w;
	Help("", (char *)(modeMenu.selection - MODECOUNT == WORDWRAPCOL?
			"Enter number of default column to wrap words, 0 sets word wrap off.":
			"Enter the default tab size, it must be greater than 0."));
	w = CreateWindow(row, col + strlen(modeMenu.options[modeMenu.selection].name) - 1,
	  1, 8, 0);
	WriteBorder(0, SINGLE, NORMVID | VBRIGHT);
	value = cfg.number[modeMenu.selection - MODECOUNT];
	firstKey = 1;
	SetCursor(w->row, w->col + 7);
	for (;;)
	{
		WSetCursor(0, 0);
		sprintf(str, " %6u ", value);
		WriteString(str);
		switch(key = GetKey())
		{
			case ENTER:
				if (value == 0 && modeMenu.selection - MODECOUNT == TABSIZE)
					break;
				cfg.number[modeMenu.selection - MODECOUNT] = value;
			case ESC:
				DeleteWindow(w);
				SetCursorType(CURSOR_NONE);
				return(-1);
			case BSP:
				if (value > 0)
					value /= 10;
				break;
			default:
				if (key >= '0' && key <= '9')
				{
					key -= '0';
					value = firstKey? key: value * 10 + key;
				}
		}
		firstKey = 0;
	}
}

/*===========================================================================*/
