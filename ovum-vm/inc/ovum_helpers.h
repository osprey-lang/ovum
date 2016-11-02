#ifndef OVUM__HELPERS_H
#define OVUM__HELPERS_H

// Various helper functions

#include "ovum.h"

inline void SetNull(Value &target)
{
	target.type = nullptr;
}
inline void SetNull(Value *target)
{
	target->type = nullptr;
}

inline void SetBool(ThreadHandle thread, Value &target, bool value)
{
	target.type = GetType_Boolean(thread);
	target.v.integer = value;
}
inline void SetBool(ThreadHandle thread, Value *target, bool value)
{
	target->type = GetType_Boolean(thread);
	target->v.integer = value;
}

inline void SetInt(ThreadHandle thread, Value &target, int64_t value)
{
	target.type = GetType_Int(thread);
	target.v.integer = value;
}
inline void SetInt(ThreadHandle thread, Value *target, int64_t value)
{
	target->type = GetType_Int(thread);
	target->v.integer = value;
}

inline void SetUInt(ThreadHandle thread, Value &target, uint64_t value)
{
	target.type = GetType_UInt(thread);
	target.v.uinteger = value;
}
inline void SetUInt(ThreadHandle thread, Value *target, uint64_t value)
{
	target->type = GetType_UInt(thread);
	target->v.uinteger = value;
}

inline void SetReal(ThreadHandle thread, Value &target, double value)
{
	target.type = GetType_Real(thread);
	target.v.real = value;
}
inline void SetReal(ThreadHandle thread, Value *target, double value)
{
	target->type = GetType_Real(thread);
	target->v.real = value;
}

inline void SetString(ThreadHandle thread, Value &target, String *value)
{
	target.type = GetType_String(thread);
	target.v.string = value;
}
inline void SetString(ThreadHandle thread, Value *target, String *value)
{
	target->type = GetType_String(thread);
	target->v.string = value;
}

OVUM_API int IntFromValue(ThreadHandle thread, Value *v);

OVUM_API int UIntFromValue(ThreadHandle thread, Value *v);

OVUM_API int RealFromValue(ThreadHandle thread, Value *v);

OVUM_API int StringFromValue(ThreadHandle thread, Value *v);

template<class T>
inline T Clamp(T value, T max, T min)
{
	return value < min ? min :
		value > max ? max :
		value;
}

template<int min, int max>
inline int Clamp(int value)
{
	return value < min ? min :
		value > max ? max :
		value;
}

template<long min, long max>
inline long Clamp(long value)
{
	return value < min ? min :
		value > max ? max :
		value;
}

template<long long min, long long max>
inline long long Clamp(long long value)
{
	return value < min ? min :
		value > max ? max :
		value;
}

template<class T>
inline void ReverseArray(size_t length, T values[])
{
	T *left = values;
	T *right = values + length - 1;

	while (left < right)
	{
		T temp = *left;
		*left = *right;
		*right = temp;

		left++;
		right--;
	}
}

// Copies the values from source to destination, in reverse order.
// 'length' is the total number of values to copy.
template<class T>
inline void CopyReversed(T destination[], const T source[], size_t length)
{
	const T *sourcePointer = source;
	T *destPointer = destination + length - 1;

	while (destPointer >= destination)
	{
		*destPointer = *sourcePointer;
		destPointer--;
		sourcePointer++;
	}
}

// This is really just a paper-thin type-safe wrapper around memcpy.
// The only real difference is that it uses a template.
// The size is multiplied by sizeof(T), by the way, so you don't have
// to worry about that either.
template<class T>
inline void CopyMemoryT(T *destination, const T *source, size_t size)
{
	memcpy(destination, source, sizeof(T) * size);
}

// Finds the smallest power of two that is greater than or equal to the given number.
template<class T>
inline T NextPowerOfTwo(T n)
{
	n--; // When n == 0, this overflows on purpose
	n |= n >> 1;
	n |= n >> 2;
	n |= n >> 4;
	n |= n >> 8;
	n |= n >> 16;
	return n + 1;
}

// Checked arithmetics

// Acknowledgement: The checked multiplication is basically lifted directly
// from SafeInt, albeit with slightly different variable names.

#define RETURN_OVERFLOW  return OVUM_ERROR_OVERFLOW
#define RETURN_DIV_ZERO  return OVUM_ERROR_DIVIDE_BY_ZERO

// UInt
inline int UInt_AddChecked(uint64_t left, uint64_t right, uint64_t &result)
{
	if (UINT64_MAX - left < right)
		RETURN_OVERFLOW;

	result = left + right;
	RETURN_SUCCESS;
}
inline int UInt_SubtractChecked(uint64_t left, uint64_t right, uint64_t &result)
{
	if (right > left)
		RETURN_OVERFLOW;

	result = left - right;
	RETURN_SUCCESS;
}
inline int UInt_MultiplyChecked(uint64_t left, uint64_t right, uint64_t &result)
{
#if OVUM_USE_INTRINSICS
	uint64_t high;
	result = _umul128(left, right, &high);

	if (high)
		RETURN_OVERFLOW;

	RETURN_SUCCESS;
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
		RETURN_OVERFLOW;

	if (output != 0)
	{
		if ((uint32_t)(output >> 32) != 0)
			RETURN_OVERFLOW;

		output <<= 32;
		uint64_t temp = (uint64_t)leftLow * (uint64_t)rightLow;
		output += temp;

		if (output < temp)
			RETURN_OVERFLOW;
	}
	else
		output = (uint64_t)leftLow * (uint64_t)rightLow;

	result = output;
	RETURN_SUCCESS;
#endif
}
inline int UInt_DivideChecked(uint64_t left, uint64_t right, uint64_t &result)
{
	if (right == 0)
		RETURN_DIV_ZERO;

	result = left / right;
	RETURN_SUCCESS;
}
inline int UInt_ModuloChecked(uint64_t left, uint64_t right, uint64_t &result)
{
	if (right == 0)
		RETURN_DIV_ZERO;

	// Note: modulo can never result in an overflow.

	result = left % right;
	RETURN_SUCCESS;
}

// Int
inline int Int_AddChecked(int64_t left, int64_t right, int64_t &result)
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
			RETURN_OVERFLOW;
	}

	result = left + right;
	RETURN_SUCCESS;
}
inline int Int_SubtractChecked(int64_t left, int64_t right, int64_t &result)
{
	if ((left ^ right) < 0)
	{
		// If the arguments have DIFFERENT signs, then an overflow is possible.
		// > If left is positive (and right is negative), then test  left > INT64_MAX + right
		// > If left is negative (and right is positive), then test  left < INT64_MIN + right
		if (left > 0 && left > INT64_MAX + right ||
			left < 0 && left < INT64_MIN + right)
			RETURN_OVERFLOW;
	}

	result = left - right;
	RETURN_SUCCESS;
}
inline int Int_MultiplyChecked(int64_t left, int64_t right, int64_t &result)
{
#if OVUM_USE_INTRINSICS
	int64_t high;
	result = _mul128(left, right, &high);

	if ((left ^ right) < 0)
	{
		if (!(high == -1 && output < 0 ||
			high == 0 && output == 0))
			RETURN_OVERFLOW;
	}
	else if (high == 0 && (uint64_t)output <= INT64_MAX)
		RETURN_OVERFLOW;

	RETURN_SUCCESS;
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

	uint64_t temp;
	if (UInt_MultiplyChecked((uint64_t)leftCopy, (uint64_t)rightCopy, temp))
		RETURN_OVERFLOW;

	if (leftNeg ^ rightNeg)
	{
		// result must be negative
		if (temp <= (uint64_t)INT64_MIN)
		{
			result = -(int64_t)temp;
			RETURN_SUCCESS;
		}
	}
	else
	{
		// result must be positive
		if (temp <= (uint64_t)INT64_MAX)
		{
			result = (int64_t)temp;
			RETURN_SUCCESS;
		}
	}

	RETURN_OVERFLOW;
#endif
}
inline int Int_DivideChecked(int64_t left, int64_t right, int64_t &result)
{
	if (right == 0)
		RETURN_DIV_ZERO;
	if (left == INT64_MIN && right == -1)
		RETURN_OVERFLOW;

	result = left / right;
	RETURN_SUCCESS;
}
inline int Int_ModuloChecked(int64_t left, int64_t right, int64_t &result)
{
	if (right == 0)
		RETURN_DIV_ZERO;

	// Note: modulo can never result in an overflow.

	result = left % right;
	RETURN_SUCCESS;
}

#undef RETURN_OVERFLOW
#undef RETURN_DIV_ZERO

// Hash helpers

// Gets the next prime number greater than or equal to the given value.
// The prime number is suitable for use as the size of a hash table.
OVUM_API int32_t HashHelper_GetPrime(int32_t min);

#endif // OVUM__HELPERS_H
