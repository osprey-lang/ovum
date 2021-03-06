#ifndef IO__PATH_H
#define IO__PATH_H

#include "../aves.h"

class Path
{
public:

#if OVUM_WINDOWS
	static const ovchar_t DirSeparator = '\\';
	static const ovchar_t AltDirSeparator = '/';
	static const ovchar_t VolumeSeparator = ':';
#else
	static const ovchar_t DirSeparator = '/';
	static const ovchar_t AltDirSeparator = '\\';
	static const ovchar_t VolumeSeparator = '/';
#endif

	static LitString<1> DirSeparatorString;

	static const size_t InvalidPathCharsCount;
	static const ovchar_t InvalidPathChars[];

	static const size_t InvalidFileNameCharsCount;
	static const ovchar_t InvalidFileNameChars[];

	static const size_t NOT_FOUND = (size_t)-1;

	inline static bool IsPathSep(ovchar_t ch)
	{
		return ch == DirSeparator || ch == AltDirSeparator;
	}

	static size_t GetExtensionIndex(String *path);

	static bool IsAbsolute(String *path);

	static int GetFullPath(ThreadHandle thread, String *path, String **result);

	static size_t GetRootLength(String *path);

	static int ValidatePath(ThreadHandle thread, String *path, bool checkWildcards);
};

AVES_API NATIVE_FUNCTION(io_Path_get_directorySeparator);
AVES_API NATIVE_FUNCTION(io_Path_get_altDirectorySeparator);

AVES_API NATIVE_FUNCTION(io_Path_getInvalidPathCharsString);
AVES_API NATIVE_FUNCTION(io_Path_getInvalidFileNameCharsString);

AVES_API NATIVE_FUNCTION(io_Path_isAbsolute);

AVES_API NATIVE_FUNCTION(io_Path_join);

AVES_API NATIVE_FUNCTION(io_Path_getFullPath);

AVES_API NATIVE_FUNCTION(io_Path_getFileName);
AVES_API NATIVE_FUNCTION(io_Path_getDirectory);

AVES_API NATIVE_FUNCTION(io_Path_getExtension);
AVES_API NATIVE_FUNCTION(io_Path_hasExtension);
AVES_API NATIVE_FUNCTION(io_Path_changeExtension);

AVES_API NATIVE_FUNCTION(io_Path_validatePath);

#endif // IO__PATH_H
