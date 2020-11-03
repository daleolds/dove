#ifndef DUIWIN_H
#define DUIWIN_H 1

#include <stddef.h>
#include <pvideo.h>

/*---------------------------------------------------------------------------
 * possible border types
 */
enum BORDERTYPE {SINGLE, DOUBLE, DOUBLESIDES, DOUBLEENDS, SINGLEPULLDOWN, CLEARBORDER};

/*---------------------------------------------------------------------------
 * window controller structure
 */
struct Window
{
	unsigned curRow, curCol, row, col, high, wide, flags;
	VCHAR fillChar, save[1];
};

/* possible flags */
#define BORDERLESS	0x01
#define DESTRUCTIVE	0x02
#define BOTTOMTITLE	0x04
#define TILEDBORDER	0x08 /* only actually displays title and one side */
#define EDGEBORDER	0x10 /* if window is flush against edge, no border there */
#define VERTICAL	0x20 /* only used by menus */
#define AUTOLOCATE	0x40 /* only used by menus */

/*---------------------------------------------------------------------------
 * function prototypes, macros and globals
 */
#define SelectWindow(wnd) (selectedWindow = (wnd))
#define WSetCursor(r, c) (selectedWindow->curRow = (r), selectedWindow->curCol = (c))
#define WGetCursor(rp, cp) (*(rp) = selectedWindow->curRow, *(cp) = selectedWindow->curCol)
#define WSetFillChar(ch) (selectedWindow->fillChar.c = (ch))
#define WSetAttribute(attr) (selectedWindow->fillChar.a = (attr))

extern Window *selectedWindow;
extern VCHAR *windowTempLine;

int InitWindows(void);
void ExitWindows(void);
Window *CreateWindow(unsigned row, unsigned col, unsigned high, unsigned wide, unsigned flags);
void DeleteWindow(Window *wnd);
void ClearWindow(void);
void ClearEOL(void);
void WriteBorder(const char *title, enum BORDERTYPE btype, VBYTE attrib);
void WriteLine(const char *s);
void WriteString(const char *s);
void WriteData(size_t len, const char *data);
void WriteChar(char c);
void _WriteTempLine(size_t len);

/*===========================================================================*/
#endif
