#include <cassert>
#include <cmath>
#include "aves_int.h"

AVES_API NATIVE_FUNCTION(aves_int)
{
	Value result = IntFromValue(thread, args[0]);
	VM_Push(thread, result);
}

AVES_API NATIVE_FUNCTION(aves_Int_getHashCode)
{
	VM_Push(thread, THISV);
}

AVES_API NATIVE_FUNCTION(aves_Int_toString)
{
	VM_Push(thread, integer::ToStringDecimal(thread, THISV.integer));
}
AVES_API NATIVE_FUNCTION(aves_Int_toStringf)
{
	Value format = args[1];

	if (IsInt(format) || IsUInt(format))
	{
		int64_t radix = format.integer;
		if (radix < 2 || radix > 36)
		{
			GC_Construct(thread, ArgumentRangeError, 0, nullptr);
			VM_Throw(thread);
		}

		if (radix == 10)
			VM_Push(thread, integer::ToStringDecimal(thread, THISV.integer));
		else if (radix == 16)
			VM_Push(thread, integer::ToStringHex(thread, THISV.integer, false));
		else
			VM_Push(thread, integer::ToStringRadix(thread, THISV.integer, (unsigned int)radix, false));
	}
	else
		VM_ThrowTypeError(thread);
}

// Operators

AVES_API NATIVE_FUNCTION(aves_Int_opEquals)
{
	bool equals = false;
	if (IsInt(args[1]))
		equals = args[0].integer == args[1].integer;
	else if (IsUInt(args[1]))
		equals = (args[0].integer & INT64_MAX) == args[1].uinteger;
	else if (IsReal(args[1]))
		equals = (double)args[0].integer == args[1].real;

	VM_PushBool(thread, equals);
}
AVES_API NATIVE_FUNCTION(aves_Int_opCompare)
{
	if (!IsInt(args[1]))
		VM_ThrowTypeError(thread);

	int64_t left  = args[0].integer;
	int64_t right = args[1].integer;

	int result = left < right ? -1 :
		left > right ? 1 :
		0;
	VM_PushInt(thread, result);
}
AVES_API NATIVE_FUNCTION(aves_Int_opShiftLeft)
{
	int64_t amount = IntFromValue(thread, args[1]).integer;

	if (amount < 0)
	{
		GC_Construct(thread, ArgumentRangeError, 0, nullptr);
		VM_Throw(thread);
	}
	if (amount > 64)
	{
		VM_PushInt(thread, 0);
		return;
	}

	VM_PushInt(thread, args[0].integer << (int)amount);
}
AVES_API NATIVE_FUNCTION(aves_Int_opShiftRight)
{
	int64_t amount = IntFromValue(thread, args[1]).integer;

	if (amount < 0)
	{
		GC_Construct(thread, ArgumentRangeError, 0, nullptr);
		VM_Throw(thread);
	}
	if (amount > 64)
	{
		VM_PushInt(thread, args[0].integer < 0 ? -1 : 0);
		return;
	}

	VM_PushInt(thread, args[0].integer >> (int)amount);
}
AVES_API NATIVE_FUNCTION(aves_Int_opAdd)
{
	Value right = args[1];
	if (IsReal(right))
	{
		VM_PushReal(thread, (double)args[0].integer + right.real);
		return;
	}
	if (!IsInt(right))
		right = IntFromValue(thread, right);

	VM_PushInt(thread, Int_AddChecked(thread, args[0].integer, right.integer));
}
AVES_API NATIVE_FUNCTION(aves_Int_opSubtract)
{
	Value right = args[1];
	if (IsReal(right))
	{
		VM_PushReal(thread, (double)args[0].integer - right.real);
		return;
	}
	if (!IsInt(right))
		right = IntFromValue(thread, right);

	VM_PushInt(thread, Int_SubtractChecked(thread, args[0].integer, right.integer));
}
AVES_API NATIVE_FUNCTION(aves_Int_opOr)
{
	Value right = args[1];
	if (!IsInt(right) && !IsUInt(right))
		VM_ThrowTypeError(thread);

	VM_PushInt(thread, args[0].integer | right.integer);
}
AVES_API NATIVE_FUNCTION(aves_Int_opXor)
{
	Value right = args[1];
	if (!IsInt(right) && !IsUInt(right))
		VM_ThrowTypeError(thread);

	VM_PushInt(thread, args[0].integer ^ right.integer);
}
AVES_API NATIVE_FUNCTION(aves_Int_opMultiply)
{
	Value right = args[1];
	if (IsReal(right))
	{
		VM_PushReal(thread, (double)args[0].integer * right.real);
		return;
	}
	if (!IsInt(right))
		right = IntFromValue(thread, right);

	VM_PushInt(thread, Int_MultiplyChecked(thread, args[0].integer, right.integer));
}
AVES_API NATIVE_FUNCTION(aves_Int_opDivide)
{
	Value right = args[1];
	if (IsReal(right))
	{
		VM_PushReal(thread, (double)args[0].integer / right.real);
		return;
	}
	if (!IsInt(right))
		right = IntFromValue(thread, right);

	VM_PushInt(thread, Int_DivideChecked(thread, args[0].integer, right.integer));
}
AVES_API NATIVE_FUNCTION(aves_Int_opModulo)
{
	Value right = args[1];
	if (IsReal(right))
	{
		VM_PushReal(thread, fmod((double)args[0].integer, right.real));
		return;
	}
	if (!IsInt(right))
		right = IntFromValue(thread, right);

	VM_PushInt(thread, Int_ModuloChecked(thread, args[0].integer, right.integer));
}
AVES_API NATIVE_FUNCTION(aves_Int_opAnd)
{
	Value right = args[1];
	if (!IsInt(right) && !IsUInt(right))
		VM_ThrowTypeError(thread);

	VM_PushInt(thread, args[0].integer & right.integer);
}
AVES_API NATIVE_FUNCTION(aves_Int_opPower)
{
	Value right = args[1];
	if (IsReal(right))
	{
		VM_PushReal(thread, pow((double)args[0].integer, right.real));
		return;
	}
	if (!IsInt(right))
		right = IntFromValue(thread, right);

	double result = pow((double)args[0].integer, (double)right.integer);

	// TODO: verify that this is safe
	if (result > INT64_MAX || result < INT64_MIN ||
		IsNaN(result) || !IsFinite(result))
		VM_ThrowOverflowError(thread);

	VM_PushInt(thread, (int64_t)result);
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

namespace integer
{
	LitString<20> MinDec = { 20, 0, STR_STATIC, '-','9','2','2','3','3','7','2','0','3','6','8','5','4','7','7','5','8','0','8',0 };
	LitString<17> MinHex = { 17, 0, STR_STATIC, '-','8','0','0','0','0','0','0','0','0','0','0','0','0','0','0','0',0 };
}

Value integer::ToStringDecimal(ThreadHandle thread, const int64_t value)
{
	// INT64_MIN is the only weird value: it's the only one that cannot
	// be represented as a positive integer. This value will probably
	// almost never be passed to this method, so let's hardcode it in!
	if (value == INT64_MIN)
	{
		Value output;
		SetString(output, _S(integer::MinDec));
		return output;
	}

	int64_t temp = value;
	bool neg = temp < 0;
	if (neg)
		temp = -temp; // Note: overflows on INT64_MIN

	// int64_t's min value is -9223372036854775808, which is 20 characters. 
	const int charCount = 20;
	uchar chars[charCount];

	int32_t length = 0;
	do
	{
		chars[charCount - ++length] = static_cast<uchar>('0' + temp % 10);
	} while (temp /= 10);

	if (neg)
		chars[charCount - ++length] = '-';

	String *outputString;
	GC_ConstructString(thread, length, chars + charCount - length, &outputString);

	Value outputValue;
	SetString(outputValue, outputString);
	return outputValue;
}

Value integer::ToStringHex(ThreadHandle thread, const int64_t value, const bool upper)
{
	// As with ToStringDecimal, we hardcode INT64_MIN's value here too.
	if (value == INT64_MIN)
	{
		Value output;
		SetString(output, _S(integer::MinHex));
		return output;
	}

	int64_t temp = value;
	bool neg = temp < 0;
	if (neg)
		temp = -temp; // Note: overflows on INT64_MIN

	const uchar letterBase = upper ? 'A' : 'a';

	// int64_t's min value in hex is -8000000000000000, which is 17 characters
	const int charCount = 20;
	uchar chars[charCount];

	int32_t length = 0;
	do
	{
		int64_t rem = temp % 16;
		chars[charCount - ++length] = static_cast<uchar>(rem >= 10 ? letterBase + rem - 10 : '0' + rem);
	} while (temp /= 16);

	if (neg)
		chars[charCount - ++length] = '-';

	String *outputString;
	GC_ConstructString(thread, length, chars + charCount - length, &outputString);

	Value outputValue;
	SetString(outputValue, outputString);
	return outputValue;
}

Value integer::ToStringRadix(ThreadHandle thread, const int64_t value, const unsigned int radix, const bool upper)
{
	// The radix is supposed to be range checked outside of this method.
	// Also, use ToStringDecimal and ToStringHex for base 10 and 16, respectively.
	assert(radix >= 2 && radix <= 36 && radix != 10 && radix != 16);

	int64_t temp = value;
	int sign = temp < 0 ? -1 : 1;
	//if (sign < 0)
	//	temp = -temp;

	const uchar letterBase = upper ? 'A' : 'a';

	// The longest possible string that could be produced by this method is a negative
	// binary string. 64-bit value = 64 binary characters + 1 for sign.
	const int charCount = 65;
	uchar chars[charCount];

	int32_t length = 0;

	do {
		int64_t rem = sign * (temp % radix);
		chars[charCount - ++length] = static_cast<uchar>(rem >= 10 ? letterBase + rem - 10 : '0' + rem);
	} while (temp /= radix);

	if (sign < 0)
		chars[charCount - ++length] = '-';

	String *outputString;
	GC_ConstructString(thread, length, chars + charCount - length, &outputString);

	Value outputValue;
	SetString(outputValue, outputString);
	return outputValue;
}