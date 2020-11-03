#include <cfgtable.h>

/*-----------------------------------------------------------------------------
 * constants
 */
#define NORMVID	(MakeAttrib(VCYAN, VBLACK))
#define BOLDVID	(MakeAttrib(VCYAN, VBLACK) | VBRIGHT)
#define REVVID	(MakeAttrib(VBLACK, VWHITE))
#define HELPVID	(MakeAttrib(VGREEN, VBLACK) | VBRIGHT)

/*-----------------------------------------------------------------------------
 * prototypes
 */

/* bind */
int KeyBindings(unsigned row, unsigned col);

/* color */
int Colors(unsigned row, unsigned col);

/* main */
void Help(const char *line0, const char *line1);
void Message(const char *fmt, ...);
extern MENU mainMenu, colorMenu, modeMenu;

/* mode */
int Modes(unsigned row, unsigned col);
int Numbers(unsigned row, unsigned col);

/* sample */
int CreateSample(unsigned row, unsigned col);
void DeleteSample(void);
void SampleWindow(unsigned selection, VBYTE attrib, VBYTE *cfgAttrib);

/*===========================================================================*/
