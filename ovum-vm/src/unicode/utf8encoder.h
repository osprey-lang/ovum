#ifndef VM__UTF8ENCODER_H
#define VM__UTF8ENCODER_H

#include "../vm.h"

namespace ovum
{

// Implements a basic UTF-8 encoder that converts UTF-16 strings into a buffer.
// The UTF-8 encoder is stateful. It is intended to be used to encode one string
// at a time, in chunks. The exact buffer size (and location) is specified when
// the UTF-8 encoder is instantiated; the UTF-16 string can be set at any time.
//
// Depending on the length of the string and the size of the buffer, it may be
// necessary to call GetNextBytes() multiple times. When the entire string has
// been processed, GetNextBytes() returns 0.
//
// Don't expect this class to behave sensibly if you give it a buffer with less
// than four bytes of space.
class Utf8Encoder
{
private:
	// The first byte of the destination buffer.
	char *const buffer;
	// The end of the destination buffer, which is one byte past the last.
	char *const bufferEnd;

	// The string data. Note that these are current values; the pointer and
	// the (remaining) length are updated by GetNextBytes().
	const ovchar_t *str;
	int32_t strLength;

	// Currently pending surrogate lead. If a character other than a
	// surrogate trail is encountered when this field is nonzero, the
	// UTF-16 is invalid and we must output the replacement character.
	ovchar_t unmatchedSurrogateLead;

public:
	Utf8Encoder(char *buffer, int32_t bufferLength);
	Utf8Encoder(char *buffer, int32_t bufferLength, String *str);
	Utf8Encoder(char *buffer, int32_t bufferLength, const ovchar_t *str, int32_t strLength);

	inline const char *GetBuffer()
	{
		return buffer;
	}

	inline int32_t GetBufferLength()
	{
		return bufferEnd - buffer;
	}

	inline void SetString(String *str)
	{
		SetString(&str->firstChar, str->length);
	}
	void SetString(const ovchar_t *str, int32_t strLength);

	// Encodes the current string data into the buffer. This method will
	// attempt to encode the entire string or fill up the buffer, whichever
	// comes first. The value returned is the number of UTF-8 bytes written.
	// When this method returns 0, the end of the string has been reached.
	int32_t GetNextBytes();

private:
	inline bool CanAppend(char *buffer, int32_t count) const
	{
		return buffer + count <= bufferEnd;
	}

	bool TryAppendAscii(char *&buffer, ovchar_t ch);

	bool TryAppendSequence2(char *&buffer, ovchar_t ch);

	bool TryAppendSequence3(char *&buffer, ovchar_t ch);

	bool TryAppendSurrogatePair(char *&buffer, ovchar_t lead, ovchar_t trail);

	inline bool TryAppendReplacementChar(char *&buffer)
	{
		return TryAppendSequence3(buffer, REPLACEMENT_CHAR);
	}

	static const ovchar_t REPLACEMENT_CHAR = 0xFFFD;
};

} // namespace ovum

#endif // VM__UTF8ENCODER_H