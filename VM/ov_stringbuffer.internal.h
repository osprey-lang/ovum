#pragma once

#ifndef VM__STRINGBUFFER_INTERNAL_H
#define VM__STRINGBUFFER_INTERNAL_H

#include "ov_vm.internal.h"

class StringBuffer
{
private:
	int32_t capacity;
	int32_t length;
	uchar *data;

public:
	StringBuffer(Thread *const thread, const int32_t capacity = StringBuffer::DefaultCapacity);
	~StringBuffer();

	inline int32_t GetLength()   { return this->length; }
	inline int32_t GetCapacity() { return this->capacity; }
	int32_t SetCapacity(Thread *const thread, const int32_t newCapacity);

	void Append(Thread *const thread, const uchar ch);
	void Append(Thread *const thread, const int32_t count, const uchar ch);
	void Append(Thread *const thread, const int32_t length, const uchar data[]);
	void Append(Thread *const thread, String *str);

	void Append(Thread *const thread, const int32_t length, const char data[]);
	void Append(Thread *const thread, const int32_t length, const wchar_t data[]);

	// Clears the buffer's contents without changing the capacity.
	void Clear();

	inline bool StartsWith(const uchar ch)
	{
		return this->length > 0 && this->data[0] == ch;
	}
	inline bool EndsWith(const uchar ch)
	{
		return this->length > 0 && this->data[this->length - 1] == ch;
	}

	String *ToString(Thread *const thread);

	// If buf is NULL, returns only the size of the resulting string,
	// including the terminating \0.
	const int ToWString(wchar_t *buf);

private:
	void EnsureMinCapacity(Thread *const thread, int32_t newAmount);

	static const size_t DefaultCapacity = 16;
	static const size_t CapacityIncrement = 32;
};

#endif // VM__STRINGBUFFER_INTERNAL_H