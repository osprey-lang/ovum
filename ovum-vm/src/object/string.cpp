#include "../vm.h"
#include "../../inc/ov_unicode.h"
#include "../../inc/ov_string.h"
#include "../gc/gc.h"
#include "../ee/thread.h"
#include "../res/staticstrings.h"
#include <memory>

inline const bool IsHashed(const String *const str)
{
	return (str->flags & StringFlags::HASHED) != StringFlags::NONE;
}

/* Available string hash algorithm implementations:
 *   1 – shameless .NET Framework steal, basically
 *   2 – shameless Mono steal, mostly
 *   3 – FNV-1a
 * If you do not select an algorithm, you'll get the lose-lose algorithm,
 * which will ensure huge numbers of collisions, and you have no one to
 * blame but yourself for not reading properly.
 */
#define STRING_HASH_ALGORITHM  3

inline int32_t String_GetHashCode(int32_t length, const ovchar_t *s)
{
#if STRING_HASH_ALGORITHM == 1
	// Rip of one of the algorithms in the .NET Framework
	int32_t hash1 = (5381 << 16) + 5381;
	int32_t hash2 = hash1;

	while (*s)
	{
		hash1 = ((hash1 << 5) + hash1) ^ *s;

		if (*s++ == 0)
			break;

		hash2 = ((hash2 << 5) + hash2) ^ *s; 
		*s++;
	}

	return hash1 + (hash2 * 1566083941);
#elif STRING_HASH_ALGORITHM == 2
	// Rip of the Mono algorithm, ever so slightly modified
	int32_t hash = 0;

	const ovchar_t *end = s + length - 1;
	while (s < end)
	{
		hash = (hash << 5) - hash + s[0];
		hash = (hash << 5) - hash + s[1];
		s += 2;
	}
	++end;
	if (s < end)
		hash = (hash << 5) - hash + *s;
	return hash;
#elif STRING_HASH_ALGORITHM == 3
	// FNV-1a
	// Note that this operates on a BYTE basis, not character
	int32_t hash = 0x811c9dc5;
	const int32_t prime = 0x01000193;

	//const uint8_t *data = reinterpret_cast<const uint8_t*>(s);
	int32_t remaining = length;
	while (remaining--)
	{
		hash = ((*s & 0xff) ^ hash) * prime;
		hash = ((*s >> 8) ^ hash) * prime;
		s++;
	}

	return hash;
#else
	// Well okay. You didn't specify a hash algorithm, suit yourself.
	int32_t hash = 0;
	int32_t remaining = length;
	while (remaining--)
		hash += *s;
	return hash;
#endif
}

OVUM_API int32_t String_GetHashCode(String *str)
{
	if (IsHashed(str))
		return str->hashCode;

	// Note: always set hashCode first, to avoid race conditions
	// in case another thread hashes the string at the same time.
	str->hashCode = String_GetHashCode(str->length, &str->firstChar);
	str->flags |= StringFlags::HASHED;

	return str->hashCode;
}

OVUM_API int32_t String_GetHashCodeSubstr(const String *str, int32_t index, int32_t count)
{
	return String_GetHashCode(count, &str->firstChar + index);
}

OVUM_API bool String_Equals(const String *a, const String *b)
{
	if (!a || !b || a == b)
		// At this point, either the pointers point to the same address, or
		// at least one is null. If both are null, they compare as equal.
		return a == b;
	if (a->length != b->length ||
		IsHashed(a) && IsHashed(b) &&
		a->hashCode != b->hashCode)
		return false; // couldn't possibly be the same string value

	// It doesn't matter which string we take the length of; 
	// they're guaranteed to be the same here anyway.
	int32_t length = a->length;

	const ovchar_t *ap = &a->firstChar;
	const ovchar_t *bp = &b->firstChar;

	// Unroll the loop by 10 characters
	while (length > 10)
	{
		if (*(int32_t*)ap != *(int32_t*)bp) break;
		if (*(int32_t*)(ap + 2) != *(int32_t*)(bp + 2)) break;
		if (*(int32_t*)(ap + 4) != *(int32_t*)(bp + 4)) break;
		if (*(int32_t*)(ap + 6) != *(int32_t*)(bp + 6)) break;
		if (*(int32_t*)(ap + 8) != *(int32_t*)(bp + 8)) break;
		ap += 10;
		bp += 10;
		length -= 10;
	}

	while (length > 0)
	{
		if (*(int32_t*)ap != *(int32_t*)bp)
			break;
		ap += 2;
		bp += 2;

		length -= 2;
	}

	return length <= 0;
}

OVUM_API bool String_EqualsIgnoreCase(const String *a, const String *b)
{
	// If either is nullptr or both refer to the same instance, then
	// they compare equal if the pointers are equal.
	if (!a || !b || a == b)
		return a == b;
	// Note: unlike string::Equals, we cannot compare hash codes here,
	// because the two strings could be differently-cased versions of
	// the same text.
	// Not 2: Unicode makes the guarantee that case folding does not
	// change the plane of a character. Most notably, a BMP character
	// cannot be case-folded out of the BMP, or vice versa. Hence, it
	// is safe to use a length comparison as an additional check.
	if (a->length != b->length)
		return false;

	// It doesn't matter which string we take the length of;
	// they're guaranteed to be the same here anyway.
	int32_t length = a->length;

	const ovchar_t *ap = &a->firstChar;
	const ovchar_t *bp = &b->firstChar;

	while (length)
	{
		// We only transform a surrogate pair into a wide character if we actually
		// find a surrogate pair at the same location in each string. Otherwise,
		// we can only skip the surrogate leads, never the character after.
		// Consider:
		//    a = lead, lead, trail
		//    b = lead, lead, trail
		// If we always skip the next character when we encounter a lead, we will
		// never find the valid surrogate pair, which may make two matching strings
		// compare as unequal.
		// Note: strings are always zero-padded, and the zero padding is not part
		// of the string's length.
		if (UC_IsSurrogateLead(*ap) && UC_IsSurrogateLead(*bp) &&
			UC_IsSurrogateTrail(*(ap + 1)) && UC_IsSurrogateTrail(*(bp + 1)))
		{
			ovwchar_t aWide = UC_ToWide(*ap, *(ap + 1));
			ovwchar_t bWide = UC_ToWide(*bp, *(bp + 1));

			if (UC_ToUpper(aWide) != UC_ToUpper(bWide))
				break;

			// Remember: we've read two characters
			ap += 2;
			bp += 2;
			length -= 2;
		}
		else
		{
			if (UC_ToUpper(*ap) != UC_ToUpper(*bp))
				break;

			ap++;
			bp++;
			length--;
		}
	}

	return length <= 0;
}

OVUM_API bool String_SubstringEquals(const String *str, int32_t startIndex, const String *part)
{
	OVUM_ASSERT(str != nullptr);
	if (startIndex >= str->length)
		return false;
	if (part == nullptr || part->length == 0)
		return true;
	if (part->length > str->length - startIndex)
		return false;

	// (The rest is basically a slightly modified copy of String_Equals)

	int32_t length = part->length;

	const ovchar_t *ap = &str->firstChar + startIndex;
	const ovchar_t *bp = &part->firstChar;

	while (length > 10) // check 5 dwords per iteration
	{
		if (*(int32_t*)ap != *(int32_t*)bp) break;
		if (*(int32_t*)(ap + 2) != *(int32_t*)(bp + 2)) break;
		if (*(int32_t*)(ap + 4) != *(int32_t*)(bp + 4)) break;
		if (*(int32_t*)(ap + 6) != *(int32_t*)(bp + 6)) break;
		if (*(int32_t*)(ap + 8) != *(int32_t*)(bp + 8)) break;
		ap += 10;
		bp += 10;
		length -= 10;
	}

	// Note: unlike String_Equals, we cannot test 4 bytes at a time
	// here, in case the substring has an odd length.
	while (length > 0)
	{
		if (*ap++ != *bp++)
			break;

		length--;
	}

	return length <= 0;
}

bool IsSurrogatePair(ovchar_t a, ovchar_t b)
{
	return UC_IsSurrogateLead(a) && UC_IsSurrogateTrail(b);
}

OVUM_API int String_Compare(const String *a, const String *b)
{
	int32_t alen = a->length, blen = b->length;

	const ovchar_t *ap = &a->firstChar;
	const ovchar_t *bp = &b->firstChar;

	while (alen && blen)
	{
		// Note: strings are zero-terminated despite having a known length.
		// We can always safely access the next character.
		ovwchar_t aw = *ap++;
		if (IsSurrogatePair((ovchar_t)aw, *ap))
		{
			aw = UC_ToWide((ovchar_t)aw, *ap++);
			alen--;
		}
		ovwchar_t bw = *bp++;
		if (IsSurrogatePair((ovchar_t)bw, *bp))
		{
			bw = UC_ToWide((ovchar_t)bw, *bp++);
			blen--;
		}

		if (aw != bw)
			// Note: without the int cast, the unsigned subtraction
			// will overflow if bw > aw. ovwchar_t is guaranteed to fit
			// inside an int32_t.
			return (int32_t)aw - (int32_t)bw;
		alen--;
		blen--;
	}

	return alen - blen;
}

OVUM_API bool String_Contains(const String *str, const String *value)
{
	OVUM_ASSERT(str != nullptr);
	if (value == nullptr || value->length == 0)
		return true;
	if (value->length > str->length)
		return false; // The string cannot contain a substring longer than itself.
	if (value->length == str->length)
		return String_Equals(str, value);

	const ovchar_t *strp = &str->firstChar;
	const ovchar_t firstValChar = value->firstChar;
	int32_t remaining = str->length - value->length + 1;
	while (remaining > 0)
	{
		if (*strp == firstValChar)
		{
			// The comparison algorithm below is basically lifted from String_SubstringEquals,
			// and then slightly modified
			int32_t length = value->length - 1;

			const ovchar_t *strpCopy = strp + 1;
			const ovchar_t *valp = &value->firstChar + 1;

			while (length > 10) // Unroll comparison loop by 10!
			{
				if (*(int32_t*)strp != *(int32_t*)valp) break;
				if (*(int32_t*)(strp + 2) != *(int32_t*)(valp + 2)) break;
				if (*(int32_t*)(strp + 4) != *(int32_t*)(valp + 4)) break;
				if (*(int32_t*)(strp + 6) != *(int32_t*)(valp + 6)) break;
				if (*(int32_t*)(strp + 8) != *(int32_t*)(valp + 8)) break;
				strp += 10;
				valp += 10;
				length -= 10;
			}

			while (length > 0)
			{
				if (*strp != *valp)
					break;
				strp++;
				valp++;

				length--;
			}

			if (length == 0)
				return true;
			// otherwise, advance to the next character in str
			strp = strpCopy;
		}
		strp++;
		remaining--;
	}

	return false;
}

OVUM_API String *String_ToUpper(ThreadHandle thread, String *str)
{
	String *newStr = thread->GetGC()->ConstructString(thread, str->length, nullptr);
	if (!newStr) return nullptr;

	const ovchar_t *a = &str->firstChar;
	ovchar_t *b = const_cast<ovchar_t*>(&newStr->firstChar);
	int32_t remaining = str->length;
	while (remaining--)
	{
		ovchar_t ach = *a;
		if (UC_IsSurrogateLead(ach) && UC_IsSurrogateTrail(*(a + 1)))
		{
			SurrogatePair surr = UC_ToSurrogatePair(UC_ToUpper(UC_ToWide(ach, *(a + 1))));
			*(uint32_t*)b = *reinterpret_cast<int32_t*>(&surr);
			a += 2;
			b += 2;
			remaining--; // we skipped two characters!
		}
		else
		{
			*b = UC_ToUpper(ach);
			a++;
			b++;
		}
	}

	return newStr;
}

OVUM_API String *String_ToLower(ThreadHandle thread, String *str)
{
	String *newStr = thread->GetGC()->ConstructString(thread, str->length, nullptr);
	if (!newStr) return nullptr;

	const ovchar_t *a = &str->firstChar;
	ovchar_t *b = const_cast<ovchar_t*>(&newStr->firstChar);
	int32_t remaining = str->length;
	while (remaining--)
	{
		ovchar_t ach = *a;
		if (UC_IsSurrogateLead(ach) && UC_IsSurrogateTrail(*(a + 1)))
		{
			SurrogatePair surr = UC_ToSurrogatePair(UC_ToLower(UC_ToWide(ach, *(a + 1))));
			*(uint32_t*)b = *reinterpret_cast<int32_t*>(&surr);
			a += 2;
			b += 2;
			remaining--; // we skipped two characters!
		}
		else
		{
			*b = UC_ToLower(ach);
			a++;
			b++;
		}
	}

	return newStr;
}

OVUM_API String *String_Concat(ThreadHandle thread, const String *a, const String *b)
{
	// Make sure the target length is within range!
	if (INT32_MAX - a->length < b->length)
		return nullptr;

	int32_t outLength = a->length + b->length;

	String *output = thread->GetGC()->ConstructString(thread, outLength, nullptr);
	if (output)
	{
		ovchar_t *outputChar = const_cast<ovchar_t*>(&output->firstChar);

		CopyMemoryT(outputChar, &a->firstChar, a->length);
		outputChar += a->length;
		CopyMemoryT(outputChar, &b->firstChar, b->length);
	}
	return output;
}

OVUM_API String *String_Concat3(ThreadHandle thread, const String *a, const String *b, const String *c)
{
	if (INT32_MAX - a->length < b->length)
		return nullptr;

	int32_t outLength = a->length + b->length;

	if (INT32_MAX - outLength < c->length)
		return nullptr;

	outLength += c->length;

	String *output = thread->GetGC()->ConstructString(thread, outLength, nullptr);
	if (output)
	{
		ovchar_t *outputChar = const_cast<ovchar_t*>(&output->firstChar);

		CopyMemoryT(outputChar, &a->firstChar, a->length);
		outputChar += a->length;
		CopyMemoryT(outputChar, &b->firstChar, b->length);
		outputChar += b->length;
		CopyMemoryT(outputChar, &c->firstChar, c->length);
	}
	return output;
}

OVUM_API String *String_ConcatRange(ThreadHandle thread, int count, String *values[])
{
	if (count <= 0)
		return thread->GetStrings()->empty;
	if (count == 1)
		return values[0];

	int32_t outLength = 0;
	for (int i = 0; i < count; i++)
	{
		int32_t strlen = values[i]->length;
		if (INT32_MAX - outLength < strlen)
			return nullptr;

		outLength += strlen;
	}

	String *output = thread->GetGC()->ConstructString(thread, outLength, nullptr);
	if (output)
	{
		ovchar_t *outputChar = const_cast<ovchar_t*>(&output->firstChar);

		for (int i = 0; i < count; i++)
		{
			String *str = values[i];
			CopyMemoryT(outputChar, &str->firstChar, str->length);
			outputChar += str->length;
		}
	}
	return output;
}

OVUM_API int32_t String_ToWString(wchar_t *dest, const String *source)
{
#if OVUM_WCHAR_SIZE == 2
	// UTF-16 (or at least UCS-2, but hopefully surrogates won't break things too much)

	int32_t outputLength = source->length + 1; // Include the \0

	if (dest)
		memcpy(dest, &source->firstChar, outputLength * sizeof(ovchar_t));

	return outputLength; // Do include \0
#elif OVUM_WCHAR_SIZE == 4
	// UTF-32

	// First, iterate over the string to find out how many surrogate pairs there are,
	// if any. These consume only one UTF-32 character.
	// We use this to calculate the length of the output (including the \0).
	int32_t outputLength = 0;

	int32_t strLen = source->length + 1; // let's include the \0
	const ovchar_t *strp = &source->firstChar;
	for (int32_t i = 0; i < strLen; i++)
	{
		if (UC_IsSurrogateLead(*strp) && UC_IsSurrogateTrail(*(strp + 1)))
		{
			i++; // skip one extra character; surrogate pair still only makes one wchar_t
			strp++;
		}
		outputLength++;
		strp++;
	}

	if (dest)
	{
		// And now we can copy things to the destination, yay
		strp = &source->firstChar;

		wchar_t *outp = dest;
		for (int i = 0; i < outputLength; i++)
		{
			if (UC_IsSurrogateLead(*strp) && UC_IsSurrogateTrail(*(strp + 1)))
			{
				*outp = UC_ToWide(*strp, *(strp + 1));
				strp++; // skip one extra character in source
			}
			else
				*outp = *strp;

			strp++;
			outp++;
		}
	}

	return outputLength; // Do include \0
#else
#error Not supported
#endif
}

OVUM_API String *String_FromCString(ThreadHandle thread, const char *source)
{
	return thread->GetGC()->ConvertString(thread, source);
}

OVUM_API String *String_FromWString(ThreadHandle thread, const wchar_t *source)
{
#if OVUM_WCHAR_SIZE == 2
	// UTF-16 (or at least UCS-2)

	size_t length = wcslen(source);

	String *output = thread->GetGC()->ConstructString(thread, length, (const ovchar_t*)source);

	return output;
#elif OVUM_WCHAR_SIZE == 4
	// UTF-32

	int32_t outLength = 0;

	const wchar_t *strp = source;
	while (*strp)
	{
		if (UC_NeedsSurrogatePair((const ovwchar_t)*strp))
			outLength += 2;
		else
			outLength++;
		strp++;
	}

	std::unique_ptr<ovchar_t[]> buffer(new ovchar_t[outLength]);

	strp = source;
	ovchar_t *outp = buffer.get();
	while (*strp)
	{
		const ovwchar_t ch = (const ovwchar_t)*strp;
		if (UC_NeedsSurrogatePair(ch))
		{
			const SurrogatePair surr = UC_ToSurrogatePair(ch);
			*outp = surr.lead;
			outp++;
			*outp = surr.trail;
		}
		else
			*outp = (ovchar_t)ch;

		outp++;
	}

	String *output = thread->GetGC()->ConstructString(thread, outLength, buffer.get());

	return output;
#else
#error Not supported
#endif
}

OVUM_API String *String_GetInterned(ThreadHandle thread, String *str)
{
	return thread->GetGC()->GetInternedString(thread, str);
}

OVUM_API String *String_Intern(ThreadHandle thread, String *str)
{
	return thread->GetGC()->InternString(thread, str);
}
