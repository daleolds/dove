#if defined(DOVE_FOR_DOS)

#error code preserved from previous versions of DOVE, but not currently supported


#include <dos.h>
#include "pvideo.h"

unsigned videoRows, videoCols;
int videoMode;

#define MONOSEG	0xb000
#define COLORSEG	0xb800
static unsigned videoSegment;

/* ----------- initialize everything ------------- */
#define OLD_MODE_CODE	0
int videoSnowCheck = 0;

int InitVideo(void)
{
	VBYTE originalDisplayPage;
//	if (mode != VIDEOCURRENT)
//	{
//		_AH = 0;
//		_AL = mode;
//		geninterrupt(0x10);
//	}
	_AH = 15;
	geninterrupt(0x10);
	videoCols = _AH;
	videoMode = _AL;
	originalDisplayPage = _BH;
	if ((videoRows = peekb(0x40, 0x84) & 0xff) == 0)
		videoRows = 24;
	videoRows++;
	videoSegment = (videoMode == VIDEO80X25MONO? MONOSEG: COLORSEG)
			+ originalDisplayPage
			* ((((unsigned)videoCols * videoRows - 1) / 1024) * 1024 + 1024);
	return 1;
}

/* ----------- clean up everything ------------- */
void ExitVideo(void)
{
}

/* ------------ return the cursor position ------------- */
void GetCursor(unsigned *row, unsigned *col)
{
	_BX = 0;
	_AX = 0x0300;
	geninterrupt(0x10);
	*col = _DL;
	*row = _DH;
}

/* ----------- set cursor size --------------- */
void SetCursorType(CURSORTYPE ct)
{
	if (ct == CURSOR_NONE)
	{
		SetCursor(videoRows, 0);
		return;
	}
	_CX = (videoMode == VIDEO80X25MONO?
	  (ct == CURSOR_BIG? 0x020c: 0x0b0c): (ct == CURSOR_BIG? 0x0107: 0x0607));
	_BX = 0;
	_AX = 0x0100;
	geninterrupt(0x10);
}

/* ----------- position the cursor ------------- */
void SetCursor(unsigned row, unsigned col)
{
	_BX = 0;
	_DX = ((row << 8) & 0xff00) + col;
	_AX = 0x0200;
	geninterrupt(0x10);
}

/*---------------------------------------------------------------------------
 * read a bunch of characters and attribute from video RAM
 */
static void VideoRead(int offset, int len, VCHAR *data)
{
	_DX = videoSegment;
	_CX = len;						/* get length (in words) to write */
	_SI = offset * sizeof(VCHAR);	/* get offset (position on screen) */

	asm	les	di, data				/* get pointer to data */
	asm	push ds
	_DS = _DX;						/* set up ds to point to video memory */
	asm repz movsw
	asm	pop	ds
}

/* ------------------------------------------------------------------- */
void VideoGetZone(unsigned row, unsigned col, unsigned high, unsigned wide, VCHAR *dest)
{
	int h = high;
	while (--h >= 0)
		VideoRead((row + h) * videoCols + col, wide, dest + h * wide);
}

/*----------------------------------------------------------------------------
 */
void VideoWrite(int offset, int len, VCHAR *data)
{
	asm	push ds;

	_DL = videoSnowCheck;
	_DI = offset * sizeof(VCHAR);	/* get offset (position on screen) */
	_ES = videoSegment;

	asm	lds	si, data;				/* get pointer to data */

	if (!_DL)
	{
		_CX = len;					/* get length (in words) to write */
		asm repz movsw;
	}
	else
	{
		_BX = len;					/* get length (in words) to write */
		_DX = 0x3da;				/* status port on color board */
	VidW10:
		asm	in al, dx;				/* get the status of display */
		asm	test al, 1;
		asm	jnz VidW10;				/* loop til no retrace */
		asm	cli;
		asm	lodsw;					/* get a character and attribute */
		asm	mov cx, ax;
	VidW20:
		asm	in al, dx;				/* get the status of display */
		asm	test al, 1;
		asm	jz VidW20;				/* loop till retrace */
		asm	mov ax, cx;
		asm	stosw;					/* write a character and attribute */
		asm	sti;
		asm	dec bx;
		asm	jnz VidW10;				/* loop if not done */
	VidWx:
		;
	}
	asm	pop ds;
}

/* ------------------------------------------------------------------- */
void VideoPutZone(unsigned row, unsigned col, unsigned high, unsigned wide, VCHAR *source)
{
	int h = high;
	while (--h >= 0)
		VideoWrite((row + h) * videoCols + col, wide, source + h * wide);
}

/* =================================================================== */
#endif