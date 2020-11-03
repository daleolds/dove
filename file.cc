#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <pfileio.h>
#include <duiwin.h>
#include <common.h>

/*--------------------------------------------------------------------------
 * Find a buffer, by name. Return a pointer to the Buffer structure
 * associated with it.  If the buffer is not found allocate and initialize
 * a new buffer header and insert it into the buffer list. The buffer list
 * is kept in alphabetical order. oldBufferExists is set to true if an old
 * buffer is found.
 */
static Buffer *InsertBuffer(const char *fname, bool &oldBufferExists)
{
	int namecmp;
	Line *lp;
	const char *justName = fname + PathLength(fname);
	Buffer *newbp, *prevbp, *bp;
	for (bp = bheadp, prevbp = 0; bp &&
			(namecmp = strcmp(justName, bp->fname + PathLength(bp->fname))) >= 0;
			prevbp = bp, bp = bp->bufp)
	{
		if (namecmp == 0)
		{
			if ((namecmp = strcmp(fname, bp->fname)) < 0)
				break;
			if (namecmp == 0)
			{
				oldBufferExists = true;
				return bp;
			}
		}
	}
	oldBufferExists = false;
	if (!(newbp = (Buffer *)malloc(sizeof(*newbp))))
		return 0;
	if (!(lp = Line::alloc()))
	{
		free(newbp);
		return 0;
	}
	memset(newbp, 0, sizeof(*newbp));
	newbp->bufp = bp;
	if (bp == bheadp)
		bheadp = newbp;
	else
		prevbp->bufp = newbp;
	newbp->viewState.position[DOT].line = newbp->linep = lp;
	strcpy(newbp->fname, fname);
	return newbp;
}

/*-----------------------------------------------------------------------------
 */
#define RESERVED_HEAP 256
static char *GetBuffer(unsigned *size, bool *local)
{
	static char safeBuffer[512] = { 0 };
	char *heapBuffer;
	unsigned sizeHeapBuffer = 16 * 1024;

	if (!(heapBuffer = (char *)malloc(sizeHeapBuffer)))
	{
		*size = sizeof(safeBuffer);
		*local = true;
		return safeBuffer;
	}
	*size = sizeHeapBuffer - RESERVED_HEAP;
	*local = false;
	return heapBuffer;
}

/*--------------------------------------------------------------------------
 * Read file "fname" into the current buffer. Called by both the read and
 * edit commands. Returns 0 for file not found, -1 for error,
 * 1 for everything else.
 */
static int ReadIn(const char *fname, int *fmode = 0, time_t *fModTime = 0)
{
	int fileHandle, rCode = 1;
	bool localBuffer;
	unsigned long totalCharsRead = 0L;
	unsigned charsRead, sizeBuffer;
	unsigned char dotrow;
	char *buffer = GetBuffer(&sizeBuffer, &localBuffer);
	if ((fileHandle = FileOpen(fname, fmode, fModTime)) == FIO_INVALID_HANDLE)
	{
		if (errno == ENOENT)
		{
			MLWrite("File \"%s\" not found", fname);
			rCode = 0;
		}
		else
		{
			MLWrite("Could not open file \"%s\": %s", fname, strerror(errno));
			rCode = -1;
		}
	}
	else
	{
		curvp->position[SAVE] = curvp->position[DOT];
		--curvp->position[SAVE];
		dotrow = curvp->dotrow;
		do
		{
			charsRead = FileRead(fileHandle, buffer, sizeBuffer);
			if (charsRead < 0 || charsRead > sizeBuffer)
			{
				rCode = -1;
				MLWrite("Could not read file \"%s\": %s", fname, strerror(errno));
				break;
			}
			if ((rCode = InsertChars(charsRead, buffer)) == 0)
			{
				rCode = -1;
				MLWrite("Not enough memory");
				break;
			}
			totalCharsRead += charsRead;
			MLWrite("%lu characters read", totalCharsRead);
		} while (charsRead);
		FileClose(fileHandle);
		curvp->position[DOT] = curvp->position[SAVE];
		++curvp->position[DOT];
		curvp->position[SAVE].line = 0;
		curvp->dotrow = dotrow;
	}
	if (!localBuffer)
		free(buffer);
	return rCode;
}

/*--------------------------------------------------------------------------
 * This function performs the details of file writing. The number of bytes
 * written is displayed. Most of the grief is error checking of some sort.
 */
static int WriteOut(const char *fname, Position *start, Position *end,
    int fmode = 0)
{
	Position p;
	int fileHandle, rCode = 1;
	bool localBuffer;
	unsigned long totalCharsWritten = 0L;
	unsigned charsWritten, i, sizeBuffer;
	char *buffer = GetBuffer(&sizeBuffer, &localBuffer);
	fileHandle = FileCreate(fname, fmode);
	if (fileHandle == FIO_INVALID_HANDLE)
	{
		MLWrite("Could not open file \"%s\"", fname);
		rCode = 0;
	}
	else
	{
		i = 0;
		p = *start;
		while (p.line != end->line || p.offset != end->offset)
		{
			if (p.offset == p.line->length())
			{
				p.line = p.line->forw();
				p.offset = 0;
			}
			else
				buffer[i++] = p.line->getc(p.offset++);
			if (i >= sizeBuffer - 1)
			{
				charsWritten = FileWrite(fileHandle, buffer, i);
				totalCharsWritten += charsWritten;
				if (charsWritten != i)
				{
					rCode = 0;
					break;
				}
				MLWrite("%lu characters written", totalCharsWritten);
				i = 0;
			}
		}
		if (rCode == 1)
		{
			if (i > 0)
			{
				charsWritten = FileWrite(fileHandle, buffer, i);
				totalCharsWritten += charsWritten;
				if (charsWritten != i)
					rCode = 0;
			}
			MLWrite("%lu characters written", totalCharsWritten);
		}
		FileClose(fileHandle);
		if (rCode == 0)
		{
			FileRemove(fname);
			MLWrite("Error writing to file \"%s\", %s", fname, strerror(errno));
		}
	}
	if (!localBuffer)
		free(buffer);
	return rCode;
}


/*--------------------------------------------------------------------------
 * Create a buffer for a filename that does not contain wild cards.
 * Returns number of buffers created or already existing (1 or 0).
 */
static int FileEdit(const char *fname)
{
	Buffer *bp, *oldCurbp;
	bool oldBufferExists;
	int rCode;
	if (!(bp = InsertBuffer(fname, oldBufferExists)))
	{
		MLWrite("Not enough memory to create another buffer");
		return 0;
	}
	if (oldBufferExists)
	{
		MLWrite("existing buffer");
		SetBuffer(bp);
		return 1;
	}
	oldCurbp = curbp;
	SetBuffer(bp);
	if ((rCode = ReadIn(fname, &bp->fmode, &bp->fModTime)) == -1)
	{
		MLWrite("error: %s", strerror(errno));
		SetBuffer(oldCurbp);
		DelBuffer(bp);
		return 0;
	}
	bp->flags = 0;
	if (rCode == 0)
		MLWrite("New file");
	else if (IsWriteProtected(fname))
		bp->flags |= B_READONLY;

	// try to guess whether to use CR_LF or just NL
	unsigned char oldmodes = modes;
	modes &= ~MODE_SHOW_CTRL_CHARS;
	unsigned crlfcount = 0, nlcount = 0, i = 0;
	for (Line *lp = curbp->linep->forw(); lp != curbp->linep && i < 100; ++i, lp = lp->forw())
		if (lp->sizeofEOL() == 1)
			nlcount++;
		else
			crlfcount++;
	modes = oldmodes;
	if (nlcount == crlfcount && (modes & MODE_CR_LF) || nlcount < crlfcount)
		bp->flags |= B_CR_LF;

	return 1;
}


/*--------------------------------------------------------------------------
 * Read a file into the current buffer. Find the name of the file, and call
 * the standard code to read in the file.
 */
int FileReadKey(void)
{
	int i;
	static Bob fname;
	if ((i = MLReply("Read file: ", fname)) != 1)
		return i;
	return ReadIn(fname.data()) == 1? 1: 0;
}


/*--------------------------------------------------------------------------
 * Create buffers for filenames that may contain wild cards.
 * Returns number of buffers created or already existing.
 */
int WildEdit(const char *fspec, bool create)
{
	char fullSpec[FIO_MAX_PATH + 1];
	FileFinder ff;
	int bufCount = 0;
	if (!CompletePath(fullSpec, fspec))
		return 0;

	if (ff.first(fullSpec))
		do
			bufCount += FileEdit(ff.fname());
		while(ff.next());

	if (bufCount)
		return bufCount;
	if (create && !strchr(fullSpec, '*') && !strchr(fullSpec, '?'))
		return FileEdit(fullSpec);
	return 0;
}

/*--------------------------------------------------------------------------
 * Create buffers for filenames that may contain wild cards along the
 * DOVEPATH environment variable.
 * Returns number of buffers created or already existing.
 */
#if defined(DOVE_FOR_GNU)
const char envPathDelimiter = ':';
#else
const char envPathDelimiter = ';';
#endif

int WildPathEdit(const char *fspec)
{
	static char defaultName[] = "DOVE", suffix[] = "PATH";
	int bufCount = 0;
	char pathspec[FIO_MAX_PATH], *sp, *dp, *pathestr, envStrName[FIO_MAX_PATH];
	const char *exeName = FileExeName();

	strcpy(envStrName, exeName + PathLength(exeName));
	if (dp = strchr(envStrName, '.'))
		*dp = 0;
	else
		strcpy(envStrName, defaultName);
	strcat(envStrName, suffix);

	if (PathLength(fspec) == 0 && (pathestr = getenv(envStrName)))
		for (sp = pathestr; *sp;  )
		{
			for (dp = pathspec ; *sp != envPathDelimiter && *sp; sp++)
				*dp++ = *sp;
			if (*sp)
				sp++;
			strcpy(dp, fspec);
			bufCount += WildEdit(pathspec, 0);
		}
	return bufCount? bufCount: WildEdit(fspec, 1);
}

/*--------------------------------------------------------------------------
 * If a block is marked, ask for a file name
 * and write the block as that file, otherwise
 * let the file save routine do everything.
 */
int FileWriteKey(void)
{
	int i;
	Region r;
	static Bob fname;

	if (curvp->position[MARK].line == 0)
		return FileSave();

	if ((i = MLReply("write block to file: ", fname)) != 1
			|| (i = GetRegion(&r, 0)) != 1)
		return i;

	const char *fn = fname.data();
	if (FileExists(fn) && !MLYesNo("File exists, overwrite"))
	{
		MLWrite("block not written");
		return 0;
	}

	return curvp->position[DOT].line == r.start.line
			&& curvp->position[DOT].offset == r.start.offset?
		WriteOut(fn, &curvp->position[DOT], &curvp->position[MARK]):
		WriteOut(fn, &curvp->position[MARK], &curvp->position[DOT]);
}

/*--------------------------------------------------------------------------
 * Save the contents of the current
 * buffer in its associated file. Does nothing
 * if nothing has changed.
 */
int FileSave(void)
{
	Position start, end;
	char bakFile[FIO_MAX_PATH], tmpFile[FIO_MAX_PATH], *fileName, *extension;
	const char *msg;
	int len;
	
	if (!(curbp->flags & B_CHANGED))
	{
		MLWrite("No changes - no need to write");
		return 1;
	}
	if (curbp->flags & B_READONLY
			|| FileExists(curbp->fname) && IsWriteProtected(curbp->fname))
	{
		MLWrite("File is read-only, can not write");
		return 0;
	}

	time_t fModTime = FileModTime(curbp->fname);
	if (curbp->fModTime && fModTime && curbp->fModTime != fModTime
			&& !MLYesNo("file has changed since read, overwrite"))
	{
		MLWrite("file not written");
		return 0;
	}

	start.line = curbp->linep->forw();
	start.offset = 0;
	end.line = curbp->linep;
	end.offset = end.line->length();
	strcpy(tmpFile, curbp->fname);
	len = strlen(tmpFile);

	do
		tmpFile[len - 1]++;
	while (!isalnum(tmpFile[len - 1]) || FileExists(tmpFile));

	if (WriteOut(tmpFile, &start, &end, curbp->fmode))
	{
		if (modes & MODE_MAKEBAKFILE)
		{
			strcpy(bakFile, curbp->fname);
			fileName = bakFile + PathLength(bakFile);
			extension = strchr(fileName, '.');
			if (extension != 0)
				*extension = 0;
			strcat(bakFile, ".bak");
			if (FileExists(bakFile) && FileRemove(bakFile))
			{
				msg = "Error deleting old .BAK; file saved as: %s";
				goto Error;
			}
			else if (strcmp(curbp->fname, bakFile) != 0 && FileExists(curbp->fname)
					&& !FileRename(curbp->fname, bakFile)
					|| !FileRename(tmpFile, curbp->fname))
			{
				msg = "Error renaming old file; new saved as: %s";
				goto Error;
			}
		}
		else if (FileExists(curbp->fname) && !FileRemove(curbp->fname))
		{
			msg = "Error deleting old file; new saved as: %s";
			goto Error;
		}
		else if (!FileRename(tmpFile, curbp->fname))
		{
			msg = "Error renaming old file; new saved as: %s";
			goto Error;
		}
		curbp->flags &= ~B_CHANGED;
		curbp->fModTime = FileModTime(curbp->fname);
		return 1;
	}
	MLWrite("Error writing file, %s NOT SAVED, %s", curbp->fname, strerror(errno));
	return 0;

Error:
	MLWrite(msg, tmpFile);
	strcpy(curbp->fname, tmpFile);
	curbp->flags &= ~B_CHANGED;
	return 1;
}

/*==========================================================================*/
