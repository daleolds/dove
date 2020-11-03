#if defined(DOVE_FOR_GNU)

#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <assert.h>
#include <errno.h>
#include <glob.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>

extern char **environ;

#include "pfileio.h"

/*---------------------------------------------------------------------------
 */
static const char *ExeName;

void SetFileExeName(const char *name) { ExeName = name; }
const char *FileExeName() { return ExeName; }

/*---------------------------------------------------------------------------
 */
int ShellOut(const char *command)
{
	if (command == 0)
		return 1;
	int pid = fork(), status;
	if (pid == -1)
		return -1;
	if (pid == 0)
	{
		const char *argv[4];
		argv[0] = getenv("SHELL");
		if (!argv[0])
			argv[0] = "/bin/sh";
		if (*command)
		{
			argv[1] = "-c";
			argv[2] = command;
			argv[3] = 0;
		}
		else
		{
			printf("entering sub-shell--------------------------------\n");
			argv[1] = "-i";
			argv[2] = 0;
		}
		//fprintf(stderr, "exec-ing %s %s %s\n", argv[0], argv[1], command);
		execve(argv[0], (char *const *)argv, environ);
		exit(127);
	}
	for (;;)
		if (waitpid(pid, &status, 0) != -1)
			return WEXITSTATUS(status);
		else if (errno != EINTR)
			return -1;
}

/*---------------------------------------------------------------------------
 */
const char *FileCfgName()
{
	static char cfgName[FIO_MAX_PATH];
	if (cfgName[0] == 0)
	{
		char *homeDir = getenv("HOME");
		if (homeDir)
		{
			strcpy(cfgName, homeDir);
			strcat(cfgName, "/");
		}
		strcat(cfgName, ".");
		strcat(cfgName, ExeName + PathLength(ExeName));
		strcat(cfgName, "conf");
	}
	return cfgName;
}

/*--------------------------------------------------------------------------
 * forms a complete path from any path and checks out the file Name
 */
bool CompletePath(char *wholePath, const char *pPath)
{
	char *partPath = (char *)pPath;
	int err;
	glob_t g;

	if (IsSubDirectory(partPath))
		return false;

	size_t pathLen = PathLength(partPath);
	char *delimit = partPath + pathLen;
	if (pathLen == 0)
	{
		if (!getcwd(wholePath, FIO_MAX_PATH - 1))
			return false;
		strcat(wholePath, "/");
	}
	else
	{
		*--delimit = 0;
		if ((err = glob(partPath, 0, 0, &g))
				|| g.gl_pathc != 1
				|| strlen(g.gl_pathv[0]) + strlen(delimit + 1) > FIO_MAX_PATH
				|| !IsSubDirectory(g.gl_pathv[0]))
		{
			*delimit = '/';
			if (!err)
				globfree(&g);
			return false;
		}
		*delimit = '/';
		strcpy(wholePath, g.gl_pathv[0]);
	}
	if (strlen(wholePath) + strlen(delimit) > FIO_MAX_PATH)
		return false;
	strcat(wholePath, delimit);
	return true;
}

//------------------------------------------------------------------------

struct GNUFFI { unsigned curName; glob_t glob; };

bool FileFinder::first(const char *pattern, bool includeDirs)
{
	close();
	incDir = includeDirs;
	GNUFFI *g = (GNUFFI *)buffer;
	assert(sizeof buffer >= sizeof *g);
	g->curName = 0;
	if (glob(pattern, GLOB_MARK | GLOB_NOSORT, 0, &g->glob) != 0)
		return false;

	if (next())
		return true;

	globfree(&g->glob);
	return false;
}

bool FileFinder::next()
{
	GNUFFI *g = (GNUFFI *)buffer;
	while (g->curName < g->glob.gl_pathc
			&& (name = g->glob.gl_pathv[g->curName++]))
		if (*name && (incDir || name[strlen(name) - 1] != '/'))
			return true;

	globfree(&g->glob);
	name = 0;
	return false;
}

void FileFinder::close()
{
	if (name)
	{
		GNUFFI *g = (GNUFFI *)buffer;
		globfree(&g->glob);
	}
	name = 0;
}

/*-----------------------------------------------------------------------------
 * returns the length of the path, with delimiter.
 */
unsigned PathLength(const char *pathAndFile)
{
	unsigned len = strlen(pathAndFile);
	while (len > 0 && pathAndFile[len - 1] != '/')
		len--;
	return len;
}

/* ------------------------------------------------------------------------ */
bool IsSubDirectory(const char *path)
{
	struct stat buf;
	return stat(path, &buf) == 0 && S_ISDIR(buf.st_mode);
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
  int handle = open(name, O_RDONLY);
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
	return open(name, O_CREAT | O_RDWR | O_TRUNC,
      mode? mode: (S_IWUSR | S_IRUSR | S_IWGRP | S_IRGRP | S_IROTH));
}

//---------------------------------------------------------------------------
void FileClose(int handle)
{
	if (handle != FIO_INVALID_HANDLE)
		close(handle);
}

//---------------------------------------------------------------------------
// returns bytes read/written
size_t FileRead(int handle, void *data, size_t size)
{
	return read(handle, data, size);
}

//---------------------------------------------------------------------------
// returns bytes read/written
size_t FileWrite(int handle, const void *data, size_t size)
{
	return write(handle, data, size);
}

/*===========================================================================*/
#endif
