#include "set.h"
#include "../aves_state.h"
#include <cstddef>

using namespace aves;

namespace aves
{

int Set::InitializeBuckets(ThreadHandle thread, size_t capacity)
{
	size_t size = HashHelper_GetPrime(capacity);

	int r = GC_AllocArrayT(thread, size, &this->buckets);
	if (r != OVUM_SUCCESS) return r;
	memset(this->buckets, -1, size * sizeof(size_t));

	r = GC_AllocArrayT(thread, size, &this->entries);
	if (r != OVUM_SUCCESS) return r;

	this->capacity = size;
	this->freeList = -1;
	RETURN_SUCCESS;
}

int Set::Resize(ThreadHandle thread)
{
	size_t newSize = HashHelper_GetPrime(this->count * 2);

	size_t *newBuckets;
	int r = GC_AllocArrayT(thread, newSize, &newBuckets);
	if (r != OVUM_SUCCESS) return r;
	memset(newBuckets, -1, newSize * sizeof(size_t));

	SetEntry *newEntries;
	r = GC_AllocArrayT(thread, newSize, &newEntries);
	if (r != OVUM_SUCCESS) return r;
	CopyMemoryT(newEntries, this->entries, this->count);

	SetEntry *e = newEntries;
	for (size_t i = 0; i < this->count; i++, e++)
	{
		size_t bucket = e->hashCode % newSize;
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
	Type_SetInstanceSize(type, sizeof(Set));
	Type_SetReferenceWalker(type, aves_Set_walkReferences);

	int status__;
	CHECKED(Type_AddNativeField(type, offsetof(Set, buckets), NativeFieldType::GC_ARRAY));
	CHECKED(Type_AddNativeField(type, offsetof(Set, entries), NativeFieldType::GC_ARRAY));
	CHECKED(Type_AddNativeField(type, offsetof(Set, itemComparer), NativeFieldType::VALUE));

retStatus__:
	return status__;
}

AVES_API NATIVE_FUNCTION(aves_Set_get_length)
{
	Set *set = THISV.Get<Set>();
	VM_PushInt(thread, set->count - set->freeCount);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_Set_get_capacity)
{
	Set *set = THISV.Get<Set>();
	VM_PushInt(thread, set->capacity);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_Set_get_itemComparer)
{
	Set *set = THISV.Get<Set>();
	VM_Push(thread, &set->itemComparer);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_Set_get_version)
{
	Set *set = THISV.Get<Set>();
	VM_PushInt(thread, set->version);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_Set_get_entryCount)
{
	Set *set = THISV.Get<Set>();
	VM_PushInt(thread, set->count);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_Set_get_maxCapacity)
{
	VM_PushInt(thread, OVUM_ISIZE_MAX);
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_Set_initialize)
{
	// initialize(capacity is Int, itemComparer is EqualityComparer)
	Alias<Set> set(THISP);

	size_t capacity = (size_t)args[1].v.integer;

	if (capacity > 0)
	{
		Pinned s(THISP);
		CHECKED(set->InitializeBuckets(thread, capacity));
	}

	set->itemComparer = args[2];
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_Set_clear)
{
	Set *set = THISV.Get<Set>();

	memset(set->buckets, -1, set->capacity * sizeof(size_t));
	memset(set->entries, 0, set->count * sizeof(SetEntry));
	set->count = 0;
	set->freeCount = 0;
	set->freeList = Set::LAST;
	set->version++;
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_Set_containsInternal)
{
	// Arguments: (item, hash is Int|UInt)
	Pinned s(THISP);
	Set *set = THISV.Get<Set>();

	if (set->buckets != nullptr)
	{
		int32_t hash = Set::GetHash(args[2].v.uinteger);
		size_t bucket = hash % set->capacity;

		for (size_t i = set->buckets[bucket]; i != Set::LAST; i = set->entries[i].next)
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
	Set *set = THISV.Get<Set>();
	if (set->buckets == nullptr)
		CHECKED(set->InitializeBuckets(thread, 0));

	int32_t hash = Set::GetHash(args[2].v.uinteger);
	size_t bucket = hash % set->capacity;

	for (size_t i = set->buckets[bucket]; i != Set::LAST; i = set->entries[i].next)
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

	size_t index;
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
	Set *set = THISV.Get<Set>();

	if (set->buckets != nullptr)
	{
		Pinned s(THISP);

		int32_t hash = Set::GetHash(args[2].v.uinteger);
		size_t bucket = hash % set->capacity;
		size_t lastEntry = Set::LAST;

		for (size_t i = set->buckets[bucket]; i != Set::LAST; i = set->entries[i].next)
		{
			if (set->entries[i].hashCode == hash)
			{
				bool equals;
				CHECKED(set->ItemEquals(thread, args + 1, &set->entries[i].value, equals));
				if (equals)
				{
					// Found it!
					SetEntry *entry = set->entries + i;
					if (lastEntry == Set::LAST)
						set->buckets[bucket] = entry->next;
					else
						set->entries[lastEntry].next = entry->next;

					entry->hashCode = SetEntry::REMOVED;
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
	Set *set = THISV.Get<Set>();
	size_t index = (size_t)args[1].v.integer;
	VM_PushBool(thread, !set->entries[index].IsRemoved());
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_Set_getEntryAt)
{
	Set *set = THISV.Get<Set>();
	size_t index = (size_t)args[1].v.integer;
	VM_Push(thread, &set->entries[index].value);
	RETURN_SUCCESS;
}

int OVUM_CDECL aves_Set_walkReferences(void *basePtr, ReferenceVisitor callback, void *cbState)
{
	Set *set = reinterpret_cast<Set*>(basePtr);
	for (size_t i = 0; i < set->count; i++)
	{
		SetEntry *e = set->entries + i;
		if (!e->IsRemoved())
		{
			int r = callback(cbState, 1, &e->value);
			if (r != OVUM_SUCCESS) return r;
		}
	}
	RETURN_SUCCESS;
}
