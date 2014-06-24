#pragma once

#ifndef AVES__WINDOWS_H
#define AVES__WINDOWS_H

#include <WinSDKVer.h>
#define _WIN32_WINNT _WIN32_WINNT_WIN7

#include <SDKDDKVer.h>

// Exclude rarely-used stuff from Windows headers
#define WIN32_LEAN_AND_MEAN

// Windows header files
#include <windows.h>

#endif // AVES__WINDOWS_H