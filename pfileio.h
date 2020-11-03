#ifndef PFILEIO_H
#define PFILEIO_H 1

#include <time.h>
#include <stddef.h>

#define FIO_MAX_PATH	255	// # of bytes, file path and name

typedef unsigned long fsize_t; // type large enough to hold file sizes

unsigned PathLength(const char *pathAndFile);
bool CompletePath(char *wholePath, const char *partPath);
void SetFileExeName(const char *name);
const char *FileExeName();
const char *FileCfgName();
int ShellOut(const char *command);

class FileFinder
{
	char *name, buffer[1024];
	bool incDir;

	// disable copy and assignment
	FileFinder(const FileFinder &other);
	FileFinder &operator=(const FileFinder &other);

public:
	FileFinder(): name(0) {}
	~FileFinder() { close(); }

	bool first(const char *pattern, bool includeDirs = false);
	bool next();
	void close();
	const char *fname() { return name; }
};

bool IsSubDirectory(const char *path);
bool IsWriteProtected(const char *path);
bool FileExists(const char *path);
bool FileRemove(const char *path);
bool FileRename(const char *oldName, const char *newName);
time_t FileModTime(const char *name);

#define FIO_INVALID_HANDLE -1

// returns handle
int FileOpen(const char *name, int *mode = 0, time_t *modTime = 0);
int FileCreate(const char *name, int mode = 0); // mode of 0 uses OS default mode

void FileClose(int handle);

// returns bytes read/written
size_t FileRead(int handle, void *data, size_t size);
size_t FileWrite(int handle, const void *data, size_t size);

/*===========================================================================*/
#endif
