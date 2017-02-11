#ifndef OVUM__STRINGBUFFER_H
#define OVUM__STRINGBUFFER_H

#include <memory>

#include "ovum.h"

// This is basically the same as ov_stringbuffer.internal.h, except rewritten slightly to be
//   1. Fully inlined;
//   2. Independent of the internal classes and functions.

class StringBuffer
{
private:
	size_t capacity;
	size_t length;
	ovchar_t *data;

	static const size_t DefaultCapacity = 128;

public:
	inline StringBuffer() :
		length(0),
		data(nullptr)
	{ }
	inline ~StringBuffer()
	{
		// Note: don't use delete; data was allocated with realloc, not new
		free(data);
		data = nullptr;
	}

	inline bool Init(size_t capacity = DefaultCapacity)
	{
		return SetCapacity(capacity);
	}

	inline size_t GetLength() const
	{
		return this->length;
	}

	inline size_t GetCapacity() const
	{
		return this->capacity;
	}

	inline bool SetCapacity(size_t newCapacity)
	{
		size_t newCap = newCapacity;
		if (newCap < this->length)
			newCap = this->length;

		if (newCap > SIZE_MAX / sizeof(ovchar_t))
			return false;

		ovchar_t *newData = reinterpret_cast<ovchar_t*>(realloc(data, sizeof(ovchar_t) * newCap));
		if (newData == nullptr)
			return false;

		this->data = newData;
		this->capacity = newCap;
		return true;
	}

	inline ovchar_t *GetDataPointer() const
	{
		return this->data;
	}

	inline bool Append(ovchar_t ch)
	{
		return Append(1, &ch);
	}
	inline bool Append(size_t count, const ovchar_t ch)
	{
		if (!EnsureMinCapacity(count))
			return false;

		ovchar_t *chp = this->data + this->length;
		for (size_t i = 0; i < count; i++)
		{
			*chp = ch;
			chp++;
		}
		this->length += count;
		return true;
	}
	inline bool Append(size_t length, const ovchar_t data[])
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

	inline bool Append(size_t length, const char data[])
	{
		if (!EnsureMinCapacity(length)) return false;

		ovchar_t *chp = this->data + this->length;
		for (size_t i = 0; i < length; i++)
		{
			*chp = data[i];
			chp++;
		}
		this->length += length;
		return true;
	}

	inline bool Insert(size_t index, size_t length, const ovchar_t data[])
	{
		if (length > 0)
		{
			if (!EnsureMinCapacity(length)) return false;

			ovchar_t *destp = this->data + this->length + length - 1;
			const ovchar_t *srcp = this->data + this->length - 1;

			size_t remaining = this->length - index;
			while (remaining--)
				*destp-- = *srcp--;

			CopyMemoryT(this->data + index, data, length);
		}
		return true;
	}
	inline bool Insert(size_t index, ovchar_t data)
	{
		return Insert(index, 1, &data);
	}
	inline bool Insert(size_t index, String *str)
	{
		return Insert(index, str->length, &str->firstChar);
	}

	// Clears the buffer's contents without changing the capacity.
	inline void Clear()
	{
		this->length = 0;
	}

	inline bool StartsWith(ovchar_t ch) const
	{
		return this->length > 0 && this->data[0] == ch;
	}
	inline bool EndsWith(ovchar_t ch) const
	{
		return this->length > 0 && this->data[this->length - 1] == ch;
	}

	inline String *ToString(ThreadHandle thread) const
	{
		return GC_ConstructString(thread, this->length, this->data);
	}

	// If buf is null, returns only the size of the resulting string,
	// including the terminating \0.
	inline size_t ToWString(wchar_t *buf)
	{
		// This is basically copied straight from String_ToWString, but optimized for StringBuffer.
#if OVUM_WCHAR_SIZE == 2
		// UTF-16 (or at least UCS-2, but hopefully surrogates won't break things too much)

		size_t outputLength = this->length; // Do NOT include the \0

		if (buf)
		{
			memcpy(buf, this->data, outputLength * sizeof(ovchar_t));
			*(buf + outputLength) = L'\0'; // Add the \0
		}

		return outputLength + 1; // Do include \0
#elif OVUM_WCHAR_SIZE == 4
		// UTF-32

		// First, iterate over the string to find out how many surrogate pairs there are,
		// if any. These consume only one UTF-32 character.
		// We use this to calculate the length of the output (including the \0).
		size_t outputLength = 0;

		size_t strLen = this->length; // let's NOT include the \0
		const ovchar_t *strp = this->data;
		for (size_t i = 0; i < strLen; i++)
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
			for (size_t i = 0; i < outputLength; i++)
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
	inline bool EnsureMinCapacity(size_t newAmount)
	{
		if (INT32_MAX - newAmount < this->length)
			return false;

		if (this->length + newAmount > this->capacity)
		{
			// Double the capacity, but make sure newAmount will actually fit too
			size_t newLength = this->length << 1;
			if (newLength < this->length + newAmount)
				newLength += newAmount;
			return SetCapacity(newLength);
		}
		return true;
	}
};

#endif // OVUM__STRINGBUFFER_H
