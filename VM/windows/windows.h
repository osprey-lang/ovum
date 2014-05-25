#pragma once

#ifndef VM__WINDOWS_H
#define VM__WINDOWS_H

#include <WinSDKVer.h>
#define _WIN32_WINNT _WIN32_WINNT_WIN7

#include <SDKDDKVer.h>

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

// Windows header files
#include <windows.h>

#endif // VM__WINDOWS_H