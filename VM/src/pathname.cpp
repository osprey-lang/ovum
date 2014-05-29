#include "pathname.internal.h"
#include <memory>

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
#if TARGET_OS == TARGET_WINDOWS
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
#if TARGET_OS == TARGET_WINDOWS
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
#if TARGET_OS == TARGET_WINDOWS
	return GC::gc->ConstructString(thread, length, reinterpret_cast<const uchar*>(data));
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

void PathName::ReplaceWith(uint32_t length, const pathchar_t *path)
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
	if (length > 0 && IsPathSep(path[0]))
		return true;

#if TARGET_OS == TARGET_WINDOWS
	// Windows only: volume label + colon, e.g. C:\One or C:Two
	if (length >= 2 && path[1] == _Path(':'))
		return true;
#endif

	return false;
}

uint32_t PathName::GetRootLength(uint32_t length, const pathchar_t *path)
{
	uint32_t index = 0;

#if TARGET_OS == TARGET_WINDOWS
	if (length > 1 && IsPathSep(path[0]))
	{
		index = 1;
	}
	else if (length >= 2 && path[1] == _Path(':'))
	{
		// Volume label + ':'
		index = 2;
		if (length >= 3 && IsPathSep(path[2]))
			index++;
	}
#else
	if (length > 0 && (path[0] == PATH_SEPC || path[0] == PATH_SEPC_ALT))
		index = 1;
#endif

	return index;
}

uint32_t PathName::StringLength(const pathchar_t *const str)
{
#if TARGET_OS == TARGET_WINDOWS
	return (uint32_t)wcslen(str);
#else
	return (uint32_t)strlen(str);
#endif
}