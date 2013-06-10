#include "ov_vm.internal.h"

const Value NULL_VALUE = NULL_CONSTANT;

OVUM_API bool IsTrue(Value value)
{
	return value.type != nullptr &&
		(!(_Tp(value.type)->flags & TYPE_PRIMITIVE) ||
		value.integer != 0);
}

OVUM_API bool IsFalse(Value value)
{
	return value.type == nullptr ||
		(_Tp(value.type)->flags & TYPE_PRIMITIVE) && value.integer == 0;
}

OVUM_API bool IsType(Value value, TypeHandle type)
{
	const Type *valtype = _Tp(value.type);
	while (valtype)
	{
		if (valtype == type)
			return true;
		valtype = valtype->baseType;
	}
	return false;
}

OVUM_API bool IsSameReference(Value a, Value b)
{
	if (a.type != b.type)
		return false;
	// a.type == b.type at this point
	if (a.type == nullptr)
		return true; // both are null
	if (_Tp(a.type)->flags & TYPE_PRIMITIVE)
		return a.integer == b.integer;
	return a.instance == b.instance;
}


OVUM_API bool IsBoolean(Value value)
{
	return value.type == stdTypes.Boolean;
}

OVUM_API bool IsInt(Value value)
{
	return value.type == stdTypes.Int;
}

OVUM_API bool IsUInt(Value value)
{
	return value.type == stdTypes.UInt;
}

OVUM_API bool IsReal(Value value)
{
	return value.type == stdTypes.Real;
}

OVUM_API bool IsString(Value value)
{
	return value.type == stdTypes.String;
}