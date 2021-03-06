#include "path.h"
#include "io.h"
#include "../aves_state.h"
#include <ovum_string.h>
#include <memory>

using namespace aves;

LitString<1> Path::DirSeparatorString = { 1, 0, StringFlags::STATIC, Path::DirSeparator,0 };

const size_t Path::InvalidPathCharsCount = 36;
// This list is also duplicated in ValidatePath.
const ovchar_t Path::InvalidPathChars[] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
	// These are based on the return value of System.IO.Path.GetInvalidPathChars() + MSDN.
	// My understanding is that Unix systems generally disallow the same characters in paths.
	'"', '<', '>', '|',
};

const size_t Path::InvalidFileNameCharsCount = 41;
const ovchar_t Path::InvalidFileNameChars[] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
	// These are taken from System.IO.Path.GetInvalidFileNameChars() + MSDN.
	'"', '<', '>', '|', '/', '\\', '?', '*', ':',
};

size_t Path::GetExtensionIndex(String *path)
{
	for (size_t i = path->length; --i >= 0;)
	{
		ovchar_t ch = (&path->firstChar)[i];

		if (ch == '.')
			return i;

		if (ch == Path::DirSeparator ||
			ch == Path::AltDirSeparator ||
			ch == Path::VolumeSeparator)
			break;
	}

	return NOT_FOUND;
}

bool Path::IsAbsolute(String *path)
{
	const ovchar_t *chp = &path->firstChar;

	const size_t length = path->length;
	// If the path begins with a directory separator, or (on Windows) volume name + ':',
	// the path is considered absolute.
	if (length >= 1 && IsPathSep(chp[0])
#if OVUM_WINDOWS
		|| length >= 2 && chp[1] == Path::VolumeSeparator
#endif
		)
		return true;

	return false;
}

int Path::GetFullPath(ThreadHandle thread, String *path, String **result)
{
	DWORD bufferLength = max(MAX_PATH, path->length + 1);
	bool retry;
	do
	{
		retry = false;
		std::unique_ptr<WCHAR[]> buffer(new WCHAR[bufferLength]);
		DWORD r = GetFullPathNameW((LPCWSTR)&path->firstChar, bufferLength, buffer.get(), nullptr);

		if (r == 0)
			return io::ThrowIOError(thread, GetLastError(), path);

		if (r >= bufferLength)
		{
			// If the buffer is too small, r contains the required buffer size,
			// including the final \0
			bufferLength = r;
			retry = true;
		}
		else
		{
			// If the buffer is big enough, r contains the actual length of the
			// full path, NOT including the final \0
			*result = GC_ConstructString(thread, (size_t)r, (const ovchar_t*)buffer.get());
		}
	} while (retry);

	return *result ? OVUM_SUCCESS : OVUM_ERROR_NO_MEMORY;
}

size_t Path::GetRootLength(String *path)
{
	size_t index = 0;
	size_t length = path->length;
	const ovchar_t *chp = &path->firstChar;

	if (length >= 1 && IsPathSep(chp[0]))
	{
		index = 1;
	}
#if OVUM_WINDOWS
	else if (length >= 2 && chp[1] == ':')
	{
		// Volume label + ':'
		index = 2;
		if (length >= 3 && IsPathSep(chp[2]))
			index++;
	}
#endif

	return index;
}

int Path::ValidatePath(ThreadHandle thread, String *path, bool checkWildcards)
{
	Aves *aves = Aves::Get(thread);

	bool error = false;

	const ovchar_t *chp = &path->firstChar;
	for (size_t i = 0; i < path->length; i++)
	{
		const ovchar_t ch = *chp++;

		if (ch < 32 || ch == '\"' || ch == '<' || ch == '>' || ch == '|')
		{
			error = true;
			break;
		}

		if (checkWildcards && (ch == '*' || ch == '?'))
		{
			error = true;
			break;
		}
	}

	if (error == true)
	{
		VM_PushNull(thread); // message, use default
		VM_PushString(thread, strings::path); // paramName
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentError, 2);
	}

	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(io_Path_get_directorySeparator)
{
	String *str = GC_ConstructString(thread, 1, &Path::DirSeparator);
	if (!str) return OVUM_ERROR_NO_MEMORY;
	VM_PushString(thread, str);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(io_Path_get_altDirectorySeparator)
{
	String *str = GC_ConstructString(thread, 1, &Path::AltDirSeparator);
	if (!str) return OVUM_ERROR_NO_MEMORY;
	VM_PushString(thread, str);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(io_Path_getInvalidPathCharsString)
{
	String *str = GC_ConstructString(thread, Path::InvalidPathCharsCount, Path::InvalidPathChars);
	if (!str) return OVUM_ERROR_NO_MEMORY;
	VM_PushString(thread, str);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(io_Path_getInvalidFileNameCharsString)
{
	String *str = GC_ConstructString(thread, Path::InvalidFileNameCharsCount, Path::InvalidFileNameChars);
	if (!str) return OVUM_ERROR_NO_MEMORY;
	VM_PushString(thread, str);
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(io_Path_isAbsolute)
{
	CHECKED(StringFromValue(thread, args));

	String *path = args[0].v.string;
	CHECKED(Path::ValidatePath(thread, path, false));

	VM_PushBool(thread, Path::IsAbsolute(path));
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(io_Path_join)
{
	Value *output = VM_Local(thread, 0);

	for (ovlocals_t i = 0; i < argc; i++)
	{
		CHECKED(StringFromValue(thread, args + i));

		String *path = args[i].v.string;
		CHECKED(Path::ValidatePath(thread, path, false));
		if (i == 0 || Path::IsAbsolute(path))
			SetString(thread, output, path);
		else
		{
			String *outputStr;
			ovchar_t lastChar = (&output->v.string->firstChar)[output->v.string->length - 1];
			if (Path::IsPathSep(lastChar)
#if OVUM_WINDOWS
				|| lastChar == Path::VolumeSeparator
#endif
				)
				outputStr = String_Concat(thread, output->v.string, path);
			else
				outputStr = String_Concat3(thread, output->v.string, Path::DirSeparatorString.AsString(), path);
			CHECKED_MEM(outputStr);
			SetString(thread, output, outputStr);
		}
	}

	VM_Push(thread, output);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(io_Path_getFullPath)
{
	String *path = args[0].v.string;
	CHECKED(Path::ValidatePath(thread, path, false));

	String *fullPath;
	CHECKED(Path::GetFullPath(thread, path, &fullPath));
	VM_PushString(thread, fullPath);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(io_Path_getFileName)
{
	String *path = args[0].v.string;
	CHECKED(Path::ValidatePath(thread, path, false));

	const ovchar_t *chp = &path->firstChar;
	for (size_t i = path->length; --i >= 0;)
	{
		ovchar_t ch = chp[i];
		if (Path::IsPathSep(ch) || ch == Path::VolumeSeparator)
		{
			CHECKED_MEM(path = GC_ConstructString(thread, path->length - i - 1, chp + i + 1));
			VM_PushString(thread, path);
			RETURN_SUCCESS;
		}
	}

	VM_PushString(thread, strings::Empty);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(io_Path_getDirectory)
{
	String *path = args[0].v.string;
	CHECKED(Path::ValidatePath(thread, path, false));

	size_t root = Path::GetRootLength(path);
	const ovchar_t *chp = &path->firstChar;

	size_t i = path->length;
	if (i > root)
	{
		while (i > root && !Path::IsPathSep(chp[--i]))
			;

		// i points to the path separator; use GC_ConstructString to make
		// a substring up to (but not including) the path separator
		CHECKED_MEM(path = GC_ConstructString(thread, i, chp));
	}

	VM_PushString(thread, path);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(io_Path_getExtension)
{
	String *path = args[0].v.string;
	CHECKED(Path::ValidatePath(thread, path, false));

	size_t extIdx = Path::GetExtensionIndex(path);
	// Note: extIdx points to the dot, not the first character of the extension
	if (extIdx == Path::NOT_FOUND || extIdx == path->length - 1)
		VM_PushNull(thread);
	else
	{
		extIdx++;
		String *ext;
		CHECKED_MEM(ext = GC_ConstructString(thread, path->length - extIdx, &path->firstChar + extIdx));
		VM_PushString(thread, ext);
	}
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(io_Path_hasExtension)
{
	CHECKED(Path::ValidatePath(thread, args[0].v.string, false));

	VM_PushBool(thread, Path::GetExtensionIndex(args[0].v.string) != Path::NOT_FOUND);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(io_Path_changeExtension)
{
	static LitString<1> dot = { 1, 0, StringFlags::STATIC, '.',0 };

	String *path = args[0].v.string;
	CHECKED(Path::ValidatePath(thread, path, false));

	if (!IS_NULL(args[1]))
		CHECKED(StringFromValue(thread, args + 1));

	size_t extIdx = Path::GetExtensionIndex(path);
	Value *retval = VM_Local(thread, 0);

	if (extIdx == Path::NOT_FOUND)
		SetString(thread, retval, path);
	else
	{
		String *str;
		CHECKED_MEM(str = GC_ConstructString(thread, extIdx, &path->firstChar));
		SetString(thread, retval, str);
	}

	if (!IS_NULL(args[1]))
	{
		String *retString;
		if (args[1].v.string->firstChar == '.')
			retString = String_Concat(thread, retval->v.string, args[1].v.string);
		else
			retString = String_Concat3(thread, retval->v.string, dot.AsString(), args[1].v.string);
		CHECKED_MEM(retString);
		SetString(thread, retval, retString);
	}

	VM_Push(thread, retval);
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(io_Path_validatePath)
{
	return Path::ValidatePath(thread, args[0].v.string, args[1].v.integer ? true : false);
}
