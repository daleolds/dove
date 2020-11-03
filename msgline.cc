#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <pkeybrd.h>
#include <pvideo.h>
#include <duiwin.h>
#include <common.h>

bool msgLineFull = false;

//---------------------------------------------------------------------------
int MLNoMem() { MLWrite("no memory"); return 0; }

//---------------------------------------------------------------------------
void MLErase() { MLWrite(""); msgLineFull = false; }

//---------------------------------------------------------------------------
void DisplayMode(const char *s, int mode)
	{ MLWrite("%s mode is %s", s, mode? "on": "off"); }

/*-------------------------------------------------------------------
 * Ask a yes or no question in the message line. Return either 1, 0.
 * The 0 status is also returned if the user bumps out of the question.
 * Used any time a confirmation is required.
 */
int MLYesNo(const char *prompt)
	{ return MLKey(prompt, "? [Yes, No] ", "") == 'Y'; }

/*-------------------------------------------------------------------
 * Ask a question that requires a single key response on the message
 * line. Return the key, guaranteed upper case if it is an alpha
 * character, 0 on non-data char, -1 on Cancel. The three prompts are
 * simply concatenated by this routine so the caller doesn't have to.
 */
int MLKey(const char *prompt0, const char *prompt1, const char *prompt2)
{
	BINDING cmd;
	unsigned msgLen = strlen(prompt0) + strlen(prompt1) + strlen(prompt2) + 1;
	unsigned msgOffset = msgLen > messageWindow->wide?
			msgLen - messageWindow->wide: 0;
	char *msg = new char[msgLen];
	sprintf(msg, "%s%s%s", prompt0, prompt1, prompt2);
	MLWrite("%s", msg + msgOffset);
	delete msg;
	ShowCursor(messageWindow->row, messageWindow->col + (msgLen - msgOffset - 1),
			attrib[MSGLINECURSOR]);
	cmd = BindKey(GetCommand());
	return cmd.type == INSCHAR? toupper(cmd.data):
		cmd.type == FUNCTIONKEY && functionTable[cmd.data].nonr == CancelKey?
		-1: 0;
}

/*---------------------------------------------------------------------------
 * inserts what it can into buffer to complete file name, only fails if no mem
 */
bool FileNameCompletion(size_t &cur, Bob &bob)
{
	static const unsigned maxCompletedNames = 100;
	Bob pattern, *names = new Bob[maxCompletedNames];
	unsigned count = 0;
	int selStr = -2;
	FileFinder ff;
	if (!names || !pattern.copy(cur, bob.data()) || !pattern.append('*'))
		goto error;

	if (ff.first(pattern.cstr(), true))
	{
		do
			if (!names[count++].copy(ff.fname()))
				goto error;
		while (count < maxCompletedNames && ff.next());

		if (count == 1)
			selStr = 0;
		else if (count > 1)
			selStr = SelectString(count == maxCompletedNames?
					"select name from partial list": "select name from list",
					count, names);

		if (selStr >= 0)
		{
			assert(names[selStr].length() >= cur);
			unsigned len = names[selStr].length() - cur;
			if (!bob.insert(cur, len, names[selStr].data() + cur))
				goto error;
			cur += len;
		}
		else if (selStr != -2)
			goto error;
	}
	delete [] names;
	return true;

error:
	delete [] names;
	return true;
}


/*-------------------------------------------------------------------
 * Write a prompt into the message line, then get a response.  Handle
 * horizontal scrolling if response is long. Don't destroy original data
 * if user aborts.
 * Returns -1 for ESCed out, 0 for nothing entered, 1 for everything else.
 */
int MLReply(const char *prompt, Bob &inBuf, bool tabFileNameCompletion)
{
	static const size_t minReplySize = 4, minPromptSize = 5;
	if (messageWindow->wide < minReplySize + minPromptSize)
		return 0;

	if (strlen(prompt) > messageWindow->wide - minReplySize)
		prompt += strlen(prompt) - (messageWindow->wide - minReplySize);
	const size_t dataCol = strlen(prompt);
	const size_t maxDisplayLen = messageWindow->wide - dataCol;
	BINDING cmd;
	char key;
	size_t displayOffset = 0, cur = inBuf.length();
	Bob buf;
	if (!buf.copy(inBuf))
		return MLNoMem();

	for (bool firstHit = msgLineFull = true;; firstHit = false)
	{
		SelectWindow(messageWindow);
		WSetCursor(0, 0);
		WriteString(prompt);

		if (cur >= displayOffset + maxDisplayLen)
			displayOffset = cur - maxDisplayLen + 1;
		else if (cur == 0)
			displayOffset = 0;
		else if (cur <= displayOffset)
			displayOffset = cur - 1;

		WSetCursor(0, dataCol);
		WriteData(buf.length() - displayOffset, &buf[displayOffset]);
		ClearEOL();
		if (buf.length() > maxDisplayLen + displayOffset)
		{
			WSetCursor(0, maxDisplayLen + dataCol - 1);
			WriteString("\xAF");
		}
		if (displayOffset > 0)
		{
			WSetCursor(0, dataCol);
			WriteString("\xAE");
		}
		ShowCursor(messageWindow->row, messageWindow->col + cur
				- displayOffset + dataCol, attrib[MSGLINECURSOR]);

		cmd = BindKey(GetCommand());
		if (cmd.type == FUNCTIONKEY)
		{
			if (functionTable[cmd.data].rept == EnterKey)
			{
				inBuf.take(buf);
				return inBuf.length()? 1: 0;
			}
			if (functionTable[cmd.data].nonr == CancelKey)
			{
				MLWrite("cancelled");
				return -1;
			}

			if (functionTable[cmd.data].rept == BackCharKey)
			{
				if (cur > 0)
					cur--;
			}
			else if (functionTable[cmd.data].rept == ForwCharKey)
			{
				if (cur < buf.length())
					cur++;
			}
			else if (functionTable[cmd.data].rept == DeleteKey)
			{
				if (cur < buf.length())
					buf.remove(cur, 1);
			}
			else if (functionTable[cmd.data].rept == InsertModeKey)
				InsertModeKey(1);
			else if (functionTable[cmd.data].rept == DeleteBackKey)
			{
				if (cur)
					buf.remove(--cur, 1);
			}
			else if (functionTable[cmd.data].rept == LiteralCharKey)
			{
				int k;
				if ((k = GetLiteral()) != -1)
				{
					key = k;
					goto DataKey;
				}
			}
			else if (functionTable[cmd.data].rept == TabKey)
			{
				if (tabFileNameCompletion)
				{
					if (firstHit)
					{
						displayOffset = cur = 0;
						buf.clear();
					}
					if (!FileNameCompletion(cur, buf))
						return MLNoMem();
				}
				else
				{
					key = '\t';
					goto DataKey;
				}
			}
			else if (functionTable[cmd.data].nonr == GoToBOLKey)
				cur = 0;
			else if (functionTable[cmd.data].nonr == GoToEOLKey)
				cur = buf.length();
			else if (functionTable[cmd.data].rept == StackOfKillsKey
					|| functionTable[cmd.data].rept == UndeleteKey)
			{
				const char *killData;
				size_t killSize;
				if (SelectedKill(killData, killSize,
								functionTable[cmd.data].rept == UndeleteKey)
						&& killData != 0)
				{
					if (firstHit)
					{
						displayOffset = cur = 0;
						buf.clear();
					}
					if (!buf.insert(cur, killSize, killData))
						return MLNoMem();
				}
			}
		}
		else if (cmd.type == INSCHAR)
		{
			key = cmd.data;
DataKey:
			if (firstHit)
			{
				displayOffset = cur = 0;
				buf.clear();
			}
			if (!(modes & MODE_INSERT))
				buf.remove(cur, 1);
			if (!buf.insert(cur++, 1, &key))
				return MLNoMem();
		}
	}
}

/*-------------------------------------------------------------------
 * Write a message into the message line. Uses printf formats.
 * Set the msgLineFull flag.
 */
void MLWrite(const char *fmt, ...)
{
	// todo: just testing msgBuf at 2, should be 256 or so
	char msgBuf[2048], *msg = 0;
	va_list argptr;
	va_start(argptr, fmt);
	size_t len = vsnprintf(msgBuf, sizeof msgBuf, fmt, argptr);
	msgBuf[sizeof msgBuf - 1] = 0;
	if (len > sizeof msgBuf && (msg = new char[++len]))
		vsnprintf(msg, len, fmt, argptr);
	va_end(argptr);
	SelectWindow(messageWindow);
	WSetCursor(0, 0);
	WriteLine(msg? msg: msgBuf);
	msgLineFull = true;
	delete [] msg;
}


/*-------------------------------------------------------------------
 * Update Row and Col display on message line.
 */
void MLRowCol(void)
{
	if (msgLineFull)
		return;
	char lineStr[LINECOLLEN + 1];
	unsigned ln = 1;
	Line *dotp = curvp->position[DOT].line;
	for (Line *lp = curbp->linep->forw(); lp != dotp; lp = lp->forw())
		++ln;
	snprintf(lineStr, sizeof lineStr, " Line: %-5uCol: %-5u", ln, CursorCol() + 1);
	SelectWindow(messageWindow);
	WSetAttribute(attrib[LINECOL]);
	WSetCursor(0, messageWindow->wide - LINECOLLEN);
	WriteLine(lineStr);
	WSetAttribute(attrib[MSGLINE]);
}

/*-------------------------------------------------------------------
 * Update time display on message line.
 */
void MLTime(void)
{
	static char lastTime[TIMELEN + 1];
	int hr;
	char timeStr[TIMELEN + 1];
	time_t tmpnow;
	struct tm *now;
	time(&tmpnow);
	now = localtime(&tmpnow);
	hr = ((hr = (now->tm_hour % 12)) == 0? 12: hr);
	snprintf(timeStr, sizeof timeStr, "%c%c %2d%c%02d%c",
			(modes & MODE_INSERT)? ' ': 'O', (macroIn != 0? 'M': ' '),
			hr, /* ((now->tm_sec & 1)? ':': ' ') */ ':', now->tm_min,
			(now->tm_hour >= 12? 'p': 'a'));
	if (strcmp(lastTime, timeStr) != 0)
	{
		SelectWindow(timeWindow);
		WSetCursor(0, 0);
		WriteLine(timeStr);
		strcpy(lastTime, timeStr);
	}
}

/*===========================================================================*/
