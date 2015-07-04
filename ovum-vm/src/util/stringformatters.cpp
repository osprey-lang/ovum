#include "stringformatters.h"
#include "stringbuffer.h"

namespace ovum
{

int32_t IntFormatter::ToDec(int64_t number, StringBuffer &dest, int32_t minLength)
{
	bool isNeg = number < 0;
	if (isNeg)
		// When number == INT64_MIN, this overflows back to INT64_MIN,
		// but that can be cast losslessly to uint64_t.
		number = -number;

	ovchar_t buffer[BUFFER_SIZE];
	ovchar_t *bufferEnd = buffer + BUFFER_SIZE;
	int32_t numberLength = BuildDecString((uint64_t)number, bufferEnd);
	// Move the pointer to the first character
	bufferEnd -= numberLength;

	int32_t length = numberLength;
	if (isNeg)
	{
		dest.Append(MINUS);
		length++;
	}

	if (minLength >= length)
	{
		dest.Append(minLength - length, ZERO);
		length = minLength;
	}

	dest.Append(numberLength, bufferEnd);

	return length;
}

int32_t IntFormatter::ToDec(uint64_t number, StringBuffer &dest, int32_t minLength)
{
	ovchar_t buffer[BUFFER_SIZE];
	ovchar_t *bufferEnd = buffer + BUFFER_SIZE;
	int32_t numberLength = BuildDecString(number, bufferEnd);
	// Move the pointer to the first character
	bufferEnd -= numberLength;

	int32_t length = numberLength;

	if (minLength >= length)
	{
		dest.Append(minLength - length, ZERO);
		length = minLength;
	}

	dest.Append(numberLength, bufferEnd);

	return length;
}

int32_t IntFormatter::ToDec(int64_t number, ovchar_t *dest, int32_t destSize)
{
	bool isNeg = number < 0;
	if (isNeg)
		// When number == INT64_MIN, this overflows back to INT64_MIN,
		// but that can be cast losslessly to uint64_t.
		number = -number;

	ovchar_t buffer[BUFFER_SIZE];
	ovchar_t *bufferEnd = buffer + BUFFER_SIZE;
	int32_t numberLength = BuildDecString((uint64_t)number, bufferEnd);
	// Move the pointer to the first character
	bufferEnd -= numberLength;

	int32_t length = numberLength + isNeg;
	if (destSize < length)
		// Destination buffer too small. Don't write anything to it; just
		// return the required length.
		return length;

	if (isNeg)
		*dest++ = MINUS;

	CopyMemoryT(dest, bufferEnd, numberLength);

	return length;
}

int32_t IntFormatter::ToDec(uint64_t number, ovchar_t *dest, int32_t destSize)
{
	ovchar_t buffer[BUFFER_SIZE];
	ovchar_t *bufferEnd = buffer + BUFFER_SIZE;
	int32_t length = BuildDecString((uint64_t)number, bufferEnd);
	// Move the pointer to the first character
	bufferEnd -= length;

	if (destSize < length)
		// Destination buffer too small. Don't write anything to it; just
		// return the required length.
		return length;

	CopyMemoryT(dest, bufferEnd, length);

	return length;
}

int32_t IntFormatter::ToHex(uint64_t number, StringBuffer &dest, bool upper, int32_t minLength)
{
	ovchar_t buffer[BUFFER_SIZE];
	ovchar_t *bufferEnd = buffer + BUFFER_SIZE;
	int32_t numberLength = BuildHexString(number, bufferEnd, upper);
	// Move the pointer to the first character
	bufferEnd -= numberLength;

	int32_t length = numberLength;

	if (minLength > length)
	{
		dest.Append(minLength - length, ZERO);
		length = minLength;
	}

	dest.Append(numberLength, bufferEnd);

	return length;
}

int32_t IntFormatter::ToHex(uint64_t number, ovchar_t *dest, int32_t destSize, bool upper)
{
	ovchar_t buffer[BUFFER_SIZE];
	ovchar_t *bufferEnd = buffer + BUFFER_SIZE;
	int32_t length = BuildHexString(number, bufferEnd, upper);
	// Move the pointer to the first character
	bufferEnd -= length;

	if (destSize < length)
		// Destination buffer too small. Don't write anything to it; just
		// return the required length.
		return length;

	CopyMemoryT(dest, bufferEnd, length);

	return length;
}

int32_t IntFormatter::BuildDecString(uint64_t number, ovchar_t *destEnd)
{
	ovchar_t *destp = destEnd;
	do
	{
		*--destp = ZERO + (int)(number % 10);
		number /= 10;
	} while (number != 0);

	// Remember: destEnd > destp
	return destEnd - destp;
}

int32_t IntFormatter::BuildHexString(uint64_t number, ovchar_t *destEnd, bool upper)
{
	ovchar_t hexBase = upper ? HEX_UPPER_BASE : HEX_LOWER_BASE;

	ovchar_t *destp = destEnd;
	do
	{
		int remainder = (int)(number % 16);
		ovchar_t hexChar;
		if (remainder > 10)
			hexChar = hexBase + remainder - 10;
		else
			hexChar = ZERO + remainder;
		*--destp = hexChar;
	} while (number != 0);

	// Remember: destEnd > destp
	return destEnd - destp;
}

} // namespace ovum
