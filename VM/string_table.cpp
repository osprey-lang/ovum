#include "ov_vm.internal.h"
#include "string_table.internal.h"

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

bool StringTable::RemoveIntern(String *value)
{
	// The string MUST be an interned string here. Must be.
	assert((value->flags & StringFlags::INTERN) == StringFlags::INTERN);
	// It also has to be hashed. Kinda hard to intern it
	// without hashing it, but let's check it anyway.
	assert((value->flags & StringFlags::HASHED) == StringFlags::HASHED);

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

/*String *StringTable::GetInterned(const int32_t length, const uchar values[])
{
	int32_t hashCode = String_GetHashCode(length, values) & INT32_MAX;

	int32_t bucket = hashCode % capacity;
	for (int32_t i = buckets[bucket]; i >= 0; i = entries[i].next)
	{
		Entry *e = entries + i;
		if (e->hashCode == hashCode && StringEquals(e->value, length, values))
			return e->value;
	}

	return nullptr;
}*/

String *StringTable::Intern(String *value)
{
	return GetValue(value, true);
}

bool StringTable::StringEquals(String *a, const int32_t blen, const uchar b[])
{
	if (a->length != blen)
		return false;

	// This is a slightly modified version of the algorithm used in String_Equals.

	// It doesn't matter which string we take the length of; 
	// they're guaranteed to be the same here anyway.
	int32_t length = a->length;

	const uchar *ap = &a->firstChar;
	const uchar *bp = b;

	// Unroll the loop by 10 characters
	while (length > 10)
	{
		if (*(int32_t*)ap != *(int32_t*)bp ||
			*(int32_t*)(ap + 2) != *(int32_t*)(bp + 2) ||
			*(int32_t*)(ap + 4) != *(int32_t*)(bp + 4) ||
			*(int32_t*)(ap + 6) != *(int32_t*)(bp + 6) ||
			*(int32_t*)(ap + 8) != *(int32_t*)(bp + 8)) break;
		ap += 10;
		bp += 10;
		length -= 10;
	}

	while (length > 0)
	{
		if (*(int32_t*)ap != *(int32_t*)bp)
			break;
		ap += 2;
		bp += 2;

		length -= 2;
	}

	return length <= 0;
}

void StringTable::DebugBuckets()
{
	using namespace std;

	int32_t bucketsUsed = 0;
	int32_t mostCollidedBucket = 0;
	int32_t maxCollisionCount = 0;

	for (int32_t i = 0; i < capacity; i++)
	{
		if (i > 0)
			wprintf(L", ");
		wprintf(L"%d", buckets[i]);
		if (buckets[i] >= 0)
		{
			bucketsUsed++;
			int32_t collisions = 0;
			for (int32_t k = buckets[i]; k >= 0; k = entries[k].next)
				collisions++;
			if (collisions > maxCollisionCount)
			{
				maxCollisionCount = collisions;
				mostCollidedBucket = i;
			}
		}
	}
	wprintf(L"\n");

	wprintf(L"Used %d out of %d buckets\n", bucketsUsed, capacity);
	wprintf(L"Most collided bucket: %d (%d)\n", mostCollidedBucket, maxCollisionCount);
}