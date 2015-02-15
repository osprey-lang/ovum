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

	typedef HANDLE FileHandle;

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
		FILE_OPEN = OPEN_EXISTING,
		// Opens the file if it exists, or creates it otherwise.
		FILE_OPEN_OR_CREATE = OPEN_ALWAYS,
		// Creates a new file. If it already exists, it is overwritten.
		FILE_CREATE = CREATE_ALWAYS,
		// Creates a new file. If it already exists, an error occurs.
		FILE_CREATE_NEW = CREATE_NEW,
	};

	enum FileAccess
	{
		// The file is opened for reading.
		FILE_ACCESS_READ = GENERIC_READ,
		// The file is opened for writing.
		FILE_ACCESS_WRITE = GENERIC_WRITE,
		// The file is opened for reading and writing.
		FILE_ACCESS_READWRITE = GENERIC_READ | GENERIC_WRITE,
	};

	enum FileShare
	{
		// No one else can access the file until it is closed.
		FILE_SHARE_NONE = 0,
		// Other handles can read from the file.
		FILE_SHARE_READ = 0x00000001,
		// Other handles can write to the file.
		FILE_SHARE_WRITE = 0x00000002,
		// Other handles can both read from and write to the file.
		FILE_SHARE_READWRITE = FILE_SHARE_READ | FILE_SHARE_WRITE,
		// The file may be deleted even before the handle is closed.
		FILE_SHARE_DELETE = 0x00000004,
	};

	enum SeekOrigin
	{
		// The offset is relative to the start of the file.
		// On some platforms, this may cause the offset argument
		// to be interpreted as an unsigned value.
		FILE_SEEK_START = FILE_BEGIN,
		// The offset is relative to the current file position.
		FILE_SEEK_CURRENT = FILE_CURRENT,
		// The offset is relative to the end of the file. Positive
		// offsets seek forward from the end.
		FILE_SEEK_END = FILE_END,
	};

	// Internal functions
	FileStatus _FileStatusFromError(DWORD error);

	// Determines whether the specified file exists on disk.
	// This function only tests for files, not directories.
	//   path:
	//     The path name to check.
	// Returns:
	//   True if there is a file with the specified name;
	//   otherwise, false.
	inline bool FileExists(const pathchar_t *path)
	{
		DWORD attrs = GetFileAttributesW(path);
		return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
	}

	// Determines whether the specified directory exists on disk.
	// This function only tests for files, not directories.
	//   path:
	//     The path name to check.
	// Returns:
	//   True if there is a directory with the specified name;
	//   otherwise, false.
	inline bool DirectoryExists(const pathchar_t *path)
	{
		DWORD attrs = GetFileAttributesW(path);
		return attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_DIRECTORY) != 0;
	}

	inline bool FileHandleIsValid(FileHandle *file)
	{
		return *file != nullptr && *file != INVALID_HANDLE_VALUE;
	}

	inline FileStatus OpenFile(const pathchar_t *fileName, FileMode mode, FileAccess access, FileShare share, FileHandle *output)
	{
		HANDLE handle = CreateFileW(
			fileName,
			(DWORD)access,
			(DWORD)share,
			nullptr,
			(DWORD)mode,
			0,
			nullptr
		);
		if (handle == INVALID_HANDLE_VALUE)
			return _FileStatusFromError(GetLastError());
		*output = handle;
		return FILE_OK;
	}

	inline FileStatus CloseFile(FileHandle *file)
	{
		BOOL r = ::CloseHandle(*file);
		if (!r)
			return _FileStatusFromError(GetLastError());
		return FILE_OK;
	}

	inline FileStatus ReadFile(FileHandle *file, size_t count, void *buffer, size_t *bytesRead)
	{
		BOOL r;

#ifdef _WIN64
		// DWORD and size_t are different sizes, can't cast bytesRead to DWORD*.
		DWORD dwBytesRead;
		r = ::ReadFile(*file, buffer, (DWORD)count, &dwBytesRead, nullptr);
		*bytesRead = (size_t)dwBytesRead;
#else // _WIN64
		// DWORD and size_t are the same size, just cast bytesRead to DWORD*.
		r = ::ReadFile(*file, buffer, (DWORD)count, (DWORD*)bytesRead, nullptr);
#endif // _WIN64

		if (!r)
			return _FileStatusFromError(GetLastError());
		return FILE_OK;
	}

	inline FileStatus WriteFile(FileHandle *file, size_t count, const void *buffer, size_t *bytesWritten)
	{
		BOOL r;

#ifdef _WIN64
		// DWORD and size_t are different sizes, can't cast bytesWritten to DWORD*.
		DWORD dwBytesWritten;
		r = ::WriteFile(*file, buffer, (DWORD)count, &dwBytesWritten, nullptr);
		*bytesWritten = (size_t)dwBytesWritten;
#else
		// DWORD and size_t are the same size, just cast bytesWritten to DWORD*.
		r = ::WriteFile(*file, buffer, (DWORD)count, (DWORD*)bytesWritten, nullptr);
#endif
		if (!r)
			return _FileStatusFromError(GetLastError());
		return FILE_OK;
	}

	inline FileStatus SeekFile(FileHandle *file, int64_t offset, SeekOrigin origin, int64_t *newOffset)
	{
		LARGE_INTEGER largeIntOffset;
		largeIntOffset.QuadPart = offset;
		BOOL r = ::SetFilePointerEx(*file, largeIntOffset, (PLARGE_INTEGER)newOffset, (DWORD)origin);
		if (!r)
			return _FileStatusFromError(GetLastError());
		return FILE_OK;
	}

} // namespace os

} // namespace ovum

#endif // VM__OS_FILESYSTEM_H