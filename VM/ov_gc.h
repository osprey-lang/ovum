#pragma once

#ifndef VM__GC_H
#define VM__GC_H

#include "ov_vm.h"

// Constructs an instance of the specified type.
// The type must not be abstract, static, primitive or aves.String.
// 'argc' refers to the number of arguments that are on the call stack,
// which get passed to an appropriate constructor of the type.
OVUM_API void GC_Construct(ThreadHandle thread, TypeHandle type, const uint16_t argc, Value *output);

// Constructs a string. If 'values' is null, the string is initialized to contain only '\0's.
// Note: 'length' does NOT include the terminating '\0'!
OVUM_API String *GC_ConstructString(ThreadHandle thread, const int32_t length, const uchar *values);

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

OVUM_API Value *GC_AddStaticReference(Value initialValue);

OVUM_API void GC_Collect(ThreadHandle thread);

OVUM_API uint32_t GC_GetObjectHashCode(Value *value);

OVUM_API void GC_Pin(Value *value);

OVUM_API void GC_Unpin(Value *value);

class Pinned
{
private:
	Value *const value;

	inline Pinned(Pinned &other) : value(nullptr) { }
	inline Pinned(Pinned &&other) : value(nullptr) { }
	inline Pinned &operator=(Pinned &other) { return *this; }
	inline Pinned &operator=(Pinned &&other) { return *this; }

public:
	inline Pinned(Value *value)
		: value(value)
	{
		GC_Pin(value);
	}
	inline ~Pinned()
	{
		GC_Unpin(value);
	}

	inline Value *operator->() const
	{
		return value;
	}
	inline Value *operator*() const
	{
		return value;
	}
};

template<typename T>
class PinnedAlias
{
private:
	Value *const value;

	inline PinnedAlias(PinnedAlias &other) : value(nullptr) { }
	inline PinnedAlias(PinnedAlias &&other) : value(nullptr) { }
	inline PinnedAlias &operator=(PinnedAlias &other) { return *this; }
	inline PinnedAlias &operator=(PinnedAlias &&other) { return *this; }

public:
	inline PinnedAlias(Value *value)
		: value(value)
	{
		GC_Pin(value);
	}
	inline ~PinnedAlias()
	{
		GC_Unpin(value);
	}

	inline T *operator->() const
	{
		return reinterpret_cast<T*>(value->instance);
	}
	inline T *operator*() const
	{
		return reinterpret_cast<T*>(value->instance);
	}
};

#endif // VM__GC_H