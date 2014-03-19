#include "aves_hash.h"
#include <memory>

#define _H(value)           ((value).common.hash)
#define U64_TO_HASH(value)  ((int32_t)(value) ^ (int32_t)((value) >> 32))

AVES_API void aves_Hash_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(HashInst));
	Type_SetReferenceGetter(type, aves_Hash_getReferences);
	Type_SetFinalizer(type, aves_Hash_finalize);
}

void InitializeBuckets(ThreadHandle thread, HashInst *inst, const int32_t capacity)
{
	int32_t size = HashHelper_GetPrime(capacity);

	inst->buckets = new int32_t[size];
	memset(inst->buckets, -1, size * sizeof(int32_t));

	inst->entries = new HashEntry[size];
	memset(inst->entries, 0, size * sizeof(HashEntry));

	inst->capacity = size;
	inst->freeList = -1;
}

void ResizeHash(ThreadHandle thread, HashInst *inst)
{
	using namespace std;

	int32_t newSize = HashHelper_GetPrime(inst->count * 2);

	unique_ptr<int32_t[]> newBuckets(new int32_t[newSize]);
	memset(newBuckets.get(), -1, newSize * sizeof(int32_t));

	unique_ptr<HashEntry[]> newEntries(new HashEntry[newSize]);
	CopyMemoryT(newEntries.get(), inst->entries, inst->count);

	HashEntry *e = newEntries.get();
	for (int32_t i = 0; i < inst->count; i++, e++)
	{
		int32_t bucket = e->hashCode % newSize;
		e->next = newBuckets[bucket];
		newBuckets[bucket] = i;
	}

	delete[] inst->buckets;
	delete[] inst->entries;

	inst->capacity = newSize;
	inst->buckets = newBuckets.release();
	inst->entries = newEntries.release();
}

int32_t FindEntry(ThreadHandle thread, HashInst *inst, Value *key, const int32_t hashCode)
{
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
				if (VM_Equals(thread))
					return i;
			}
		}
	}

	return -1; // not found
}

AVES_API NATIVE_FUNCTION(aves_Hash_get_length)
{
	HashInst *inst = _H(THISV);

	VM_PushInt(thread, inst->count - inst->freeCount);
}
AVES_API NATIVE_FUNCTION(aves_Hash_get_capacity)
{
	HashInst *inst = _H(THISV);

	VM_PushInt(thread, inst->capacity);
}
AVES_API NATIVE_FUNCTION(aves_Hash_get_version)
{
	HashInst *inst = _H(THISV);

	VM_PushInt(thread, inst->version);
}
AVES_API NATIVE_FUNCTION(aves_Hash_get_entryCount)
{
	HashInst *inst = _H(THISV);

	VM_PushInt(thread, inst->count);
}

AVES_API NATIVE_FUNCTION(aves_Hash_initialize)
{
	// Args: (capacity: Int)
	HashInst *inst = _H(THISV);
	inst->freeList = -1;

	int64_t capacity = args[1].integer;
	if (capacity > 0)
		InitializeBuckets(thread, inst, (int32_t)capacity);
}
AVES_API NATIVE_FUNCTION(aves_Hash_getItemInternal)
{
	// Args: (key: non-null, hash: Int|UInt)
	HashInst *inst = _H(THISV);

	int32_t hashCode = U64_TO_HASH(args[2].uinteger);
	int32_t index = FindEntry(thread, inst, args + 1, hashCode);
	if (index >= 0)
	{
		VM_Push(thread, inst->entries[index].value);
		return;
	}

	// TODO: Key Not Found error
	// ArgumentError(message, paramName)
	VM_PushString(thread, error_strings::HashKeyNotFound);
	VM_PushString(thread, strings::key);
	GC_Construct(thread, Types::ArgumentError, 2, nullptr);
	VM_Throw(thread);
}
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
}
AVES_API NATIVE_FUNCTION(aves_Hash_insert)
{
	// Args: (key: non-null, hash: Int|UInt, value, add: Boolean)
	HashInst *inst = _H(THISV);

	bool add = !!args[4].integer;

	if (inst->buckets == nullptr)
		InitializeBuckets(thread, inst, 0);

	int32_t hashCode = U64_TO_HASH(args[2].uinteger) & INT32_MAX;
	int32_t bucket = hashCode % inst->capacity;

	for (int32_t i = inst->buckets[bucket]; i >= 0; i = inst->entries[i].next)
	{
		HashEntry *entry = inst->entries + i;
		if (entry->hashCode == hashCode)
		{
			VM_Push(thread, args[1]); // key
			VM_Push(thread, entry->key);
			if (VM_Equals(thread))
			{
				if (add)
				{
					GC_Construct(thread, Types::DuplicateKeyError, 0, nullptr);
					VM_Throw(thread);
				}

				entry->value = args[3];
				inst->version++;
				return; // Done!
			}
		}
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
			ResizeHash(thread, inst);
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
AVES_API NATIVE_FUNCTION(aves_Hash_hasKeyInternal)
{
	// Args: (key: non-null, hash: Int|UInt)
	int32_t hashCode = U64_TO_HASH(args[2].uinteger);
	int32_t index = FindEntry(thread, _H(THISV), args + 1, hashCode);

	VM_PushBool(thread, index >= 0);
}
AVES_API NATIVE_FUNCTION(aves_Hash_hasValue)
{
	// Args: (value)
	HashInst *inst = _H(THISV);

	for (int32_t i = 0; i < inst->count; i++)
		if (inst->entries[i].hashCode >= 0)
		{
			VM_Push(thread, args[1]); // value
			VM_Push(thread, inst->entries[i].value);
			if (VM_Equals(thread))
			{
				VM_PushBool(thread, true);
				return;
			}
		}

	VM_PushBool(thread, false);
}
AVES_API NATIVE_FUNCTION(aves_Hash_removeInternal)
{
	// Args: (key: non-null, hash: Int|UInt)

	HashInst *inst = _H(THISV);

	if (inst->buckets != nullptr)
	{
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
				if (VM_Equals(thread))
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
					return;
				}
			}
			lastEntry = i;
		}
	}

	VM_PushBool(thread, false);
}

AVES_API NATIVE_FUNCTION(aves_HashEntry_get_hashCode)
{
	HashEntry *e = reinterpret_cast<HashEntry*>(THISV.instance);
	VM_PushInt(thread, e->hashCode);
}
AVES_API NATIVE_FUNCTION(aves_HashEntry_get_nextIndex)
{
	HashEntry *e = reinterpret_cast<HashEntry*>(THISV.instance);
	VM_PushInt(thread, e->next);
}
AVES_API NATIVE_FUNCTION(aves_HashEntry_get_key)
{
	HashEntry *e = reinterpret_cast<HashEntry*>(THISV.instance);
	VM_Push(thread, e->key);
}
AVES_API NATIVE_FUNCTION(aves_HashEntry_get_value)
{
	HashEntry *e = reinterpret_cast<HashEntry*>(THISV.instance);
	VM_Push(thread, e->value);
}

AVES_API void InitHashInstance(ThreadHandle thread, HashInst *hash, const int32_t capacity)
{
	if (capacity > 0)
		InitializeBuckets(thread, hash, capacity);
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

void aves_Hash_finalize(void *basePtr)
{
	HashInst *hash = reinterpret_cast<HashInst*>(basePtr);

	delete[] hash->buckets;
	delete[] hash->entries;
	hash->capacity  = 0;
	hash->count     = 0;
	hash->freeCount = 0;
	hash->freeList  = -1;
	hash->version   = -1;
}