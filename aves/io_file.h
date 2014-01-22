#pragma once

#ifndef IO__FILE_H
#define IO__FILE_H

// This file also includes the implementation of io.FileStream.

#include "aves.h"

// Keep all these enum values synchronised with osp/io/fileenums.osp

enum class FileMode
{
	OPEN           = 1,
	OPEN_OR_CREATE = 2,
	CREATE         = 3,
	CREATE_NEW_    = 4, // Windows headers define a CREATE_NEW macro, sigh
	TRUNCATE       = 5,
	APPEND         = 6,
};

enum class FileAccess
{
	READ       = 1,
	WRITE      = 2,
	READ_WRITE = READ | WRITE,
};
ENUM_OPS(FileAccess, int);

enum class FileShare
{
	NONE       = 0,
	READ       = 1,
	WRITE      = 2,
	READ_WRITE = READ | WRITE,
	DELETE_    = 4, // Windows headers define a DELETE macro, sigh
};
ENUM_OPS(FileShare, int);

enum class SeekOrigin
{
	START = 1,
	CURRENT = 2,
	END = 3,
};

AVES_API NATIVE_FUNCTION(io_File_existsInternal);
AVES_API NATIVE_FUNCTION(io_File_getSizeInternal);
AVES_API NATIVE_FUNCTION(io_File_deleteInternal);
AVES_API NATIVE_FUNCTION(io_File_moveInternal);

// FileStream implementation

class FileStream
{
#ifdef _WIN32
	typedef HANDLE FileHandle;
#else
	typedef FILE *FileHandle;
#endif

public:
	HANDLE handle;
	// Cached, so that canRead and canWrite are fast.
	FileAccess access;

	// TODO: File buffering and other fun stuff

	void EnsureOpen(ThreadHandle thread);

	static void ErrorHandleClosed(ThreadHandle thread);
};

AVES_API void io_FileStream_initType(TypeHandle type);

AVES_API NATIVE_FUNCTION(io_FileStream_init);

AVES_API NATIVE_FUNCTION(io_FileStream_get_canRead);
AVES_API NATIVE_FUNCTION(io_FileStream_get_canWrite);
AVES_API NATIVE_FUNCTION(io_FileStream_get_canSeek);

AVES_API NATIVE_FUNCTION(io_FileStream_get_length);

AVES_API NATIVE_FUNCTION(io_FileStream_readByte);
AVES_API NATIVE_FUNCTION(io_FileStream_readMaxInternal);

AVES_API NATIVE_FUNCTION(io_FileStream_writeByte);
AVES_API NATIVE_FUNCTION(io_FileStream_writeInternal);

AVES_API NATIVE_FUNCTION(io_FileStream_flush);

AVES_API NATIVE_FUNCTION(io_FileStream_seekInternal);

AVES_API NATIVE_FUNCTION(io_FileStream_close);

bool io_FileStream_getReferences(void *basePtr, unsigned int &valc, Value **target);
void io_FileStream_finalize(ThreadHandle thread, void *basePtr);

#endif // IO__FILE_H