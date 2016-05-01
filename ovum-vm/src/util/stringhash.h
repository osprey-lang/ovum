#pragma once

#include "../vm.h"
#include "../../inc/ov_string.h"

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
private:
	int32_t capacity;
	int32_t count;
	int32_t *buckets;
	StringHashEntry<T> *entries;

	bool Insert(String *key, T value, bool add);

public:
	StringHash(int32_t capacity);
	~StringHash();

	bool Get(String *key, T &value) const;
	bool Add(String *key, T value);
	bool Set(String *key, T value);

	int32_t GetCount() const;
	bool GetByIndex(int32_t index, T &result) const;

	void FreeValues();
	void DeleteValues();
	void DeleteArrayValues();

	friend class Type;
};

template<class T>
class StringHashEntry
{
public:
	int32_t hashCode;
	int32_t next;
	String *key;
	T value;
};


template<class T>
StringHash<T>::StringHash(int32_t capacity)
{
	if (capacity <= 0)
	{
		count = this->capacity = 0;
		buckets = nullptr;
	}
	else
	{
		count = 0;
		this->capacity = HashHelper_GetPrime(capacity);

		buckets = new int32_t[this->capacity];
		memset(buckets, -1, sizeof(int32_t*) * this->capacity);

		entries = new StringHashEntry<T>[this->capacity];
		memset(entries, 0, sizeof(StringHashEntry<T>) * this->capacity);
	}
}

template<class T>
StringHash<T>::~StringHash()
{
	if (buckets)
	{
		capacity = 0;
		count = 0;
		delete[] buckets;
		delete[] entries;
		buckets = nullptr;
		entries = nullptr;
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

	int32_t bucket = hashCode % capacity;

	for (int32_t i = buckets[bucket]; i >= 0; i = entries[i].next)
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

	int32_t index = count++;

	StringHashEntry<T> *e = entries + index;
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
		int32_t bucket = hashCode % capacity;

		for (int32_t i = buckets[bucket]; i >= 0; i = entries[i].next)
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
int32_t StringHash<T>::GetCount() const
{
	return count;
}

template<class T>
bool StringHash<T>::GetByIndex(int32_t index, T &result) const
{
	if (index < 0 || index >= count)
		return false;
	result = entries[index].value;
	return true;
}

template<class T>
void StringHash<T>::FreeValues()
{
	for (int32_t i = 0; i < count; i++)
	{
		free(entries[i].value);
		entries[i].value = nullptr;
	}
}

template<class T>
void StringHash<T>::DeleteValues()
{
	for (int32_t i = 0; i < count; i++)
	{
		delete entries[i].value;
		entries[i].value = nullptr;
	}
}

template<class T>
void StringHash<T>::DeleteArrayValues()
{
	for (int32_t i = 0; i < count; i++)
	{
		delete[] entries[i].value;
		entries[i].value = nullptr;
	}
}

} // namespace ovum
