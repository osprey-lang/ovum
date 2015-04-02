#include <cmath>
#include <cassert>
#include <memory>
#include "aves_uint.h"
#include "aves_real.h"

#define LEFT  (args[0])
#define RIGHT (args[1])

AVES_API BEGIN_NATIVE_FUNCTION(aves_uint)
{
	CHECKED(UIntFromValue(thread, args));
	VM_Push(thread, args + 0);
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_UInt_getHashCode)
{
	// The Value.uinteger and Value.integer fields overlap, so
	// instead of casting, we can just use the integer field!
	VM_PushInt(thread, args[0].integer);
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_UInt_toString)
{
	String *str;
	CHECKED_MEM(str = uinteger::ToString(thread, THISV.uinteger, 10, 0, false));
	VM_PushString(thread, str);
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_UInt_toStringf)
{
	Value *format = args + 1;

	String *str;
	if (format->type == Types::Int || format->type == Types::UInt)
	{
		if (format->integer < 2 || format->integer > 36)
		{
			VM_PushString(thread, strings::format); // paramName
			VM_PushString(thread, error_strings::RadixOutOfRange); // message
			return VM_ThrowErrorOfType(thread, Types::ArgumentRangeError, 2);
		}

		str = uinteger::ToString(thread, THISV.uinteger, (int)format->integer, 0, false);
	}
	else if (IsString(thread, format))
	{
		int radix, minWidth;
		bool upper;
		CHECKED(integer::ParseFormatString(thread, format->common.string, &radix, &minWidth, &upper));

		str = uinteger::ToString(thread, THISV.uinteger, radix, minWidth, upper);
	}
	else
		return VM_ThrowTypeError(thread);
	CHECKED_MEM(str);
	VM_PushString(thread, str);
}
END_NATIVE_FUNCTION

// Operators

AVES_API NATIVE_FUNCTION(aves_UInt_opEquals)
{
	bool equals = false;
	if (RIGHT.type == Types::UInt)
		equals = LEFT.uinteger == RIGHT.uinteger;
	else if (RIGHT.type == Types::Int)
		equals = RIGHT.integer >= 0 && LEFT.uinteger == RIGHT.uinteger;
	else if (RIGHT.type == Types::Real)
		equals = (double)LEFT.uinteger == RIGHT.real;

	VM_PushBool(thread, equals);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_UInt_opCompare)
{
	int result;
	if (RIGHT.type == Types::UInt)
	{
		uint64_t left  = LEFT.uinteger;
		uint64_t right = RIGHT.uinteger;
		result = left < right ? -1 :
			left > right ? 1 :
			0;
	}
	else if (RIGHT.type == Types::Int)
	{
		uint64_t left = LEFT.uinteger;
		int64_t right = RIGHT.integer;
		if ((int32_t)(right >> 32) < 0 ||
			(uint32_t)(left >> 32) > INT32_MAX)
			result = 1;
		else
			result = left < (uint64_t)right ? -1 :
				left > (uint64_t)right ? 1 :
				0;
	}
	else if (RIGHT.type == Types::Real)
	{
		double left  = (double)LEFT.uinteger;
		double right = RIGHT.real;
		result = real::Compare(left, right);
	}
	else
		return VM_ThrowTypeError(thread);
	VM_PushInt(thread, result);
	RETURN_SUCCESS;
}
AVES_API BEGIN_NATIVE_FUNCTION(aves_UInt_opShiftLeft)
{
	CHECKED(IntFromValue(thread, &RIGHT));
	int64_t amount = RIGHT.integer;

	if (amount < 0)
		return VM_ThrowErrorOfType(thread, Types::ArgumentRangeError, 0);
	if (amount > 64)
	{
		VM_PushUInt(thread, 0);
		RETURN_SUCCESS;
	}

	VM_PushUInt(thread, LEFT.uinteger << (int)amount);
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_UInt_opShiftRight)
{
	CHECKED(IntFromValue(thread, &RIGHT));
	int64_t amount = RIGHT.integer;

	if (amount < 0)
		return VM_ThrowErrorOfType(thread, Types::ArgumentRangeError, 0);
	if (amount > 64)
	{
		VM_PushUInt(thread, 0);
		RETURN_SUCCESS;
	}

	VM_PushUInt(thread, LEFT.uinteger >> (int)amount);
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_UInt_opAdd)
{
	if (RIGHT.type != Types::UInt)
	{
		if (RIGHT.type == Types::Real)
		{
			VM_PushReal(thread, (double)LEFT.uinteger + RIGHT.real);
			RETURN_SUCCESS;
		}
		CHECKED(UIntFromValue(thread, &RIGHT));
	}

	uint64_t result;
	if (UInt_AddChecked(LEFT.uinteger, RIGHT.uinteger, result))
		return VM_ThrowOverflowError(thread);
	VM_PushUInt(thread, result);
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_UInt_opSubtract)
{
	if (RIGHT.type != Types::UInt)
	{
		if (RIGHT.type == Types::Real)
		{
			VM_PushReal(thread, (double)LEFT.uinteger - RIGHT.real);
			RETURN_SUCCESS;
		}
		CHECKED(UIntFromValue(thread, &RIGHT));
	}

	uint64_t result;
	if (UInt_SubtractChecked(LEFT.uinteger, RIGHT.uinteger, result))
		return VM_ThrowOverflowError(thread);
	VM_PushUInt(thread, result);
}
END_NATIVE_FUNCTION
AVES_API NATIVE_FUNCTION(aves_UInt_opOr)
{
	if (RIGHT.type != Types::Int && RIGHT.type != Types::UInt)
		return VM_ThrowTypeError(thread);

	VM_PushUInt(thread, LEFT.uinteger | RIGHT.uinteger);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_UInt_opXor)
{
	if (RIGHT.type != Types::Int && RIGHT.type != Types::UInt)
		return VM_ThrowTypeError(thread);

	VM_PushUInt(thread, LEFT.uinteger ^ RIGHT.uinteger);
	RETURN_SUCCESS;
}
AVES_API BEGIN_NATIVE_FUNCTION(aves_UInt_opMultiply)
{
	if (RIGHT.type != Types::UInt)
	{
		if (RIGHT.type == Types::Real)
		{
			VM_PushReal(thread, (double)LEFT.uinteger * RIGHT.real);
			RETURN_SUCCESS;
		}
		CHECKED(UIntFromValue(thread, &RIGHT));
	}

	uint64_t result;
	if (UInt_MultiplyChecked(LEFT.uinteger, RIGHT.uinteger, result))
		return VM_ThrowOverflowError(thread);
	VM_PushUInt(thread, result);
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_UInt_opDivide)
{
	if (RIGHT.type != Types::UInt)
	{
		if (RIGHT.type == Types::Real)
		{
			VM_PushReal(thread, (double)LEFT.uinteger / RIGHT.real);
			RETURN_SUCCESS;
		}
		CHECKED(UIntFromValue(thread, &RIGHT));
	}

	uint64_t result;
	int r = UInt_DivideChecked(LEFT.uinteger, RIGHT.uinteger, result);
	if (r)
	{
		if (r == OVUM_ERROR_DIVIDE_BY_ZERO)
			return VM_ThrowDivideByZeroError(thread);
		return VM_ThrowOverflowError(thread);
	}
	VM_PushUInt(thread, result);
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_UInt_opModulo)
{
	if (RIGHT.type != Types::UInt)
	{
		if (RIGHT.type == Types::Real)
		{
			VM_PushReal(thread, fmod((double)LEFT.uinteger, RIGHT.real));
			RETURN_SUCCESS;
		}
		CHECKED(UIntFromValue(thread, &RIGHT));
	}

	uint64_t result;
	if (UInt_ModuloChecked(LEFT.uinteger, RIGHT.uinteger, result))
		return VM_ThrowDivideByZeroError(thread);
	VM_PushUInt(thread, result);
}
END_NATIVE_FUNCTION
AVES_API NATIVE_FUNCTION(aves_UInt_opAnd)
{
	if (RIGHT.type != Types::Int && RIGHT.type != Types::UInt)
		return VM_ThrowTypeError(thread);

	VM_PushUInt(thread, LEFT.uinteger & RIGHT.uinteger);
	RETURN_SUCCESS;
}
AVES_API BEGIN_NATIVE_FUNCTION(aves_UInt_opPower)
{
	if (RIGHT.type != Types::UInt)
	{
		if (RIGHT.type == Types::Real)
		{
			VM_PushReal(thread, pow((double)LEFT.uinteger, RIGHT.real));
			RETURN_SUCCESS;
		}
		CHECKED(UIntFromValue(thread, &RIGHT));
	}

	uint64_t result;
	if (uinteger::Power(LEFT.uinteger, RIGHT.uinteger, result))
		return VM_ThrowOverflowError(thread);
	VM_PushUInt(thread, result);
}
END_NATIVE_FUNCTION
AVES_API NATIVE_FUNCTION(aves_UInt_opPlus)
{
	VM_Push(thread, args + 0);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_UInt_opNot)
{
	VM_PushUInt(thread, ~args[0].uinteger);
	RETURN_SUCCESS;
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