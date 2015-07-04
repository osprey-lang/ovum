#include "../../aves.h"

namespace win32_helpers
{
	String *GetSystemErrorMessage(ThreadHandle thread, DWORD error)
	{
		return GetSystemHResultMessage(thread, HRESULT_FROM_WIN32(error));
	}

	String *GetSystemHResultMessage(ThreadHandle thread, HRESULT hr)
	{
		LPWSTR errorMessage = nullptr;

		FormatMessageW(
			FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL,
			hr,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPWSTR)&errorMessage,
			0,
			nullptr);

		String *result = nullptr;
		if (errorMessage != nullptr)
		{
			result = GC_ConstructString(thread, wcslen(errorMessage), errorMessage);
			LocalFree(errorMessage);
		}

		// If null, something went wrong!
		return result;
	}
}
