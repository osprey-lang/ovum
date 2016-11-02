#include "utf8encoder.h"
#include "../../inc/ovum_unicode.h"

namespace ovum
{

Utf8Encoder::Utf8Encoder(char *buffer, int32_t bufferLength) :
	buffer(buffer), bufferEnd(buffer + bufferLength)
{
	SetString(nullptr, 0);
}
Utf8Encoder::Utf8Encoder(char *buffer, int32_t bufferLength, String *str) :
	buffer(buffer), bufferEnd(buffer + bufferLength)
{
	SetString(str);
}
Utf8Encoder::Utf8Encoder(char *buffer, int32_t bufferLength, const ovchar_t *str, int32_t strLength) :
	buffer(buffer), bufferEnd(buffer + bufferLength)
{
	SetString(str, strLength);
}

void Utf8Encoder::SetString(const ovchar_t *str, int32_t strLength)
{
	this->str = str;
	this->strLength = strLength;
	this->unmatchedSurrogateLead = 0;
}

int32_t Utf8Encoder::GetNextBytes()
{
	// Current byte in the buffer
	char *bufp = this->buffer;

	for (; strLength > 0; str++, strLength--)
	{
		ovchar_t ch = *str;
		if (unmatchedSurrogateLead)
		{
			ovchar_t lead = unmatchedSurrogateLead;
			unmatchedSurrogateLead = 0;

			if (UC_IsSurrogateTrail(ch))
			{
				if (!TryAppendSurrogatePair(bufp, lead, ch))
					break;
				continue;
			}

			// If the current character is not a surrogate lead, we must
			// append a replacement character for the mismatched surrogate
			// lead, AND process the current character normally.
			if (!TryAppendReplacementChar(bufp))
				break;
		}

		bool success;
		if (ch <= 0x7F)
		{
			// Fast path for ASCII characters
			success = TryAppendAscii(bufp, ch);
		}
		else if (ch <= 0x07FF)
		{
			// U+0080 to U+07FF => 2-byte sequence
			success = TryAppendSequence2(bufp, ch);
		}
		else
		{
			if (UC_IsSurrogateLead(ch))
			{
				// We can't do anything with the surrogate lead just yet; we
				// have to wait until we've read the next character. If the
				// next character is a surrogate trail, we should write a
				// replacement character. Otherwise, we compose the two lead
				// and trail into a Unicode code point.
				unmatchedSurrogateLead = ch;
				// Behave as if the character has been eaten up.
				success = true;
			}
			else if (UC_IsSurrogateTrail(ch))
			{
				// Surrogate trail without a preceding lead? Replacement char!
				success = TryAppendReplacementChar(bufp);
			}
			else
			{
				// U+0800 to U+FFFF => 3-byte sequence
				// (excluding surrogate leads)
				success = TryAppendSequence3(bufp, ch);
			}
		}

		if (!success)
			break;
	}

	// If we're at the end of the string and there's an unmatched
	// surrogate lead, we have to append a replacement character now.
	if (strLength == 0 && unmatchedSurrogateLead != 0)
		TryAppendReplacementChar(bufp);

	return bufp - this->buffer;
}

bool Utf8Encoder::TryAppendAscii(char *&buffer, ovchar_t ch)
{
	if (!CanAppend(buffer, 1))
		return false;
	
	*buffer++ = (char)ch;
	return true;
}

bool Utf8Encoder::TryAppendSequence2(char *&buffer, ovchar_t ch)
{
	if (!CanAppend(buffer, 2))
		return false;

	*buffer++ = (char)(0xC0 | (ch >> 6));
	*buffer++ = (char)(0x80 | ch & 0x3F);
	return true;
}

bool Utf8Encoder::TryAppendSequence3(char *&buffer, ovchar_t ch)
{
	if (!CanAppend(buffer, 3))
		return false;

	*buffer++ = (char)(0xE0 | (ch >> 12));
	*buffer++ = (char)(0x80 | (ch >> 6) & 0x3F);
	*buffer++ = (char)(0x80 | ch & 0x3F);
	return true;
}

bool Utf8Encoder::TryAppendSurrogatePair(char *&buffer, ovchar_t lead, ovchar_t trail)
{
	if (!CanAppend(buffer, 4))
		return false;

	ovwchar_t wch = UC_ToWide(lead, trail);
	*buffer++ = (char)(0xF0 | (wch >> 18));
	*buffer++ = (char)(0x80 | (wch >> 12) & 0x3F);
	*buffer++ = (char)(0x80 | (wch >> 6) & 0x3F);
	*buffer++ = (char)(0x80 | wch & 0x3F);
	return true;
}

} // namespace ovum
