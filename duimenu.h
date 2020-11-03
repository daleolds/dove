#ifndef DUIMENU_H
#define DUIMENU_H 1

#include <duiwin.h>

/*-----------------------------------------------------------------------------
 * menu option definitions
 */
typedef struct MENUOPTION
{
	const char *name;
	int value;
	struct MENU *subMenu;
	int (*function)(unsigned row, unsigned col);
} MENUOPTION;

/*-----------------------------------------------------------------------------
 * menu definitions
 */
typedef struct MENU
{
	/* MENU flag bits are defined with the window flag bits */
	unsigned flags, selection, row, col;
	VBYTE normalAttrib, selectAttrib, accentAttrib;
	enum BORDERTYPE	borderType;
	unsigned optionCount;
	MENUOPTION *options;
	const char *title;
	Window *window;
	void (*infoFunction)(void);
} MENU;

/*-----------------------------------------------------------------------------
 * function prototypes
 */
int InitMenus(void);
void ExitMenus(void);
int CreateMenu(MENU *m);
void DeleteMenu(MENU *m);
int ProcessMenu(MENU *m);

/*===========================================================================*/
#endif
