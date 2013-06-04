#pragma once

#ifndef VM__STRINGBUFFER_H
#define VM__STRINGBUFFER_H

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

public:
	inline StringBuffer(ThreadHandle thread, const int32_t capacity = StringBuffer::DefaultCapacity)
		 : length(0), data(NULL)
	{
		SetCapacity(thread, capacity);
	}
	inline ~StringBuffer()
	{
		// Note: don't use delete; data was allocated with realloc, not new
		if (data)
		{
			free(data);
			data = NULL;
		}
	}

	inline int32_t GetLength()   { return this->length; }
	inline int32_t GetCapacity() { return this->capacity; }
	inline int32_t SetCapacity(ThreadHandle thread, const int32_t newCapacity)
	{
		int32_t newCap = newCapacity;
		if (newCap < this->length)
			newCap = this->length;

		if (newCap > SIZE_MAX / sizeof(uchar))
			VM_ThrowMemoryError(thread);

		uchar *newData = reinterpret_cast<uchar*>(realloc(data, sizeof(uchar) * newCap));
		if (newData == NULL)
			VM_ThrowMemoryError(thread);

		this->data = newData;
		this->capacity = newCap;
		return newCap;
	}

	inline void Append(ThreadHandle thread, const uchar ch)
	{
		Append(thread, 1, &ch);
	}
	inline void Append(ThreadHandle thread, const int32_t count, const uchar ch)
	{
		EnsureMinCapacity(thread, count);

		uchar *chp = this->data + this->length;
		for (int32_t i = 0; i < count; i++)
		{
			*chp = ch;
			chp++;
		}
		this->length += count;
	}
	inline void Append(ThreadHandle thread, const int32_t length, const uchar data[])
	{
		EnsureMinCapacity(thread, length);

		CopyMemoryT(this->data + this->length, data, length);
		this->length += length;
	}
	inline void Append(ThreadHandle thread, String *str)
	{
		Append(thread, str->length, &str->firstChar);
	}

	inline void Append(ThreadHandle thread, const int32_t length, const char data[])
	{
		EnsureMinCapacity(thread, length);

		uchar *chp = this->data + this->length;
		for (int32_t i = 0; i < length; i++)
		{
			*chp = data[i];
			chp++;
		}
		this->length += length;
	}
	inline void Append(ThreadHandle thread, const int32_t length, const wchar_t data[])
	{
		if (sizeof(wchar_t) == sizeof(uchar))
			// Assume wchar_t is UTF-16 (or at least UCS-2)
			Append(thread, length, (uchar*)data);
		else if (sizeof(wchar_t) == sizeof(wuchar))
		{
			// Assume wchar_t is UTF-32
			EnsureMinCapacity(thread, length);

			for (int32_t i = 0; i < length; i++)
			{
				const wuchar ch = (wuchar)data[i];
				if (UC_NeedsSurrogatePair(ch))
				{
					const SurrogatePair surr = UC_ToSurrogatePair(ch);
					Append(thread, 2, (uchar*)&surr);
				}
				else
					Append(thread, 1, (uchar*)&ch);
			}
		}
		else
			VM_ThrowError(thread);
	}

	// Clears the buffer's contents without changing the capacity.
	inline void Clear()
	{
		if (this->length)
		{
			memset(this->data, 0, sizeof(uchar) * this->capacity);
			this->length = 0;
		}
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
		String *output;
		GC_ConstructString(thread, this->length, this->data, &output);
		return output;
	}

	// If buf is NULL, returns only the size of the resulting string,
	// including the terminating \0.
	inline const int ToWString(wchar_t *buf)
	{
		// This is basically copied straight from String_ToWString, but optimized for StringBuffer.

		if (sizeof(wchar_t) == sizeof(uchar))
		{
			// assume wchar_t is UTF-16 (or at least UCS-2, but hopefully surrogates won't break things too much)
			int outputLength = this->length; // Do NOT include the \0

			if (buf)
			{
				memcpy(buf, this->data, outputLength * sizeof(uchar));
				*(buf + outputLength) = L'\0'; // Add the \0
			}

			return outputLength + 1; // Do include \0
		}
		else if (sizeof(wchar_t) == sizeof(wuchar))
		{
			// assume sizeof(wchar_t) == 4, which is probably UTF-32

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
		}
		else
			return -1; // not supported
	}

private:
	inline void EnsureMinCapacity(ThreadHandle thread, int32_t newAmount)
	{
		if (INT32_MAX - newAmount < this->length)
			VM_ThrowOverflowError(thread);

		if (this->length + newAmount > this->capacity)
		{
			int32_t newLength = this->length + newAmount ;
			int32_t remainder = newLength % CapacityIncrement;
			SetCapacity(thread, newLength + (remainder ? CapacityIncrement - remainder : 0));
		}
	}

	static const size_t DefaultCapacity = 16;
	static const size_t CapacityIncrement = 32;
};

#endif // VM__STRINGBUFFER_H