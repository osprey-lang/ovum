#pragma once

#ifndef VM__GC_H
#define VM__GC_H

#include "ov_vm.h"

// Constructs an instance of the specified type.
// The type must not be abstract, static, primitive or aves.String.
// 'argc' refers to the number of arguments that are on the call stack,
// which get passed to an appropriate constructor of the type.
OVUM_API void GC_Construct(ThreadHandle thread, TypeHandle type, const uint16_t argc, Value *output);

// Constructs a string. If 'values' is NULL, the string is initialized to contain only '\0's.
// Note: 'length' does NOT include the terminating '\0'!
OVUM_API void GC_ConstructString(ThreadHandle thread, const int32_t length, const uchar *values, String **result);

// Converts a C string to an Ovum string. The return value of this method should probably be cached
// in a managed field of some kind, to avoid allocating memory every time the string is needed.
// NOTE: Uses the standard-library strlen() to get the length of the string.
OVUM_API String *GC_ConvertString(ThreadHandle thread, const char *string);

// Informs the GC that a certain amount of unmanaged memory has been allocated,
// which helps the GC better schedule garbage collection.
// NOTE: Consumers of this method MUST take care to remove EXACTLY as much memory
// pressure as they add, or the GC will experience performance problems.
OVUM_API void GC_AddMemoryPressure(ThreadHandle thread, const size_t size);

// Informs the GC that a certain amount of unmanaged memory has been released,
// which helps the GC better schedule garbage collection.
// NOTE: Consumers of this method MUST take care to remove EXACTLY as much memory
// pressure as they add, or the GC will experience performance problems.
OVUM_API void GC_RemoveMemoryPressure(ThreadHandle thread, const size_t size);

#endif // VM__GC_H