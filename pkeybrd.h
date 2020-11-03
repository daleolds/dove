#ifndef PKEYBRD_H
#define PKEYBRD_H 1

/*
 * Keys are unsigned ints, upper byte is modifier bits,
 * lower byte is key data
 */
 
/*-----------------------------------------------------------------------------
 * Modifier bits. SCROLL, NUMLOCK, CAPSLOCK and INSLOCK are never
 * set in a key, only in the modifier state variable.
 * FUNC is never set in the modifier state variable.
 */
#define FUNC		0x0100
#define SHIFT		0x0200
#define DCTRL		0x0400
#define ALT			0x0800
#define SCROLL		0x1000
#define NUMLOCK		0x2000
#define CAPSLOCK	0x4000

#define REPT		0x8000		/* used for repeat counts in Dove */


/*-----------------------------------------------------------------------------
 * function key definitions
 */
enum FUNCKEY
{
	ESC = (FUNC|1),	/* has ALT and UNSHIFT */
	PRTSC, BREAK,		/* have DCTRL only */

	/*
	 * TAB thru F12 are keys that have ALT	| DCTRL | SHIFT	| UNSHIFT
	 */
	TAB, F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,

	/*
	 * NPDOT thru NP5 are keys that have DCTRL | SHIFT | UNSHIFT
	 * num pad (order of unshifted functions must match order of cursor keys)
	 */
	NPDOT, NP0, NP1, NP2, NP3, NP4, NP6, NP7, NP8, NP9, NP5,

	/*
	 * DEL thru NPSLASH are keys that have ALT | DCTRL | UNSHIFT
	 * DEL thru PGUP must match num pad and be in order for IsCursorKey macro
	 */
	DEL,
	INS,
	END,
	DOWN,
	PGDN,
	LEFT,
	RIGHT,
	HOME,
	UP,
	PGUP,
	BSP, ENTER, NPENTER, NPSTAR, NPMINUS, NPPLUS, NPSLASH,

	NONKEY,				/* for keys that aren't currently handled */
	ERRORKEY,			/* for errors from the keyboard code */
	REFRESHDISPLAYKEY,	/* hack. caused by a window resize signal */
};

/*----------------------------------------------------------------------
 * macros
 */
/* modifiers other that FUNC must be handled explicitly */
#define IsCursorKey(c)	((c) >= UP && (c) <= PGUP)
#define KeyData(c)		((c) & 0xff)
#define IsDataKey(c)		(!((c) & (FUNC|DCTRL|SHIFT|ALT)))

/*----------------------------------------------------------------------
 * prototypes
 */
int InitKeys(void);
void ExitKeys(void);
unsigned GetKey(void);
int KeyHit(unsigned millisecondsToWait);
void UngetKey(unsigned key);

/*======================================================================*/
#endif
