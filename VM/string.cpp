#include "ov_vm.internal.h"
#include "ov_unicode.h"
#include "ov_string.h"

OVUM_API int32_t String_GetHashCode(String *str)
{
	if (str->flags & STR_HASHED)
		return str->hashCode;

	int32_t hash1 = (5381 << 16) + 5381;
	int32_t hash2 = hash1;

    int32_t c;
	const uchar *s = &str->firstChar;
    while (c = s[0])
	{
        hash1 = ((hash1 << 5) + hash1) ^ c;

        c = s[1];
        if (c == 0)
            break;

        hash2 = ((hash2 << 5) + hash2) ^ c; 
        s += 2; 
    }

	str->flags = _E(StringFlags, str->flags | STR_HASHED);
	return str->hashCode = hash1 + (hash2 * 1566083941);
}

OVUM_API bool String_Equals(const String *a, const String *b)
{
	if (!a || !b || a == b)
		// At this point, either the pointers point to the same address, or
		// at least one is null. If both are null, they compare as equal.
		return a == b;
	if (a->length != b->length ||
		(a->flags & STR_HASHED) && (b->flags & STR_HASHED) &&
		a->hashCode != b->hashCode)
		return false; // couldn't possibly be the same string value

	// It doesn't matter which string we take the length of; 
	// they're guaranteed to be the same here anyway.
	int32_t length = a->length;

	const uchar *ap = &a->firstChar;
	const uchar *bp = &b->firstChar;

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

	// Note: this depends on the fact that strings are null-terminated, and
	// that the null character is never included in the string length.
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
	if (a->length != b->length)
		return false;

	// It doesn't matter which string we take the length of;
	// they're guaranteed to be the same here anyway.
	int32_t length = a->length;

	const uchar *ap = &a->firstChar;
	const uchar *bp = &b->firstChar;

	while (length)
	{
		// We only transform a surrogate pair into a wide character if both
		// *ap and *bp are surrogate leads, and if both are followed by surrogate
		// trails. In all other cases, the values of each character are compared.
		if (UC_IsSurrogateLead(*ap) && UC_IsSurrogateLead(*bp))
		{
			const uchar aLead = *ap++;
			const uchar bLead = *bp++;

			if (length == 1 && aLead != bLead)
				break; // couldn't possibly be a valid surrogate pair

			// Skip surrogate lead! If this puts us past the end of the string,
			// then IsSurrogateTrail() will return false for both *ap and *bp,
			// which will dereference to '\0'. Once again, we're relying on the
			// fact that strings are zero-terminated and this terminator is NOT
			// part of the string length.
			length--;

			if (!UC_IsSurrogateTrail(*ap) || !UC_IsSurrogateTrail(*bp))
			{
				// Note: we do need to perform a case-insensitive comparison for *ap and *bp here,
				// because *ap and *bp could be letters in a writing system with case distinction!
				// We do not need to do that for aLead and bLead, however, because surrogate chars
				// do not ever make a case distinction.
				// Also note: it can never be the case that length == 0 && aLead != bLead at this
				// point, because that would have been caught by an earlier condition. However, it
				// is possible that length > 0, in which case it was > 1 in the earlier condition,
				// so we still need to test aLead and bLead here.
				if (aLead != bLead || UC_ToUpper(*ap) != UC_ToUpper(*bp))
					break;
				// If length == 0 at this point, it must mean that aLead == bLead, which means that
				// the strings compare equal!
				length--;
				continue;
			}

			// *ap and *bp are surrogate trails at this point
			const wuchar aWide = UC_ToWide(aLead, *ap++);
			const wuchar bWide = UC_ToWide(bLead, *bp++);

			if (UC_ToUpper(aWide) != UC_ToUpper(bWide))
				break;

			length--;
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

OVUM_API int String_Compare(const String *a, const String *b)
{
	int32_t alen = a->length, blen = b->length;

	const uchar *ap = &a->firstChar;
	const uchar *bp = &b->firstChar;

	while (alen && blen)
	{
		if (*ap != *bp)
			// Note: without the int cast, the unsigned subtraction
			// will overflow if *bp > *ap. uchar is guaranteed to fit
			// inside an int.
			return (int)*ap - (int)*bp;
		alen--;
		blen--;
	}

	return alen - blen;
}

OVUM_API String *String_ToUpper(ThreadHandle thread, String *str)
{
	String *newStr;
	GC::gc->ConstructString(_Th(thread), str->length, nullptr, &newStr);

	const uchar *a = &str->firstChar;
	uchar *b = const_cast<uchar*>(&newStr->firstChar);
	int32_t remaining = str->length;
	while (remaining--)
	{
		uchar ach = *a;
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
	String *newStr;
	GC::gc->ConstructString(_Th(thread), str->length, nullptr, &newStr);

	const uchar *a = &str->firstChar;
	uchar *b = const_cast<uchar*>(&newStr->firstChar);
	int32_t remaining = str->length;
	while (remaining--)
	{
		uchar ach = *a;
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
		_Th(thread)->ThrowOverflowError();

	int32_t outLength = a->length + b->length;

	String *output;
	GC::gc->ConstructString(_Th(thread), outLength, nullptr, &output);

	uchar *outputChar = const_cast<uchar*>(&output->firstChar);

	CopyMemoryT(outputChar, &a->firstChar, a->length);
	outputChar += a->length;
	CopyMemoryT(outputChar, &b->firstChar, b->length);

	return output;
}

OVUM_API String *String_Concat3(ThreadHandle thread, const String *a, const String *b, const String *c)
{
	if (INT32_MAX - a->length < b->length)
		_Th(thread)->ThrowOverflowError();

	int32_t outLength = a->length + b->length;

	if (INT32_MAX - outLength < c->length)
		_Th(thread)->ThrowOverflowError();

	outLength += c->length;

	String *output;
	GC::gc->ConstructString(_Th(thread), outLength, nullptr, &output);
	uchar *outputChar = const_cast<uchar*>(&output->firstChar);

	CopyMemoryT(outputChar, &a->firstChar, a->length);
	outputChar += a->length;
	CopyMemoryT(outputChar, &b->firstChar, b->length);
	outputChar += b->length;
	CopyMemoryT(outputChar, &c->firstChar, c->length);

	return output;
}

OVUM_API String *String_ConcatRange(ThreadHandle thread, const unsigned int count, String *values[])
{
	if (count == 0)
		return static_strings::empty;
	if (count == 1)
		return values[0];

	int32_t outLength = 0;
	for (unsigned int i = 0; i < count; i++)
	{
		int32_t strlen = values[i]->length;
		if (INT32_MAX - outLength < strlen)
			_Th(thread)->ThrowOverflowError();

		outLength += strlen;
	}

	String *output;
	GC::gc->ConstructString(_Th(thread), outLength, nullptr, &output);
	uchar *outputChar = const_cast<uchar*>(&output->firstChar);

	for (unsigned int i = 0; i < count; i++)
	{
		String *str = values[i];
		CopyMemoryT(outputChar, &str->firstChar, str->length);
		outputChar += str->length;
	}

	return output;
}

OVUM_API const int String_ToWString(wchar_t *dest, const String *source)
{
	if (sizeof(wchar_t) == sizeof(uchar))
	{
		// assume wchar_t is UTF-16 (or at least UCS-2, but hopefully surrogates won't break things too much)
		int outputLength = source->length + 1; // Include the \0

		if (dest)
			memcpy(dest, &source->firstChar, outputLength * sizeof(uchar));

		return outputLength; // Do include \0
	}
	else if (sizeof(wchar_t) == sizeof(wuchar))
	{
		// assume sizeof(wchar_t) == 4, which is probably UTF-32

		// First, iterate over the string to find out how many surrogate pairs there are,
		// if any. These consume only one UTF-32 character.
		// We use this to calculate the length of the output (including the \0).
		int outputLength = 0;

		int32_t strLen = source->length + 1; // let's include the \0
		const uchar *strp = &source->firstChar;
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
	}
	else
		return -1; // not supported
}

OVUM_API String *String_FromCString(ThreadHandle thread, const char *source)
{
	return GC_ConvertString(thread, source);
}

OVUM_API String *String_FromWString(ThreadHandle thread, const wchar_t *source)
{
	if (sizeof(wchar_t) == sizeof(uchar))
	{
		// assume wchar_t is UTF-16 (or at least UCS-2)
		size_t length = wcslen(source);

		String *output;
		GC::gc->ConstructString(_Th(thread), length, (const uchar*)source, &output);

		return output;
	}
	else if (sizeof(wchar_t) == sizeof(wuchar))
	{
		// assume wchar_t is UTF-32
		int32_t outLength = 0;
		size_t sourceLength = wcslen(source);

		const wchar_t *strp = source;
		while (*strp)
		{
			if (UC_NeedsSurrogatePair((const wuchar)*strp))
				outLength += 2;
			else
				outLength++;
			strp++;
		}

		MutableString *output;
		GC::gc->ConstructString(_Th(thread), outLength, nullptr, (String**)&output);

		if (outLength)
		{
			const wchar_t *strp = source;
			uchar *outp = &output->firstChar;
			while (*strp)
			{
				const wuchar ch = (const wuchar)*strp;
				if (UC_NeedsSurrogatePair(ch))
				{
					const SurrogatePair surr = UC_ToSurrogatePair(ch);
					*outp = surr.lead;
					outp++;
					*outp = surr.trail;
				}
				else
					*outp = (uchar)ch;

				outp++;
			}
		}

		return (String*)output;
	}
	else
		return nullptr; /// not supported :(
}