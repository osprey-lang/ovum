#include "ov_vm.internal.h"

const Value NULL_VALUE = NULL_CONSTANT;

OVUM_API bool IsTrue(Value *value)
{
	return IsTrue_(value);
}

OVUM_API bool IsFalse(Value *value)
{
	return IsFalse_(value);
}

OVUM_API bool IsType(Value *value, TypeHandle type)
{
	return Type::ValueIsType(value, type);
}

OVUM_API bool IsSameReference(Value *a, Value *b)
{
	return IsSameReference_(a, b);
}


OVUM_API bool IsBoolean(Value *value)
{
	return value->type == VM::vm->types.Boolean;
}

OVUM_API bool IsInt(Value *value)
{
	return value->type == VM::vm->types.Int;
}

OVUM_API bool IsUInt(Value *value)
{
	return value->type == VM::vm->types.UInt;
}

OVUM_API bool IsReal(Value *value)
{
	return value->type == VM::vm->types.Real;
}

OVUM_API bool IsString(Value *value)
{
	return value->type == VM::vm->types.String;
}

OVUM_API void ReadReference(Value *ref, Value *target)
{
	if ((uintptr_t)ref->type == LOCAL_REFERENCE)
		*target = *reinterpret_cast<Value*>(ref->reference);
	else if ((uintptr_t)ref->type == STATIC_REFERENCE)
		reinterpret_cast<StaticRef*>(ref->reference)->Read(target);
	else
	{
		uintptr_t offset = ~(uintptr_t)ref->type;
		GCObject *gco = reinterpret_cast<GCObject*>((char*)ref->reference - offset);
		while (gco->fieldAccessFlag.test_and_set(std::memory_order_acquire))
			;
		*target = *reinterpret_cast<Value*>(ref->reference);
		gco->fieldAccessFlag.clear(std::memory_order_release);
	}
}

OVUM_API void WriteReference(Value *ref, Value *value)
{
	if ((uintptr_t)ref->type == LOCAL_REFERENCE)
		*reinterpret_cast<Value*>(ref->reference) = *value;
	else if ((uintptr_t)ref->type == STATIC_REFERENCE)
		reinterpret_cast<StaticRef*>(ref->reference)->Write(value);
	else
	{
		uintptr_t offset = ~(uintptr_t)ref->type;
		GCObject *gco = reinterpret_cast<GCObject*>((char*)ref->reference - offset);
		while (gco->fieldAccessFlag.test_and_set(std::memory_order_acquire))
			;
		*reinterpret_cast<Value*>(ref->reference) = *value;
		gco->fieldAccessFlag.clear(std::memory_order_release);
	}
}