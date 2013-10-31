#include <cmath>
#include <cstdio>
#include "aves_real.h"
#include "ov_stringbuffer.h"

#define LEFT  (args[0])
#define RIGHT (args[1])

AVES_API NATIVE_FUNCTION(aves_real)
{
	Value result = RealFromValue(thread, args[0]);
	VM_Push(thread, result);
}

AVES_API NATIVE_FUNCTION(aves_Real_get_isInfinite)
{
	VM_PushBool(thread, !IsFinite(THISV.real));
}

AVES_API NATIVE_FUNCTION(aves_Real_getHashCode)
{
	// Value.integer overlaps with Value.real, and they're both 64-bits,
	// so this is perfectly fine!
	VM_PushInt(thread, args[1].integer);
}
AVES_API NATIVE_FUNCTION(aves_Real_toString)
{
	char result[128]; // hope this is big enough!
	int count = sprintf_s(result, 128, "%g", THISV.real); // I hate you, C++ <3
	StringBuffer buf(thread, count);
	buf.Append(thread, count, result);

	String *output = buf.ToString(thread);
	VM_PushString(thread, output);
}

AVES_API NATIVE_FUNCTION(aves_Real_opEquals)
{
	double right = RealFromValue(thread, RIGHT).real;
	double left = LEFT.real;

	if (IsNaN(left) || IsNaN(right))
		VM_PushBool(thread, IsNaN(left) == IsNaN(right));
	else
		VM_PushBool(thread, left == right);
}
AVES_API NATIVE_FUNCTION(aves_Real_opCompare)
{
	if (RIGHT.type != Types::Real)
		VM_ThrowTypeError(thread);

	double left = LEFT.real;
	double right = RIGHT.real;

	// Real values are ordered as follows:
	//   NaN < -∞ < ... < -ε < -0.0 = +0.0 < +ε < ... < +∞
	// Notice that NaN is ordered before all other values.

	if (IsNaN(left))
	{
		VM_PushInt(thread, IsNaN(right) ? 0 : -1);
		return;
	}

	int result = left < right ? -1 :
		left > right ? 1 :
		0;
	VM_PushInt(thread, result);
}
AVES_API NATIVE_FUNCTION(aves_Real_opAdd)
{
	double right = RealFromValue(thread, RIGHT).real;
	VM_PushReal(thread, LEFT.real + right);
}
AVES_API NATIVE_FUNCTION(aves_Real_opSubtract)
{
	double right = RealFromValue(thread, RIGHT).real;
	VM_PushReal(thread, LEFT.real - right);
}
AVES_API NATIVE_FUNCTION(aves_Real_opMultiply)
{
	double right = RealFromValue(thread, RIGHT).real;
	VM_PushReal(thread, LEFT.real * right);
}
AVES_API NATIVE_FUNCTION(aves_Real_opDivide)
{
	double right = RealFromValue(thread, RIGHT).real;
	VM_PushReal(thread, LEFT.real / right);
}
AVES_API NATIVE_FUNCTION(aves_Real_opModulo)
{
	double right = RealFromValue(thread, RIGHT).real;
	VM_PushReal(thread, fmod(LEFT.real, right));
}
AVES_API NATIVE_FUNCTION(aves_Real_opPower)
{
	double right = RealFromValue(thread, RIGHT).real;
	VM_PushReal(thread, pow(LEFT.real, right));
}
AVES_API NATIVE_FUNCTION(aves_Real_opPlus)
{
	VM_Push(thread, LEFT);
}
AVES_API NATIVE_FUNCTION(aves_Real_opNegate)
{
	VM_PushReal(thread, -args[0].real);
}