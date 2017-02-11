#include "hash.h"
#include "../aves_state.h"
#include <cstddef>

using namespace aves;

namespace aves
{

int Hash::InitializeBuckets(ThreadHandle thread, size_t capacity)
{
	size_t size = HashHelper_GetPrime(capacity);

	int r = GC_AllocArrayT(thread, size, &this->buckets);
	if (r != OVUM_SUCCESS) return r;
	memset(this->buckets, -1, size * sizeof(size_t));

	r = GC_AllocArrayT(thread, size, &this->entries);
	if (r != OVUM_SUCCESS) return r;

	this->capacity = size;
	this->freeList = LAST;
	RETURN_SUCCESS;
}

int Hash::Resize(ThreadHandle thread)
{
	size_t newSize = HashHelper_GetPrime(this->count * 2);

	size_t *newBuckets;
	int r = GC_AllocArray(thread, newSize, sizeof(size_t), reinterpret_cast<void**>(&newBuckets));
	if (r != OVUM_SUCCESS) return r;
	memset(newBuckets, -1, newSize * sizeof(size_t));

	HashEntry *newEntries;
	r = GC_AllocArray(thread, newSize, sizeof(HashEntry), reinterpret_cast<void**>(&newEntries));
	if (r != OVUM_SUCCESS) return r;
	CopyMemoryT(newEntries, this->entries, this->count);

	HashEntry *e = newEntries;
	for (size_t i = 0; i < this->count; i++, e++)
	{
		size_t bucket = e->hashCode % newSize;
		e->next = newBuckets[bucket];
		newBuckets[bucket] = i;
	}

	this->capacity = newSize;
	this->buckets = newBuckets;
	this->entries = newEntries;
	RETURN_SUCCESS;
}

int Hash::FindEntry(ThreadHandle thread, Value *key, int32_t hashCode, size_t &index)
{
	index = LAST;

	if (this->buckets != nullptr)
	{
		for (size_t i = this->buckets[hashCode % this->capacity]; i != LAST; i = this->entries[i].next)
		{
			HashEntry *entry = this->entries + i;
			if (entry->hashCode == hashCode)
			{
				bool equals;
				int r = this->KeyEquals(thread, key, &entry->key, equals);
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

int Hash::KeyEquals(ThreadHandle thread, Value *a, Value *b, bool &equals)
{
	// Call this.keyComparer.equals(a, b)
	VM_Push(thread, &this->keyComparer);
	VM_Push(thread, a);
	VM_Push(thread, b);

	Value result;
	int r = VM_InvokeMember(thread, strings::equals, 2, &result);
	if (r == OVUM_SUCCESS)
		equals = IsTrue(&result);
	return r;
}

int Hash::MergeIntoTopOfStack(ThreadHandle thread)
{
	int status__;

	for (size_t i = 0; i < this->count; i++)
	{
		HashEntry *e = this->entries + i;
		if (e->IsRemoved())
			// This entry has been removed; skip it.
			continue;

		VM_Dup(thread); // the other hash table
		VM_Push(thread, &e->key);
		VM_Push(thread, &e->value);
		CHECKED(VM_StoreIndexer(thread, 1));
	}

retStatus__:
	return status__;
}

} // namespace aves

AVES_API int OVUM_CDECL aves_Hash_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(aves::Hash));
	Type_SetReferenceWalker(type, aves_Hash_walkReferences);

	int status__;
	CHECKED(Type_AddNativeField(type, offsetof(aves::Hash, buckets), NativeFieldType::GC_ARRAY));
	CHECKED(Type_AddNativeField(type, offsetof(aves::Hash, entries), NativeFieldType::GC_ARRAY));
	CHECKED(Type_AddNativeField(type, offsetof(aves::Hash, keyComparer), NativeFieldType::VALUE));

retStatus__:
	return status__;
}

AVES_API NATIVE_FUNCTION(aves_Hash_get_length)
{
	Hash *inst = THISV.Get<Hash>();

	VM_PushInt(thread, (int64_t)inst->GetLength());
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_Hash_get_capacity)
{
	Hash *inst = THISV.Get<Hash>();

	VM_PushInt(thread, (int64_t)inst->capacity);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_Hash_get_keyComparer)
{
	Hash *inst = THISV.Get<Hash>();

	VM_Push(thread, &inst->keyComparer);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_Hash_get_version)
{
	Hash *inst = THISV.Get<Hash>();

	VM_PushInt(thread, inst->version);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_Hash_get_entryCount)
{
	Hash *inst = THISV.Get<Hash>();

	VM_PushInt(thread, (int64_t)inst->count);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_Hash_get_maxCapacity)
{
	VM_PushInt(thread, OVUM_ISIZE_MAX);
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_Hash_initialize)
{
	// initialize(capacity: Int, keyComparer: EqualityComparer)
	Alias<Hash> inst(THISP);
	inst->freeList = Hash::LAST;

	int64_t capacity = args[1].v.integer;
	if (capacity > 0)
	{
		Pinned h(THISP);
		CHECKED(inst->InitializeBuckets(thread, (size_t)capacity));
	}

	inst->keyComparer = args[2];

	RETURN_SUCCESS;
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_Hash_getItemInternal)
{
	// getItemInternal(key: non-null, hash: Int|UInt)
	Aves *aves = Aves::Get(thread);

	{ Pinned h(THISP);
		Hash *inst = THISV.Get<Hash>();

		int32_t hashCode = Hash::GetHash(args[2].v.uinteger);
		size_t index;
		CHECKED(inst->FindEntry(thread, args + 1, hashCode, index));
		if (index != Hash::LAST)
		{
			VM_Push(thread, &inst->entries[index].value);
			RETURN_SUCCESS;
		}
	}

	VM_PushString(thread, error_strings::HashKeyNotFound); // message
	VM_PushString(thread, strings::key); // paramName
	return VM_ThrowErrorOfType(thread, aves->aves.ArgumentError, 2);
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_Hash_getEntry)
{
	Aves *aves = Aves::Get(thread);

	Hash *inst = THISV.Get<Hash>();

	size_t index = (size_t)args[1].v.integer;

	HashEntry *entryPointer = inst->entries + index;
	if (!entryPointer->IsRemoved())
	{
		Value entry;
		entry.type = aves->aves.HashEntry;
		entry.v.instance = reinterpret_cast<uint8_t*>(entryPointer);

		VM_Push(thread, &entry); // yay
	}
	else
	{
		VM_PushNull(thread);
	}
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_Hash_insert)
{
	// insert(key: non-null, hash: Int|UInt, value, add: Boolean)
	Aves *aves = Aves::Get(thread);

	Pinned h(THISP);
	Hash *inst = THISV.Get<Hash>();

	bool add = !!args[4].v.integer;

	if (inst->buckets == nullptr)
		CHECKED(inst->InitializeBuckets(thread, 0));

	int32_t hashCode = Hash::GetHash(args[2].v.uinteger);
	size_t bucket = hashCode % inst->capacity;

	for (size_t i = inst->buckets[bucket]; i != Hash::LAST; )
	{
		HashEntry *entry = inst->entries + i;
		if (entry->hashCode == hashCode)
		{
			bool equals;
			CHECKED(inst->KeyEquals(thread, args + 1, &entry->key, equals));
			if (equals)
			{
				if (add)
					return VM_ThrowErrorOfType(thread, aves->aves.DuplicateKeyError, 0);

				entry->value = args[3];
				inst->version++;
				RETURN_SUCCESS; // Done!
			}
		}
		i = entry->next;
	}

	// The key is not in the hash table, let's add it!
	size_t index;
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
			CHECKED(inst->Resize(thread));
			bucket = hashCode % inst->capacity;
		}
		index = inst->count;
		inst->count++;
	}

	{
		HashEntry *entry = inst->entries + index;
		entry->hashCode = hashCode;
		entry->next = inst->buckets[bucket];
		entry->key = args[1];
		entry->value = args[3];
		inst->buckets[bucket] = index;
	}
	inst->version++;
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_Hash_containsKeyInternal)
{
	// containsKeyInternal(key: non-null, hash: Int|UInt)
	int32_t hashCode = Hash::GetHash(args[2].v.uinteger);
	size_t index;
	{ Pinned h(THISP);
		Hash *inst = THISV.Get<Hash>();
		CHECKED(inst->FindEntry(thread, args + 1, hashCode, index));
	}

	VM_PushBool(thread, index != Hash::LAST);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_Hash_containsValue)
{
	// containsValue(value)
	Alias<Hash> inst(THISP);

	for (size_t i = 0; i < inst->count; i++)
		if (!inst->entries[i].IsRemoved())
		{
			VM_Push(thread, args + 1); // value
			VM_Push(thread, &inst->entries[i].value);
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
	int32_t hashCode = Hash::GetHash(args[2].v.uinteger);
	size_t index;
	{ Pinned h(THISP);
		Hash *inst = THISV.Get<Hash>();
		CHECKED(inst->FindEntry(thread, args + 1, hashCode, index));

		if (index != Hash::LAST)
		{
			HashEntry *entry = inst->entries + index;
			WriteReference(args + 3, &entry->value);
		}
	}

	VM_PushBool(thread, index != Hash::LAST);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_Hash_removeInternal)
{
	// removeInternal(key: non-null, hash: Int|UInt)
	Hash *inst = THISV.Get<Hash>();

	if (inst->buckets != nullptr)
	{
		Pinned h(THISP);
		int32_t hashCode = Hash::GetHash(args[2].v.uinteger);
		size_t bucket = hashCode % inst->capacity;
		size_t lastEntry = Hash::LAST;

		for (size_t i = inst->buckets[bucket]; i != Hash::LAST; )
		{
			HashEntry *entry = inst->entries + i;
			if (entry->hashCode == hashCode)
			{
				bool equals;
				CHECKED(inst->KeyEquals(thread, args + 1, &entry->key, equals));
				if (equals)
				{
					// Key found!
					if (lastEntry == Hash::LAST)
						inst->buckets[bucket] = entry->next;
					else
						inst->entries[lastEntry].next = entry->next;

					entry->hashCode = HashEntry::REMOVED;
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
			i = entry->next;
		}
	}

	VM_PushBool(thread, false);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_Hash_concatInternal)
{
	// concatInternal(other: Hash)

	PinnedAlias<Hash> ha(THISP), hb(args + 1);

	// Construct the output hash, and leave it on the stack.
	// Always use the key comparer from the first hash table.
	VM_PushInt(thread, static_cast<int64_t>(ha->GetLength() + hb->GetLength())); // capacity
	VM_Push(thread, &ha->keyComparer); // keyComparer
	CHECKED(GC_Construct(thread, GetType_Hash(thread), 2, nullptr));

	CHECKED(ha->MergeIntoTopOfStack(thread));
	CHECKED(hb->MergeIntoTopOfStack(thread));

	// Result is on the top of the stack
}
END_NATIVE_FUNCTION;

AVES_API NATIVE_FUNCTION(aves_Hash_pinEntries)
{
	Hash *hash = THISV.Get<Hash>();
	GC_PinInst(hash->entries);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_Hash_unpinEntries)
{
	Hash *hash = THISV.Get<Hash>();
	GC_UnpinInst(hash->entries);
	RETURN_SUCCESS;
}

AVES_API int OVUM_CDECL InitHashInstance(ThreadHandle thread, size_t capacity, Value *result)
{
	VM_PushInt(thread, capacity);
	return GC_Construct(thread, GetType_Hash(thread), 1, result);
}

int OVUM_CDECL aves_Hash_walkReferences(void *basePtr, ReferenceVisitor callback, void *cbState)
{
	Hash *hash = reinterpret_cast<Hash*>(basePtr);
	for (size_t i = 0; i < hash->count; i++)
	{
		HashEntry *e = hash->entries + i;
		if (!e->IsRemoved())
		{
			// Key and value are adjacent, with the key first
			int r = callback(cbState, 2, &e->key);
			if (r != OVUM_SUCCESS) return r;
		}
	}
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_HashEntry_get_hashCode)
{
	HashEntry *e = THISV.Get<HashEntry>();
	VM_PushInt(thread, e->hashCode);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_HashEntry_get_nextIndex)
{
	HashEntry *e = THISV.Get<HashEntry>();
	VM_PushInt(thread, e->next);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_HashEntry_get_key)
{
	HashEntry *e = THISV.Get<HashEntry>();
	VM_Push(thread, &e->key);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_HashEntry_get_value)
{
	HashEntry *e = THISV.Get<HashEntry>();
	VM_Push(thread, &e->value);
	RETURN_SUCCESS;
}
