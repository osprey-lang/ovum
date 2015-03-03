#pragma once

#ifndef VM__WINDOWS_H
#define VM__WINDOWS_H

#include <WinSDKVer.h>
#define _WIN32_WINNT _WIN32_WINNT_WIN7

#include <SDKDDKVer.h>

// Exclude rarely-used stuff from Windows headers
#define WIN32_LEAN_AND_MEAN

// Windows header files
#include <windows.h>

// Some Windows macros that mess with the Ovum source.
#undef LoadString
#undef Yield
#undef FILE_SHARE_READ
#undef FILE_SHARE_WRITE
#undef FILE_SHARE_DELETE

#include "windows/def.h"

#endif // VM__WINDOWS_H