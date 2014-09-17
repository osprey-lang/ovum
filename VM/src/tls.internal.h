#pragma once

// This file contains a platform-independent API implementation
// for dealing with thread-local storage.

#ifndef VM__TLS_INTERNAL_H
#define VM__TLS_INTERNAL_H

#include "../inc/ov_vm.h"

#if OVUM_TARGET == OVUM_WINDOWS
#include "../windows/windows.h"

typedef DWORD TlsKey;
#else
#error Not supported
#endif

namespace ovum
{

// Non-template implementation class for TlsEntry<T>.
// This is required because:
//   1. We want TlsEntry to be a template class (generally safer);
//   2. Template classes must be defined inline;
//   3. We need the implementation to be in a source file.
// This class uses the same API as TlsEntry<T>, except that the
// Get and Set methods operate on void*.
// You should generally not use this class in your code.
class _TlsEntry
{
private:
	bool inited;
	TlsKey key;

	DISABLE_COPY_AND_ASSIGN(_TlsEntry);

public:
	_TlsEntry();

	inline bool IsValid() const
	{
		return inited;
	}
	bool Alloc();
	void Free();
	void *const Get();
	void Set(void *const value);
};

template<typename T>
class TlsEntry
{
private:
	_TlsEntry entry;

public:
	inline TlsEntry()
		: entry()
	{ }

	// Determines whether the TLS key is valid, that is,
	// whether it's been allocated for the calling process.
	inline bool IsValid() const
	{
		return entry.IsValid();
	}

	// Attempts to allocate storage for this TLS key. Returns
	// true if successful; otherwise, false.
	inline bool Alloc()
	{
		return entry.Alloc();
	}

	// Frees the storage for this TLS key. The value stored
	// in the key is NOT destructed in any way.
	inline void Free()
	{
		entry.Free();
	}

	// Gets the value stored in this TLS key on the currently
	// executing thread. If the key is not valid (IsValid()
	// returns false), then null is always returned.
	inline T *const Get()
	{
		return reinterpret_cast<T*>(entry.Get());
	}

	// Sets the value of this TLS key on the current thread.
	inline void Set(T *const value)
	{
		entry.Set(value);
	}
};

} // namespace ovum

#endif