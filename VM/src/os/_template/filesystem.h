#pragma once

#ifndef VM__OS_FILESYSTEM_H
#define VM__OS_FILESYSTEM_H

#include "def.h"
#include "../../../inc/ov_vm.h"

// Note: The functions exported by this header file depend on pathchar_t,
// which is defined in Ovum's public headers. If you require a special
// definition of pathchar_t, modify ov_pathchar.h.

namespace ovum
{

namespace os
{

	typedef ... FileHandle;

	enum FileStatus
	{
		// EVERYTHING IS FINE. There is no need to worry.
		FILE_OK = 0,
		// Unspecified I/O error.
		FILE_IO_ERROR = 1,
		// The end of the file has been reached.
		FILE_EOF = 2,
		// The file could not be found.
		FILE_NOT_FOUND = 3,
		// Access to the file is denied.
		FILE_ACCESS_DENIED = 4,
		// An attempt was made to open a file with FILE_CREATE_NEW, but
		// the file already exists.
		FILE_ALREADY_EXISTS = 5,
	};

	enum FileMode
	{
		// Opens an existing file. If it doesn't exist, an error occurrs.
		FILE_OPEN = ...,
		// Opens the file if it exists, or creates it otherwise.
		FILE_OPEN_OR_CREATE = ...,
		// Creates a new file. If it already exists, it is overwritten.
		FILE_CREATE = ...,
		// Creates a new file. If it already exists, an error occurs.
		FILE_CREATE_NEW = ...,
	};

	enum FileAccess
	{
		// The file is opened for reading.
		FILE_ACCESS_READ = ...,
		// The file is opened for writing.
		FILE_ACCESS_WRITE = ...,
		// The file is opened for reading and writing.
		FILE_ACCESS_READWRITE = ...,
	};

	enum FileShare
	{
		// No one else can access the file until it is closed.
		FILE_SHARE_NONE = ...,
		// Other handles can read from the file.
		FILE_SHARE_READ = ...,
		// Other handles can write to the file.
		FILE_SHARE_WRITE = ...,
		// Other handles can both read from and write to the file.
		FILE_SHARE_READWRITE = ...,
		// The file may be deleted even before the handle is closed.
		FILE_SHARE_DELETE = ...,
	};

	enum SeekOrigin
	{
		// The offset is relative to the start of the file.
		// On some platforms, this may cause the offset argument
		// to be interpreted as an unsigned value.
		FILE_SEEK_START = ...,
		// The offset is relative to the current file position.
		FILE_SEEK_CURRENT = ...,
		// The offset is relative to the end of the file. Positive
		// offsets seek forward from the end.
		FILE_SEEK_END = ...,
	};

	// Determines whether the specified file exists on disk.
	// This function only tests for files, not directories.
	//   path:
	//     The path name to check.
	// Returns:
	//   True if there is a file with the specified name;
	//   otherwise, false.
	bool FileExists(const pathchar_t *path);

	// Determines whether the specified directory exists on disk.
	// This function only tests for files, not directories.
	//   path:
	//     The path name to check.
	// Returns:
	//   True if there is a directory with the specified name;
	//   otherwise, false.
	bool DirectoryExists(const pathchar_t *path);

	bool FileHandleIsValid(FileHandle *file);

	FileStatus OpenFile(const pathchar_t *fileName, FileMode mode, FileAccess access, FileShare share, FileHandle *output);

	FileStatus CloseFile(FileHandle *file);

	FileStatus ReadFile(FileHandle *file, size_t count, void *buffer, size_t *bytesRead);

	FileStatus WriteFile(FileHandle *file, size_t count, const void *buffer, size_t *bytesWritten);

	FileStatus SeekFile(FileHandle *file, int64_t offset, SeekOrigin origin, int64_t *newOffset);

} // namespace os

} // namespace ovum

#endif // VM__OS_FILESYSTEM_H