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

	void ParseFormatString(ThreadHandle thread, String *str, int *radix, int *minWidth, bool *upper);

	inline const int64_t Power(ThreadHandle thread, const int64_t base, const int64_t exponent)
	{
		if (exponent < 0)
		{
			GC_Construct(thread, Types::ArgumentRangeError, 0, nullptr);
			VM_Throw(thread);
		}

		int64_t a = base;
		int64_t b = exponent;

		int64_t result = 1;
		while (b > 0)
		{
			if ((b & 1) != 0)
				result = Int_MultiplyChecked(thread, result, a);
			a = Int_MultiplyChecked(thread, a, a);
			b >>= 1;
		}

		return result;
	}
}

#endif // AVES__INT_H