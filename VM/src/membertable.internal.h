#pragma once

#ifndef VM__MEMBERTABLE_INTERNAL_H
#define VM__MEMBERTABLE_INTERNAL_H

#include "ov_vm.internal.h"

template<class T>
class MemberTable
{
private:
	int32_t capacity; // The total number of slots
	int32_t length; // The total number of entries
	T *entries;

	inline void Init(const int32_t capacity)
	{
		this->capacity = capacity;
		if (capacity != 0)
			this->entries = new T[capacity];
		else
			this->entries = nullptr;
	}

	inline void Add(const T item)
	{
		entries[length] = item;
		length++;
	}

	inline void DeleteEntries()
	{
		for (int32_t i = 0; i < length; i++)
			delete entries[i];
	}

	inline void FreeEntries()
	{
		for (int32_t i = 0; i < length; i++)
			free(entries[i]);
	}

public:
	inline MemberTable(const int32_t capacity)
		: length(0)
	{
		Init(capacity);
	}
	inline MemberTable()
	{
		Init(0);
	}

	inline ~MemberTable()
	{
#ifdef PRINT_DEBUG_INFO
		wprintf(L"Destroying member table\n");
#endif
		if (this->entries)
			delete[] this->entries;
#ifdef PRINT_DEBUG_INFO
		wprintf(L"Finished destroying member table\n");
#endif
	}

	inline T operator[](const int32_t index) const
	{
		if (index < 0 || index >= length)
			return nullptr; // niet gevonden
		return entries[index];
	}

	inline const int32_t GetLength() const { return length; }
	inline const int32_t GetCapacity() const { return capacity; }

	inline T *GetEntryPointer(const int32_t index) { return entries + index; }
	
	inline bool HasItem(const int32_t index) const
	{
		return index >= 0 && index < length;
	}

	inline TokenId GetNextId(TokenId mask) const
	{
		return (length + 1) | mask;
	}

	friend class Module;
};

#endif // VM__MEMBERTABLE_INTERNAL_H