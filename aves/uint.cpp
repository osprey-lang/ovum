#include <cmath>
#include <cassert>
#include "aves_uint.h"

#define LEFT  (args[0])
#define RIGHT (args[1])

AVES_API NATIVE_FUNCTION(aves_uint)
{
	Value result = UIntFromValue(thread, args[0]);
	VM_Push(thread, result);
}

AVES_API NATIVE_FUNCTION(aves_UInt_getHashCode)
{
	// The Value.uinteger and Value.integer fields overlap, so
	// instead of casting, we can just use the integer field!
	VM_PushInt(thread, args[0].integer);
}

AVES_API NATIVE_FUNCTION(aves_UInt_toString)
{
	VM_Push(thread, uinteger::ToStringDecimal(thread, THISV.uinteger));
}
AVES_API NATIVE_FUNCTION(aves_UInt_toStringf)
{
	Value format = args[1];

	if (IsInt(format) || IsUInt(format))
	{
		int64_t radix = format.integer;
		if (radix < 2 || radix > 36)
		{
			GC_Construct(thread, Types::ArgumentRangeError, 0, nullptr);
			VM_Throw(thread);
		}

		if (radix == 10)
			VM_Push(thread, uinteger::ToStringDecimal(thread, THISV.uinteger));
		else if (radix == 16)
			VM_Push(thread, uinteger::ToStringHex(thread, THISV.uinteger, false));
		else
			VM_Push(thread, uinteger::ToStringRadix(thread, THISV.uinteger, (unsigned int)radix, false));
	}
	else
		VM_ThrowTypeError(thread);
}

AVES_API NATIVE_FUNCTION(aves_UInt_opEquals)
{
	bool equals = false;
	if (RIGHT.type == Types::UInt)
		equals = LEFT.uinteger == RIGHT.uinteger;
	else if (RIGHT.type == Types::Int)
		equals = LEFT.uinteger == (RIGHT.integer & INT64_MAX);
	else if (RIGHT.type == Types::Real)
		equals = (double)LEFT.uinteger == RIGHT.real;

	VM_PushBool(thread, equals);
}
AVES_API NATIVE_FUNCTION(aves_UInt_opCompare)
{
	if (RIGHT.type != Types::UInt)
		VM_ThrowTypeError(thread);

	uint64_t left  = LEFT.uinteger;
	uint64_t right = RIGHT.uinteger;

	int result = left < right ? -1 :
		left > right ? 1 :
		0;
	VM_PushInt(thread, result);
}
AVES_API NATIVE_FUNCTION(aves_UInt_opShiftLeft)
{
	int64_t amount = IntFromValue(thread, RIGHT).integer;

	if (amount < 0)
	{
		GC_Construct(thread, Types::ArgumentRangeError, 0, nullptr);
		VM_Throw(thread);
	}
	if (amount > 64)
	{
		VM_PushUInt(thread, 0);
		return;
	}

	VM_PushUInt(thread, LEFT.uinteger << (int)amount);
}
AVES_API NATIVE_FUNCTION(aves_UInt_opShiftRight)
{
	int64_t amount = IntFromValue(thread, RIGHT).integer;

	if (amount < 0)
	{
		GC_Construct(thread, Types::ArgumentRangeError, 0, nullptr);
		VM_Throw(thread);
	}
	if (amount > 64)
	{
		VM_PushUInt(thread, 0);
		return;
	}

	VM_PushUInt(thread, LEFT.uinteger >> (int)amount);
}
AVES_API NATIVE_FUNCTION(aves_UInt_opAdd)
{
	if (RIGHT.type == Types::Real)
	{
		VM_PushReal(thread, (double)LEFT.uinteger + RIGHT.real);
		return;
	}
	if (RIGHT.type != Types::UInt)
		RIGHT = UIntFromValue(thread, RIGHT);

	VM_PushUInt(thread, UInt_AddChecked(thread, LEFT.uinteger, RIGHT.uinteger));
}
AVES_API NATIVE_FUNCTION(aves_UInt_opSubtract)
{
	if (RIGHT.type == Types::Real)
	{
		VM_PushReal(thread, (double)LEFT.uinteger - RIGHT.real);
		return;
	}
	if (RIGHT.type != Types::UInt)
		RIGHT = UIntFromValue(thread, RIGHT);

	VM_PushUInt(thread, UInt_SubtractChecked(thread, LEFT.uinteger, RIGHT.uinteger));
}
AVES_API NATIVE_FUNCTION(aves_UInt_opOr)
{
	if (RIGHT.type != Types::Int && RIGHT.type != Types::UInt)
		VM_ThrowTypeError(thread);

	VM_PushUInt(thread, LEFT.uinteger | RIGHT.uinteger);
}
AVES_API NATIVE_FUNCTION(aves_UInt_opXor)
{
	if (RIGHT.type != Types::Int && RIGHT.type != Types::UInt)
		VM_ThrowTypeError(thread);

	VM_PushUInt(thread, LEFT.uinteger ^ RIGHT.uinteger);
}
AVES_API NATIVE_FUNCTION(aves_UInt_opMultiply)
{
	if (RIGHT.type == Types::Real)
	{
		VM_PushReal(thread, (double)LEFT.uinteger * RIGHT.real);
		return;
	}
	if (RIGHT.type != Types::UInt)
		RIGHT = UIntFromValue(thread, RIGHT);

	VM_PushUInt(thread, UInt_MultiplyChecked(thread, LEFT.uinteger, RIGHT.uinteger));
}
AVES_API NATIVE_FUNCTION(aves_UInt_opDivide)
{
	if (RIGHT.type == Types::Real)
	{
		VM_PushReal(thread, (double)LEFT.uinteger / RIGHT.real);
		return;
	}
	if (RIGHT.type != Types::UInt)
		RIGHT = UIntFromValue(thread, RIGHT);

	VM_PushUInt(thread, UInt_DivideChecked(thread, LEFT.uinteger, RIGHT.uinteger));
}
AVES_API NATIVE_FUNCTION(aves_UInt_opModulo)
{
	if (RIGHT.type == Types::Real)
	{
		VM_PushReal(thread, fmod((double)LEFT.uinteger, RIGHT.real));
		return;
	}
	if (RIGHT.type != Types::UInt)
		RIGHT = UIntFromValue(thread, RIGHT);

	VM_PushUInt(thread, UInt_ModuloChecked(thread, LEFT.uinteger, RIGHT.uinteger));
}
AVES_API NATIVE_FUNCTION(aves_UInt_opAnd)
{
	if (RIGHT.type != Types::Int && RIGHT.type != Types::UInt)
		VM_ThrowTypeError(thread);

	VM_PushUInt(thread, LEFT.uinteger & RIGHT.uinteger);
}
AVES_API NATIVE_FUNCTION(aves_UInt_opPower)
{
	if (RIGHT.type == Types::Real)
	{
		VM_PushReal(thread, pow((double)LEFT.uinteger, RIGHT.real));
		return;
	}
	if (RIGHT.type != Types::UInt)
		RIGHT = UIntFromValue(thread, RIGHT);

	uint64_t result = uinteger::Power(thread, LEFT.uinteger, RIGHT.uinteger);
	VM_PushUInt(thread, result);
}
AVES_API NATIVE_FUNCTION(aves_UInt_opPlus)
{
	VM_Push(thread, args[0]);
}
AVES_API NATIVE_FUNCTION(aves_UInt_opNot)
{
	VM_PushUInt(thread, ~args[0].uinteger);
}

// Internal methods

Value uinteger::ToStringDecimal(ThreadHandle thread, const uint64_t value)
{
	// uint64_t's max value is 18446744073709551615, which is 20 characters. 
	const int charCount = 20;
	uchar chars[charCount];

	uint64_t temp = value;
	int length = 0;
	do
	{
		chars[charCount - ++length] = static_cast<uchar>('0' + temp % 10);
	} while (temp /= 10);

	String *outputString = GC_ConstructString(thread, length, chars + charCount - length);

	Value outputValue;
	SetString(outputValue, outputString);
	return outputValue;
}

Value uinteger::ToStringHex(ThreadHandle thread, const uint64_t value, const bool upper)
{
	const uchar letterBase = upper ? 'A' : 'a';

	// uint64_t's max value in hex is ffffffffffffffff, which is 16 characters
	const int charCount = 16;
	uchar chars[charCount];

	uint64_t temp = value;
	int length = 0;
	do
	{
		int64_t rem = temp % 16;
		chars[charCount - ++length] = static_cast<uchar>(rem >= 10 ? letterBase + rem - 10 : '0' + rem);
	} while (temp /= 16);

	String *outputString = GC_ConstructString(thread, length, chars + charCount - length);

	Value outputValue;
	SetString(outputValue, outputString);
	return outputValue;
}

Value uinteger::ToStringRadix(ThreadHandle thread, const uint64_t value, const unsigned int radix, const bool upper)
{
	// The radix is supposed to be range checked outside of this method.
	// Also, use ToStringDecimal and ToStringHex for base 10 and 16, respectively.
	assert(radix >= 2 && radix <= 36 && radix != 10 && radix != 16);

	const uchar letterBase = upper ? 'A' : 'a';

	// The longest possible string that could be produced by ToStringRadix
	// is uint64_t's max value in binary. 64-bit value = 64 binary digits.
	const int charCount = 64;
	uchar chars[charCount];

	uint64_t temp = value;
	int length = 0;
	do {
		int rem = temp % radix; // radix is clamped to [2, 36], so this is fine
		chars[charCount - ++length] = static_cast<uchar>(rem >= 10 ? letterBase + rem - 10 : '0' + rem);
	} while (temp /= radix);

	String *outputString = GC_ConstructString(thread, length, chars + charCount - length);

	Value outputValue;
	SetString(outputValue, outputString);
	return outputValue;
}