#ifndef OVUM__GC_H
#define OVUM__GC_H

#include "ovum.h"

// Constructs an instance of the specified type. The type's constructor is called with the
// specified number of arguments from the stack.
//
// Parameters:
//   type:
//     The type to construct an instance of. This type cannot be abstract,
//     static, primitive, or aves.String.
//   argc:
//     The number of arguments to pass to the constructor. These arguments
//     must be pushed onto the thread's evaluation stack.
//   output:
//     A pointer that receives the constructed value. If null, the value is
//     pushed onto the stack instead.
OVUM_API int GC_Construct(ThreadHandle thread, TypeHandle type, ovlocals_t argc, Value *output);

// Constructs a string. If 'values' is null, the string is initialized to contain only '\0's.
//
// Returns null if the string could not be constructed.
//
// NOTE: 'length' does NOT include the terminating '\0'.
OVUM_API String *GC_ConstructString(ThreadHandle thread, size_t length, const ovchar_t *values);

// Allocates a managed value of the specified type and size. Unlike GC_Construct, this
// function does NOT call the constructor on the allocated value. Moreover, the size of
// the instance must be specified. This function should only be called by custom native
// allocators of variable-size types.
//
// The resulting value is not initialized in any way, other than having all its bytes
// zeroed. It is entirely the caller's responsibility to initialize the value correctly.
//
// The actual number of bytes allocated may differ from the 'size' parameter, partly
// because the GC adds a small header to all objects, partly because of alignment. The
// GC only guarantees that at least the requested number of bytes are allocated.
//
// Parameters:
//   type:
//     The type to allocate an instance of. When collecting garbage, the GC will examine
//     the data in the returned instance according to the fields in this type.
//   size:
//     The number of bytes to allocate. If the allocation succeeds, at least this number
//     of bytes are guaranteed to be available at the value's instance pointer. Note
//     that if the base type of 'type' has a non-zero size, it must be included in this
//     argument as well; this parameter contains the total size to be allocated.
//   output:
//     Receives the allocated value. This pointer cannot be null.
OVUM_API int GC_Alloc(ThreadHandle thread, TypeHandle type, size_t size, Value *output);

// Allocates a non-resizable GC-managed array of arbitrary values. This value should be put
// in a native field of type NativeFieldType::GC_ARRAY, to ensure that the value is reachable
// by the GC while the owning instance is alive. See Type_AddNativeField for more details.
//
// Note that this function checks for overflows and throws an aves.OverflowError (returning
// OVUM_ERROR_THROWN) if length * itemSize is larger than SIZE_MAX.
//
// Parameters:
//   thread:
//     The current thread.
//   length:
//     The number of items the array will contain.
//   itemSize:
//     The size, in bytes, of each item.
//   output:
//     A pointer that receives the resulting array.
OVUM_API int GC_AllocArray(ThreadHandle thread, size_t length, size_t itemSize, void **output);

template<typename T>
int GC_AllocArrayT(ThreadHandle thread, size_t length, T **output)
{
	return GC_AllocArray(thread, length, sizeof(T), reinterpret_cast<void**>(output));
}

// Allocates a non-resizable GC-managed array of Value instances. See GC_AllocArray for more
// details on how the return value should be handled.
//
// Parameters:
//   thread:
//     The current thread.
//   length:
//     The number of items the array will contain.
//   output:
//     A pointer that receives the resulting array.
OVUM_API int GC_AllocValueArray(ThreadHandle thread, size_t length, Value **output);

// Informs the GC that a certain amount of unmanaged memory has been allocated,
// which helps the GC better schedule garbage collection.
// NOTE: Consumers of this method MUST take care to remove EXACTLY as much memory
// pressure as they add, or the GC will experience performance problems.
OVUM_API void GC_AddMemoryPressure(ThreadHandle thread, size_t size);

// Informs the GC that a certain amount of unmanaged memory has been released,
// which helps the GC better schedule garbage collection.
// NOTE: Consumers of this method MUST take care to remove EXACTLY as much memory
// pressure as they add, or the GC will experience performance problems.
OVUM_API void GC_RemoveMemoryPressure(ThreadHandle thread, size_t size);

OVUM_API Value *GC_AddStaticReference(ThreadHandle thread, Value *initialValue);

// Forces an immediate garbage collection.
OVUM_API void GC_Collect(ThreadHandle thread);

// Gets the number of times garbage collection has occurred.
OVUM_API uint32_t GC_GetCollectCount(ThreadHandle thread);

OVUM_API int GC_GetGeneration(Value *value);

OVUM_API uint32_t GC_GetObjectHashCode(Value *value);

OVUM_API void GC_Pin(Value *value);
OVUM_API void GC_PinInst(void *value);

OVUM_API void GC_Unpin(Value *value);
OVUM_API void GC_UnpinInst(void *value);

class Pinned
{
private:
	Value *const value;

	inline Pinned(Pinned &other) : value(nullptr) { }
	inline Pinned &operator=(Pinned &other) { return *this; }

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
	void *const instance;

	inline PinnedAlias(PinnedAlias &other) : value(nullptr) { }
	inline PinnedAlias &operator=(PinnedAlias &other) { return *this; }

public:
	inline PinnedAlias(Value *value)
		: instance(value->v.instance)
	{
		GC_Pin(value);
	}
	inline PinnedAlias(T *instance)
		: instance(instance)
	{
		GC_PinInst(instance);
	}
	inline ~PinnedAlias()
	{
		GC_UnpinInst(instance);
	}

	inline T *operator->() const
	{
		return reinterpret_cast<T*>(instance);
	}
	inline T *operator*() const
	{
		return reinterpret_cast<T*>(instance);
	}
};

template<class T>
class PinnedArray
{
private:
	T *const value;

	inline PinnedArray(PinnedArray<T> &other) : value(nullptr) { }
	inline PinnedArray<T> &operator=(PinnedArray<T> &other) { return *this; }

public:
	inline PinnedArray(T *value)
		: value(value)
	{
		GC_PinInst(value);
	}
	inline ~PinnedArray()
	{
		GC_UnpinInst(value);
	}

	inline T *operator->() const
	{
		return value;
	}
	inline T *operator*() const
	{
		return value;
	}

	inline T &operator[](int index)                const { return value[index]; }
	inline T &operator[](long index)               const { return value[index]; }
	inline T &operator[](long long index)          const { return value[index]; }
	inline T &operator[](unsigned int index)       const { return value[index]; }
	inline T &operator[](unsigned long index)      const { return value[index]; }
	inline T &operator[](unsigned long long index) const { return value[index]; }
};

#endif // OVUM__GC_H
