#include "../vm.h"
#include "../../inc/ovum_string.h"
#include "stringtable.h"

namespace ovum
{

StringTable::StringTable(const int capacity) :
	count(0), freeCount(0), freeList(-1)
{
	this->capacity = HashHelper_GetPrime(capacity);

	this->buckets = new int32_t[this->capacity];
	memset(this->buckets, -1, this->capacity * sizeof(int32_t));

	this->entries = new Entry[this->capacity];
	memset(this->entries, 0, this->capacity * sizeof(Entry));
}

StringTable::~StringTable()
{
	delete[] this->buckets;
	delete[] this->entries;
}

String *StringTable::GetValue(String *value, const bool add)
{
	int32_t hashCode = String_GetHashCode(value) & INT32_MAX;

	int32_t bucket = hashCode % capacity;
	for (int32_t i = buckets[bucket]; i >= 0; i = entries[i].next)
	{
		Entry *e = entries + i;
		if (e->hashCode == hashCode && String_Equals(e->value, value))
			return e->value;
	}

	// The bucket did not contain the specified value.
	if (add)
	{
		int32_t index;
		if (freeCount > 0)
		{
			index = freeList;
			freeList = entries[freeList].next;
			freeCount--;
		}
		else
		{
			if (count == capacity)
			{
				Resize();
				bucket = hashCode % capacity;
			}
			index = count++;
		}

		Entry *e = entries + index;
		e->next     = buckets[bucket];
		e->hashCode = hashCode;
		e->value    = value;
		buckets[bucket] = index;
		value->flags |= StringFlags::INTERN;
		return value; // We just interned it! yay!
	}

	return nullptr;
}

void StringTable::Resize()
{
	int32_t newSize = HashHelper_GetPrime(capacity * 2);

	int32_t *newBuckets = new int32_t[newSize];
	memset(newBuckets, -1, newSize * sizeof(int32_t));

	Entry *newEntries = new Entry[newSize];
	CopyMemoryT(newEntries, entries, count);

	Entry *e = newEntries;
	for (int32_t i = 0; i < count; i++, e++)	
	{
		int32_t bucket = e->hashCode % newSize;
		e->next = newBuckets[bucket];
		newBuckets[bucket] = i;
	}

	delete[] this->buckets;
	delete[] this->entries;

	capacity = newSize;
	this->buckets = newBuckets;
	this->entries = newEntries;
}

String *StringTable::GetInterned(String *value)
{
	return GetValue(value, false);
}

String *StringTable::Intern(String *value)
{
	return GetValue(value, true);
}

bool StringTable::RemoveIntern(String *value)
{
	// The string MUST be an interned string here. Must be.
	OVUM_ASSERT((value->flags & StringFlags::INTERN) == StringFlags::INTERN);
	// It also has to be hashed. Kinda hard to intern it
	// without hashing it, but let's check it anyway.
	OVUM_ASSERT((value->flags & StringFlags::HASHED) == StringFlags::HASHED);

	int32_t bucket = (value->hashCode & INT32_MAX) % capacity;
	Entry *lastEntry = nullptr;
	for (int32_t i = buckets[bucket]; i >= 0; i = lastEntry->next)
	{
		Entry *e = entries + i;
		if (e->value == value) // Compare pointers for great speed
		{
			// We found it!
			if (lastEntry == nullptr)
				buckets[bucket] = e->next;
			else
				lastEntry->next = e->next;

			e->hashCode = -1;
			e->next = freeList;
			e->value = nullptr;
			freeList = i;
			freeCount++;
			// Do we need this? This method isn't supposed to be called
			// outside of the GC's collection cycle.
			value->flags &= ~StringFlags::INTERN;
			return true;
		}
		lastEntry = e;
	}

	return false;
}

void StringTable::UpdateIntern(String *value)
{
	// The string should be interned already.
	OVUM_ASSERT((value->flags & StringFlags::INTERN) == StringFlags::INTERN);
	// It should also not be possible to have a non-hashed intern.
	OVUM_ASSERT((value->flags & StringFlags::HASHED) == StringFlags::HASHED);

	int32_t hashCode = value->hashCode & INT32_MAX;
	Entry *entry;
	for (int32_t i = buckets[hashCode % capacity]; i >= 0; i = entry->next)
	{
		entry = entries + i;
		if (entry->hashCode == hashCode && String_Equals(value, entry->value))
		{
			entry->value = value;
			break;
		}
	}
}

} // namespace ovum
