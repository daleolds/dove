#include <stdio.h>
#include <limits.h>

#include <pkeybrd.h>
#include <duiwin.h>
#include <common.h>

#define MACROCHUNK	8

unsigned short *macroOut, *macroIn;		/* Input, output for macro	*/

static unsigned short *macroBuffer;		/* Macro buffer				*/
static unsigned macroRept = 0;			/* Repeat count for macro	*/
static unsigned macroSize = 0;			/* entries in macro block	*/
static int macroIndex;

/*--------------------------------------------------------------------------
 */
int ReadMacrosKey(void)
{
	if (macroIn)
	{
		macroIn--;				/* erase keystroke that got us here */
		MLWrite("Cannot read macros while recording");
		return 0;
	}
	if (!ReadConfig())
	{
		MLWrite("Error reading configuration file");
		return 0;
	}
	return 1;
}

/*--------------------------------------------------------------------------
 */
int SaveMacrosKey(void)
{
	if (macroIn != 0)
	{
		macroIn--;				/* erase keystroke that got us here */
		MLWrite("Cannot save macros while recording");
		return 0;
	}
	if (!WriteConfig())
	{
		MLWrite("Error writing configuration file");
		return 0;
	}
	return 1;
}

/*--------------------------------------------------------------------------
 * Cancel a command.
 * Kill off keyboard macro in progress.
 */
int CancelKey(void)
{
	if (macroIn != 0)
	{
		free(macroBuffer);
		macroBuffer = macroIn = 0;
		MLWrite("Macro recording cancelled");
	}
	return 0;
}

/*--------------------------------------------------------------------------
 * Begin or End a keyboard macro.
 * Set up variables and return.
 */
int RecordMacroKey(void)
{
	BINDING cmd;
	if (macroIn == 0)
	{
		MLWrite("Press macro key on which to record");
		cmd = BindKey(GetCommand());
		if (cmd.type != MACROKEY)
		{
			MLWrite("Not a macro key");
			return 0;
		}
		macroIndex = cmd.data;
		if (macroTable[macroIndex] != 0
				&& !MLYesNo("Overwrite existing macro"))
			return 0;
		if ((macroBuffer = (unsigned short *)malloc(MACROCHUNK * sizeof(*macroBuffer))) == 0)
		{
			MLWrite("Not enough memory to record macro");
			return 0;
		}
		macroSize = MACROCHUNK;
		MLWrite("Start macro");
		macroIn = macroBuffer;
	}
	else
	{
		if (macroTable[macroIndex] != 0)
			free(macroTable[macroIndex]);
		if (macroIn == macroBuffer + 1)
		{
			free(macroBuffer);
			macroTable[macroIndex] = macroBuffer = 0;
		}
		else
		{
			*(macroIn - 1) = NONKEY; /* replace keystroke that got us here */
			macroTable[macroIndex] = macroBuffer;
		}
		MLWrite("End macro");
		macroIn = 0;
	}
	return 1;
}

/*--------------------------------------------------------------------------
 */
static unsigned MacroBlockLength(Line *line, unsigned offset,
		unsigned size, unsigned short *buffer)
{
	unsigned length = 0, rept = 0, lastChar = NONKEY, thisChar, eollen;
	while (size)
	{
		if (!(eollen = line->sizeofEOL()))
			return 0;
		if (offset >= line->length() - eollen)
		{
			line = line->forw();
			offset = 0;
			thisChar = ENTER;
			size -= eollen;
		}
		else
		{
			thisChar = line->getc(offset) & 0xff;
			offset++;
			size--;
		}
		if (lastChar == thisChar && rept < 32 * 1024 - 2)
			rept++;
		else
		{
			if (rept > 0)
			{
				if (rept > 1)
				{
					if (buffer != 0)
						*buffer++ = rept | REPT;
					length++;
				}
				if (buffer != 0)
					*buffer++ = lastChar;
				length++;
			}
			rept = 1;
			lastChar = thisChar;
		}
	}
	if (rept > 1)
	{
		if (buffer != 0)
			*buffer++ = rept | REPT;
		length++;
	}
	if (buffer != 0)
		*buffer++ = lastChar;
	length++;
	if (buffer != 0)
		*buffer = NONKEY;
	return length + 1;
}

/*--------------------------------------------------------------------------
 * Attach a marked block to a macro key.
 */
int BlockMacroKey(int n)
{
	BINDING cmd;
	Region region;
	unsigned i, sz;

	GetRegion(&region, n);
	if (region.size == 0)
	{
		MLWrite("No block to attach to macro");
		return 0;
	}
	if (region.size > UINT_MAX - 2)
	{
		MLWrite("Marked area too large to convert to macro");
		return 0;
	}
	MLWrite("Press macro key on which to attach marked block");
	cmd = BindKey(GetCommand());
	if (cmd.type != MACROKEY)
	{
		MLWrite("Not a macro key");
		return 0;
	}
	i = cmd.data;
	if (macroTable[i] != 0 && !MLYesNo("Overwrite existing macro"))
		return 0;

	if (!(sz = MacroBlockLength(region.start.line, region.start.offset,
			(unsigned)region.size, 0)))
	{
		MLWrite("Internal block calculation error");
		return 0;
	}

	if (!(macroTable[i] = (unsigned short *)malloc(sz * sizeof *macroTable[i])))
	{
		MLWrite("Not enough memory to convert block to macro");
		return 0;
	}
	MacroBlockLength(region.start.line, region.start.offset,
			(unsigned int)region.size, macroTable[i]);
	curvp->position[MARK].line = 0;
	MLErase();
	return 1;
}

/*--------------------------------------------------------------------------
 * Execute a macro. Set up variables and return.
 */
int ExecuteMacro(unsigned key, unsigned repeat)
{
	if (macroIn != 0)
	{
		macroIn--;				/* erase keystroke that got us here */
		MLWrite("Cannot Execute Macro while recording");
	}
	else if (macroTable[key] == 0)
		MLWrite("No macro is recorded for this key");
	else if (repeat != 0)
	{
		macroOut = macroTable[key];
		macroRept = repeat - 1;
		return 1;
	}
	return 0;
}

/*--------------------------------------------------------------------------
 * This is the general command translation routine, maps a key to routine. 
 */
BINDING BindKey(unsigned short key)
{
	BINDING cmd = {NOTBOUND, 0, key};
	if (key & REPT)
		return cmd;
	if (IsDataKey(key))
	{
		cmd.type = INSCHAR;
		cmd.data = key;
		return cmd;
	}
	int i = FindBinding(key, false);
	return i == -1? cmd: bindingTable[i];
}

/*-------------------------------------------------------------------
 */
int CommandAvailable(void)
{
	return macroOut || KeyHit(0);
}

/*-------------------------------------------------------------------
 *
 */
unsigned GetCommand(void)
{
	unsigned c, n, k;
	unsigned short *tmpBuffer;
	while (macroOut == 0 && !KeyHit(250))
		MLTime();				/* update clock display	while waiting */
	if (macroOut != 0)
	{
		c = *macroOut++;
		if (*macroOut == NONKEY)
			macroOut = (macroRept-- == 0? 0: macroBuffer);
	}
	else
	{
		c = GetKey();
		if (c >= (ALT|'0') && c <= (ALT|'9'))	/* build up repeat counts */
		{
			n = 0;
			do
			{
				c = (c & ~ALT) - '0';
				n = ((long)n*10+c >= (long)REPT? (int)(REPT-1): (n*10+c));
				MLWrite("Repeat: %u", n);
				c = GetKey();
			} while (c >= (ALT|'0') && c <= (ALT|'9'));
			UngetKey(c);
			c = n | REPT;
		}
		else if (KeyHit(0))
		{
			for (n = 1, k = NONKEY; KeyHit(0) && (k = GetKey()) == c; n++)
				;
			if (k != NONKEY)
				UngetKey(k);
			if (n > 1)
			{
				UngetKey(c);
				c = n | REPT;
			}
		}
		if (macroIn != 0 && c != NONKEY)		/* Save macro strokes.	*/
		{
			if (macroIn < macroBuffer + macroSize - 1)
				*macroIn++ = c;
			else if ((tmpBuffer = (unsigned short *)malloc(
			  (macroSize + MACROCHUNK) * sizeof(*macroBuffer))) == 0)
			{
				MLWrite("No more near memory, macro cancelled");
				free(macroBuffer);
				macroBuffer = macroIn = 0;
			}
			else
			{
				memcpy(tmpBuffer, macroBuffer, macroSize*sizeof(*macroBuffer));
				macroSize += MACROCHUNK;
				macroIn = tmpBuffer + (unsigned)(macroIn - macroBuffer);
				free(macroBuffer);
				macroBuffer = tmpBuffer;
				*macroIn++ = c;
			}
		}
	}
	return c;
}

/*===========================================================================*/
