#include "pathname.h"
#include <memory>

namespace ovum
{

const PathName::noinit_t PathName::noinit = { };

PathName::PathName(noinit_t)
	: data(nullptr), length(0), capacity(0)
{ }

PathName::PathName(const pathchar_t *const path)
	: data(nullptr), length(0), capacity(0)
{
	uint32_t pathLength = StringLength(path);
	Init(pathLength);
	this->length = pathLength;
	CopyMemoryT(this->data, path, pathLength + 1); // +1 for \0
}
PathName::PathName(uint32_t capacity)
	: data(nullptr), length(0), capacity(0)
{
	Init(capacity);
	data[0] = ZERO;
}
PathName::PathName(String *path)
	: data(nullptr), length(0), capacity(0)
{
#if OVUM_WIDE_PATHCHAR
	Init(path->length);
	this->length = path->length;
	CopyMemoryT(this->data, reinterpret_cast<const pathchar_t*>(&path->firstChar), path->length);
#else
#error Not implemented
#endif
}
PathName::PathName(const PathName &other)
	: data(nullptr), length(0), capacity(0)
{
	Init(other.length);
	this->length = other.length;
	CopyMemoryT(this->data, other.data, other.length + 1); // +1 for \0
}

PathName::PathName(const pathchar_t *const path, std::nothrow_t)
	: data(nullptr), length(0), capacity(0)
{
	uint32_t pathLength = StringLength(path);
	Init(pathLength, std::nothrow);
	if (IsValid())
	{
		this->length = pathLength;
		CopyMemoryT(this->data, path, pathLength + 1); // +1 for \0
	}
}
PathName::PathName(uint32_t capacity, std::nothrow_t)
	: data(nullptr), length(0), capacity(0)
{
	Init(capacity, std::nothrow);
	if (IsValid())
		data[0] = ZERO;
}
PathName::PathName(String *path, std::nothrow_t)
	: data(nullptr), length(0), capacity(0)
{
#if OVUM_WIDE_PATHCHAR
	Init(path->length, std::nothrow);
	if (IsValid())
	{
		this->length = path->length;
		CopyMemoryT(this->data, reinterpret_cast<const pathchar_t*>(&path->firstChar), path->length);
	}
#else
#error Not implemented
#endif
}
PathName::PathName(const PathName &other, std::nothrow_t)
	: data(nullptr), length(0), capacity(0)
{
	Init(other.length, std::nothrow);
	if (IsValid())
	{
		this->length = other.length;
		CopyMemoryT(this->data, other.data, other.length + 1); // +1 for \0
	}
}

PathName::~PathName()
{
	delete[] this->data;
}

String *PathName::ToManagedString(ThreadHandle thread) const
{
#if OVUM_WIDE_PATHCHAR
	return thread->GetGC()->ConstructString(thread, length, reinterpret_cast<const uchar*>(data));
#else
#error Not implemented
#endif
}

void PathName::Init(uint32_t capacity)
{
	this->data = new pathchar_t[capacity + 1]; // +1 for \0
	this->capacity = capacity;
}
void PathName::Init(uint32_t capacity, std::nothrow_t)
{
	this->data = new(std::nothrow) pathchar_t[capacity + 1]; // +1 for \0
	if (IsValid())
		this->capacity = capacity;
}

bool PathName::EnsureMinCapacity(uint32_t minCapacity)
{
	using namespace std;

	if (this->capacity < minCapacity)
	{
		uint32_t newCap = max(minCapacity, this->capacity * 2);
		unique_ptr<pathchar_t[]> newValues(new(std::nothrow) pathchar_t[newCap]);
		if (newValues.get() == nullptr)
			return false;

		CopyMemoryT(newValues.get(), this->data, this->length);
		delete[] this->data;
		this->data = newValues.release();
		this->capacity = newCap;
	}

	return true;
}

uint32_t PathName::RemoveFileName()
{
	uint32_t root = GetRootLength(length, data);
	uint32_t i = length;
	while (i > root && !IsPathSep(data[--i]))
		;
	// i is now at the path separator, or at the last
	// character before the root. Time to truncate!
	data[i] = ZERO;
	length = i;
	return i;
}

uint32_t PathName::ClipTo(uint32_t index, uint32_t length)
{
	if (index >= this->length || length == 0)
	{
		this->Clear();
	}
	else if (index == 0)
	{
		this->length = min(this->length, length);
		data[this->length] = ZERO;
	}
	else
	{
		length = min(this->length - index, length);
		// Copy one character at a time instead of using CopyMemoryT,
		// to avoid potential problems with overlapping characters.
		for (uint32_t i = 0; i < length; i++)
			data[i] = data[index + i];
		data[length] = ZERO;
		this->length = length;
	}
	return this->length;
}

uint32_t PathName::AppendInner(uint32_t count, const pathchar_t *path)
{
	if (count > 0)
	{
		if (!EnsureMinCapacity(this->length + count))
			throw std::bad_alloc();

		CopyMemoryT(this->data + this->length, path, count);
		this->length += count;
		this->data[this->length] = ZERO;
	}

	return this->length;
}

uint32_t PathName::JoinInner(uint32_t count, const pathchar_t *path)
{
	if (IsRooted(count, path))
	{
		this->ReplaceWithInner(count, path);
	}
	else
	{
		pathchar_t last = this->data[length - 1];
		bool needSep = !IsPathSep(last);

		if (!EnsureMinCapacity(length + count + (needSep ? 1 : 0)))
			throw std::bad_alloc();

		if (needSep)
			this->data[length++] = OVUM_PATH_SEPC;
		CopyMemoryT(this->data + length, path, count); // +1 for \0
		length += count;
		this->data[length] = ZERO;
	}

	return this->length;
}

void PathName::ReplaceWithInner(uint32_t length, const pathchar_t *path)
{
	if (!EnsureMinCapacity(length))
		throw std::bad_alloc();

	CopyMemoryT(this->data, path, length);
	this->length = length;
	this->data[length] = ZERO;
}

bool PathName::IsRooted(uint32_t length, const pathchar_t *path)
{
	// Starts with path separator, e.g. /hello/nope
	if (length >= 1 && IsPathSep(path[0]))
		return true;

#if OVUM_WINDOWS
	// Windows only: volume label + ':', e.g. C:\One or C:Two
	if (length >= 2 && path[1] == OVUM_PATH(':'))
		return true;
#endif

	return false;
}

uint32_t PathName::GetRootLength(uint32_t length, const pathchar_t *path)
{
	uint32_t index = 0;

	if (length >= 1 && IsPathSep(path[0]))
	{
		index = 1;
	}
#if OVUM_WINDOWS
	// Windows only: volume label + ':'
	else if (length >= 2 && path[1] == OVUM_PATH(':'))
	{
		index = 2;
		// + optional path separator
		if (length >= 3 && IsPathSep(path[2]))
			index++;
	}
#endif

	return index;
}

uint32_t PathName::StringLength(const pathchar_t *const str)
{
#if OVUM_WIDE_PATHCHAR
	return (uint32_t)wcslen(str);
#else
	return (uint32_t)strlen(str);
#endif
}

} // namespace ovum