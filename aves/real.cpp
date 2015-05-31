#include <cmath>
#include <cstdio>
#include <memory>
#include "aves_real.h"
#include "aves_state.h"
#include "ov_stringbuffer.h"
#include "dtoa.config.h"

using namespace aves;

#ifdef _MSC_VER
#define isnan    _isnan
#define isfinite _finite
#define isinf(d) (!_finite(d))
#endif

#define LEFT  (args[0])
#define RIGHT (args[1])

AVES_API BEGIN_NATIVE_FUNCTION(aves_real)
{
	CHECKED(RealFromValue(thread, args));
	VM_Push(thread, args + 0);
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_Real_get_isNaN)
{
	VM_PushBool(thread, isnan(THISV.v.real) ? true : false);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_Real_get_isInfinite)
{
	VM_PushBool(thread, isinf(THISV.v.real));
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_Real_getHashCode)
{
	double realValue = THISV.v.real;
	if (realValue <= INT64_MAX &&
		realValue >= INT64_MIN &&
		fmod(realValue, 1.0) == 0)
		VM_PushInt(thread, (int64_t)realValue);
	else
		// Value.integer overlaps with Value.real, and they're both 64-bits,
		// so this is perfectly fine!
		VM_PushInt(thread, THISV.v.integer);
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_Real_toString)
{
	using namespace std;
	static const int precision = 16;

	int decimal = 0, sign = 0;
	unique_ptr<char, dtoa_deleter> resultBuf(_aves_dtoa(THISV.v.real, FPM_MAX_SIGNIFICANT, precision, &decimal, &sign, nullptr));
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

	String *output;
	CHECKED_MEM(output = GC_ConstructString(thread, bufp - buf, buf));
	VM_PushString(thread, output);
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_Real_parseInternal)
{
	// Arguments: (str is String, start is Int, end is Int)
	// Real.parse ensures that the string only contains whitespace.
	// Also, start and end are guaranteed to be within the range
	// of int32_t. (End is inclusive.)

	String *str = args[0].v.string;
	int32_t start = (int32_t)args[1].v.integer;
	int32_t end = (int32_t)args[2].v.integer;

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
	RETURN_SUCCESS;

failure:
	VM_PushNull(thread);
	RETURN_SUCCESS;
}

#define CHECKED_TO_REAL(thread,result) \
	int r = RealFromValue(thread, result); \
	if (r != OVUM_SUCCESS) return r;

AVES_API NATIVE_FUNCTION(aves_Real_opEquals)
{
	Aves *aves = Aves::Get(thread);

	bool result = false;
	if (RIGHT.type == aves->aves.Real)
	{
		double left = LEFT.v.real;
		double right = RIGHT.v.real;
		result = isnan(left) && isnan(right) || left == right;
	}
	else if (RIGHT.type == aves->aves.Int)
		result = LEFT.v.real == (double)RIGHT.v.integer;
	else if (RIGHT.type == aves->aves.UInt)
		result = LEFT.v.real == (double)RIGHT.v.uinteger;

	VM_PushBool(thread, result);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_Real_opCompare)
{
	Aves *aves = Aves::Get(thread);

	double right;
	if (RIGHT.type == aves->aves.Real)
		right = RIGHT.v.real;
	else if (RIGHT.type == aves->aves.Int)
		right = (double)RIGHT.v.integer;
	else if (RIGHT.type == aves->aves.UInt)
		right = (double)RIGHT.v.uinteger;
	else
		return VM_ThrowTypeError(thread);

	int result = real::Compare(LEFT.v.real, right);

	VM_PushInt(thread, result);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_Real_opAdd)
{
	CHECKED_TO_REAL(thread, &RIGHT);
	VM_PushReal(thread, LEFT.v.real + RIGHT.v.real);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_Real_opSubtract)
{
	CHECKED_TO_REAL(thread, &RIGHT);
	VM_PushReal(thread, LEFT.v.real - RIGHT.v.real);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_Real_opMultiply)
{
	CHECKED_TO_REAL(thread, &RIGHT);
	VM_PushReal(thread, LEFT.v.real * RIGHT.v.real);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_Real_opDivide)
{
	CHECKED_TO_REAL(thread, &RIGHT);
	VM_PushReal(thread, LEFT.v.real / RIGHT.v.real);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_Real_opModulo)
{
	CHECKED_TO_REAL(thread, &RIGHT);
	VM_PushReal(thread, fmod(LEFT.v.real, RIGHT.v.real));
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_Real_opPower)
{
	CHECKED_TO_REAL(thread, &RIGHT);
	VM_PushReal(thread, pow(LEFT.v.real, RIGHT.v.real));
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_Real_opPlus)
{
	VM_Push(thread, &LEFT);
	RETURN_SUCCESS;
}

AVES_API NATIVE_FUNCTION(aves_Real_opNegate)
{
	VM_PushReal(thread, -args[0].v.real);
	RETURN_SUCCESS;
}

namespace real
{
	int Compare(double left, double right)
	{
		// Real values are ordered as follows:
		//   NaN < -∞ < ... < -ε < -0.0 = +0.0 < +ε < ... < +∞
		// Notice that NaN is ordered before all other values.

		int nan = isnan(right) - isnan(left);
		if (nan != 0)
			return nan;
		if (left < right)
			return -1;
		if (left > right)
			return 1;
		return 0;
	}
}