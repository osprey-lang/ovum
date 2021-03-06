#pragma once

#include "../vm.h"

// This file contains a platform-independent API implementation
// for dealing with thread-local storage.

namespace ovum
{

// Implements a thread-local storage entry. The thread-local storage slot
// contains a pointer to T. The constructor for TlsEntry does not attempt
// to allocate a TLS slot; that is done by the Alloc method.
template<typename T>
class TlsEntry
{
public:
	inline TlsEntry() :
		inited(false)
	{ }

	// Determines whether the TLS key is valid, that is,
	// whether it's been allocated for the calling process.
	inline bool IsValid() const
	{
		return inited;
	}

	// Attempts to allocate storage for this TLS key. Returns
	// true if successful; otherwise, false.
	inline bool Alloc()
	{
		if (IsValid())
			return true;

		if (os::TlsAlloc(&key))
		{
			inited = true;
			return true;
		}
		return false;
	}

	// Frees the storage for this TLS key. The value stored
	// in the key is NOT destructed in any way.
	inline void Free()
	{
		if (IsValid())
			os::TlsFree(&key);
	}

	// Gets the value stored in this TLS key on the currently
	// executing thread. If the key is not valid (IsValid()
	// returns false), then null is always returned.
	inline T *const Get()
	{
		if (!IsValid())
			return nullptr;
		return reinterpret_cast<T*>(os::TlsGet(&key));
	}

	// Sets the value of this TLS key on the current thread.
	inline void Set(T *const value)
	{
		if (IsValid())
			os::TlsSet(&key, value);
	}

private:
	bool inited;
	os::TlsKey key;

	OVUM_DISABLE_COPY_AND_ASSIGN(TlsEntry);
};

} // namespace ovum
