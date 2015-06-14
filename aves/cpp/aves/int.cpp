#include "int.h"
#include "real.h"
#include "../aves_state.h"
#include <memory>
#include <cmath>

using namespace aves;

#define LEFT  (args[0])
#define RIGHT (args[1])

AVES_API BEGIN_NATIVE_FUNCTION(aves_int)
{
	CHECKED(IntFromValue(thread, args));
	VM_Push(thread, args + 0);
	RETURN_SUCCESS;
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_Int_getHashCode)
{
	VM_Push(thread, THISP);
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_Int_toString)
{
	String *str;
	CHECKED_MEM(str = integer::ToString(thread, THISV.v.integer, 10, 0, false));
	VM_PushString(thread, str);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_Int_toStringf)
{
	Aves *aves = Aves::Get(thread);

	Value *format = args + 1;

	String *str;
	if (format->type == aves->aves.Int || format->type == aves->aves.UInt)
	{
		if (format->v.integer < 2 || format->v.integer > 36)
		{
			VM_PushString(thread, strings::format);
			VM_PushString(thread, error_strings::RadixOutOfRange);
			return VM_ThrowErrorOfType(thread, aves->aves.ArgumentRangeError, 2);
		}

		str = integer::ToString(thread, THISV.v.integer, (int)format->v.integer, 0, false);
	}
	else if (IsString(thread, format))
	{
		int radix, minWidth;
		bool upper;
		CHECKED(integer::ParseFormatString(thread, format->v.string, &radix, &minWidth, &upper));

		str = integer::ToString(thread, THISV.v.integer, radix, minWidth, upper);
	}
	else
		return VM_ThrowTypeError(thread);
	CHECKED_MEM(str);
	VM_PushString(thread, str);
}
END_NATIVE_FUNCTION

// Operators

AVES_API NATIVE_FUNCTION(aves_Int_opEquals)
{
	Aves *aves = Aves::Get(thread);

	bool equals = false;
	if (RIGHT.type == aves->aves.Int)
		equals = LEFT.v.integer == RIGHT.v.integer;
	else if (RIGHT.type == aves->aves.UInt)
		equals = LEFT.v.integer >= 0 && LEFT.v.uinteger == RIGHT.v.uinteger;
	else if (RIGHT.type == aves->aves.Real)
		equals = (double)LEFT.v.integer == RIGHT.v.real;

	VM_PushBool(thread, equals);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_Int_opCompare)
{
	Aves *aves = Aves::Get(thread);

	int result;
	if (RIGHT.type == aves->aves.Int)
	{
		int64_t left = LEFT.v.integer;
		int64_t right = RIGHT.v.integer;
		result = left < right ? -1 :
			left > right ? 1 :
			0;
	}
	else if (RIGHT.type == aves->aves.UInt)
	{
		int64_t  left = LEFT.v.integer;
		uint64_t right = RIGHT.v.uinteger;
		if ((int32_t)(left >> 32) < 0 ||
			(uint32_t)(right >> 32) > INT32_MAX)
			result = -1;
		else
			result = (uint64_t)left < right ? -1 :
				(uint64_t)left > right ? 1 :
				0;
	}
	else if (RIGHT.type == aves->aves.Real)
	{
		double left = (double)LEFT.v.integer;
		double right = RIGHT.v.real;
		result = real::Compare(left, right);
	}
	else
		return VM_ThrowTypeError(thread);

	VM_PushInt(thread, result);
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_Int_opShiftLeft)
{
	Aves *aves = Aves::Get(thread);

	CHECKED(IntFromValue(thread, &RIGHT));
	int64_t amount = RIGHT.v.integer;

	if (amount < 0)
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentRangeError, 0);
	if (amount > 64)
	{
		VM_PushInt(thread, 0);
		RETURN_SUCCESS;
	}

	VM_PushInt(thread, LEFT.v.integer << (int)amount);
	RETURN_SUCCESS;
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_Int_opShiftRight)
{
	Aves *aves = Aves::Get(thread);

	CHECKED(IntFromValue(thread, &RIGHT));
	int64_t amount = RIGHT.v.integer;

	if (amount < 0)
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentRangeError, 0);
	if (amount > 64)
	{
		VM_PushInt(thread, LEFT.v.integer < 0 ? -1 : 0);
		RETURN_SUCCESS;
	}

	VM_PushInt(thread, LEFT.v.integer >> (int)amount);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_Int_opAdd)
{
	Aves *aves = Aves::Get(thread);

	if (RIGHT.type != aves->aves.Int)
	{
		if (RIGHT.type == aves->aves.Real)
		{
			VM_PushReal(thread, (double)LEFT.v.integer + RIGHT.v.real);
			RETURN_SUCCESS;
		}
		CHECKED(IntFromValue(thread, &RIGHT));
	}

	int64_t result;
	if (Int_AddChecked(LEFT.v.integer, RIGHT.v.integer, result))
		return VM_ThrowOverflowError(thread);
	VM_PushInt(thread, result);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_Int_opSubtract)
{
	Aves *aves = Aves::Get(thread);

	if (RIGHT.type != aves->aves.Int)
	{
		if (RIGHT.type == aves->aves.Real)
		{
			VM_PushReal(thread, (double)LEFT.v.integer - RIGHT.v.real);
			RETURN_SUCCESS;
		}
		CHECKED(IntFromValue(thread, &RIGHT));
	}

	int64_t result;
	if (Int_SubtractChecked(LEFT.v.integer, RIGHT.v.integer, result))
		return VM_ThrowOverflowError(thread);
	VM_PushInt(thread, result);
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_Int_opOr)
{
	Aves *aves = Aves::Get(thread);

	if (RIGHT.type != aves->aves.Int && RIGHT.type != aves->aves.UInt)
		return VM_ThrowTypeError(thread);

	VM_PushInt(thread, LEFT.v.integer | RIGHT.v.integer);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_Int_opXor)
{
	Aves *aves = Aves::Get(thread);

	if (RIGHT.type != aves->aves.Int && RIGHT.type != aves->aves.UInt)
		return VM_ThrowTypeError(thread);

	VM_PushInt(thread, LEFT.v.integer ^ RIGHT.v.integer);
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_Int_opMultiply)
{
	Aves *aves = Aves::Get(thread);

	if (RIGHT.type != aves->aves.Int)
	{
		if (RIGHT.type == aves->aves.Real)
		{
			VM_PushReal(thread, (double)LEFT.v.integer * RIGHT.v.real);
			RETURN_SUCCESS;
		}
		CHECKED(IntFromValue(thread, &RIGHT));
	}

	int64_t result;
	if (Int_MultiplyChecked(LEFT.v.integer, RIGHT.v.integer, result))
		return VM_ThrowOverflowError(thread);
	VM_PushInt(thread, result);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_Int_opDivide)
{
	Aves *aves = Aves::Get(thread);

	if (RIGHT.type != aves->aves.Int)
	{
		if (RIGHT.type == aves->aves.Real)
		{
			VM_PushReal(thread, (double)LEFT.v.integer / RIGHT.v.real);
			RETURN_SUCCESS;
		}
		CHECKED(IntFromValue(thread, &RIGHT));
	}

	int64_t result;
	int r = Int_DivideChecked(LEFT.v.integer, RIGHT.v.integer, result);
	if (r)
	{
		if (r == OVUM_ERROR_DIVIDE_BY_ZERO)
			return VM_ThrowDivideByZeroError(thread);
		return VM_ThrowOverflowError(thread);
	}
	VM_PushInt(thread, result);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_Int_opModulo)
{
	Aves *aves = Aves::Get(thread);

	if (RIGHT.type != aves->aves.Int)
	{
		if (RIGHT.type == aves->aves.Real)
		{
			VM_PushReal(thread, fmod((double)LEFT.v.integer, RIGHT.v.real));
			RETURN_SUCCESS;
		}
		CHECKED(IntFromValue(thread, &RIGHT));
	}

	int64_t result;
	if (Int_ModuloChecked(LEFT.v.integer, RIGHT.v.integer, result))
		return VM_ThrowDivideByZeroError(thread);
	VM_PushInt(thread, result);
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_Int_opAnd)
{
	Aves *aves = Aves::Get(thread);

	if (RIGHT.type != aves->aves.Int && RIGHT.type != aves->aves.UInt)
		return VM_ThrowTypeError(thread);

	VM_PushInt(thread, LEFT.v.integer & RIGHT.v.integer);
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_Int_opPower)
{
	Aves *aves = Aves::Get(thread);

	if (RIGHT.type != aves->aves.Int)
	{
		if (RIGHT.type == aves->aves.Real)
		{
			VM_PushReal(thread, pow((double)LEFT.v.integer, RIGHT.v.real));
			RETURN_SUCCESS;
		}
		CHECKED(IntFromValue(thread, &RIGHT));
	}

	if (RIGHT.v.integer < 0)
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentRangeError, 0);

	int64_t result;
	if (integer::Power(LEFT.v.integer, RIGHT.v.integer, result))
		return VM_ThrowOverflowError(thread);
	VM_PushInt(thread, result);
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_Int_opPlus)
{
	VM_Push(thread, args + 0);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_Int_opNegate)
{
	if (args[0].v.integer == INT64_MIN)
		return VM_ThrowOverflowError(thread);

	VM_PushInt(thread, -args[0].v.integer);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_Int_opNot)
{
	VM_PushInt(thread, ~args[0].v.integer);
	RETURN_SUCCESS;
}

// Internal methods

String *integer::ToString(ThreadHandle thread, const int64_t value,
	const int radix, const int minWidth, const bool upper)
{
	using namespace std;

	static const int smallBufferSize = 128;

	String *str;
	if (minWidth < smallBufferSize)
	{
		ovchar_t buf[smallBufferSize];
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
		unique_ptr<ovchar_t[]> buf(new ovchar_t[bufSize]);
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

int32_t integer::ToStringDecimal(ThreadHandle thread, const int64_t value,
	const int minWidth, const int bufferSize, ovchar_t *buf)
{
	// INT64_MIN is the only weird value: it's the only one that cannot
	// be represented as a positive integer. This value will probably
	// almost never be passed to this method, so let's just pass it on
	// to the slightly less efficient ToStringRadix!
	if (value == INT64_MIN)
		return ToStringRadix(thread, value, 10, false, minWidth, bufferSize, buf);

	ovchar_t *chp = buf + bufferSize;

	int64_t temp = value;
	bool neg = temp < 0;
	if (neg)
		temp = -temp;
	
	int32_t length = 0;
	do
	{
		*--chp = (ovchar_t)'0' + temp % 10;
		length++;
	} while (temp /= 10);

	while (length < minWidth)
	{
		*--chp = (ovchar_t)'0';
		length++;
	}

	if (neg)
	{
		*--chp = (ovchar_t)'-';
		length++;
	}

	return length;
}

int32_t integer::ToStringHex(ThreadHandle thread, const int64_t value,
	const bool upper, const int minWidth,
	const int bufferSize, ovchar_t *buf)
{
	// As with ToStringDecimal, we treat INT64_MIN specially here too.
	if (value == INT64_MIN)
		return ToStringRadix(thread, value, 16, upper, minWidth, bufferSize, buf);

	ovchar_t *chp = buf + bufferSize;

	int64_t temp = value;
	bool neg = temp < 0;
	if (neg)
		temp = -temp;

	const ovchar_t letterBase = upper ? 'A' : 'a';
	
	int32_t length = 0;
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

	if (neg)
	{
		*--chp = (ovchar_t)'-';
		length++;
	}

	return length;
}

int32_t integer::ToStringRadix(ThreadHandle thread, const int64_t value,
	const int radix, const bool upper, const int minWidth,
	const int bufferSize, ovchar_t *buf)
{
	// The radix is supposed to be range checked outside of this method.
	// Also, use ToStringDecimal and ToStringHex for base 10 and 16, respectively.
	OVUM_ASSERT(radix >= 2 && radix <= 36 && (radix != 10 && radix != 16 || value == INT64_MIN));

	ovchar_t *chp = buf + bufferSize;

	int64_t temp = value;
	int sign = temp < 0 ? -1 : 1;

	const ovchar_t letterBase = upper ? 'A' : 'a';

	int32_t length = 0;

	do {
		int rem = sign * (temp % radix);
		*--chp = rem >= 10 ? letterBase + rem - 10 : (ovchar_t)'0' + rem;
		length++;
	} while (temp /= radix);

	while (length < minWidth)
	{
		*--chp = (ovchar_t)'0';
		length++;
	}

	if (sign < 0)
	{
		*--chp = (ovchar_t)'-';
		length++;
	}

	return length;
}

int integer::ParseFormatString(ThreadHandle thread, String *str, int *radix, int *minWidth, bool *upper)
{
	*radix = 10;
	*minWidth = 0;
	*upper = false;

	static const unsigned int MaxWidth = 2048;

	const ovchar_t *ch = &str->firstChar;
	int32_t i = 0;
	switch (*ch)
	{
	case '0': // '0'+ (specifies width of number)
		do
		{
			(*minWidth)++;
		} while (i++ < str->length && *++ch == '0');
		break;

	case 'D': // 'd'[width]
	case 'd': // 'D'[width]
		i++; // take the D
		ch++;
		if (str->length > 1)
			goto parseMinWidth;
		break;

	case 'x': // 'x'[width]
	case 'X': // 'X'[width]
		i++; // skip the X
		*upper = *ch++ == 'X';
		*radix = 16;
		if (str->length > 1)
			goto parseMinWidth;
		break;

	case 'r': // 'r'radix[':'width]
	case 'R': // 'R'radix[':'width] (or with ',' in place of ':')
		i++; // skip the R
		*upper = *ch++ == 'R';
		if (str->length < 2)
			goto throwFormatError;

		if (*ch < '0' || *ch > '9')
			goto throwFormatError;

		*radix = *ch++ - '0';
		i++;
		if (str->length > 2 && *ch >= '0' && *ch <= '9')
		{
			*radix = *radix * 10 + (*ch++ - '0');
			i++;
		}
		if (*radix < 2 || *radix > 36)
			goto throwFormatError;

		if (*ch != ':' && *ch != ',')
			break;
		ch++, i++; // skip ':'/','
		if (i == str->length)
			goto throwFormatError;

parseMinWidth:
		while (i < str->length && *ch >= '0' && *ch <= '9')
		{
			*minWidth *= 10;
			*minWidth += *ch++ - '0';
			i++;
			if (*minWidth > MaxWidth)
				break;
		}
		break;

	default:
		goto throwFormatError;
	}

	if (i != str->length || *minWidth > MaxWidth)
		goto throwFormatError;

	RETURN_SUCCESS;

throwFormatError:
	Aves *aves = Aves::Get(thread);
	VM_PushString(thread, error_strings::InvalidIntegerFormat);
	return VM_ThrowErrorOfType(thread, aves->aves.ArgumentError, 1);
}