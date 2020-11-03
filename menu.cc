#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pvideo.h>
#include <duiwin.h>
#include <pkeybrd.h>
#include <duimenu.h>
#include <common.h>

/*-----------------------------------------------------------------------------
 */
#define ENTRYWIDTH (MAXFUNCTIONNAME + MAXKEYNAME)

static char *MakeMenuEntry(int funcIndex)
{
	char *s, *name;
	int i, desclen = strlen(functionTable[funcIndex].description);
	int len, keynameslen = 0;
	if ((s = (char *)malloc(ENTRYWIDTH + 1)) == 0)
		return 0;
	sprintf(s, "%-*.*s", ENTRYWIDTH, ENTRYWIDTH, functionTable[funcIndex].description);
	for (i = 0; i < bindingCount; i++)
	{
		if (bindingTable[i].type == FUNCTIONKEY
				&& bindingTable[i].data == funcIndex)
		{
			name = KeyName(bindingTable[i].key, 0);
			len = strlen(name);
			if (len + keynameslen + desclen + 2 <= ENTRYWIDTH)
			{
				if (keynameslen)
					keynameslen++;
				keynameslen += len;
				memcpy(s + ENTRYWIDTH - keynameslen, name, len);
			}
		}
	}
	return s;
}

/*-----------------------------------------------------------------------------
 */
static MENU *FreeMenuSystem(MENU *m)
{
	int i;
	if (m->options != 0)
	{
		for (i = 0; i < m->optionCount; i++)
		{
			if (m->options[i].name != 0)
				free((void *)m->options[i].name);
			if (m->options[i].subMenu != 0)
				FreeMenuSystem(m->options[i].subMenu);
		}
		free(m->options);
	}
	free(m);
	return 0;
}

/*-----------------------------------------------------------------------------
 */
static MENU *InitMenuSystem(void)
{
	MENU *m, *sub;
	MENUOPTION *opt;
	int i, j, func;

	if (!(m = (MENU *)malloc(sizeof(*m))))
		return 0;
	memset(m, 0, sizeof(*m));

	m->flags = DESTRUCTIVE|EDGEBORDER;
	m->optionCount = CT_COUNT;
	m->normalAttrib = attrib[DELETETEXT];
	m->selectAttrib = attrib[DELETEMARK];
	m->accentAttrib = attrib[DELETEBORDER];
	m->borderType = SINGLE;

	if (!(opt = (MENUOPTION *)malloc(sizeof *opt * CT_COUNT)))
		return FreeMenuSystem(m);
	memset(opt, 0, sizeof *opt * CT_COUNT);
	m->options = opt;
	for (i = 0; i < CT_COUNT; i++)
	{
		if (!(m->options[i].name = strdup(categories[i].title))
				|| !(sub = (MENU *)malloc(sizeof *sub)))
			return FreeMenuSystem(m);
		m->options[i].subMenu = sub;
		*sub = *m;
		sub->flags = VERTICAL|AUTOLOCATE;
		sub->optionCount = categories[i].entries;
		if (!(opt = (MENUOPTION *)malloc(sizeof *opt * sub->optionCount)))
			return FreeMenuSystem(m);
		memset(opt, 0, sizeof(*opt) * sub->optionCount);
		sub->options = opt;
		for (j = func = 0; j < sub->optionCount; j++, func++)
		{
			while (functionTable[func].category != i)
				func++;
			if (!(opt[j].name = MakeMenuEntry(func)))
				return FreeMenuSystem(m);
			opt[j].value = func;
		}
	}
	return m;
}

/*-----------------------------------------------------------------------------
 */
int MenuKey(int n)
{
	MENU *m;
	int retCode;

	if (!(m = InitMenuSystem()))
		return 0;
	SetCursorType(CURSOR_NONE);
	CreateMenu(m);
	retCode = ProcessMenu(m);
	DeleteMenu(m);
	FreeMenuSystem(m);
   DisplayViews();
	if (retCode != -1 && retCode != -2)
		retCode = functionTable[retCode].nonr == 0?
			  (*(functionTable[retCode].rept))(n):
			  (*(functionTable[retCode].nonr))();
	return retCode;
}

/*===========================================================================*/
