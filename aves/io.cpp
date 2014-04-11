#include "aves_io.h"
#ifndef _WIN32
#include <errno.h>
#endif

namespace io_errors
{
	namespace
	{
		static LitString<33> _AccessDenied = LitString<33>::FromCString("Access to the resource is denied.");
		static LitString<30> _DiskFull     = LitString<30>::FromCString("Not enough free space on disk.");
		static LitString<27> _SeekFailed   = LitString<27>::FromCString("Could not seek in the file.");
	}

	static String *AccessDenied = _S(_AccessDenied);
	static String *DiskFull     = _S(_DiskFull);
	static String *SeekFailed   = _S(_SeekFailed);
}

int io::ThrowIOError(ThreadHandle thread, ErrorCode code, String *pathName)
{
	String *message = nullptr;

	int r = OVUM_SUCCESS;
#ifdef _WIN32
	switch (code)
	{
	case ERROR_FILE_NOT_FOUND:
	case ERROR_PATH_NOT_FOUND:
		VM_PushString(thread, pathName);
		r = GC_Construct(thread, Types::FileNotFoundError, 1, nullptr);
		goto throwError;
	case ERROR_ACCESS_DENIED:
		message = io_errors::AccessDenied;
		break;
	case ERROR_SEEK:
		message = io_errors::SeekFailed;
		break;
	case ERROR_DISK_FULL:
	case ERROR_DISK_QUOTA_EXCEEDED:
		message = io_errors::DiskFull;
		break;
	}
#else
	// TODO: POSIX error codes
	switch (code)
	{
	}
#endif

	if (message == nullptr)
		VM_PushNull(thread);
	else
		VM_PushString(thread, message);
	r = GC_Construct(thread, Types::IOError, 1, nullptr);
throwError:
	if (r == OVUM_SUCCESS)
		r = VM_Throw(thread);
	return r;
}