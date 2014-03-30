#include "io_file.h"
#include "aves_io.h"
#include "io_path.h"
#include "aves_buffer.h"
#include <memory>

bool io::ReadFileAttributes(ThreadHandle thread, String *fileName, WIN32_FILE_ATTRIBUTE_DATA *data, bool throwOnError)
{
	// Ovum and Win32 are both UTF-16, so we can just use the string value as-is.
	VM_EnterUnmanagedRegion(thread);

	BOOL r = GetFileAttributesExW((LPCWSTR)&fileName->firstChar,
		GetFileExInfoStandard, data);

	VM_LeaveUnmanagedRegion(thread);

	if (!r && throwOnError)
		ThrowIOError(thread, GetLastError(), fileName);

	return (bool)r;
}

AVES_API NATIVE_FUNCTION(io_File_existsInternal)
{
	String *fileName = args[0].common.string;
	Path::ValidatePath(thread, fileName, false);

	WIN32_FILE_ATTRIBUTE_DATA data;

	bool r;
	{ Pinned fn(args + 0);
		r = io::ReadFileAttributes(thread, fileName, &data, false);
	}

	if (r)
		r = data.dwFileAttributes != -1 &&
			((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0);

	VM_PushBool(thread, r);
}

AVES_API NATIVE_FUNCTION(io_File_getSizeInternal)
{
	String *fileName = args[0].common.string;
	Path::ValidatePath(thread, fileName, false);

	WIN32_FILE_ATTRIBUTE_DATA data;
	{ Pinned fn(args + 0);
		io::ReadFileAttributes(thread, fileName, &data, true);
	}

	VM_PushInt(thread, (int64_t)data.nFileSizeLow | ((int64_t)data.nFileSizeHigh << 32));
}

AVES_API NATIVE_FUNCTION(io_File_deleteInternal)
{
	String *fileName = args[0].common.string;
	Path::ValidatePath(thread, fileName, false);

	BOOL r;
	{ Pinned fn(args + 0);
		VM_EnterUnmanagedRegion(thread);

		r = DeleteFileW((LPCWSTR)&fileName->firstChar);

		VM_LeaveUnmanagedRegion(thread);
	}

	if (!r)
		io::ThrowIOError(thread, GetLastError(), fileName);
}

AVES_API NATIVE_FUNCTION(io_File_moveInternal)
{
	String *srcName = args[0].common.string;
	String *destName = args[1].common.string;

	Path::ValidatePath(thread, srcName, false);
	Path::ValidatePath(thread, destName, false);

	BOOL r;
	{ Pinned src(args + 0), dst(args + 1);
		VM_EnterUnmanagedRegion(thread);

		r = MoveFileW((LPCWSTR)&srcName->firstChar, (LPCWSTR)&destName->firstChar);

		VM_LeaveUnmanagedRegion(thread);
	}

	if (!r)
		io::ThrowIOError(thread, GetLastError());
}

// FileStream implementation

#define _FS(v) reinterpret_cast<FileStream*>((v).instance)

void FileStream::EnsureOpen(ThreadHandle thread)
{
	if (handle == NULL)
		ErrorHandleClosed(thread);
}

void FileStream::ErrorHandleClosed(ThreadHandle thread)
{
	VM_PushString(thread, error_strings::FileHandleClosed);
	GC_Construct(thread, Types::InvalidStateError, 1, nullptr);
	VM_Throw(thread);
}

AVES_API void io_FileStream_initType(TypeHandle type)
{
	Type_SetInstanceSize(type, (uint32_t)sizeof(FileStream));
	Type_SetFinalizer(type, io_FileStream_finalize);
}

AVES_API NATIVE_FUNCTION(io_FileStream_init)
{
	// Args: (fileName is String, mode is FileMode, access is FileAccess, share is FileShare)

	String *fileName = args[1].common.string;
	Path::ValidatePath(thread, fileName, true);

	// Let's turn mode, access and share into appropriate arguments for CreateFile()
	// 'mode' corresponds to the dwCreationDisposition parameter.
	DWORD mode, access, share;

	switch (args[2].integer) // mode
	{
	case FileMode::OPEN:           mode = OPEN_EXISTING;     break;
	case FileMode::OPEN_OR_CREATE: mode = OPEN_ALWAYS;       break;
	case FileMode::CREATE:         mode = CREATE_ALWAYS;     break;
	case FileMode::CREATE_NEW_:    mode = CREATE_NEW;        break;
	case FileMode::TRUNCATE:       mode = TRUNCATE_EXISTING; break;
	// Additional processing is done later for append
	case FileMode::APPEND:         mode = OPEN_ALWAYS;       break;
	default:
		VM_PushString(thread, strings::mode);
		GC_Construct(thread, Types::ArgumentRangeError, 1, nullptr);
		VM_Throw(thread);
		break;
	}

	switch (args[3].integer) // access
	{
	case FileAccess::READ:       access = GENERIC_READ; break;
	case FileAccess::WRITE:      access = GENERIC_WRITE; break;
	case FileAccess::READ_WRITE: access = GENERIC_READ | GENERIC_WRITE; break;
	default:
		// io.FileAccess is an enum set, but only
		// the three combinations above are valid.
		VM_PushString(thread, strings::access);
		GC_Construct(thread, Types::ArgumentRangeError, 1, nullptr);
		VM_Throw(thread);
		break;
	}
	if ((FileMode)args[2].integer == FileMode::APPEND)
	{
		if (access != GENERIC_WRITE)
		{
			VM_PushString(thread, error_strings::AppendMustBeWriteOnly); // message
			VM_PushString(thread, strings::access); // paramName
			GC_Construct(thread, Types::ArgumentError, 2, nullptr);
			VM_Throw(thread);
		}
		// access is now updated to FILE_APPEND_DATA; mode remains the same.
		// It seems that no other access flags for appending.
		access = FILE_APPEND_DATA;
	}

	if (args[4].uinteger > 7) // uinteger so that negative numbers are > 0
	{
		VM_PushString(thread, strings::share);
		GC_Construct(thread, Types::ArgumentRangeError, 1, nullptr);
		VM_Throw(thread);
	}
	// By a genuine coincidence, io.FileShare's values perfectly match those
	// used by the Windows API, so we can just assign the value as-is.
	// Great minds assign values alike, I guess!
	share = (DWORD)args[4].integer;

	HANDLE handle;
	{ Pinned fn(args + 1);
		VM_EnterUnmanagedRegion(thread);

		handle = CreateFileW((LPCWSTR)&fileName->firstChar,
			access, share, nullptr, mode,
			FILE_ATTRIBUTE_NORMAL, NULL);

		VM_LeaveUnmanagedRegion(thread);
	}

	if (handle == INVALID_HANDLE_VALUE)
		io::ThrowIOError(thread, GetLastError(), fileName);

	FileStream *stream = _FS(THISV);
	stream->handle = handle;
	stream->access = (FileAccess)args[3].integer;
}

AVES_API NATIVE_FUNCTION(io_FileStream_get_canRead)
{
	FileStream *stream = _FS(THISV);
	if (stream->handle == NULL)
		VM_PushBool(thread, false); // The handle has been closed
	else
		VM_PushBool(thread, (stream->access & FileAccess::READ) == FileAccess::READ);
}
AVES_API NATIVE_FUNCTION(io_FileStream_get_canWrite)
{
	FileStream *stream = _FS(THISV);
	if (stream->handle == NULL)
		VM_PushBool(thread, false); // The handle has been closed
	else
		VM_PushBool(thread, (stream->access & FileAccess::WRITE) == FileAccess::WRITE);
}
AVES_API NATIVE_FUNCTION(io_FileStream_get_canSeek)
{
	FileStream *stream = _FS(THISV);
	if (stream->handle == NULL)
		VM_PushBool(thread, false); // The handle has been closed
	else
		// TODO: Figure out if there are any circumstances under which
		//       it is not possible to seek in a file.
		//       (Other than when the handle has been closed.)
		VM_PushBool(thread, true);
}

AVES_API NATIVE_FUNCTION(io_FileStream_get_length)
{
	FileStream *stream = _FS(THISV);
	stream->EnsureOpen(thread);

	HANDLE handle = stream->handle;

	VM_EnterUnmanagedRegion(thread);

	LARGE_INTEGER size;
	BOOL r = GetFileSizeEx(handle, &size);

	VM_LeaveUnmanagedRegion(thread);

	if (!r)
		io::ThrowIOError(thread, GetLastError());

	VM_PushInt(thread, size.QuadPart);
}

AVES_API NATIVE_FUNCTION(io_FileStream_readByte)
{
	FileStream *stream = _FS(THISV);
	stream->EnsureOpen(thread);

	HANDLE handle = stream->handle;

	VM_EnterUnmanagedRegion(thread);

	uint8_t byte;
	DWORD bytesRead;
	BOOL r = ReadFile(handle, &byte, 1, &bytesRead, nullptr);

	VM_LeaveUnmanagedRegion(thread);

	if (!r)
		io::ThrowIOError(thread, GetLastError());

	if (bytesRead == 0)
		VM_PushInt(thread, -1);
	else
		VM_PushInt(thread, byte);
}
AVES_API NATIVE_FUNCTION(io_FileStream_readMaxInternal)
{
	// Args: (buf is Buffer, offset is Int, count is Int)
	// FileStream.readMax verifies that offset and count are
	// within the buffer, and that buf is actually a Buffer.
	FileStream *stream = _FS(THISV);
	stream->EnsureOpen(thread);

	HANDLE handle = stream->handle;
	// The GC will never move the Buffer::bytes pointer
	uint8_t *buffer = reinterpret_cast<Buffer*>(args[1].instance)->bytes;
	buffer += (int32_t)args[2].integer;

	int32_t count = (int32_t)args[3].integer;

	VM_EnterUnmanagedRegion(thread);

	DWORD bytesRead;
	BOOL r = ReadFile(handle, buffer, count, &bytesRead, nullptr);

	VM_LeaveUnmanagedRegion(thread);

	if (!r)
		io::ThrowIOError(thread, GetLastError());

	VM_PushInt(thread, bytesRead);
}

AVES_API NATIVE_FUNCTION(io_FileStream_writeByte)
{
	FileStream *stream = _FS(THISV);
	stream->EnsureOpen(thread);

	IntFromValue(thread, args + 1);

	HANDLE handle = stream->handle;

	VM_EnterUnmanagedRegion(thread);

	DWORD bytesWritten;
	uint8_t byte = (uint8_t)args[1].integer;
	BOOL r = WriteFile(handle, &byte, 1, &bytesWritten, nullptr);

	VM_LeaveUnmanagedRegion(thread);

	if (!r)
		io::ThrowIOError(thread, GetLastError());
}

AVES_API NATIVE_FUNCTION(io_FileStream_writeInternal)
{
	// Args: (buf is Buffer, offset is Int, count is Int)
	// FileStream.write verifies that offset and count are
	// within the buffer, and that buf is actually a Buffer.
	FileStream *stream = _FS(THISV);
	stream->EnsureOpen(thread);

	HANDLE handle = stream->handle;
	// The GC will never move the Buffer::bytes pointer,
	// no need to pin it
	uint8_t *buffer = reinterpret_cast<Buffer*>(args[1].instance)->bytes;
	buffer += (int32_t)args[2].integer;

	int32_t count = (int32_t)args[3].integer;

	VM_EnterUnmanagedRegion(thread);

	DWORD bytesWritten;
	BOOL r = WriteFile(handle, buffer, count, &bytesWritten, nullptr);

	VM_LeaveUnmanagedRegion(thread);

	if (!r)
		io::ThrowIOError(thread, GetLastError());
}

AVES_API NATIVE_FUNCTION(io_FileStream_flush)
{
	FileStream *stream = _FS(THISV);
	stream->EnsureOpen(thread);

	if ((stream->access & FileAccess::WRITE) != FileAccess::WRITE)
	{
		VM_PushString(thread, error_strings::CannotFlushReadOnlyStream);
		GC_Construct(thread, Types::InvalidStateError, 1, nullptr);
		VM_Throw(thread);
	}

	HANDLE handle = stream->handle;

	VM_EnterUnmanagedRegion(thread);

	BOOL r = FlushFileBuffers(handle);

	VM_LeaveUnmanagedRegion(thread);

	if (!r)
		io::ThrowIOError(thread, GetLastError());
}

AVES_API NATIVE_FUNCTION(io_FileStream_seekInternal)
{
	// Args: (offset is Int, origin is SeekOrigin)
	FileStream *stream = _FS(THISV);
	stream->EnsureOpen(thread);

	DWORD seekOrigin;
	switch (args[2].integer)
	{
	case SeekOrigin::START:   seekOrigin = FILE_BEGIN;   break;
	case SeekOrigin::CURRENT: seekOrigin = FILE_CURRENT; break;
	case SeekOrigin::END:     seekOrigin = FILE_END;     break;
	default:
		VM_PushString(thread, strings::origin);
		GC_Construct(thread, Types::ArgumentRangeError, 1, nullptr);
		VM_Throw(thread);
		break;
	}

	HANDLE handle = stream->handle;
	LARGE_INTEGER seekOffset;
	seekOffset.QuadPart = args[1].integer;

	VM_EnterUnmanagedRegion(thread);

	LARGE_INTEGER newOffset;
	BOOL r = SetFilePointerEx(handle, seekOffset, &newOffset, seekOrigin);

	VM_LeaveUnmanagedRegion(thread);

	if (!r)
		io::ThrowIOError(thread, GetLastError());

	VM_PushInt(thread, newOffset.QuadPart);
}

AVES_API NATIVE_FUNCTION(io_FileStream_close)
{
	PinnedAlias<FileStream> stream(THISP);

	// Note: it's safe to call FileStream.close() multiple times

	if (stream->handle != NULL)
	{
		HANDLE handle = stream->handle;
		BOOL r;

		if ((stream->access & FileAccess::WRITE) == FileAccess::WRITE)
		{
			// Flush any pending buffers
			VM_EnterUnmanagedRegion(thread);
			r = FlushFileBuffers(handle);
			VM_LeaveUnmanagedRegion(thread);

			if (!r)
				io::ThrowIOError(thread, GetLastError());
		}

		// Try to close the handle
		VM_EnterUnmanagedRegion(thread);
		r = CloseHandle(handle);
		VM_LeaveUnmanagedRegion(thread);

		if (!r)
			io::ThrowIOError(thread, GetLastError());
		stream->handle = NULL;
	}
}

void io_FileStream_finalize(void *basePtr)
{
	FileStream *stream = reinterpret_cast<FileStream*>(basePtr);

	// Close the file handle if it's still open.
	// It's not safe to flush the buffer here, so we don't do it.
	// If you abandon a FileStream, you have no one to blame but yourself!
	if (stream->handle != NULL)
	{
		CloseHandle(stream->handle); // Ignore errors; can't do anything with them anyway
		stream->handle = NULL;
	}
}