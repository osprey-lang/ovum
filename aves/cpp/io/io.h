#pragma once

#ifndef AVES__IO_H
#define AVES__IO_H

#include "../aves.h"

// This file contains helper functions for I/O stuff.

namespace io
{
#ifdef _WIN32
	// Corresponds to the return type of GetLastError()
	typedef DWORD ErrorCode;
#else
	// TODO: POSIX error codes
	typedef int ErrorCode;
#endif

	// Throws an io.IOError or a class derived from it, based on an error code.
	// Specialized classes are used when they are available; for example, if the
	// error code represents "file not found", io.FileNotFoundError is thrown.
	// If no suitable derived class can be found, then io.IOError is thrown, with
	// a general I/O error message.
	// Parameters:
	//   thread:
	//       The thread on which to throw the error.
	//   code:
	//       The error code to process.
	//   pathName:
	//       The path or file name that caused the error. Not all error codes make
	//       use of this parameter.
	int ThrowIOError(ThreadHandle thread, ErrorCode code, String *pathName = nullptr);

	int ReadFileAttributes(ThreadHandle thread, String *fileName, WIN32_FILE_ATTRIBUTE_DATA *data, bool throwOnError, bool &success);
}

#endif // AVES__IO_H