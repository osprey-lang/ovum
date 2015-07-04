#include "utf8encoding.h"
#include "../aves_state.h"
#include <ov_unicode.h>

using namespace aves;

AVES_API BEGIN_NATIVE_FUNCTION(aves_Utf8Encoding_getByteCount)
{
	CHECKED(StringFromValue(thread, args + 1));

	Utf8Encoder enc;
	enc.Reset();
	int32_t byteCount = enc.GetByteCount(thread, args[1].v.string, true);

	VM_PushInt(thread, byteCount);
}
END_NATIVE_FUNCTION
AVES_API NATIVE_FUNCTION(aves_Utf8Encoding_getBytesInternal)
{
	// getBytesInternal(str is String, buf is Buffer, offset is Int)

	Utf8Encoder enc;
	enc.Reset();
	Buffer *buf = args[2].Get<Buffer>();
	int32_t offset = (int32_t)args[3].v.integer;
	int32_t byteCount = enc.GetBytes(thread, args[1].v.string, buf, offset, true);
	if (byteCount < 0)
		return ~byteCount;

	VM_PushInt(thread, byteCount);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_Utf8Encoding_getCharCountInternal)
{
	// getCharCountInternal(buf is Buffer, offset is Int, count is Int)

	Utf8Decoder dec;
	dec.Reset();
	int32_t charCount = dec.GetCharCount(thread,
		args[1].Get<Buffer>(),
		(int32_t)args[2].v.integer,
		(int32_t)args[3].v.integer,
		true);

	VM_PushInt(thread, charCount);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_Utf8Encoding_getCharsInternal)
{
	// getCharsInternal(buf is Buffer, offset is Int, count is Int, sb is StringBuffer)

	StringBuffer *sb = args[4].Get<StringBuffer>();

	Utf8Decoder dec;
	dec.Reset();
	int32_t charCount = dec.GetChars(thread,
		args[1].Get<Buffer>(),
		(int32_t)args[2].v.integer,
		(int32_t)args[3].v.integer,
		sb,
		true);
	if (charCount < 0)
		return ~charCount;

	VM_PushInt(thread, charCount);
	RETURN_SUCCESS;
}


// Encoder

int32_t Utf8Encoder::GetByteCount(ThreadHandle thread, String *str, bool flush)
{
	// Gotta take a copy, as we can't modify the state!
	ovchar_t surrogateChar = this->surrogateChar;

	int32_t count = 0;
	const ovchar_t *chp = &str->firstChar;

	for (int32_t i = 0; i < str->length; i++)
	{
		ovchar_t ch = chp[i];
		if (surrogateChar)
		{
			if (!UC_IsSurrogateTrail(ch))
			{
				// At this point, we have a lead not followed by a trail,
				// so we have to output U+FFFD, or EF BF BD encoded.
				count += 3;

				// ch is not a surrogate trail, but it may still be a lead.
				goto append;
			}

			// Surrogate pairs always represent >U+FFFF, and any
			// character above U+FFFF requires 4 bytes in UTF-8.
			count += 4;
			surrogateChar = 0;
			continue;
		}

		append:
		if (ch >= 0xD800 && ch <= 0xDFFF)
		{
			if (ch >= 0xDC00) // trail without lead becomes U+FFFD
				count += 3;
			else // lead, wait for the next character before making a judgement
				surrogateChar = ch;
			continue;
		}

		if (ch > 0x07FF)
			count++;
		if (ch > 0x007F)
			count++;
		count++;
	}

	if (flush && surrogateChar)
		count += 3; // Trailing lead surrogate? U+FFFD!

	return count;
}

int32_t Utf8Encoder::GetBytes(ThreadHandle thread, String *str, Buffer *buf, int32_t offset, bool flush)
{
	ovchar_t surrogateChar = this->surrogateChar;

	int32_t count = 0;
	const ovchar_t *chp = &str->firstChar;
	uint8_t *bp = buf->bytes + offset;

	for (int32_t i = 0; i < str->length; i++)
	{
		ovchar_t ch = chp[i];
		if (surrogateChar)
		{
			if (!UC_IsSurrogateTrail(ch))
			{
				// Add U+FFFD, which is EF BF BD encoded.
				if ((uint32_t)offset + 3 > buf->size)
					return ~BufferOverrunError(thread);

				*bp++ = 0xEF;
				*bp++ = 0xBF;
				*bp++ = 0xBD;
				offset += 3;
				count += 3;

				goto append;
			}

			if ((uint32_t)offset + 4 > buf->size)
				return ~BufferOverrunError(thread);

			ovwchar_t wch = UC_ToWide(surrogateChar, ch);
			*bp++ = 0xf0 | (wch >> 18);
			*bp++ = 0x80 | (wch >> 12) & 0x3F;
			*bp++ = 0x80 | (wch >> 6) & 0x3F;
			*bp++ = 0x80 | wch & 0x3F;
			offset += 4;
			count += 4;
			surrogateChar = 0;
			continue;
		}

		append:
		if (ch >= 0xD800 && ch <= 0xDFFF)
		{
			if (ch >= 0xDC00) // trail without lead becomes U+FFFD
			{
				if ((uint32_t)offset + 3 > buf->size)
					return ~BufferOverrunError(thread);

				*bp++ = 0xEF;
				*bp++ = 0xBF;
				*bp++ = 0xBD;
				offset += 3;
				count += 3;
			}
			else
				surrogateChar = ch;
			continue;
		}

		if (ch > 0x07FF)
		{
			if ((uint32_t)offset + 3 > buf->size)
				return ~BufferOverrunError(thread);
			*bp++ = 0xE0 | (ch >> 12);
			*bp++ = 0x80 | (ch >> 6) & 0x3F;
			*bp++ = 0x80 | ch & 0x3F;
			offset += 3;
			count += 3;
		}
		else if (ch > 0x7F)
		{
			if ((uint32_t)offset + 2 > buf->size)
				return ~BufferOverrunError(thread);
			*bp++ = 0xC0 | (ch >> 6) & 0x3F;
			*bp++ = 0x80 | ch & 0x3F;
			offset += 2;
			count += 2;
		}
		else
		{
			if ((uint32_t)offset + 1 > buf->size)
				return ~BufferOverrunError(thread);
			*bp++ = (uint8_t)ch;
			offset++;
			count++;
		}
	}

	if (flush && surrogateChar)
	{
		if ((uint32_t)offset + 3 > buf->size)
			return ~BufferOverrunError(thread);

		*bp++ = 0xEF;
		*bp++ = 0xBF;
		*bp++ = 0xBD;
		count += 3;

		surrogateChar = 0;
	}

	// And now update the state!
	this->surrogateChar = surrogateChar;

	return count;
}

void Utf8Encoder::Reset()
{
	this->surrogateChar = 0;
}

int Utf8Encoder::BufferOverrunError(ThreadHandle thread)
{
	Aves *aves = Aves::Get(thread);

	VM_PushString(thread, error_strings::EncodingBufferOverrun);
	return VM_ThrowErrorOfType(thread, aves->aves.ArgumentError, 1);
}

AVES_API void aves_Utf8Encoder_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(Utf8Encoder));
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_Utf8Encoder_getByteCount)
{
	// getByteCount(str, flush)
	Utf8Encoder *enc = THISV.Get<Utf8Encoder>();
	CHECKED(StringFromValue(thread, args + 1));

	int32_t byteCount = enc->GetByteCount(thread, args[1].v.string, IsTrue(args + 2));
	VM_PushInt(thread, byteCount);
}
END_NATIVE_FUNCTION
AVES_API NATIVE_FUNCTION(aves_Utf8Encoder_getBytesInternal)
{
	// getBytesInternal(str is String, buf is Buffer, offset is Int, flush is Boolean)
	Utf8Encoder *enc = THISV.Get<Utf8Encoder>();

	int32_t byteCount = enc->GetBytes(thread, args[1].v.string,
		args[2].Get<Buffer>(),
		(int32_t)args[3].v.integer,
		!!args[4].v.integer);
	if (byteCount < 0)
		return ~byteCount;

	VM_PushInt(thread, byteCount);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_Utf8Encoder_reset)
{
	THISV.Get<Utf8Encoder>()->Reset();
	RETURN_SUCCESS;
}


// Decoder

// Used for local state copies
union BytesLeft
{
	uint8_t bytes[4];
	uint32_t all;

	inline BytesLeft(uint32_t all)
		: all(all)
	{ }
};

int32_t Utf8Decoder::GetCharCount(ThreadHandle thread, Buffer *buf, int32_t offset, int32_t count, bool flush)
{
	// Copy to local; can't update the state in here
	int32_t state = this->state;
	BytesLeft left(this->bytesLeftAll);

	int32_t charCount = 0;
	uint8_t *bp = buf->bytes + offset;

	int32_t i = 0;
	if (state == 0)
	{
		while (i < count)
		{
			// Cast up to uint32 for speed.
			uint32_t b = bp[i];
			if (b > 0x7F)
				// If we find a non-ASCII character, exit here and enter the slow path.
				break;

			charCount++;
			// Don't increment i until here: if we enter the slow path,
			// i remain on the non-ASCII character.
			i++;
		}
	}

	while (i < count)
	{
		uint32_t b = bp[i];

		switch (state)
		{
		// 2-byte sequences
		case 1:
			// Even if b is not a continuation byte, we do want to output
			// one character here, whether it be the decoded 2-byte sequence
			// or U+FFFD (for an overlong sequence, or no continuation byte).
			charCount++;
			// In any case, we've reached the end of the sequence.
			state = 0;
			if ((b & 0xC0) != 0x80)
				// Not a continuation byte! Default processing.
				goto defaultCase;
			break;

		// 3-byte sequences
		case 2:
			if ((b & 0xC0) != 0x80)
			{
				// Not a continuation byte! U+FFFD.
				charCount++;
				state = 0;
				goto defaultCase;
			}
			left.bytes[1] = (uint8_t)b;
			state = 3;
			break;
		case 3:
			// Even if b is not a continuation byte, we do want to output
			// one character here. It may be the decoded 3-byte sequence,
			// or it may be U+FFFD. The 3-byte sequence may be overlong,
			// but U+FFFD is still only one character.
			charCount++;
			// In any case, we've reached the end of the sequence.
			state = 0;
			if ((b & 0xC0) != 0x80)
				// Not a continuation byte! Process b in the default manner.
				goto defaultCase;
			break;

		// 4-byte sequences
		case 4:
		case 5:
			if ((b & 0xC0) != 0x80)
			{
				// Not a continuation byte! U+FFFD.
				charCount++;
				state = 0;
				goto defaultCase;
			}
			// Continuation byte! Record it and advance to the next state.
			left.bytes[state - 3] = (uint8_t)b;
			state++;
			break;
		case 6:
			// Now we have three possibilities:
			//   * b is a continuation byte:
			//     - the sequence is overlong or otherwise invalid
			//       (one character added, U+FFFD)
			//     - valid sequence (surrogate pair; 4-byte sequences are >U+FFFF)
			//   * b is not a continuation byte (one char added, U+FFFD)
			// In all three cases, we add at least one character, and we've
			// reached the end of the sequence.
			charCount++;
			state = 0;

			if ((b & 0xC0) != 0x80)
				// Not a continuation byte: process defaultly.
				goto defaultCase;

			// If the sequence is valid, it's always >U+FFFF,
			// but MAY reach as high as U+1FFFFF, which is
			// not valid. In particular, the following are invalid:
			//   <=U+FFFF      (overlong)
			//   >=U+10FFFE
			//   U+1FFFE, U+1FFFF
			// (codepoints ending in FFFF and FFFE are not allowed)
			// In all other cases, we need a surrogate pair.
			// Therefore, we have to decode the sequence now:
			{
				ovwchar_t wch = ((left.bytes[0] & 0x0F) << 18) |
					((left.bytes[1] & 0x3F) << 12) |
					((left.bytes[2] & 0x3F) << 6) |
					(b & 0x3F);
				if (wch > 0xFFFF && wch < 0x10FFFE &&
					wch != 0x1FFFE && wch != 0x1FFFF)
					charCount++;
			}
			break;

		case 7:
			if ((b & 0xC0) != 0x80 || left.all == 1)
			{
				// Not a continuation byte, or the end of
				// the sequence. 
				charCount++; // U+FFFD, always
				state = 0;
				if ((b & 0xC0) != 0x80)
					// Not a continuation byte? Process defaultly.
					goto defaultCase;
			}
			else
				left.all--;
			break;

		default: // 0
		defaultCase:
			if (b > 0x7F)
			{
				if (b >= 0xC0)
				{
					// Beginning of multi-byte sequence
					left.bytes[0] = (uint8_t)b;
					if (b <= 0xDF)
						// 2-byte sequence
						state = 1;
					else if (b <= 0xEF)
						// 3-byte sequence
						state = 2;
					else if (b <= 0xF7)
						// 4-byte sequence
						state = 4;
					else if (b <= 0xFD)
					{
						// 5- or 6-byte sequence
						state = 7;
						left.all = b > 0xFB ? 5 : 4;
					}
					else
						// FF or FE: always invalid
						goto singleReplacement;
					break;
				}
				// Else: continuation byte outside multi-byte sequence

				singleReplacement:
				// Fall through to count replacement char
				b = (uint32_t)ReplacementChar;
			}
			// Single byte or replacement char
			charCount++;
			break;
		}
	}

	if (flush && state != 0)
		charCount++;

	return charCount;
}

int32_t Utf8Decoder::GetChars(ThreadHandle thread, Buffer *buf, int32_t offset, int32_t count, StringBuffer *sb, bool flush)
{
	// This method uses the same overall structure as GetCharCount,
	// hence comments will not be as plentiful. The only big difference,
	// which is a big difference, is that this actually appends chars.

	int32_t state = this->state;
	BytesLeft left(this->bytesLeftAll);

	int32_t charCount = 0;
	uint8_t *bp = buf->bytes + offset;

	int32_t i = 0;
	if (state == 0)
	{
		while (i < count)
		{
			uint32_t b = bp[i];
			if (b > 0x7F)
				// If we find a non-ASCII character, exit here and enter the slow path.
				break;

			if (!sb->Append((ovchar_t)b))
				return ~OVUM_ERROR_NO_MEMORY;

			charCount++;
			// Don't increment i until here: if we enter the slow path,
			// i remain on the non-ASCII character.
			i++;
		}
	}

	while (i < count)
	{
		uint32_t b = bp[i++];
		ovchar_t ch;

		switch (state)
		{
		// 2-byte sequences
		case 1:
			charCount++;
			state = 0;
			if ((b & 0xC0) != 0x80)
			{
				if (!sb->Append(ReplacementChar))
					return ~OVUM_ERROR_NO_MEMORY;
				goto defaultCase;
			}

			ch = ((left.bytes[0] & 0x1F) << 6) |
				(b & 0x3F);
			if (ch < 0x0080) // overlong sequence
				ch = ReplacementChar;
			if (!sb->Append(ch))
				return ~OVUM_ERROR_NO_MEMORY;
			break;

		// 3-byte sequences
		case 2:
			if ((b & 0xC0) != 0x80)
			{
				if (!sb->Append(ReplacementChar))
					return ~OVUM_ERROR_NO_MEMORY;
				charCount++;
				state = 0;
				goto defaultCase;
			}

			left.bytes[1] = (uint8_t)b;
			state = 3;
			break;
		case 3:
			charCount++;
			state = 0;
			if ((b & 0xC0) != 0x80)
			{
				if (!sb->Append(ReplacementChar))
					return ~OVUM_ERROR_NO_MEMORY;
				goto defaultCase;
			}

			ch = ((left.bytes[0] & 0x0F) << 12) |
				((left.bytes[1] & 0x3F) << 6) |
				(b & 0x3F);
			// <U+0800 = overlong
			// U+D800-U+DFFF = surrogate character (invalid)
			// >=U+FFFE = invalid (U+FFFE and U+FFFF are not allowed)
			if (ch < 0x0800 ||
				(ch >= 0xD800 && ch <= 0xDFFF) ||
				ch > 0xFFFE)
				ch = ReplacementChar;
			if (!sb->Append(ch))
				return ~OVUM_ERROR_NO_MEMORY;
			break;

		// 4-byte sequences
		case 4:
		case 5:
			if ((b & 0xC0) != 0x80)
			{
				if (!sb->Append(ReplacementChar))
					return ~OVUM_ERROR_NO_MEMORY;
				charCount++;
				state = 0;
				goto defaultCase;
			}

			left.bytes[state - 3] = (uint8_t)b;
			state++;
			break;
		case 6:
			charCount++;
			state = 0;

			if ((b & 0xC0) != 0x80)
			{
				if (!sb->Append(ReplacementChar))
					return ~OVUM_ERROR_NO_MEMORY;
				goto defaultCase;
			}

			// <=U+FFFF = overlong
			// >=U+10FFFE = invalid
			// U+1FFFE, U+1FFFF = invalid
			// (codepoints ending in FFFF and FFFE are not allowed)
			// In all other cases, we need a surrogate pair.
			{
				ovwchar_t wch = ((left.bytes[0] & 0x0F) << 18) |
					((left.bytes[1] & 0x3F) << 12) |
					((left.bytes[2] & 0x3F) << 6) |
					(b & 0x3F);
				if (wch > 0xFFFF && wch < 0x10FFFE &&
					wch != 0x1FFFE && wch != 0x1FFFF)
				{
					charCount++;
					SurrogatePair pair = UC_ToSurrogatePair(wch);
					if (!sb->Append(pair.lead) ||
						!sb->Append(pair.trail))
						return ~OVUM_ERROR_NO_MEMORY;
				}
				else
					if (!sb->Append(ReplacementChar))
						return ~OVUM_ERROR_NO_MEMORY;
			}
			break;

		case 7:
			if ((b & 0xC0) != 0x80 || left.all == 1)
			{
				charCount++;
				state = 0;
				// U+FFFD, always
				if (!sb->Append(ReplacementChar))
					return ~OVUM_ERROR_NO_MEMORY;
				if ((b & 0xC0) != 0x80)
					// Not a continuation byte? Process defaultly.
					goto defaultCase;
			}
			else
				left.all--;
			break;

		default: // 0
		defaultCase:
			if (b > 0x7F)
			{
				if (b >= 0xC0)
				{
					// Beginning of multi-byte sequence
					left.bytes[0] = (uint8_t)b;
					if (b <= 0xDF)
						// 2-byte sequence
						state = 1;
					else if (b <= 0xEF)
						// 3-byte sequence
						state = 2;
					else if (b <= 0xF7)
						// 4-byte sequence
						state = 4;
					else if (b <= 0xFD)
					{
						// 5- or 6-byte sequence
						state = 7;
						left.all = b > 0xFB ? 5 : 4;
					}
					else
						// FF or FE: always invalid
						goto singleReplacement;
					break;
				}
				// Else: continuation byte outside multi-byte sequence

				singleReplacement:
				// Fall through to append replacement char
				b = (uint32_t)ReplacementChar;
			}
			// Single byte or replacement char
			if (!sb->Append((ovchar_t)b))
				return ~OVUM_ERROR_NO_MEMORY;
			charCount++;
			break;
		}
	}

	if (flush && state != 0)
	{
		if (!sb->Append(ReplacementChar))
			return ~OVUM_ERROR_NO_MEMORY;
		charCount++;
		state = 0;
	}

	// Copy state to instance
	this->state = state;
	this->bytesLeftAll = left.all;

	return charCount;
}

void Utf8Decoder::Reset()
{
	this->state = 0;
	this->bytesLeftAll = 0;
}

AVES_API void aves_Utf8Decoder_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(Utf8Decoder));
}

AVES_API NATIVE_FUNCTION(aves_Utf8Decoder_getCharCountInternal)
{
	// getCharCountInternal(buf is Buffer, offset is Int, count is Int, flush is Boolean)
	Utf8Decoder *dec = THISV.Get<Utf8Decoder>();

	int32_t charCount = dec->GetCharCount(thread,
		args[1].Get<Buffer>(),
		(int32_t)args[2].v.integer,
		(int32_t)args[3].v.integer,
		!!args[4].v.integer);

	VM_PushInt(thread, charCount);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_Utf8Decoder_getCharsInternal)
{
	// getCharsInternal(buf is Buffer, offset is Int, count is Int, sb is StringBuffer, flush is Boolean)
	Utf8Decoder *dec = THISV.Get<Utf8Decoder>();

	int32_t charCount = dec->GetChars(thread,
		args[1].Get<Buffer>(),
		(int32_t)args[2].v.integer,
		(int32_t)args[3].v.integer,
		args[4].Get<StringBuffer>(),
		!!args[5].v.integer);
	if (charCount < 0)
		return ~charCount;

	VM_PushInt(thread, charCount);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_Utf8Decoder_reset)
{
	THISV.Get<Utf8Decoder>()->Reset();
	RETURN_SUCCESS;
}

// Native APIs
AVES_API int32_t aves_GetUtf8ByteCount(ThreadHandle thread, String *str)
{
	Utf8Encoder enc;
	enc.Reset();
	return enc.GetByteCount(thread, str, true);
}
AVES_API int32_t aves_GetUtf8Bytes(ThreadHandle thread, String *str, uint8_t *buffer, uint32_t bufSize, int32_t offset)
{
	Utf8Encoder enc;
	enc.Reset();
	Buffer buf;
	buf.size = bufSize;
	buf.bytes = buffer;
	return enc.GetBytes(thread, str, &buf, offset, true);
}

AVES_API int32_t aves_GetUtf8CharCount(ThreadHandle thread, uint8_t *buffer, uint32_t bufSize, int32_t offset, int32_t count)
{
	Utf8Decoder dec;
	dec.Reset();
	Buffer buf;
	buf.size = bufSize;
	buf.bytes = buffer;
	return dec.GetCharCount(thread, &buf, offset, count, true);
}
AVES_API int32_t aves_GetUtf8Chars(ThreadHandle thread, uint8_t *buffer, uint32_t bufSize, int32_t offset, int32_t count, StringBuffer *sb)
{
	Utf8Decoder dec;
	dec.Reset();
	Buffer buf;
	buf.size = bufSize;
	buf.bytes = buffer;
	return dec.GetChars(thread, &buf, offset, count, sb, true);
}
