#pragma once

#ifndef VM__HELPERS_H
#define VM__HELPERS_H

// Various helper functions

#include "ov_vm.h"

OVUM_API void IntFromValue(ThreadHandle thread, Value *v);

OVUM_API void UIntFromValue(ThreadHandle thread, Value *v);

OVUM_API void RealFromValue(ThreadHandle thread, Value *v);

OVUM_API void StringFromValue(ThreadHandle thread, Value *v);


// Checked arithmetics

// Acknowledgement: The checked multiplication is basically lifted directly
// from SafeInt, albeit with slightly different variable names.

// UINT
inline uint64_t UInt_AddChecked(ThreadHandle thread, const uint64_t left, const uint64_t right)
{
	if (UINT64_MAX - left < right)
		VM_ThrowOverflowError(thread);

	return left + right;
}
inline uint64_t UInt_SubtractChecked(ThreadHandle thread, const uint64_t left, const uint64_t right)
{
	if (right > left)
		VM_ThrowOverflowError(thread);

	return left - right;
}
inline uint64_t UInt_MultiplyChecked(ThreadHandle thread, const uint64_t left, const uint64_t right)
{
#if USE_INTRINSICS
	uint64_t high;
	uint64_t output = _umul128(left, right, &high);

	if (high)
		VM_ThrowOverflowError(thread);

	return output;
#else
	uint32_t leftHigh, leftLow, rightHigh, rightLow;

    leftHigh  = (uint32_t)(left >> 32);
    leftLow   = (uint32_t)left;
    rightHigh = (uint32_t)(right >> 32);
    rightLow  = (uint32_t)right;

	uint64_t output = 0;

	if (leftHigh == 0)
	{
		if (rightHigh != 0)
			output = (uint64_t)leftLow * (uint64_t)rightHigh;
	}
	else if (rightHigh == 0)
	{
		if (leftHigh != 0)
			output = (uint64_t)leftHigh * (uint64_t)rightLow;
	}
	else
		VM_ThrowOverflowError(thread);

	if (output != 0)
	{
		if ((uint32_t)(output >> 32) != 0)
			VM_ThrowOverflowError(thread);

		output <<= 32;
		uint64_t temp = (uint64_t)leftLow * (uint64_t)rightLow;
		output += temp;

		if (output < temp)
			VM_ThrowOverflowError(thread);
	}
	else
		output = (uint64_t)leftLow * (uint64_t)rightLow;

	return output;
#endif
}
inline uint64_t UInt_DivideChecked(ThreadHandle thread, const uint64_t left, const uint64_t right)
{
	if (right == 0)
		VM_ThrowDivideByZeroError(thread);

	return left / right;
}
inline uint64_t UInt_ModuloChecked(ThreadHandle thread, const uint64_t left, const uint64_t right)
{
	if (right == 0)
		VM_ThrowDivideByZeroError(thread);

	// Note: modulo can never result in an overflow.

	return left % right;
}

// INT
inline int64_t Int_AddChecked(ThreadHandle thread, const int64_t left, const int64_t right)
{
	if ((left ^ right) >= 0)
	{
		// If both integers have the same sign, then an overflow is possible.
		// > If both are positive, then test  INT64_MAX - left < right
		// > If both are negative, then test  left < INT64_MIN - right
		// NOTE: if both operands are 0, this is evaluated too.
		bool neg = left < 0;
		if (neg && (left < INT64_MIN - right) ||
			!neg && (INT64_MAX - left < right))
			VM_ThrowOverflowError(thread);
	}

	return left + right;
}
inline int64_t Int_SubtractChecked(ThreadHandle thread, const int64_t left, const int64_t right)
{
	if ((left ^ right) < 0)
	{
		// If the arguments have DIFFERENT signs, then an overflow is possible.
		// > If left is positive (and right is negative), then test  left > INT64_MAX + right
		// > If left is negative (and right is positive), then test  left < INT64_MIN + right
		if (left > 0 && left > INT64_MAX + right ||
			left < 0 && left < INT64_MIN + right)
			VM_ThrowOverflowError(thread);
	}

	return left - right;
}
inline int64_t Int_MultiplyChecked(ThreadHandle thread, const int64_t left, const int64_t right)
{
#if USE_INTRINSICS
	int64_t high;
	int64_t output = _mul128(left, right, &high);

	if ((left ^ right) < 0)
	{
		if (!(high == -1 && output < 0 ||
			high == 0 && output == 0))
			VM_ThrowOverflowError(thread);
	}
	else if (high == 0 && (uint64_t)output <= INT64_MAX)
		VM_ThrowOverflowError(thread);

	return output;
#else
	bool leftNeg  = false;
	bool rightNeg = false;

	int64_t leftCopy  = left;
	int64_t rightCopy = right;

	if (leftCopy < 0)
	{
		leftNeg = true;
		leftCopy = -leftCopy; // NOTE: overflows on INT64_MIN
	}

	if (rightCopy < 0)
	{
		rightNeg = true;
		rightCopy = -rightCopy; // Also overflows on INT64_MIN
	}

	uint64_t temp = UInt_MultiplyChecked(thread, (uint64_t)leftCopy, (uint64_t)rightCopy);

	// If the unsigned multiplication overflowed, the thread would have thrown.

	if (leftNeg ^ rightNeg)
	{
		// result must be negative
		if (temp <= (uint64_t)INT64_MIN)
			return -(int64_t)temp;
	}
	else
	{
		// result must be positive
		if (temp <= (uint64_t)INT64_MAX)
			return (int64_t)temp;
	}

	VM_ThrowOverflowError(thread);
	return 0; // compilers are not clever enough to realise that VM_ThrowOverflowError() exits the method
#endif
}
inline int64_t Int_DivideChecked(ThreadHandle thread, const int64_t left, const int64_t right)
{
	if (right == 0)
		VM_ThrowDivideByZeroError(thread);
	if (left == INT64_MIN && right == -1)
		VM_ThrowOverflowError(thread);

	return left / right;
}
inline int64_t Int_ModuloChecked(ThreadHandle thread, const int64_t left, const int64_t right)
{
	if (right == 0)
		VM_ThrowDivideByZeroError(thread);

	// Note: modulo can never result in an overflow.

	return left % right;
}

// HASH HELPERS

// Gets the next prime number greater than or equal to the given value.
// The prime number is suitable for use as the size of a hash table.
OVUM_API int32_t HashHelper_GetPrime(const int32_t min);


#endif // VM__HELPERS_H