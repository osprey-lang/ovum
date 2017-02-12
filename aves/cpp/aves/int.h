#ifndef AVES__INT_H
#define AVES__INT_H

#include "../aves.h"

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
	String *ToString(
		ThreadHandle thread,
		int64_t value,
		int radix,
		size_t minWidth,
		bool upper
	);

	size_t ToStringDecimal(
		ThreadHandle thread,
		int64_t value,
		size_t minWidth,
		size_t bufferSize,
		ovchar_t *buf
	);

	size_t ToStringHex(
		ThreadHandle thread,
		int64_t value,
		bool upper,
		size_t minWidth,
		size_t bufferSize,
		ovchar_t *buf
	);

	size_t ToStringRadix(
		ThreadHandle thread,
		int64_t value,
		int radix,
		bool upper,
		size_t minWidth,
		size_t bufferSize,
		ovchar_t *buf
	);

	int ParseFormatString(
		ThreadHandle thread,
		String *str,
		int *radix,
		size_t *minWidth,
		bool *upper
	);

	inline int Power(int64_t base, int64_t exponent, int64_t &output)
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
