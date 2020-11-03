#if defined(DOVE_FOR_GNU)

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/time.h>

#include <assert.h>

//#include <curses.h>
#define COLOR_BLACK	0
#define COLOR_RED	1
#define COLOR_GREEN	2
#define COLOR_YELLOW	3
#define COLOR_BLUE	4
#define COLOR_MAGENTA	5
#define COLOR_CYAN	6
#define COLOR_WHITE	7
#define ERR     (-1)

#include <ncursesw/term.h>
#include <termios.h>
#include <fcntl.h>

#include "pvideo.h"
#include "pkeybrd.h"

unsigned videoRows, videoCols;

#define MAX_CAP_SEQ	32
#define KEY_SEQ_WAIT 1000 // microseconds to wait to complete key sequence

static struct termios DCShellModes, DCTermModes;
static char *SeqCursorAddress, *SeqCursorModeStart, *SeqCursorModeEnd;
static char *SeqClearScreen, *SeqSetBackColor, *SeqSetForeColor;
static char *SeqCursorGone, *SeqCursorNorm, *SeqCursorHigh;
static const char *SeqStartBorders, *SeqEndBorders;
static char *SeqSetBold, *SeqClearAttributes;
static unsigned TTYCurRow, TTYCurCol, UserCurRow, UserCurCol;
static VCHAR *Screen = 0;
static CURSORTYPE UserCursorType = CURSOR_SMALL, TTYCursorType = CURSOR_SMALL;
static CURSORFAKE UserCursorFake = CF_TTY_ONLY;
static VBYTE UserFakeCursorAttr, CurAttr, RealCursorAttr;
static char SeqANSIForeColor[] = "\033[3%dm", SeqANSIBackColor[] = "\033[4%dm";
static bool CurBold = false;

//---------------------------------------------------------------------------
static void ResetGlobals()
{
	videoRows = videoCols = 0;
	memset(&DCShellModes, 0, sizeof DCShellModes);
	memset(&DCTermModes, 0, sizeof DCTermModes);
	SeqCursorAddress = SeqCursorModeStart = SeqCursorModeEnd = 0;
	SeqClearScreen = SeqSetBackColor = SeqSetForeColor = 0;
	SeqCursorGone = SeqCursorNorm = SeqCursorHigh = 0;
	SeqStartBorders = SeqEndBorders = SeqSetBold = SeqClearAttributes = 0;
	TTYCurRow = TTYCurCol = UserCurRow = UserCurCol = 0;
	delete Screen;
	Screen = 0;
	UserCursorType = CURSOR_SMALL;
	TTYCursorType = CURSOR_SMALL;
	UserCursorFake = CF_TTY_ONLY;
	UserFakeCursorAttr = CurAttr = RealCursorAttr = 0;
	CurBold = false;
}

//---------------------------------------------------------------------------
// mapping of terminfo capability strings to key values

struct CM
{
	const char *capName;
	const char *capSeq;
	unsigned seqLen;
	unsigned key;
} CapMap[] = 
{
	/* arrow keys */
	{0, "\x1b[D", 0, LEFT},
	{"kcub1", 0, 0, LEFT},
	{0, "\x1bOD", 0, LEFT},
	{0, "\x1b[d", 0, SHIFT | LEFT},
	{"kLFT", 0, 0, SHIFT | LEFT},
	{0, "\x1b[1;2D", 0, SHIFT | LEFT},
	{0, "\x1b[1;3D", 0, ALT | LEFT},
//	{0, "\x1b\x1b[D", 0, ALT | LEFT},
//	{0, "\x1b\x1b[d", 0, ALT | SHIFT | LEFT},
	{0, "\x1b[1;4D", 0, ALT | SHIFT | LEFT},
	{0, "\x1b[1;5D", 0, DCTRL | LEFT},
	{0, "\x1bOd", 0, DCTRL | LEFT},
	{0, "\x1b[1;6D", 0, DCTRL | SHIFT | LEFT},

	{0, "\x1b[B", 0, DOWN},
	{"kcud1", 0, 0, DOWN},
	{0, "\x1bOB", 0, DOWN},
	{"cud1", 0, 0, DOWN},
	{0, "\x1b[1;2B", 0, SHIFT | DOWN},
	{"kind", 0, 0, SHIFT | DOWN},
	{0, "\x1b[b", 0, SHIFT | DOWN},
	{0, "\x1b[1;3B", 0, ALT | DOWN},
//	{0, "\x1b\x1b[B", 0, ALT | DOWN},
	{0, "\x1b[1;4B", 0, ALT | SHIFT | DOWN},
//	{0, "\x1b\x1b[b", 0, ALT | SHIFT | DOWN},
	{0, "\x1b[1;5B", 0, DCTRL | DOWN},
	{0, "\x1bOb", 0, DCTRL | DOWN},
	{0, "\x1b[1;6B", 0, DCTRL | SHIFT | DOWN},

	{0, "\x1bOC", 0, RIGHT},
	{"kcuf1", 0, 0, RIGHT},
	{0, "\x1b[C", 0, RIGHT},
	{"cuf1", 0, 0, RIGHT},
	{0, "\x1b[c", 0, SHIFT | RIGHT},
	{"kRIT", 0, 0, SHIFT | RIGHT},
	{0, "\x1b[1;2C", 0, SHIFT | RIGHT},
//	{0, "\x1b\x1b[C", 0, ALT | RIGHT},
	{0, "\x1b[1;3C", 0, ALT | RIGHT},
	{0, "\x1b[1;4C", 0, ALT | SHIFT | RIGHT},
//	{0, "\x1b\x1b[c", 0, ALT | SHIFT | RIGHT},
	{0, "\x1b[1;5C", 0, DCTRL | RIGHT},
	{0, "\x1bOc", 0, DCTRL | RIGHT},
	{0, "\x1b[1;6C", 0, DCTRL | SHIFT | RIGHT},

	{0, "\x1bOA", 0, UP},
	{"kcuu1", 0, 0, UP},
	{0, "\x1b[A", 0, UP},
	{"cuu1", 0, 0, UP},
	{0, "\x1b[1;2A", 0, SHIFT | UP},
	{"kri", 0, 0, SHIFT | UP},
	{0, "\x1b[a", 0, SHIFT | UP},
//	{0, "\x1b\x1b[A", 0, ALT | UP},
	{0, "\x1b[1;3A", 0, ALT | UP},
//	{0, "\x1b\x1b[a", 0, ALT | SHIFT | UP},
	{0, "\x1b[1;4A", 0, ALT | SHIFT | UP},
	{0, "\x1bOa", 0, DCTRL | UP},
	{0, "\x1b[1;[5A", 0, DCTRL | UP},
	{0, "\x1b[1;[6A", 0, DCTRL | SHIFT | UP},

	/* enter, backspace, tab */
	{0, "\r", 0, ENTER},
	{"cr", 0, 0, ENTER},
	{0, "\r", 0, ENTER},
	{"kent", 0, 0, ENTER},
	{0, "\n", 0, DCTRL | ENTER},
	{"cud1", 0, 0, DCTRL | ENTER},
	{0, "\x1b\n", 0, ALT | ENTER},

	{0, "\t", 0, TAB},
	{"ht", 0, 0, TAB},
	{0, "\x1b[Z", 0, SHIFT | TAB},
	{"kcbt", 0, 0, SHIFT | TAB},
	{0, "\x7f", 0, BSP},
	{"kbs", 0, 0, BSP},
	{0, "\x1b\x7f", 0, ALT | BSP},
	{0, "\x08", 0, DCTRL | BSP},
	{"cub1", 0, 0, DCTRL | BSP},

	/* home, end, page up/down, insert, delete */ 
	{0, "\x1b[7~", 0, HOME},
	{"khome", 0, 0, HOME},
	{0, "\x1bOH", 0, HOME},
	{"home", 0, 0, HOME},
	{0, "\x1b[1~", 0, HOME},
	{0, "\x1b[7$", 0, SHIFT | HOME},
	{"kHOM", 0, 0, SHIFT | HOME},
	{0, "\x1b[7^", 0, DCTRL | HOME},
	{0, "\x1b[7@", 0, DCTRL | SHIFT | HOME},

	{0, "\x1b[F", 0, END},
	{0, "\x1bOF", 0, END},
	{"kend", 0, 0, END},
	{0, "\x1b[8~", 0, END},
	{"kslt", 0, 0, END},
	{0, "\x1b[4~", 0, END},
	{0, "\x1b[8$", 0, SHIFT | END},
	{"kEND", 0, 0, SHIFT | END},
	{0, "\x1b[8^", 0, DCTRL | END},
	{"kel", 0, 0, DCTRL | END},
	{0, "\x1b[8@", 0, DCTRL | SHIFT | END},

	{0, "\x1b[6~", 0, PGDN},
	{"knp", 0, 0, PGDN},
	{0, "\x1b[6^", 0, DCTRL | PGDN},
	{0, "\x1b[6;5~", 0, DCTRL | PGDN},
	{0, "\x1b[6;3~", 0, ALT | PGDN},

	{0, "\x1b[5~", 0, PGUP},
	{"kpp", 0, 0, PGUP},
	{0, "\x1b[5^", 0, DCTRL | PGUP},
	{0, "\x1b[5;5~", 0, DCTRL | PGUP},
	{0, "\x1b[5;3~", 0, ALT | PGUP},

	{"ich1", 0, 0, INS},
	{0, "\x1b[2~", 0, INS},
	{"kich1", 0, 0, INS},
	{0, "\x1b[2;3~", 0, ALT | INS},
	{0, "\x1b[2^", 0, DCTRL | INS},
	{0, "\x1b[2@", 0, DCTRL | SHIFT | INS},

	{"dch1", 0, 0, DEL},
	{0, "\x1b[3~", 0, DEL},
	{"kdch1", 0, 0, DEL},
	{0, "\x1b[3;2~", 0, SHIFT | DEL},
	{"kDC", 0, 0, SHIFT | DEL},
	{0, "\x1b[3$", 0, SHIFT | DEL},
	{0, "\x1b[3;3~", 0, ALT | DEL},
	{0, "\x1b[3;5~", 0, DCTRL | DEL},
	{0, "\x1b[3^", 0, DCTRL | DEL},
	{0, "\x1b[3@", 0, DCTRL | SHIFT | DEL},

	/* function keys */
	{0, "\x1bOP", 0, F1},
	{"kf1", 0, 0, F1},
	{0, "\x1b[11~", 0, F1},
	{0, "\x1bOQ", 0, F2},
	{"kf2", 0, 0, F2},
	{0, "\x1b[12~", 0, F2},
	{0, "\x1bOR", 0, F3},
	{"kf3", 0, 0, F3},
	{0, "\x1b[13~", 0, F3},
	{0, "\x1bOS", 0, F4},
	{"kf4", 0, 0, F4},
	{0, "\x1b[14~", 0, F4},
	{0, "\x1bOt", 0, F5},
	{"kf5", 0, 0, F5},
	{0, "\x1b[15~", 0, F5},
	{0, "\x1bOu", 0, F6},
	{"kf6", 0, 0, F6},
	{0, "\x1b[17~", 0, F6},
	{0, "\x1bOv", 0, F7},
	{"kf7", 0, 0, F7},
	{0, "\x1b[18~", 0, F7},
	{0, "\x1bOl", 0, F8},
	{"kf8", 0, 0, F8},
	{0, "\x1b[19~", 0, F8},
	{0, "\x1bOw", 0, F9},
	{"kf9", 0, 0, F9},
	{0, "\x1b[20~", 0, F9},
	{0, "\x1bOx", 0, F10},
	{"kf10", 0, 0, F10},
	{0, "\x1b[21~", 0, F10},
	{0, "\x1b[23~", 0, F11}, 
	{"kf11", 0, 0, F11}, 
	{0, "\x1b[24~", 0, F12}, 
	{"kf12", 0, 0, F12}, 
	{0, "\x1b[25~", 0, SHIFT | F3},
	{"kf13", 0, 0, SHIFT | F3},
	{0, "\x1b[26~", 0, SHIFT | F4},
	{"kf14", 0, 0, SHIFT | F4},
	{0, "\x1b[28~", 0, SHIFT | F5},
	{"kf15", 0, 0, SHIFT | F5},
	{0, "\x1b[29~", 0, SHIFT | F6},
	{"kf16", 0, 0, SHIFT | F6},
	{0, "\x1b[31~", 0, SHIFT | F7},
	{"kf17", 0, 0, SHIFT | F7},
	{0, "\x1b[32~", 0, SHIFT | F8},
	{"kf18", 0, 0, SHIFT | F8},
	{0, "\x1b[33~", 0, SHIFT | F9},
	{"kf19", 0, 0, SHIFT | F9},
	{0, "\x1b[34~", 0, SHIFT | F10},
	{"kf20", 0, 0, SHIFT | F10},

	/* keypad keys */
	{0, "\x1bOw", 0, NP7},
	{"ka1", 0, 0, NP7},
	{0, "\x1bOy", 0, NP9},
	{"ka3", 0, 0, NP9},
	{0, "\x1bOu", 0, NP5},
	{"kb2", 0, 0, NP5},
	{0, "\x1bOq", 0, NP1},
	{"kc1", 0, 0, NP1},
	{0, "\x1bOs", 0, NP3},
	{"kc3", 0, 0, NP3},
	{0, 0, 0, NPENTER},
	{0, "\x1bOM", 0, NPENTER},
};

//---------------------------------------------------------------------------

// this string is PC line drawing chars
static unsigned char PCBorders[] =
{
	218,191,217,192,179,196,
	201,187,188,200,186,205,
	214,183,189,211,186,196,
	213,184,190,212,179,205,
	194,194,217,192,179,196,
	0
};

// this string is the initialized to the VT100 line drawing character set
// the init code replaces it with the chars from the 0 capability
static char BorderChars[] = "lkjmxqlkjmxqlkjmxqlkjmxqwwjmxq";

// if no line drawing charset, use these
static char DefaultBorders[] = "<>><|-<>><|-<>><|-<>><|-TT><|-";

static char TTBuffer[4096], *TTBufp = TTBuffer, *TTBufHeadEnd = TTBuffer;

//---------------------------------------------------------------------------
static void TTFlush()
{
	if (TTBufp != TTBuffer)
	{
		ssize_t len = TTBufp - TTBuffer;
		ssize_t wlen = len? write(1, TTBuffer, len): 0;
		int err = wlen == -1? errno: 0;
		assert(!err && wlen == len);
		TTBufp = TTBufHeadEnd = TTBuffer;
	}
}

/*---------------------------------------------------------------------------
 * This is where padding should be handled. The init routine could determine
 * if any padding is present. If so this routine should call the terminfo
 * function to add padding to the string before stuffing it in the buffer.
 * Or we could handle it like the SLang curses-like library -- just
 * remove all padding info at load time.
 */
static void TTPutSeq(const char *s)
{
	if (!s)
		return;
	unsigned len = strlen(s);
	assert(len < sizeof(TTBuffer));
	if ((TTBufp - TTBuffer) + len > sizeof(TTBuffer))
		TTFlush();
	memcpy(TTBufp, s, len);
	TTBufp += len;
}

//---------------------------------------------------------------------------
static inline void TTBorders(bool on)
{
	static bool CurBorders = false;
	if (CurBorders != on)
		TTPutSeq((CurBorders = on)? SeqStartBorders: SeqEndBorders);
}

//---------------------------------------------------------------------------
static void TTPutC(char c)
{
	char *s;

	if (TTBufp - TTBuffer >= sizeof(TTBuffer))
		TTFlush();

	if (c <= 0x7e && c >= 0x20)
		TTBorders(false);
	else if (!c || !(s = strchr((char *)PCBorders, c)))
	{
		TTBorders(false);
		c = '~';
	}
	else
	{
		TTBorders(true);
		c = BorderChars[s - (char *)PCBorders];
	}
	*TTBufp++ = c;
	TTYCurCol++;
}

//---------------------------------------------------------------------------
void TTPositionCursor(unsigned row, unsigned col)
{
	if (col != TTYCurCol || row != TTYCurRow)
	{
		TTYCurRow = row;
		TTYCurCol = col;
		TTPutSeq(tparm(SeqCursorAddress, TTYCurRow, TTYCurCol));
	}
}

//---------------------------------------------------------------------------
static void TTRead(unsigned row, unsigned col, unsigned charCount, VCHAR *vchars)
{
	assert(row < videoRows && col < videoCols);
	assert(charCount <= videoCols - col);
	memcpy(vchars, &Screen[row * videoCols + col], charCount * sizeof(*vchars));
}

//---------------------------------------------------------------------------
static void TTWrite(unsigned row, unsigned col, unsigned charCount, VCHAR *vchars)
{
	static const int DUI2TerminfoColor[8] = { COLOR_BLACK, COLOR_BLUE,
			COLOR_GREEN, COLOR_CYAN, COLOR_RED, COLOR_MAGENTA,
			COLOR_YELLOW, COLOR_WHITE };

	unsigned i;
	VCHAR *svp, *dvp;

	assert(row < videoRows && col < videoCols);
	assert(charCount <= videoCols - col);

	for (dvp = &Screen[row * videoCols + col], svp = vchars, i = 0;
			i < charCount; i++, dvp++, svp++)
	{
		if (svp->a != dvp->a || svp->c != dvp->c)
		{
			dvp->a = svp->a;
			dvp->c = svp->c;
			TTPositionCursor(row, col + i);
			if (svp->a != CurAttr)
			{
				if (SeqSetBold && CurBold != (svp->a & VBRIGHT) != 0)
				{
					CurBold = !CurBold;
					if (CurBold)
						TTPutSeq(SeqSetBold);
					else
					{
						TTPutSeq(SeqClearAttributes);
						CurAttr = MakeAttrib(VBLACK, VBLACK);
					}
				}
				TTPutSeq(tparm(SeqSetForeColor, DUI2TerminfoColor[svp->a & 0x7]));
				TTPutSeq(tparm(SeqSetBackColor, DUI2TerminfoColor[(svp->a >> 4) & 0x7]));
				CurAttr = svp->a;
			}
			TTPutC(svp->c);
		}
	}
}

//---------------------------------------------------------------------------
static void TTHideCursor()
{
	if (TTYCursorType == CURSOR_NONE)
		return;
	TTYCursorType = CURSOR_NONE;
	if (UserCursorFake != CF_TTY_ONLY)
	{
		VCHAR vc;
		TTRead(UserCurRow, UserCurCol, 1, &vc);
		vc.a = RealCursorAttr;
		TTWrite(UserCurRow, UserCurCol, 1, &vc);
		TTPositionCursor(UserCurRow, UserCurCol);
	}
	if (UserCursorFake != CF_FAKE_ONLY && SeqCursorGone)
		TTPutSeq(SeqCursorGone);
}

static void TTUserCursor()
{
	if (UserCursorType == CURSOR_NONE)
	{
		TTHideCursor();
		return;
	}
	if (UserCursorFake != CF_TTY_ONLY)
	{
		VCHAR vc;
		TTRead(UserCurRow, UserCurCol, 1, &vc);
		RealCursorAttr = vc.a;
		vc.a = UserFakeCursorAttr;
		TTWrite(UserCurRow, UserCurCol, 1, &vc);
		TTPositionCursor(UserCurRow, UserCurCol);
	}
	if (UserCursorFake != CF_FAKE_ONLY)
	{
		TTPutSeq(UserCursorType == CURSOR_BIG? SeqCursorHigh: SeqCursorNorm);
		TTPositionCursor(UserCurRow, UserCurCol);
	}
	TTYCursorType = UserCursorType;
}

/*-----------------------------------------------------------------------------
 */
static bool TTResized = false;

static void SigWINCHHandler(int sig)
{
	if (sig == SIGWINCH)
	{
		TTResized = true;
		signal(SIGWINCH, SigWINCHHandler);
	}
}

//---------------------------------------------------------------------------
static unsigned VidInitCount = 0;

int InitVideo()
{
	unsigned i, len;
	int err;
	char *capSeq, *borderPairs, *bp, *dp, *enableAltCharSet;
//	char *termName = getenv("COLORTERM");
//	if (!termName)
//		termName = getenv("TERM");

	if (VidInitCount++)
		return 1;

	if (tcgetattr(0, &DCShellModes) != 0 || tcgetattr(0, &DCTermModes) != 0)
	{
		VidInitCount--;
		return 0;
	}
	if (setupterm(0 /* termName */, 1, &err) == ERR)
	{
		puts("InitVideo failed, could not setupterm"); 
		VidInitCount--;
		return 0;
	}

	videoRows = tigetnum((char *)"lines");
	videoCols = tigetnum((char *)"cols");
//	assert(videoRows == LINES && videoCols == COLS && !Screen);
	SeqClearScreen = tigetstr((char *)"clear");
	SeqCursorAddress = tigetstr((char *)"cup");
	SeqCursorModeStart = tigetstr((char *)"smcup");
	SeqCursorModeEnd = tigetstr((char *)"rmcup");
	SeqCursorGone = tigetstr((char *)"civis");
	SeqCursorNorm = tigetstr((char *)"cnorm");
	SeqCursorHigh = tigetstr((char *)"cvvis");
	SeqStartBorders = tigetstr((char *)"smacs");
	SeqEndBorders = tigetstr((char *)"rmacs");
	SeqSetBold = tigetstr((char *)"bold");
	SeqClearAttributes = tigetstr((char *)"sgr0");
	if (!SeqClearAttributes)
		SeqSetBold = 0;
	borderPairs = tigetstr((char *)"acsc");
	enableAltCharSet = tigetstr((char *)"enacs");
	if (!(SeqSetForeColor = tigetstr((char *)"setaf")))
		SeqSetForeColor = tigetstr((char *)"setf");
	if (!(SeqSetBackColor = tigetstr((char *)"setab")))
		SeqSetBackColor = tigetstr((char *)"setb");
	if (!SeqSetForeColor || !SeqSetBackColor)
	{
		SeqSetBackColor = SeqANSIBackColor;
		SeqSetForeColor = SeqANSIForeColor;
	}
//	DCTermModes.c_iflag = 0;
//	DCTermModes.c_lflag = 0;
//	DCTermModes.c_oflag = 0;
	cfmakeraw(&DCTermModes);
	DCTermModes.c_cc[VMIN] = 1;	
	DCTermModes.c_cc[VTIME] = 0;
	if (tcsetattr(0, TCSANOW, &DCTermModes) != 0
			|| !SeqClearScreen || !SeqCursorAddress
			|| !(Screen = new VCHAR[videoRows * videoCols]))
	{
		ExitVideo();
		return 0;
	}
	
	CurAttr = MakeAttrib(VBLACK, VBLACK);
	for (i = videoRows * videoCols; i > 0; )
	{
		Screen[--i].a = CurAttr;
		Screen[i].c = ' ';
	}

	if (!SeqStartBorders || !SeqEndBorders || !borderPairs
			|| strlen(borderPairs) % 2)
	{
		SeqStartBorders = SeqEndBorders = "";
		strcpy(BorderChars, DefaultBorders);
	}
	else
		for (bp = borderPairs; *bp; bp += 2)
			for (dp = BorderChars; *dp; dp++)
				if (*dp == bp[0])
					*dp = bp[1];

	for (i = 0; i < ELEMCOUNT(CapMap); i++)
		if (CapMap[i].capName
				&& (capSeq = tigetstr((char *)CapMap[i].capName))
				&& capSeq != (char *)-1
				&& (len = strlen(capSeq)) <= MAX_CAP_SEQ)
		{
			CapMap[i].capSeq = capSeq;
			CapMap[i].seqLen = len;
		}
		else if (CapMap[i].capSeq)
			CapMap[i].seqLen = strlen(CapMap[i].capSeq);

	TTBorders(false);
	TTPutSeq(SeqCursorModeStart);
	TTPutSeq(SeqClearScreen);
	TTPutSeq(enableAltCharSet);
	signal(SIGWINCH, SigWINCHHandler);
	TTHideCursor();
	return 1;
}

//---------------------------------------------------------------------------
void ExitVideo()
{
	assert(VidInitCount > 0);
	if (VidInitCount-- == 1)
	{
		TTUserCursor();
		TTBorders(false);
		TTPutSeq(SeqClearAttributes);
		TTPutSeq(SeqClearScreen);
		TTPutSeq(SeqCursorNorm);
		TTPutSeq(SeqCursorModeEnd);
		TTFlush();
		tcsetattr(0, TCSANOW, &DCShellModes);
		signal(SIGWINCH, SIG_DFL);
		ResetGlobals();
	}
}

//---------------------------------------------------------------------------
// returns count of keys added to buffer
//
unsigned TTYGetKeys(unsigned maxKeys, unsigned *keys, unsigned millisToWait)
{
	static char inBuffer[1 + MAX_CAP_SEQ]; // be able to hold ESC + MAX_CAP_SEQ
	static unsigned inBufUsed = 0;
	static timeval partialSeqStartTime = {0, 0};

	assert(maxKeys);
	if (maxKeys == 0)
		return 0;

	if (TTResized)
	{
		TTResized = false;
		*keys = REFRESHDISPLAYKEY;
		return 1;
	}

	// first check to see if data is already available
 	timeval timeout = {0, 0};
  	fd_set fdin;
  	FD_ZERO(&fdin);
  	FD_SET(0, &fdin);
	int fdcount = select(1, &fdin, 0, 0, &timeout);
	if (fdcount == 0)
	{
		// no data available, flush the screen and wait for data
		assert(TTBufp >= TTBufHeadEnd);
		if (inBufUsed == 0 && TTBufHeadEnd != TTBufp && TTBufp != TTBuffer)
		{
			TTUserCursor();
			TTFlush(); // got nothing else to do, flush the screen
			TTHideCursor(); // preload buffer with cursor off, but don't flush
			TTBufHeadEnd = TTBufp;
		}
		timeout.tv_usec = millisToWait;
		FD_ZERO(&fdin);
		FD_SET(0, &fdin);
		fdcount = select(1, &fdin, 0, 0, &timeout);
	}
	ssize_t newChars = 0;
	if (fdcount == 1)
	{
		newChars = read(0, inBuffer + inBufUsed, sizeof inBuffer - inBufUsed);
		assert(newChars != -1);
		if (newChars == -1)
			newChars = 0;
		//fprintf(stderr, "used %u, adding %lu: %.*s\n", inBufUsed, newChars, (int)newChars, inBuffer + inBufUsed);
	}

	if (!newChars && inBufUsed)
	{
		/* if we have a partial sequence that is less than KEY_SEQ_WAIT
		 * microseconds old, more still may be coming. leave partial
		 * sequence data alone, return 0 -- we may get the rest next time
		 */
		timeval now, partialSeqAge;
		gettimeofday(&now, 0);
		timersub(&now, &partialSeqStartTime, &partialSeqAge);
		if (!partialSeqAge.tv_sec && partialSeqAge.tv_usec <= KEY_SEQ_WAIT)
			return 0;
	}

	inBufUsed += newChars;
	char *cur = inBuffer;
	bool escaped = false;
	unsigned i, keyCount = 0, leftlen;

	while (keyCount < maxKeys)
	{
		leftlen = inBufUsed - (cur - inBuffer);

		if (leftlen == 0)
		{
			inBufUsed = 0;
			if (escaped)
				if (newChars)
				{
					gettimeofday(&partialSeqStartTime, 0);
					inBufUsed = 1;
					inBuffer[0] = '\x1b';
				}
				else
					keys[keyCount++] = ESC;
			//fprintf(stderr, "input cleared\n");
			return keyCount;
		}

		/* sorting the CapMap table at init time and then doing a binary
		 * search here would be a good idea.
		 */
		for (i = 0; i < ELEMCOUNT(CapMap); i++)
		{
			if (CapMap[i].capSeq && *CapMap[i].capSeq == *cur)
			{
				unsigned len = leftlen;
				if (len > CapMap[i].seqLen)
					len = CapMap[i].seqLen;
				assert(len);
				if (memcmp(CapMap[i].capSeq, cur, len) == 0)
				{
					if (len == CapMap[i].seqLen)
					{
						keys[keyCount++] = CapMap[i].key | (escaped? ALT: 0);
						cur += len;
						escaped = false;
						break;
					}
					if (newChars)
					{
						// record start time of partial sequence
						gettimeofday(&partialSeqStartTime, 0);
						//fprintf(stderr, "processing partial sequence\n");
						goto out;
					}
				}
			}
		}
		if (i == ELEMCOUNT(CapMap))
		{
			if (*cur != '\x1b')
			{
				if (!escaped)
					keys[keyCount++] = *cur > 0 && *cur < '\x20'?
							(*cur | 0x40 | DCTRL): *cur;
				else if (*cur >= 0 && *cur < '\x20')
					keys[keyCount++] = *cur | 0x40 | DCTRL | ALT;
				else
					keys[keyCount++] = *cur >= 'A' && *cur <= 'Z'?
							*cur | SHIFT | ALT: toupper(*cur) | ALT;
				escaped = false;
			}
			else if (escaped)
			{
				keys[keyCount++] = ALT | ESC;
				escaped = false;
			}
			else
				escaped = true;
			cur++;
		}
	}
	assert(!escaped);
	leftlen = inBufUsed - (cur - inBuffer);

out:
	if (cur != inBuffer)
	{
		if (escaped)
		{
			cur--;
			leftlen++;
			//fprintf(stderr, "adjusting for incomplete escape\n");
		}
		memmove(inBuffer, cur, leftlen);
		inBufUsed = leftlen;
	}
	//fprintf(stderr, "returning %u keys, leftover %u: %.*s\n", keyCount, inBufUsed, inBufUsed, inBuffer);
	return keyCount;
}

//---------------------------------------------------------------------------
void SetCursorType(CURSORTYPE cursorType)
{
	assert(cursorType == CURSOR_NONE || cursorType == CURSOR_BIG || cursorType == CURSOR_SMALL);
	UserCursorType = cursorType;
}

//---------------------------------------------------------------------------
void FakeCursor(CURSORFAKE fakeStyle, VBYTE cursorAttr)
{
	assert(fakeStyle == CF_TTY_ONLY || fakeStyle == CF_FAKE_ONLY || fakeStyle == CF_FAKE_AND_TTY);
	UserCursorFake = fakeStyle;
	UserFakeCursorAttr = cursorAttr;
}

//---------------------------------------------------------------------------
void SetCursor(unsigned row, unsigned col)
{
	UserCurRow = row;
	UserCurCol = col;
	TTPositionCursor(row, col);
}

//---------------------------------------------------------------------------
void GetCursor(unsigned *row, unsigned *col)
{
	assert(UserCurRow < videoRows && UserCurCol < videoCols);
	*row = UserCurRow;
	*col = UserCurCol;
}

//---------------------------------------------------------------------------
void VideoGetZone(unsigned row, unsigned col, unsigned high, unsigned wide,
		VCHAR *dest)
{
	for (unsigned i = 0; i < high; i++)
		TTRead(row + i, col, wide, dest + i * wide);
}


//---------------------------------------------------------------------------
void VideoPutZone(unsigned row, unsigned col, unsigned high, unsigned wide,
		VCHAR *source)
{
	for (unsigned i = 0; i < high; i++)
		TTWrite(row + i, col, wide, source + i * wide);
}

/* =================================================================== */
#endif
