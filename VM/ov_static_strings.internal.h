#pragma once

#ifndef VM__STATIC_STRINGS_INTERNAL_H
#define VM__STATIC_STRINGS_INTERNAL_H

#include "ov_vm.internal.h"

// Contains various string values that are used by various parts of the VM.
// These are all initialised (in static_strings.cpp) from LitString<>s, and
// as such have the STR_STATIC flag set.

namespace static_strings
{
	extern String *empty;   // ""

	extern String *_new;    // ".new"
	extern String *_iter;   // ".iter"
	extern String *_call;   // ".call"
	extern String *_init;	// ".init"

	extern String *toString; // "toString"

	namespace errors
	{
		extern String *ToStringWrongType;
	}
}

#endif // VM__STATIC_STRINGS_INTERNAL_H