#include <cmath>
#include <cassert>
#include <memory>
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
	VM_PushString(thread, uinteger::ToString(thread, THISV.uinteger, 10, 0, false));
}
AVES_API NATIVE_FUNCTION(aves_UInt_toStringf)
{
	Value *format = args + 1;

	if (format->type == Types::Int || format->type == Types::UInt)
	{
		if (format->integer < 2 || format->integer > 36)
		{
			VM_PushString(thread, strings::format);
			VM_PushString(thread, error_strings::RadixOutOfRange);
			GC_Construct(thread, Types::ArgumentRangeError, 2, nullptr);
			VM_Throw(thread);
		}

		VM_PushString(thread, uinteger::ToString(thread, THISV.uinteger, (int)format->integer, 0, false));
	}
	else if (IsString(format))
	{
		int radix, minWidth;
		bool upper;
		integer::ParseFormatString(thread, format->common.string, &radix, &minWidth, &upper);

		VM_PushString(thread, uinteger::ToString(thread, THISV.uinteger, radix, minWidth, upper));
	}
	else
		VM_ThrowTypeError(thread);
}

// Operators

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

String *uinteger::ToString(ThreadHandle thread, const uint64_t value,
	const int radix, const int minWidth,
	const bool upper)
{
	using namespace std;

	static const int smallBufferSize = 128;

	String *str;
	if (minWidth < smallBufferSize)
	{
		uchar buf[smallBufferSize];
		int32_t length;
		if (radix == 10)
			length = ToStringDecimal(thread, value, minWidth, smallBufferSize, buf);
		else if (radix == 16)
			length = ToStringHex(thread, value, upper, minWidth, smallBufferSize, buf);
		else
			length = ToStringRadix(thread, value, radix, upper, minWidth, smallBufferSize, buf);
		str = GC_ConstructString(thread, length, buf + smallBufferSize - length);
	}
	else
	{
		int bufSize = minWidth + 1;
		unique_ptr<uchar[]> buf(new uchar[bufSize]);
		int32_t length;
		if (radix == 10)
			length = ToStringDecimal(thread, value, minWidth, bufSize, buf.get());
		else if (radix == 16)
			length = ToStringHex(thread, value, upper, minWidth, bufSize, buf.get());
		else
			length = ToStringRadix(thread, value, radix, upper, minWidth, bufSize, buf.get());
		str = GC_ConstructString(thread, length, buf.get() + bufSize - length);
	}

	return str;
}

int32_t uinteger::ToStringDecimal(ThreadHandle thread, const uint64_t value,
	const int minWidth, const int bufferSize, uchar *buf)
{
	uchar *chp = buf + bufferSize;

	uint64_t temp = value;
	int length = 0;
	do
	{
		*--chp = (uchar)'0' + (int)(temp % 10);
		length++;
	} while (temp /= 10);

	while (length < minWidth)
	{
		*--chp = (uchar)'0';
		length++;
	}

	return length;
}

int32_t uinteger::ToStringHex(ThreadHandle thread, const uint64_t value,
	const bool upper, const int minWidth,
	const int bufferSize, uchar *buf)
{
	uchar *chp = buf + bufferSize;

	const uchar letterBase = upper ? 'A' : 'a';

	uint64_t temp = value;
	int length = 0;
	do
	{
		int rem = temp % 16;
		*--chp = rem >= 10 ? letterBase + rem - 10 : (uchar)'0' + rem;
		length++;
	} while (temp /= 16);

	while (length < minWidth)
	{
		*--chp = (uchar)'0';
		length++;
	}

	return length;
}

int32_t uinteger::ToStringRadix(ThreadHandle thread, const uint64_t value,
	const int radix, const bool upper, const int minWidth,
	const int bufferSize, uchar *buf)
{
	// The radix is supposed to be range checked outside of this method.
	// Also, use ToStringDecimal and ToStringHex for base 10 and 16, respectively.
	assert(radix >= 2 && radix <= 36 && radix != 10 && radix != 16);

	uchar *chp = buf + bufferSize;

	const uchar letterBase = upper ? 'A' : 'a';
	
	uint64_t temp = value;
	int length = 0;
	do {
		int rem = temp % radix; // radix is clamped to [2, 36], so this is fine
		*--chp = rem >= 10 ? letterBase + rem - 10 : (uchar)'0' + rem;
		length++;
	} while (temp /= radix);

	while (length < minWidth)
	{
		*--chp = (uchar)'0';
		length++;
	}

	return length;
}