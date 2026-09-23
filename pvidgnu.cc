#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/time.h>

#include <assert.h>

#include <sys/ioctl.h>
#include <termios.h>
#include <errno.h>
#include <fcntl.h>

#include "pvideo.h"
#include "pkeybrd.h"

unsigned videoRows, videoCols;

#define MAX_CAP_SEQ	32
#define KEY_SEQ_WAIT 1000 // microseconds to wait to complete key sequence

#define MIN_ROWS 4
#define MIN_COLS 20

// no particular known limit, just no need to get close to signed 16-bit values
#define MAX_ROWS 8 * 1024
#define MAX_COLS 8 * 1024

static struct termios DCShellModes, DCTermModes;

// fixed xterm/ANSI sequences, used instead of looking them up in terminfo
static const char SeqCursorModeStart[] = "\033[?1049h", SeqCursorModeEnd[] = "\033[?1049l";
static const char SeqClearScreen[] = "\033[H\033[2J";
static const char SeqCursorGone[] = "\033[?25l", SeqCursorNorm[] = "\033[?12l\033[?25h";
static const char SeqCursorHigh[] = "\033[?12;25h";
static const char SeqStartBorders[] = "\033(0", SeqEndBorders[] = "\033(B";
static const char SeqSetBold[] = "\033[1m", SeqClearAttributes[] = "\033[m";
static unsigned TTYCurRow, TTYCurCol, UserCurRow, UserCurCol;
static VCHAR *Screen = 0;
static CURSORTYPE UserCursorType = CURSOR_SMALL, TTYCursorType = CURSOR_SMALL;
static CURSORFAKE UserCursorFake = CF_TTY_ONLY;
static VBYTE UserFakeCursorAttr, CurAttr, RealCursorAttr;
static bool CurBold = false;

//---------------------------------------------------------------------------
static void ResetGlobals()
{
	videoRows = videoCols = 0;
	memset(&DCShellModes, 0, sizeof DCShellModes);
	memset(&DCTermModes, 0, sizeof DCTermModes);
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
// mapping of xterm, Linux console, screen and tmux key sequences to key values

struct CM
{
	const char *capSeq;
	unsigned seqLen;
	unsigned key;
} CapMap[] =
{
	/* arrow keys */
	{"\x1b[D", 0, LEFT},
	{"\x1bOD", 0, LEFT},
	{"\x1b[d", 0, SHIFT | LEFT},
	{"\x1b[1;2D", 0, SHIFT | LEFT},
	{"\x1b[1;3D", 0, ALT | LEFT},
//	{"\x1b\x1b[D", 0, ALT | LEFT},
//	{"\x1b\x1b[d", 0, ALT | SHIFT | LEFT},
	{"\x1b[1;4D", 0, ALT | SHIFT | LEFT},
	{"\x1b[1;5D", 0, DCTRL | LEFT},
	{"\x1bOd", 0, DCTRL | LEFT},
	{"\x1b[1;6D", 0, DCTRL | SHIFT | LEFT},

	{"\x1b[B", 0, DOWN},
	{"\x1bOB", 0, DOWN},
	{"\x1b[1;2B", 0, SHIFT | DOWN},
	{"\x1b[b", 0, SHIFT | DOWN},
	{"\x1b[1;3B", 0, ALT | DOWN},
//	{"\x1b\x1b[B", 0, ALT | DOWN},
	{"\x1b[1;4B", 0, ALT | SHIFT | DOWN},
//	{"\x1b\x1b[b", 0, ALT | SHIFT | DOWN},
	{"\x1b[1;5B", 0, DCTRL | DOWN},
	{"\x1bOb", 0, DCTRL | DOWN},
	{"\x1b[1;6B", 0, DCTRL | SHIFT | DOWN},

	{"\x1bOC", 0, RIGHT},
	{"\x1b[C", 0, RIGHT},
	{"\x1b[c", 0, SHIFT | RIGHT},
	{"\x1b[1;2C", 0, SHIFT | RIGHT},
//	{"\x1b\x1b[C", 0, ALT | RIGHT},
	{"\x1b[1;3C", 0, ALT | RIGHT},
	{"\x1b[1;4C", 0, ALT | SHIFT | RIGHT},
//	{"\x1b\x1b[c", 0, ALT | SHIFT | RIGHT},
	{"\x1b[1;5C", 0, DCTRL | RIGHT},
	{"\x1bOc", 0, DCTRL | RIGHT},
	{"\x1b[1;6C", 0, DCTRL | SHIFT | RIGHT},

	{"\x1bOA", 0, UP},
	{"\x1b[A", 0, UP},
	{"\x1b[1;2A", 0, SHIFT | UP},
	{"\x1b[a", 0, SHIFT | UP},
//	{"\x1b\x1b[A", 0, ALT | UP},
	{"\x1b[1;3A", 0, ALT | UP},
//	{"\x1b\x1b[a", 0, ALT | SHIFT | UP},
	{"\x1b[1;4A", 0, ALT | SHIFT | UP},
	{"\x1bOa", 0, DCTRL | UP},
	{"\x1b[1;5A", 0, DCTRL | UP},
	{"\x1b[1;6A", 0, DCTRL | SHIFT | UP},

	/* enter, backspace, tab */
	{"\r", 0, ENTER},
	{"\n", 0, DCTRL | ENTER},
	{"\x1b\n", 0, ALT | ENTER},

	{"\t", 0, TAB},
	{"\x1b[Z", 0, SHIFT | TAB},
	{"\x1b\t", 0, SHIFT | TAB}, // Linux console
	{"\x7f", 0, BSP},
	{"\x1b\x7f", 0, ALT | BSP},
	{"\x08", 0, DCTRL | BSP},

	/* home, end, page up/down, insert, delete */
	{"\x1b[7~", 0, HOME},
	{"\x1bOH", 0, HOME},
	{"\x1b[H", 0, HOME},
	{"\x1b[1~", 0, HOME},
	{"\x1b[7$", 0, SHIFT | HOME},
	{"\x1b[1;2H", 0, SHIFT | HOME},
	{"\x1b[7^", 0, DCTRL | HOME},
	{"\x1b[7@", 0, DCTRL | SHIFT | HOME},

	{"\x1b[F", 0, END},
	{"\x1bOF", 0, END},
	{"\x1b[8~", 0, END},
	{"\x1b[4~", 0, END},
	{"\x1b[8$", 0, SHIFT | END},
	{"\x1b[1;2F", 0, SHIFT | END},
	{"\x1b[8^", 0, DCTRL | END},
	{"\x1b[8@", 0, DCTRL | SHIFT | END},

	{"\x1b[6~", 0, PGDN},
	{"\x1b[6^", 0, DCTRL | PGDN},
	{"\x1b[6;5~", 0, DCTRL | PGDN},
	{"\x1b[6;3~", 0, ALT | PGDN},

	{"\x1b[5~", 0, PGUP},
	{"\x1b[5^", 0, DCTRL | PGUP},
	{"\x1b[5;5~", 0, DCTRL | PGUP},
	{"\x1b[5;3~", 0, ALT | PGUP},

	{"\x1b[2~", 0, INS},
	{"\x1b[2;3~", 0, ALT | INS},
	{"\x1b[2^", 0, DCTRL | INS},
	{"\x1b[2@", 0, DCTRL | SHIFT | INS},

	{"\x1b[3~", 0, DEL},
	{"\x1b[3;2~", 0, SHIFT | DEL},
	{"\x1b[3$", 0, SHIFT | DEL},
	{"\x1b[3;3~", 0, ALT | DEL},
	{"\x1b[3;5~", 0, DCTRL | DEL},
	{"\x1b[3^", 0, DCTRL | DEL},
	{"\x1b[3@", 0, DCTRL | SHIFT | DEL},

	/* function keys */
	{"\x1bOP", 0, F1},
	{"\x1b[11~", 0, F1},
	{"\x1b[[A", 0, F1}, // Linux console
	{"\x1bOQ", 0, F2},
	{"\x1b[12~", 0, F2},
	{"\x1b[[B", 0, F2}, // Linux console
	{"\x1bOR", 0, F3},
	{"\x1b[13~", 0, F3},
	{"\x1b[[C", 0, F3}, // Linux console
	{"\x1bOS", 0, F4},
	{"\x1b[14~", 0, F4},
	{"\x1b[[D", 0, F4}, // Linux console
	{"\x1bOt", 0, F5},
	{"\x1b[15~", 0, F5},
	{"\x1b[[E", 0, F5}, // Linux console
	{"\x1bOu", 0, F6},
	{"\x1b[17~", 0, F6},
	{"\x1bOv", 0, F7},
	{"\x1b[18~", 0, F7},
	{"\x1bOl", 0, F8},
	{"\x1b[19~", 0, F8},
	{"\x1bOw", 0, F9},
	{"\x1b[20~", 0, F9},
	{"\x1bOx", 0, F10},
	{"\x1b[21~", 0, F10},
	{"\x1b[23~", 0, F11},
	{"\x1b[24~", 0, F12},
	{"\x1b[25~", 0, SHIFT | F3},
	{"\x1b[26~", 0, SHIFT | F4},
	{"\x1b[28~", 0, SHIFT | F5},
	{"\x1b[29~", 0, SHIFT | F6},
	{"\x1b[31~", 0, SHIFT | F7},
	{"\x1b[32~", 0, SHIFT | F8},
	{"\x1b[33~", 0, SHIFT | F9},
	{"\x1b[34~", 0, SHIFT | F10},
	{"\x1b[1;2P", 0, SHIFT | F1},
	{"\x1b[1;2Q", 0, SHIFT | F2},
	{"\x1b[1;2R", 0, SHIFT | F3},
	{"\x1b[1;2S", 0, SHIFT | F4},
	{"\x1b[15;2~", 0, SHIFT | F5},
	{"\x1b[17;2~", 0, SHIFT | F6},
	{"\x1b[18;2~", 0, SHIFT | F7},
	{"\x1b[19;2~", 0, SHIFT | F8},
	{"\x1b[20;2~", 0, SHIFT | F9},
	{"\x1b[21;2~", 0, SHIFT | F10},
	{"\x1b[23;2~", 0, SHIFT | F11},
	{"\x1b[24;2~", 0, SHIFT | F12},

	/* keypad keys */
	{"\x1bOw", 0, NP7},
	{"\x1bOy", 0, NP9},
	{"\x1bOu", 0, NP5},
	{"\x1b[G", 0, NP5}, // Linux console
	{"\x1bOq", 0, NP1},
	{"\x1bOs", 0, NP3},
	{"\x1bOM", 0, NPENTER},
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

// the same line drawing chars in the VT100 alternate character set
static const char BorderChars[] = "lkjmxqlkjmxqlkjmxqlkjmxqwwjmxq";

static char TTBuffer[8 * 1024], *TTBufp = TTBuffer, *TTBufHeadEnd = TTBuffer;

//---------------------------------------------------------------------------
static void TTFlush()
{
	if (TTBufp != TTBuffer)
	{
		ssize_t len = TTBufp - TTBuffer;
		ssize_t wlen = len? write(1, TTBuffer, len): 0;
		assert(wlen != -1);
		(void)wlen;
		//if (wlen != len) fprintf(stderr, "flush write expected: %ld, got %ld\n", len, wlen);
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
		char seq[24];
		snprintf(seq, sizeof seq, "\033[%u;%uH", TTYCurRow + 1, TTYCurCol + 1);
		TTPutSeq(seq);
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
	// DUI color order is BGR, ANSI is RGB
	static const int DUI2ANSIColor[8] = { 0, 4, 2, 6, 1, 5, 3, 7 };

	unsigned i;
	VCHAR *svp, *dvp;

	//assert(row < videoRows && col < videoCols);
	if (row >= videoRows ||col >= videoCols)
		fprintf(stderr, "row: %u (%u), col: %u (%u)", row, videoRows, col, videoCols);
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
				if (CurBold != (svp->a & VBRIGHT) != 0)
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
				char seq[16];
				snprintf(seq, sizeof seq, "\033[3%d;4%dm", DUI2ANSIColor[svp->a & 0x7],
						DUI2ANSIColor[(svp->a >> 4) & 0x7]);
				TTPutSeq(seq);
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
	if (UserCursorFake != CF_FAKE_ONLY)
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
	if (sig != SIGWINCH)
		return;
	struct winsize osize, nsize = {0,0,0,0};
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &osize) == -1)
		assert(errno != 0);
	nsize.ws_row = osize.ws_row < MIN_ROWS? MIN_ROWS:
			osize.ws_row > MAX_ROWS? MAX_ROWS: osize.ws_row;
	nsize.ws_col = osize.ws_col < MIN_COLS? MIN_COLS:
			osize.ws_col > MAX_COLS? MAX_COLS: osize.ws_col;
	if ((osize.ws_row != nsize.ws_row || osize.ws_col != nsize.ws_col)
			&& ioctl(STDOUT_FILENO, TIOCSWINSZ, &nsize) == -1)
		assert(errno != 0);
	if (nsize.ws_row != videoRows || nsize.ws_col != videoCols)
		TTResized = true;
	signal(SIGWINCH, SigWINCHHandler);
}

//---------------------------------------------------------------------------
static unsigned VidInitCount = 0;

int InitVideo()
{
	unsigned i;
	const char *termName = getenv("TERM");
	struct winsize ws = {0, 0, 0, 0};

	if (VidInitCount++)
		return 1;

	if (!termName || !*termName || strcmp(termName, "dumb") == 0)
	{
		puts("InitVideo failed, TERM is unset or dumb");
		VidInitCount--;
		return 0;
	}
	if (tcgetattr(0, &DCShellModes) != 0 || tcgetattr(0, &DCTermModes) != 0)
	{
		VidInitCount--;
		return 0;
	}

	// window size from the tty, else $LINES/$COLUMNS, else 24x80
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row && ws.ws_col)
	{
		videoRows = ws.ws_row;
		videoCols = ws.ws_col;
	}
	else
	{
		const char *env;
		videoRows = (env = getenv("LINES"))? atoi(env): 24;
		videoCols = (env = getenv("COLUMNS"))? atoi(env): 80;
	}
	if (videoRows < MIN_ROWS || videoCols < MIN_COLS
			|| videoRows > MAX_ROWS || videoCols > MAX_COLS)
	{
		puts("InitVideo failed, rows or columns out of bounds");
		VidInitCount--;
		return 0;
	}
//	DCTermModes.c_iflag = 0;
//	DCTermModes.c_lflag = 0;
//	DCTermModes.c_oflag = 0;
	cfmakeraw(&DCTermModes);
	DCTermModes.c_cc[VMIN] = 1;
	DCTermModes.c_cc[VTIME] = 0;
	if (tcsetattr(0, TCSANOW, &DCTermModes) != 0
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

	for (i = 0; i < ELEMCOUNT(CapMap); i++)
		CapMap[i].seqLen = strlen(CapMap[i].capSeq);

	TTBorders(false);
	TTPutSeq(SeqCursorModeStart);
	TTPutSeq(SeqClearScreen);
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
		assert(inBufUsed <= sizeof inBuffer);
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
			{
				if (newChars)
				{
					gettimeofday(&partialSeqStartTime, 0);
					inBufUsed = 1;
					inBuffer[0] = '\x1b';
				}
				else
					keys[keyCount++] = ESC;
			}
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
