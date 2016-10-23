#pragma once

#include "../vm.h"
#include "module.h"
#include "modulefile.h"

namespace ovum
{

template<class T>
class MemberTableBase
{
public:
	// The total number of slots
	int32_t capacity;
	// The total number of entries (that is, number of used slots).
	int32_t length;
	Box<T[]> entries;

	inline MemberTableBase() :
		capacity(0),
		length(0),
		entries(nullptr)
	{ }

	inline void Init(const int32_t capacity)
	{
		// Init should only be called with a non-zero capacity once
		OVUM_ASSERT(this->capacity == 0);

		this->capacity = capacity;
		if (capacity != 0)
			this->entries = Box<T[]>(new T[capacity]);
		else
			this->entries = nullptr;
	}

	inline bool HasItem(const int32_t index) const
	{
		return index >= 0 && index < length;
	}
};

template<class T>
class MemberTable : private MemberTableBase<T>
{
public:
	inline MemberTable(const int32_t capacity) :
		MemberTableBase()
	{
		Init(capacity);
	}
	inline MemberTable() :
		MemberTableBase()
	{ }

	inline T &operator[](const int32_t index) const
	{
		return entries[index];
	}

	inline const int32_t GetLength() const
	{
		return length;
	}

	inline const int32_t GetCapacity() const
	{
		return capacity;
	}

private:
	inline void Init(const int32_t capacity)
	{
		this->MemberTableBase::Init(capacity);
	}

	inline void Add(T &&item)
	{
		entries[length] = std::move(item);
		length++;
	}

	friend class Module;
	friend class ModuleReader;
};

template<class T>
class MemberTable<T*> : private MemberTableBase<T*>
{
public:
	inline MemberTable(const int32_t capacity) :
		MemberTableBase()
	{
		Init(capacity);
	}
	inline MemberTable() :
		MemberTableBase()
	{ }

	inline T *operator[](const int32_t index) const
	{
		if (!HasItem(index))
			return nullptr;
		return entries[index];
	}

	inline const int32_t GetLength() const
	{
		return length;
	}

	inline const int32_t GetCapacity() const
	{
		return capacity;
	}

private:
	inline void Init(const int32_t capacity)
	{
		this->MemberTableBase::Init(capacity);
	}

	inline void Add(T *item)
	{
		entries[length] = item;
		length++;
	}

	friend class Module;
	friend class ModuleReader;
};

} // namespace ovum
