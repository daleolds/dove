#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include "pvideo.h"
#include "duiwin.h"
#include "pkeybrd.h"
#include "duimenu.h"

static char barSpace[] = "    ", space[] = " ";

/*-----------------------------------------------------
 */
int InitMenus(void) { return InitKeys() && InitWindows(); }
void ExitMenus(void) { ExitKeys(); ExitWindows(); }


/*-----------------------------------------------------------------------------
 *
 */
enum MENUDISPLAY {INACTIVE, ACTIVE, SUBACTIVE};

static void WriteMenu(MENU *m, enum MENUDISPLAY flag)
{
	int i;
	Window *oldWindow = selectedWindow;
	SelectWindow(m->window);
	WriteBorder(m->title, m->borderType,
	  flag == ACTIVE? m->accentAttrib: m->normalAttrib);
	WSetCursor(0, 0);
	WSetAttribute(m->normalAttrib);
	if (m->flags & VERTICAL)
		for (i = 0; i < m->optionCount; i++)
		{
			WSetAttribute(i != m->selection || flag == INACTIVE?
					m->normalAttrib:
					flag == ACTIVE? m->selectAttrib: m->accentAttrib);
			WriteString(space);
			WriteLine(m->options[i].name);
		}
	else
	{
		WriteString(space);
		for (i = 0; i < m->optionCount; i++)
		{
			WSetAttribute(i != m->selection || flag == INACTIVE?
					m->normalAttrib:
					flag == ACTIVE? m->selectAttrib: m->accentAttrib);
			WriteString(m->options[i].name);
			WSetAttribute(m->normalAttrib);
			WriteString(barSpace);
		}
		WriteLine(space);
	}
	if (m->infoFunction != 0)
		(m->infoFunction)();
	SelectWindow(oldWindow);
}

/* -------------------------------------------------------------------
 */
int CreateMenu(MENU *m)
{
	unsigned i, h, w;
	if (m->flags & VERTICAL)
	{
		for (h = w = 0; h < m->optionCount; h++)
			if ((i = strlen(m->options[h].name)) > w)
				w = i;
	}
	else
	{
		for (w = 0, i = 0; i < m->optionCount; i++)
			w += strlen(m->options[i].name) + sizeof(barSpace) - 1;
		w -= sizeof(barSpace) - 1;
		h = 1;
	}
	if ((i = (m->title? strlen(m->title): 0) + 6) > w)
		w = i;
	if ((m->window = CreateWindow(m->row, m->col, h, w + 2, m->flags)) == 0)
		return(-1);
	WriteMenu(m, INACTIVE);
	return 0;
}

/* -------------------------------------------------------------------
 */
void DeleteMenu(MENU *m)
{
	DeleteWindow(m->window);
}


/*-----------------------------------------------------------------------------
 */
static int ProcessSelection(MENU *m)
{
	int i, retCode;
	unsigned subRow, subCol;
	MENUOPTION opt;
	opt = m->options[m->selection];
	if (opt.subMenu == 0 && opt.function == 0)
		retCode = opt.value;
	else
	{
		WriteMenu(m, SUBACTIVE);
		if (m->flags & VERTICAL)
		{
			subRow = m->row + m->selection;
			subCol = m->col + 4;
		}
		else
		{
			for (retCode = 1, i = 0; i < m->selection; i++)
				retCode += strlen(m->options[i].name) + sizeof(barSpace) - 1;
			subRow = m->row;
			subCol = m->col + retCode - 1;
		}
		if (opt.subMenu == 0)
			retCode = opt.function(subRow, subCol);
		else
		{
			if (opt.subMenu->flags & AUTOLOCATE)
			{
				opt.subMenu->row = subRow + 1;
				if (!(opt.subMenu->flags & BORDERLESS))
					opt.subMenu->row++;
				opt.subMenu->col = subCol;
			}
			if ((retCode = CreateMenu(opt.subMenu)) != -1)
			{
				retCode = ProcessMenu(opt.subMenu);
				DeleteMenu(opt.subMenu);
			}
		}
	}
	return retCode;
}


/* -------------------------------------------------------------------
 */
int ProcessMenu(MENU *m)
{
	int retCode = 0, c, subMenuOpen = 0;
	unsigned i, oldSelection = m->selection;
	for (;;)
	{
		if (!KeyHit(0))
			WriteMenu(m, ACTIVE);
		if (IsDataKey(c = GetKey()))
			for (i = 0; i < m->optionCount; i++)
			{
				if (toupper((int)m->options[i].name[0]) == toupper(c))
				{
					m->selection = i;
					if ((retCode = ProcessSelection(m)) != -1 && retCode != -2)
						goto ExitMenu;
					break;
				}
			}
		else if (c == ENTER || c == NPENTER)
		{
			if ((retCode = ProcessSelection(m)) != -1 && retCode != -2)
				goto ExitMenu;
		}
		else if (m->flags & VERTICAL?
				c == NP4 || c == NP6 || c == LEFT || c == RIGHT:
				c == NP8 || c == NP2 || c == UP || c == DOWN)
		{
			UngetKey(c);
			m->selection = oldSelection;
			retCode = -2;
			goto ExitMenu;
		}
		else
		{
			switch(c)
			{
				case ESC:
					m->selection = oldSelection;
					retCode = -1;
					goto ExitMenu;
				case NP8:
				case NP4:
				case UP:
				case LEFT:
					m->selection = m->selection == 0? m->optionCount - 1:
					  m->selection - 1;
					break;
				case NP2:
				case NP6:
				case RIGHT:
				case DOWN:
					m->selection = (m->selection + 1) % m->optionCount;
			}
			if (subMenuOpen && m->options[m->selection].subMenu != 0)
			{
				if ((retCode = ProcessSelection(m)) != -1 && retCode != -2)
					goto ExitMenu;
			}
		}
		subMenuOpen = (retCode == -2);
	}
ExitMenu:
	WriteMenu(m, INACTIVE);
	return(retCode);
}

/* ========================================================================= */
