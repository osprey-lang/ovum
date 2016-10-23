#pragma once

#include "../vm.h"
#include "module.h"
#include "modulefile.h"

namespace ovum
{

template<class T>
class MemberTable
{
private:
	int32_t capacity; // The total number of slots
	int32_t length; // The total number of entries
	T *entries;

public:
	inline MemberTable(const int32_t capacity)
		: capacity(0), length(0), entries(nullptr)
	{
		Init(capacity);
	}
	inline MemberTable()
		: capacity(0), length(0), entries(nullptr)
	{ }

	inline ~MemberTable()
	{
		delete[] this->entries;
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

	inline Token GetNextId(Token mask) const
	{
		return (length + 1) | mask;
	}

private:
	inline void Init(const int32_t capacity)
	{
		// Init should only be called with a non-zero capacity once
		OVUM_ASSERT(this->capacity == 0);

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

	friend class Module;
	friend class ModuleReader;
};

} // namespace ovum
