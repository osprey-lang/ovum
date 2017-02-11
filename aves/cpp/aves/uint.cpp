#include "uint.h"
#include "real.h"
#include "../aves_state.h"
#include <cmath>
#include <memory>

using namespace aves;

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
	VM_PushInt(thread, args[0].v.integer);
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_UInt_toString)
{
	String *str;
	CHECKED_MEM(str = uinteger::ToString(thread, THISV.v.uinteger, 10, 0, false));
	VM_PushString(thread, str);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_UInt_toStringf)
{
	Aves *aves = Aves::Get(thread);

	Value *format = args + 1;

	String *str;
	if (format->type == aves->aves.Int || format->type == aves->aves.UInt)
	{
		if (format->v.integer < 2 || format->v.integer > 36)
		{
			VM_PushString(thread, strings::format); // paramName
			VM_PushString(thread, error_strings::RadixOutOfRange); // message
			return VM_ThrowErrorOfType(thread, aves->aves.ArgumentRangeError, 2);
		}

		str = uinteger::ToString(thread, THISV.v.uinteger, (int)format->v.integer, 0, false);
	}
	else if (IsString(thread, format))
	{
		int radix;
		size_t minWidth;
		bool upper;
		CHECKED(integer::ParseFormatString(thread, format->v.string, &radix, &minWidth, &upper));

		str = uinteger::ToString(thread, THISV.v.uinteger, radix, minWidth, upper);
	}
	else
	{
		VM_PushString(thread, strings::format); // paramName
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentTypeError, 1);
	}
	CHECKED_MEM(str);
	VM_PushString(thread, str);
}
END_NATIVE_FUNCTION

// Operators

AVES_API NATIVE_FUNCTION(aves_UInt_opEquals)
{
	Aves *aves = Aves::Get(thread);

	bool equals = false;
	if (RIGHT.type == aves->aves.UInt)
		equals = LEFT.v.uinteger == RIGHT.v.uinteger;
	else if (RIGHT.type == aves->aves.Int)
		equals = RIGHT.v.integer >= 0 && LEFT.v.uinteger == RIGHT.v.uinteger;
	else if (RIGHT.type == aves->aves.Real)
		equals = (double)LEFT.v.uinteger == RIGHT.v.real;

	VM_PushBool(thread, equals);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_UInt_opCompare)
{
	Aves *aves = Aves::Get(thread);

	int result;
	if (RIGHT.type == aves->aves.UInt)
	{
		uint64_t left = LEFT.v.uinteger;
		uint64_t right = RIGHT.v.uinteger;
		result = left < right ? -1 :
			left > right ? 1 :
			0;
	}
	else if (RIGHT.type == aves->aves.Int)
	{
		uint64_t left = LEFT.v.uinteger;
		int64_t right = RIGHT.v.integer;
		if ((int32_t)(right >> 32) < 0 ||
			(uint32_t)(left >> 32) > INT32_MAX)
			result = 1;
		else
			result = left < (uint64_t)right ? -1 :
				left > (uint64_t)right ? 1 :
				0;
	}
	else if (RIGHT.type == aves->aves.Real)
	{
		double left = (double)LEFT.v.uinteger;
		double right = RIGHT.v.real;
		result = real::Compare(left, right);
	}
	else
	{
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentTypeError, 0);
	}
	VM_PushInt(thread, result);
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_UInt_opShiftLeft)
{
	Aves *aves = Aves::Get(thread);

	CHECKED(IntFromValue(thread, &RIGHT));
	int64_t amount = RIGHT.v.integer;

	if (amount < 0)
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentRangeError, 0);
	if (amount > 64)
	{
		VM_PushUInt(thread, 0);
		RETURN_SUCCESS;
	}

	VM_PushUInt(thread, LEFT.v.uinteger << (int)amount);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_UInt_opShiftRight)
{
	Aves *aves = Aves::Get(thread);

	CHECKED(IntFromValue(thread, &RIGHT));
	int64_t amount = RIGHT.v.integer;

	if (amount < 0)
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentRangeError, 0);
	if (amount > 64)
	{
		VM_PushUInt(thread, 0);
		RETURN_SUCCESS;
	}

	VM_PushUInt(thread, LEFT.v.uinteger >> (int)amount);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_UInt_opAdd)
{
	Aves *aves = Aves::Get(thread);

	if (RIGHT.type != aves->aves.UInt)
	{
		if (RIGHT.type == aves->aves.Real)
		{
			VM_PushReal(thread, (double)LEFT.v.uinteger + RIGHT.v.real);
			RETURN_SUCCESS;
		}
		CHECKED(UIntFromValue(thread, &RIGHT));
	}

	uint64_t result;
	if (UInt_AddChecked(LEFT.v.uinteger, RIGHT.v.uinteger, result))
		return VM_ThrowOverflowError(thread);
	VM_PushUInt(thread, result);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_UInt_opSubtract)
{
	Aves *aves = Aves::Get(thread);

	if (RIGHT.type != aves->aves.UInt)
	{
		if (RIGHT.type == aves->aves.Real)
		{
			VM_PushReal(thread, (double)LEFT.v.uinteger - RIGHT.v.real);
			RETURN_SUCCESS;
		}
		CHECKED(UIntFromValue(thread, &RIGHT));
	}

	uint64_t result;
	if (UInt_SubtractChecked(LEFT.v.uinteger, RIGHT.v.uinteger, result))
		return VM_ThrowOverflowError(thread);
	VM_PushUInt(thread, result);
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_UInt_opOr)
{
	Aves *aves = Aves::Get(thread);

	if (RIGHT.type != aves->aves.Int && RIGHT.type != aves->aves.UInt)
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentTypeError, 0);

	VM_PushUInt(thread, LEFT.v.uinteger | RIGHT.v.uinteger);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_UInt_opXor)
{
	Aves *aves = Aves::Get(thread);

	if (RIGHT.type != aves->aves.Int && RIGHT.type != aves->aves.UInt)
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentTypeError, 0);

	VM_PushUInt(thread, LEFT.v.uinteger ^ RIGHT.v.uinteger);
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_UInt_opMultiply)
{
	Aves *aves = Aves::Get(thread);

	if (RIGHT.type != aves->aves.UInt)
	{
		if (RIGHT.type == aves->aves.Real)
		{
			VM_PushReal(thread, (double)LEFT.v.uinteger * RIGHT.v.real);
			RETURN_SUCCESS;
		}
		CHECKED(UIntFromValue(thread, &RIGHT));
	}

	uint64_t result;
	if (UInt_MultiplyChecked(LEFT.v.uinteger, RIGHT.v.uinteger, result))
		return VM_ThrowOverflowError(thread);
	VM_PushUInt(thread, result);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_UInt_opDivide)
{
	Aves *aves = Aves::Get(thread);

	if (RIGHT.type != aves->aves.UInt)
	{
		if (RIGHT.type == aves->aves.Real)
		{
			VM_PushReal(thread, (double)LEFT.v.uinteger / RIGHT.v.real);
			RETURN_SUCCESS;
		}
		CHECKED(UIntFromValue(thread, &RIGHT));
	}

	uint64_t result;
	int r = UInt_DivideChecked(LEFT.v.uinteger, RIGHT.v.uinteger, result);
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
	Aves *aves = Aves::Get(thread);

	if (RIGHT.type != aves->aves.UInt)
	{
		if (RIGHT.type == aves->aves.Real)
		{
			VM_PushReal(thread, fmod((double)LEFT.v.uinteger, RIGHT.v.real));
			RETURN_SUCCESS;
		}
		CHECKED(UIntFromValue(thread, &RIGHT));
	}

	uint64_t result;
	if (UInt_ModuloChecked(LEFT.v.uinteger, RIGHT.v.uinteger, result))
		return VM_ThrowDivideByZeroError(thread);
	VM_PushUInt(thread, result);
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_UInt_opAnd)
{
	Aves *aves = Aves::Get(thread);

	if (RIGHT.type != aves->aves.Int && RIGHT.type != aves->aves.UInt)
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentTypeError, 0);

	VM_PushUInt(thread, LEFT.v.uinteger & RIGHT.v.uinteger);
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_UInt_opPower)
{
	Aves *aves = Aves::Get(thread);

	if (RIGHT.type != aves->aves.UInt)
	{
		if (RIGHT.type == aves->aves.Real)
		{
			VM_PushReal(thread, pow((double)LEFT.v.uinteger, RIGHT.v.real));
			RETURN_SUCCESS;
		}
		CHECKED(UIntFromValue(thread, &RIGHT));
	}

	uint64_t result;
	if (uinteger::Power(LEFT.v.uinteger, RIGHT.v.uinteger, result))
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
	VM_PushUInt(thread, ~args[0].v.uinteger);
	RETURN_SUCCESS;
}

// Internal methods

String *uinteger::ToString(
	ThreadHandle thread,
	uint64_t value,
	int radix,
	size_t minWidth,
	bool upper
)
{
	static const size_t smallBufferSize = 128;

	String *str;
	if (minWidth < smallBufferSize)
	{
		ovchar_t buf[smallBufferSize];
		size_t length;
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
		size_t bufSize = minWidth + 1;
		std::unique_ptr<ovchar_t[]> buf(new ovchar_t[bufSize]);
		size_t length;
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

size_t uinteger::ToStringDecimal(
	ThreadHandle thread,
	uint64_t value,
	size_t minWidth,
	size_t bufferSize,
	ovchar_t *buf
)
{
	ovchar_t *chp = buf + bufferSize;

	uint64_t temp = value;
	size_t length = 0;
	do
	{
		*--chp = (ovchar_t)'0' + (int)(temp % 10);
		length++;
	} while (temp /= 10);

	while (length < minWidth)
	{
		*--chp = (ovchar_t)'0';
		length++;
	}

	return length;
}

size_t uinteger::ToStringHex(
	ThreadHandle thread,
	uint64_t value,
	bool upper,
	size_t minWidth,
	size_t bufferSize,
	ovchar_t *buf
)
{
	ovchar_t *chp = buf + bufferSize;

	const ovchar_t letterBase = upper ? 'A' : 'a';

	uint64_t temp = value;
	size_t length = 0;
	do
	{
		int rem = temp % 16;
		*--chp = rem >= 10 ? letterBase + rem - 10 : (ovchar_t)'0' + rem;
		length++;
	} while (temp /= 16);

	while (length < minWidth)
	{
		*--chp = (ovchar_t)'0';
		length++;
	}

	return length;
}

size_t uinteger::ToStringRadix(
	ThreadHandle thread,
	uint64_t value,
	int radix,
	bool upper,
	size_t minWidth,
	size_t bufferSize,
	ovchar_t *buf
)
{
	// The radix is supposed to be range checked outside of this method.
	// Also, use ToStringDecimal and ToStringHex for base 10 and 16, respectively.
	OVUM_ASSERT(radix >= 2 && radix <= 36 && radix != 10 && radix != 16);

	ovchar_t *chp = buf + bufferSize;

	const ovchar_t letterBase = upper ? 'A' : 'a';
	
	uint64_t temp = value;
	size_t length = 0;
	do {
		int rem = temp % radix; // radix is clamped to [2, 36], so this is fine
		*--chp = rem >= 10 ? letterBase + rem - 10 : (ovchar_t)'0' + rem;
		length++;
	} while (temp /= radix);

	while (length < minWidth)
	{
		*--chp = (ovchar_t)'0';
		length++;
	}

	return length;
}
