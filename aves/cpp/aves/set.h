#ifndef AVES__SET_H
#define AVES__SET_H

#include "../aves.h"

namespace aves
{

struct SetEntry
{
	int32_t hashCode;
	int32_t next;
	Value value;
};

class Set
{
public:
	int32_t capacity;   // the number of "slots" in buckets, entries and values
	int32_t count;      // the number of entries (not buckets) that have been used
	int32_t freeCount;  // the number of entries that were previously used, and have now been freed (and can thus be reused)
	int32_t freeList;   // the index of the first freed entry
	int32_t version;    // the "version" of the hash, incremented whenever changes are made

	int32_t *buckets;   // indexes into entries
	SetEntry *entries;  // entries!

	Value itemComparer;

	int InitializeBuckets(ThreadHandle thread, int32_t capacity);

	int Resize(ThreadHandle thread);

	int ItemEquals(ThreadHandle thread, Value *a, Value *b, bool &equals);

	static inline int32_t GetHash(uint64_t value)
	{
		return ((int32_t)value ^ (int32_t)(value >> 32)) & INT32_MAX;
	}
};

} // namespace aves

AVES_API void OVUM_CDECL aves_Set_init(TypeHandle type);


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

int OVUM_CDECL aves_Set_getReferences(void *basePtr, ReferenceVisitor callback, void *cbState);

#endif // AVES__SET_H
