#ifndef AVES__SET_H
#define AVES__SET_H

#include "../aves.h"

namespace aves
{

struct SetEntry
{
	// Lower 31 bits of hash code. If the bucket used to contain a value that
	// has since been removed, contains REMOVED.
	int32_t hashCode;
	// Index of next entry in bucket. If this is the last entry in the bucket,
	// has the value Set::LAST.
	size_t next;
	Value value;

	inline bool IsRemoved() const
	{
		return hashCode == REMOVED;
	}

	// When the hash code of an entry is set to this value, indicates that
	// it used to contain a value that has since been removed.
	static const int32_t REMOVED = -1;
};

class Set
{
public:
	static const size_t LAST = (size_t)-1;

	// The number of "slots" in buckets and entries.
	size_t capacity;
	// The number of entries (not buckets) that have been used.
	size_t count;
	// The number of entries that were previously used, and have now been freed
	// (and can thus be reused).
	size_t freeCount;
	// The index of the first freed entry. If the free list is empty, has the
	// value LAST.
	size_t freeList;
	// The "version" of the set, incremented whenever changes are made.
	int32_t version;

	// Indexes into entries.
	size_t *buckets;
	// The actual values stored in the set.
	SetEntry *entries;

	Value itemComparer;

	int InitializeBuckets(ThreadHandle thread, size_t capacity);

	int Resize(ThreadHandle thread);

	int ItemEquals(ThreadHandle thread, Value *a, Value *b, bool &equals);

	static inline int32_t GetHash(uint64_t value)
	{
		return ((int32_t)value ^ (int32_t)(value >> 32)) & INT32_MAX;
	}
};

} // namespace aves

AVES_API int OVUM_CDECL aves_Set_init(TypeHandle type);

AVES_API NATIVE_FUNCTION(aves_Set_get_length);
AVES_API NATIVE_FUNCTION(aves_Set_get_capacity);
AVES_API NATIVE_FUNCTION(aves_Set_get_itemComparer);
AVES_API NATIVE_FUNCTION(aves_Set_get_version);
AVES_API NATIVE_FUNCTION(aves_Set_get_entryCount);
AVES_API NATIVE_FUNCTION(aves_Set_get_maxCapacity);

AVES_API NATIVE_FUNCTION(aves_Set_initialize);
AVES_API NATIVE_FUNCTION(aves_Set_containsInternal);
AVES_API NATIVE_FUNCTION(aves_Set_addInternal);
AVES_API NATIVE_FUNCTION(aves_Set_removeInternal);
AVES_API NATIVE_FUNCTION(aves_Set_clear);
AVES_API NATIVE_FUNCTION(aves_Set_hasEntryAt);
AVES_API NATIVE_FUNCTION(aves_Set_getEntryAt);

int OVUM_CDECL aves_Set_walkReferences(void *basePtr, ReferenceVisitor callback, void *cbState);

#endif // AVES__SET_H
