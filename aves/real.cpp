﻿#include <cmath>
#include <cstdio>
#include <memory>
#include "aves_real.h"
#include "ov_stringbuffer.h"
#include "dtoa.config.h"

#ifdef _MSC_VER
#define isnan    _isnan
#define isfinite _finite
#define isinf(d) (!_finite(d))
#endif

#define LEFT  (args[0])
#define RIGHT (args[1])

AVES_API NATIVE_FUNCTION(aves_real)
{
	RealFromValue(thread, args);
	VM_Push(thread, args[0]);
}

AVES_API NATIVE_FUNCTION(aves_Real_get_isNaN)
{
	VM_PushBool(thread, isnan(THISV.real));
}
AVES_API NATIVE_FUNCTION(aves_Real_get_isInfinite)
{
	VM_PushBool(thread, isinf(THISV.real));
}

AVES_API NATIVE_FUNCTION(aves_Real_getHashCode)
{
	double realValue = THISV.real;
	if (realValue <= INT64_MAX &&
		realValue >= INT64_MIN &&
		fmod(realValue, 1.0) == 0)
		VM_PushInt(thread, (int64_t)realValue);
	else
		// Value.integer overlaps with Value.real, and they're both 64-bits,
		// so this is perfectly fine!
		VM_PushInt(thread, THISV.integer);
}
AVES_API NATIVE_FUNCTION(aves_Real_toString)
{
	using namespace std;
	static const int precision = 16;

	int decimal = 0, sign = 0;
	unique_ptr<char, dtoa_deleter> resultBuf(_aves_dtoa(THISV.real, FPM_MAX_SIGNIFICANT, precision, &decimal, &sign, nullptr));
	char *result = resultBuf.get(); // for simplicity
	int32_t length = (int32_t)strlen(result);

	uchar buf[32]; // The return value will NEVER be bigger than this
	uchar *bufp = buf;

	if (sign && result[0] != 'N') // NaN may be sign == 1, so we do need the special check
		*bufp++ = '-';

	if (decimal != 9999 && (decimal < 0 ?
		-decimal + length >= precision :
		decimal >= precision))
	{
		// Too many digits! Use scientific notation.
		// Note: decimal is always less than 1000 for
		// non-Infinity, non-NaN values.
		bool negativeDecimal = decimal < 0;
		if (negativeDecimal)
			decimal = -decimal + 1;
		else
			decimal--;
		// Always append the first character
		*bufp++ = result[0];
		if (length > 1)
		{
			// followed by a decimal point and the rest,
			// if there is a rest.
			*bufp++ = '.';
			for (int i = 1; i < length; i++)
				*bufp++ = result[i];
		}
		*bufp++ = 'e';
		*bufp++ = negativeDecimal ? '-' : '+';
		if (decimal >= 100)
			*bufp++ = '0' + decimal / 100;
		if (decimal >= 10)
			*bufp++ = '0' + (decimal / 10) % 10;
		*bufp++ = '0' + decimal % 10;
	}
	else if (decimal <= 0)
	{
		// Append "0." followed by enough zeroes, and then the rest.
		*bufp++ = '0';
		*bufp++ = '.';
		for (int i = 0; i < -decimal; i++)
			*bufp++ = '0';
		for (int i = 0; i < length; i++)
			*bufp++ = result[i];
	}
	else if (decimal >= length)
	{
		// Append the number, followed by enough zeroes
		int i;
		for (i = 0; i < length; i++)
			*bufp++ = result[i];

		if (decimal != 9999)
			while (i++ < decimal)
				*bufp++ = '0';
	}
	else
	{
		// The decimal point is somewhere within the returned number
		int i;
		for (i = 0; i < decimal; i++)
			*bufp++ = result[i];
		*bufp++ = '.';
		while (i < length)
			*bufp++ = result[i++];
	}
	*bufp = 0;

	String *output = GC_ConstructString(thread, bufp - buf, buf);
	VM_PushString(thread, output);
}

AVES_API NATIVE_FUNCTION(aves_Real_parseInternal)
{
	// Arguments: (str is String, start is Int, end is Int)
	// Real.parse ensures that the string only contains whitespace.
	// Also, start and end are guaranteed to be within the range
	// of int32_t. (End is inclusive.)

	String *str = args[0].common.string;
	int32_t start = (int32_t)args[1].integer;
	int32_t end = (int32_t)args[2].integer;

	double result;
	{
		// Create a temporary buffer of ASCII characters to pass into _aves_strtod
		int32_t length = end - start + 1;
		// (One extra char for \0)
		std::unique_ptr<char[]> ascii(new char[length + 1]);
		for (int32_t i = 0; i < length; i++)
			ascii[i] = (char)((&str->firstChar)[start + i]);
		ascii[length] = '\0';

		char *strEnd;
		result = _aves_strtod(ascii.get(), &strEnd);
		if (strEnd != ascii.get() + length)
			goto failure;
	}

	VM_PushReal(thread, result);
	return;

failure:
	VM_PushNull(thread);
	return;
}

AVES_API NATIVE_FUNCTION(aves_Real_opEquals)
{
	bool result = false;
	if (RIGHT.type == Types::Real)
	{
		double left  = LEFT.real;
		double right = RIGHT.real;
		result = isnan(left) && isnan(right) || left == right;
	}
	else if (RIGHT.type == Types::Int)
		result = LEFT.real == (double)RIGHT.integer;
	else if (RIGHT.type == Types::UInt)
		result = LEFT.real == (double)RIGHT.uinteger;

	VM_PushBool(thread, result);
}
AVES_API NATIVE_FUNCTION(aves_Real_opCompare)
{
	double right;
	if (RIGHT.type == Types::Real)
		right = RIGHT.real;
	else if (RIGHT.type == Types::Int)
		right = (double)RIGHT.integer;
	else if (RIGHT.type == Types::UInt)
		right = (double)RIGHT.uinteger;
	else
		VM_ThrowTypeError(thread);

	double left = LEFT.real;

	// Real values are ordered as follows:
	//   NaN < -∞ < ... < -ε < -0.0 = +0.0 < +ε < ... < +∞
	// Notice that NaN is ordered before all other values.

	if (isnan(left))
	{
		VM_PushInt(thread, isnan(right) ? 0 : -1);
		return;
	}

	int result = left < right ? -1 :
		left > right ? 1 :
		0;
	VM_PushInt(thread, result);
}
AVES_API NATIVE_FUNCTION(aves_Real_opAdd)
{
	RealFromValue(thread, &RIGHT);
	VM_PushReal(thread, LEFT.real + RIGHT.real);
}
AVES_API NATIVE_FUNCTION(aves_Real_opSubtract)
{
	RealFromValue(thread, &RIGHT);
	VM_PushReal(thread, LEFT.real - RIGHT.real);
}
AVES_API NATIVE_FUNCTION(aves_Real_opMultiply)
{
	RealFromValue(thread, &RIGHT);
	VM_PushReal(thread, LEFT.real * RIGHT.real);
}
AVES_API NATIVE_FUNCTION(aves_Real_opDivide)
{
	RealFromValue(thread, &RIGHT);
	VM_PushReal(thread, LEFT.real / RIGHT.real);
}
AVES_API NATIVE_FUNCTION(aves_Real_opModulo)
{
	RealFromValue(thread, &RIGHT);
	VM_PushReal(thread, fmod(LEFT.real, RIGHT.real));
}
AVES_API NATIVE_FUNCTION(aves_Real_opPower)
{
	RealFromValue(thread, &RIGHT);
	VM_PushReal(thread, pow(LEFT.real, RIGHT.real));
}
AVES_API NATIVE_FUNCTION(aves_Real_opPlus)
{
	VM_Push(thread, LEFT);
}
AVES_API NATIVE_FUNCTION(aves_Real_opNegate)
{
	VM_PushReal(thread, -args[0].real);
}