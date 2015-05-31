#pragma once

#ifndef AVES__WINDOWS_H
#define AVES__WINDOWS_H

// Required by helper functions
#include <ov_vm.h>

#include <WinSDKVer.h>
#define _WIN32_WINNT _WIN32_WINNT_WIN7

#include <SDKDDKVer.h>

// Exclude rarely-used stuff from Windows headers
#define WIN32_LEAN_AND_MEAN

// Windows header files
#include <windows.h>

// Helper functions
namespace win32_helpers
{
	String *GetSystemErrorMessage(ThreadHandle thread, DWORD error);
	String *GetSystemHResultMessage(ThreadHandle thread, HRESULT hr);
}

#endif // AVES__WINDOWS_H