#include "aves_utf16encoding.h"
#include "aves_utf8encoding.h" // For Utf8Encoder::BufferOverrunError
#include <new> // For placement new

AVES_API void aves_Utf16Encoding_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(Utf16Encoding));
}

AVES_API NATIVE_FUNCTION(aves_Utf16Encoding_new)
{
	Utf16Encoding *encoding = reinterpret_cast<Utf16Encoding*>(THISV.instance);
	encoding->bigEndian = IsTrue(args + 1);
}

AVES_API NATIVE_FUNCTION(aves_Utf16Encoding_get_bigEndian)
{
	Utf16Encoding *encoding = reinterpret_cast<Utf16Encoding*>(THISV.instance);
	VM_PushBool(thread, encoding->bigEndian);
}

AVES_API NATIVE_FUNCTION(aves_Utf16Encoding_getByteCount)
{
	// getByteCount(str)
	StringFromValue(thread, args + 1);

	Utf16Encoding *encoding = reinterpret_cast<Utf16Encoding*>(THISV.instance);

	Utf16Encoder enc(encoding->bigEndian);
	int32_t byteCount = enc.GetByteCount(thread, args[1].common.string, true);

	VM_PushInt(thread, byteCount);
}
AVES_API NATIVE_FUNCTION(aves_Utf16Encoding_getBytesInternal)
{
	// getBytesInternal(str is String, buf is Buffer, offset is Int)
	Utf16Encoding *encoding = reinterpret_cast<Utf16Encoding*>(THISV.instance);

	Utf16Encoder enc(encoding->bigEndian);
	int32_t byteCount = enc.GetBytes(thread,
		args[1].common.string,
		reinterpret_cast<Buffer*>(args[2].instance),
		(int32_t)args[3].integer, true);

	VM_PushInt(thread, byteCount);
}

AVES_API NATIVE_FUNCTION(aves_Utf16Encoding_getCharCountInternal)
{
	// getCharCountInternal(buf is Buffer, offset is Int, count is Int)
	Utf16Encoding *encoding = reinterpret_cast<Utf16Encoding*>(THISV.instance);

	Utf16Decoder dec(encoding->bigEndian);
	int32_t charCount = dec.GetCharCount(thread,
		reinterpret_cast<Buffer*>(args[1].instance),
		(int32_t)args[2].integer, (int32_t)args[3].integer,
		true);

	VM_PushInt(thread, charCount);
}
AVES_API NATIVE_FUNCTION(aves_Utf16Encoding_getCharsInternal)
{
	// getCharsInternal(buf is Buffer, offset is Int, count is Int, sb is StringBuffer)
	Utf16Encoding *encoding = reinterpret_cast<Utf16Encoding*>(THISV.instance);

	Utf16Decoder dec(encoding->bigEndian);
	int32_t charCount = dec.GetChars(thread,
		reinterpret_cast<Buffer*>(args[1].instance),
		(int32_t)args[2].integer, (int32_t)args[3].integer,
		reinterpret_cast<StringBuffer*>(args[4].instance),
		true);

	VM_PushInt(thread, charCount);
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
	if (offset + 2 * str->length > buf->size)
		Utf8Encoder::BufferOverrunError(thread);

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
	Utf16Encoder *enc = reinterpret_cast<Utf16Encoder*>(THISV.instance);
	new (enc) Utf16Encoder(IsTrue(args + 1));
}

AVES_API NATIVE_FUNCTION(aves_Utf16Encoder_getByteCount)
{
	// getByteCount(str, flush)
	Utf16Encoder *enc = reinterpret_cast<Utf16Encoder*>(THISV.instance);
	StringFromValue(thread, args + 1);

	int32_t byteCount = enc->GetByteCount(thread,
		args[1].common.string, IsTrue(args + 2));

	VM_PushInt(thread, byteCount);
}
AVES_API NATIVE_FUNCTION(aves_Utf16Encoder_getBytesInternal)
{
	// getBytesInternal(str is String, buf is Buffer, offset is Int, flush is Boolean)
	Utf16Encoder *enc = reinterpret_cast<Utf16Encoder*>(THISV.instance);

	int32_t byteCount = enc->GetBytes(thread,
		args[1].common.string,
		reinterpret_cast<Buffer*>(args[2].instance),
		(int32_t)args[3].integer, !!args[4].integer);

	VM_PushInt(thread, byteCount);
}
AVES_API NATIVE_FUNCTION(aves_Utf16Encoder_reset)
{
	reinterpret_cast<Utf16Encoder*>(THISV.instance)->Reset();
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
			sb->Append(thread, ch);
			charCount++;
		}
		else
			prevByte = *bp;
		hasPrevByte = !hasPrevByte;
	}

	if (flush && hasPrevByte)
	{
		sb->Append(thread, ReplacementChar);
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
	Utf16Decoder *dec = reinterpret_cast<Utf16Decoder*>(THISV.instance);
	new (dec) Utf16Decoder(IsTrue(args + 1));
}

AVES_API NATIVE_FUNCTION(aves_Utf16Decoder_getCharCountInternal)
{
	// getCharCountInternal(buf is Buffer, offset is Int, count is Int, flush is Boolean)
	Utf16Decoder *dec = reinterpret_cast<Utf16Decoder*>(THISV.instance);

	int32_t charCount = dec->GetCharCount(thread,
		reinterpret_cast<Buffer*>(args[1].instance),
		(int32_t)args[2].integer, (int32_t)args[3].integer,
		!!args[4].integer);

	VM_PushInt(thread, charCount);
}
AVES_API NATIVE_FUNCTION(aves_Utf16Decoder_getCharsInternal)
{
	// getCharsInternal(buf is Buffer, offset is Int, count is Int, sb is StringBuffer, flush is Boolean)
	Utf16Decoder *dec = reinterpret_cast<Utf16Decoder*>(THISV.instance);

	int32_t charCount = dec->GetChars(thread,
		reinterpret_cast<Buffer*>(args[1].instance),
		(int32_t)args[2].integer, (int32_t)args[3].integer,
		reinterpret_cast<StringBuffer*>(args[4].instance),
		!!args[4].integer);

	VM_PushInt(thread, charCount);
}
AVES_API NATIVE_FUNCTION(aves_Utf16Decoder_reset)
{
	reinterpret_cast<Utf16Decoder*>(THISV.instance)->Reset();
}