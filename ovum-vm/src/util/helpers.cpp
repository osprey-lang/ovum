#include "../vm.h"
#include "../../inc/ovum_helpers.h"
#include "../ee/thread.h"
#include "../object/value.h"
#include "../res/staticstrings.h"
#include <math.h>

namespace hash_helper
{
	const size_t PrimeCount = 72;
	const size_t Primes[] = {
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
	ovum::VM *vm = thread->GetVM();
	if (v->type != vm->types.Int)
	{
		if (v->type == vm->types.UInt)
		{
			if (v->v.uinteger > INT64_MAX)
				return thread->ThrowOverflowError();
			v->type = vm->types.Int; // This is safe: v is passed by value.
		}
		else if (v->type == vm->types.Real)
		{
			// TODO: Verify that this is safe; if not, find another way of doing this.
			if (v->v.real > INT64_MAX || v->v.real < INT64_MIN)
				return thread->ThrowOverflowError();

			v->type = vm->types.Int;
			v->v.integer = (int64_t)v->v.real;
		}
		else
		{
			return thread->ThrowTypeConversionError(thread->GetStrings()->error.ToIntFailed);
		}
	}
	RETURN_SUCCESS;
}

OVUM_API int UIntFromValue(ThreadHandle thread, Value *v)
{
	ovum::VM *vm = thread->GetVM();
	if (v->type != vm->types.UInt)
	{
		if (v->type == vm->types.Int)
		{
			if (v->v.integer < 0) // simple! This is even safe if the architecture doesn't use 2's complement!
				return thread->ThrowOverflowError();
			v->type = vm->types.UInt; // This is safe: v is passed by value
		}
		else if (v->type == vm->types.Real)
		{
			// TODO: Verify that this is safe; if not, find another way of doing this.
			if (v->v.real > UINT64_MAX || v->v.real < 0)
				return thread->ThrowOverflowError();

			v->type = vm->types.UInt;
			v->v.uinteger = (uint64_t)v->v.real;
		}
		else
		{
			return thread->ThrowTypeConversionError(thread->GetStrings()->error.ToUIntFailed);
		}
	}
	RETURN_SUCCESS;
}

OVUM_API int RealFromValue(ThreadHandle thread, Value *v)
{
	ovum::VM *vm = thread->GetVM();
	// Note: during this conversion, it's more than possible that the
	// int or uint value is too large to be precisely represented as
	// a double. This is not considered an error condition.
	if (v->type != vm->types.Real)
	{
		double result;
		if (v->type == vm->types.Int)
			result = (double)v->v.integer;
		else if (v->type == vm->types.UInt)
			result = (double)v->v.uinteger;
		else
			return thread->ThrowTypeConversionError(thread->GetStrings()->error.ToRealFailed);
		v->type = vm->types.Real;
		v->v.real = result;
	}
	RETURN_SUCCESS;
}

OVUM_API int StringFromValue(ThreadHandle thread, Value *v)
{
	ovum::VM *vm = thread->GetVM();
	if (v->type != vm->types.String)
	{
		if (v->type == nullptr)
		{
			ovum::SetString_(vm, v, thread->GetStrings()->empty);
			RETURN_SUCCESS;
		}

		thread->Push(v);
		int r = thread->InvokeMember(thread->GetStrings()->members.toString, 0, v);
		if (r != OVUM_SUCCESS) return r;

		if (v->type != vm->types.String)
			return thread->ThrowTypeConversionError(thread->GetStrings()->error.ToStringWrongReturnType);
	}
	RETURN_SUCCESS;
}

// HASH HELPERS

bool HashHelper_IsPrime(size_t n)
{
	if ((n & 1) == 0)
		// 2 is the only even prime!
		return n == 2;

	size_t max = (size_t)sqrt((double)n);
	for (size_t div = 3; div <= max; div += 2)
		if ((n % div) == 0)
			return false;

	return true;
}

OVUM_API size_t HashHelper_GetPrime(size_t min)
{
	// Check the table first
	for (size_t i = 0; i < hash_helper::PrimeCount; i++)
		if (hash_helper::Primes[i] >= min)
			return hash_helper::Primes[i];

	// Outside of the table; time to compute!
	for (size_t i = min | 1; i < SIZE_MAX; i += 2)
		if (HashHelper_IsPrime(i))
			return i;

	// Oh well.
	return min;
}
