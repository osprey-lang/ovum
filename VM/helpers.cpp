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


OVUM_API int IntFromValue(ThreadHandle thread, Value *v)
{
	if (v->type != VM::vm->types.Int)
	{
		if (v->type == VM::vm->types.UInt)
		{
			if (v->uinteger > INT64_MAX)
				return thread->ThrowOverflowError();
			v->type = VM::vm->types.Int; // This is safe: v is passed by value.
		}
		else if (v->type == VM::vm->types.Real)
		{
			// TODO: Verify that this is safe; if not, find another way of doing this.
			if (v->real > INT64_MAX || v->real < INT64_MIN)
				return thread->ThrowOverflowError();

			v->type = VM::vm->types.Int;
			v->integer = (int64_t)v->real;
		}
		else
			return thread->ThrowTypeError(errors::toIntFailed);
	}
	RETURN_SUCCESS;
}

OVUM_API int UIntFromValue(ThreadHandle thread, Value *v)
{
	if (v->type != VM::vm->types.UInt)
	{
		if (v->type == VM::vm->types.Int)
		{
			if (v->integer < 0) // simple! This is even safe if the architecture doesn't use 2's complement!
				return thread->ThrowOverflowError();
			v->type = VM::vm->types.UInt; // This is safe: v is passed by value
		}
		else if (v->type == VM::vm->types.Real)
		{
			// TODO: Verify that this is safe; if not, find another way of doing this.
			if (v->real > UINT64_MAX || v->real < 0)
				return thread->ThrowOverflowError();

			v->type = VM::vm->types.UInt;
			v->uinteger = (uint64_t)v->real;
		}
		else
			return thread->ThrowTypeError(errors::toUIntFailed);
	}
	RETURN_SUCCESS;
}

OVUM_API int RealFromValue(ThreadHandle thread, Value *v)
{
	// Note: during this conversion, it's more than possible that the
	// int or uint value is too large to be precisely represented as
	// a double. This is not considered an error condition.
	if (v->type != VM::vm->types.Real)
	{
		if (v->type == VM::vm->types.Int)
			SetReal_(v, (double)v->integer);
		else if (v->type == VM::vm->types.UInt)
			SetReal_(v, (double)v->uinteger);
		else
			return thread->ThrowTypeError(errors::toRealFailed);
	}
	RETURN_SUCCESS;
}

OVUM_API int StringFromValue(ThreadHandle thread, Value *v)
{
	if (v->type != VM::vm->types.String)
	{
		if (v->type == nullptr)
		{
			SetString_(v, static_strings::empty);
			RETURN_SUCCESS;
		}

		thread->Push(*v);
		int r = thread->InvokeMember(static_strings::toString, 0, v);
		if (r != OVUM_SUCCESS) return r;

		if (v->type != VM::vm->types.String)
			return thread->ThrowTypeError(static_strings::errors::ToStringWrongType);
	}
	RETURN_SUCCESS;
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