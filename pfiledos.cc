#if defined(DOVE_FOR_DOS)

#error code preserved from previous versions of DOVE, but not currently supported

#include <dir.h>
#include <dos.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include <pfileio.h>

/*--------------------------------------------------------------------------
 */
static char invalidChars[] = ".\"[]:|<>+=;,";
#define InvalidChar(c) (c >= 0 && c < 0x21 || strchr(invalidChars, c) != 0)


/* Thoroughly checks out just the file name */
static int ValidFileName(char *fileName)
{
	unsigned len;

	for (len = 0; len < 8 && *fileName && *fileName != '.'; fileName++, len++)
	{
		if (InvalidChar(*fileName))
			return 0;
	}
	if (len == 0)
		return 0;
	if (*fileName == 0)
		return 1;
	if (*fileName++ != '.')
		return 0;
	for (len = 0; len < 3 && *fileName; fileName++, len++)
	{
		if (InvalidChar(*fileName))
			return 0;
	}
	return *fileName == 0? 1: 0;
}

/* forms a complete path from any path and checks out the file Name */
int CompletePath(char *wholePath, const char *partPath)
{
	char currentDir[FIO_MAX_PATH + 1], delimiterChar, *delimit, *colon;
	int rCode = 0, pathLen;
	unsigned driveCount, newDrive, oldDrive;

	if (IsSubDirectory(partPath))
		return 0;

	pathLen = PathLength(partPath);
	if (!ValidFileName(partPath + pathLen))
		return 0;
	colon = strchr(partPath, ':');
	if ((partPath[0] == '\\' || partPath[0] == '/')
			&& (partPath[1] == '\\' || partPath[1] == '/')
			|| colon > partPath + 1 || colon == partPath)
	{	/*
		 * possibly UNC or NetWare Path, fully specified,
		 * just check to see that it exists.
		 */
		delimit = partPath + pathLen;
		if (delimit != colon)
			delimit--;
		delimiterChar = *delimit;
		*delimit = 0;
		if (IsSubDirectory(partPath))
		{
			*delimit = delimiterChar;
			strcpy(wholePath, partPath);
			strlwr(wholePath);
			if (*(delimit = wholePath + strlen(wholePath) - 1) == '.')
				*delimit = 0;
			rCode = 1;
		}
		else
			*delimit = delimiterChar;
		return rCode;
	}

	newDrive = UINT_MAX;
	_dos_getdrive(&oldDrive);
	
	if (colon != 0)
	{
		newDrive = tolower(*partPath) - 'a' + 1;
		_dos_setdrive(newDrive, &driveCount);
		partPath += 2;
		pathLen -= 2;
	}
	if (getcwd(currentDir, sizeof(currentDir)) == 0)
		;
	else if (pathLen == 0)
	{
		strcpy(wholePath, currentDir);
		delimit = wholePath + strlen(wholePath);
		if (delimit - wholePath > 3)
			*delimit++ = '\\';
		strcpy(delimit, partPath);
		strlwr(wholePath);
		if (*(delimit = wholePath + strlen(wholePath) - 1) == '.')
			*delimit = 0;
		rCode = 1;
	}
	else
	{
		delimit = partPath + pathLen;
		if (pathLen > 1)
			delimit--;
		delimiterChar = *delimit;
		*delimit = 0;
		if (chdir(partPath) != 0)
			*delimit = delimiterChar;
		else
		{
			*delimit = delimiterChar;
			if (getcwd(wholePath, FIO_MAX_PATH - 2))
			{
				strcat(wholePath, delimit);
				strlwr(wholePath);
				if (*(delimit = wholePath + strlen(wholePath) - 1) == '.')
					*delimit = 0;
				rCode = 1;
			}
		}
		chdir(currentDir);
	}
	if (newDrive != UINT_MAX)
		_dos_setdrive(oldDrive, &driveCount);
	return rCode;
}

/* ------------------------------------------------------------------------ */
void ExpandArg(char *buffer, char *path)
{
	char *t, *b;
	int isWild;
	for (b = buffer, t = path, isWild = 0; *t != '\0'; t++, b++)
		if ((*b = *t) == '?' || *t == '*')
			isWild = 1;
	*b = '\0';
	if (isWild)
		return;
	for (b = buffer + strlen(buffer) - 1; *b != ':' && b >= buffer; b--)
		;
	b = (*b == ':'? b + 1: buffer);
	if (b[0] == '\0' || b[0] == '\\' && b[1] == '\0')
		strcat(buffer, "*.*");
	else
	{
		while (*b == '.')
			b++;
		if (*b == '\0')
			strcat(buffer, "\\*.*");
	}
}

/* ------------------------------------------------------------------------ */
void ExpandArgToFile(char *buffer, char *path)
{
	char *t, *b;
	int isWild;
	for (b = buffer, t = path, isWild = 0; *t != '\0'; t++, b++)
		if ((*b = *t) == '?' || *t == '*')
			isWild = 1;
	*b = '\0';
	if (isWild)
		return;
	for (b = buffer + strlen(buffer) - 1; *b != ':' && b >= buffer; b--)
		;
	b = (*b == ':'? b + 1: buffer);
	if (b[0] == '\0' || b[0] == '\\' && b[1] == '\0')
		strcat(buffer, "*.*");
	else
	{
		while (*b == '.')
			b++;
		if (*b == '\0' || IsSubDirectory(buffer))
			strcat(buffer, "\\*.*");
	}
}

/* ------------------------------------------------------------------------ */
static void SetFFInfo(FILEFINDINFO *ffi)
{
	_finddata_t *fp = (_finddata_t *)&ffi->buffer[0];
	ffi->name = fp->name;
	ffi->time = 0; // fp->wr_time;
	ffi->date = 0; // fp->wr_date;
	ffi->size = fp->size;
	ffi->attribute = fp->attrib;
}

int FileFindFirst(char *pattern, unsigned attribFilter, FILEFINDINFO *ffi)
{
	int err;
	if (!(err = _findfirst(pattern, (_finddata_t *)&ffi->buffer[0])))
		SetFFInfo(ffi);
	return err;
}

int FileFindNext(FILEFINDINFO *ffi)
{
	int err;
	if (!(err = _findnext((_finddata_t *)&ffi->buffer[0])))
		SetFFInfo(ffi);
	return err;
}

void FileFindClose(FILEFINDINFO *ffi)
{
	_findclose((_finddata_t *)&ffi->buffer[0]);
}

/* ------------------------------------------------------------------------ */
int IsSubDirectory(char *path)
{
	unsigned attrib;
	return (GetFileAttributes(path) & _A_SUBDIR)? 1: 0;
}

/*-----------------------------------------------------------------------------
 */
int IsWriteProtected(char *path)
{
	unsigned attrib;
	return _dos_getfileattr(path, &attrib) == 0? (attrib & _A_RDONLY): 0;
}

/*-----------------------------------------------------------------------------
 */
int FileExists(char *path)
{
	unsigned attrib;
	return _dos_getfileattr(path, &attrib) == 0?
			!(attrib & (_A_VOLID|_A_SUBDIR)): 0;
}

/*-----------------------------------------------------------------------------
 * returns the length of the path, with delimiter.
 */
unsigned PathLength(char *pathAndFile)
{
	unsigned len;
	char c;
	for (len = strlen(pathAndFile); len > 0; len--)
	{
		c = pathAndFile[len - 1];
		if (c == '\\' || c == ':' || c == '/')
			break;
	}
	return len;
}

/*===========================================================================*/
#endif

