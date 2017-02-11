#pragma once

#include "../vm.h"

namespace ovum
{

class StringBuffer
{
public:
	explicit StringBuffer(size_t capacity = StringBuffer::DefaultCapacity);
	~StringBuffer();

	inline size_t GetLength()
	{
		return this->length;
	}

	inline size_t GetCapacity()
	{
		return this->capacity;
	}

	size_t SetCapacity(size_t newCapacity);

	void Append(ovchar_t ch);
	void Append(size_t count, const ovchar_t ch);
	void Append(size_t length, const ovchar_t data[]);
	void Append(String *str);

	void Append(size_t length, const char data[]);
#if OVUM_WCHAR_SIZE != 2
	void Append(size_t length, const wchar_t data[]);
#endif

	// Clears the buffer's contents without changing the capacity.
	void Clear();

	inline bool StartsWith(ovchar_t ch)
	{
		return this->length > 0 && this->data[0] == ch;
	}

	inline bool EndsWith(ovchar_t ch)
	{
		return this->length > 0 && this->data[this->length - 1] == ch;
	}

	String *ToString(Thread *const thread);

	// If buf is null, returns only the size of the resulting string,
	// including the terminating \0.
	size_t ToWString(wchar_t *buf);

private:
	size_t capacity;
	size_t length;
	ovchar_t *data;

	void EnsureMinCapacity(size_t newAmount);

	static const size_t DefaultCapacity = 128;
};

} // namespace ovum
