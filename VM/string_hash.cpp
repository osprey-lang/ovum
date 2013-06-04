#include "ov_vm.internal.h"
#include "ov_string.h"

template<class T>
StringHash<T>::StringHash(int32_t capacity)
{
	if (capacity <= 0)
	{
		this->length = this->capacity = 0;
		this->buckets = NULL;
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
		buckets = NULL;
	}
}

template<class T>
bool StringHash<T>::Insert(String *key, T value, bool add)
{
	if (buckets == NULL)
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
	newEntry->next = buckets[bucket]; // may be NULL

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