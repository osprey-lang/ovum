#pragma once

#include "../vm.h"
#include "../../inc/ovum_string.h"

namespace ovum
{

// This file contains a StringHash template class, because these are commonly needed in the VM.
// A StringHash maps a String* to any type, using a relatively naïve implementation.
// This class is intended for collections that do not need to be resized, because this class
// actually does not support resizing. It always has a fixed number of buckets.

template<class T> class StringHashEntry;

template<class T>
class StringHash
{
public:
	explicit StringHash(size_t capacity);

	bool Get(String *key, T &value) const;

	bool Add(String *key, T value);

	bool Set(String *key, T value);

	size_t GetCount() const;

	bool GetByIndex(size_t index, T &result) const;

	void FreeValues();

	void DeleteValues();

	void DeleteArrayValues();

private:
	typedef StringHashEntry<T> Entry;

	size_t capacity;
	size_t count;
	Box<size_t[]> buckets;
	Box<Entry[]> entries;

	bool Insert(String *key, T value, bool add);

	friend class Type;
};

template<class T>
class StringHashEntry
{
public:
	static const size_t LAST = (size_t)-1;

	int32_t hashCode;
	size_t next;
	String *key;
	T value;
};


template<class T>
StringHash<T>::StringHash(size_t capacity) :
	capacity(0),
	count(0),
	buckets(),
	entries()
{
	if (capacity == 0)
	{
		this->capacity = 0;
	}
	else
	{
		this->capacity = HashHelper_GetPrime(capacity);

		buckets.reset(new size_t[this->capacity]);
		memset(buckets.get(), -1, sizeof(size_t) * this->capacity);

		entries.reset(new Entry[this->capacity]);
		memset(entries.get(), 0, sizeof(Entry) * this->capacity);
	}
}

template<class T>
bool StringHash<T>::Insert(String *key, T value, bool add)
{
	if (buckets == nullptr)
		// If you've said you're not going to put anything in the hash,
		// you don't get to put anything in it.
		return false;

	int32_t hashCode = String_GetHashCode(key) & INT32_MAX;

	size_t bucket = hashCode % capacity;

	for (size_t i = buckets[bucket]; i != Entry::LAST; i = entries[i].next)
		if (hashCode == entries[i].hashCode &&
			String_Equals(key, entries[i].key))
		{
			if (add)
				return false; // NOPE
			entries[i].value = value;
			return true;
		}

	// Not found, let's add it!

	if (count == capacity)
		return false; // already full, sorry!

	size_t index = count++;

	Entry *e = entries.get() + index;
	e->hashCode = hashCode;
	e->next = buckets[bucket];
	e->key = key;
	e->value = value;
	buckets[bucket] = index;
	return true;
}

template<class T>
bool StringHash<T>::Get(String *key, T &value) const
{
	if (buckets)
	{
		int32_t hashCode = String_GetHashCode(key) & INT32_MAX;
		size_t bucket = hashCode % capacity;

		for (size_t i = buckets[bucket]; i != Entry::LAST; i = entries[i].next)
			if (hashCode == entries[i].hashCode &&
				String_Equals(key, entries[i].key))
			{
				value = entries[i].value;
				return true;
			}
	}
	return false;
}

template<class T>
bool StringHash<T>::Add(String *key, T value)
{
	return Insert(key, value, true);
}

template<class T>
bool StringHash<T>::Set(String *key, T value)
{
	return Insert(key, value, false);
}

template<class T>
size_t StringHash<T>::GetCount() const
{
	return count;
}

template<class T>
bool StringHash<T>::GetByIndex(size_t index, T &result) const
{
	if (index >= count)
		return false;
	result = entries[index].value;
	return true;
}

template<class T>
void StringHash<T>::FreeValues()
{
	for (size_t i = 0; i < count; i++)
	{
		free(entries[i].value);
		entries[i].value = nullptr;
	}
}

template<class T>
void StringHash<T>::DeleteValues()
{
	for (size_t i = 0; i < count; i++)
	{
		delete entries[i].value;
		entries[i].value = nullptr;
	}
}

template<class T>
void StringHash<T>::DeleteArrayValues()
{
	for (size_t i = 0; i < count; i++)
	{
		delete[] entries[i].value;
		entries[i].value = nullptr;
	}
}

} // namespace ovum
