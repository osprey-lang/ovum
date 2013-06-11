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


OVUM_API Value IntFromValue(ThreadHandle thread, Value v)
{
	if (v.type == stdTypes.UInt)
	{
		if (v.uinteger > INT64_MAX)
			_Th(thread)->ThrowOverflowError();
		v.type = stdTypes.Int; // This is safe: v is passed by value.
	}
	else if (v.type == stdTypes.Real)
	{
		// TODO: Verify that this is safe; if not, find another way of doing this.
		if (v.real > INT64_MAX || v.real < INT64_MIN)
			_Th(thread)->ThrowOverflowError();

		v.type = stdTypes.Int;
		v.integer = (int64_t)v.real;
	}
	else if (v.type != stdTypes.Int)
		_Th(thread)->ThrowTypeError(errors::toIntFailed);

	return v;
}

OVUM_API Value UIntFromValue(ThreadHandle thread, Value v)
{
	if (v.type == stdTypes.Int)
	{
		if (v.integer < 0) // simple! This is even safe if the architecture doesn't use 2's complement!
			_Th(thread)->ThrowOverflowError();
		v.type = stdTypes.UInt; // This is safe: v is passed by value
	}
	else if (v.type == stdTypes.Real)
	{
		// TODO: Verify that this is safe; if not, find another way of doing this.
		if (v.real > UINT64_MAX || v.real < 0)
			_Th(thread)->ThrowOverflowError();

		v.type = stdTypes.UInt;
		v.uinteger = (uint64_t)v.real;
	}
	else if (v.type != stdTypes.UInt)
		_Th(thread)->ThrowTypeError(errors::toUIntFailed);

	return v;
}

OVUM_API Value RealFromValue(ThreadHandle thread, Value v)
{
	// Note: during this conversion, it's more than possible that the
	// int or uint value is too large to be precisely represented as
	// a double. This is not considered an error condition.
	if (v.type == stdTypes.Int)
		SetReal_(v, (double)v.integer);
	else if (v.type == stdTypes.UInt)
		SetReal_(v, (double)v.uinteger);
	else if (v.type != stdTypes.Real)
		_Th(thread)->ThrowTypeError(errors::toRealFailed);

	return v;
}

OVUM_API Value StringFromValue(ThreadHandle thread, Value v)
{
	if (v.type != stdTypes.String)
	{
		_Th(thread)->Push(v);
		_Th(thread)->InvokeMember(static_strings::toString, 0, &v);
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
		if (neg && !(left < INT64_MIN - right) ||
			!neg && !(INT64_MAX - left < right))
			_Th(thread)->ThrowOverflowError();
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
			_Th(thread)->ThrowOverflowError();
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
			_Th(thread)->ThrowOverflowError();
	}
	else if (high == 0 && (uint64_t)output <= INT64_MAX)
		_Th(thread)->ThrowOverflowError();

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

	_Th(thread)->ThrowOverflowError();
	return 0; // compilers are not clever enough to realise that thread->ThrowOverflowError() exits the method
}

#endif

OVUM_API int64_t Int_DivideChecked(ThreadHandle thread, const int64_t left, const int64_t right)
{
	if (right == 0)
		_Th(thread)->ThrowDivideByZeroError();
	if (left == INT64_MIN && right == -1)
		_Th(thread)->ThrowOverflowError();

	return left / right;
}

OVUM_API int64_t Int_ModuloChecked(ThreadHandle thread, const int64_t left, const int64_t right)
{
	if (right == 0)
		_Th(thread)->ThrowDivideByZeroError();

	// Note: modulo can never result in an overflow.

	return left % right;
}

// UINT

OVUM_API uint64_t UInt_AddChecked(ThreadHandle thread, const uint64_t left, const uint64_t right)
{
	if (UINT64_MAX - left < right)
		_Th(thread)->ThrowOverflowError();

	return left + right;
}

OVUM_API uint64_t UInt_SubtractChecked(ThreadHandle thread, const uint64_t left, const uint64_t right)
{
	if (right > left)
		_Th(thread)->ThrowOverflowError();

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
		_Th(thread)->ThrowOverflowError();

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
		_Th(thread)->ThrowOverflowError();

	if (output != 0)
	{
		if ((uint32_t)(output >> 32) != 0)
			_Th(thread)->ThrowOverflowError();

		output <<= 32;
		uint64_t temp = (uint64_t)leftLow * (uint64_t)rightLow;
		output += temp;

		if (output < temp)
			_Th(thread)->ThrowOverflowError();
	}
	else
		output = (uint64_t)leftLow * (uint64_t)rightLow;

	return output;
}

#endif

OVUM_API uint64_t UInt_DivideChecked(ThreadHandle thread, const uint64_t left, const uint64_t right)
{
	if (right == 0)
		_Th(thread)->ThrowDivideByZeroError();

	return left / right;
}

OVUM_API uint64_t UInt_ModuloChecked(ThreadHandle thread, const uint64_t left, const uint64_t right)
{
	if (right == 0)
		_Th(thread)->ThrowDivideByZeroError();

	// Note: modulo can never result in an overflow.

	return left % right;
}