#pragma once

#ifndef IO__PATH_H
#define IO__PATH_H

#include "aves.h"

class Path
{
public:

#ifdef _WIN32
	static const uchar DirSeparator = '\\';
	static const uchar AltDirSeparator = '/';
	static const uchar VolumeSeparator = ':';
#else
	static const uchar DirSeparator = '/';
	static const uchar AltDirSeparator = '\\';
	static const uchar VolumeSeparator = '/';
#endif

	static LitString<1> DirSeparatorString;

	static const int InvalidPathCharsCount;
	static const uchar InvalidPathChars[];

	static const int InvalidFileNameCharsCount;
	static const uchar InvalidFileNameChars[];

	static int32_t GetExtensionIndex(String *path);

	static bool IsAbsolute(String *path);

	static int GetFullPath(ThreadHandle thread, String *path, String **result);

	static int ValidatePath(ThreadHandle thread, String *path, bool checkWildcards);
};

AVES_API NATIVE_FUNCTION(io_Path_get_directorySeparator);
AVES_API NATIVE_FUNCTION(io_Path_get_altDirectorySeparator);

AVES_API NATIVE_FUNCTION(io_Path_getInvalidPathCharsString);
AVES_API NATIVE_FUNCTION(io_Path_getInvalidFileNameCharsString);

AVES_API NATIVE_FUNCTION(io_Path_isAbsolute);

AVES_API NATIVE_FUNCTION(io_Path_join);

AVES_API NATIVE_FUNCTION(io_Path_getFullPath);

AVES_API NATIVE_FUNCTION(io_Path_getExtension);
AVES_API NATIVE_FUNCTION(io_Path_hasExtension);
AVES_API NATIVE_FUNCTION(io_Path_changeExtension);

AVES_API NATIVE_FUNCTION(io_Path_validatePath);

#endif // IO__PATH_H