#include "char.h"
#include "string.h"
#include "../aves_state.h"
#include <ov_string.h>
#include <ov_stringbuffer.h>

using namespace aves;

LitString<2> Char::ToLitString(const wuchar ch)
{
	LitString<2> output = {
		ch > 0xFFFF ? 2 : 1,
		0, StringFlags::STATIC
	};

	if (ch > 0xFFFF)
	{
		SurrogatePair pair = UC_ToSurrogatePair(ch);
		output.chars[0] = pair.lead;
		output.chars[1] = pair.trail;
	}
	else
		output.chars[0] = (uchar)ch;

	return output;
}

wuchar Char::FromValue(Value *value)
{
	return (wuchar)value->v.integer;
}

AVES_API NATIVE_FUNCTION(aves_Char_get_length)
{
	wuchar ch = Char::FromValue(THISP);

	VM_PushInt(thread, ch > 0xFFFF ? 2 : 1);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_Char_get_category)
{
	Aves *aves = Aves::Get(thread);

	wuchar ch = Char::FromValue(THISP);

	UnicodeCategory cat = UC_GetCategoryW(ch);

	// The values of native type UnicodeCategory are not the same as
	// the values of the Osprey type, so we need to convert!

	Value catValue;
	catValue.type = aves->aves.UnicodeCategory;
	catValue.v.integer = unicode::OvumCategoryToAves(cat);

	VM_Push(thread, &catValue);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_Char_toUpper)
{
	Aves *aves = Aves::Get(thread);

	wuchar ch = Char::FromValue(THISP);

	Value upper;
	upper.type = aves->aves.Char;
	upper.v.integer = (int32_t)UC_GetCaseMapW(ch).upper;

	VM_Push(thread, &upper);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_Char_toLower)
{
	Aves *aves = Aves::Get(thread);

	wuchar ch = Char::FromValue(THISP);

	Value lower;
	lower.type = aves->aves.Char;
	lower.v.integer = (int32_t)UC_GetCaseMapW(ch).lower;

	VM_Push(thread, &lower);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_Char_getHashCode)
{
	wuchar ch = Char::FromValue(THISP);
	LitString<2> str = Char::ToLitString(ch);

	VM_PushInt(thread, String_GetHashCode(str.AsString()));
	RETURN_SUCCESS;
}
AVES_API BEGIN_NATIVE_FUNCTION(aves_Char_toString)
{
	wuchar ch = Char::FromValue(THISP);
	LitString<2> litStr = Char::ToLitString(ch);

	String *str;
	CHECKED_MEM(str = GC_ConstructString(thread, litStr.length, litStr.chars));

	VM_PushString(thread, str);
	RETURN_SUCCESS;
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_Char_fromCodepoint)
{
	Aves *aves = Aves::Get(thread);

	CHECKED(IntFromValue(thread, args + 0));

	int64_t cp = args[0].v.integer;
	if (cp < 0 || cp > 0x10FFFF)
	{
		VM_PushString(thread, strings::cp);
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentRangeError, 1);
	}

	Value character;
	character.type = aves->aves.Char;
	character.v.integer = cp;
	VM_Push(thread, &character);
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_Char_opEquals)
{
	Aves *aves = Aves::Get(thread);

	bool eq;
	if (args[1].type == aves->aves.Char)
		eq = args[0].v.integer == args[1].v.integer;
	else if (args[1].type == aves->aves.String)
	{
		LitString<2> left = Char::ToLitString((wuchar)args[0].v.integer);
		eq = String_Equals(left.AsString(), args[1].v.string);
	}
	else
		eq = false;

	VM_PushBool(thread, eq);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_Char_opCompare)
{
	Aves *aves = Aves::Get(thread);

	int result;

	if (args[1].type == aves->aves.Char)
		result = (int32_t)args[0].v.integer - (int32_t)args[1].v.integer;
	else if (args[1].type == aves->aves.String)
	{
		LitString<2> left = Char::ToLitString((wuchar)args[0].v.integer);
		result = String_Compare(left.AsString(), args[1].v.string);
	}
	else
		VM_ThrowTypeError(thread);

	VM_PushInt(thread, result);
	RETURN_SUCCESS;
}
AVES_API BEGIN_NATIVE_FUNCTION(aves_Char_opMultiply)
{
	Aves *aves = Aves::Get(thread);

	CHECKED(IntFromValue(thread, args + 1));

	int64_t times = args[1].v.integer;
	if (times == 0)
	{
		VM_PushString(thread, strings::Empty);
		RETURN_SUCCESS;
	}

	LitString<2> str = Char::ToLitString((wuchar)args[0].v.integer);
	int64_t length;
	if (Int_MultiplyChecked(times, str.length, length))
		return VM_ThrowOverflowError(thread);
	if (length > INT32_MAX)
	{
		VM_PushString(thread, strings::times);
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentRangeError, 1);
	}

	StringBuffer buf;
	CHECKED_MEM(buf.Init((int32_t)length));

	for (int32_t i = 0; i < (int32_t)length; i++)
		CHECKED_MEM(buf.Append(str.AsString()));

	String *result;
	CHECKED_MEM(result = buf.ToString(thread));
	VM_PushString(thread, result);
}
END_NATIVE_FUNCTION
AVES_API NATIVE_FUNCTION(aves_Char_opPlus)
{
	VM_PushInt(thread, args[0].v.integer);
	RETURN_SUCCESS;
}