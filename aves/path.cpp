#include "io_path.h"
#include "aves_io.h"
#include "ov_string.h"
#include <memory>
#include <cassert>

LitString<1> Path::DirSeparatorString = { 1, 0, StringFlags::STATIC, Path::DirSeparator,0 };

const int Path::InvalidPathCharsCount = 36;
// This list is also duplicated in ValidatePath.
const uchar Path::InvalidPathChars[] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
	// These are based on the return value of System.IO.Path.GetInvalidPathChars() + MSDN.
	// My understanding is that Unix systems generally disallow the same characters in paths.
	'"', '<', '>', '|',
};

const int Path::InvalidFileNameCharsCount = 41;
const uchar Path::InvalidFileNameChars[] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
	// These are taken from System.IO.Path.GetInvalidFileNameChars() + MSDN.
	'"', '<', '>', '|', '/', '\\', '?', '*', ':',
};

int32_t Path::GetExtensionIndex(String *path)
{
	for (int32_t i = path->length; --i >= 0; )
	{
		uchar ch = (&path->firstChar)[i];

		if (ch == '.')
			return i;

		if (ch == Path::DirSeparator ||
			ch == Path::AltDirSeparator ||
			ch == Path::VolumeSeparator)
			break;
	}

	return -1;
}

bool Path::IsAbsolute(String *path)
{
	const uchar *chp = &path->firstChar;

	const int32_t length = path->length;
	// If the path begins with a directory separator, or (on Windows) volume name + ':',
	// the path is considered absolute.
	if (length >= 1 && (chp[0] == Path::DirSeparator || chp[0] == Path::AltDirSeparator)
#ifdef _WIN32
		|| length >= 2 && chp[1] == Path::VolumeSeparator
#endif
		)
		return true;

	return false;
}

String *Path::GetFullPath(ThreadHandle thread, String *path)
{
	String *fullPath = nullptr;

	DWORD bufferLength = max(MAX_PATH, path->length + 1);
	bool retry;
	do
	{
		retry = false;
		std::unique_ptr<WCHAR[]> buffer(new WCHAR[bufferLength]);
		DWORD r = GetFullPathNameW((LPCWSTR)&path->firstChar, bufferLength, buffer.get(), nullptr);

		if (r == 0)
			io::ThrowIOError(thread, GetLastError(), path);

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
			fullPath = GC_ConstructString(thread, (int32_t)r, (const uchar*)buffer.get());
		}
	} while (retry);

	return fullPath;
}

void Path::ValidatePath(ThreadHandle thread, String *path, bool checkWildcards)
{
	bool error = false;

	const uchar *chp = &path->firstChar;
	for (int32_t i = 0; i < path->length; i++)
	{
		const uchar ch = *chp++;

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
		GC_Construct(thread, Types::ArgumentError, 2, nullptr);
		VM_Throw(thread);
	}
}

AVES_API NATIVE_FUNCTION(io_Path_get_directorySeparator)
{
	String *str = GC_ConstructString(thread, 1, &Path::DirSeparator);
	VM_PushString(thread, str);
}

AVES_API NATIVE_FUNCTION(io_Path_get_directorySeparator2)
{
	String *str = GC_ConstructString(thread, 1, &Path::AltDirSeparator);
	VM_PushString(thread, str);
}

AVES_API NATIVE_FUNCTION(io_Path_getInvalidPathCharsString)
{
	String *str = GC_ConstructString(thread, Path::InvalidPathCharsCount, Path::InvalidPathChars);
	VM_PushString(thread, str);
}

AVES_API NATIVE_FUNCTION(io_Path_getInvalidFileNameCharsString)
{
	String *str = GC_ConstructString(thread, Path::InvalidFileNameCharsCount, Path::InvalidFileNameChars);
	VM_PushString(thread, str);
}

AVES_API NATIVE_FUNCTION(io_Path_isAbsolute)
{
	StringFromValue(thread, args);

	String *path = args[0].common.string;
	Path::ValidatePath(thread, path, false);

	VM_PushBool(thread, Path::IsAbsolute(path));
}

AVES_API NATIVE_FUNCTION(io_Path_join)
{
	Value *output = VM_Local(thread, 0);

	for (int i = 0; i < argc; i++)
	{
		StringFromValue(thread, args + i);

		String *path = args[i].common.string;
		Path::ValidatePath(thread, path, false);
		if (Path::IsAbsolute(path))
			SetString(output, path);
		else
		{
			uchar lastChar = (&output->common.string->firstChar)[output->common.string->length - 1];
			if (lastChar == Path::DirSeparator ||
				lastChar == Path::AltDirSeparator)
				SetString(output, String_Concat(thread, output->common.string, path));
			else
				SetString(output, String_Concat3(thread, output->common.string, _S(Path::DirSeparatorString), path));
		}
	}

	VM_Push(thread, *output);
}

AVES_API NATIVE_FUNCTION(io_Path_getFullPath)
{
	String *path = args[0].common.string;
	Path::ValidatePath(thread, path, false);

	String *fullPath = Path::GetFullPath(thread, path);
	assert(fullPath != nullptr);
	VM_PushString(thread, fullPath);
}

AVES_API NATIVE_FUNCTION(io_Path_getExtension)
{
	String *path = args[0].common.string;
	Path::ValidatePath(thread, path, false);

	int32_t extIdx = Path::GetExtensionIndex(path);
	// Note: extIdx points to the dot, not the first character of the extension
	if (extIdx == -1 || extIdx == path->length - 1)
		VM_PushNull(thread);
	else
	{
		String *ext = GC_ConstructString(thread, path->length - (extIdx + 1), &path->firstChar + extIdx + 1);
		VM_PushString(thread, ext);
	}
}

AVES_API NATIVE_FUNCTION(io_Path_hasExtension)
{
	Path::ValidatePath(thread, args[0].common.string, false);

	VM_PushBool(thread, Path::GetExtensionIndex(args[0].common.string) != -1);
}

AVES_API NATIVE_FUNCTION(io_Path_changeExtension)
{
	static LitString<1> dot = { 1, 0, StringFlags::STATIC, '.',0 };

	String *path = args[0].common.string;
	Path::ValidatePath(thread, path, false);

	if (!IS_NULL(args[1]))
		StringFromValue(thread, args + 1);

	int32_t extIdx = Path::GetExtensionIndex(path);
	Value *retval = VM_Local(thread, 0);

	if (extIdx == -1)
		SetString(retval, path);
	else
		SetString(retval, GC_ConstructString(thread, extIdx, &path->firstChar));

	if (!IS_NULL(args[1]))
	{
		if (args[1].common.string->firstChar == '.')
			SetString(retval, String_Concat(thread, retval->common.string, args[1].common.string));
		else
			SetString(retval, String_Concat3(thread, retval->common.string, _S(dot), args[1].common.string));
	}

	VM_Push(thread, *retval);
}

AVES_API NATIVE_FUNCTION(io_Path_validatePath)
{
	Path::ValidatePath(thread, args[0].common.string, args[1].integer ? true : false);
}