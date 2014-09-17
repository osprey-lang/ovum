#include "ov_stringbuffer.internal.h"
#include "ov_unicode.internal.h"

namespace ovum
{

namespace buffer_errors
{
	LitString<70> _MemoryError = LitString<70>::FromCString("There was not enough memory to increase the size of the string buffer.");

	String *MemoryError = _S(_MemoryError);
}

StringBuffer::StringBuffer(const int32_t capacity) : length(0), data(nullptr)
{
	SetCapacity(capacity);
}
StringBuffer::~StringBuffer()
{
	// Note: don't use delete; data was allocated with realloc, not new
	if (data)
	{
		free(data);
		data = nullptr;
	}
}

int32_t StringBuffer::SetCapacity(const int32_t newCapacity)
{
	int32_t newCap = newCapacity;
	if (newCap < this->length)
		newCap = this->length;

	if (newCap > SIZE_MAX / sizeof(uchar))
		throw std::bad_alloc();

	uchar *newData = reinterpret_cast<uchar*>(realloc(data, sizeof(uchar) * newCap));
	if (newData == nullptr)
		throw std::bad_alloc();

	this->data = newData;
	this->capacity = newCap;
	return newCap;
}

void StringBuffer::EnsureMinCapacity(int32_t newAmount)
{
	if (INT32_MAX - newAmount < this->length)
		throw std::exception("Could not resize string buffer: an overflow occurred.");

	if (this->length + newAmount > this->capacity)
	{
		// Double the capacity, but make sure newAmount will actually fit too
		int32_t newLength = this->length << 1;
		if (newLength < this->length + newAmount)
			newLength += newAmount;
		SetCapacity(newLength);
	}
}

void StringBuffer::Append(const int32_t length, const uchar data[])
{
	EnsureMinCapacity(length);

	CopyMemoryT(this->data + this->length, data, length);
	this->length += length;
}

void StringBuffer::Append(const int32_t count, const uchar ch)
{
	EnsureMinCapacity(count);

	uchar *chp = this->data + this->length;
	for (int32_t i = 0; i < count; i++)
	{
		*chp = ch;
		chp++;
	}
	this->length += count;
}

void StringBuffer::Append(String *str)
{
	// Just pass it on! Whee!
	Append(str->length, &str->firstChar);
}

void StringBuffer::Append(const uchar ch)
{
	// And this too! Whee!
	Append(1, &ch);
}

void StringBuffer::Append(const int32_t length, const char data[])
{
	EnsureMinCapacity(length);

	uchar *chp = this->data + this->length;
	for (int32_t i = 0; i < length; i++)
	{
		*chp = data[i];
		chp++;
	}
	this->length += length;
}

#if OVUM_WCHAR_SIZE != 2
void StringBuffer::Append(const int32_t length, const wchar_t data[])
{
#if OVUM_WCHAR_SIZE == 4
	// UTF-32

	EnsureMinCapacity(length);

	for (int32_t i = 0; i < length; i++)
	{
		const wuchar ch = (wuchar)data[i];
		if (UC_NeedsSurrogatePair(ch))
		{
			const SurrogatePair surr = UC_ToSurrogatePair(ch);
			Append(2, (uchar*)&surr);
		}
		else
			Append(1, (uchar*)&ch);
	}
#else
#error Not supported
#endif
}
#endif

void StringBuffer::Clear()
{
	this->length = 0;
}

String *StringBuffer::ToString(Thread *const thread)
{
	return thread->GetGC()->ConstructString(thread, this->length, this->data);
}

int StringBuffer::ToWString(wchar_t *buf)
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

} // namespace ovum