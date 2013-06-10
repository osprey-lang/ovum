#include <cmath>
#include "aves_math.h"

#define GET_REAL_VALUE()	double value = RealFromValue(thread, args[0]).real

AVES_API NATIVE_FUNCTION(aves_math_abs)
{
	Value value = args[0];

	if (IsUInt(value))
		VM_Push(thread, value);
	else if (IsReal(value))
		VM_PushReal(thread, abs(value.real));
	else if (IsInt(value))
	{
		if (value.integer == INT64_MIN)
			VM_ThrowOverflowError(thread);
		VM_PushInt(thread, abs(value.integer));
	}
	else
		VM_ThrowTypeError(thread);
}

AVES_API NATIVE_FUNCTION(aves_math_acos)
{
	GET_REAL_VALUE();

	//if (value < -1 || value > 1)
	//{
	//	GC_Construct(thread, ArgumentRangeError, 0, nullptr);
	//	VM_Throw(thread);
	//}

	VM_PushReal(thread, acos(value));
}

AVES_API NATIVE_FUNCTION(aves_math_asin)
{
	GET_REAL_VALUE();

	//if (value < -1 || value > 1)
	//{
	//	GC_Construct(thread, ArgumentRangeError, 0, nullptr);
	//	VM_Throw(thread);
	//}

	VM_PushReal(thread, asin(value));
}

AVES_API NATIVE_FUNCTION(aves_math_atan)
{
	GET_REAL_VALUE();

	VM_PushReal(thread, atan(value));
}

AVES_API NATIVE_FUNCTION(aves_math_atan2)
{
	double y = RealFromValue(thread, args[0]).real;
	double x = RealFromValue(thread, args[1]).real;

	VM_PushReal(thread, atan2(y, x));
}

AVES_API NATIVE_FUNCTION(aves_math_cbrt)
{
	GET_REAL_VALUE();

	VM_PushReal(thread, pow(value, 1.0/3.0));
}

AVES_API NATIVE_FUNCTION(aves_math_ceil)
{
	Value value = args[0];

	if (IsInt(value) || IsUInt(value))
		VM_Push(thread, value);
	else if (IsReal(value))
		VM_PushReal(thread, ceil(value.real));
	else
		VM_ThrowTypeError(thread);
}

AVES_API NATIVE_FUNCTION(aves_math_cos)
{
	GET_REAL_VALUE();

	VM_PushReal(thread, cos(value));
}

AVES_API NATIVE_FUNCTION(aves_math_cosh)
{
	GET_REAL_VALUE();

	VM_PushReal(thread, cosh(value));
}

AVES_API NATIVE_FUNCTION(aves_math_exp)
{
	GET_REAL_VALUE();

	VM_PushReal(thread, exp(value));
}

AVES_API NATIVE_FUNCTION(aves_math_floor)
{
	Value value = args[0];

	if (IsInt(value) || IsUInt(value))
		VM_Push(thread, value);
	else if (IsReal(value))
		VM_PushReal(thread, floor(value.real));
	else
		VM_ThrowTypeError(thread);
}

AVES_API NATIVE_FUNCTION(aves_math_logE)
{
	GET_REAL_VALUE();

	VM_PushReal(thread, log(value));
}

AVES_API NATIVE_FUNCTION(aves_math_logBase)
{
	GET_REAL_VALUE();

	double base = RealFromValue(thread, args[1]).real;

	// log x to base b = ln x / ln b

	VM_PushReal(thread, log(value) / log(base));
}

AVES_API NATIVE_FUNCTION(aves_math_log10)
{
	GET_REAL_VALUE();

	VM_PushReal(thread, log10(value));
}

AVES_API NATIVE_FUNCTION(aves_math_max)
{
	VM_Push(thread, args[1]); // b
	VM_Push(thread, args[0]); // a
	if (VM_Compare(thread) > 0) // b > a
		VM_Push(thread, args[1]); // b
	else
		VM_Push(thread, args[0]); // a
}

AVES_API NATIVE_FUNCTION(aves_math_min)
{
	VM_Push(thread, args[1]); // b
	VM_Push(thread, args[0]); // a
	if (VM_Compare(thread) < 0) // b < a
		VM_Push(thread, args[1]); // b
	else
		VM_Push(thread, args[0]); // a
}

AVES_API NATIVE_FUNCTION(aves_math_sign)
{
	Value value = args[0];
	if (IsUInt(value))
		VM_PushInt(thread, value.uinteger > 0); // If value.uinteger > 0, then push 1; otherwise, 0!
	else if (IsInt(value))
	{
		int64_t intValue = value.integer;
		VM_PushInt(thread,
			intValue > 0 ?  1 :
			intValue < 0 ? -1 :
			0);
	}
	else if (IsReal(value))
	{
		double realValue = value.real;
		VM_PushInt(thread,
			realValue > 0 ?  1 :
			realValue < 0 ? -1 :
			0); // includes NaN
	}
	else
		VM_ThrowTypeError(thread);
}

AVES_API NATIVE_FUNCTION(aves_math_sin)
{
	GET_REAL_VALUE();

	VM_PushReal(thread, sin(value));
}

AVES_API NATIVE_FUNCTION(aves_math_sinh)
{
	GET_REAL_VALUE();

	VM_PushReal(thread, sinh(value));
}

AVES_API NATIVE_FUNCTION(aves_math_sqrt)
{
	GET_REAL_VALUE();

	VM_PushReal(thread, sqrt(value));
}

AVES_API NATIVE_FUNCTION(aves_math_tan)
{
	GET_REAL_VALUE();

	VM_PushReal(thread, tan(value));
}

AVES_API NATIVE_FUNCTION(aves_math_tanh)
{
	GET_REAL_VALUE();

	VM_PushReal(thread, tanh(value));
}