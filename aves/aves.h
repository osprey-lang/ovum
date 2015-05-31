#pragma once

#ifndef AVES__AVES_H
#define AVES__AVES_H

#ifdef AVES_EXPORTS
#define _AVES_API __declspec(dllexport)
#else
#define _AVES_API __declspec(dllimport)
#pragma comment(lib, "aves.lib")
#endif

#define AVES_API	extern "C" _AVES_API

// Ovum
#include "ov_vm.h"

// Forward declarations

namespace aves
{
	class Aves;
}

// OS-specific headers

#if OVUM_WINDOWS
# include "windows/windows.h"
#else
# error aves does not support the target operating system
#endif

// Project headers
#include "aves_ns.h"
#include "aves_int.h"
#include "aves_uint.h"
#include "aves_shared_strings.h"

#endif // AVES__AVES_H