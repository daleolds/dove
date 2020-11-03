#if defined(DOVE_FOR_WIN32)

#define WIN32_EXTRA_LEAN
#include <windows.h>
#include <assert.h>
#include "pvideo.h"

unsigned videoRows, videoCols;

static HANDLE hConsoleOutput;

int InitVideo(void)
{
	if ((hConsoleOutput = GetStdHandle(STD_OUTPUT_HANDLE))
			== INVALID_HANDLE_VALUE)
		return 0;

	CONSOLE_SCREEN_BUFFER_INFO screenInfo;
	if (!GetConsoleScreenBufferInfo(hConsoleOutput, &screenInfo))
		return 0;

	videoRows = screenInfo.dwSize.Y;
	videoCols = screenInfo.dwSize.X;

	return 1;
}

void ExitVideo(void) {SetCursorType(CURSOR_SMALL); }

void SetCursor(unsigned row, unsigned col)
{
	COORD pos = {col, row};
	SetConsoleCursorPosition(hConsoleOutput, pos);
}

void GetCursor(unsigned *row, unsigned *col)
{
	CONSOLE_SCREEN_BUFFER_INFO screenInfo;
	if (!GetConsoleScreenBufferInfo(hConsoleOutput, &screenInfo))
		*row = *col = 0;
	else
	{
		*col = screenInfo.dwCursorPosition.X;
		*row = screenInfo.dwCursorPosition.Y;
	}
}

void SetCursorType(CURSORTYPE ct)
{
	CONSOLE_CURSOR_INFO info;
	info.dwSize = ct == CURSOR_BIG? 98: 8;
	info.bVisible = ct == CURSOR_NONE? 0: 1;
	SetConsoleCursorInfo(hConsoleOutput, &info);
}


void FakeCursor(CURSORFAKE fakeStyle, VBYTE cursorAttr)
{
	assert(fakeStyle == CF_TTY_ONLY || fakeStyle == CF_FAKE_ONLY || fakeStyle == CF_FAKE_AND_TTY);
}

void VideoGetZone(unsigned row, unsigned col, unsigned high, unsigned wide, VCHAR *dest)
{
	COORD zoneSize = {wide, high}, zoneStart = {0, 0};
	SMALL_RECT zoneSource = {col, row, col + wide - 1, row + high - 1};
	ReadConsoleOutput(hConsoleOutput, (CHAR_INFO *)dest, zoneSize,
			zoneStart, &zoneSource);
}

void VideoPutZone(unsigned row, unsigned col, unsigned high, unsigned wide, VCHAR *source)
{
	COORD zoneSize = {wide, high}, zoneStart = {0, 0};
	SMALL_RECT zoneSource = {col, row, col + wide - 1, row + high - 1};
	WriteConsoleOutput(hConsoleOutput, (CHAR_INFO *)source, zoneSize,
			zoneStart, &zoneSource);
}

/* =================================================================== */
#endif
