#if defined(DOVE_FOR_DOS)

#error code preserved from previous versions of DOVE, but not currently supported

#define PRINT_SCANS	0

#if PRINT_SCANS
#include <stdio.h>
#endif

#include <stddef.h>
#include <bios.h>
#include <dos.h>
#include "pkeybrd.h"

#define MAX_UNGETS  30

static unsigned keybuf[MAX_UNGETS], *keybp = keybuf;

/*---------------------------------------------------------------------------
 * most of these bit definitions correspond
 * to the upper byte of the key modifiers.
 */
#define BIOS_RSHIFT	0x01
#define BIOS_LSHIFT	0x02
#define BIOS_CTRL		0x04
#define BIOS_ALT		0x08
#define BIOS_SCROLL	0x10
#define BIOS_NUM		0x20
#define BIOS_CAPS		0x40
#define BIOS_INS		0x80

static char requestBase = 0;

/* -------------------------------------------------------------------
 * keys which return extended key codes,
 * the extended key code is used as an index
 * into this table.
 */
static unsigned extkeys[] =
{
/* 00 */	DCTRL|BREAK,
/* 01 */	ALT|ESC,			NONKEY,			DCTRL|'2',	NONKEY,
			NONKEY,			NONKEY,			NONKEY,		NONKEY,
			NONKEY,			NONKEY,			NONKEY,		NONKEY,
			NONKEY,			ALT|BSP,			SHIFT|TAB,	ALT|'Q',
/* 11 */	ALT|'W',			ALT|'E',			ALT|'R',		ALT|'T',
			ALT|'Y',			ALT|'U',			ALT|'I',		ALT|'O',
			ALT|'P',			ALT|'[',			ALT|']',		ALT|ENTER,
			NONKEY,			ALT|'A',			ALT|'S',		ALT|'D',
/* 21 */	ALT|'F',			ALT|'G',			ALT|'H',		ALT|'J',
			ALT|'K',			ALT|'L',			ALT|';',		ALT|'\'',
			ALT|'`',			NONKEY,			ALT|'\\',	ALT|'Z',
			ALT|'X',			ALT|'C',			ALT|'V',		ALT|'B',
/* 31 */	ALT|'N',			ALT|'M',			ALT|',',		ALT|'.',
			ALT|'/',			NONKEY,			ALT|NPSTAR,	NONKEY,
			NONKEY,			NONKEY,			F1,			F2,
			F3,				F4,				F5,			F6,
/* 41 */	F7,				F8,				F9,			F10,
			NONKEY,			NONKEY,			SHIFT|NP7,	SHIFT|NP8,
			SHIFT|NP9,		ALT|NPMINUS,	SHIFT|NP4,	SHIFT|NP5,
			SHIFT|NP6,		ALT|NPPLUS,		SHIFT|NP1,	SHIFT|NP2,
/* 51 */	SHIFT|NP3,		SHIFT|NP0,		SHIFT|NPDOT,	SHIFT|F1,
			SHIFT|F2,		SHIFT|F3,		SHIFT|F4,	SHIFT|F5,
			SHIFT|F6,		SHIFT|F7,		SHIFT|F8,	SHIFT|F9,
			SHIFT|F10,		DCTRL|F1,			DCTRL|F2,		DCTRL|F3,
/* 61 */	DCTRL|F4,			DCTRL|F5,			DCTRL|F6,		DCTRL|F7,
			DCTRL|F8,			DCTRL|F9,			DCTRL|F10,	ALT|F1,
			ALT|F2,			ALT|F3,			ALT|F4,		ALT|F5,
			ALT|F6,			ALT|F7,			ALT|F8,		ALT|F9,
/* 71 */	ALT|F10,			DCTRL|PRTSC,		DCTRL|NP4,	DCTRL|NP6,
			DCTRL|NP1,		DCTRL|NP3,		DCTRL|NP7,	ALT|'1',
			ALT|'2',			ALT|'3',			ALT|'4',		ALT|'5',
			ALT|'6',			ALT|'7',			ALT|'8',		ALT|'9',
/* 81 */	ALT|'0',			ALT|'-',			ALT|'=',		DCTRL|NP9,
			F11,				F12,				SHIFT|F11,	SHIFT|F12,
			DCTRL|F11,		DCTRL|F12,		ALT|F11,		ALT|F12,
			DCTRL|NP8,		DCTRL|NPMINUS,	DCTRL|NP5,	DCTRL|NPPLUS,
/* 91 */	DCTRL|NP2,		DCTRL|NP0,		DCTRL|NPDOT,	DCTRL|TAB,
			DCTRL|NPSLASH,	DCTRL|NPSTAR,	ALT|HOME,	ALT|UP,
			ALT|PGUP,		NONKEY,			ALT|LEFT,	NONKEY,
			ALT|RIGHT,		NONKEY,			ALT|END,		ALT|DOWN,
/* a1 */	ALT|PGDN,		ALT|INS,			ALT|DEL,		ALT|NPSLASH,
			ALT|TAB,			ALT|NPENTER,
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
	{0xe00d, NPENTER},
	{0x372a, NPSTAR},
	{0x4a2d, NPMINUS},
	{0x4e2b, NPPLUS},
	{0xe02f, NPSLASH},
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
	{0x0e7f, DCTRL|BSP},
	{0x1c0a, DCTRL|ENTER},
	{0xe00a, DCTRL|NPENTER},
};


/* -------------------------------------------------------------------
 * interface to bios keyboard routines
 */
static int BIOSKeyHit(void)
{
	asm {
			mov	ah, requestBase
			inc	ah
			int	0x16
			jz	NoKey
		}
	return 1;
NoKey:
	return 0;
}

/* -------------------------------------------------------------------
 */
static int BIOSKeyboard(char request)
{
	asm	{
			mov	ah, requestBase
			add	ah, request
			int	0x16
		}
	return _AX;
}

/* -------------------------------------------------------------------
 */
static int IsExtendedKeyboard(void)
{
	/* flush keys from buffer */
FlushKeys:
	asm	{
			mov		ah, 1
			int		0x16			// key available?
			jz		KeysFlushed		// no-->
			mov		ah, 0
			int		0x16			// get the key
			jmp		FlushKeys
		}

KeysFlushed:
	asm {
		/* push key 0xff, scan code 0xff on the keyboard buffer */
			mov		cx, 0xffff
			mov		ax, 0x05ff
			int		0x16
			or		al, al
			jnz		KeyNotPushed
		}

		asm		mov		bx, 0
TryAgain:
		asm {
			/* see if extended key and scan code is available */
				mov		ah, 0x11
				push	bx
				int		0x16
				pop		bx
				jz		NotExtended
	
			/* key is available, get it */
				mov		ah, 0x10
				push	bx
				int		0x16
				pop		bx
				cmp		ax, 0xffff
				je		Extended
				inc		bx
				cmp		bx, 16
				jl		TryAgain
			}

NotExtended:						// flush key that was pushed
	asm	{
			mov		ah, 1
			int		0x16			// key available?
			jz		KeyNotPushed	// no-->
			mov		ah, 0
			int		0x16			// get the key
			jmp		NotExtended
		}
KeyNotPushed:
	return 0;	

Extended:
	return 1;
}

/* -------------------------------------------------------------------
 * Initialization function
 */
int InitKeys(void)
{
	requestBase = IsExtendedKeyboard()? 0x10: 0;
	return 1;
}

/* -------------------------------------------------------------------
 * Exit functions
 */
void ExitKeys(void)
{
}

/* -------------------------------------------------------------------
 * return != 0 if key is available
 */
int KeyHit(unsigned millisecondsToWait)
{
	(void)millisecondsToWait;
	return keybp > keybuf || BIOSKeyHit();
}

/* -------------------------------------------------------------------
 * Pushes a key code onto the ungetkey stack
 */
void UngetKey(unsigned key)
{
	if (keybp < keybuf + MAX_UNGETS)
		*keybp++ = key;
}

/* -------------------------------------------------------------------
 * return modifier state
 */
static unsigned GetModifierState(void)
{
	unsigned biosState, newState = 0;
	biosState = BIOSKeyboard(2) & 0xff;
	if (biosState & (BIOS_RSHIFT|BIOS_LSHIFT))
		newState |= SHIFT;
	newState |= (biosState & ~(BIOS_RSHIFT|BIOS_LSHIFT)) << 8;
	return newState;
}

/* -------------------------------------------------------------------
 * This routine returns a key from the ungetkey buffer or gets one from the
 * ROM BIOS_ buffer and converts it to the key definitions found in pkeybrd.h
 */
unsigned GetKey(void)
{
	unsigned i, k, al, ah;
	if (keybp > keybuf)
	    return *--keybp;
	k = BIOSKeyboard(0);
	al = k & 0xff;
	ah = (k >> 8) & 0xff;

#if PRINT_SCANS
	printf("scan %x, char %x\n", ah, al);
#endif

	if (al != 0 && al != 0xE0)
	{
		for (i = 0; i < sizeof(spkeys)/sizeof(spkeys[0]); i++)
			if (spkeys[i].scan == k)
			{
				i = spkeys[i].key;
				goto checkCtrlShift;
			}
		if (al >= 0x20 || ah == 0)
			return al;
		i = al | DCTRL | 0x40;
		goto checkCtrlShift;
	}

	i = ah < sizeof(extkeys)/sizeof(extkeys[0])? extkeys[ah]: NONKEY;
	if (al == 0xE0)
	{
		k = i & ~(DCTRL | SHIFT);
		if (k >= NPDOT && k <= NP9)
		{
			i += DEL - NPDOT;
			i &= ~SHIFT;
		}
	}
checkCtrlShift:
	if (i != NONKEY && !(i & (ALT | SHIFT)) && GetModifierState() & SHIFT)
		i |= SHIFT;
	return i;
}

//===========================================================================
#endif
