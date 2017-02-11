#include "field.h"
#include "../ee/thread.h"
#include "../object/type.h"
#include "../gc/gcobject.h"

namespace ovum
{

int Field::ReadField(Thread *const thread, Value *instance, Value *dest) const
{
	if (instance->type == nullptr)
		return thread->ThrowNullReferenceError();
	if (!Type::ValueIsType(instance, this->declType))
		return thread->ThrowTypeError();

	GCObject *gco = GCObject::FromInst(instance->v.instance);
	gco->fieldAccessLock.Enter();
	*dest = *reinterpret_cast<Value*>(instance->v.instance + this->offset);
	gco->fieldAccessLock.Leave();

	RETURN_SUCCESS;
}

int Field::ReadFieldFast(Thread *const thread, Value *instance, Value *dest) const
{
	if (instance->type == nullptr)
		return thread->ThrowNullReferenceError();

	GCObject *gco = GCObject::FromInst(instance->v.instance);
	gco->fieldAccessLock.Enter();
	*dest = *reinterpret_cast<Value*>(instance->v.instance + this->offset);
	gco->fieldAccessLock.Leave();

	RETURN_SUCCESS;
}

void Field::ReadFieldUnchecked(Value *instance, Value *dest) const
{
	GCObject *gco = GCObject::FromInst(instance->v.instance);
	gco->fieldAccessLock.Enter();
	*dest = *reinterpret_cast<Value*>(instance->v.instance + this->offset);
	gco->fieldAccessLock.Leave();
}

int Field::WriteField(Thread *const thread, Value *instanceAndValue) const
{
	if (instanceAndValue[0].type == nullptr)
		return thread->ThrowNullReferenceError();
	if (!Type::ValueIsType(instanceAndValue, this->declType))
		return thread->ThrowTypeError();

	GCObject *gco = GCObject::FromInst(instanceAndValue[0].v.instance);
	gco->fieldAccessLock.Enter();
	*reinterpret_cast<Value*>(instanceAndValue[0].v.instance + this->offset) = instanceAndValue[1];
	gco->fieldAccessLock.Leave();

	RETURN_SUCCESS;
}

int Field::WriteFieldFast(Thread *const thread, Value *instanceAndValue) const
{
	if (instanceAndValue[0].type == nullptr)
		return thread->ThrowNullReferenceError();

	GCObject *gco = GCObject::FromInst(instanceAndValue[0].v.instance);
	gco->fieldAccessLock.Enter();
	*reinterpret_cast<Value*>(instanceAndValue[0].v.instance + this->offset) = instanceAndValue[1];
	gco->fieldAccessLock.Leave();

	RETURN_SUCCESS;
}

void Field::WriteFieldUnchecked(Value *instanceAndValue) const
{
	GCObject *gco = GCObject::FromInst(instanceAndValue[0].v.instance);
	gco->fieldAccessLock.Enter();
	*reinterpret_cast<Value*>(instanceAndValue[0].v.instance + this->offset) = instanceAndValue[1];
	gco->fieldAccessLock.Leave();
}

} // namespace ovum

OVUM_API size_t Field_GetOffset(FieldHandle field)
{
	return field->offset;
}
