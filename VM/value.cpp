#include "ov_vm.internal.h"

const Value NULL_VALUE = NULL_CONSTANT;

OVUM_API bool IsTrue(Value value)
{
	return IsTrue_(value);
}

OVUM_API bool IsFalse(Value value)
{
	return IsFalse_(value);
}

OVUM_API bool IsType(Value value, TypeHandle type)
{
	return Type::ValueIsType(value, type);
}

OVUM_API bool IsSameReference(Value a, Value b)
{
	return IsSameReference_(a, b);
}


OVUM_API bool IsBoolean(Value value)
{
	return value.type == VM::vm->types.Boolean;
}

OVUM_API bool IsInt(Value value)
{
	return value.type == VM::vm->types.Int;
}

OVUM_API bool IsUInt(Value value)
{
	return value.type == VM::vm->types.UInt;
}

OVUM_API bool IsReal(Value value)
{
	return value.type == VM::vm->types.Real;
}

OVUM_API bool IsString(Value value)
{
	return value.type == VM::vm->types.String;
}