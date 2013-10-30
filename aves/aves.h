#pragma once

#ifndef AVES__AVES_H
#define AVES__AVES_H

#ifndef AVES_EXPORTS
#pragma comment(lib, "aves.lib")
#endif

#ifdef AVES_EXPORTS
#define _AVES_API __declspec(dllexport)
#else
#define _AVES_API __declspec(dllimport)
#endif

#define AVES_API	extern "C" _AVES_API

#include "targetver.h"

#define WIN32_LEAN_AND_MEAN		// Exclude rarely-used stuff from Windows headers

// Windows header files
#include <windows.h>

// Ovum
#include "ov_vm.h"

// Project headers
#include "aves_ns.h"
#include "aves_int.h"
#include "aves_uint.h"
#include "aves_shared_strings.h"

#endif // AVES__AVES_H