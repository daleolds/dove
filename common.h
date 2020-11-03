#include <string.h>
#include <assert.h>
#include <stdlib.h>

#include <pfileio.h>
#include <duiwin.h>
#include <cfgtable.h>

/*-----------------------------------------------------------------------------
* constants
*/
#define TIMELEN		11	/* length of time display field 		*/
#define LINECOLLEN	22	/* length of line, col display field 	*/
//#define MAXMSG		256	/* maximum length of message 			*/

/*-----------------------------------------------------------------------------
* structure definitions
*/

/* All text is kept in circularly linked lists of "Line" structures. These
 * begin at the header line (which is the last line of the
 * buffer). This line is pointed to by the "Buffer". Each line contains a
 * the number of bytes in the line (the "used" size), the size of the text
 * array, and the text. 
 */
class Line
{
	Line *fp, *bp; // Links to the forward/back lines
	size_t max, len; // Allocated and actually used sizes
	char dat[1];

	// can't construct, copy or assign these things
	Line(const Line& other);
	Line &operator=(const Line& other);

public:
	Line() { assert(false); } // only construct with alloc

	Line *forw() { return fp; }
	Line *back() { return bp; }
	void link(Line *newNext);
	void unlink();

	static Line *alloc(size_t sz = 0, const char *data = 0);
	static void free(Line *lp) { ::free(lp); }

	char getc(size_t i) { assert(i < len); return dat[i]; }
	void putc(size_t i, char c) { assert(i < len); dat[i] = c; }
	const char *data(size_t i = 0) { return &dat[i]; }
	size_t length() { return len; }
	size_t sizeofEOL();
	size_t offsetofEOL() { return len - sizeofEOL(); }
	size_t unused() { assert(max >= len); return max - len; }
	size_t maxLen() { return max; }
	void truncate(size_t i) { assert(i <= len); len = i; }
	void insert(size_t offset, size_t size, const char *data);
	void remove(size_t offset, size_t size);
};

/* A buffer position consists of a pointer to a line within the buffer and
 * an offset within that line. An array of common positions is kept in both
 * the View header and the Buffer header. Both arrays must be identical
 * in structure because they are occasionally copied over each other.
 */
struct Position
{
	Line *line;
	size_t offset; // offset within the above line

	void operator++()
	{
		if (offset == line->offsetofEOL()) { line = line->forw(); offset = 0; }
		else ++offset;
	}
	
	void operator--()
	{
		if (offset == 0) { line = line->back(); offset = line->offsetofEOL(); }
		else --offset;
	}
};

// Position flags
#define PF_TEMP_MARK 0x001

/* both BufferS and Views contain an array of common positions. This is the
 * definition of what those positions mean, and the number of them.
 */
enum POSITIONS {DOT, MARK, SAVE, USER, POSITIONCOUNT};

/* There is a view structure allocated for every active display view. The
 * views are kept in a big list, in top to bottom screen order, with the
 * listhead at "vheadp". Each view contains its own common position array
 * with the same meaning as the position array in the buffer header. Each
 * view also has its own display offset for horizontal scrolling.
 */
struct View
{
	View *next;
	struct Buffer *bufp; // Buffer displayed in view
	Window *winp; // window attached to this view
	Position position[POSITIONCOUNT]; // current common positions
	unsigned positionFlags, leftcol; // left most column to display
	unsigned char dotrow; // row in view for dot
};

/* Text is kept in buffers. A buffer header, described
 * below, exists for every buffer in the system. The buffers are
 * kept in a big list, so that commands that search for a buffer by
 * name can find the buffer header. There is a safe store for the
 * common positions in the header, but this is only valid if the buffer
 * is not being displayed (that is, if "viewCount" is 0). The text for
 * the buffer is kept in a circularly linked list of lines, with
 * a pointer to the header line in "linep".
 */

enum SEQCMDS {SEQ_NONE, SEQ_CURSOR_COL, SEQ_DELETE, SEQ_OVERWRITE, SEQ_COPY,
		SEQ_BACKDEL, SEQ_HSCROLL, SEQ_SMASK, SEQ_RMASK};

// buffer flags
#define B_CHANGED	0x01
#define B_READONLY	0x02
#define B_CR_LF		0x04

struct Buffer
{
	Buffer *bufp; // Link to next Buffer
	unsigned char thisSeqCmd, lastSeqCmd, flags, viewCount; // Count of views on buffer
	Line *linep; // Link to the header Line
	View viewState; // safe place for dot, mark, etc, when buffer is not in view
	char fname[FIO_MAX_PATH];
	int fmode;
	time_t fModTime;
};

/* The starting position of a region and the size of the region in
 * characters is kept in a region structure. Used by the region commands.
 */
struct Region
{
	Position start;
	fsize_t size; // Length in characters
	fsize_t newlines; // count of newlines within

	void get(char *buffer);
};

/*---------------------------------------------------------------------------
 * binary objects -- somewhat like the std::string, but dove does not
 * use std::stuff yet.
 */
class Bob
{
	char *dat;
	size_t len, max;

	// disable copy and assignment since we don't use exceptions
	Bob(const Bob &other);
	Bob &operator=(const Bob &other);

public:
	Bob(): dat(0), len(0), max(0) {}
	~Bob() { delete [] dat; }

	bool copy(size_t size, const char *data)
	{
		char *p = new char[size + 1];
		if (p) { delete [] dat; memcpy(dat = p, data, len = max = size); p[max] = 0; }
		return p != 0;
	}

	void take(Bob &other)
	{
		delete [] dat; dat = other.dat; len = other.len; max = other.max;
		other.dat = 0; other.len = other.max = 0;
	}

	void clear() { len = max = 0; delete [] dat; dat = 0; }
	bool copy(const Bob &other) { return copy(other.len, other.dat); }
	bool copy(const char *s) { return copy(strlen(s), s); }
	const char &operator[](unsigned i)
	{
		static char empty;
		assert(i == 0 || dat && i < len);
		return len == 0? empty: dat[i];
	}
	size_t length() { return len; }
	const char *data() { return dat; }
	char *insert(size_t offset, size_t size);
	bool insert(size_t offset, size_t size, const char *data)
	{
		char *p = insert(offset, size);
		if (p) memcpy(p, data, size);
		return p != 0;
	}
	bool append(const char *s) { return insert(len, strlen(s), s); }
	bool append(size_t size, const char *data) { return insert(len, size, data); }
	bool append(char c) { return insert(len, 1, &c); }
	void remove(size_t offset, size_t size)
	{
		if (size)
		{
			assert(offset < len && len <= max && size <= max && offset + size <= max);
			memmove(dat + offset, dat + offset + size, 1 + len - offset - size);
			len -= size;
		}
	}
	const char *cstr() { return dat; }
};

/*-----------------------------------------------------------------------------
* prototypes and globals
*/

/* buffer */
int NextBufferKey(int n);
int PrevBufferKey(int n);
int CreateBufferKey(void);
int CreateBufferFromPathKey(void);
int RenameBufferKey(void);
int DeleteBufferKey(void);
void DelBuffer(Buffer *bp);
void SetBuffer(Buffer *bp);
bool SetBufferChanged();

/* cursor */
int GoToBOLKey(void);
int BackCharKey(int n);
int GoToEOLKey(void);
int ForwCharKey(int n);
int GoToBOBKey(void);
int GoToEOBKey(void);
int ForwLineKey(int n);
int BackLineKey(int n);
int ForwPageKey(int n);
int BackPageKey(int n);
int BackWordKey(int n);
int ForwWordKey(int n);
int GoToBOVKey(void);
int GoToEOVKey(void);
int ScrollDownKey(int n);
int ScrollUpKey(int n);
int ScrollLeftKey(int n);
int ScrollRightKey(int n);
int MarkToBOLKey(void);
int MarkBackCharKey(int n);
int MarkToEOLKey(void);
int MarkForwCharKey(int n);
int MarkToBOBKey(void);
int MarkToEOBKey(void);
int MarkToBOVKey(void);
int MarkToEOVKey(void);
int MarkForwLineKey(int n);
int MarkBackLineKey(int n);
int MarkForwPageKey(int n);
int MarkBackPageKey(int n);
int MarkForwWordKey(int n);
int MarkBackWordKey(int n);
int MarkScrollDownKey(int n);
int MarkScrollUpKey(int n);

/* display */
extern VBYTE attrib[ATTRIBCOUNT];
extern Window *screen, *messageWindow, *timeWindow;
void DisplayViews(void);
int InitScreen(void);
void ExitScreen(void);
void ShowCursor(unsigned row, unsigned col, VBYTE attr);
unsigned CursorCol(void);
unsigned DisplayCol(Line *lp, unsigned lo);
unsigned DisplayOffset(Line *lp, unsigned goalCol);

/* file */
int FileReadKey(void);
int FileWriteKey(void);
int FileSave(void);
int WildEdit(const char *fspec, bool create);
int WildPathEdit(const char *fspec);

/* keybrd */
extern unsigned short *macroOut, *macroIn;
int ReadMacrosKey(void);
int SaveMacrosKey(void);
int RecordMacroKey(void);
int BlockMacroKey(int n);
int ExecuteMacro(unsigned int key, unsigned int repeat);
int CancelKey(void);
BINDING BindKey(unsigned short key);
int CommandAvailable(void);
unsigned int GetCommand(void);

/* kill */
//void BufferText(Line *line, unsigned offset, unsigned size, char *data);
int SelectString(const char *title, int stringCount, Bob *strings);
int SelectedKill(const char *&kill, size_t &size, bool topKill = false);
int CopyKey(int n);
int DeleteKey(int n);
int DeleteBackKey(int n);
int RetypeKey(int n);
int StackOfKillsKey(int n);
int UndeleteKey(int n);
int InsertOverwriteChar(int ch);

/* line */
int DeleteChars(fsize_t n);
int InsertChars(size_t len, const char *block);

/* main */
extern View *curvp, *vheadp;
extern Buffer *curbp, *bheadp;
int QuitKey(void);
int ShellKey(void);
int CommandKey(void);

/* mark */
int ToggleMarkKey(int n);
int SwapMarkKey(void);
int GetRegion(Region *reg, unsigned n);

/* menu */
int MenuKey(int n);

/* misc */
int SavePositionKey(void);
int SetPositionKey(void);
int MemoryLeftKey(void);
int ShowPositionKey(void);
int TwiddleKey(void);
int LiteralCharKey(int n);
int UpperRegionKey(int n);
int LowerRegionKey(int n);
int InsertTimeKey(int n);
int GetLiteral(void);

/* msgline */
extern bool msgLineFull;
void MLRowCol(void);
void MLTime(void);
int MLYesNo(const char *prompt);
int MLKey(const char *prompt0, const char *prompt1, const char *prompt2);
int MLReply(const char *prompt, Bob &reply, bool tabFileNameCompletion = false);
void MLWrite(const char *fmt, ...);
void MLErase(void);
void DisplayMode(const char *s, int mode);
int MLNoMem();

/* popup */
char *KeyName(int key, int braceFlag);
int HelpKey(void);

/* search */
int ReplaceKey(int n);
int QueryReplaceKey(int n);
int ForwSearchKey(int n); 
int GlobalReplaceKey(int n);
int GlobalQueryReplaceKey(int n);
int GlobalSearchKey(int n);
int BackSearchKey(int n);
int ToggleCaseSearchKey(int n);
int FindMatchKey(void);
int SetSearchMaskKey(int n);
int SetReplaceMaskKey(int n);

/* type */
int ChangeMode(int n, int modeBit);
int InsertModeKey(int n);
int TabModeKey(int n);
int TabKey(int n);
int EnterKey(int n);
int WordWrapModeKey(int n);
int IndentModeKey(int n);
int CtrlCharsModeKey(int n);
int CRLFModeKey(int n);
int TypeChars(size_t len, const char *block);

/* view */
int NextViewKey(int n);
int PrevViewKey(int n);
int DeleteViewKey(void);
int CreateViewKey(void);
int EnlargeViewKey(int n);
int ShrinkViewKey(int n);

/*==========================================================================*/
