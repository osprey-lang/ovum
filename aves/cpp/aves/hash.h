#ifndef AVES__HASH_H
#define AVES__HASH_H

#include "../aves.h"

namespace aves
{

struct HashEntry
{
	int32_t hashCode; // Lower 31 bits of hash code; -1 = unused
	int32_t next;     // Index of next entry in bucket; -1 = last
	Value key;
	Value value;
};

class Hash
{
public:
	int32_t capacity;   // the number of "slots" in buckets and entries
	int32_t count;      // the number of entries (not buckets) that have been used
	int32_t freeCount;  // the number of entries that were previously used, and have now been freed (and can thus be reused)
	int32_t freeList;   // the index of the first freed entry
	int32_t version;    // the "version" of the hash, incremented whenever changes are made

	int32_t *buckets;
	HashEntry *entries;

	Value keyComparer;

	inline int32_t GetLength()
	{
		return count - freeCount;
	}

	int InitializeBuckets(ThreadHandle thread, int32_t capacity);

	int Resize(ThreadHandle thread);

	int FindEntry(ThreadHandle thread, Value *key, int32_t hashCode, int32_t &index);

	int KeyEquals(ThreadHandle thread, Value *a, Value *b, bool &equals);

	// Merges this hash table's entries into the value on top of the evaluation stack,
	// which must also be a hash table. If this hash table shares keys with the other,
	// their values will be overwritten, as if assigned to with an indexer.
	//
	// Upon returning, this method makes sure the other hash table remains on top of
	// the evaluation stack.
	int MergeIntoTopOfStack(ThreadHandle thread);

	static inline int32_t GetHash(uint64_t value)
	{
		return ((int32_t)value ^ (int32_t)(value >> 32)) & INT32_MAX;
	}
};

} // namespace aves

AVES_API int OVUM_CDECL aves_Hash_init(TypeHandle type);

AVES_API NATIVE_FUNCTION(aves_Hash_get_length);
AVES_API NATIVE_FUNCTION(aves_Hash_get_capacity);
AVES_API NATIVE_FUNCTION(aves_Hash_get_keyComparer);
AVES_API NATIVE_FUNCTION(aves_Hash_get_version);
AVES_API NATIVE_FUNCTION(aves_Hash_get_entryCount);
AVES_API NATIVE_FUNCTION(aves_Hash_get_maxCapacity);

AVES_API NATIVE_FUNCTION(aves_Hash_initialize);
AVES_API NATIVE_FUNCTION(aves_Hash_getItemInternal);
AVES_API NATIVE_FUNCTION(aves_Hash_getEntry);
AVES_API NATIVE_FUNCTION(aves_Hash_insert);
AVES_API NATIVE_FUNCTION(aves_Hash_containsKeyInternal);
AVES_API NATIVE_FUNCTION(aves_Hash_containsValue);
AVES_API NATIVE_FUNCTION(aves_Hash_tryGetInternal);
AVES_API NATIVE_FUNCTION(aves_Hash_removeInternal);
AVES_API NATIVE_FUNCTION(aves_Hash_concatInternal);
AVES_API NATIVE_FUNCTION(aves_Hash_pinEntries);
AVES_API NATIVE_FUNCTION(aves_Hash_unpinEntries);

AVES_API int OVUM_CDECL InitHashInstance(ThreadHandle thread, int32_t capacity, Value *result);

AVES_API int OVUM_CDECL ConcatenateHashes(ThreadHandle thread, Value *a, Value *b, Value *result);

int OVUM_CDECL aves_Hash_getReferences(void *basePtr, ReferenceVisitor callback, void *cbState);

AVES_API NATIVE_FUNCTION(aves_HashEntry_get_hashCode);
AVES_API NATIVE_FUNCTION(aves_HashEntry_get_nextIndex);
AVES_API NATIVE_FUNCTION(aves_HashEntry_get_key);
AVES_API NATIVE_FUNCTION(aves_HashEntry_get_value);

#endif // AVES__HASH_H
