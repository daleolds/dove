#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <pkeybrd.h>
#include <pvideo.h>
#include <duiwin.h>
#include <duimenu.h>
#include <cfgcommo.h>

#define WIDTH (MAXKEYNAME + MAXFUNCTIONNAME + 2)

/*--------------------------------------------------------------------------
 */
static const char *FunctionName(int i)
{
	static char name[MAXFUNCTIONNAME + 1];
	switch(bindingTable[i].type)
	{
		case NOTBOUND:
			name[0] = 0;
			break;
		case INSCHAR:
			sprintf(name, "Type character '%c'", bindingTable[i].data);
			break;
		case MACROKEY:
			sprintf(name, "Macro Key %u", bindingTable[i].data);
			break;
		case FUNCTIONKEY:
			return functionTable[bindingTable[i].data].description;
	}
	return name;
}

/*-----------------------------------------------------------------------------
 *
 */
static unsigned CharToTypeOrMacroNum(int doMacro)
{
	const char *prompt = doMacro? "Enter macro number (0-9): ": "Enter character: ";
	unsigned key;
	Window *w;
	w = CreateWindow(8, 55, 1, strlen(prompt) + 2, 0);
	WriteBorder(0, SINGLE, NORMVID | VBRIGHT);
	SetCursor(w->row, w->col + strlen(prompt));
	WSetCursor(0, 0);
	WriteString((char *)prompt);
	for (;;)
	{
		if (doMacro)
			Help("Enter the number of this macro.", "It must be from 0 to 9.");
		else
			Help(
"Press any alphanumeric key, or hold down the ALT key, type the character's",
"decimal value on the numeric key pad, let up the ALT key."
			);
		key = GetKey();
		if (key == ESC || doMacro && key >= '0' && key <= '9'
				|| !doMacro && IsDataKey(key))
		{
			DeleteWindow(w);
			SetCursorType(CURSOR_NONE);
			return key;
		}
	}
}

/*-----------------------------------------------------------------------------
 */

#define FSELECTIONS (functionCount + 3)
 
static void SelectFunction(BINDING *cmd)
{
	static const char *specialFunctions[] =
	{
		"(No Binding)",
		"Type a character",
		"Macro Key"
	};
	unsigned base, i, selection, key;
	Window *w;
	BINDING bndg = *cmd;

	if ((w = CreateWindow(4, 10 + WIDTH - 3, videoRows - 7, MAXFUNCTIONNAME + 2, 0)) == 0)
		return;
 	WriteBorder("Functions", DOUBLE, BOLDVID);
	ClearWindow();
	selection = base = 0;
	switch(bndg.type)
	{
		case NOTBOUND: selection = 0; break;
		case INSCHAR: selection = 1; break;
		case MACROKEY: selection = 2; break;
		case FUNCTIONKEY: selection = bndg.data + 3;
	}
	for (;;)
	{
		Help(
"Navigate with UP, DOWN, PGUP, PGDN, HOME, END or first letter of function name.",
"\"(No Binding)\" deletes binding, \"Type a character\" to bind to any character."
			);
		if (selection < base)
			base = selection;
		else if (selection - base >= w->high)
			base = selection - (w->high - 1);
		SelectWindow(w);
		for (i = base; i < base + w->high; i++)
		{
			WSetCursor(i - base, 1);
			WSetAttribute(i == selection? REVVID: NORMVID);
			WriteLine(i < 3? specialFunctions[i]: functionTable[i - 3].description);
		}
		switch(key = GetKey())
		{
			case ESC:
				goto abort;
			case ENTER:
			case NPENTER:
				bndg.data = 0;
				switch (selection)
				{
					case 0: bndg.type = NOTBOUND; goto done;
					case 1:
					case 2:
					 	WriteBorder("Functions", DOUBLE, NORMVID);
						WSetCursor(selection - base, 1);
						WSetAttribute(BOLDVID);
						WriteLine(selection < 3? specialFunctions[selection]:
								functionTable[selection - 3].description);
						i = CharToTypeOrMacroNum(selection == 2);
						SelectWindow(w);
					 	WriteBorder("Functions", DOUBLE, BOLDVID);
						if (i == ESC)
							break;
						bndg.type = selection == 2? MACROKEY: INSCHAR;
						bndg.data = selection == 2? i - '0': i;
						goto done;
					default: bndg.type = FUNCTIONKEY;
						bndg.data = selection - 3;
						goto done;
				}
				break;
			case NP2:
			case DOWN:
				if (selection + 1 < FSELECTIONS)
					selection++;
				break;
			case NP8:
			case UP:
				if (selection > 0)
					selection--;
				break;
			case NP3:
			case PGDN:
				selection += w->high;
				if (selection > FSELECTIONS - 1)
					selection = FSELECTIONS - 1;
				base += w->high;
				if (base > FSELECTIONS - w->high)
					base = FSELECTIONS - w->high;
				break;
			case NP9:
			case PGUP:
				selection = selection < w->high? 0: selection - w->high;
				base = base < w->high? 0: base - w->high;
				break;
			case NP7:
			case HOME:
				selection = base = 0;
				break;
			case NP1:
			case END:
				selection = FSELECTIONS - 1;
				break;
			default:
				if (IsDataKey(key))
				{
					for (i = (selection + 1) % FSELECTIONS; i != selection;
							i = (i + 1) % FSELECTIONS)
						if (i < 3)
						{
							if (toupper(*specialFunctions[i]) ==
									toupper(KeyData(key)))
							{
								selection = i;
								break;
							}
						}
						else if (toupper(*functionTable[i - 3].description) ==
								toupper(KeyData(key)))
						{
							selection = i;
							break;
						}
				}
		}
	}


done:
	*cmd = bndg;
abort:
	DeleteWindow(w);
}

/*-----------------------------------------------------------------------------
 */
void CompressBindingTable()
{
	for (int i = 0; i < bindingCount; )
		if (bindingTable[i].type != NOTBOUND)
			i++;
		else
			memmove(&bindingTable[i], &bindingTable[i + 1],
					(--bindingCount - i) * sizeof *bindingTable);
}

/*-----------------------------------------------------------------------------
 */
int KeyBindings(unsigned row, unsigned col)
{
	int base, i, selection, key;
	char line[WIDTH + 1];
	Window *w;

	(void)row;
	(void)col;

	if ((w = CreateWindow(4, 10, videoRows - 7, WIDTH + 2, 0)) == 0)
		return(-1);
 	WriteBorder("Bindings", DOUBLE, BOLDVID);
	ClearWindow();
	for (selection = base = 0;;)
	{
		Help(
"Use UP, DOWN, PGUP, PGDN, HOME, END or press 'k' and then the key to find.",
"NP0-NP9 refer to num pad keys. Insert, Home, etc, are \"101 keyboard\" keys."
			);
		SelectWindow(w);
		for (i = base; i < base + w->high; i++)
		{
			WSetCursor(i - base, 1);
			WSetAttribute(i == selection? REVVID: NORMVID);
			if (i >= bindingCount)
				line[0] = 0;
			else
				sprintf(line, "%-*s  %-*s",
						MAXKEYNAME, KeyName(bindingTable[i].key, 0),
						MAXFUNCTIONNAME, FunctionName(i));
			WriteLine(line);
		}
		switch(key = GetKey())
		{
			case REFRESHDISPLAYKEY:
			case ESC:
				DeleteWindow(w);
				CompressBindingTable();
				return(-1);
			case ENTER:
			case NPENTER:
				if (!bindingTable)
					break;
			 	WriteBorder("Bindings", DOUBLE, NORMVID);
				WSetCursor(selection - base, 1);
				WSetAttribute(BOLDVID);
				sprintf(line, "%-*s  %-*s", MAXKEYNAME,
						KeyName(bindingTable[selection].key, 0), MAXFUNCTIONNAME,
						FunctionName(selection));
				WriteLine(line);
				SelectFunction(&bindingTable[selection]);
				SelectWindow(w);
			 	WriteBorder("Bindings", DOUBLE, BOLDVID);
				break;
			case NP2:
			case DOWN:
				if (selection + 1 < bindingCount)
					selection++;
				break;
			case NP8:
			case UP:
				if (selection > 0)
					selection--;
				break;
			case NP3:
			case PGDN:
				selection += w->high;
				if (selection > bindingCount - 1)
					selection = bindingCount - 1;
				base += w->high;
				if (base > bindingCount - w->high)
					base = bindingCount - w->high;
				break;
			case NP9:
			case PGUP:
				selection = selection < w->high? 0: selection - w->high;
				base = base < w->high? 0: base - w->high;
				break;
			case NP7:
			case HOME:
				selection = base = 0;
				break;
			case NP1:
			case END:
				selection = bindingCount - 1;
				break;
			case NP0:
			case INS:
			case 'k':
			case 'K':
				Help(
"Now press the key to to find. If you press a key combination and nothing",
"happens then that key combination cannot be bound. Try another."
					);
				key = GetKey();
			default:
				if (IsDataKey(key))
				{
					Message("Key is regular character key '%c', press a key to continue", key);
					GetKey();
				}
				else if (key >= (ALT|'0') && key <= (ALT|'9'))
				{
					Message("Key is repeat count key %d, press a key to continue", KeyData(key) - '0');
					GetKey();
				}
				else if (key == REFRESHDISPLAYKEY)
				{
					DeleteWindow(w);
					CompressBindingTable();
					return(-1);
				}
				else if (key == NONKEY || key == ERRORKEY)
				{
					Message("key is operational value %s", KeyName(key, 0));
					GetKey();
				}
				else if ((i = FindBinding(key, true)) < 0)
				{
					Message(i == -2? "Insufficient memory, press a key":
						"Key code 0x%x is unknown, press a key", key);
					GetKey();
				}
				else
					selection = i;
				break;
		}
		if (selection < base)
			base = selection;
		else if (selection - base >= w->high)
			base = selection - (w->high - 1);
	}
}

/*===========================================================================*/

