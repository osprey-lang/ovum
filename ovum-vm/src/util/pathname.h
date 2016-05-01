#pragma once

#include "../vm.h"
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

	static const pathchar_t ZERO = OVUM_PATH('\0');

	// Size of the buffer used when re-encoding a path as UTF-8.
	// Paths are extremely unlikely to be longer than this; there
	// is simply no need for a larger buffer.
	static const int32_t UTF8_BUFFER_SIZE = 128;

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
		return AppendOvchar((uint32_t)path->length, &path->firstChar);
	}
	inline uint32_t Append(uint32_t length, const pathchar_t *path)
	{
		return AppendInner(length, path);
	}
#if !OVUM_WIDE_PATHCHAR
	inline uint32_t Append(uint32_t length, const ovchar_t *path)
	{
		return AppendOvchar(length, path);
	}
#endif

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
		return JoinOvchar((uint32_t)path->length, &path->firstChar);
	}
	inline uint32_t Join(uint32_t length, const pathchar_t *path)
	{
		return JoinInner(length, path);
	}
#if !OVUM_WIDE_PATHCHAR
	inline uint32_t Join(uint32_t length, const ovchar_t *path)
	{
		return JoinOvchar(length, path);
	}
#endif

	uint32_t RemoveFileName();

	inline void Clear()
	{
		length = 0;
		data[0] = ZERO;
	}

	inline void ReplaceWith(const PathName &other)
	{
		Clear();
		ReplaceWithInner(other.length, other.data);
	}
	inline void ReplaceWith(const pathchar_t *path)
	{
		Clear();
		ReplaceWithInner(StringLength(path), path);
	}
	inline void ReplaceWith(String *path)
	{
		Clear();
		ReplaceWithOvchar((uint32_t)path->length, &path->firstChar);
	}
	inline void ReplaceWith(uint32_t length, const pathchar_t *path)
	{
		Clear();
		ReplaceWithInner(length, path);
	}
#if !OVUM_WIDE_PATHCHAR
	inline void ReplaceWith(uint32_t length, const ovchar_t *path)
	{
		Clear();
		ReplaceWithOvchar(length, path);
	}
#endif

	// Clips the path name to the specified substring, removing
	// characters that are outside that range.
	// Returns: The length of the string after clipping.
	uint32_t ClipTo(uint32_t index, uint32_t length);

	String *ToManagedString(ThreadHandle thread) const;

private:
	void Init(uint32_t capacity);
	void Init(uint32_t capacity, std::nothrow_t);

	bool EnsureMinCapacity(uint32_t minCapacity);

	void ReplaceWithInner(uint32_t length, const pathchar_t *path);

	void ReplaceWithOvchar(uint32_t length, const ovchar_t *path);

	uint32_t AppendInner(uint32_t length, const pathchar_t *path);

	uint32_t AppendOvchar(uint32_t length, const ovchar_t *path);

	uint32_t JoinInner(uint32_t length, const pathchar_t *path);

	uint32_t JoinOvchar(uint32_t length, const ovchar_t *path);

	static inline bool IsPathSep(pathchar_t ch)
	{
		return ch == OVUM_PATH_SEPC || ch == OVUM_PATH_SEPC_ALT;
	}

	static bool IsRooted(uint32_t length, const pathchar_t *path);

	static uint32_t GetRootLength(uint32_t length, const pathchar_t *path);

	static uint32_t StringLength(const pathchar_t *const str);
};

} // namespace ovum
