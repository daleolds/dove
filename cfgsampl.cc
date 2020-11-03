#include <string.h>
#include <pkeybrd.h>
#include <duiwin.h>
#include <duimenu.h>
#include <cfgcommo.h>

static Window *sampleWindow;
static Window *sampleMsgLine;

#define SWIDTH	30
#define SHEIGHT	5
#define MWIDTH	(SWIDTH + 2)
#define MHEIGHT	1

/*-----------------------------------------------------------------------------
 */
int CreateSample(unsigned row, unsigned col)
{
	if (row < 1)
		row = 1;
	else if (row > videoRows - (SHEIGHT + 1 + MHEIGHT))
		row = videoRows - (SHEIGHT + 1 + MHEIGHT);
	if (col < 1)
		col = 1;
	else if (col > videoCols - (SWIDTH + 1))
		col = videoCols - (SWIDTH + 1);
	sampleWindow = CreateWindow(row, col, SHEIGHT, SWIDTH, 0);
	sampleMsgLine = CreateWindow(row + SHEIGHT + 1, col - 1, MHEIGHT,
		MWIDTH, BORDERLESS);
	return (sampleWindow != 0 && sampleMsgLine != 0);
}

/*-----------------------------------------------------------------------------
 */
void DeleteSample(void)
{
	DeleteWindow(sampleWindow);
	DeleteWindow(sampleMsgLine);
}

/*-----------------------------------------------------------------------------
 */
void SampleWindow(unsigned selection, VBYTE attrib, VBYTE *cfgAttrib)
{
	VBYTE configuredAttrib = cfgAttrib[selection];
	cfgAttrib[selection] = attrib;

	switch (selection)
	{
		case DELETEBORDER:
		case DELETETEXT:
		case DELETEMARK:
			SelectWindow(sampleWindow);
			WriteBorder("Sample Deletions Window", DOUBLE, cfgAttrib[DELETEBORDER]);
			WSetAttribute(cfgAttrib[DELETETEXT]);
			ClearWindow();
			WSetCursor(1, 1);
			WriteString("sample \"kill stack\" text");
			WSetCursor(2, 1);
			WSetAttribute(cfgAttrib[DELETEMARK]);
			WriteString("selected text to undelete");
			goto ClearMsgLine;

		case HELPBORDER:
		case HELPTEXT:
			SelectWindow(sampleWindow);
			WriteBorder("Sample Help Window", DOUBLE, cfgAttrib[HELPBORDER]);
			WSetAttribute(cfgAttrib[HELPTEXT]);
			ClearWindow();
			WSetCursor(1, 1);
			WriteString("sample help text");
		ClearMsgLine:
			SelectWindow(sampleMsgLine);
			WSetAttribute(NORMVID);
			ClearWindow();
			break;

		default:
			SelectWindow(sampleWindow);
			WriteBorder("Sample View Window", DOUBLE, cfgAttrib[VIEWBORDER]);
			WSetAttribute(cfgAttrib[VIEWTEXT]);
			ClearWindow();
			WSetCursor(1, 1);
			WSetAttribute(cfgAttrib[VIEWTEXTCURSOR]);
			WriteString("n");
			WSetAttribute(cfgAttrib[VIEWTEXT]);
			WriteString("ormal text");
			WSetCursor(2, 1);
			WSetAttribute(cfgAttrib[VIEWMARKCURSOR]);
			WriteString("m");
			WSetAttribute(cfgAttrib[VIEWMARK]);
			WriteString("arked text");
			WSetAttribute(cfgAttrib[VIEWTEXT]);
			WSetCursor(3, 1);
			WriteString("Long line and overflow mark:");
			WSetAttribute(cfgAttrib[VIEWLONGLINE]);
			WriteString("\xAF");

			SelectWindow(sampleMsgLine);
			WSetCursor(0, 0);
			WSetAttribute(cfgAttrib[MSGLINECURSOR]);
			WriteString("M");
			WSetAttribute(cfgAttrib[MSGLINE]);
			WriteString("essage Line      ");
			WSetAttribute(cfgAttrib[LINECOL]);
			WriteString("Line Col");
			WSetAttribute(cfgAttrib[TIME]);
			WriteString("  Time");
			break;
	}
	cfgAttrib[selection] = configuredAttrib;
}

/*===========================================================================*/
