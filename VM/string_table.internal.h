#pragma once

#ifndef VM__STRING_TABLE_INTERNAL_H
#define VM__STRING_TABLE_INTERNAL_H

#include <cstdint>

typedef struct String_S String;
class GC;

// The StringTable class contains the implementation of the string
// intern table, which is effectively a hash set of String* values.
// This is used by the GC when strings are constructed during module
// loading, to avoid the allocation of multiple identical strings.
// Strings can also be explicitly interned.
class StringTable
{
private:
	struct Entry
	{
		int32_t next;     // index of the next entry in the bucket; -1 if unused
		int32_t hashCode; // The lower 31 bits of the hash code
		String *value;    // the actual string!
	};

	int32_t capacity;   // size of buckets and entries
	int32_t count;      // total number of entries used
	int32_t freeCount;  // total number of freed entries
	int32_t freeList;   // index of first freed entry
	int32_t *buckets;   // indexes into entries
	Entry *entries; // the actual entries

	String *GetValue(String *value, const bool add);

	void Resize();

	bool StringEquals(String *a, const int32_t blen, const uchar b[]);

public:
	StringTable(const int32_t capacity);
	~StringTable();

	String *GetInterned(String *value);
	/*String *GetInterned(const int32_t length, const uchar values[]);*/

	inline bool HasInterned(String *value)
	{
		return GetInterned(value) != nullptr;
	}
	/*inline bool HasInterned(const int32_t length, const uchar values[])
	{
		return GetInterned(length, values) != nullptr;
	}*/

	String *Intern(String *value);

	bool RemoveIntern(String *value);

	void DebugBuckets();

	friend class GC;
};

#endif // VM__STRING_TABLE_INTERNAL_H