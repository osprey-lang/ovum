#include "aves_utf8encoding.h"
#include "ov_unicode.h"

AVES_API NATIVE_FUNCTION(aves_Utf8Encoding_getByteCount)
{
	StringFromValue(thread, args + 1);

	Utf8Encoder enc;
	enc.Reset();
	int32_t byteCount = enc.GetByteCount(thread, args[1].common.string, true);

	VM_PushInt(thread, byteCount);
}
AVES_API NATIVE_FUNCTION(aves_Utf8Encoding_getBytesInternal)
{
	// getBytesInternal(str is String, buf is Buffer, offset is Int)

	Utf8Encoder enc;
	enc.Reset();
	Buffer *buf = reinterpret_cast<Buffer*>(args[2].instance);
	int32_t offset = (int32_t)args[3].integer;
	int32_t byteCount = enc.GetBytes(thread, args[1].common.string, buf, offset, true);

	VM_PushInt(thread, byteCount);
}

AVES_API NATIVE_FUNCTION(aves_Utf8Encoding_getCharCountInternal)
{
	// getCharCountInternal(buf is Buffer, offset is Int, count is Int)

	Utf8Decoder dec;
	dec.Reset();
	int32_t charCount = dec.GetCharCount(thread,
		reinterpret_cast<Buffer*>(args[1].instance),
		(int32_t)args[2].integer, (int32_t)args[3].integer, true);

	VM_PushInt(thread, charCount);
}
AVES_API NATIVE_FUNCTION(aves_Utf8Encoding_getCharsInternal)
{
	// getCharsInternal(buf is Buffer, offset is Int, count is Int, sb is StringBuffer)

	StringBuffer *sb = reinterpret_cast<StringBuffer*>(args[4].instance);

	Utf8Decoder dec;
	dec.Reset();
	int32_t charCount = dec.GetChars(thread,
		reinterpret_cast<Buffer*>(args[1].instance),
		(int32_t)args[2].integer, (int32_t)args[3].integer,
		sb, true);

	VM_PushInt(thread, charCount);
}


// Encoder

int32_t Utf8Encoder::GetByteCount(ThreadHandle thread, String *str, bool flush)
{
	// Gotta take a copy, as we can't modify the state!
	uchar surrogateChar = this->surrogateChar;

	int32_t count = 0;
	const uchar *chp = &str->firstChar;

	for (int32_t i = 0; i < str->length; i++)
	{
		uchar ch = chp[i];
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
	uchar surrogateChar = this->surrogateChar;

	int32_t count = 0;
	const uchar *chp = &str->firstChar;
	uint8_t *bp = buf->bytes + offset;

	for (int32_t i = 0; i < str->length; i++)
	{
		uchar ch = chp[i];
		if (surrogateChar)
		{
			if (!UC_IsSurrogateTrail(ch))
			{
				// Add U+FFFD, which is EF BF BD encoded.
				if (offset + 3 > buf->size)
					BufferOverrunError(thread);

				*bp++ = 0xEF;
				*bp++ = 0xBF;
				*bp++ = 0xBD;
				offset += 3;
				count += 3;

				goto append;
			}

			if (offset + 4 > buf->size)
				BufferOverrunError(thread);

			wuchar wch = UC_ToWide(surrogateChar, ch);
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
				if (offset + 3 > buf->size)
					BufferOverrunError(thread);

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
			if (offset + 3 > buf->size)
				BufferOverrunError(thread);
			*bp++ = 0xE0 | (ch >> 12);
			*bp++ = 0x80 | (ch >> 6) & 0x3F;
			*bp++ = 0x80 | ch & 0x3F;
			offset += 3;
			count += 3;
		}
		else if (ch > 0x7F)
		{
			if (offset + 2 > buf->size)
				BufferOverrunError(thread);
			*bp++ = 0xC0 | (ch >> 6) & 0x3F;
			*bp++ = 0x80 | ch & 0x3F;
			offset += 2;
			count += 2;
		}
		else
		{
			if (offset + 1 > buf->size)
				BufferOverrunError(thread);
			*bp++ = (uint8_t)ch;
			offset++;
			count++;
		}
	}

	if (flush && surrogateChar)
	{
		if (offset + 3 > buf->size)
			BufferOverrunError(thread);

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

void Utf8Encoder::BufferOverrunError(ThreadHandle thread)
{
	VM_PushString(thread, error_strings::EncodingBufferOverrun);
	GC_Construct(thread, Types::ArgumentError, 1, nullptr);
	VM_Throw(thread);
}

AVES_API void aves_Utf8Encoder_init(TypeHandle type)
{
	Type_SetInstanceSize(type, sizeof(Utf8Encoder));
}

AVES_API NATIVE_FUNCTION(aves_Utf8Encoder_getByteCount)
{
	// getByteCount(str, flush)
	Utf8Encoder *enc = reinterpret_cast<Utf8Encoder*>(THISV.instance);
	StringFromValue(thread, args + 1);

	int32_t byteCount = enc->GetByteCount(thread, args[1].common.string, IsTrue(args + 2));
	VM_PushInt(thread, byteCount);
}
AVES_API NATIVE_FUNCTION(aves_Utf8Encoder_getBytesInternal)
{
	// getBytesInternal(str is String, buf is Buffer, offset is Int, flush is Boolean)
	Utf8Encoder *enc = reinterpret_cast<Utf8Encoder*>(THISV.instance);

	int32_t byteCount = enc->GetBytes(thread, args[1].common.string,
		reinterpret_cast<Buffer*>(args[2].instance),
		(int32_t)args[3].integer,
		!!args[4].integer);

	VM_PushInt(thread, byteCount);
}
AVES_API NATIVE_FUNCTION(aves_Utf8Encoder_reset)
{
	reinterpret_cast<Utf8Encoder*>(THISV.instance)->Reset();
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
	for (int32_t i = 0; i < count; i++)
	{
		// Cast up to uint32 for speed.
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
				wuchar wch = ((left.bytes[0] & 0x0F) << 18) |
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
			// Switch on the high nibble of the byte
			switch (b >> 4)
			{
			case 0xC:
			case 0xD:
				// 2-byte sequence
				left.bytes[0] = (uint8_t)b;
				state = 1;
				break;
			case 0xE:
				// 3-byte sequence
				left.bytes[0] = (uint8_t)b;
				state = 2;
				break;
			case 0xF:
				// 4-, 5-, or 6-byte sequence
				// FE and FF are never valid in UTF-8, so they
				// get turned into U+FFFD.

				switch (b & 0x0f)
				{
				case 0x8: case 0x9: case 0xA: case 0xB:
				case 0xC: case 0xD:
					// 8-B = 111110xx: 5-byte sequence
					// C-D = 1111110x: 6-byte sequence
					// In both cases, the sequence is either overlong or
					// represents something larger than U+10FFFF, so we
					// spit out U+FFFD.
					state = 7; // >5-byte sequence
					left.all = b > 0xB ? 5 : 4; // number of cont. bytes to skip
					break;
				case 0xE:
				case 0xF:
					charCount++; // U+FFFD
					break;
				default: // 0-7
					// 11110xxx: 4-byte sequence
					state = 4;
					left.bytes[0] = (uint8_t)b;
					break;
				}
				break;
			default:
				// Single-byte sequence
				// This may be a continuation byte outside of
				// a multibyte sequence. These are turned into
				// U+FFFD, which is one character.
				charCount++;
				break;
			}
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
	for (int32_t i = 0; i < count; i++)
	{
		uint32_t b = bp[i];
		uchar ch;

		switch (state)
		{
		// 2-byte sequences
		case 1:
			charCount++;
			state = 0;
			if ((b & 0xC0) != 0x80)
			{
				sb->Append(thread, ReplacementChar);
				goto defaultCase;
			}

			ch = ((left.bytes[0] & 0x1F) << 6) |
				(b & 0x3F);
			if (ch < 0x0080) // overlong sequence
				ch = ReplacementChar;
			sb->Append(thread, ch);
			break;

		// 3-byte sequences
		case 2:
			if ((b & 0xC0) != 0x80)
			{
				sb->Append(thread, ReplacementChar);
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
				sb->Append(thread, ReplacementChar);
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
			sb->Append(thread, ch);
			break;

		// 4-byte sequences
		case 4:
		case 5:
			if ((b & 0xC0) != 0x80)
			{
				sb->Append(thread, ReplacementChar);
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
				sb->Append(thread, ReplacementChar);
				goto defaultCase;
			}

			// <=U+FFFF = overlong
			// >=U+10FFFE = invalid
			// U+1FFFE, U+1FFFF = invalid
			// (codepoints ending in FFFF and FFFE are not allowed)
			// In all other cases, we need a surrogate pair.
			{
				wuchar wch = ((left.bytes[0] & 0x0F) << 18) |
					((left.bytes[1] & 0x3F) << 12) |
					((left.bytes[2] & 0x3F) << 6) |
					(b & 0x3F);
				if (wch > 0xFFFF && wch < 0x10FFFE &&
					wch != 0x1FFFE && wch != 0x1FFFF)
				{
					charCount++;
					SurrogatePair pair = UC_ToSurrogatePair(wch);
					sb->Append(thread, pair.lead);
					sb->Append(thread, pair.trail);
				}
				else
					sb->Append(thread, ReplacementChar);
			}
			break;

		case 7:
			if ((b & 0xC0) != 0x80 || left.all == 1)
			{
				charCount++;
				state = 0;
				// U+FFFD, always
				sb->Append(thread, ReplacementChar);
				if ((b & 0xC0) != 0x80)
					// Not a continuation byte? Process defaultly.
					goto defaultCase;
			}
			else
				left.all--;
			break;

		default: // 0
		defaultCase:
			switch (b >> 4)
			{
			case 0xC:
			case 0xD:
				// 2-byte sequence
				left.bytes[0] = (uint8_t)b;
				state = 1;
				break;
			case 0xE:
				// 3-byte sequence
				left.bytes[0] = (uint8_t)b;
				state = 2;
				break;
			case 0xF:
				// 4-, 5-, or 6-byte sequence
				switch (b & 0x0f)
				{
				case 0x8: case 0x9: case 0xA: case 0xB:
				case 0xC: case 0xD:
					state = 7;
					left.all = b > 0xB ? 5 : 4;
					break;
				case 0xE:
				case 0xF:
					sb->Append(thread, ReplacementChar);
					charCount++;
					break;
				default: // 0-7
					state = 4;
					left.bytes[0] = (uint8_t)b;
					break;
				}
				break;
			default:
				// Single-byte sequence
				if (b > 0x7F)
					// b must be a continuation character outside
					// a multibyte sequence, which is not okay.
					b = ReplacementChar;
				sb->Append(thread, (uchar)b);
				charCount++;
				break;
			}
			break;
		}
	}

	if (flush && state != 0)
	{
		sb->Append(thread, ReplacementChar);
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
	Utf8Decoder *dec = reinterpret_cast<Utf8Decoder*>(THISV.instance);

	int32_t charCount = dec->GetCharCount(thread,
		reinterpret_cast<Buffer*>(args[1].instance),
		(int32_t)args[2].integer, (int32_t)args[3].integer,
		!!args[4].integer);

	VM_PushInt(thread, charCount);
}
AVES_API NATIVE_FUNCTION(aves_Utf8Decoder_getCharsInternal)
{
	// getCharsInternal(buf is Buffer, offset is Int, count is Int, sb is StringBuffer, flush is Boolean)
	Utf8Decoder *dec = reinterpret_cast<Utf8Decoder*>(THISV.instance);

	int32_t charCount = dec->GetChars(thread,
		reinterpret_cast<Buffer*>(args[1].instance),
		(int32_t)args[2].integer, (int32_t)args[3].integer,
		reinterpret_cast<StringBuffer*>(args[4].instance),
		!!args[5].integer);

	VM_PushInt(thread, charCount);
}
AVES_API NATIVE_FUNCTION(aves_Utf8Decoder_reset)
{
	reinterpret_cast<Utf8Decoder*>(THISV.instance)->Reset();
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