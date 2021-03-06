#include "string.h"
#include "char.h"
#include "../aves_state.h"
#include <ovum_stringbuffer.h>
#include <ovum_string.h>
#include <ovum_unicode.h>

using namespace aves;

namespace error_strings
{
	LitString<36> _IndexOutOfRange    = LitString<36>::FromCString("String character index out of range.");
	LitString<55> _FormatValueType    = LitString<55>::FromCString("The argument to String.format must be a List or a Hash.");
	LitString<62> _ReplaceEmptyString = LitString<62>::FromCString("The oldValue in a replacement cannot be the empty string (\"\").");

	String *IndexOutOfRange    = _IndexOutOfRange.AsString();
	String *FormatValueType    = _FormatValueType.AsString();
	String *ReplaceEmptyString = _ReplaceEmptyString.AsString();
}

int GetIndex(ThreadHandle thread, String *str, Value *arg, size_t &result)
{
	int r = IntFromValue(thread, arg);
	if (r != OVUM_SUCCESS) return r;
	int64_t index = arg->v.integer;
	if (index < 0 || index >= str->length)
	{
		Aves *aves = Aves::Get(thread);
		VM_PushString(thread, strings::index); // paramName
		VM_PushString(thread, error_strings::IndexOutOfRange); // message
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentRangeError, 2);
	}

	result = (size_t)index;
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_String_get_item)
{
	Aves *aves = Aves::Get(thread);

	String *str = THISV.v.string;

	size_t index;
	CHECKED(GetIndex(thread, str, args + 1, index));

	Value output;
	output.type = aves->aves.Char;
	output.v.integer = (&str->firstChar)[index];
	VM_Push(thread, &output);
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_String_get_length)
{
	VM_PushInt(thread, (int64_t)THISV.v.string->length);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_String_get_isInterned)
{
	String *str = THISV.v.string;
	VM_PushBool(thread, (str->flags & StringFlags::INTERN) == StringFlags::INTERN);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_String_equalsIgnoreCase)
{
	Aves *aves = Aves::Get(thread);

	bool eq;
	if (args[1].type == aves->aves.String)
		eq = String_EqualsIgnoreCase(THISV.v.string, args[1].v.string);
	else if (args[1].type == aves->aves.Char)
	{
		LitString<2> other = Char::ToLitString((ovwchar_t)args[1].v.integer);
		eq = String_EqualsIgnoreCase(THISV.v.string, other.AsString());
	}
	else
		eq = false;

	VM_PushBool(thread, eq);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_String_contains)
{
	Aves *aves = Aves::Get(thread);

	bool result;
	if (args[1].type == aves->aves.String)
	{
		result = String_Contains(THISV.v.string, args[1].v.string);
	}
	else if (args[1].type == aves->aves.Char)
	{
		LitString<2> value = Char::ToLitString((ovwchar_t)args[1].v.integer);
		result = String_Contains(THISV.v.string, value.AsString());
	}
	else
	{
		VM_PushString(thread, strings::value); // paramName
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentTypeError, 1);
	}

	VM_PushBool(thread, result);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_String_startsWith)
{
	Aves *aves = Aves::Get(thread);

	String *str = THISV.v.string;

	bool result;
	if (args[1].type == aves->aves.String)
	{
		result = String_SubstringEquals(str, 0, args[1].v.string);
	}
	else if (args[1].type == aves->aves.Char)
	{
		LitString<2> part = Char::ToLitString((ovwchar_t)args[1].v.integer);
		result = String_SubstringEquals(str, 0, part.AsString());
	}
	else
	{
		VM_PushString(thread, strings::value); // paramName
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentTypeError, 1);
	}

	VM_PushBool(thread, result);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_String_endsWith)
{
	Aves *aves = Aves::Get(thread);

	String *str = THISV.v.string;

	bool result;
	if (args[1].type == aves->aves.String)
	{
		String *part = args[1].v.string;
		result = String_SubstringEquals(str, str->length - part->length, part);
	}
	else if (args[1].type == aves->aves.Char)
	{
		LitString<2> part = Char::ToLitString((ovwchar_t)args[1].v.integer);
		result = String_SubstringEquals(str, str->length - part.length, part.AsString());
	}
	else
	{
		VM_PushString(thread, strings::value); // paramName
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentTypeError, 1);
	}

	VM_PushBool(thread, result);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_String_indexOfInternal)
{
	// indexOfInternal(value is String, startIndex is Int, count is Int)
	// The public-facing methods range-check all the values.
	Aves *aves = Aves::Get(thread);

	String *str = THISV.v.string;
	String *part = args[1].v.string;
	size_t startIndex = (size_t)args[2].v.integer;
	size_t count = (size_t)args[3].v.integer;

	size_t index = string::IndexOf(str, part, startIndex, count);

	if (index == string::NOT_FOUND)
		VM_PushNull(thread);
	else
		VM_PushInt(thread, index);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_String_lastIndexOf)
{
	Aves *aves = Aves::Get(thread);

	String *str = THISV.v.string;

	size_t index;
	if (args[1].type == aves->aves.String)
	{
		index = string::LastIndexOf(str, args[1].v.string);
	}
	else if (args[1].type == aves->aves.Char)
	{
		LitString<2> part = Char::ToLitString((ovwchar_t)args[1].v.integer);
		index = string::LastIndexOf(str, part.AsString());
	}
	else
	{
		VM_PushString(thread, strings::value); // paramName
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentTypeError, 1);
	}

	if (index == string::NOT_FOUND)
		VM_PushNull(thread);
	else
		VM_PushInt(thread, index);
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_String_reverse)
{
	String *outputString;
	CHECKED_MEM(outputString = GC_ConstructString(thread, THISV.v.string->length, nullptr));

	Value *output = VM_Local(thread, 0);
	SetString(thread, output, outputString);

	const ovchar_t *srcp = &THISV.v.string->firstChar;
	ovchar_t *dstp = const_cast<ovchar_t*>(&outputString->firstChar + outputString->length - 1);

	size_t remaining = outputString->length;
	while (remaining-- > 0)
	{
		if (UC_IsSurrogateLead(*srcp) && UC_IsSurrogateTrail(*(srcp + 1)))
		{
			*reinterpret_cast<uint32_t*>(--dstp) = *reinterpret_cast<const uint32_t*>(srcp++);
			remaining--;
		}
		else
			*dstp = *srcp;
		dstp--;
		srcp++;
	}

	VM_Push(thread, output);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_String_substringInternal)
{
	// substringInternal(startIndex is Int, count is Int)
	// Public-facing methods check the types and range-check the values.
	Aves *aves = Aves::Get(thread);

	String *str = THISV.v.string;
	size_t startIndex = (size_t)args[1].v.integer;
	size_t count = (size_t)args[2].v.integer;

	String *output;
	if (count == 0)
	{
		output = strings::Empty;
	}
	else if (startIndex == 0 && count == str->length)
	{
		// The substring spans the entire string. We can just return the original.
		output = str;
	}
	else
	{
		CHECKED_MEM(output = GC_ConstructString(thread, count, &str->firstChar + startIndex));
	}

	VM_PushString(thread, output);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_String_format)
{
	Aves *aves = Aves::Get(thread);

	Value *values = args + 1;

	String *result = nullptr;
	{ Pinned str(THISP);
		if (IsType(values, GetType_List(thread)))
		{
			CHECKED(string::Format(thread, str->v.string, values->v.list, result));
		}
		else if (IsType(values, GetType_Hash(thread)))
		{
			CHECKED(string::Format(thread, str->v.string, values, result));
		}
		else
		{
			VM_PushString(thread, strings::values); // paramName
			return VM_ThrowErrorOfType(thread, aves->aves.ArgumentTypeError, 1);
		}
	}
	CHECKED_MEM(result);

	VM_PushString(thread, result);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_String_repeat)
{
	Aves *aves = Aves::Get(thread);

	CHECKED(IntFromValue(thread, args + 1));

	int64_t times = args[1].v.integer;
	if (times < 0)
	{
		VM_PushString(thread, strings::times); // paramName
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentRangeError, 1);
	}

	if (times == 0)
	{
		VM_PushString(thread, strings::Empty);
		RETURN_SUCCESS;
	}

	String *str = THISV.v.string;
	uint64_t length;
	if (UInt_MultiplyChecked((uint64_t)times, (uint64_t)str->length, length) != OVUM_SUCCESS)
		return VM_ThrowOverflowError(thread);
	if (length > OVUM_ISIZE_MAX)
	{
		VM_PushString(thread, strings::times); // paramName
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentRangeError, 1);
	}

	StringBuffer buf;
	CHECKED_MEM(buf.Init((size_t)length));

	for (size_t i = 0; i < (size_t)times; i++)
		CHECKED_MEM(buf.Append(str));

	String *result;
	CHECKED_MEM(result = buf.ToString(thread));
	VM_PushString(thread, result);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_String_replaceInner)
{
	// replaceInner(oldValue is String, newValue is String, maxTimes is Int)
	// (Public-facing methods ensure the types are correct)
	Aves *aves = Aves::Get(thread);

	String *oldValue = args[1].v.string;
	if (oldValue->length == 0)
	{
		VM_PushString(thread, error_strings::ReplaceEmptyString);
		VM_PushString(thread, strings::oldValue);
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentError, 2);
	}

	if (args[3].v.integer == 0) // No replacements to perform! Return 'this'.
	{
		VM_PushString(thread, THISV.v.string);
		RETURN_SUCCESS;
	}

	String *newValue = args[2].v.string;

	String *result;
	if (oldValue->length == 1 && newValue->length == 1)
		result = string::Replace(thread, THISV.v.string, oldValue->firstChar, newValue->firstChar, args[3].v.integer);
	else
		result = string::Replace(thread, THISV.v.string, oldValue, newValue, args[3].v.integer);
	CHECKED_MEM(result);

	VM_PushString(thread, result);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_String_splice)
{
	// splice(startIndex is Int, removeCount is Int, newValue is String)
	// Public-facing methods check the types and range-check the values.
	PinnedAlias<String> str(THISP), newValue(args + 3);
	size_t startIndex = (size_t)args[1].v.integer;
	size_t removeCount = (size_t)args[2].v.integer;

	size_t resultLength = str->length - removeCount + newValue->length;
	String *result;
	CHECKED_MEM(result = GC_ConstructString(thread, resultLength, nullptr));

	const ovchar_t *srcp = &str->firstChar;
	ovchar_t *destp = const_cast<ovchar_t*>(&result->firstChar);

	// Copy the first part of the source string into the result.
	if (startIndex > 0)
	{
		CopyMemoryT(destp, srcp, startIndex);
		srcp += startIndex;
		destp += startIndex;
	}

	// Insert the new value.
	if (newValue->length > 0)
	{
		CopyMemoryT(destp, &newValue->firstChar, newValue->length);
		destp += newValue->length;
	}

	// Skip the part of the source string that is to be removed.
	srcp += removeCount;

	// And finally insert the remainder of the source string.
	if (str->length > startIndex + removeCount)
	{
		size_t remaining = str->length - startIndex - removeCount;
		CopyMemoryT(destp, srcp, remaining);
	}

	VM_PushString(thread, result);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_String_split)
{
	// arguments: (separator)
	// locals: { output is List }

	CHECKED(StringFromValue(thread, args + 1));
	PinnedAlias<String> str(THISP), sep(args + 1);

	Value *output = VM_Local(thread, 0);
	Value ignore;

	if (sep->length == 0) // Split into separate characters
	{
		// Construct the output list
		VM_PushInt(thread, str->length);
		CHECKED(GC_Construct(thread, GetType_List(thread), 1, output));

		// And then copy each individual character to the output
		const ovchar_t *chp = &str->firstChar;
		size_t remaining = str->length;
		while (remaining-- > 0)
		{
			VM_Push(thread, output);
			VM_PushString(thread, GC_ConstructString(thread, 1, chp++));
			CHECKED(VM_InvokeMember(thread, strings::add, 1, &ignore));
		}
	}
	else
	{
		// Construct the output list
		VM_PushInt(thread, str->length / 2);
		CHECKED(GC_Construct(thread, GetType_List(thread), 1, output));

		const ovchar_t *chp = &str->firstChar;
		const ovchar_t *chStart = chp;
		size_t index = 0;
		while (index < str->length)
		{
			if (*chp == sep->firstChar)
			{
				if (String_SubstringEquals(*str, index, *sep))
				{
					// We mound a fatch! I mean, we found a match!
					// Copy characters from chStart to chp into the output,
					// and be aware that chp is inclusive.
					VM_Push(thread, output);
					if (chp == chStart)
						VM_PushString(thread, strings::Empty);
					else
					{
						String *part;
						CHECKED_MEM(part = GC_ConstructString(thread, chp - chStart, chStart));
						VM_PushString(thread, part);
					}
					CHECKED(VM_InvokeMember(thread, strings::add, 1, &ignore));
					index += sep->length;
					chp += sep->length;
					chStart = chp;
					continue;
				}
			}
			index++;
			chp++;
		}

		// And add the last bit of the string, too
		VM_Push(thread, output);
		String *rest;
		if (chStart == &str->firstChar)
			// No match found, just add the entire string
			rest = *str;
		else if (chp == chStart)
			rest = strings::Empty;
		else
			CHECKED_MEM(rest = GC_ConstructString(thread, chp - chStart, chStart));
		VM_PushString(thread, rest);
		CHECKED(VM_InvokeMember(thread, strings::add, 1, &ignore));
	}

	VM_Push(thread, output);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_String_padInner)
{
	// padInner(minLength is Int, char is Char, side is StringPad)
	// The public-facing methods make sure char is of length 1, so
	// we can safely cast it to ovchar_t here.
	Aves *aves = Aves::Get(thread);

	int64_t minLength64 = args[1].v.integer;
	if (minLength64 < 0 || minLength64 > OVUM_ISIZE_MAX)
	{
		VM_PushString(thread, strings::minLength); // paramName
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentRangeError, 1);
	}

	StringPad side = (StringPad)args[3].v.integer;
	if (side < PAD_START || side > PAD_BOTH)
	{
		VM_PushString(thread, strings::side); // paramName
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentRangeError, 1);
	}

	String *str = THISV.v.string;
	if (str->length >= (size_t)minLength64)
	{
		VM_PushString(thread, str);
		RETURN_SUCCESS;
	}

	size_t padLength = (size_t)minLength64 - str->length;

	String *result;
	{ Pinned s(THISP);
		CHECKED_MEM(result = GC_ConstructString(thread, str->length + padLength, nullptr));
		ovchar_t *resultp = const_cast<ovchar_t*>(&result->firstChar);

		ovchar_t ch = (ovchar_t)args[2].v.integer;
		switch (side)
		{
		case PAD_START:
			while (padLength-- > 0)
				*resultp++ = ch;

			CopyMemoryT(resultp, &str->firstChar, str->length);
			break;
		case PAD_END:
			CopyMemoryT(resultp, &str->firstChar, str->length);
			resultp += str->length;

			while (padLength-- > 0)
				*resultp++ = ch;
			break;
		case PAD_BOTH:
			{
				size_t padBefore = padLength / 2;
				padLength -= padBefore;
				while (padBefore-- > 0)
					*resultp++ = ch;

				CopyMemoryT(resultp, &str->firstChar, str->length);
				resultp += str->length;

				while (padLength-- > 0)
					*resultp++ = ch;
			}
			break;
		}
	}

	VM_PushString(thread, result);
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_String_toUpper)
{
	String *result = String_ToUpper(thread, THISV.v.string);
	if (result == nullptr) return OVUM_ERROR_NO_MEMORY;
	VM_PushString(thread, result);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_String_toLower)
{
	String *result = String_ToLower(thread, THISV.v.string);
	if (result == nullptr) return OVUM_ERROR_NO_MEMORY;
	VM_PushString(thread, result);
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_String_getCharacter)
{
	Aves *aves = Aves::Get(thread);

	String *str = THISV.v.string;
	size_t index;
	CHECKED(GetIndex(thread, str, args + 1, index));

	const ovchar_t *chp = &str->firstChar + index;

	ovwchar_t result;
	if (UC_IsSurrogateLead(chp[0]) && UC_IsSurrogateTrail(chp[1]))
		result = UC_ToWide(chp[0], chp[1]);
	else
		result = *chp;

	Value character;
	character.type = aves->aves.Char;
	character.v.integer = result;
	VM_Push(thread, &character);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_String_getCodePoint)
{
	String *str = THISV.v.string;
	size_t index;
	CHECKED(GetIndex(thread, str, args + 1, index));

	const ovchar_t *chp = &str->firstChar + index;

	ovwchar_t result;
	if (UC_IsSurrogateLead(chp[0]) && UC_IsSurrogateTrail(chp[1]))
		result = UC_ToWide(chp[0], chp[1]);
	else
		result = *chp;

	VM_PushInt(thread, result);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_String_getCategory)
{
	Aves *aves = Aves::Get(thread);

	String *str = THISV.v.string;
	size_t index;
	CHECKED(GetIndex(thread, str, args + 1, index));

	UnicodeCategory cat = UC_GetCategory(&str->firstChar, index);

	// The values of native type UnicodeCategory are not the same as
	// the values of the Osprey type, so we need to convert!

	Value output;
	output.type = aves->aves.UnicodeCategory;
	output.v.integer = unicode::OvumCategoryToAves(cat);
	VM_Push(thread, &output);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_String_isSurrogatePair)
{
	String *str = THISV.v.string;
	size_t index;
	CHECKED(GetIndex(thread, str, args + 1, index));

	VM_PushBool(thread, UC_IsSurrogateLead((&str->firstChar)[index]) &&
		UC_IsSurrogateTrail((&str->firstChar)[index + 1]));
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_String_getInterned)
{
	String *str = THISV.v.string;
	str = String_GetInterned(thread, str);
	if (str == nullptr)
		VM_PushNull(thread);
	else
		VM_PushString(thread, str);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_String_intern)
{
	String *str = THISV.v.string;
	VM_PushString(thread, String_Intern(thread, str));
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_String_getHashCode)
{
	int32_t hashCode = String_GetHashCode(THISV.v.string);

	VM_PushInt(thread, hashCode);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_String_getHashCodeSubstring)
{
	// getHashCodeSubstring(index is Int, count is Int)
	// index and count are range-checked in the wrapper function.
	size_t index = (size_t)args[1].v.integer;
	size_t count = (size_t)args[2].v.integer;
	int32_t hashCode = String_GetHashCodeSubstr(THISV.v.string, index, count);

	VM_PushInt(thread, hashCode);
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_String_fromCodePoint)
{
	Aves *aves = Aves::Get(thread);

	if (args[0].type != aves->aves.Char)
		CHECKED(IntFromValue(thread, args));
	int64_t cp64 = args[0].v.integer;

	if (cp64 < 0 || cp64 > 0x10FFFF)
	{
		VM_PushString(thread, strings::codePoint); // paramName
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentRangeError, 1);
	}

	String *output;
	if (UC_NeedsSurrogatePair((ovwchar_t)cp64))
	{
		SurrogatePair pair = UC_ToSurrogatePair((ovwchar_t)cp64);
		output = GC_ConstructString(thread, 2, reinterpret_cast<ovchar_t*>(&pair));
	}
	else
	{
		ovchar_t cp = (ovchar_t)cp64;
		output = GC_ConstructString(thread, 1, reinterpret_cast<ovchar_t*>(&cp));
	}
	CHECKED_MEM(output);

	// Return value is on the stack
	VM_PushString(thread, output);
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_String_opEquals)
{
	Aves *aves = Aves::Get(thread);

	bool eq;
	if (args[1].type == aves->aves.String)
		eq = String_Equals(args[0].v.string, args[1].v.string);
	else if (args[1].type == aves->aves.Char)
	{
		LitString<2> right = Char::ToLitString((ovwchar_t)args[1].v.integer);
		eq = String_Equals(args[0].v.string, right.AsString());
	}
	else
		eq = false;

	VM_PushBool(thread, eq);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_String_opCompare)
{
	Aves *aves = Aves::Get(thread);

	int result;
	if (args[1].type == aves->aves.String)
		result = String_Compare(args[0].v.string, args[1].v.string);
	else if (args[1].type == aves->aves.Char)
	{
		LitString<2> right = Char::ToLitString((ovwchar_t)args[1].v.integer);
		result = String_Compare(args[0].v.string, right.AsString());
	}
	else
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentTypeError, 0);

	VM_PushInt(thread, result);
	RETURN_SUCCESS;
}

size_t string::IndexOf(const String *str, const String *part, size_t startIndex, size_t count)
{
	if (part->length == 0)
		return startIndex;

	const ovchar_t *strp = &str->firstChar;
	ovchar_t firstPartChar = part->firstChar;

	size_t endIndex = startIndex + count - part->length + 1;
	for (size_t i = startIndex; i < endIndex; i++)
	{
		if (strp[i] == firstPartChar &&
			String_SubstringEquals(str, i, part))
			return i;
	}

	return NOT_FOUND;
}

size_t string::LastIndexOf(const String *str, const String *part)
{
	const ovchar_t *strp = &str->firstChar;

	for (size_t i = str->length - part->length; i >= 0; i--)
	{
		if (strp[i] == part->firstChar &&
			String_SubstringEquals(str, i, part))
			return i;
	}

	return NOT_FOUND;
}

#define CHECKED_F(expr)      if ((r = (expr)) != OVUM_SUCCESS) goto failure;
#define CHK_APPEND(buf, ...) if (!(buf).Append(__VA_ARGS__)) goto failure;

enum class FormatAlignment
{
	LEFT   = 0,
	CENTER = 1,
	RIGHT  = 2,
};

inline int AppendAlignedFormatString(
	StringBuffer &buf,
	String *value,
	FormatAlignment alignment,
	size_t alignmentWidth
)
{
	size_t valueLength = value->length;
	switch (alignment)
	{
	case FormatAlignment::LEFT:
		CHK_APPEND(buf, value);

		if (valueLength < alignmentWidth)
			CHK_APPEND(buf, alignmentWidth - valueLength, ' ');
		break;
	case FormatAlignment::CENTER:
		if (valueLength < alignmentWidth)
			CHK_APPEND(buf, (alignmentWidth - valueLength) / 2, ' ');

		CHK_APPEND(buf, value);

		if (valueLength < alignmentWidth)
			CHK_APPEND(buf, (alignmentWidth - valueLength + 1) / 2, ' ');
		break;
	case FormatAlignment::RIGHT:
		if (valueLength < alignmentWidth)
			CHK_APPEND(buf, alignmentWidth - valueLength, ' ');

		CHK_APPEND(buf, value);
		break;
	}
	RETURN_SUCCESS;

failure:
	return OVUM_ERROR_NO_MEMORY;
}

int ScanDecimalNumber(
	ThreadHandle thread,
	const ovchar_t * &chp,
	size_t &index,
	size_t &number
)
{
	size_t result = 0;

	do
	{
		size_t num = *chp - '0';

		uint64_t temp = (uint64_t)result * 10ULL + num;
		if (temp > UINT32_MAX)
			return VM_ThrowOverflowError(thread);
		result = (size_t)temp;

		chp++;
		index++;
	} while (*chp >= '0' && *chp <= '9');

	number = result;
	RETURN_SUCCESS;
}

int string::Format(
	ThreadHandle thread,
	const String *format,
	ListInst *list,
	String *&result
)
{
	int r;
	StringBuffer buf;
	if (!buf.Init(format->length)) { r = OVUM_ERROR_NO_MEMORY; goto failure; }

	const size_t length = format->length;
	size_t start = 0;
	size_t index = 0;

	const ovchar_t *chBase = &format->firstChar;
	const ovchar_t *chp = chBase;

	while (index < length)
	{
		const ovchar_t ch = *chp;

		// In all cases, we need to do this anyway, and this makes it slightly
		// faster to look up the next character after 'ch'.
		chp++;

		switch (ch)
		{
		case '{':
			// {} is not allowed, and { must be followed by at least one digit
			if (*chp == '}' || *chp < '0' || *chp > '9')
				goto formatError;
			// output everything up to (but not including) the {
			if (start < index)
				buf.Append(index - start, chBase + start);

			// Scan placeholder values!
			// Permitted formats:
			//   {idx}
			//   {idx<align}  -- left align
			//   {idx>align}  -- right align
			//   {idx=align}  -- center
			// idx and align are always decimal digits, '0'..'9'
			{
				index++;
				size_t placeholderIndex;
				CHECKED_F(ScanDecimalNumber(thread, chp, index, placeholderIndex));
				// chp is now after the last digit in the placeholder index

				size_t alignmentWidth = 0;
				FormatAlignment alignment = FormatAlignment::LEFT;
				if (*chp == '<' || *chp == '>' || *chp == '=') // alignment follows here, whee
				{
					index++;
					alignment = *chp == '=' ? FormatAlignment::CENTER :
						*chp == '>' ? FormatAlignment::RIGHT :
						FormatAlignment::LEFT;
					chp++;
					alignmentWidth;
					CHECKED_F(ScanDecimalNumber(thread, chp, index, alignmentWidth));
				}

				if (*chp != '}' || placeholderIndex >= list->length)
					goto formatError;

				Value *value = VM_Local(thread, 0);
				*value = list->values[placeholderIndex];
				CHECKED_F(StringFromValue(thread, value));

				CHECKED_F(AppendAlignedFormatString(buf, value->v.string, alignment, alignmentWidth));
			}

			start = ++index;
			chp++;

			break;
		case '\\':
			if (*chp == '{' || *chp == '}')
			{
				// I actually can't end a comment with a backslash. wtf, C++?
				// Output everything up to (but not including) the backslash
				if (start < index)
					CHK_APPEND(buf, index - start, chBase + start);

				start = ++index; // whee
				chp++; // skip to {
				// fall through to default (the { or } is interpreted literally)
			}
			// else: fall through to default case (the \ is interpreted literally)
		default:
			index++;
			break;
		}
	}

	// and then we append all the remaining characters
	if (start < index)
		CHK_APPEND(buf, index - start, chBase + start);

	r = (result = buf.ToString(thread)) ?
		OVUM_SUCCESS :
		OVUM_ERROR_NO_MEMORY;
failure:
	return r;
formatError:
	Aves *aves = Aves::Get(thread);
	VM_PushNull(thread); // message
	VM_PushString(thread, strings::format); // paramName
	return VM_ThrowErrorOfType(thread, aves->aves.ArgumentError, 2);
}

template<size_t BufLen>
int ScanFormatIdentifier(
	ThreadHandle thread,
	LitString<BufLen> &buffer,
	size_t &index,
	const ovchar_t * &chp,
	Value &result
)
{
	const ovchar_t *chStart = chp;
	// Identifiers follow the following format:
	//     [\p{L}\p{Nl}_][\p{L}\p{Nl}\p{Nd}\p{Mn}\p{Mc}\p{Pc}\p{Cf}]*
	// Note that '_' is part of Pc, which is why it's not explicitly mentioned
	// in the second character class.

	bool surrogate;
	UnicodeCategory cat = UC_GetCategory(chp, 0, &surrogate);
	if ((cat & UC_TOP_CATEGORY_MASK) != UC_LETTER &&
		cat != UC_NUMBER_LETTER && *chp != '_')
		goto formatError;

	size_t length = 0;
	while (true)
	{
		size_t skip = 1 + surrogate;
		if (length + skip < buffer.length)
		{
			buffer.chars[length] = *chp;
			if (surrogate)
				buffer.chars[length + 1] = *(chp + 1);
		}
		chp    += skip;
		index  += skip;
		length += skip;

		cat = UC_GetCategory(chp, 0, &surrogate);
		if (!((cat & UC_TOP_CATEGORY_MASK) == UC_LETTER ||
			cat == UC_NUMBER_LETTER || cat == UC_NUMBER_DECIMAL ||
			cat == UC_MARK_NONSPACING || cat == UC_MARK_SPACING ||
			cat == UC_PUNCT_CONNECTOR || cat == UC_FORMAT))
			break; // done
	}

	if (length < buffer.length) // all of the everything is contained within the buffer
	{
		buffer.chars[length] = 0; // trailing 0, always!
		*const_cast<size_t*>(&buffer.length) = length;
		SetString(thread, result, buffer.AsString());
	}
	else
	{
		String *outputString = GC_ConstructString(thread, length, chStart);
		if (outputString == nullptr)
			return OVUM_ERROR_NO_MEMORY;
		SetString(thread, result, outputString);
	}
	RETURN_SUCCESS;

formatError:
	Aves *aves = Aves::Get(thread);
	VM_PushNull(thread);
	VM_PushString(thread, strings::format);
	return VM_ThrowErrorOfType(thread, aves->aves.ArgumentError, 2);
}

int string::Format(
	ThreadHandle thread,
	const String *format,
	Value *hash,
	String *&result
)
{
	int r;
	StringBuffer buf;
	if (!buf.Init(format->length)) { r = OVUM_ERROR_NO_MEMORY; goto failure; }

	const size_t length = format->length;
	size_t start = 0;
	size_t index = 0;

	const ovchar_t *chBase = &format->firstChar;
	const ovchar_t *chp = chBase;

	while (index < length)
	{
		const ovchar_t ch = *chp;

		// In all cases, we need to do this anyway, and this makes it slightly
		// faster to look up the next character after 'ch'.
		chp++;

		switch (ch)
		{
		case '{':
			// {} is not allowed
			if (*chp == '}')
				goto formatError;
			// output everything up to (but not including) the {
			if (start < index)
				CHK_APPEND(buf, index - start, chBase + start);

			// Scan placeholder values!
			// Permitted formats:
			//   {name}
			//   {name<align}  -- left align
			//   {name>align}  -- right align
			//   {name=align}  -- center
			// name is an Osprey identifier; align consists of decimal digits, '0'..'9'
			{
				index++;
				// Most placeholder names are likely to be very short, certainly shorter
				// than 63 characters. Therefore, we have a buffer of 64 uchars, with
				// space reserved for one \0 at the end, which is filled up when reading
				// characters from the string. If only the buffer was used, then ScanFormatIdentifier
				// sets phKey to point to the buffer; otherwise, it allocates a string
				// and sets phKey to that.
				LitString<63> buffer = { 0, 0, StringFlags::STATIC };
				Value phKey;
				CHECKED_F(ScanFormatIdentifier(thread, buffer, index, chp, phKey));
				// chp is now after the last character in the placeholder name

				size_t alignmentWidth = 0;
				FormatAlignment alignment = FormatAlignment::LEFT;
				if (*chp == '<' || *chp == '>' || *chp == '=') // alignment follows here, whee
				{
					index++;
					alignment = *chp == '=' ? FormatAlignment::CENTER :
						*chp == '>' ? FormatAlignment::RIGHT :
						FormatAlignment::LEFT;
					chp++;
					CHECKED_F(ScanDecimalNumber(thread, chp, index, alignmentWidth));
				}

				if (*chp != '}')
					goto formatError;

				// Load the value using the hash's indexer
				Value *value = VM_Local(thread, 0);
				VM_Push(thread, hash);
				VM_Push(thread, &phKey);
				CHECKED_F(VM_LoadIndexer(thread, 1, value));
				// Convert it to a string
				CHECKED_F(StringFromValue(thread, value));
				// And append it
				CHECKED_F(AppendAlignedFormatString(buf, value->v.string, alignment, alignmentWidth));
			}

			start = ++index;
			chp++;

			break;
		case '\\':
			if (*chp == '{' || *chp == '}')
			{
				// I actually can't end a comment with a backslash. wtf, C++?
				// Output everything up to (but not including) the backslash
				if (start < index)
					CHK_APPEND(buf, index - start, chBase + start);

				start = ++index; // whee
				chp++; // skip to {
				// fall through to default (the { or } is interpreted literally)
			}
			// else: fall through to default case (the \ is interpreted literally)
		default:
			index++;
			break;
		}
	}

	// and then we append all the remaining characters
	if (start < index)
		CHK_APPEND(buf, index - start, chBase + start);

	r = (result = buf.ToString(thread)) ?
		OVUM_SUCCESS :
		OVUM_ERROR_NO_MEMORY;

failure:
	return r;

formatError:
	Aves *aves = Aves::Get(thread);
	VM_PushNull(thread);
	VM_PushString(thread, strings::format);
	return VM_ThrowErrorOfType(thread, aves->aves.ArgumentError, 2);
}

String *string::Replace(
	ThreadHandle thread,
	String *input,
	ovchar_t oldChar,
	ovchar_t newChar,
	int64_t maxTimes
)
{
	String *output = GC_ConstructString(thread, input->length, &input->firstChar);
	if (output)
	{
		ovchar_t *outp = const_cast<ovchar_t*>(&output->firstChar);
		int64_t remaining = maxTimes;
		size_t length = input->length;
		while (length-- && (maxTimes < 0 || remaining))
		{
			if (*outp == oldChar)
			{
				*outp = newChar;
				if (maxTimes > 0)
					remaining--;
			}
			outp++;
		}
	}
	return output;
}

String *string::Replace(
	ThreadHandle thread,
	String *input,
	String *oldValue,
	String *newValue,
	int64_t maxTimes
)
{
	StringBuffer buf;
	if (!buf.Init(input->length)) goto failure;

	const ovchar_t *inp = &input->firstChar;
	size_t imax = input->length - oldValue->length + 1;

	size_t start = 0;
	size_t lengthCollected = 0;
	int64_t remaining = maxTimes;

	size_t i = 0;
	while (i < input->length)
	{
		if (i < imax &&
			*inp == oldValue->firstChar &&
			String_SubstringEquals(input, i, oldValue))
		{
			if (lengthCollected > 0)
				if (!buf.Append(lengthCollected, &input->firstChar + start)) goto failure;

			if (!buf.Append(newValue)) goto failure;
			start = i + oldValue->length;
			lengthCollected = 0;
			inp += oldValue->length;
			i += oldValue->length;

			if (maxTimes > 0)
			{
				if (remaining == 1) // last one!
				{
					// We need to append the rest of the original string now.
					lengthCollected = imax - start;
					break;
				}
				else
					remaining--;
			}
		}
		else
		{
			lengthCollected++;
			inp++;
			i++;
		}
	}

	if (lengthCollected == input->length)
		return input; // No match

	if (lengthCollected > 0)
		if (!buf.Append(lengthCollected, &input->firstChar + start))
			goto failure;

	return buf.ToString(thread);
failure:
	return nullptr;
}
