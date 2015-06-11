#include "gcobject.h"
#include "../object/type.h"

namespace ovum
{

uint8_t *GCObject::InstanceBase()
{
	return (uint8_t*)this + GCO_SIZE;
}

uint8_t *GCObject::InstanceBase(Type *type)
{
	return (uint8_t*)this + GCO_SIZE + type->fieldsOffset;
}

Value *GCObject::FieldsBase()
{
	return (Value*)((char*)this + GCO_SIZE);
}

Value *GCObject::FieldsBase(Type *type)
{
	return (Value*)((char*)this + GCO_SIZE + type->fieldsOffset);
}

GCObject *GCObject::FromInst(void *inst)
{
	return reinterpret_cast<GCObject*>((char*)inst - GCO_SIZE);
}

GCObject *GCObject::FromValue(Value *value)
{
	return FromInst(value->v.instance);
}

}