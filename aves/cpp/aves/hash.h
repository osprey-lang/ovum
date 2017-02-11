#ifndef AVES__HASH_H
#define AVES__HASH_H

#include "../aves.h"

namespace aves
{

struct HashEntry
{
	// Lower 31 bits of hash code. If the bucket used to contain a value that
	// has since been removed, contains REMOVED.
	int32_t hashCode;
	// Index of next entry in bucket. If this is the last entry in the bucket,
	// has the value Hash::LAST.
	size_t next;
	Value key;
	Value value;

	inline bool IsRemoved() const
	{
		return hashCode == REMOVED;
	}

	// When the hash code of an entry is set to this value, indicates that
	// it used to contain a value that has since been removed.
	static const int32_t REMOVED = -1;
};

class Hash
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
	// The index of the first freed entry. If there is non, has the value LAST.
	size_t freeList;
	// The "version" of the hash, incremented whenever changes are made.
	int32_t version;

	// Indexes into entries.
	size_t *buckets;
	// The actual values stored in the hash.
	HashEntry *entries;

	Value keyComparer;

	inline size_t GetLength()
	{
		return count - freeCount;
	}

	int InitializeBuckets(ThreadHandle thread, size_t capacity);

	int Resize(ThreadHandle thread);

	int FindEntry(ThreadHandle thread, Value *key, int32_t hashCode, size_t &index);

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

AVES_API int OVUM_CDECL InitHashInstance(ThreadHandle thread, size_t capacity, Value *result);

AVES_API int OVUM_CDECL ConcatenateHashes(ThreadHandle thread, Value *a, Value *b, Value *result);

int OVUM_CDECL aves_Hash_walkReferences(void *basePtr, ReferenceVisitor callback, void *cbState);

AVES_API NATIVE_FUNCTION(aves_HashEntry_get_hashCode);
AVES_API NATIVE_FUNCTION(aves_HashEntry_get_nextIndex);
AVES_API NATIVE_FUNCTION(aves_HashEntry_get_key);
AVES_API NATIVE_FUNCTION(aves_HashEntry_get_value);

#endif // AVES__HASH_H
