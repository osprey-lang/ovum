#include "math.h"
#include "../aves_state.h"
#include <cmath>

using namespace aves;

#define GET_REAL_VALUE() CHECKED(RealFromValue(thread, args)); double value = args[0].v.real;

AVES_API NATIVE_FUNCTION(aves_math_abs)
{
	Aves *aves = Aves::Get(thread);

	TypeHandle type = args[0].type;

	if (type == aves->aves.UInt)
	{
		VM_Push(thread, args + 0);
	}
	else if (type == aves->aves.Real)
	{
		VM_PushReal(thread, abs(args[0].v.real));
	}
	else if (type == aves->aves.Int)
	{
		int64_t integer = args[0].v.integer;
		if (integer == INT64_MIN)
			return VM_ThrowOverflowError(thread);
		if (integer < 0)
			integer = -integer;
		VM_PushInt(thread, integer);
	}
	else
	{
		VM_PushString(thread, strings::n); // paramName
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentTypeError, 1);
	}
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
	double y = args[0].v.real;
	double x = args[1].v.real;

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
	Aves *aves = Aves::Get(thread);

	TypeHandle type = args[0].type;

	if (type == aves->aves.Int || type == aves->aves.UInt)
	{
		VM_Push(thread, args + 0);
	}
	else if (type == aves->aves.Real)
	{
		VM_PushReal(thread, ceil(args[0].v.real));
	}
	else
	{
		VM_PushString(thread, strings::n); // paramName
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentTypeError, 1);
	}
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
	Aves *aves = Aves::Get(thread);

	TypeHandle type = args[0].type;

	if (type == aves->aves.Int || type == aves->aves.UInt)
	{
		VM_Push(thread, args + 0);
	}
	else if (type == aves->aves.Real)
	{
		VM_PushReal(thread, floor(args[0].v.real));
	}
	else
	{
		VM_PushString(thread, strings::n); // paramName
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentTypeError, 1);
	}
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
	double base = args[1].v.real;

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
	Aves *aves = Aves::Get(thread);

	Value value = args[0];
	if (value.type == aves->aves.UInt)
	{
		VM_PushInt(thread, value.v.uinteger > 0); // If value.uinteger > 0, then push 1; otherwise, 0!
	}
	else if (value.type == aves->aves.Int)
	{
		int64_t intValue = value.v.integer;
		VM_PushInt(thread,
			intValue > 0 ?  1 :
			intValue < 0 ? -1 :
			0);
	}
	else if (value.type == aves->aves.Real)
	{
		double realValue = value.v.real;
		VM_PushInt(thread,
			realValue > 0 ?  1 :
			realValue < 0 ? -1 :
			0); // includes NaN
	}
	else
	{
		VM_PushString(thread, strings::n); // paramName
		return VM_ThrowErrorOfType(thread, aves->aves.ArgumentTypeError, 1);
	}
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
