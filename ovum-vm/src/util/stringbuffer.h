#pragma once

#include "../vm.h"

namespace ovum
{

class StringBuffer
{
private:
	int32_t capacity;
	int32_t length;
	ovchar_t *data;

public:
	StringBuffer(const int32_t capacity = StringBuffer::DefaultCapacity);
	~StringBuffer();

	inline int32_t GetLength()   { return this->length; }
	inline int32_t GetCapacity() { return this->capacity; }
	int32_t SetCapacity(const int32_t newCapacity);

	void Append(const ovchar_t ch);
	void Append(const int32_t count, const ovchar_t ch);
	void Append(const int32_t length, const ovchar_t data[]);
	void Append(String *str);

	void Append(const int32_t length, const char data[]);
#if OVUM_WCHAR_SIZE != 2
	void Append(const int32_t length, const wchar_t data[]);
#endif

	// Clears the buffer's contents without changing the capacity.
	void Clear();

	inline bool StartsWith(const ovchar_t ch)
	{
		return this->length > 0 && this->data[0] == ch;
	}
	inline bool EndsWith(const ovchar_t ch)
	{
		return this->length > 0 && this->data[this->length - 1] == ch;
	}

	String *ToString(Thread *const thread);

	// If buf is null, returns only the size of the resulting string,
	// including the terminating \0.
	int ToWString(wchar_t *buf);

private:
	void EnsureMinCapacity(int32_t newAmount);

	static const size_t DefaultCapacity = 128;
};

} // namespace ovum
