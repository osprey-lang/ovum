#include "aves_set.h"
#include <memory>

#define _Set(value)         reinterpret_cast<::SetInst*>((value).instance)
#define U64_TO_HASH(value)  ((int32_t)(value) ^ (int32_t)((value) >> 32))

AVES_API void aves_Set_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(SetInst));
	Type_SetReferenceGetter(type, aves_Set_getReferences);
	Type_SetFinalizer(type, aves_Set_finalize);
}

void InitializeBuckets(SetInst *set, const int32_t capacity)
{
	int32_t size = HashHelper_GetPrime(capacity);

	set->capacity = size;

	set->buckets = new int32_t[size];
	memset(set->buckets, -1, size * sizeof(int32_t));

	set->entries = new SetEntry[size];
	memset(set->entries, 0, size * sizeof(SetEntry));

	set->freeList = -1;
}

void ResizeSet(ThreadHandle thread, SetInst *set)
{
	using namespace std;

	int32_t newSize = HashHelper_GetPrime(set->count * 2);

	unique_ptr<int32_t[]> newBuckets(new int32_t[newSize]);
	memset(newBuckets.get(), -1, newSize * sizeof(int32_t));

	unique_ptr<SetEntry[]> newEntries(new SetEntry[newSize]);
	CopyMemoryT(newEntries.get(), set->entries, set->count);
	
	SetEntry *e = newEntries.get();
	for (int32_t i = 0; i < set->count; i++, e++)
	{
		int32_t bucket = e->hashCode % newSize;
		e->next = newBuckets[bucket];
		newBuckets[bucket] = i;
	}

	delete[] set->buckets;
	delete[] set->entries;

	set->buckets = newBuckets.release();
	set->entries = newEntries.release();
	set->capacity = newSize;
}

AVES_API NATIVE_FUNCTION(aves_Set_new)
{
	SetInst *set = _Set(THISV);
	int64_t capacity = args[1].integer;
	if (capacity < 0 || capacity > INT32_MAX)
	{
		VM_PushString(thread, strings::capacity);
		GC_Construct(thread, Types::ArgumentRangeError, 1, nullptr);
		VM_Throw(thread);
	}

	if (capacity > 0)
		InitializeBuckets(set, (int32_t)capacity);
}

AVES_API NATIVE_FUNCTION(aves_Set_get_length)
{
	SetInst *set = _Set(THISV);
	VM_PushInt(thread, set->count - set->freeCount);
}

AVES_API NATIVE_FUNCTION(aves_Set_clear)
{
	SetInst *set = _Set(THISV);

	memset(set->buckets, -1, set->capacity * sizeof(int32_t*));
	memset(set->entries, 0, set->capacity * sizeof(SetEntry));
	set->count = 0;
	set->freeCount = 0;
	set->freeList = -1;
	set->version++;
}
AVES_API NATIVE_FUNCTION(aves_Set_containsInternal)
{
	// Arguments: (item, hash is Int|UInt)
	Pinned s(THISP);
	SetInst *set = _Set(THISV);

	if (set->buckets != nullptr)
	{
		int32_t hash = U64_TO_HASH(args[2].uinteger) & INT32_MAX;
		int32_t bucket = hash % set->capacity;

		for (int32_t i = set->buckets[bucket]; i >= 0; i = set->entries[i].next)
		{
			if (set->entries[i].hashCode == hash)
			{
				VM_Push(thread, args[1]); // item
				VM_Push(thread, set->entries[i].value);
				if (VM_Equals(thread))
				{
					VM_PushBool(thread, true);
					return;
				}
			}
		}
	}

	VM_PushBool(thread, false);
}
AVES_API NATIVE_FUNCTION(aves_Set_addInternal)
{
	// Arguments: (item, hash is Int|UInt)
	Pinned s(THISP);
	SetInst *set = _Set(THISV);
	if (set->buckets == nullptr)
		InitializeBuckets(set, 0);

	int32_t hash = U64_TO_HASH(args[2].uinteger) & INT32_MAX;
	int32_t bucket = hash % set->capacity;

	for (int32_t i = set->buckets[bucket]; i >= 0; i = set->entries[i].next)
	{
		if (set->entries[i].hashCode == hash)
		{
			VM_Push(thread, args[1]); // item
			VM_Push(thread, set->entries[i].value);
			if (VM_Equals(thread))
			{
				VM_PushBool(thread, false); // Already in the set!
				return;
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
			ResizeSet(thread, set);
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
AVES_API NATIVE_FUNCTION(aves_Set_removeInternal)
{
	// Arguments: (item, hash is Int|UInt)
	Pinned s(THISP);
	SetInst *set = _Set(THISV);

	if (set->buckets != nullptr)
	{
		int32_t hash = U64_TO_HASH(args[2].uinteger) & INT32_MAX;
		int32_t bucket = hash % set->capacity;
		int32_t lastEntry = -1;

		for (int32_t i = set->buckets[bucket]; i >= 0; i = set->entries[i].next)
		{
			if (set->entries[i].hashCode == hash)
			{
				VM_Push(thread, args[1]); // item
				VM_Push(thread, set->entries[i].value);
				if (VM_Equals(thread))
				{
					// Found it!
					SetEntry *entry = set->entries + i;
					if (lastEntry < 0)
						set->buckets[bucket] = entry->next;
					else
						set->entries[lastEntry].next = entry->next;

					entry->hashCode = -1;
					entry->next = set->freeList;
					entry->value = NULL_VALUE;
					set->freeList = i;
					set->freeCount++;
					set->version++;
					VM_PushBool(thread, true);
					return;
				}
			}
			lastEntry = i;
		}
	}

	VM_PushBool(thread, false); // not found
}

AVES_API NATIVE_FUNCTION(aves_Set_get_version)
{
	SetInst *set = _Set(THISV);
	VM_PushInt(thread, set->version);
}
AVES_API NATIVE_FUNCTION(aves_Set_get_entryCount)
{
	SetInst *set = _Set(THISV);
	VM_PushInt(thread, set->count);
}
AVES_API NATIVE_FUNCTION(aves_Set_hasEntryAt)
{
	SetInst *set = _Set(THISV);
	int32_t index = (int32_t)args[1].integer;
	VM_PushBool(thread, set->entries[index].hashCode >= 0);
}
AVES_API NATIVE_FUNCTION(aves_Set_getEntryAt)
{
	SetInst *set = _Set(THISV);
	int32_t index = (int32_t)args[1].integer;
	VM_Push(thread, set->entries[index].value);
}

bool aves_Set_getReferences(void *basePtr, unsigned int *valc, Value **target, int32_t *state)
{
	SetInst *set = reinterpret_cast<SetInst*>(basePtr);
	int32_t i = *state;
	while (i < set->count)
	{
		if (set->entries[i].hashCode != -1)
		{
			*valc = 1;
			*target = &set->entries[i].value;
			*state = i + 1;
			return true;
		}
		i++;
	}
	return false;
}

void aves_Set_finalize(void *basePtr)
{
	SetInst *set = reinterpret_cast<SetInst*>(basePtr);

	delete[] set->buckets;
	delete[] set->entries;
	set->capacity  = 0;
	set->count     = 0;
	set->freeCount = 0;
	set->freeList  = -1;
	set->version   = -1;
}