#include "ov_vm.internal.h"
#include "ov_helpers.h"

namespace errors
{
	namespace
	{
		LitString<43> _toIntFailed  = LitString<43>::FromCString("The value could not be converted to an Int.");
		LitString<43> _toUIntFailed = LitString<43>::FromCString("The value could not be converted to a UInt.");
		LitString<43> _toRealFailed = LitString<43>::FromCString("The value could not be converted to a Real.");
	}

	String *toIntFailed  = _S(_toIntFailed);
	String *toUIntFailed = _S(_toUIntFailed);
	String *toRealFailed = _S(_toRealFailed);
}

namespace hash_helper
{
	const int32_t PrimeCount = 72;
	const int32_t Primes[] = {
		3, 7, 11, 17, 23, 29, 37, 47, 59, 71, 89, 107, 131, 163, 197,
		239, 293, 353, 431, 521, 631, 761, 919, 1103, 1327, 1597, 1931,
		2333, 2801, 3371, 4049, 4861, 5839, 7013, 8419, 10103, 12143,
		14591, 17519, 21023, 25229, 30293, 36353, 43627, 52361, 62851,
		75431, 90523, 108631, 130363, 156437, 187751, 225307, 270371,
		324449, 389357, 467237, 560689, 672827, 807403, 968897, 1162687,
		1395263, 1674319, 2009191, 2411033, 2893249, 3471899, 4166287,
		4999559, 5999471, 7199369
	};
}


OVUM_API Value IntFromValue(ThreadHandle thread, Value v)
{
	if (v.type == VM::vm->types.UInt)
	{
		if (v.uinteger > INT64_MAX)
			thread->ThrowOverflowError();
		v.type = VM::vm->types.Int; // This is safe: v is passed by value.
	}
	else if (v.type == VM::vm->types.Real)
	{
		// TODO: Verify that this is safe; if not, find another way of doing this.
		if (v.real > INT64_MAX || v.real < INT64_MIN)
			thread->ThrowOverflowError();

		v.type = VM::vm->types.Int;
		v.integer = (int64_t)v.real;
	}
	else if (v.type != VM::vm->types.Int)
		thread->ThrowTypeError(errors::toIntFailed);

	return v;
}

OVUM_API Value UIntFromValue(ThreadHandle thread, Value v)
{
	if (v.type == VM::vm->types.Int)
	{
		if (v.integer < 0) // simple! This is even safe if the architecture doesn't use 2's complement!
			thread->ThrowOverflowError();
		v.type = VM::vm->types.UInt; // This is safe: v is passed by value
	}
	else if (v.type == VM::vm->types.Real)
	{
		// TODO: Verify that this is safe; if not, find another way of doing this.
		if (v.real > UINT64_MAX || v.real < 0)
			thread->ThrowOverflowError();

		v.type = VM::vm->types.UInt;
		v.uinteger = (uint64_t)v.real;
	}
	else if (v.type != VM::vm->types.UInt)
		thread->ThrowTypeError(errors::toUIntFailed);

	return v;
}

OVUM_API Value RealFromValue(ThreadHandle thread, Value v)
{
	// Note: during this conversion, it's more than possible that the
	// int or uint value is too large to be precisely represented as
	// a double. This is not considered an error condition.
	if (v.type == VM::vm->types.Int)
		SetReal_(v, (double)v.integer);
	else if (v.type == VM::vm->types.UInt)
		SetReal_(v, (double)v.uinteger);
	else if (v.type != VM::vm->types.Real)
		thread->ThrowTypeError(errors::toRealFailed);

	return v;
}

OVUM_API Value StringFromValue(ThreadHandle thread, Value v)
{
	if (v.type != VM::vm->types.String)
	{
		if (v.type == nullptr)
		{
			SetString_(v, static_strings::empty);
			return v;
		}

		thread->Push(v);
		thread->InvokeMember(static_strings::toString, 0, &v);

		if (v.type != VM::vm->types.String)
			thread->ThrowTypeError(static_strings::errors::ToStringWrongType);
	}

	return v;
}

// Checked arithmetics

// INT

OVUM_API int64_t Int_AddChecked(ThreadHandle thread, const int64_t left, const int64_t right)
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
			thread->ThrowOverflowError();
	}

	return left + right;
}

OVUM_API int64_t Int_SubtractChecked(ThreadHandle thread, const int64_t left, const int64_t right)
{
	if ((left ^ right) < 0)
	{
		// If the arguments have DIFFERENT signs, then an overflow is possible.
		// > If left is positive, then test  left > INT64_MAX + right
		// > If left is negative, then test  left < INT64_MAX + right
		if (left < 0 && !(left > INT64_MAX + right) ||
			left > 0 && !(left < INT64_MAX + right))
			thread->ThrowOverflowError();
	}

	return left - right;
}

// The checked multiplication is basically lifted directly from SafeInt,
// albeit with slightly different variable names.
#if USE_INTRINSICS

OVUM_API int64_t Int_MultiplyChecked(ThreadHandle thread, const int64_t left, const int64_t right)
{
	int64_t high;
	int64_t output = _mul128(left, right, &high);

	if ((left ^ right) < 0)
	{
		if (!(high == -1 && output < 0 ||
			high == 0 && output == 0))
			thread->ThrowOverflowError();
	}
	else if (high == 0 && (uint64_t)output <= INT64_MAX)
		thread->ThrowOverflowError();

	return output;
}

#else

OVUM_API int64_t Int_MultiplyChecked(ThreadHandle thread, const int64_t left, const int64_t right)
{
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

	thread->ThrowOverflowError();
	return 0; // compilers are not clever enough to realise that thread->ThrowOverflowError() exits the method
}

#endif

OVUM_API int64_t Int_DivideChecked(ThreadHandle thread, const int64_t left, const int64_t right)
{
	if (right == 0)
		thread->ThrowDivideByZeroError();
	if (left == INT64_MIN && right == -1)
		thread->ThrowOverflowError();

	return left / right;
}

OVUM_API int64_t Int_ModuloChecked(ThreadHandle thread, const int64_t left, const int64_t right)
{
	if (right == 0)
		thread->ThrowDivideByZeroError();

	// Note: modulo can never result in an overflow.

	return left % right;
}

// UINT

OVUM_API uint64_t UInt_AddChecked(ThreadHandle thread, const uint64_t left, const uint64_t right)
{
	if (UINT64_MAX - left < right)
		thread->ThrowOverflowError();

	return left + right;
}

OVUM_API uint64_t UInt_SubtractChecked(ThreadHandle thread, const uint64_t left, const uint64_t right)
{
	if (right > left)
		thread->ThrowOverflowError();

	return left - right;
}

// The checked multiplication is basically lifted directly from SafeInt,
// albeit with slightly different variable names.
#if USE_INTRINSICS

OVUM_API uint64_t UInt_MultiplyChecked(ThreadHandle thread, const uint64_t left, const uint64_t right)
{
	uint64_t high;
	uint64_t output = _mul128(left, right, &high);

	if (high)
		thread->ThrowOverflowError();

	return output;
}

#else

OVUM_API uint64_t UInt_MultiplyChecked(ThreadHandle thread, const uint64_t left, const uint64_t right)
{
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
		thread->ThrowOverflowError();

	if (output != 0)
	{
		if ((uint32_t)(output >> 32) != 0)
			thread->ThrowOverflowError();

		output <<= 32;
		uint64_t temp = (uint64_t)leftLow * (uint64_t)rightLow;
		output += temp;

		if (output < temp)
			thread->ThrowOverflowError();
	}
	else
		output = (uint64_t)leftLow * (uint64_t)rightLow;

	return output;
}

#endif

OVUM_API uint64_t UInt_DivideChecked(ThreadHandle thread, const uint64_t left, const uint64_t right)
{
	if (right == 0)
		thread->ThrowDivideByZeroError();

	return left / right;
}

OVUM_API uint64_t UInt_ModuloChecked(ThreadHandle thread, const uint64_t left, const uint64_t right)
{
	if (right == 0)
		thread->ThrowDivideByZeroError();

	// Note: modulo can never result in an overflow.

	return left % right;
}

// HASH HELPERS

bool HashHelper_IsPrime(const int32_t n)
{
	if ((n & 1) == 0)
		// 2 is the only even prime!
		return n == 2;

	int32_t max = (int32_t)sqrt((double)n);
	for (int32_t div = 3; div <= max; div += 2)
		if ((n % div) == 0)
			return false;

	return true;
}

OVUM_API int32_t HashHelper_GetPrime(const int32_t min)
{
	// Check the table first
	for (int i = 0; i < hash_helper::PrimeCount; i++)
		if (hash_helper::Primes[i] >= min)
			return hash_helper::Primes[i];

	// Outside of the table; time to compute!
	for (int32_t i = min | 1; i < INT32_MAX; i += 2)
		if (HashHelper_IsPrime(i))
			return i;

	// Oh well.
	return min;
}