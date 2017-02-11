#pragma once

#include "../vm.h"
#include "pathname.h"
#include <cstdlib>

namespace ovum
{

// Provides basic integer-to-string conversion functionality.
class IntFormatter
{
public:
	static inline size_t ToDec(int32_t number, StringBuffer &dest, size_t minLength = 0)
	{
		return ToDec((int64_t)number, dest, minLength);
	}

	static inline size_t ToDec(uint32_t number, StringBuffer &dest, size_t minLength = 0)
	{
		return ToDec((uint64_t)number, dest, minLength);
	}

	static size_t ToDec(int64_t number, StringBuffer &dest, size_t minLength = 0);

	static size_t ToDec(uint64_t number, StringBuffer &dest, size_t minLength = 0);

	static inline size_t ToDec(int32_t number, ovchar_t *dest, size_t destSize)
	{
		return ToDec((int64_t)number, dest, destSize);
	}

	static inline size_t ToDec(uint32_t number, ovchar_t *dest, size_t destSize)
	{
		return ToDec((uint64_t)number, dest, destSize);
	}

	static size_t ToDec(int64_t number, ovchar_t *dest, size_t destSize);

	static size_t ToDec(uint64_t number, ovchar_t *dest, size_t destSize);

	static inline size_t ToHex(uint32_t number, StringBuffer &dest, bool upper, size_t minLength = 0)
	{
		return ToHex((uint64_t)number, dest, upper, minLength);
	}

	static inline size_t ToHex(uint32_t number, ovchar_t *dest, size_t destSize, bool upper)
	{
		return ToHex((uint64_t)number, dest, destSize, upper);
	}

	static size_t ToHex(uint64_t number, StringBuffer &dest, bool upper, size_t minLength = 0);

	static size_t ToHex(uint64_t number, ovchar_t *dest, size_t destSize, bool upper);

private:
	// These functions will never be called with anything larger than 64 bits.
	// UINT64_MAX = 18446744073709551615 = 20 characters. INT64_MIN is also
	// 20 characters. Hence, a 32-char buffer will do just fine - that is,
	// unless the minLength specifies something larger. In that case, we will
	// have to use a heap buffer.
	static const size_t BUFFER_SIZE = 32;

	static const ovchar_t ZERO = (ovchar_t)'0';
	static const ovchar_t MINUS = (ovchar_t)'-';

	static const ovchar_t HEX_LOWER_BASE = (ovchar_t)'a';
	static const ovchar_t HEX_UPPER_BASE = (ovchar_t)'A';

	static size_t BuildDecString(uint64_t number, ovchar_t *destEnd);

	static size_t BuildHexString(uint64_t number, ovchar_t *destEnd, bool upper);
};

} // namespace ovum
