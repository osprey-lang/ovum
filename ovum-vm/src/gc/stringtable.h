#pragma once

#include "../vm.h"

namespace ovum
{

// The StringTable class contains the implementation of the string
// intern table, which is effectively a hash set of String* values.
// This is used by the GC when strings are constructed during module
// loading, to avoid the allocation of multiple identical strings.
// Strings can also be explicitly interned.
class StringTable
{
public:
	StringTable(size_t capacity);

	String *GetInterned(String *value);

	inline bool HasInterned(String *value)
	{
		return GetInterned(value) != nullptr;
	}

	String *Intern(String *value);

	bool RemoveIntern(String *value);

	void UpdateIntern(String *value);

private:
	static const size_t LAST = (size_t)-1;

	struct Entry
	{
		// Index of the next entry in the bucket. If there is no next
		// entry, the value is LAST.
		size_t next;
		// The lower 31 bits of the hash code.
		int32_t hashCode;
		// The actual string!
		String *value;
	};

	// Size of buckets and entries.
	size_t capacity;
	// Total number of entries used.
	size_t count;
	// Total number of entries that were freed after being used.
	size_t freeCount;
	// Index of first freed entry.
	size_t freeList;
	// Indexes into entries.
	Box<size_t[]> buckets;
	// The actual entries.
	Box<Entry[]> entries;

	String *GetValue(String *value, bool add);

	void Resize();

	friend class GC;
};

} // namespace ovum
