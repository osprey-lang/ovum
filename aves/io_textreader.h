#pragma once

#ifndef IO__TEXTREADER_H
#define IO__TEXTREADER_H

#include "aves.h"

class TextReaderInst
{
public:
	// These need to be Values because we need to
	// be able to pass them to managed methods, or
	// we only access them from managed code.
	Value stream;       // io.Stream
	Value encoding;     // aves.Encoding
	Value decoder;      // aves.Decoder
	Value byteBuffer;   // aves.Buffer
	Value charBuffer;   // aves.StringBuffer
	// Not these, however!
	int32_t charCount;
	int32_t charOffset;
	bool keepOpen;

	static MethodHandle FillBuffer;
	static String *FillBufferName;
};

AVES_API void CDECL io_TextReader_init(TypeHandle type);

AVES_API NATIVE_FUNCTION(io_TextReader_get_stream);
AVES_API NATIVE_FUNCTION(io_TextReader_set_stream);

AVES_API NATIVE_FUNCTION(io_TextReader_get_encoding);
AVES_API NATIVE_FUNCTION(io_TextReader_set_encoding);

AVES_API NATIVE_FUNCTION(io_TextReader_get_keepOpen);
AVES_API NATIVE_FUNCTION(io_TextReader_set_keepOpen);

AVES_API NATIVE_FUNCTION(io_TextReader_get_decoder);
AVES_API NATIVE_FUNCTION(io_TextReader_set_decoder);

AVES_API NATIVE_FUNCTION(io_TextReader_get_byteBuffer);
AVES_API NATIVE_FUNCTION(io_TextReader_set_byteBuffer);

AVES_API NATIVE_FUNCTION(io_TextReader_get_charBuffer);
AVES_API NATIVE_FUNCTION(io_TextReader_set_charBuffer);

AVES_API NATIVE_FUNCTION(io_TextReader_get_charCount);
AVES_API NATIVE_FUNCTION(io_TextReader_set_charCount);

AVES_API NATIVE_FUNCTION(io_TextReader_get_charOffset);
AVES_API NATIVE_FUNCTION(io_TextReader_set_charOffset);

AVES_API NATIVE_FUNCTION(io_TextReader_readLine);

#endif // IO__TEXTREADER_H