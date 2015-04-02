#pragma once

#ifndef AVES__INT_H
#define AVES__INT_H

#include "aves.h"

AVES_API NATIVE_FUNCTION(aves_int);

AVES_API NATIVE_FUNCTION(aves_Int_getHashCode);

AVES_API NATIVE_FUNCTION(aves_Int_toString);
AVES_API NATIVE_FUNCTION(aves_Int_toStringf);

// Operators

AVES_API NATIVE_FUNCTION(aves_Int_opEquals);
AVES_API NATIVE_FUNCTION(aves_Int_opCompare);
AVES_API NATIVE_FUNCTION(aves_Int_opShiftLeft);
AVES_API NATIVE_FUNCTION(aves_Int_opShiftRight);
AVES_API NATIVE_FUNCTION(aves_Int_opAdd);
AVES_API NATIVE_FUNCTION(aves_Int_opSubtract);
AVES_API NATIVE_FUNCTION(aves_Int_opOr);
AVES_API NATIVE_FUNCTION(aves_Int_opXor);
AVES_API NATIVE_FUNCTION(aves_Int_opMultiply);
AVES_API NATIVE_FUNCTION(aves_Int_opDivide);
AVES_API NATIVE_FUNCTION(aves_Int_opModulo);
AVES_API NATIVE_FUNCTION(aves_Int_opAnd);
AVES_API NATIVE_FUNCTION(aves_Int_opPower);
AVES_API NATIVE_FUNCTION(aves_Int_opPlus);
AVES_API NATIVE_FUNCTION(aves_Int_opNegate);
AVES_API NATIVE_FUNCTION(aves_Int_opNot);

// Internal methods
namespace integer
{
	String *ToString(ThreadHandle thread, const int64_t value,
		const int radix, const int minWidth,
		const bool upper);

	int32_t ToStringDecimal(ThreadHandle thread, const int64_t value,
		const int minWidth,
		const int bufferSize, uchar *buf);
	int32_t ToStringHex(ThreadHandle thread, const int64_t value,
		const bool upper, const int minWidth,
		const int bufferSize, uchar *buf);
	int32_t ToStringRadix(ThreadHandle thread, const int64_t value,
		const int radix, const bool upper, const int minWidth,
		const int bufferSize, uchar *buf);

	int ParseFormatString(ThreadHandle thread, String *str, int *radix, int *minWidth, bool *upper);

	inline int Power(const int64_t base, const int64_t exponent, int64_t &output)
	{
		int64_t a = base;
		int64_t b = exponent;

		int64_t result = 1;
		while (b > 0)
		{
			if ((b & 1) != 0)
				if (Int_MultiplyChecked(result, a, result))
					return OVUM_ERROR_OVERFLOW;
			b >>= 1;
			if (b > 0)
				// This sometimes overflows for the last iteration, after which
				// the value is not even be used; for example, at 2**32 * 2**32
				if (Int_MultiplyChecked(a, a, a))
					return OVUM_ERROR_OVERFLOW;
		}

		output = result;
		RETURN_SUCCESS;
	}
}

#endif // AVES__INT_H