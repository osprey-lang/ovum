#include <cmath>
#include "aves_math.h"

#define GET_REAL_VALUE() CHECKED(RealFromValue(thread, args)); double value = args[0].real;

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
			return VM_ThrowOverflowError(thread);
		if (value.integer < 0)
			value.integer = -value.integer;
		VM_PushInt(thread, value.integer);
	}
	else
		return VM_ThrowTypeError(thread);
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_math_acos)
{
	GET_REAL_VALUE();
	
	VM_PushReal(thread, acos(value));
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_math_asin)
{
	GET_REAL_VALUE();

	//if (value < -1 || value > 1)
	//{
	//	GC_Construct(thread, ArgumentRangeError, 0, nullptr);
	//	VM_Throw(thread);
	//}

	VM_PushReal(thread, asin(value));
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_math_atan)
{
	GET_REAL_VALUE();

	VM_PushReal(thread, atan(value));
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_math_atan2)
{
	CHECKED(RealFromValue(thread, args + 0));
	CHECKED(RealFromValue(thread, args + 1));
	double y = args[0].real;
	double x = args[1].real;

	VM_PushReal(thread, atan2(y, x));
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_math_cbrt)
{
	GET_REAL_VALUE();

	VM_PushReal(thread, pow(value, 1.0/3.0));
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_math_ceil)
{
	Value value = args[0];

	if (IsInt(value) || IsUInt(value))
		VM_Push(thread, value);
	else if (IsReal(value))
		VM_PushReal(thread, ceil(value.real));
	else
		return VM_ThrowTypeError(thread);
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_math_cos)
{
	GET_REAL_VALUE();

	VM_PushReal(thread, cos(value));
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_math_cosh)
{
	GET_REAL_VALUE();

	VM_PushReal(thread, cosh(value));
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_math_exp)
{
	GET_REAL_VALUE();

	VM_PushReal(thread, exp(value));
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_math_floor)
{
	Value value = args[0];

	if (IsInt(value) || IsUInt(value))
		VM_Push(thread, value);
	else if (IsReal(value))
		VM_PushReal(thread, floor(value.real));
	else
		return VM_ThrowTypeError(thread);
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_math_logE)
{
	GET_REAL_VALUE();

	VM_PushReal(thread, log(value));
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_math_logBase)
{
	GET_REAL_VALUE();

	CHECKED(RealFromValue(thread, args + 1));
	double base = args[1].real;

	// log x to base b = ln x / ln b

	VM_PushReal(thread, log(value) / log(base));
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_math_log10)
{
	GET_REAL_VALUE();

	VM_PushReal(thread, log10(value));
}
END_NATIVE_FUNCTION

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
		return VM_ThrowTypeError(thread);
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_math_sin)
{
	GET_REAL_VALUE();

	VM_PushReal(thread, sin(value));
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_math_sinh)
{
	GET_REAL_VALUE();

	VM_PushReal(thread, sinh(value));
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_math_sqrt)
{
	GET_REAL_VALUE();

	VM_PushReal(thread, sqrt(value));
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_math_tan)
{
	GET_REAL_VALUE();

	VM_PushReal(thread, tan(value));
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_math_tanh)
{
	GET_REAL_VALUE();

	VM_PushReal(thread, tanh(value));
}
END_NATIVE_FUNCTION