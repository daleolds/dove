#ifndef PVIDEO_H
#define PVIDEO_H 1

#define ELEMCOUNT(x) (sizeof(x)/sizeof((x)[0]))

typedef unsigned char VBYTE;
#define MAX_VBYTE 0xff

/*-----------------------------------------------------------------------------
 * video colors
 */
#define VBLACK		0
#define VBLUE		1
#define VGREEN		2
#define VCYAN		3
#define VRED		4
#define VMAGENTA	5
#define VYELLOW		6
#define VWHITE		7

#define VBRIGHT		8 // only applies to foreground

#define MakeAttrib(fg, bg) ((fg) | ((bg) << 4))

/*-----------------------------------------------------------------------------
 * types
 */
#if defined(DOVE_FOR_DOS) || defined(DOVE_FOR_GNU)
typedef struct VCHAR { VBYTE c, a; } VCHAR;
#else
typedef struct VCHAR { VBYTE c, fill; unsigned short a; } VCHAR;
#endif

/*-----------------------------------------------------------------------------
 * function prototypes and globals
 */
extern unsigned videoRows, videoCols;

int InitVideo(void);
void ExitVideo(void);

void SetCursor(unsigned row, unsigned col);
void GetCursor(unsigned *row, unsigned *col);

typedef enum CURSORFAKE { CF_TTY_ONLY, CF_FAKE_ONLY, CF_FAKE_AND_TTY } CURSORFAKE;
void FakeCursor(CURSORFAKE fakeStyle, VBYTE cursorAttr);

typedef enum CURSORTYPE { CURSOR_NONE, CURSOR_BIG, CURSOR_SMALL } CURSORTYPE;
void SetCursorType(CURSORTYPE cursorType);

void VideoGetZone(unsigned row, unsigned col, unsigned high, unsigned wide, VCHAR *dest);
void VideoPutZone(unsigned row, unsigned col, unsigned high, unsigned wide, VCHAR *source);

/*===========================================================================*/
#endif
