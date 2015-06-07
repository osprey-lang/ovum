#include "vm.h"

const Value NULL_VALUE = NULL_CONSTANT;

OVUM_API bool IsTrue(Value *value)
{
	return ovum::IsTrue_(value);
}

OVUM_API bool IsFalse(Value *value)
{
	return ovum::IsFalse_(value);
}

OVUM_API bool IsType(Value *value, TypeHandle type)
{
	return ovum::Type::ValueIsType(value, type);
}

OVUM_API bool IsSameReference(Value *a, Value *b)
{
	return ovum::IsSameReference_(a, b);
}


OVUM_API bool IsBoolean(ThreadHandle thread, Value *value)
{
	return value->type == thread->GetVM()->types.Boolean;
}

OVUM_API bool IsInt(ThreadHandle thread, Value *value)
{
	return value->type == thread->GetVM()->types.Int;
}

OVUM_API bool IsUInt(ThreadHandle thread, Value *value)
{
	return value->type == thread->GetVM()->types.UInt;
}

OVUM_API bool IsReal(ThreadHandle thread, Value *value)
{
	return value->type == thread->GetVM()->types.Real;
}

OVUM_API bool IsString(ThreadHandle thread, Value *value)
{
	return value->type == thread->GetVM()->types.String;
}

OVUM_API void ReadReference(Value *ref, Value *target)
{
	using namespace ovum;

	if ((uintptr_t)ref->type == LOCAL_REFERENCE)
		*target = *reinterpret_cast<Value*>(ref->v.reference);
	else if ((uintptr_t)ref->type == STATIC_REFERENCE)
		reinterpret_cast<StaticRef*>(ref->v.reference)->Read(target);
	else
	{
		uintptr_t offset = ~(uintptr_t)ref->type;
		GCObject *gco = reinterpret_cast<GCObject*>((char*)ref->v.reference - offset);
		gco->fieldAccessLock.Enter();
		*target = *reinterpret_cast<Value*>(ref->v.reference);
		gco->fieldAccessLock.Leave();
	}
}

OVUM_API void WriteReference(Value *ref, Value *value)
{
	using namespace ovum;

	if ((uintptr_t)ref->type == LOCAL_REFERENCE)
		*reinterpret_cast<Value*>(ref->v.reference) = *value;
	else if ((uintptr_t)ref->type == STATIC_REFERENCE)
		reinterpret_cast<StaticRef*>(ref->v.reference)->Write(value);
	else
	{
		uintptr_t offset = ~(uintptr_t)ref->type;
		GCObject *gco = reinterpret_cast<GCObject*>((char*)ref->v.reference - offset);
		gco->fieldAccessLock.Enter();
		*reinterpret_cast<Value*>(ref->v.reference) = *value;
		gco->fieldAccessLock.Leave();
	}
}