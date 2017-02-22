#include "def.h"
#include "../../unicode/utf8encoder.h"

// Implementations of various functions from the Windows-specific header files.
// The sort of functions we want to discourage the compiler from inlining.

namespace ovum
{

namespace os
{

	FileStatus _FileStatusFromError(DWORD error)
	{
		switch (error)
		{
		case ERROR_HANDLE_EOF:
			return FILE_EOF;
		case ERROR_FILE_NOT_FOUND:
		case ERROR_PATH_NOT_FOUND:
			return FILE_NOT_FOUND;
		case ERROR_ACCESS_DENIED:
			return FILE_ACCESS_DENIED;
		case ERROR_FILE_EXISTS:
		case ERROR_ALREADY_EXISTS:
			return FILE_ALREADY_EXISTS;
		default:
			return FILE_IO_ERROR;
		}
	}

	FileStatus OpenMemoryMappedFile(const pathchar_t *name, FileMode mode, MmfAccess access, FileShare share, MemoryMappedFile *output)
	{
		// First, transform 'access' to an appropriate FileAccess value
		FileAccess fileAccess;
		switch (access)
		{
		case MMF_OPEN_READ:
		case MMF_OPEN_READ_EXEC:
			fileAccess = FILE_ACCESS_READ;
			break;
		case MMF_OPEN_WRITE:
		case MMF_OPEN_WRITE_EXEC:
		case MMF_OPEN_PRIVATE:
		case MMF_OPEN_PRIVATE_EXEC:
			fileAccess = FILE_ACCESS_READWRITE;
			break;
		}

		FileHandle file;
		FileStatus r = OpenFile(name, mode, fileAccess, share, &file);
		if (r != FILE_OK)
			return r; // Nope.

		// Create the file mapping without a maximum size
		HANDLE mapping = ::CreateFileMappingW(file, nullptr, (DWORD)access, 0, 0, nullptr);
		if (mapping == nullptr)
		{
			// Gotta clean up after ourselves. Ignore the return value; we're about
			// to return an error anyway.
			CloseHandle(file);
			return FILE_IO_ERROR; // Also nope.
		}

		output->file = file;
		output->mapping = mapping;
		return FILE_OK;
	}

	LibraryStatus _LibraryStatusFromError(DWORD error)
	{
		switch (error)
		{
		case ERROR_FILE_NOT_FOUND:
		case ERROR_PATH_NOT_FOUND:
		case ERROR_MOD_NOT_FOUND:
			// ERROR_MOD_NOT_FOUND appears to occur both when the library file
			// itself cannot be found /and/ when one of its dependencies could
			// not be loaded. We return LIBRARY_FILE_NOT_FOUND as a compromise.
			return LIBRARY_FILE_NOT_FOUND;
		case ERROR_DLL_NOT_FOUND:
			return LIBRARY_MISSING_DEPENDENCY;
		case ERROR_ACCESS_DENIED:
			return LIBRARY_ACCESS_DENIED;
		case ERROR_INVALID_DLL:
			// This error may also occur as a result of a broken dependency,
			// but this is the closest we'll get to a "broken DLL" error.
			// (Don't use ERROR_INVALID_LIBRARY. That's for library folders.)
			return LIBRARY_BAD_IMAGE;
		default:
			return LIBRARY_ERROR;
		}
	}

	bool ConsoleWriteFile(HANDLE handle, const ovchar_t *str, size_t length)
	{
		// Assume the console can handle UTF-8
		const size_t BUFFER_SIZE = 2048;
		char buffer[BUFFER_SIZE];
		Utf8Encoder encoder(buffer, BUFFER_SIZE, str, length);

		DWORD remaining;
		while ((remaining = (DWORD)encoder.GetNextBytes()) > 0)
		{
			char *bytes = buffer;
			do
			{
				DWORD written = 0;
				BOOL r = ::WriteFile(handle, bytes, remaining, &written, nullptr);
				if (!r)
					// Something went wrong. :( Can't really do anything about it.
					return false;

				remaining -= written;
				bytes += written;
			} while (remaining > 0);
		}

		return true;
	}

	bool ConsoleWrite_(HANDLE handle, const ovchar_t *str, size_t length)
	{
		DWORD remaining = (DWORD)length;
		do
		{
			DWORD written = 0;
			BOOL r = ::WriteConsoleW(handle, str, remaining, &written, nullptr);
			if (!r)
			{
				// WriteConsole will fail with a standard handle if it
				// has been redirected to a file. We can use WriteFile
				// in that case, so let's try that.
				return ConsoleWriteFile(handle, str, length);
			}

			remaining -= written;
			str += written;
		} while (remaining > 0);

		return true;
	}

} // namespace os

} // namespace ovum
