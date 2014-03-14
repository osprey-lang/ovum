#include "aves_char.h"
#include "ov_string.h"
#include "ov_stringbuffer.h"

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

AVES_API NATIVE_FUNCTION(aves_Char_get_length)
{
	wuchar ch = (wuchar)THISV.integer;

	VM_PushInt(thread, ch > 0xFFFF ? 2 : 1);
}

AVES_API NATIVE_FUNCTION(aves_Char_get_category)
{
	wuchar ch = (wuchar)THISV.integer;

	Value cat;
	cat.type = Types::UnicodeCategory;
	cat.integer = (int32_t)UC_GetCategoryW(ch);

	VM_Push(thread, cat);
}

AVES_API NATIVE_FUNCTION(aves_Char_toUpper)
{
	wuchar ch = (wuchar)THISV.integer;

	Value upper;
	upper.type = Types::Char;
	upper.integer = (int32_t)UC_GetCaseMapW(ch).upper;

	VM_Push(thread, upper);
}
AVES_API NATIVE_FUNCTION(aves_Char_toLower)
{
	wuchar ch = (wuchar)THISV.integer;

	Value lower;
	lower.type = Types::Char;
	lower.integer = (int32_t)UC_GetCaseMapW(ch).lower;

	VM_Push(thread, lower);
}

AVES_API NATIVE_FUNCTION(aves_Char_getHashCode)
{
	wuchar ch = (wuchar)THISV.integer;
	LitString<2> str = Char::ToLitString(ch);

	VM_PushInt(thread, String_GetHashCode(_S(str)));
}
AVES_API NATIVE_FUNCTION(aves_Char_toString)
{
	wuchar ch = (wuchar)THISV.integer;
	LitString<2> str = Char::ToLitString(ch);

	VM_PushString(thread, GC_ConstructString(thread, str.length, str.chars));
}

AVES_API NATIVE_FUNCTION(aves_Char_fromCodepoint)
{
	IntFromValue(thread, args + 0);

	int64_t cp = args[0].integer;
	if (cp < 0 || cp > 0x10FFFF)
	{
		VM_PushString(thread, strings::cp);
		GC_Construct(thread, Types::ArgumentRangeError, 1, nullptr);
		VM_Throw(thread);
	}

	Value character;
	character.type = Types::Char;
	character.integer = cp;
	VM_Push(thread, character);
}

AVES_API NATIVE_FUNCTION(aves_Char_opEquals)
{
	bool eq;
	if (args[1].type == Types::Char)
		eq = args[0].integer == args[1].integer;
	else if (args[1].type == Types::String)
	{
		LitString<2> left = Char::ToLitString((wuchar)args[0].integer);
		eq = String_Equals(_S(left), args[1].common.string);
	}
	else
		eq = false;

	VM_PushBool(thread, eq);
}
AVES_API NATIVE_FUNCTION(aves_Char_opCompare)
{
	int result;

	if (args[1].type == Types::Char)
		result = (int32_t)args[0].integer - (int32_t)args[1].integer;
	else if (args[1].type == Types::String)
	{
		LitString<2> left = Char::ToLitString((wuchar)args[0].integer);
		result = String_Compare(_S(left), args[1].common.string);
	}
	else
		VM_ThrowTypeError(thread);

	VM_PushInt(thread, result);
}
AVES_API NATIVE_FUNCTION(aves_Char_opMultiply)
{
	IntFromValue(thread, args + 1);

	int64_t times = args[1].integer;
	if (times == 0)
	{
		VM_PushString(thread, strings::Empty);
		return;
	}

	LitString<2> str = Char::ToLitString((wuchar)args[0].integer);
	int64_t length = Int_MultiplyChecked(thread, times, str.length);
	if (length > INT32_MAX)
	{
		VM_PushString(thread, strings::times);
		GC_Construct(thread, Types::ArgumentRangeError, 1, nullptr);
		VM_Throw(thread);
	}

	StringBuffer buf(thread, (int32_t)length);

	for (int32_t i = 0; i < (int32_t)length; i++)
		buf.Append(thread, _S(str));

	String *result = buf.ToString(thread);
	VM_PushString(thread, result);
}
AVES_API NATIVE_FUNCTION(aves_Char_opPlus)
{
	VM_PushInt(thread, args[0].integer);
}