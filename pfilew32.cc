#if defined(DOVE_FOR_WIN32)
#define WIN32_LEAN_AND_MEAN 1
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include <stdlib.h>

#include "pfileio.h"

void SetFileExeName(const char *name) { }

/*---------------------------------------------------------------------------
 */
const char *FileExeName()
{
	static char exeName[FIO_MAX_PATH];
	if (exeName[0] == 0 && GetModuleFileName(0, exeName, sizeof exeName) == 0)
		exeName[0] = 0;
	return exeName;
}

/*---------------------------------------------------------------------------
 */
int ShellOut(const char *cmdline)
{
	if (cmdline == 0)
		return 1;
	system(cmdline);
	return 0;
}

/*---------------------------------------------------------------------------
 */
const char *FileCfgName()
{
	static char cfgName[FIO_MAX_PATH], defName[] = "dove.cfg";
	if (cfgName[0] == 0)
	{
		char *p, *fileName, *ext = strchr(defName, '.');
		strcpy(cfgName, FileExeName());
		fileName = cfgName + PathLength(cfgName);
		if (!(p = strchr(fileName, '.')))
			strcpy(fileName, defName);
		else
			strcpy(p, ext);

		if (FileExists(fileName))
			strcpy(cfgName, fileName);
	}
	return cfgName;
}

/*---------------------------------------------------------------------------
 */
bool CompletePath(char *wholePath, const char *partPath)
{
	char *unused;
	DWORD length = GetFullPathName(partPath, FIO_MAX_PATH, wholePath, &unused);
	return length == 0 || length > FIO_MAX_PATH? 0: 1;
}

/* ------------------------------------------------------------------------ */
struct W32FFI
{
	_finddata_t finddata;
	long handle;
	unsigned plen;
	char fullname[MAX_PATH];
};

bool FileFinder::first(const char *pattern, bool includeDirs)
{
	close();
	incDir = includeDirs;

	W32FFI *fi = (W32FFI *)buffer;
	assert(sizeof buffer >= sizeof *fi);

	if ((fi->handle = _findfirst(pattern, &fi->finddata)) == -1)
		return false;

	name = fi->fullname;
	memcpy(name, pattern, fi->plen = PathLength(pattern));
	strcpy(name + fi->plen, fi->finddata.name);

	if (!(fi->finddata.attrib & (_A_HIDDEN | _A_SYSTEM))
			&& (incDir || !(fi->finddata.attrib & _A_SUBDIR)))
	{
		strcpy(fi->fullname + fi->plen, fi->finddata.name);
		if (fi->finddata.attrib & _A_SUBDIR)
			strcat(fi->fullname, "\\");
		return true;
	}
	return next();
}

bool FileFinder::next()
{
	W32FFI *fi = (W32FFI *)buffer;
	while (_findnext(fi->handle, &fi->finddata) != -1)
		if (!(fi->finddata.attrib & (_A_HIDDEN | _A_SYSTEM))
				&& (incDir || !(fi->finddata.attrib & _A_SUBDIR)))
		{
			strcpy(fi->fullname + fi->plen, fi->finddata.name);
			if (fi->finddata.attrib & _A_SUBDIR)
				strcat(fi->fullname, "\\");
			return true;
		}
	close();
	return false;
}

void FileFinder::close()
{
	if (name)
		_findclose(((W32FFI *)buffer)->handle);
	name = 0;
}

/*-----------------------------------------------------------------------------
 * returns the length of the path, with delimiter.
 */
unsigned PathLength(const char *pathAndFile)
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

/* ------------------------------------------------------------------------ */
bool IsSubDirectory(const char *path)
{
	return (GetFileAttributes(path) & _A_SUBDIR)? true: false;
}

time_t FileModTime(const char *path)
{
  struct stat buf;
	return stat(path, &buf) == 0? buf.st_mtime: 0;
}

/*-----------------------------------------------------------------------------
 */
bool IsWriteProtected(const char *path) { return access(path, W_OK) != 0; }
bool FileExists(const char *path) { return access(path, F_OK) == 0; }
bool FileRemove(const char *path) { return unlink(path) == 0; }
bool FileRename(const char *oldName, const char *newName)
    { return rename(oldName, newName) == 0; }

//---------------------------------------------------------------------------
// returns handle

int FileOpen(const char *name, int *mode, time_t *modTime)
{
  int handle = open(name, O_RDONLY | O_BINARY);
  if (handle != -1 && (mode || modTime))
  {
    struct stat buf;
  	if (fstat(handle, &buf) != 0)
    {
      close(handle);
      handle = -1;
    }
    else
    {
      if (mode)
        *mode = buf.st_mode;
      if (modTime)
        *modTime = buf.st_mtime;
    }
  }
  return handle;
}

int FileCreate(const char *name, int mode)
{
	return open(name, O_CREAT | O_RDWR | O_TRUNC | O_BINARY,
      mode? mode: (S_IWRITE | S_IREAD));
}

//---------------------------------------------------------------------------
void FileClose(int handle)
{
	if (handle != FIO_INVALID_HANDLE)
		close(handle);
}

//---------------------------------------------------------------------------
// returns bytes read/written
unsigned FileRead(int handle, void *data, unsigned size)
{
	return read(handle, data, size);
}

//---------------------------------------------------------------------------
// returns bytes read/written
unsigned FileWrite(int handle, const void *data, unsigned size)
{
	return write(handle, data, size);
}

/*===========================================================================*/
#endif
