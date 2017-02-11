#include "utf16encoding.h"
#include "utf8encoding.h" // For Utf8Encoder::BufferOverrunError
#include <new> // For placement new

AVES_API int aves_Utf16Encoding_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(Utf16Encoding));
	RETURN_SUCCESS;
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
	ssize_t byteCount = enc.GetByteCount(thread, args[1].v.string, true);

	VM_PushInt(thread, byteCount);
}
END_NATIVE_FUNCTION
AVES_API NATIVE_FUNCTION(aves_Utf16Encoding_getBytesInternal)
{
	// getBytesInternal(str: String, buf: Buffer, offset: Int)
	Utf16Encoding *encoding = THISV.Get<Utf16Encoding>();

	Utf16Encoder enc(encoding->bigEndian);
	ssize_t byteCount = enc.GetBytes(
		thread,
		args[1].v.string,
		args[2].Get<Buffer>(),
		(size_t)args[3].v.integer,
		true
	);
	if (byteCount < 0)
		return (int)~byteCount;

	VM_PushInt(thread, byteCount);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_Utf16Encoding_getCharCountInternal)
{
	// getCharCountInternal(buf: Buffer, offset: Int, count: Int)
	Utf16Encoding *encoding = THISV.Get<Utf16Encoding>();

	Utf16Decoder dec(encoding->bigEndian);
	ssize_t charCount = dec.GetCharCount(
		thread,
		args[1].Get<Buffer>(),
		(size_t)args[2].v.integer,
		(size_t)args[3].v.integer,
		true
	);

	VM_PushInt(thread, charCount);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_Utf16Encoding_getCharsInternal)
{
	// getCharsInternal(buf: Buffer, offset: Int, count: Int, sb: StringBuffer)
	Utf16Encoding *encoding = THISV.Get<Utf16Encoding>();

	Utf16Decoder dec(encoding->bigEndian);
	ssize_t charCount = dec.GetChars(
		thread,
		args[1].Get<Buffer>(),
		(size_t)args[2].v.integer,
		(size_t)args[3].v.integer,
		args[4].Get<StringBuffer>(),
		true
	);
	if (charCount < 0)
		return (int)~charCount;

	VM_PushInt(thread, charCount);
	RETURN_SUCCESS;
}


// Encoder

Utf16Encoder::Utf16Encoder(bool bigEndian) :
	bigEndian(bigEndian)
{ }

ssize_t Utf16Encoder::GetByteCount(ThreadHandle thread, String *str, bool flush)
{
	// Simple code is nice,
	// (this comment is a haiku)
	// wordy code tiring.
	return static_cast<ssize_t>(str->length * 2);
}

ssize_t Utf16Encoder::GetBytes(ThreadHandle thread, String *str, Buffer *buf, size_t offset, bool flush)
{
	if ((offset + 2 * str->length) > buf->size)
		return ~Utf8Encoder::BufferOverrunError(thread);

	const ovchar_t *chp = &str->firstChar;
	uint8_t *bp = buf->bytes + offset;

	bool bigEndian = this->bigEndian;
	for (size_t i = 0; i < str->length; i++)
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

	return static_cast<ssize_t>(str->length * 2);
}

AVES_API int aves_Utf16Encoder_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(Utf16Encoder));
	RETURN_SUCCESS;
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

	ssize_t byteCount = enc->GetByteCount(
		thread,
		args[1].v.string,
		IsTrue(args + 2)
	);

	VM_PushInt(thread, byteCount);
}
END_NATIVE_FUNCTION
AVES_API NATIVE_FUNCTION(aves_Utf16Encoder_getBytesInternal)
{
	// getBytesInternal(str is String, buf is Buffer, offset is Int, flush is Boolean)
	Utf16Encoder *enc = THISV.Get<Utf16Encoder>();

	ssize_t byteCount = enc->GetBytes(
		thread,
		args[1].v.string,
		args[2].Get<Buffer>(),
		(size_t)args[3].v.integer,
		!!args[4].v.integer
	);
	if (byteCount < 0)
		return (int)~byteCount;

	VM_PushInt(thread, byteCount);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_Utf16Encoder_reset)
{
	THISV.Get<Utf16Encoder>()->Reset();
	RETURN_SUCCESS;
}


// Decoder

Utf16Decoder::Utf16Decoder(bool bigEndian) :
	bigEndian(bigEndian)
{
	Reset();
}

ssize_t Utf16Decoder::GetCharCount(ThreadHandle thread, Buffer *buf, size_t offset, size_t count, bool flush)
{
	if (hasPrevByte)
		count++;

	// 2 bytes = one UTF-16 code unit
	ssize_t charCount = static_cast<ssize_t>(count / 2);

	if (flush && (count & 1) == 1)
		// If flush and the byte count is odd, we must append U+FFFD.
		charCount++;

	return charCount;
}
ssize_t Utf16Decoder::GetChars(ThreadHandle thread, Buffer *buf, size_t offset, size_t count, StringBuffer *sb, bool flush)
{
	bool bigEndian = this->bigEndian;
	bool hasPrevByte = this->hasPrevByte;
	uint8_t prevByte = this->prevByte;

	ssize_t charCount = 0;
	uint8_t *bp = buf->bytes + offset;
	for (size_t i = 0; i < count; i++, bp++)
	{
		if (hasPrevByte)
		{
			ovchar_t ch;
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
		charCount++;
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

AVES_API int aves_Utf16Decoder_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(Utf16Decoder));
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_Utf16Decoder_new)
{
	Utf16Decoder *dec = THISV.Get<Utf16Decoder>();
	new(dec) Utf16Decoder(IsTrue(args + 1));
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_Utf16Decoder_getCharCountInternal)
{
	// getCharCountInternal(buf: Buffer, offset: Int, count: Int, flush: Boolean)
	Utf16Decoder *dec = THISV.Get<Utf16Decoder>();

	ssize_t charCount = dec->GetCharCount(
		thread,
		args[1].Get<Buffer>(),
		(size_t)args[2].v.integer,
		(size_t)args[3].v.integer,
		!!args[4].v.integer
	);

	VM_PushInt(thread, charCount);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_Utf16Decoder_getCharsInternal)
{
	// getCharsInternal(buf: Buffer, offset: Int, count: Int, sb: StringBuffer, flush: Boolean)
	Utf16Decoder *dec = THISV.Get<Utf16Decoder>();

	ssize_t charCount = dec->GetChars(
		thread,
		args[1].Get<Buffer>(),
		(size_t)args[2].v.integer,
		(size_t)args[3].v.integer,
		args[4].Get<StringBuffer>(),
		!!args[4].v.integer
	);
	if (charCount < 0)
		return (int)~charCount;

	VM_PushInt(thread, charCount);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_Utf16Decoder_reset)
{
	THISV.Get<Utf16Decoder>()->Reset();
	RETURN_SUCCESS;
}
