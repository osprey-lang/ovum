#pragma once

#ifndef AVES__UINT_H
#define AVES__UINT_H

#include "aves.h"

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
	String *ToString(ThreadHandle thread, const uint64_t value,
		const int radix, const int minWidth, const bool upper);

	int32_t ToStringDecimal(ThreadHandle thread, const uint64_t value,
		const int minWidth,
		const int bufferSize, uchar *buf);
	int32_t ToStringHex(ThreadHandle thread, const uint64_t value,
		const bool upper, const int minWidth,
		const int bufferSize, uchar *buf);
	int32_t ToStringRadix(ThreadHandle thread, const uint64_t value,
		const int radix, const bool upper, const int minWidth,
		const int bufferSize, uchar *buf);

	inline const uint64_t Power(ThreadHandle thread, const uint64_t base, const uint64_t exponent)
	{
		uint64_t a = base;
		uint64_t b = exponent;

		uint64_t result = 1;
		while (b > 0)
		{
			if ((b & 1) != 0)
				result = UInt_MultiplyChecked(thread, result, a);
			a = UInt_MultiplyChecked(thread, a, a);
			b >>= 1;
		}

		return result;
	}
}

#endif // AVES__UINT_H