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

	// Specifies how a file may be accessed, once opened. These values
	// can be combined as flags. FILE_ACCESS_READWRITE is a shorthand
	// for FILE_ACCESS_READ | FILE_ACCESS_WRITE.
	enum FileAccess
	{
		// The file is opened for reading.
		FILE_ACCESS_READ = ...,
		// The file is opened for writing.
		FILE_ACCESS_WRITE = ...,
		// The file is opened for reading and writing.
		FILE_ACCESS_READWRITE = ...,
	};

	// Specifies whether and how the file may be accessed by other
	// processes once opened. These values can be combined as flags.
	// FILE_SHARE_READWRITE is a shorthand for FILE_SHARE_READ | FILE_SHARE_WRITE.
	// These flags may be advisory (not enforced on a system level)
	// on some OSes. Also, the FILE_SHARE_DELETE flag may not be
	// supported, or may not be distinct from FILE_SHARE_WRITE.
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

	// Determines whether the specified file handle is (probably)
	// valid; that is, refers to an open file. This accuracy of
	// this check varies between implementations. It should only
	// really be used to test whether a file handle variable has
	// been initialized to something other than the default value.
	//   file:
	//     The file handle to test.
	// Returns:
	//   True if the file handle looks like it might be valid;
	//   otherwise, false.
	bool FileHandleIsValid(FileHandle *file);

	// Opens a file with the specified name, mode, read/write access
	// and sharing mode.
	//   fileName:
	//     The name of the file to open.
	//   mode:
	//     Specifies how the file is opened. See FileMode's members
	//     for a description of each mode.
	//   access:
	//     Specifies whether the file should be opened for reading,
	//     writing, or both.
	//   share:
	//     Specifies whether other processes can open the file at
	//     the same time, and if so, what they can do with it.
	//   output:
	//     Receives a handle to the newly opened file, if the function
	//     call succeeds.
	// Returns:
	//   A status code which indicates whether an error occurred. If
	//   the file was opened successfully, FILE_OK is returned.
	FileStatus OpenFile(const pathchar_t *fileName, FileMode mode, FileAccess access, FileShare share, FileHandle *output);

	// Closes a file handle previously opened with OpenFile.
	//   file:
	//     The file to close.
	// Returns:
	//   A status code which indicates whether an error occurred. If
	//   the file was closed successfully, FILE_OK is returned.
	FileStatus CloseFile(FileHandle *file);

	// Reads a number of bytes from the specified file.
	//   file:
	//     The file to read from.
	//   count:
	//     The maximum number of bytes to read. The function may read
	//     fewer bytes, depending on the implementation. The bytesRead
	//     parameter is used to find out how many bytes were actually
	//     consumed.
	//   buffer:
	//     The buffer that receives bytes from the file. This buffer
	//     MUST be large enough to contain at least count bytes.
	//   bytesRead:
	//     Receives the actual number of bytes that were read. If this
	//     is set to 0, it means the end of the file was reached.
	// Returns:
	//   A status code which indicates whether an error occurred. If the
	//   data was read successfully, FILE_OK is returned. This function
	//   does not return FILE_EOF; that status code is reserved for future
	//   and internal use. End-of-file is signalled by setting bytesRead
	//   to 0.
	FileStatus ReadFile(FileHandle *file, size_t count, void *buffer, size_t *bytesRead);

	// Writes a number of bytes to the specified file.
	//   file:
	//     The file to write to.
	//   count:
	//     The maximum number of bytes to write.
	//   buffer:
	//     The buffer whose bytes are written to the file. The contents
	//     of this buffer must not be modified while the file is being
	//     written, or undefined behaviour will occur.
	//   bytesWritten:
	//     Receives the number of bytes written. This should equal count
	//     when writing to a regular file.
	// Returns:
	//   A status code which indicates whether an error occurred. If the
	//   data was written successfully, FILE_OK is returned. This function
	//   does not return FILE_EOF; that status code is reserved for future
	//   and internal use. Moreover, the file expands as needed.
	FileStatus WriteFile(FileHandle *file, size_t count, const void *buffer, size_t *bytesWritten);

	// Sets the current file cursor for the specified file.
	//   file:
	//     The file to seek within.
	//   offset:
	//     The number of bytes to move the file cursor. Positive values
	//     seek toward the end of the file, negative values toward the
	//     beginning.
	//   origin:
	//     Determines what "end" of the file the offset is relative to.
	//     Note: when origin = FILE_SEEK_END, a positive offset will
	//     still seek forwards in the file; that is, past the end.
	//   newOffset:
	//     Receives the new position of the file cursor, relative to the
	//     beginning of the file.
	// Returns:
	//   A status code which indicates whether an error occurred. If the
	//   file cursor was successfully changed, FILE_OK is returned.
	FileStatus SeekFile(FileHandle *file, int64_t offset, SeekOrigin origin, int64_t *newOffset);

} // namespace os

} // namespace ovum

#endif // VM__OS_FILESYSTEM_H