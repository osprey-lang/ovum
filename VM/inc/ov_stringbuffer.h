#pragma once

#ifndef VM__STRINGBUFFER_H
#define VM__STRINGBUFFER_H

#include <memory>

#include "ov_vm.h"
#include "ov_unicode.h"

// This is basically the same as ov_stringbuffer.internal.h, except rewritten slightly to be
//   1. Fully inlined;
//   2. Independent of the internal classes and functions.

class StringBuffer
{
private:
	int32_t capacity;
	int32_t length;
	uchar *data;

	static const size_t DefaultCapacity = 128;

public:
	inline StringBuffer()
		 : length(0), data(nullptr)
	{ }
	inline ~StringBuffer()
	{
		// Note: don't use delete; data was allocated with realloc, not new
		if (data)
		{
			free(data);
			data = nullptr;
		}
	}

	inline bool Init(const int32_t capacity = DefaultCapacity)
	{
		return SetCapacity(capacity);
	}

	inline int32_t GetLength()   { return this->length; }
	inline int32_t GetCapacity() { return this->capacity; }
	inline bool SetCapacity(const int32_t newCapacity)
	{
		int32_t newCap = newCapacity;
		if (newCap < this->length)
			newCap = this->length;

		if (newCap > SIZE_MAX / sizeof(uchar))
			return false;

		uchar *newData = reinterpret_cast<uchar*>(realloc(data, sizeof(uchar) * newCap));
		if (newData == nullptr)
			return false;

		this->data = newData;
		this->capacity = newCap;
		return true;
	}

	inline bool Append(const uchar ch)
	{
		return Append(1, &ch);
	}
	inline bool Append(const int32_t count, const uchar ch)
	{
		if (!EnsureMinCapacity(count)) return false;

		uchar *chp = this->data + this->length;
		for (int32_t i = 0; i < count; i++)
		{
			*chp = ch;
			chp++;
		}
		this->length += count;
		return true;
	}
	inline bool Append(const int32_t length, const uchar data[])
	{
		if (!EnsureMinCapacity(length)) return false;

		CopyMemoryT(this->data + this->length, data, length);
		this->length += length;
		return true;
	}
	inline bool Append(String *str)
	{
		return Append(str->length, &str->firstChar);
	}

	inline bool Append(const int32_t length, const char data[])
	{
		if (!EnsureMinCapacity(length)) return false;

		uchar *chp = this->data + this->length;
		for (int32_t i = 0; i < length; i++)
		{
			*chp = data[i];
			chp++;
		}
		this->length += length;
		return true;
	}

	inline bool Insert(const int32_t index, const int32_t length, const uchar data[])
	{
		if (length > 0)
		{
			if (!EnsureMinCapacity(length)) return false;

			uchar *destp = this->data + this->length + length - 1;
			const uchar *srcp = this->data + this->length - 1;

			int32_t remaining = this->length - index;
			while (remaining--)
				*destp-- = *srcp--;

			CopyMemoryT(this->data + index, data, length);
		}
		return true;
	}
	inline bool Insert(const int32_t index, const uchar data)
	{
		return Insert(index, 1, &data);
	}
	inline bool Insert(const int32_t index, String *str)
	{
		return Insert(index, str->length, &str->firstChar);
	}

	// Clears the buffer's contents without changing the capacity.
	inline void Clear()
	{
		this->length = 0;
	}

	inline bool StartsWith(const uchar ch)
	{
		return this->length > 0 && this->data[0] == ch;
	}
	inline bool EndsWith(const uchar ch)
	{
		return this->length > 0 && this->data[this->length - 1] == ch;
	}

	inline String *ToString(ThreadHandle thread)
	{
		return GC_ConstructString(thread, this->length, this->data);
	}

	// If buf is null, returns only the size of the resulting string,
	// including the terminating \0.
	inline const int ToWString(wchar_t *buf)
	{
		// This is basically copied straight from String_ToWString, but optimized for StringBuffer.
#if OVUM_WCHAR_SIZE == 2
		// UTF-16 (or at least UCS-2, but hopefully surrogates won't break things too much)

		int outputLength = this->length; // Do NOT include the \0

		if (buf)
		{
			memcpy(buf, this->data, outputLength * sizeof(uchar));
			*(buf + outputLength) = L'\0'; // Add the \0
		}

		return outputLength + 1; // Do include \0
#elif OVUM_WCHAR_SIZE == 4
		// UTF-32

		// First, iterate over the string to find out how many surrogate pairs there are,
		// if any. These consume only one UTF-32 character.
		// We use this to calculate the length of the output (including the \0).
		int outputLength = 0;

		int32_t strLen = this->length; // let's NOT include the \0
		const uchar *strp = this->data;
		for (int32_t i = 0; i < strLen; i++)
		{
			if (UC_IsSurrogateLead(*strp) && UC_IsSurrogateTrail(*(strp + 1)))
			{
				i++; // skip one extra character; surrogate pair still only makes one wchar_t
				strp++;
			}
			outputLength++;
			strp++;
		}

		if (buf)
		{
			// And now we can copy things to the destination, yay
			strp = this->data;

			wchar_t *outp = buf;
			for (int i = 0; i < outputLength; i++)
			{
				if (UC_IsSurrogateLead(*strp) && UC_IsSurrogateTrail(*(strp + 1)))
				{
					*outp = UC_ToWide(*strp, *(strp + 1));
					strp++; // skip one extra character in source
				}
				else
					*outp = *strp;

				strp++;
				outp++;
			}

			// outp is now one character beyond the end of the string
			*outp = L'\0'; // and add \0
		}

		return outputLength + 1; // Do include \0
#else
#error Not supported
#endif
	}

private:
	inline bool EnsureMinCapacity(int32_t newAmount)
	{
		if (INT32_MAX - newAmount < this->length)
			return false;

		if (this->length + newAmount > this->capacity)
		{
			// Double the capacity, but make sure newAmount will actually fit too
			int32_t newLength = this->length << 1;
			if (newLength < this->length + newAmount)
				newLength += newAmount;
			return SetCapacity(newLength);
		}
		return true;
	}
};

#endif // VM__STRINGBUFFER_H