#ifndef AVES__WINDOWS_H
#define AVES__WINDOWS_H

// Required by helper functions
#include <ovum.h>

#include <WinSDKVer.h>
#define _WIN32_WINNT _WIN32_WINNT_WIN7

#include <SDKDDKVer.h>

// Exclude rarely-used stuff from Windows headers
#define WIN32_LEAN_AND_MEAN

// Windows header files
#include <windows.h>

// The ssize_t type is POSIX-specific, but it's also quite useful.
// Define it here.
typedef SSIZE_T ssize_t;

// Helper functions
namespace win32_helpers
{
	String *GetSystemErrorMessage(ThreadHandle thread, DWORD error);
	String *GetSystemHResultMessage(ThreadHandle thread, HRESULT hr);
}

#endif // AVES__WINDOWS_H
