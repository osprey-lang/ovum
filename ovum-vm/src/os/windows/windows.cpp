#include "def.h"

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
			return FILE_IO_ERROR; // Also nope.

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

} // namespace os

} // namespace ovum