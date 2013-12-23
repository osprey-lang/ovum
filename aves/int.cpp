#include <cassert>
#include <cmath>
#include <memory>
#include "aves_int.h"

#define LEFT  (args[0])
#define RIGHT (args[1])

AVES_API NATIVE_FUNCTION(aves_int)
{
	IntFromValue(thread, args);
	VM_Push(thread, args[0]);
}

AVES_API NATIVE_FUNCTION(aves_Int_getHashCode)
{
	VM_Push(thread, THISV);
}

AVES_API NATIVE_FUNCTION(aves_Int_toString)
{
	VM_PushString(thread, integer::ToString(thread, THISV.integer, 10, 0, false));
}
AVES_API NATIVE_FUNCTION(aves_Int_toStringf)
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

		VM_PushString(thread, integer::ToString(thread, THISV.integer, (int)format->integer, 0, false));
	}
	else if (IsString(format))
	{
		int radix, minWidth;
		bool upper;
		integer::ParseFormatString(thread, format->common.string, &radix, &minWidth, &upper);

		VM_PushString(thread, integer::ToString(thread, THISV.integer, radix, minWidth, upper));
	}
	else
		VM_ThrowTypeError(thread);
}

// Operators

AVES_API NATIVE_FUNCTION(aves_Int_opEquals)
{
	bool equals = false;
	if (RIGHT.type == Types::Int)
		equals = LEFT.integer == RIGHT.integer;
	else if (RIGHT.type == Types::UInt)
		equals = (LEFT.integer & INT64_MAX) == RIGHT.uinteger;
	else if (RIGHT.type == Types::Real)
		equals = (double)LEFT.integer == RIGHT.real;

	VM_PushBool(thread, equals);
}
AVES_API NATIVE_FUNCTION(aves_Int_opCompare)
{
	if (RIGHT.type != Types::Int)
		VM_ThrowTypeError(thread);

	int64_t left  = LEFT.integer;
	int64_t right = RIGHT.integer;

	int result = left < right ? -1 :
		left > right ? 1 :
		0;
	VM_PushInt(thread, result);
}
AVES_API NATIVE_FUNCTION(aves_Int_opShiftLeft)
{
	IntFromValue(thread, &RIGHT);
	int64_t amount = RIGHT.integer;

	if (amount < 0)
	{
		GC_Construct(thread, Types::ArgumentRangeError, 0, nullptr);
		VM_Throw(thread);
	}
	if (amount > 64)
	{
		VM_PushInt(thread, 0);
		return;
	}

	VM_PushInt(thread, LEFT.integer << (int)amount);
}
AVES_API NATIVE_FUNCTION(aves_Int_opShiftRight)
{
	IntFromValue(thread, &RIGHT);
	int64_t amount = RIGHT.integer;

	if (amount < 0)
	{
		GC_Construct(thread, Types::ArgumentRangeError, 0, nullptr);
		VM_Throw(thread);
	}
	if (amount > 64)
	{
		VM_PushInt(thread, LEFT.integer < 0 ? -1 : 0);
		return;
	}

	VM_PushInt(thread, LEFT.integer >> (int)amount);
}
AVES_API NATIVE_FUNCTION(aves_Int_opAdd)
{
	if (RIGHT.type != Types::Int)
	{
		if (RIGHT.type == Types::Real)
		{
			VM_PushReal(thread, (double)LEFT.integer + RIGHT.real);
			return;
		}
		IntFromValue(thread, &RIGHT);
	}

	VM_PushInt(thread, Int_AddChecked(thread, LEFT.integer, RIGHT.integer));
}
AVES_API NATIVE_FUNCTION(aves_Int_opSubtract)
{
	if (RIGHT.type != Types::Int)
	{
		if (RIGHT.type == Types::Real)
		{
			VM_PushReal(thread, (double)LEFT.integer - RIGHT.real);
			return;
		}
		IntFromValue(thread, &RIGHT);
	}

	VM_PushInt(thread, Int_SubtractChecked(thread, LEFT.integer, RIGHT.integer));
}
AVES_API NATIVE_FUNCTION(aves_Int_opOr)
{
	if (RIGHT.type != Types::Int && RIGHT.type != Types::UInt)
		VM_ThrowTypeError(thread);

	VM_PushInt(thread, LEFT.integer | RIGHT.integer);
}
AVES_API NATIVE_FUNCTION(aves_Int_opXor)
{
	if (RIGHT.type != Types::Int && RIGHT.type != Types::UInt)
		VM_ThrowTypeError(thread);

	VM_PushInt(thread, LEFT.integer ^ RIGHT.integer);
}
AVES_API NATIVE_FUNCTION(aves_Int_opMultiply)
{
	if (RIGHT.type != Types::Int)
	{
		if (RIGHT.type == Types::Real)
		{
			VM_PushReal(thread, (double)LEFT.integer * RIGHT.real);
			return;
		}
		IntFromValue(thread, &RIGHT);
	}

	VM_PushInt(thread, Int_MultiplyChecked(thread, LEFT.integer, RIGHT.integer));
}
AVES_API NATIVE_FUNCTION(aves_Int_opDivide)
{
	if (RIGHT.type != Types::Int)
	{
		if (RIGHT.type == Types::Real)
		{
			VM_PushReal(thread, (double)LEFT.integer / RIGHT.real);
			return;
		}
		IntFromValue(thread, &RIGHT);
	}

	VM_PushInt(thread, Int_DivideChecked(thread, LEFT.integer, RIGHT.integer));
}
AVES_API NATIVE_FUNCTION(aves_Int_opModulo)
{
	if (RIGHT.type != Types::Int)
	{
		if (RIGHT.type == Types::Real)
		{
			VM_PushReal(thread, fmod((double)LEFT.integer, RIGHT.real));
			return;
		}
		IntFromValue(thread, &RIGHT);
	}

	VM_PushInt(thread, Int_ModuloChecked(thread, LEFT.integer, RIGHT.integer));
}
AVES_API NATIVE_FUNCTION(aves_Int_opAnd)
{
	if (RIGHT.type != Types::Int && RIGHT.type != Types::UInt)
		VM_ThrowTypeError(thread);

	VM_PushInt(thread, LEFT.integer & RIGHT.integer);
}
AVES_API NATIVE_FUNCTION(aves_Int_opPower)
{
	if (RIGHT.type != Types::Int)
	{
		if (RIGHT.type == Types::Real)
		{
			VM_PushReal(thread, pow((double)LEFT.integer, RIGHT.real));
			return;
		}
		IntFromValue(thread, &RIGHT);
	}

	int64_t result = integer::Power(thread, LEFT.integer, RIGHT.integer);
	VM_PushInt(thread, result);
}
AVES_API NATIVE_FUNCTION(aves_Int_opPlus)
{
	VM_Push(thread, args[0]);
}
AVES_API NATIVE_FUNCTION(aves_Int_opNegate)
{
	if (args[0].integer == INT64_MIN)
		VM_ThrowOverflowError(thread);

	VM_PushInt(thread, -args[0].integer);
}
AVES_API NATIVE_FUNCTION(aves_Int_opNot)
{
	VM_PushInt(thread, ~args[0].integer);
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

int32_t integer::ToStringDecimal(ThreadHandle thread, const int64_t value,
	const int minWidth, const int bufferSize, uchar *buf)
{
	// INT64_MIN is the only weird value: it's the only one that cannot
	// be represented as a positive integer. This value will probably
	// almost never be passed to this method, so let's just pass it on
	// to the slightly less efficient ToStringRadix!
	if (value == INT64_MIN)
		return ToStringRadix(thread, value, 10, false, minWidth, bufferSize, buf);

	uchar *chp = buf + bufferSize;

	int64_t temp = value;
	bool neg = temp < 0;
	if (neg)
		temp = -temp;
	
	int32_t length = 0;
	do
	{
		*--chp = (uchar)'0' + temp % 10;
		length++;
	} while (temp /= 10);

	while (length < minWidth)
	{
		*--chp = (uchar)'0';
		length++;
	}

	if (neg)
	{
		*--chp = (uchar)'-';
		length++;
	}

	return length;
}

int32_t integer::ToStringHex(ThreadHandle thread, const int64_t value,
	const bool upper, const int minWidth,
	const int bufferSize, uchar *buf)
{
	// As with ToStringDecimal, we treat INT64_MIN specially here too.
	if (value == INT64_MIN)
		return ToStringRadix(thread, value, 16, upper, minWidth, bufferSize, buf);

	uchar *chp = buf + bufferSize;

	int64_t temp = value;
	bool neg = temp < 0;
	if (neg)
		temp = -temp;

	const uchar letterBase = upper ? 'A' : 'a';
	
	int32_t length = 0;
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

	if (neg)
	{
		*--chp = (uchar)'-';
		length++;
	}

	return length;
}

int32_t integer::ToStringRadix(ThreadHandle thread, const int64_t value,
	const int radix, const bool upper, const int minWidth,
	const int bufferSize, uchar *buf)
{
	// The radix is supposed to be range checked outside of this method.
	// Also, use ToStringDecimal and ToStringHex for base 10 and 16, respectively.
	assert(radix >= 2 && radix <= 36 && (radix != 10 && radix != 16 || value == INT64_MIN));

	uchar *chp = buf + bufferSize;

	int64_t temp = value;
	int sign = temp < 0 ? -1 : 1;

	const uchar letterBase = upper ? 'A' : 'a';

	int32_t length = 0;

	do {
		int rem = sign * (temp % radix);
		*--chp = rem >= 10 ? letterBase + rem - 10 : (uchar)'0' + rem;
		length++;
	} while (temp /= radix);

	while (length < minWidth)
	{
		*--chp = (uchar)'0';
		length++;
	}

	if (sign < 0)
	{
		*--chp = (uchar)'-';
		length++;
	}

	return length;
}

void integer::ParseFormatString(ThreadHandle thread, String *str, int *radix, int *minWidth, bool *upper)
{
	*radix = 10;
	*minWidth = 0;
	*upper = false;

	static const unsigned int MaxWidth = 2048;

	const uchar *ch = &str->firstChar;
	int32_t i = 0;
	switch (*ch)
	{
	case '0': // '0'+ (specifies width of number)
		{
			do
			{
				(*minWidth)++;
			} while (i++ < str->length && *++ch == '0');
		}
		break;

	case 'D': // 'd'[width]
	case 'd': // 'D'[width]
		i++; // eat the D
		ch++;
		if (str->length > 1) goto parseMinWidth;
		break;

	case 'x': // 'x'[width]
	case 'X': // 'X'[width]
		i++; // skip the X
		*upper = *ch++ == 'X';
		*radix = 16;
		if (str->length > 1) goto parseMinWidth;
		break;

	case 'r': // 'r'radix[':'width]
	case 'R': // 'R'radix[':'width]
		i++; // skip the R
		*upper = *ch++ == 'R';
		if (str->length < 2) goto throwFormatError;

		if (*ch < '0' || *ch > '9') goto throwFormatError;

		*radix = *ch++ - '0';
		i++;
		if (str->length > 2 && *ch >= '0' && *ch <= '9')
		{
			*radix = *radix * 10 + (*ch++ - '0');
			i++;
		}
		if (*radix < 2 || *radix > 36)
		{
			VM_PushString(thread, strings::format); // paramName
			VM_PushString(thread, error_strings::RadixOutOfRange); // message
			GC_Construct(thread, Types::ArgumentRangeError, 2, nullptr);
			VM_Throw(thread);
		}

		if (*ch != ':')
			break;
		ch++, i++; // skip ':'
		if (i == str->length) goto throwFormatError;

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

	default: goto throwFormatError;
	}

	if (i != str->length || *minWidth > MaxWidth)
		goto throwFormatError;

	return;

throwFormatError:
	VM_PushString(thread, error_strings::InvalidIntegerFormat);
	GC_Construct(thread, Types::ArgumentError, 1, nullptr);
	VM_Throw(thread);
	return;
}