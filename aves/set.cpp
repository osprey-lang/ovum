#include "aves_set.h"
#include <cstddef>

#define U64_TO_HASH(value)  ((int32_t)(value) ^ (int32_t)((value) >> 32))

AVES_API void CDECL aves_Set_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(SetInst));
	Type_SetReferenceGetter(type, aves_Set_getReferences);
	
	Type_AddNativeField(type, offsetof(SetInst, buckets), NativeFieldType::GC_ARRAY);
	Type_AddNativeField(type, offsetof(SetInst, entries), NativeFieldType::GC_ARRAY);
}

int InitializeBuckets(ThreadHandle thread, SetInst *set, const int32_t capacity)
{
	int32_t size = HashHelper_GetPrime(capacity);

	int r = GC_AllocArrayT(thread, size, &set->buckets);
	if (r != OVUM_SUCCESS) return r;
	memset(set->buckets, -1, size * sizeof(int32_t));

	r = GC_AllocArrayT(thread, size, &set->entries);
	if (r != OVUM_SUCCESS) return r;

	set->capacity = size;
	set->freeList = -1;
	RETURN_SUCCESS;
}

int ResizeSet(ThreadHandle thread, SetInst *set)
{
	int32_t newSize = HashHelper_GetPrime(set->count * 2);

	int32_t *newBuckets;
	int r = GC_AllocArrayT(thread, newSize, &newBuckets);
	if (r != OVUM_SUCCESS) return r;
	memset(newBuckets, -1, newSize * sizeof(int32_t));

	SetEntry *newEntries;
	r = GC_AllocArrayT(thread, newSize, &newEntries);
	if (r != OVUM_SUCCESS) return r;
	CopyMemoryT(newEntries, set->entries, set->count);
	
	SetEntry *e = newEntries;
	for (int32_t i = 0; i < set->count; i++, e++)
	{
		int32_t bucket = e->hashCode % newSize;
		e->next = newBuckets[bucket];
		newBuckets[bucket] = i;
	}

	set->buckets = newBuckets;
	set->entries = newEntries;
	set->capacity = newSize;
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_Set_new)
{
	SetInst *set = THISV.Get<SetInst>();
	int64_t capacity = args[1].v.integer;
	if (capacity < 0 || capacity > INT32_MAX)
	{
		VM_PushString(thread, strings::capacity);
		return VM_ThrowErrorOfType(thread, Types::ArgumentRangeError, 1);
	}

	if (capacity > 0)
	{
		Pinned s(THISP);
		CHECKED(InitializeBuckets(thread, set, (int32_t)capacity));
	}
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_Set_get_length)
{
	SetInst *set = THISV.Get<SetInst>();
	VM_PushInt(thread, set->count - set->freeCount);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_Set_clear)
{
	SetInst *set = THISV.Get<SetInst>();

	memset(set->buckets, -1, set->capacity * sizeof(int32_t*));
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
	SetInst *set = THISV.Get<SetInst>();

	if (set->buckets != nullptr)
	{
		int32_t hash = U64_TO_HASH(args[2].v.uinteger) & INT32_MAX;
		int32_t bucket = hash % set->capacity;

		for (int32_t i = set->buckets[bucket]; i >= 0; i = set->entries[i].next)
		{
			if (set->entries[i].hashCode == hash)
			{
				VM_Push(thread, args + 1); // item
				VM_Push(thread, &set->entries[i].value);
				bool equals;
				CHECKED(VM_Equals(thread, &equals));
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
	// Arguments: (item, hash is Int|UInt)
	Pinned s(THISP);
	SetInst *set = THISV.Get<SetInst>();
	if (set->buckets == nullptr)
		CHECKED(InitializeBuckets(thread, set, 0));

	int32_t hash = U64_TO_HASH(args[2].v.uinteger) & INT32_MAX;
	int32_t bucket = hash % set->capacity;

	for (int32_t i = set->buckets[bucket]; i >= 0; i = set->entries[i].next)
	{
		if (set->entries[i].hashCode == hash)
		{
			VM_Push(thread, args + 1); // item
			VM_Push(thread, &set->entries[i].value);
			bool equals;
			CHECKED(VM_Equals(thread, &equals));
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
			CHECKED(ResizeSet(thread, set));
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
	// Arguments: (item, hash is Int|UInt)
	Pinned s(THISP);
	SetInst *set = THISV.Get<SetInst>();

	if (set->buckets != nullptr)
	{
		int32_t hash = U64_TO_HASH(args[2].v.uinteger) & INT32_MAX;
		int32_t bucket = hash % set->capacity;
		int32_t lastEntry = -1;

		for (int32_t i = set->buckets[bucket]; i >= 0; i = set->entries[i].next)
		{
			if (set->entries[i].hashCode == hash)
			{
				VM_Push(thread, args + 1); // item
				VM_Push(thread, &set->entries[i].value);
				bool equals;
				CHECKED(VM_Equals(thread, &equals));
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

AVES_API NATIVE_FUNCTION(aves_Set_get_version)
{
	SetInst *set = THISV.Get<SetInst>();
	VM_PushInt(thread, set->version);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_Set_get_entryCount)
{
	SetInst *set = THISV.Get<SetInst>();
	VM_PushInt(thread, set->count);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_Set_hasEntryAt)
{
	SetInst *set = THISV.Get<SetInst>();
	int32_t index = (int32_t)args[1].v.integer;
	VM_PushBool(thread, set->entries[index].hashCode >= 0);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_Set_getEntryAt)
{
	SetInst *set = THISV.Get<SetInst>();
	int32_t index = (int32_t)args[1].v.integer;
	VM_Push(thread, &set->entries[index].value);
	RETURN_SUCCESS;
}

int CDECL aves_Set_getReferences(void *basePtr, ReferenceVisitor callback, void *cbState)
{
	SetInst *set = reinterpret_cast<SetInst*>(basePtr);
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