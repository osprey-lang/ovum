#include "set.h"
#include "../aves_state.h"
#include <cstddef>

using namespace aves;

namespace aves
{

int Set::InitializeBuckets(ThreadHandle thread, int32_t capacity)
{
	int32_t size = HashHelper_GetPrime(capacity);

	int r = GC_AllocArrayT(thread, size, &this->buckets);
	if (r != OVUM_SUCCESS) return r;
	memset(this->buckets, -1, size * sizeof(int32_t));

	r = GC_AllocArrayT(thread, size, &this->entries);
	if (r != OVUM_SUCCESS) return r;

	this->capacity = size;
	this->freeList = -1;
	RETURN_SUCCESS;
}

int Set::Resize(ThreadHandle thread)
{
	int32_t newSize = HashHelper_GetPrime(this->count * 2);

	int32_t *newBuckets;
	int r = GC_AllocArrayT(thread, newSize, &newBuckets);
	if (r != OVUM_SUCCESS) return r;
	memset(newBuckets, -1, newSize * sizeof(int32_t));

	SetEntry *newEntries;
	r = GC_AllocArrayT(thread, newSize, &newEntries);
	if (r != OVUM_SUCCESS) return r;
	CopyMemoryT(newEntries, this->entries, this->count);

	SetEntry *e = newEntries;
	for (int32_t i = 0; i < this->count; i++, e++)
	{
		int32_t bucket = e->hashCode % newSize;
		e->next = newBuckets[bucket];
		newBuckets[bucket] = i;
	}

	this->buckets = newBuckets;
	this->entries = newEntries;
	this->capacity = newSize;
	RETURN_SUCCESS;
}

int Set::ItemEquals(ThreadHandle thread, Value *a, Value *b, bool &equals)
{
	// Call this.itemComparer.equals(a, b)
	VM_Push(thread, &this->itemComparer);
	VM_Push(thread, a);
	VM_Push(thread, b);

	Value result;
	int r = VM_InvokeMember(thread, strings::equals, 2, &result);
	if (r == OVUM_SUCCESS)
		equals = IsTrue(&result);
	return r;
}

} // namespace aves

AVES_API int OVUM_CDECL aves_Set_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(aves::Set));
	Type_SetReferenceWalker(type, aves_Set_walkReferences);

	int status__;
	CHECKED(Type_AddNativeField(type, offsetof(aves::Set, buckets), NativeFieldType::GC_ARRAY));
	CHECKED(Type_AddNativeField(type, offsetof(aves::Set, entries), NativeFieldType::GC_ARRAY));
	CHECKED(Type_AddNativeField(type, offsetof(aves::Set, itemComparer), NativeFieldType::VALUE));

retStatus__:
	return status__;
}

AVES_API NATIVE_FUNCTION(aves_Set_get_length)
{
	aves::Set *set = THISV.Get<aves::Set>();
	VM_PushInt(thread, set->count - set->freeCount);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_Set_get_capacity)
{
	aves::Set *set = THISV.Get<aves::Set>();
	VM_PushInt(thread, set->capacity);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_Set_get_itemComparer)
{
	aves::Set *set = THISV.Get<aves::Set>();
	VM_Push(thread, &set->itemComparer);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_Set_get_version)
{
	aves::Set *set = THISV.Get<aves::Set>();
	VM_PushInt(thread, set->version);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_Set_get_entryCount)
{
	aves::Set *set = THISV.Get<aves::Set>();
	VM_PushInt(thread, set->count);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_Set_get_maxCapacity)
{
	VM_PushInt(thread, INT32_MAX);
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_Set_initialize)
{
	// initialize(capacity is Int, itemComparer is EqualityComparer)
	Alias<aves::Set> set(THISP);

	int32_t capacity = (int32_t)args[1].v.integer;

	if (capacity > 0)
	{
		Pinned s(THISP);
		CHECKED(set->InitializeBuckets(thread, (int32_t)capacity));
	}

	set->itemComparer = args[2];
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_Set_clear)
{
	aves::Set *set = THISV.Get<aves::Set>();

	memset(set->buckets, -1, set->capacity * sizeof(int32_t));
	memset(set->entries, 0, set->count * sizeof(SetEntry));
	set->count = 0;
	set->freeCount = 0;
	set->freeList = -1;
	set->version++;
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_Set_containsInternal)
{
	// Arguments: (item, hash is Int|UInt)
	Pinned s(THISP);
	aves::Set *set = THISV.Get<aves::Set>();

	if (set->buckets != nullptr)
	{
		int32_t hash = aves::Set::GetHash(args[2].v.uinteger) & INT32_MAX;
		int32_t bucket = hash % set->capacity;

		for (int32_t i = set->buckets[bucket]; i >= 0; i = set->entries[i].next)
		{
			if (set->entries[i].hashCode == hash)
			{
				bool equals;
				CHECKED(set->ItemEquals(thread, args + 1, &set->entries[i].value, equals));
				if (equals)
				{
					VM_PushBool(thread, true);
					RETURN_SUCCESS;
				}
			}
		}
	}

	VM_PushBool(thread, false);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_Set_addInternal)
{
	// addInternal(item, hash is Int|UInt)
	Pinned s(THISP);
	aves::Set *set = THISV.Get<aves::Set>();
	if (set->buckets == nullptr)
		CHECKED(set->InitializeBuckets(thread, 0));

	int32_t hash = aves::Set::GetHash(args[2].v.uinteger);
	int32_t bucket = hash % set->capacity;

	for (int32_t i = set->buckets[bucket]; i >= 0; i = set->entries[i].next)
	{
		if (set->entries[i].hashCode == hash)
		{
			bool equals;
			CHECKED(set->ItemEquals(thread, args + 1, &set->entries[i].value, equals));
			if (equals)
			{
				VM_PushBool(thread, false); // Already in the set!
				RETURN_SUCCESS;
			}
		}
	}

	int32_t index;
	if (set->freeCount > 0)
	{
		index = set->freeList;
		set->freeList = set->entries[index].next;
		set->freeCount--;
	}
	else
	{
		if (set->count == set->capacity)
		{
			CHECKED(set->Resize(thread));
			bucket = hash % set->capacity;
		}
		index = set->count;
		set->count++;
	}

	set->entries[index].hashCode = hash;
	set->entries[index].next = set->buckets[bucket];
	set->entries[index].value = args[1]; // item
	set->buckets[bucket] = index;
	set->version++;

	VM_PushBool(thread, true); // Added new
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_Set_removeInternal)
{
	// removeInternal(item, hash is Int|UInt)
	Pinned s(THISP);
	aves::Set *set = THISV.Get<aves::Set>();

	if (set->buckets != nullptr)
	{
		int32_t hash = aves::Set::GetHash(args[2].v.uinteger);
		int32_t bucket = hash % set->capacity;
		int32_t lastEntry = -1;

		for (int32_t i = set->buckets[bucket]; i >= 0; i = set->entries[i].next)
		{
			if (set->entries[i].hashCode == hash)
			{
				bool equals;
				CHECKED(set->ItemEquals(thread, args + 1, &set->entries[i].value, equals));
				if (equals)
				{
					// Found it!
					SetEntry *entry = set->entries + i;
					if (lastEntry < 0)
						set->buckets[bucket] = entry->next;
					else
						set->entries[lastEntry].next = entry->next;

					entry->hashCode = -1;
					entry->next = set->freeList;
					entry->value.type = nullptr;
					set->freeList = i;
					set->freeCount++;
					set->version++;
					VM_PushBool(thread, true);
					RETURN_SUCCESS;
				}
			}
			lastEntry = i;
		}
	}

	VM_PushBool(thread, false); // not found
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_Set_hasEntryAt)
{
	aves::Set *set = THISV.Get<aves::Set>();
	int32_t index = (int32_t)args[1].v.integer;
	VM_PushBool(thread, set->entries[index].hashCode >= 0);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_Set_getEntryAt)
{
	aves::Set *set = THISV.Get<aves::Set>();
	int32_t index = (int32_t)args[1].v.integer;
	VM_Push(thread, &set->entries[index].value);
	RETURN_SUCCESS;
}

int OVUM_CDECL aves_Set_walkReferences(void *basePtr, ReferenceVisitor callback, void *cbState)
{
	aves::Set *set = reinterpret_cast<aves::Set*>(basePtr);
	for (int32_t i = 0; i < set->count; i++)
	{
		SetEntry *e = set->entries + i;
		if (e->hashCode >= 0)
		{
			int r = callback(cbState, 1, &e->value);
			if (r != OVUM_SUCCESS) return r;
		}
	}
	RETURN_SUCCESS;
}
