#include "aves_utf16encoding.h"
#include "aves_utf8encoding.h" // For Utf8Encoder::BufferOverrunError
#include <new> // For placement new

AVES_API void aves_Utf16Encoding_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(Utf16Encoding));
}

AVES_API NATIVE_FUNCTION(aves_Utf16Encoding_new)
{
	Utf16Encoding *encoding = THISV.Get<Utf16Encoding>();
	encoding->bigEndian = IsTrue(args + 1);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_Utf16Encoding_get_bigEndian)
{
	Utf16Encoding *encoding = THISV.Get<Utf16Encoding>();
	VM_PushBool(thread, encoding->bigEndian);
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_Utf16Encoding_getByteCount)
{
	// getByteCount(str)
	CHECKED(StringFromValue(thread, args + 1));

	Utf16Encoding *encoding = THISV.Get<Utf16Encoding>();

	Utf16Encoder enc(encoding->bigEndian);
	int32_t byteCount = enc.GetByteCount(thread, args[1].v.string, true);

	VM_PushInt(thread, byteCount);
}
END_NATIVE_FUNCTION
AVES_API NATIVE_FUNCTION(aves_Utf16Encoding_getBytesInternal)
{
	// getBytesInternal(str is String, buf is Buffer, offset is Int)
	Utf16Encoding *encoding = THISV.Get<Utf16Encoding>();

	Utf16Encoder enc(encoding->bigEndian);
	int32_t byteCount = enc.GetBytes(thread,
		args[1].v.string,
		args[2].Get<Buffer>(),
		(int32_t)args[3].v.integer,
		true);
	if (byteCount < 0)
		return ~byteCount;

	VM_PushInt(thread, byteCount);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_Utf16Encoding_getCharCountInternal)
{
	// getCharCountInternal(buf is Buffer, offset is Int, count is Int)
	Utf16Encoding *encoding = THISV.Get<Utf16Encoding>();

	Utf16Decoder dec(encoding->bigEndian);
	int32_t charCount = dec.GetCharCount(thread,
		args[1].Get<Buffer>(),
		(int32_t)args[2].v.integer,
		(int32_t)args[3].v.integer,
		true);

	VM_PushInt(thread, charCount);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_Utf16Encoding_getCharsInternal)
{
	// getCharsInternal(buf is Buffer, offset is Int, count is Int, sb is StringBuffer)
	Utf16Encoding *encoding = THISV.Get<Utf16Encoding>();

	Utf16Decoder dec(encoding->bigEndian);
	int32_t charCount = dec.GetChars(thread,
		args[1].Get<Buffer>(),
		(int32_t)args[2].v.integer,
		(int32_t)args[3].v.integer,
		args[4].Get<StringBuffer>(),
		true);
	if (charCount < 0)
		return ~charCount;

	VM_PushInt(thread, charCount);
	RETURN_SUCCESS;
}


// Encoder

Utf16Encoder::Utf16Encoder(bool bigEndian)
	: bigEndian(bigEndian)
{ }

int32_t Utf16Encoder::GetByteCount(ThreadHandle thread, String *str, bool flush)
{
	// Simple code is nice,
	// (this comment is a haiku)
	// wordy code tiring.
	return str->length * 2;
}

int32_t Utf16Encoder::GetBytes(ThreadHandle thread, String *str, Buffer *buf, int32_t offset, bool flush)
{
	if ((uint32_t)(offset + 2 * str->length) > buf->size)
		return ~Utf8Encoder::BufferOverrunError(thread);

	const uchar *chp = &str->firstChar;
	uint8_t *bp = buf->bytes + offset;

	bool bigEndian = this->bigEndian;
	for (int32_t i = 0; i < str->length; i++)
	{
		if (bigEndian)
		{
			*bp++ = *chp >> 8;
			*bp++ = *chp & 0xff;
		}
		else
		{
			*bp++ = *chp & 0xff;
			*bp++ = *chp >> 8;
		}
		chp++;
	}

	return str->length * 2;
}

AVES_API void aves_Utf16Encoder_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(Utf16Encoder));
}

AVES_API NATIVE_FUNCTION(aves_Utf16Encoder_new)
{
	Utf16Encoder *enc = THISV.Get<Utf16Encoder>();
	new(enc) Utf16Encoder(IsTrue(args + 1));
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_Utf16Encoder_getByteCount)
{
	// getByteCount(str, flush)
	Utf16Encoder *enc = THISV.Get<Utf16Encoder>();
	CHECKED(StringFromValue(thread, args + 1));

	int32_t byteCount = enc->GetByteCount(thread,
		args[1].v.string, IsTrue(args + 2));

	VM_PushInt(thread, byteCount);
}
END_NATIVE_FUNCTION
AVES_API NATIVE_FUNCTION(aves_Utf16Encoder_getBytesInternal)
{
	// getBytesInternal(str is String, buf is Buffer, offset is Int, flush is Boolean)
	Utf16Encoder *enc = THISV.Get<Utf16Encoder>();

	int32_t byteCount = enc->GetBytes(thread,
		args[1].v.string,
		args[2].Get<Buffer>(),
		(int32_t)args[3].v.integer,
		!!args[4].v.integer);
	if (byteCount < 0)
		return ~byteCount;

	VM_PushInt(thread, byteCount);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_Utf16Encoder_reset)
{
	THISV.Get<Utf16Encoder>()->Reset();
	RETURN_SUCCESS;
}


// Decoder

Utf16Decoder::Utf16Decoder(bool bigEndian)
	: bigEndian(bigEndian)
{
	Reset();
}

int32_t Utf16Decoder::GetCharCount(ThreadHandle thread, Buffer *buf, int32_t offset, int32_t count, bool flush)
{
	if (hasPrevByte)
		count++;

	// 2 bytes = one UTF-16 code unit
	int32_t charCount = count / 2;

	if (flush && (count & 1) == 1)
		// If flush and the byte count is odd, we must append U+FFFD.
		charCount++;

	return charCount;
}
int32_t Utf16Decoder::GetChars(ThreadHandle thread, Buffer *buf, int32_t offset, int32_t count, StringBuffer *sb, bool flush)
{
	bool bigEndian = this->bigEndian;
	bool hasPrevByte = this->hasPrevByte;
	uint8_t prevByte = this->prevByte;

	int32_t charCount = 0;
	uint8_t *bp = buf->bytes + offset;
	for (int32_t i = 0; i < count; i++, bp++)
	{
		if (hasPrevByte)
		{
			uchar ch;
			if (bigEndian)
				ch = (prevByte << 8) | *bp;
			else
				ch = (*bp << 8) | prevByte;
			if (!sb->Append(ch))
				return ~OVUM_ERROR_NO_MEMORY;
			charCount++;
		}
		else
			prevByte = *bp;
		hasPrevByte = !hasPrevByte;
	}

	if (flush && hasPrevByte)
	{
		if (!sb->Append(ReplacementChar))
			return ~OVUM_ERROR_NO_MEMORY;
		hasPrevByte = false;
	}

	// And now update the state!
	this->hasPrevByte = hasPrevByte;
	this->prevByte = prevByte;

	return charCount;
}

void Utf16Decoder::Reset()
{
	hasPrevByte = false;
	prevByte = 0;
}

AVES_API void aves_Utf16Decoder_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(Utf16Decoder));
}

AVES_API NATIVE_FUNCTION(aves_Utf16Decoder_new)
{
	Utf16Decoder *dec = THISV.Get<Utf16Decoder>();
	new(dec) Utf16Decoder(IsTrue(args + 1));
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_Utf16Decoder_getCharCountInternal)
{
	// getCharCountInternal(buf is Buffer, offset is Int, count is Int, flush is Boolean)
	Utf16Decoder *dec = THISV.Get<Utf16Decoder>();

	int32_t charCount = dec->GetCharCount(thread,
		args[1].Get<Buffer>(),
		(int32_t)args[2].v.integer,
		(int32_t)args[3].v.integer,
		!!args[4].v.integer);

	VM_PushInt(thread, charCount);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_Utf16Decoder_getCharsInternal)
{
	// getCharsInternal(buf is Buffer, offset is Int, count is Int, sb is StringBuffer, flush is Boolean)
	Utf16Decoder *dec = THISV.Get<Utf16Decoder>();

	int32_t charCount = dec->GetChars(thread,
		args[1].Get<Buffer>(),
		(int32_t)args[2].v.integer,
		(int32_t)args[3].v.integer,
		args[4].Get<StringBuffer>(),
		!!args[4].v.integer);
	if (charCount < 0)
		return ~charCount;

	VM_PushInt(thread, charCount);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_Utf16Decoder_reset)
{
	THISV.Get<Utf16Decoder>()->Reset();
	RETURN_SUCCESS;
}