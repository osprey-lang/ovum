#include "aves_hash.h"
#include <cstddef>

#define _H(value)           ((value).common.hash)
#define U64_TO_HASH(value)  ((int32_t)(value) ^ (int32_t)((value) >> 32))

AVES_API void aves_Hash_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(HashInst));
	Type_SetReferenceGetter(type, aves_Hash_getReferences);

	Type_AddNativeField(type, offsetof(HashInst, buckets), NativeFieldType::GC_ARRAY);
	Type_AddNativeField(type, offsetof(HashInst, entries), NativeFieldType::GC_ARRAY);
}

int InitializeBuckets(ThreadHandle thread, HashInst *inst, const int32_t capacity)
{
	int32_t size = HashHelper_GetPrime(capacity);

	int r = GC_AllocArrayT(thread, size, &inst->buckets);
	if (r != OVUM_SUCCESS) return r;
	memset(inst->buckets, -1, size * sizeof(int32_t));

	r = GC_AllocArrayT(thread, size, &inst->entries);
	if (r != OVUM_SUCCESS) return r;

	inst->capacity = size;
	inst->freeList = -1;
	RETURN_SUCCESS;
}

int ResizeHash(ThreadHandle thread, HashInst *inst)
{
	int32_t newSize = HashHelper_GetPrime(inst->count * 2);

	int32_t *newBuckets;
	int r = GC_AllocArray(thread, newSize, sizeof(int32_t), reinterpret_cast<void**>(&newBuckets));
	if (r != OVUM_SUCCESS) return r;
	memset(newBuckets, -1, newSize * sizeof(int32_t));

	HashEntry *newEntries;
	r = GC_AllocArray(thread, newSize, sizeof(HashEntry), reinterpret_cast<void**>(&newEntries));
	if (r != OVUM_SUCCESS) return r;
	CopyMemoryT(newEntries, inst->entries, inst->count);

	HashEntry *e = newEntries;
	for (int32_t i = 0; i < inst->count; i++, e++)
	{
		int32_t bucket = e->hashCode % newSize;
		e->next = newBuckets[bucket];
		newBuckets[bucket] = i;
	}

	inst->capacity = newSize;
	inst->buckets = newBuckets;
	inst->entries = newEntries;
	RETURN_SUCCESS;
}

int FindEntry(ThreadHandle thread, HashInst *inst, Value *key, const int32_t hashCode, int32_t &index)
{
	index = -1;

	if (inst->buckets != nullptr)
	{
		int32_t hashCode2 = hashCode & INT32_MAX;
		for (int32_t i = inst->buckets[hashCode2 % inst->capacity]; i >= 0; i = inst->entries[i].next)
		{
			HashEntry entry = inst->entries[i];
			if (entry.hashCode == hashCode2)
			{
				VM_Push(thread, *key);
				VM_Push(thread, entry.key);
				bool equals;
				int r = VM_Equals(thread, &equals);
				if (r != OVUM_SUCCESS) return r;
				if (equals)
				{
					index = i;
					break;
				}
			}
		}
	}

	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_Hash_get_length)
{
	HashInst *inst = _H(THISV);

	VM_PushInt(thread, inst->count - inst->freeCount);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_Hash_get_capacity)
{
	HashInst *inst = _H(THISV);

	VM_PushInt(thread, inst->capacity);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_Hash_get_version)
{
	HashInst *inst = _H(THISV);

	VM_PushInt(thread, inst->version);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_Hash_get_entryCount)
{
	HashInst *inst = _H(THISV);

	VM_PushInt(thread, inst->count);
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_Hash_initialize)
{
	// Args: (capacity: Int)
	HashInst *inst = _H(THISV);
	inst->freeList = -1;

	int64_t capacity = args[1].integer;
	if (capacity > 0)
	{
		Pinned h(THISP);
		CHECKED(InitializeBuckets(thread, inst, (int32_t)capacity));
	}
	RETURN_SUCCESS;
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_Hash_getItemInternal)
{
	// Args: (key: non-null, hash: Int|UInt)
	{ Pinned h(THISP);
		HashInst *inst = _H(THISV);

		int32_t hashCode = U64_TO_HASH(args[2].uinteger);
		int32_t index;
		CHECKED(FindEntry(thread, inst, args + 1, hashCode, index));
		if (index >= 0)
		{
			VM_Push(thread, inst->entries[index].value);
			RETURN_SUCCESS;
		}
	}

	// ArgumentError(message, paramName)
	VM_PushString(thread, error_strings::HashKeyNotFound);
	VM_PushString(thread, strings::key);
	CHECKED(GC_Construct(thread, Types::ArgumentError, 2, nullptr));
	return VM_Throw(thread);
}
END_NATIVE_FUNCTION
AVES_API NATIVE_FUNCTION(aves_Hash_getEntry)
{
	HashInst *inst = _H(THISV);

	int32_t index = (int32_t)args[1].integer;

	HashEntry *entryPointer = inst->entries + index;
	if (entryPointer->hashCode >= 0)
	{
		Value entry;
		entry.type = Types::HashEntry;
		entry.instance = reinterpret_cast<uint8_t*>(entryPointer);

		VM_Push(thread, entry); // yay
	}
	else
		VM_PushNull(thread);
	RETURN_SUCCESS;
}
AVES_API BEGIN_NATIVE_FUNCTION(aves_Hash_insert)
{
	// Args: (key: non-null, hash: Int|UInt, value, add: Boolean)
	Pinned h(THISP);
	HashInst *inst = _H(THISV);

	bool add = !!args[4].integer;

	if (inst->buckets == nullptr)
		CHECKED(InitializeBuckets(thread, inst, 0));

	int32_t hashCode = U64_TO_HASH(args[2].uinteger) & INT32_MAX;
	int32_t bucket = hashCode % inst->capacity;

	for (int32_t i = inst->buckets[bucket]; i >= 0; )
	{
		HashEntry *entry = inst->entries + i;
		if (entry->hashCode == hashCode)
		{
			VM_Push(thread, args[1]); // key
			VM_Push(thread, entry->key);
			bool equals;
			CHECKED(VM_Equals(thread, &equals));
			if (equals)
			{
				if (add)
				{
					CHECKED(GC_Construct(thread, Types::DuplicateKeyError, 0, nullptr));
					return VM_Throw(thread);
				}

				entry->value = args[3];
				inst->version++;
				RETURN_SUCCESS; // Done!
			}
		}
		i = entry->next;
	}

	// The key is not in the hash table, let's add it!
	int32_t index;
	if (inst->freeCount > 0)
	{
		index = inst->freeList;
		inst->freeList = inst->entries[index].next;
		inst->freeCount--;
	}
	else
	{
		if (inst->count == inst->capacity)
		{
			CHECKED(ResizeHash(thread, inst));
			bucket = hashCode % inst->capacity;
		}
		index = inst->count;
		inst->count++;
	}

	{
		HashEntry *entry = inst->entries + index;
		entry->hashCode = hashCode;
		entry->next     = inst->buckets[bucket];
		entry->key      = args[1];
		entry->value    = args[3];
		inst->buckets[bucket] = index;
	}
	inst->version++;
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_Hash_hasKeyInternal)
{
	// Args: (key: non-null, hash: Int|UInt)
	int32_t hashCode = U64_TO_HASH(args[2].uinteger);
	int32_t index;
	{ Pinned h(THISP);
		CHECKED(FindEntry(thread, _H(THISV), args + 1, hashCode, index));
	}

	VM_PushBool(thread, index >= 0);
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_Hash_hasValue)
{
	// Args: (value)
	Alias<HashInst> inst(THISP);

	for (int32_t i = 0; i < inst->count; i++)
		if (inst->entries[i].hashCode >= 0)
		{
			VM_Push(thread, args[1]); // value
			VM_Push(thread, inst->entries[i].value);
			bool equals;
			CHECKED(VM_Equals(thread, &equals));
			if (equals)
			{
				VM_PushBool(thread, true);
				RETURN_SUCCESS;
			}
		}

	VM_PushBool(thread, false);
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_Hash_tryGetInternal)
{
	// tryGetInternal(key: non-null, hash: Int|UInt, ref value)
	int32_t hashCode = U64_TO_HASH(args[2].uinteger);
	int32_t index;
	{ Pinned h(THISP);
		HashInst *hash = _H(THISV);
		CHECKED(FindEntry(thread, hash, args + 1, hashCode, index));

		if (index >= 0)
		{
			HashEntry *entry = hash->entries + index;
			WriteReference(args + 3, &entry->value);
		}
	}

	VM_PushBool(thread, index >= 0);
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_Hash_removeInternal)
{
	// Args: (key: non-null, hash: Int|UInt)
	HashInst *inst = _H(THISV);

	if (inst->buckets != nullptr)
	{
		Pinned h(THISP);
		int32_t hashCode = U64_TO_HASH(args[2].uinteger);
		int32_t bucket = hashCode % inst->capacity;
		int32_t lastEntry = -1;

		for (int32_t i = inst->buckets[bucket]; i >= 0; i = inst->entries[i].next)
		{
			HashEntry *entry = inst->entries + i;
			if (entry->hashCode == hashCode)
			{
				VM_Push(thread, args[1]);
				VM_Push(thread, entry->key);
				bool equals;
				CHECKED(VM_Equals(thread, &equals));
				if (equals)
				{
					// Key found!
					if (lastEntry < 0)
						inst->buckets[bucket] = entry->next;
					else
						inst->entries[lastEntry].next = entry->next;

					entry->hashCode = -1;
					entry->next = inst->freeList;
					entry->key.type = nullptr;
					entry->value.type = nullptr;
					inst->freeList = i;
					inst->freeCount++;
					inst->version++;
					VM_PushBool(thread, true);
					RETURN_SUCCESS;
				}
			}
			lastEntry = i;
		}
	}

	VM_PushBool(thread, false);
}
END_NATIVE_FUNCTION
AVES_API NATIVE_FUNCTION(aves_Hash_pinEntries)
{
	HashInst *hash = _H(THISV);
	GC_PinInst(hash->entries);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_Hash_unpinEntries)
{
	HashInst *hash = _H(THISV);
	GC_UnpinInst(hash->entries);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_HashEntry_get_hashCode)
{
	HashEntry *e = reinterpret_cast<HashEntry*>(THISV.instance);
	VM_PushInt(thread, e->hashCode);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_HashEntry_get_nextIndex)
{
	HashEntry *e = reinterpret_cast<HashEntry*>(THISV.instance);
	VM_PushInt(thread, e->next);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_HashEntry_get_key)
{
	HashEntry *e = reinterpret_cast<HashEntry*>(THISV.instance);
	VM_Push(thread, e->key);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_HashEntry_get_value)
{
	HashEntry *e = reinterpret_cast<HashEntry*>(THISV.instance);
	VM_Push(thread, e->value);
	RETURN_SUCCESS;
}

AVES_API int InitHashInstance(ThreadHandle thread, HashInst *hash, const int32_t capacity)
{
	if (capacity > 0)
	{
		PinnedAlias<HashInst> p(hash);

		int r = InitializeBuckets(thread, hash, capacity);
		if (r != OVUM_SUCCESS) return r;
	}
	RETURN_SUCCESS;
}

bool aves_Hash_getReferences(void *basePtr, unsigned int *valc, Value **target, int32_t *state)
{
	HashInst *hash = (HashInst*)basePtr;
	int32_t i = *state;
	while (i < hash->count)
	{
		if (hash->entries[i].hashCode != -1)
		{
			*valc = 2;
			*target = &hash->entries[i].key;
			*state = i + 1;
			return true;
		}
		i++;
	}
	return false;
}