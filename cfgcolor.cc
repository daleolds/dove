#include <pkeybrd.h>
#include <pvideo.h>
#include <duiwin.h>
#include <duimenu.h>
#include <cfgcommo.h>

static void ColorSample(void);

#define SelectedAttrib() (foreGroundMenu.selection | (backGroundMenu.selection << 4))

/*-----------------------------------------------------------------------------
 * foreground submenu
 */
static MENUOPTION foreGroundOptions[] =
{
	{"Black",			0, 0, 0},
	{"Blue", 			1, 0, 0},
	{"Green", 			2, 0, 0},
	{"Cyan", 			3, 0, 0},
	{"Red",				4, 0, 0},
	{"Magenta",			5, 0, 0},
	{"Brown",			6, 0, 0},
	{"Light Gray",		7, 0, 0},
	{"Dark Gray",		8, 0, 0},
	{"Light Blue", 		9, 0, 0},
	{"Light Green",		0, 0, 0},
	{"Light Cyan", 		11, 0, 0},
	{"Light Red",		12, 0, 0},
	{"Light Magenta",	13, 0, 0},
	{"Yellow",			14, 0, 0},
	{"White",			15, 0, 0},
};

static MENU foreGroundMenu =
{
	VERTICAL|AUTOLOCATE, 0, 0, 0, NORMVID, REVVID, BOLDVID, DOUBLESIDES,
	sizeof(foreGroundOptions)/sizeof(foreGroundOptions[0]), foreGroundOptions,
	0, 0, ColorSample
};

/*-----------------------------------------------------------------------------
 * background submenu, uses first 8 foreGroundMenu options
 */
 
static MENU backGroundMenu =
{
	VERTICAL|AUTOLOCATE, 0, 0, 0, NORMVID, REVVID, BOLDVID, DOUBLESIDES,
	8, foreGroundOptions, 0, 0, ColorSample
};

/*-----------------------------------------------------------------------------
 * attrib menu
 */
static MENUOPTION attribOptions[] =
{
	{"Foreground Color", 0, &foreGroundMenu, 0},
	{"Background Color", 0, &backGroundMenu, 0},
};

static MENU attribMenu =
{
	0, 0, 5, 41, NORMVID, REVVID, BOLDVID, DOUBLESIDES,
	sizeof(attribOptions)/sizeof(attribOptions[0]), attribOptions, 0, 0,
	ColorSample
};


/*-----------------------------------------------------------------------------
 */
static void ColorSample(void)
{
	SampleWindow(colorMenu.selection, SelectedAttrib(), cfg.attrib);
}

/*-----------------------------------------------------------------------------
 *
 */
int Colors(unsigned row, unsigned col)
{
	(void)row; (void)col;
	foreGroundMenu.selection = cfg.attrib[colorMenu.selection] & 0xf;
	backGroundMenu.selection = (cfg.attrib[colorMenu.selection] >> 4) & 0x7;
	Help("", "");
	CreateSample(18, 1);
	CreateMenu(&attribMenu);
	ProcessMenu(&attribMenu);
	DeleteMenu(&attribMenu);
	DeleteSample();
	cfg.attrib[colorMenu.selection] = SelectedAttrib();
	return(-1);
}

/*===========================================================================*/
