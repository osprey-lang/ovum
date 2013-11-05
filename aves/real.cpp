#include <cmath>
#include <cstdio>
#include <memory>
#include "aves_real.h"
#include "ov_stringbuffer.h"
#include "dtoa.config.h"

#define LEFT  (args[0])
#define RIGHT (args[1])

AVES_API NATIVE_FUNCTION(aves_real)
{
	Value result = RealFromValue(thread, args[0]);
	VM_Push(thread, result);
}

AVES_API NATIVE_FUNCTION(aves_Real_get_isInfinite)
{
	VM_PushBool(thread, !IsFinite(THISV.real));
}

AVES_API NATIVE_FUNCTION(aves_Real_getHashCode)
{
	// Value.integer overlaps with Value.real, and they're both 64-bits,
	// so this is perfectly fine!
	VM_PushInt(thread, args[1].integer);
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

	if (decimal != 9999 && decimal > precision || decimal < -precision)
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
		//if (decimal >= 10) - Precision is 20, so this automatically follows
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

AVES_API NATIVE_FUNCTION(aves_Real_opEquals)
{
	bool result = false;
	if (RIGHT.type == Types::Real)
		result = LEFT.real == RIGHT.real;
	else if (RIGHT.type == Types::Int)
		result = LEFT.real == (double)RIGHT.integer;
	else if (RIGHT.type == Types::UInt)
		result = LEFT.real == (double)RIGHT.uinteger;

	VM_PushBool(thread, result);
}
AVES_API NATIVE_FUNCTION(aves_Real_opCompare)
{
	if (RIGHT.type != Types::Real)
		VM_ThrowTypeError(thread);

	double left = LEFT.real;
	double right = RIGHT.real;

	// Real values are ordered as follows:
	//   NaN < -∞ < ... < -ε < -0.0 = +0.0 < +ε < ... < +∞
	// Notice that NaN is ordered before all other values.

	if (IsNaN(left))
	{
		VM_PushInt(thread, IsNaN(right) ? 0 : -1);
		return;
	}

	int result = left < right ? -1 :
		left > right ? 1 :
		0;
	VM_PushInt(thread, result);
}
AVES_API NATIVE_FUNCTION(aves_Real_opAdd)
{
	double right = RealFromValue(thread, RIGHT).real;
	VM_PushReal(thread, LEFT.real + right);
}
AVES_API NATIVE_FUNCTION(aves_Real_opSubtract)
{
	double right = RealFromValue(thread, RIGHT).real;
	VM_PushReal(thread, LEFT.real - right);
}
AVES_API NATIVE_FUNCTION(aves_Real_opMultiply)
{
	double right = RealFromValue(thread, RIGHT).real;
	VM_PushReal(thread, LEFT.real * right);
}
AVES_API NATIVE_FUNCTION(aves_Real_opDivide)
{
	double right = RealFromValue(thread, RIGHT).real;
	VM_PushReal(thread, LEFT.real / right);
}
AVES_API NATIVE_FUNCTION(aves_Real_opModulo)
{
	double right = RealFromValue(thread, RIGHT).real;
	VM_PushReal(thread, fmod(LEFT.real, right));
}
AVES_API NATIVE_FUNCTION(aves_Real_opPower)
{
	double right = RealFromValue(thread, RIGHT).real;
	VM_PushReal(thread, pow(LEFT.real, right));
}
AVES_API NATIVE_FUNCTION(aves_Real_opPlus)
{
	VM_Push(thread, LEFT);
}
AVES_API NATIVE_FUNCTION(aves_Real_opNegate)
{
	VM_PushReal(thread, -args[0].real);
}