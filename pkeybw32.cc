#if defined(DOVE_FOR_WIN32)

#define PRINT_SCANS	0

#include <pshpack4.h> /* work around watcom 10.6 bug in wincon.h */
#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
#include <poppack.h> /* work around watcom 10.6 bug in wincon.h */

#if PRINT_SCANS
#include <stdio.h>
#endif

#include "pkeybrd.h"

#define MAX_UNGETS  30

static unsigned keybuf[MAX_UNGETS], *keybp = keybuf;

/* -------------------------------------------------------------------
 * keys which return extended key codes,
 * the extended key code is used as an index
 * into this table.
 */
static unsigned extkeys[] =
{
/* 00 */	BREAK,
/* 01 */	ESC,		'1',		'2',		'3',	'4',		'5',		'6',	'7',
			'8',		'9',		'0',		'-',	'=',		BSP,		TAB,	'Q',
/* 11 */	'W',		'E',		'R',		'T',	'Y',		'U',		'I',	'O',
			'P',		'[',		']',		ENTER,	NONKEY,	'A',	'S',	'D',
/* 21 */	'F',		'G',		'H',		'J',	'K',		'L',		';',	'\'',
			'`',		NONKEY,	'\\',		'Z',	'X',		'C',		'V',	'B',
/* 31 */	'N',		'M',		',',		'.',	'/',		NONKEY,	NPSTAR,	NONKEY,
			NONKEY,	NONKEY,	F1,		F2,	F3,		F4,		F5,	F6,
/* 41 */	F7,		F8,		F9,		F10,	NONKEY,	NONKEY,	NP7,	NP8,
			NP9,		NPMINUS,	NP4,		NP5,	NP6,		NPPLUS,	NP1,	NP2,
/* 51 */	NP3,		NP0,		NPDOT,	F1,	F2,		F3,		F11,	F12,
};

/* -------------------------------------------------------------------
 * special keys, distinguished by scan code.
 */
static struct spkey { unsigned scan, key; } spkeys[] =
{
	{0x011b, ESC},
	{0x0e08, BSP},
	{0x0f09, TAB},
	{0x1c0d, ENTER},
	{0x372a, NPSTAR},
	{0x4a2d, NPMINUS},
	{0x4e2b, NPPLUS},
	{0x532e, NPDOT},
	{0x5230, NP0},
	{0x4f31, NP1},
	{0x5032, NP2},
	{0x5133, NP3},
	{0x4b34, NP4},
	{0x4c35, NP5},
	{0x4d36, NP6},
	{0x4737, NP7},
	{0x4838, NP8},
	{0x4939, NP9},
	{0x0e7f, BSP},
	{0x1c0a, ENTER},
};


/* -------------------------------------------------------------------
 */
static HANDLE hConsoleInput;
static DWORD kbdmode;

#define KBD_TEXTMODE (ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | \
		ENABLE_PROCESSED_INPUT | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT)

int InitKeys(void)
{
	if ((hConsoleInput = GetStdHandle(STD_INPUT_HANDLE)) == INVALID_HANDLE_VALUE
			|| !GetConsoleMode(hConsoleInput, &kbdmode)
			|| kbdmode & KBD_TEXTMODE
			&& !SetConsoleMode(hConsoleInput, kbdmode & ~KBD_TEXTMODE))
		return 0;

	return 1;
}

void ExitKeys(void)
{
	if (kbdmode & KBD_TEXTMODE)
		SetConsoleMode(hConsoleInput, kbdmode & ~KBD_TEXTMODE);
}

static unsigned GetWin32Key(unsigned *repeatCount)
{
	INPUT_RECORD ir;
	DWORD count;
	unsigned k, asciiChar, scanCode;
	unsigned long state;

	*repeatCount = 1;

	if (!ReadConsoleInput(hConsoleInput, &ir, 1, &count))
	{
		k = ERRORKEY;
#if PRINT_SCANS
		printf("Last error %ld, 0x%lx\n", GetLastError(), GetLastError());
#endif
		goto out;
	}

	if (ir.EventType != KEY_EVENT || !ir.Event.KeyEvent.bKeyDown)
	{
		k = NONKEY;
		goto out;
	}

#if PRINT_SCANS
	printf("repcount %u, key %x, scan %x, char %x, state %lx\n",
			ir.Event.KeyEvent.wRepeatCount, ir.Event.KeyEvent.wVirtualKeyCode,
			ir.Event.KeyEvent.wVirtualScanCode,
			(int)ir.Event.KeyEvent.uChar.AsciiChar,
			(long)ir.Event.KeyEvent.dwControlKeyState);
#endif

	asciiChar = ir.Event.KeyEvent.uChar.AsciiChar;
	scanCode = ir.Event.KeyEvent.wVirtualScanCode;
	state = ir.Event.KeyEvent.dwControlKeyState;
	*repeatCount = ir.Event.KeyEvent.wRepeatCount;
	if (*repeatCount == 0)
		*repeatCount = 1;

	if (asciiChar != 0 && !(state & ENHANCED_KEY))
	{
		k = (scanCode << 8) | asciiChar;
		for (int i = 0; i < sizeof(spkeys)/sizeof(spkeys[0]); i++)
			if (spkeys[i].scan == k)
			{
				k = spkeys[i].key;
				goto outState;
			}
		if (state & (RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED))
		{
			if (asciiChar >= 'a' && asciiChar <= 'z')
				asciiChar -= 'a' - 'A';
			else if (asciiChar < 0x20)
				asciiChar += '@';
			k = asciiChar | ALT;
			goto out;
		}
		if (!(state & (RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED)))
			k = asciiChar;
		else if (asciiChar >= 0x20)
			k = NONKEY;
		else
		{
			k = (asciiChar + '@') | DCTRL;
			if (state & SHIFT_PRESSED)
				k |= SHIFT;
		}
		goto out;
	}

	k = scanCode < sizeof(extkeys)/sizeof(extkeys[0])? extkeys[scanCode]: NONKEY;
	if (k == NONKEY)
		goto out;
	if (state & ENHANCED_KEY)
	{
		unsigned i = k & ~(DCTRL | SHIFT);
		if (i >= NPDOT && i <= NP9)
			k += DEL - NPDOT;
	}

outState:
	if (!(k & (DCTRL | SHIFT | ALT)))
	{
		if (state & (RIGHT_ALT_PRESSED | LEFT_ALT_PRESSED))
			k |= ALT;
		else
		{
			if (state & SHIFT_PRESSED)
				k |= SHIFT;
			if (state & (RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED))
				k |= DCTRL;
		}
	}
out:
	return k;
}

int KeyHit(unsigned millisecondsToWait)
{
	unsigned k, r;
	if (keybp > keybuf)
		return 1;
	if (WaitForSingleObject(hConsoleInput, millisecondsToWait) != WAIT_OBJECT_0
			|| (k = GetWin32Key(&r)) == NONKEY)
		return 0;
	while (r--)
		UngetKey(k);
	return 1;
}

unsigned GetKey(void)
{
	unsigned k, r;
	if (keybp > keybuf)
	    return *--keybp;
	do
		k = GetWin32Key(&r);
	while (k == NONKEY);
#if PRINT_SCANS
	if (r > 1)
		printf("repeat %u times\n", r);
#endif
	while (--r)
		UngetKey(k);
	return k;
}

void UngetKey(unsigned key)
{
	if (keybp < keybuf + MAX_UNGETS)
		*keybp++ = key;
}

//===========================================================================
#endif
