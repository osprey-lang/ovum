#include "file.h"
#include "io.h"
#include "path.h"
#include "../aves/buffer.h"
#include "../aves_state.h"
#include <memory>

using namespace aves;

int io::ReadFileAttributes(ThreadHandle thread, String *fileName, WIN32_FILE_ATTRIBUTE_DATA *data, bool throwOnError, bool &success)
{
	// Ovum and Win32 are both UTF-16, so we can just use the string value as-is.
	VM_EnterUnmanagedRegion(thread);

	BOOL r = GetFileAttributesExW((LPCWSTR)&fileName->firstChar,
		GetFileExInfoStandard, data);

	VM_LeaveUnmanagedRegion(thread);

	if (!r && throwOnError)
		return ThrowIOError(thread, GetLastError(), fileName);

	success = r ? true : false;
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(io_File_existsInternal)
{
	String *fileName = args[0].v.string;
	CHECKED(Path::ValidatePath(thread, fileName, false));

	WIN32_FILE_ATTRIBUTE_DATA data;

	bool result;
	{ Pinned fn(args + 0);
		CHECKED(io::ReadFileAttributes(thread, fileName, &data, false, result));
	}

	if (result)
		result = data.dwFileAttributes != -1 &&
			((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0);

	VM_PushBool(thread, result);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(io_File_getSizeInternal)
{
	String *fileName = args[0].v.string;
	CHECKED(Path::ValidatePath(thread, fileName, false));

	bool _;
	WIN32_FILE_ATTRIBUTE_DATA data;
	{ Pinned fn(args + 0);
		CHECKED(io::ReadFileAttributes(thread, fileName, &data, true, _));
	}

	VM_PushInt(thread, (int64_t)data.nFileSizeLow | ((int64_t)data.nFileSizeHigh << 32));
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(io_File_deleteInternal)
{
	String *fileName = args[0].v.string;
	CHECKED(Path::ValidatePath(thread, fileName, false));

	BOOL r;
	{ Pinned fn(args + 0);
		VM_EnterUnmanagedRegion(thread);

		r = DeleteFileW((LPCWSTR)&fileName->firstChar);

		VM_LeaveUnmanagedRegion(thread);
	}

	if (!r)
		return io::ThrowIOError(thread, GetLastError(), fileName);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(io_File_moveInternal)
{
	String *srcName = args[0].v.string;
	String *destName = args[1].v.string;

	CHECKED(Path::ValidatePath(thread, srcName, false));
	CHECKED(Path::ValidatePath(thread, destName, false));

	BOOL r;
	{ Pinned src(args + 0), dst(args + 1);
		VM_EnterUnmanagedRegion(thread);

		r = MoveFileW((LPCWSTR)&srcName->firstChar, (LPCWSTR)&destName->firstChar);

		VM_LeaveUnmanagedRegion(thread);
	}

	if (!r)
		return io::ThrowIOError(thread, GetLastError());
}
END_NATIVE_FUNCTION

// FileStream implementation

#define _FS(v) reinterpret_cast<FileStream*>((v).instance)

int FileStream::EnsureOpen(ThreadHandle thread)
{
	if (handle == NULL)
		return ErrorHandleClosed(thread);
	RETURN_SUCCESS;
}

int FileStream::ErrorHandleClosed(ThreadHandle thread)
{
	Aves *aves = Aves::Get(thread);

	VM_PushString(thread, error_strings::FileHandleClosed);
	return VM_ThrowErrorOfType(thread, aves->aves.InvalidStateError, 1);
}

AVES_API void io_FileStream_initType(TypeHandle type)
{
	Type_SetInstanceSize(type, (uint32_t)sizeof(FileStream));
	Type_SetFinalizer(type, io_FileStream_finalize);

	Type_AddNativeField(type, offsetof(FileStream, fileName), NativeFieldType::STRING);
}

AVES_API BEGIN_NATIVE_FUNCTION(io_FileStream_init)
{
	// init(fileName is String, mode is FileMode, access is FileAccess, share is FileShare)
	Aves *aves = Aves::Get(thread);

	String *fileName = args[1].v.string;
	CHECKED(Path::ValidatePath(thread, fileName, true));

	// Let's turn mode, access and share into appropriate arguments for CreateFile()
	// 'mode' corresponds to the dwCreationDisposition parameter.
	DWORD mode, access, share;

	switch (args[2].v.integer) // mode
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
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentRangeError, 1);
	}

	switch (args[3].v.integer) // access
	{
	case FileAccess::READ:       access = GENERIC_READ; break;
	case FileAccess::WRITE:      access = GENERIC_WRITE; break;
	case FileAccess::READ_WRITE: access = GENERIC_READ | GENERIC_WRITE; break;
	default:
		// io.FileAccess is an enum set, but only
		// the three combinations above are valid.
		VM_PushString(thread, strings::access);
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentRangeError, 1);
	}
	if ((FileMode)args[2].v.integer == FileMode::APPEND)
	{
		if (access != GENERIC_WRITE)
		{
			VM_PushString(thread, error_strings::AppendMustBeWriteOnly); // message
			VM_PushString(thread, strings::access); // paramName
			return VM_ThrowErrorOfType(thread, aves->aves.ArgumentError, 2);
		}
		// access is now updated to FILE_APPEND_DATA; mode remains the same.
		// It seems that no other access flags are needed for appending.
		access = FILE_APPEND_DATA;
	}

	if (args[4].v.uinteger > 7) // uinteger so that negative numbers are > 0
	{
		VM_PushString(thread, strings::share);
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentRangeError, 1);
	}
	// By a genuine coincidence, io.FileShare's values perfectly match those
	// used by the Windows API, so we can just assign the value as-is.
	// Great minds assign values alike, I guess!
	share = (DWORD)args[4].v.integer;

	HANDLE handle;
	{ Pinned fn(args + 1);
		VM_EnterUnmanagedRegion(thread);

		handle = CreateFileW((LPCWSTR)&fileName->firstChar,
			access, share, nullptr, mode,
			FILE_ATTRIBUTE_NORMAL, NULL);

		VM_LeaveUnmanagedRegion(thread);
	}

	if (handle == INVALID_HANDLE_VALUE)
		return io::ThrowIOError(thread, GetLastError(), fileName);
	// Verify that the handle refers to a file on disk.
	DWORD fileType = GetFileType(handle);
	if (fileType != FILE_TYPE_DISK)
	{
		VM_PushString(thread, error_strings::FileStreamWithNonFile);
		return VM_ThrowErrorOfType(thread, aves->aves.NotSupportedError, 1);
	}

	FileStream *stream = THISV.Get<FileStream>();
	stream->handle = handle;
	stream->access = (FileAccess)args[3].v.integer;
	stream->fileName = fileName;
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(io_FileStream_get_canRead)
{
	FileStream *stream = THISV.Get<FileStream>();
	if (stream->handle == NULL)
		VM_PushBool(thread, false); // The handle has been closed
	else
		VM_PushBool(thread, (stream->access & FileAccess::READ) == FileAccess::READ);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(io_FileStream_get_canWrite)
{
	FileStream *stream = THISV.Get<FileStream>();
	if (stream->handle == NULL)
		VM_PushBool(thread, false); // The handle has been closed
	else
		VM_PushBool(thread, (stream->access & FileAccess::WRITE) == FileAccess::WRITE);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(io_FileStream_get_canSeek)
{
	FileStream *stream = THISV.Get<FileStream>();
	if (stream->handle == NULL)
		VM_PushBool(thread, false); // The handle has been closed
	else
		// TODO: Figure out if there are any circumstances under which
		//       it is not possible to seek in a file.
		//       (Other than when the handle has been closed.)
		VM_PushBool(thread, true);
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(io_FileStream_get_length)
{
	FileStream *stream = THISV.Get<FileStream>();
	CHECKED(stream->EnsureOpen(thread));

	HANDLE handle = stream->handle;

	VM_EnterUnmanagedRegion(thread);

	LARGE_INTEGER size;
	BOOL r = GetFileSizeEx(handle, &size);

	VM_LeaveUnmanagedRegion(thread);

	if (!r)
		return io::ThrowIOError(thread, GetLastError());

	VM_PushInt(thread, size.QuadPart);
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(io_FileStream_get_fileName)
{
	FileStream *stream = THISV.Get<FileStream>();
	VM_PushString(thread, stream->fileName);
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(io_FileStream_readByte)
{
	FileStream *stream = THISV.Get<FileStream>();
	CHECKED(stream->EnsureOpen(thread));

	HANDLE handle = stream->handle;

	VM_EnterUnmanagedRegion(thread);

	uint8_t byte;
	DWORD bytesRead;
	BOOL r = ReadFile(handle, &byte, 1, &bytesRead, nullptr);

	VM_LeaveUnmanagedRegion(thread);

	if (!r)
		return io::ThrowIOError(thread, GetLastError());

	if (bytesRead == 0)
		VM_PushInt(thread, -1);
	else
		VM_PushInt(thread, byte);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(io_FileStream_readMaxInternal)
{
	// Args: (buf is Buffer, offset is Int, count is Int)
	// FileStream.readMax verifies that offset and count are
	// within the buffer, and that buf is actually a Buffer.
	FileStream *stream = THISV.Get<FileStream>();
	CHECKED(stream->EnsureOpen(thread));

	HANDLE handle = stream->handle;
	// The GC will never move the Buffer::bytes pointer
	uint8_t *buffer = reinterpret_cast<Buffer*>(args[1].v.instance)->bytes;
	buffer += (int32_t)args[2].v.integer;

	int32_t count = (int32_t)args[3].v.integer;

	VM_EnterUnmanagedRegion(thread);

	DWORD bytesRead;
	BOOL r = ReadFile(handle, buffer, count, &bytesRead, nullptr);

	VM_LeaveUnmanagedRegion(thread);

	if (!r)
		return io::ThrowIOError(thread, GetLastError());

	VM_PushInt(thread, bytesRead);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(io_FileStream_writeByte)
{
	FileStream *stream = THISV.Get<FileStream>();
	CHECKED(stream->EnsureOpen(thread));

	CHECKED(IntFromValue(thread, args + 1));

	HANDLE handle = stream->handle;

	VM_EnterUnmanagedRegion(thread);

	DWORD bytesWritten;
	uint8_t byte = (uint8_t)args[1].v.integer;
	BOOL r = WriteFile(handle, &byte, 1, &bytesWritten, nullptr);

	VM_LeaveUnmanagedRegion(thread);

	if (!r)
		return io::ThrowIOError(thread, GetLastError());
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(io_FileStream_writeInternal)
{
	// Args: (buf is Buffer, offset is Int, count is Int)
	// FileStream.write verifies that offset and count are
	// within the buffer, and that buf is actually a Buffer.
	FileStream *stream = THISV.Get<FileStream>();
	CHECKED(stream->EnsureOpen(thread));

	HANDLE handle = stream->handle;
	// The GC will never move the Buffer::bytes pointer,
	// no need to pin it
	uint8_t *buffer = reinterpret_cast<Buffer*>(args[1].v.instance)->bytes;
	buffer += (int32_t)args[2].v.integer;

	int32_t count = (int32_t)args[3].v.integer;

	VM_EnterUnmanagedRegion(thread);

	DWORD bytesWritten;
	BOOL r = WriteFile(handle, buffer, count, &bytesWritten, nullptr);

	VM_LeaveUnmanagedRegion(thread);

	if (!r)
		return io::ThrowIOError(thread, GetLastError());
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(io_FileStream_flush)
{
	Aves *aves = Aves::Get(thread);

	FileStream *stream = THISV.Get<FileStream>();
	CHECKED(stream->EnsureOpen(thread));

	if ((stream->access & FileAccess::WRITE) != FileAccess::WRITE)
	{
		VM_PushString(thread, error_strings::CannotFlushReadOnlyStream);
		return VM_ThrowErrorOfType(thread, aves->aves.InvalidStateError, 1);
	}

	HANDLE handle = stream->handle;

	VM_EnterUnmanagedRegion(thread);

	BOOL r = FlushFileBuffers(handle);

	VM_LeaveUnmanagedRegion(thread);

	if (!r)
		return io::ThrowIOError(thread, GetLastError());
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(io_FileStream_seekInternal)
{
	Aves *aves = Aves::Get(thread);

	// seekInternal(offset is Int, origin is SeekOrigin)
	FileStream *stream = THISV.Get<FileStream>();
	CHECKED(stream->EnsureOpen(thread));

	DWORD seekOrigin;
	switch (args[2].v.integer)
	{
	case SeekOrigin::START:   seekOrigin = FILE_BEGIN;   break;
	case SeekOrigin::CURRENT: seekOrigin = FILE_CURRENT; break;
	case SeekOrigin::END:     seekOrigin = FILE_END;     break;
	default:
		VM_PushString(thread, strings::origin);
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentRangeError, 1);
	}

	HANDLE handle = stream->handle;
	LARGE_INTEGER seekOffset;
	seekOffset.QuadPart = args[1].v.integer;

	VM_EnterUnmanagedRegion(thread);

	LARGE_INTEGER newOffset;
	BOOL r = SetFilePointerEx(handle, seekOffset, &newOffset, seekOrigin);

	VM_LeaveUnmanagedRegion(thread);

	if (!r)
		return io::ThrowIOError(thread, GetLastError());

	VM_PushInt(thread, newOffset.QuadPart);
}
END_NATIVE_FUNCTION

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
				return io::ThrowIOError(thread, GetLastError());
		}

		// Try to close the handle
		VM_EnterUnmanagedRegion(thread);
		r = CloseHandle(handle);
		VM_LeaveUnmanagedRegion(thread);

		if (!r)
			return io::ThrowIOError(thread, GetLastError());
		stream->handle = NULL;
	}

	RETURN_SUCCESS;
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
