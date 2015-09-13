#include "string.h"
#include "char.h"
#include "../aves_state.h"
#include <ov_stringbuffer.h>
#include <ov_string.h>
#include <ov_unicode.h>

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

int GetIndex(ThreadHandle thread, String *str, Value *arg, int32_t &result)
{
	Aves *aves = Aves::Get(thread);

	int r = IntFromValue(thread, arg);
	if (r != OVUM_SUCCESS) return r;
	int64_t index = arg->v.integer;
	if (index < 0 || index >= str->length)
	{
		VM_PushString(thread, strings::index); // paramName
		VM_PushString(thread, error_strings::IndexOutOfRange); // message
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentRangeError, 2);
	}

	result = (int32_t)index;
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_String_get_item)
{
	Aves *aves = Aves::Get(thread);

	String *str = THISV.v.string;

	int32_t index;
	CHECKED(GetIndex(thread, str, args + 1, index));

	Value output;
	output.type = aves->aves.Char;
	output.v.integer = (&str->firstChar)[index];
	VM_Push(thread, &output);
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_String_get_length)
{
	VM_PushInt(thread, THISV.v.string->length);
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
		result = String_Contains(THISV.v.string, args[1].v.string);
	else if (args[1].type == aves->aves.Char)
	{
		LitString<2> value = Char::ToLitString((ovwchar_t)args[1].v.integer);
		result = String_Contains(THISV.v.string, value.AsString());
	}
	else
		return VM_ThrowTypeError(thread);

	VM_PushBool(thread, result);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_String_startsWith)
{
	Aves *aves = Aves::Get(thread);

	String *str = THISV.v.string;

	bool result;
	if (args[1].type == aves->aves.String)
		result = String_SubstringEquals(str, 0, args[1].v.string);
	else if (args[1].type == aves->aves.Char)
	{
		LitString<2> part = Char::ToLitString((ovwchar_t)args[1].v.integer);
		result = String_SubstringEquals(str, 0, part.AsString());
	}
	else
		return VM_ThrowTypeError(thread);

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
		return VM_ThrowTypeError(thread);

	VM_PushBool(thread, result);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_String_indexOf)
{
	Aves *aves = Aves::Get(thread);

	String *str = THISV.v.string;

	int32_t index;
	if (args[1].type == aves->aves.String)
		index = string::IndexOf(str, args[1].v.string);
	else if (args[1].type == aves->aves.Char)
	{
		LitString<2> part = Char::ToLitString((ovwchar_t)args[1].v.integer);
		index = string::IndexOf(str, part.AsString());
	}
	else
		return VM_ThrowTypeError(thread);

	if (index == -1)
		VM_PushNull(thread);
	else
		VM_PushInt(thread, index);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_String_lastIndexOf)
{
	Aves *aves = Aves::Get(thread);

	String *str = THISV.v.string;

	int32_t index;
	if (args[1].type == aves->aves.String)
		index = string::LastIndexOf(str, args[1].v.string);
	else if (args[1].type == aves->aves.Char)
	{
		LitString<2> part = Char::ToLitString((ovwchar_t)args[1].v.integer);
		index = string::LastIndexOf(str, part.AsString());
	}
	else
		return VM_ThrowTypeError(thread);

	if (index == -1)
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

	int32_t remaining = outputString->length;
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

AVES_API BEGIN_NATIVE_FUNCTION(aves_String_substr1)
{
	// substr(start)
	String *str = THISV.v.string;

	int32_t start;
	CHECKED(GetIndex(thread, str, args + 1, start));
	int32_t count = str->length - start;

	if (start == 0)
	{
		VM_PushString(thread, str);
		RETURN_SUCCESS;
	}
	if (count == 0)
	{
		VM_PushString(thread, strings::Empty);
		RETURN_SUCCESS;
	}

	String *output;
	CHECKED_MEM(output= GC_ConstructString(thread, count, &str->firstChar + start));
	
	VM_PushString(thread, output);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_String_substr2)
{
	// substr(start, count)
	Aves *aves = Aves::Get(thread);

	String *str = THISV.v.string;

	int32_t start;
	CHECKED(GetIndex(thread, str, args + 1, start));
	CHECKED(IntFromValue(thread, args + 2));
	int64_t count = args[2].v.integer;
	if (start + count > str->length)
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentRangeError, 0);

	if (start == 0 && count == str->length)
	{
		VM_PushString(thread, str);
		RETURN_SUCCESS;
	}
	if (count == 0)
	{
		VM_PushString(thread, strings::Empty);
		RETURN_SUCCESS;
	}

	String *output;
	CHECKED_MEM(output = GC_ConstructString(thread, (int32_t)count, &str->firstChar + start));

	VM_PushString(thread, output);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_String_format)
{
	Value *values = args + 1;

	String *result = nullptr;
	{ Pinned str(THISP);
		if (IsType(values, GetType_List(thread)))
			CHECKED(string::Format(thread, str->v.string, values->v.list, result));
		else if (IsType(values, GetType_Hash(thread)))
			CHECKED(string::Format(thread, str->v.string, values, result));
		else
			return VM_ThrowTypeError(thread, error_strings::FormatValueType);
	}
	CHECKED_MEM(result);

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
		int32_t remaining = str->length;
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
		int32_t index = 0;
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
	if (minLength64 < 0 || minLength64 > INT32_MAX)
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
	int32_t padLength = (int32_t)minLength64 - str->length;
	if (padLength <= 0)
	{
		VM_PushString(thread, str);
		RETURN_SUCCESS;
	}

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
				int32_t padBefore = padLength / 2;
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
	int32_t index;
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

AVES_API BEGIN_NATIVE_FUNCTION(aves_String_getCodepoint)
{
	String *str = THISV.v.string;
	int32_t index;
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
	int32_t index;
	CHECKED(GetIndex(thread, str, args + 1, index));

	UnicodeCategory cat = UC_GetCategory(&str->firstChar, (unsigned int)index);

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
	int32_t index;
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

AVES_API NATIVE_FUNCTION(aves_String_getHashCodeSubstr)
{
	// getHashCodeSubstr(index is Int, count is Int)
	// index and count are range-checked in the wrapper function.
	int32_t index = (int32_t)args[1].v.integer;
	int32_t count = (int32_t)args[2].v.integer;
	int32_t hashCode = String_GetHashCodeSubstr(THISV.v.string, index, count);

	VM_PushInt(thread, hashCode);
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_String_fromCodepoint)
{
	Aves *aves = Aves::Get(thread);

	if (args[0].type != aves->aves.Char)
		CHECKED(IntFromValue(thread, args));
	int64_t cp64 = args[0].v.integer;

	if (cp64 < 0 || cp64 > 0x10FFFF)
	{
		VM_PushString(thread, strings::codepoint); // paramName
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
		return VM_ThrowTypeError(thread);

	VM_PushInt(thread, result);
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_String_opMultiply)
{
	Aves *aves = Aves::Get(thread);

	CHECKED(IntFromValue(thread, args + 1));

	int64_t times = args[1].v.integer;
	if (times == 0)
	{
		VM_PushString(thread, strings::Empty);
		RETURN_SUCCESS;
	}

	String *str = THISV.v.string;
	int64_t length;
	if (Int_MultiplyChecked(times, str->length, length) != OVUM_SUCCESS)
		return VM_ThrowOverflowError(thread);
	if (length > INT32_MAX)
	{
		VM_PushString(thread, strings::times); // paramName
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentRangeError, 1);
	}

	StringBuffer buf;
	CHECKED_MEM(buf.Init((int32_t)length));

	for (int32_t i = 0; i < (int32_t)times; i++)
		CHECKED_MEM(buf.Append(str));

	String *result;
	CHECKED_MEM(result = buf.ToString(thread));
	VM_PushString(thread, result);
}
END_NATIVE_FUNCTION

int32_t string::IndexOf(const String *str, const String *part)
{
	const ovchar_t *strp = &str->firstChar;

	int32_t imax = str->length - part->length + 1;
	for (int32_t i = 0; i < imax; i++)
	{
		if (strp[i] == part->firstChar &&
			String_SubstringEquals(str, i, part))
			return i;
	}

	return -1;
}

int32_t string::LastIndexOf(const String *str, const String *part)
{
	const ovchar_t *strp = &str->firstChar;

	for (int32_t i = str->length - part->length; i >= 0; i--)
	{
		if (strp[i] == part->firstChar &&
			String_SubstringEquals(str, i, part))
			return i;
	}

	return -1;
}

#define CHECKED_F(expr)      if ((r = (expr)) != OVUM_SUCCESS) goto failure;
#define CHK_APPEND(buf, ...) if (!(buf).Append(__VA_ARGS__)) goto failure;

enum class FormatAlignment
{
	LEFT   = 0,
	CENTER = 1,
	RIGHT  = 2,
};

inline int AppendAlignedFormatString(StringBuffer &buf, String *value,
	FormatAlignment alignment, uint32_t alignmentWidth)
{
	uint32_t valueLength = (uint32_t)value->length;
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

int ScanDecimalNumber(ThreadHandle thread, const ovchar_t * &chp, unsigned int &index, uint32_t &number)
{
	uint32_t result = 0;

	do
	{
		uint32_t num = *chp - '0';

		uint64_t temp = (uint64_t)result * 10ULL + num;
		if (temp > UINT32_MAX)
			return VM_ThrowOverflowError(thread);
		result = (uint32_t)temp;

		chp++;
		index++;
	} while (*chp >= '0' && *chp <= '9');

	number = result;
	RETURN_SUCCESS;
}

int string::Format(ThreadHandle thread, const String *format, ListInst *list, String *&result)
{
	int r;
	StringBuffer buf;
	if (!buf.Init(format->length)) { r = OVUM_ERROR_NO_MEMORY; goto failure; }

	const uint32_t length = format->length;
	unsigned int start = 0;
	unsigned int index = 0;

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
				uint32_t placeholderIndex;
				CHECKED_F(ScanDecimalNumber(thread, chp, index, placeholderIndex));
				// chp is now after the last digit in the placeholder index

				uint32_t alignmentWidth = 0;
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

				if (*chp != '}' || placeholderIndex >= (uint32_t)list->length)
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

template<int BufLen>
int ScanFormatIdentifier(ThreadHandle thread, LitString<BufLen> &buffer,
	unsigned int &index, const ovchar_t * &chp, Value &result)
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

	int32_t length = 0;
	while (true)
	{
		int skip = 1 + surrogate;
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
		*const_cast<int32_t*>(&buffer.length) = length;
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

int string::Format(ThreadHandle thread, const String *format, Value *hash, String *&result)
{
	int r;
	StringBuffer buf;
	if (!buf.Init(format->length)) { r = OVUM_ERROR_NO_MEMORY; goto failure; }

	const uint32_t length = format->length;
	unsigned int start = 0;
	unsigned int index = 0;

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

				uint32_t alignmentWidth = 0;
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

String *string::Replace(ThreadHandle thread, String *input, const ovchar_t oldChar, const ovchar_t newChar, const int64_t maxTimes)
{
	String *output = GC_ConstructString(thread, input->length, &input->firstChar);
	if (output)
	{
		ovchar_t *outp = const_cast<ovchar_t*>(&output->firstChar);
		int64_t remaining = maxTimes;
		int32_t length = input->length;
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

String *string::Replace(ThreadHandle thread, String *input, String *oldValue, String *newValue, const int64_t maxTimes)
{
	StringBuffer buf;
	if (!buf.Init(input->length)) goto failure;

	const ovchar_t *inp = &input->firstChar;
	int32_t imax = input->length - oldValue->length + 1;

	int32_t start = 0;
	int32_t lengthCollected = 0;
	int64_t remaining = maxTimes;

	int32_t i = 0;
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
