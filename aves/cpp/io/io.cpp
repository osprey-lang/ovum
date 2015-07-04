#include "io.h"
#include "../aves_state.h"
#ifndef _WIN32
#include <errno.h>
#endif

using namespace aves;

namespace io_errors
{
	namespace
	{
		static LitString<33> _AccessDenied = LitString<33>::FromCString("Access to the resource is denied.");
		static LitString<30> _DiskFull     = LitString<30>::FromCString("Not enough free space on disk.");
		static LitString<27> _SeekFailed   = LitString<27>::FromCString("Could not seek in the file.");
	}

	static String *AccessDenied = _AccessDenied.AsString();
	static String *DiskFull     = _DiskFull.AsString();
	static String *SeekFailed   = _SeekFailed.AsString();
}

int io::ThrowIOError(ThreadHandle thread, ErrorCode code, String *pathName)
{
	Aves *aves = Aves::Get(thread);

	String *message = nullptr;

	int r = OVUM_SUCCESS;
#if OVUM_WINDOWS
	switch (code)
	{
	case ERROR_FILE_NOT_FOUND:
	case ERROR_PATH_NOT_FOUND:
		VM_PushString(thread, pathName);
		r = GC_Construct(thread, aves->io.FileNotFoundError, 1, nullptr);
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

	if (message == nullptr)
	{
		message = win32_helpers::GetSystemErrorMessage(thread, code);
		if (message == nullptr)
			return OVUM_ERROR_NO_MEMORY;
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
	r = GC_Construct(thread, aves->io.IOError, 1, nullptr);
throwError:
	if (r == OVUM_SUCCESS)
		r = VM_Throw(thread);
	return r;
}
