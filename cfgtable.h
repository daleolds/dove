#ifndef CFGTABLE_H
#define CFGTABLE_H

#include <pvideo.h>

/*-----------------------------------------------------------------------------
 * configuration definitions
 */
enum MODEFLAGS
{
	MODE_INDENT = 0x01, MODE_CASESENSITIVE = 0x02, MODE_MAKEBAKFILE = 0x04,
	MODE_INSERT = 0x08, MODE_REALTABS = 0x10, MODE_SHOW_CTRL_CHARS = 0x20,
	MODE_CR_LF = 0x40, MODE_TTY_CURSOR = 0x80, MODECOUNT = 8
};

enum NUMBER { TABSIZE, WORDWRAPCOL, NUMBERCOUNT };

enum ATTRIB
{
	VIEWTEXT, VIEWTEXTCURSOR, VIEWMARK, VIEWMARKCURSOR, VIEWBORDER,
	VIEWLONGLINE, MSGLINE, MSGLINECURSOR, LINECOL, TIME, HELPBORDER,
	HELPTEXT, DELETEBORDER, DELETETEXT, DELETEMARK, ATTRIBCOUNT
};

typedef struct CONFIGINFO
{
	char message[28];
	unsigned short version, modes, number[NUMBERCOUNT];
	VBYTE attrib[ATTRIBCOUNT];
} CONFIGINFO;

extern CONFIGINFO cfg;
extern unsigned modes, tabSize, wordWrapCol;

/*----------------------------------------------------------------------------
 */
typedef struct BINDING { unsigned short type: 2, data: 14, key; } BINDING;

enum BINDINGTYPE {NOTBOUND, INSCHAR, MACROKEY, FUNCTIONKEY};

extern BINDING *bindingTable;
extern unsigned bindingCount;

#define MACROCOUNT 10
extern unsigned short *macroTable[MACROCOUNT];

/*---------------------------------------------------------------------------
 * used to divide the key bindings into
 * categories for the help screens
 */
typedef struct CATEGORY
{
	unsigned entries;
	const char *title;
} CATEGORY;

typedef enum CATEGORYTYPE
{
	CT_FILE, CT_EDIT, CT_SEARCH, CT_CURSOR, CT_BLOCK, CT_MISC, CT_OPTION,
	CT_VIEW, CT_HELP, CT_COUNT
} CATEGORYTYPE;

extern CATEGORY categories[CT_COUNT];

/*---------------------------------------------------------------------------
 */
typedef struct KEYFUNCTION
{
	int (*nonr)(void), (*rept)(int n);
	CATEGORYTYPE category;
	const char *description;
	unsigned defaultKey, defaultKeyB;
}	KEYFUNCTION;

#define MAXFUNCTIONNAME 27

extern KEYFUNCTION functionTable[];
extern unsigned functionCount;

/*--------------------------------------------------------------------------
 */
extern const char *copyrightStrings[];
void ConfigMain();
int FindBinding(unsigned key, bool insert);
#define MAXKEYNAME	(5 + 6 + 8 + 1)
char *KeyName(int key, int braceFlag);
unsigned MacroLen(unsigned short *m);
bool ReadConfig();
bool WriteConfig();
void ExitConfig();
void SetActiveModes();

/*===========================================================================*/
#endif
