#ifndef AVES__UTF8ENCODING
#define AVES__UTF8ENCODING

#include "../aves.h"
#include "buffer.h"
#include <ov_stringbuffer.h>

// Summary of UTF-8:
//   U+0000 – U+007F:    0xxxxxxx
//   U+0080 – U+07FF:    110xxxxx 10xxxxxx
//   U+0800 – U+FFFF:    1110xxxx 10xxxxxx 10xxxxxx
//   U+10000 – U+10FFFF: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
// Some extra caveats:
//   * The bytes FF and FE cannot occur in valid UTF-8.
//   * A sequence is not permitted to be overlong. That is, it cannot encode
//     a codepoint that is smaller than the intended range of sequences of
//     that length. E.g. a 3-byte sequence must encode something between
//     U+0800 and U+FFFF.
//   * A sequence is not permitted to encode a surrogate character, even if
//     paired with another surrogate.
//   * Codepoints ending in FFFF or FFFE are not valid.
// Whenever an invalid sequence is encountered, we output U+FFFD and move on
// to the next byte. Note that if we run into an incomplete multibyte sequence,
// we output one U+FFFD for the characters we have already consumed, then
// process the non-continuation-byte separately.

// Note: Utf8Encoder and Utf8Decoder lack natively implemented constructors
// because the GC automatically zeroes all the bytes. The initial state of
// both types happens to be all zeroes, so there's no need to do extra work.


AVES_API NATIVE_FUNCTION(aves_Utf8Encoding_getByteCount);
AVES_API NATIVE_FUNCTION(aves_Utf8Encoding_getBytesInternal);

AVES_API NATIVE_FUNCTION(aves_Utf8Encoding_getCharCountInternal);
AVES_API NATIVE_FUNCTION(aves_Utf8Encoding_getCharsInternal);


// Encoder

class Utf8Encoder
{
public:
	// If we encounter a surrogate lead, the next character may be a trail,
	// in which case we combine the two to get our codepoint.
	// If there is a surrogate lead and no trail following it, we throw
	// something.
	ovchar_t surrogateChar;

	int32_t GetByteCount(ThreadHandle thread, String *str, bool flush);
	int32_t GetBytes(ThreadHandle thread, String *str, Buffer *buf, int32_t offset, bool flush);

	void Reset();

	static int BufferOverrunError(ThreadHandle thread);
};

AVES_API int aves_Utf8Encoder_init(TypeHandle type);

AVES_API NATIVE_FUNCTION(aves_Utf8Encoder_getByteCount);
AVES_API NATIVE_FUNCTION(aves_Utf8Encoder_getBytesInternal);
AVES_API NATIVE_FUNCTION(aves_Utf8Encoder_reset);


// Decoder

class Utf8Decoder
{
	// The decoder never has to consume more than four bytes
	// to produce a Unicode character. As a result, we never
	// need to remember more than three, because the very
	// next byte will always decide what to do.
	// However, bytesLeft is still four bytes long because it
	// means it can be copied in a single operation.

public:
	// The state field contains one of the following values:
	//   0 = bytesLeft is empty; we expect a "normal" character or the
	//       beginning of a multibyte sequence to follow.
	//
	//   1 = bytesLeft contains the first byte of a two-byte sequence;
	//       hence, we expect a continuation byte.
	//
	//   2 = bytesLeft contains the first byte of a three-byte sequence;
	//       hence, we expect two continuation bytes.
	//   3 = bytesLeft contains the two initial bytes of a three-byte
	//       sequence; hence, we expect one continuation byte
	//
	//   Similarly, 4-6 are for the first, second and third bytes,
	//   respectively, of a four-byte sequence.
	//
	//   7 is for 5- and 6-byte sequences. In these cases, we use the
	//   bytesLeftAll to contain the number of continuation bytes left
	//   to skip. All 5- and 6-byte sequences result in U+FFFD, since
	//   they are always either overlong or represent something greater
	//   than U+10FFFF.
	//
	// When the expectation is not met, we output U+FFFD, and reset to 0.
	int32_t state;
	union
	{
		uint8_t bytesLeft[4];
		uint32_t bytesLeftAll;
	};

	int32_t GetCharCount(ThreadHandle thread, Buffer *buf, int32_t offset, int32_t count, bool flush);
	int32_t GetChars(ThreadHandle thread, Buffer *buf, int32_t offset, int32_t count, StringBuffer *sb, bool flush);

	void Reset();

	static const ovchar_t ReplacementChar = 0xFFFD;
};

AVES_API int aves_Utf8Decoder_init(TypeHandle type);

AVES_API NATIVE_FUNCTION(aves_Utf8Decoder_getCharCountInternal);
AVES_API NATIVE_FUNCTION(aves_Utf8Decoder_getCharsInternal);
AVES_API NATIVE_FUNCTION(aves_Utf8Decoder_reset);

// Some native APIs!
AVES_API int32_t aves_GetUtf8ByteCount(ThreadHandle thread, String *str);
AVES_API int32_t aves_GetUtf8Bytes(ThreadHandle thread, String *str, uint8_t *buffer, uint32_t bufSize, int32_t offset);

AVES_API int32_t aves_GetUtf8CharCount(ThreadHandle thread, uint8_t *buffer, uint32_t bufSize, int32_t offset, int32_t count);
AVES_API int32_t aves_GetUtf8Chars(ThreadHandle thread, uint8_t *buffer, uint32_t bufSize, int32_t offset, int32_t count, StringBuffer *sb);

#endif // AVES__UTF8ENCODING
