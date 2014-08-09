#pragma once

#ifndef VM__PATHNAME_INTERNAL_H
#define VM__PATHNAME_INTERNAL_H

#include "ov_vm.internal.h"
#include <new>

namespace ovum
{

// This header file exports a class PathName, which
// is a mutable sequence of pathchar_t. It supports
// various path name manipulation methods.

class PathName
{
private:
	typedef struct { } noinit_t;
	static const noinit_t noinit;

	static const pathchar_t ZERO = _Path('\0');

	// Pointer to the first character in the string
	pathchar_t *data;
	// Number of characters actually used
	uint32_t length;
	// Total size of character array
	uint32_t capacity;

	// No-init constructor; just sets all fields to their defaults
	PathName(noinit_t);

public:
	explicit PathName(const pathchar_t *const path);
	explicit PathName(uint32_t capacity);
	explicit PathName(String *path);
	PathName(const PathName &other);

	PathName(const pathchar_t *const path, std::nothrow_t);
	PathName(uint32_t capacity, std::nothrow_t);
	PathName(String *path, std::nothrow_t);
	PathName(const PathName &other, std::nothrow_t);

	~PathName();

	inline bool IsValid() const
	{
		return data != nullptr;
	}

	inline uint32_t GetLength() const
	{
		return length;
	}

	inline uint32_t GetCapacity() const
	{
		return capacity;
	}

	inline pathchar_t *GetDataPointer()
	{
		return data;
	}
	inline const pathchar_t *GetDataPointer() const
	{
		return data;
	}

	// Determines whether the path is rooted, that is,
	// the path is absolute.
	// Examples: C:\Hello, /usr/bin
	inline bool IsRooted() const
	{
		return IsRooted(length, data);
	}

	// Appends the characters of another path to this instance as-is.
	// Returns: The length of the path after appending.
	inline uint32_t Append(const PathName &other)
	{
		return AppendInner(other.length, other.data);
	}
	inline uint32_t Append(const pathchar_t *path)
	{
		return AppendInner(StringLength(path), path);
	}
	inline uint32_t Append(String *path)
	{
#if OVUM_TARGET == OVUM_WINDOWS
		return AppendInner(path->length, reinterpret_cast<const pathchar_t*>(&path->firstChar));
#else
#error Not implemented
#endif
	}

	// Joins this path with another, which is done as follows:
	//   * If the other path is rooted, this path is replaced by
	//     the other path.
	//   * Otherwise, the other path's characters are added to
	//     this path, separated by a PATH_SEP if this path does
	//     not end in one.
	// Returns: The length of the path after joining.
	inline uint32_t Join(const PathName &other)
	{
		return JoinInner(other.length, other.data);
	}
	inline uint32_t Join(const pathchar_t *path)
	{
		return JoinInner(StringLength(path), path);
	}
	inline uint32_t Join(String *path)
	{
#if OVUM_TARGET == OVUM_WINDOWS
		return JoinInner(path->length, reinterpret_cast<const pathchar_t*>(&path->firstChar));
#else
#error Not implemented
#endif
	}

	inline uint32_t RemoveFileName()
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

	inline void Clear()
	{
		length = 0;
		data[0] = ZERO;
	}

	inline void ReplaceWith(const PathName &other)
	{
		Clear();
		ReplaceWith(other.length, other.data);
	}
	inline void ReplaceWith(const pathchar_t *path)
	{
		Clear();
		ReplaceWith(StringLength(path), path);
	}
	inline void ReplaceWith(String *path)
	{
		Clear();
#if OVUM_TARGET == OVUM_WINDOWS
		ReplaceWith(path->length, reinterpret_cast<const pathchar_t*>(&path->firstChar));
#else
#error Not implemented
#endif
	}

	// Clips the path name to the specified substring, removing
	// characters that are outside that range.
	// Returns: The length of the string after clipping.
	inline uint32_t ClipTo(uint32_t index, uint32_t length)
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

	String *ToManagedString(ThreadHandle thread) const;

private:
	void Init(uint32_t capacity);
	void Init(uint32_t capacity, std::nothrow_t);

	bool EnsureMinCapacity(uint32_t minCapacity);

	void ReplaceWith(uint32_t length, const pathchar_t *path);

	inline uint32_t AppendInner(uint32_t count, const pathchar_t *path)
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

	inline uint32_t JoinInner(uint32_t count, const pathchar_t *path)
	{
		if (IsRooted(count, path))
		{
			this->ReplaceWith(count, path);
		}
		else
		{
			pathchar_t last = this->data[length - 1];
			bool needSep = !IsPathSep(last);

			if (!EnsureMinCapacity(length + count + (needSep ? 1 : 0)))
				throw std::bad_alloc();

			if (needSep)
				this->data[length++] = PATH_SEPC;
			CopyMemoryT(this->data + length, path, count); // +1 for \0
			length += count;
			this->data[length] = ZERO;
		}

		return this->length;
	}

	static inline bool IsPathSep(pathchar_t ch)
	{
		return ch == PATH_SEPC || ch == PATH_SEPC_ALT;
	}

	static bool IsRooted(uint32_t length, const pathchar_t *path);

	static uint32_t GetRootLength(uint32_t length, const pathchar_t *path);

	static uint32_t StringLength(const pathchar_t *const str);
};

} // namespace ovum

#endif // VM__PATHNAME_INTERNAL_H