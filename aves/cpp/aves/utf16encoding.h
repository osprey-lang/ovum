#ifndef AVES__UTF16ENCODING
#define AVES__UTF16ENCODING

#include "../aves.h"
#include "buffer.h"
#include <ovum_stringbuffer.h>

struct Utf16Encoding
{
	bool bigEndian;
};

AVES_API int aves_Utf16Encoding_init(TypeHandle type);

AVES_API NATIVE_FUNCTION(aves_Utf16Encoding_new);

AVES_API NATIVE_FUNCTION(aves_Utf16Encoding_get_bigEndian);

AVES_API NATIVE_FUNCTION(aves_Utf16Encoding_getByteCount);
AVES_API NATIVE_FUNCTION(aves_Utf16Encoding_getBytesInternal);

AVES_API NATIVE_FUNCTION(aves_Utf16Encoding_getCharCountInternal);
AVES_API NATIVE_FUNCTION(aves_Utf16Encoding_getCharsInternal);


// Encoder

class Utf16Encoder
{
public:
	Utf16Encoder(bool bigEndian);

	bool bigEndian;
	// Utf16Encoder does not require any state, beyond its endianness.
	// All strings are already UTF-16, so we can just write the UTF-16
	// code units straight to the buffer.

	ssize_t GetByteCount(ThreadHandle thread, String *str, bool flush);

	ssize_t GetBytes(ThreadHandle thread, String *str, Buffer *buf, size_t offset, bool flush);

	inline void Reset() { }
};

AVES_API int aves_Utf16Encoder_init(TypeHandle type);

AVES_API NATIVE_FUNCTION(aves_Utf16Encoder_new);

AVES_API NATIVE_FUNCTION(aves_Utf16Encoder_getByteCount);
AVES_API NATIVE_FUNCTION(aves_Utf16Encoder_getBytesInternal);
AVES_API NATIVE_FUNCTION(aves_Utf16Encoder_reset);


// Decoder

class Utf16Decoder
{
	// All UTF-16 code units are 2 bytes. While decoding a buffer, we
	// may end up with half a code unit, and that's the only state we
	// need to worry about in this class.
	// Oh, and the endianness, that's kinda important.

public:
	Utf16Decoder(bool bigEndian);

	bool bigEndian;
	bool hasPrevByte;
	uint8_t prevByte;

	ssize_t GetCharCount(ThreadHandle thread, Buffer *buf, size_t offset, size_t count, bool flush);

	ssize_t GetChars(ThreadHandle thread, Buffer *buf, size_t offset, size_t count, StringBuffer *sb, bool flush);

	void Reset();

	static const ovchar_t ReplacementChar = 0xFFFD;
};

AVES_API int aves_Utf16Decoder_init(TypeHandle type);

AVES_API NATIVE_FUNCTION(aves_Utf16Decoder_new);

AVES_API NATIVE_FUNCTION(aves_Utf16Decoder_getCharCountInternal);
AVES_API NATIVE_FUNCTION(aves_Utf16Decoder_getCharsInternal);
AVES_API NATIVE_FUNCTION(aves_Utf16Decoder_reset);

#endif // AVES__UTF16ENCODING
