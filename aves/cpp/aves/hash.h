#ifndef AVES__HASH_H
#define AVES__HASH_H

#include "../aves.h"

namespace aves
{

class Hash : public ::HashInst
{
public:
	Value keyComparer;

	int InitializeBuckets(ThreadHandle thread, int32_t capacity);

	int Resize(ThreadHandle thread);

	int FindEntry(ThreadHandle thread, Value *key, int32_t hashCode, int32_t &index);

	int KeyEquals(ThreadHandle thread, Value *a, Value *b, bool &equals);

	static inline int32_t GetHash(uint64_t value)
	{
		return ((int32_t)value ^ (int32_t)(value >> 32)) & INT32_MAX;
	}
};

} // namespace aves

AVES_API void OVUM_CDECL aves_Hash_init(TypeHandle type);

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
AVES_API NATIVE_FUNCTION(aves_Hash_hasKeyInternal);
AVES_API NATIVE_FUNCTION(aves_Hash_hasValue);
AVES_API NATIVE_FUNCTION(aves_Hash_tryGetInternal);
AVES_API NATIVE_FUNCTION(aves_Hash_removeInternal);
AVES_API NATIVE_FUNCTION(aves_Hash_pinEntries);
AVES_API NATIVE_FUNCTION(aves_Hash_unpinEntries);

AVES_API int OVUM_CDECL InitHashInstance(ThreadHandle thread, HashInst *hash, const int32_t capacity);

int OVUM_CDECL aves_Hash_getReferences(void *basePtr, ReferenceVisitor callback, void *cbState);

AVES_API NATIVE_FUNCTION(aves_HashEntry_get_hashCode);
AVES_API NATIVE_FUNCTION(aves_HashEntry_get_nextIndex);
AVES_API NATIVE_FUNCTION(aves_HashEntry_get_key);
AVES_API NATIVE_FUNCTION(aves_HashEntry_get_value);

#endif // AVES__HASH_H
