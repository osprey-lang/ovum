#pragma once

#ifndef VM__STRING_HASH_INTERNAL_H
#define VM__STRING_HASH_INTERNAL_H

#include "ov_vm.internal.h"
#include "ov_string.h"

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
	int32_t length;
	StringHashEntry<T> **buckets;

	bool Insert(String *key, T value, bool add);

public:
	StringHash(int32_t capacity);
	~StringHash();

	bool Get(String *key, T &value) const;
	bool Add(String *key, T value);
	bool Set(String *key, T value);
};

template<class T>
class StringHashEntry
{
public:
	int32_t hashCode;
	String *key;
	T value;
	StringHashEntry *next;
};


template<class T>
StringHash<T>::StringHash(int32_t capacity)
{
	if (capacity <= 0)
	{
		this->length = this->capacity = 0;
		this->buckets = nullptr;
	}
	else
	{
		this->length = 0;
		this->capacity = capacity;
		this->buckets = new StringHashEntry<T>*[NextPowerOfTwo(capacity)];
	}
}

template<class T>
StringHash<T>::~StringHash()
{
	// Note: String is a managed type. We do not need to delete the keys.
	if (buckets)
	{
		for (int i = 0; i < capacity; i++)
		{
			StringHashEntry<T> *next = buckets[i];
			while (next)
			{
				StringHashEntry<T> *e = next;
				next = e->next;
				delete e;
			}
		}

		delete[] buckets;
		buckets = nullptr;
	}
}

template<class T>
bool StringHash<T>::Insert(String *key, T value, bool add)
{
	if (buckets == nullptr)
		// If you've said you're not going to put anything in the hash,
		// you don't get to put anything in it.
		return false;

	int32_t hash = String_GetHashCode(key);

	int32_t bucket = hash % capacity;

	for (StringHashEntry<T> *e = buckets[bucket]; e; e = e->next)
		if (e->hashCode == hash && String_Equals(e->key, key))
		{
			if (add)
				return false; // NOPE
			e->value = value;
			return true;
		}

	// Not found - let's insert a new entry!

	// First, create it.
	StringHashEntry<T> *newEntry = new StringHashEntry<T>();
	newEntry->hashCode = hash;
	newEntry->key = key;
	newEntry->value = value;
	newEntry->next = buckets[bucket]; // may be null

	// Then update the bucket. Whoo!
	buckets[bucket] = newEntry;
	return true;
}

template<class T>
bool StringHash<T>::Get(String *key, T &value) const
{
	if (buckets)
	{
		int32_t hash = String_GetHashCode(key);
		int32_t bucket = hash % capacity;

		for (StringHashEntry<T> *e = buckets[bucket]; e; e = e->next)
			if (e->hashCode == hash && String_Equals(e->key, key))
			{
				value = e->value;
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

#endif // VM__STRING_HASH_INTERNAL_H