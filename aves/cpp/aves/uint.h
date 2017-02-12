#ifndef AVES__UINT_H
#define AVES__UINT_H

#include "../aves.h"

AVES_API NATIVE_FUNCTION(aves_uint);

AVES_API NATIVE_FUNCTION(aves_UInt_getHashCode);

AVES_API NATIVE_FUNCTION(aves_UInt_toString);
AVES_API NATIVE_FUNCTION(aves_UInt_toStringf);

AVES_API NATIVE_FUNCTION(aves_UInt_opEquals);
AVES_API NATIVE_FUNCTION(aves_UInt_opCompare);
AVES_API NATIVE_FUNCTION(aves_UInt_opShiftLeft);
AVES_API NATIVE_FUNCTION(aves_UInt_opShiftRight);
AVES_API NATIVE_FUNCTION(aves_UInt_opAdd);
AVES_API NATIVE_FUNCTION(aves_UInt_opSubtract);
AVES_API NATIVE_FUNCTION(aves_UInt_opOr);
AVES_API NATIVE_FUNCTION(aves_UInt_opXor);
AVES_API NATIVE_FUNCTION(aves_UInt_opMultiply);
AVES_API NATIVE_FUNCTION(aves_UInt_opDivide);
AVES_API NATIVE_FUNCTION(aves_UInt_opModulo);
AVES_API NATIVE_FUNCTION(aves_UInt_opAnd);
AVES_API NATIVE_FUNCTION(aves_UInt_opPower);
AVES_API NATIVE_FUNCTION(aves_UInt_opPlus);
AVES_API NATIVE_FUNCTION(aves_UInt_opNot);

// Internal methods
namespace uinteger
{
	String *ToString(
		ThreadHandle thread,
		uint64_t value,
		int radix,
		size_t minWidth,
		bool upper
	);

	size_t ToStringDecimal(
		ThreadHandle thread,
		uint64_t value,
		size_t minWidth,
		size_t bufferSize,
		ovchar_t *buf
	);

	size_t ToStringHex(
		ThreadHandle thread,
		uint64_t value,
		bool upper,
		size_t minWidth,
		size_t bufferSize,
		ovchar_t *buf
	);

	size_t ToStringRadix(
		ThreadHandle thread,
		uint64_t value,
		int radix,
		bool upper,
		size_t minWidth,
		size_t bufferSize,
		ovchar_t *buf
	);

	inline int Power(uint64_t base, uint64_t exponent, uint64_t &output)
	{
		uint64_t a = base;
		uint64_t b = exponent;

		uint64_t result = 1;
		while (b > 0)
		{
			if ((b & 1) != 0)
				if (UInt_MultiplyChecked(result, a, result))
					return OVUM_ERROR_OVERFLOW;
			b >>= 1;
			if (b > 0)
				// This sometimes overflows for the last iteration, after which
				// the value is not even be used; specifically, at 2**32 * 2**32
				if (UInt_MultiplyChecked(a, a, a))
					return OVUM_ERROR_OVERFLOW;
		}

		output = result;
		RETURN_SUCCESS;
	}
}

#endif // AVES__UINT_H
