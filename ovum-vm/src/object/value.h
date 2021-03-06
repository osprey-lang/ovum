#pragma once

#include "../vm.h"
#include "../object/type.h"

namespace ovum
{

static const uintptr_t LOCAL_REFERENCE = (uintptr_t)-1;
static const uintptr_t STATIC_REFERENCE = (uintptr_t)-3;

// These access VM::types directly, instead of calling the various GetType_* methods.

inline void SetNull_(Value &target)
{
	target.type = nullptr;
}
inline void SetNull_(Value *target)
{
	target->type = nullptr;
}

inline void SetBool_(VM *vm, Value &target, bool value)
{
	target.type = vm->types.Boolean;
	target.v.integer = value;
}
inline void SetBool_(VM *vm, Value *target, bool value)
{
	target->type = vm->types.Boolean;
	target->v.integer = value;
}

inline void SetInt_(VM *vm, Value &target, int64_t value)
{
	target.type = vm->types.Int;
	target.v.integer = value;
}
inline void SetInt_(VM *vm, Value *target, int64_t value)
{
	target->type = vm->types.Int;
	target->v.integer = value;
}

inline void SetUInt_(VM *vm, Value &target, uint64_t value)
{
	target.type = vm->types.UInt;
	target.v.uinteger = value;
}
inline void SetUInt_(VM *vm, Value *target, uint64_t value)
{
	target->type = vm->types.UInt;
	target->v.uinteger = value;
}

inline void SetReal_(VM *vm, Value &target, double value)
{
	target.type = vm->types.Real;
	target.v.real = value;
}
inline void SetReal_(VM *vm, Value *target, double value)
{
	target->type = vm->types.Real;
	target->v.real = value;
}

inline void SetString_(VM *vm, Value &target, String *value)
{
	target.type = vm->types.String;
	target.v.string = value;
}
inline void SetString_(VM *vm, Value *target, String *value)
{
	target->type = vm->types.String;
	target->v.string = value;
}

// Similarly, these actually access the Type class directly.

inline bool IsTrue_(Value *value)
{
	Type *type = value->type;
	if (type == nullptr)
		return false;
	return type->specialType != SpecialTypeId::BOOLEAN ||
		value->v.integer != 0;
}

inline bool IsFalse_(Value *value)
{
	Type *type = value->type;
	if (type == nullptr)
		return true;
	return type->specialType == SpecialTypeId::BOOLEAN &&
		value->v.integer == 0;
}

inline bool IsSameReference_(Value a, Value b)
{
	if (a.type != b.type)
		return false;
	// a.type == b.type at this point
	if (a.type == nullptr)
		return true; // both are null
	if (a.type->IsPrimitive())
		return a.v.integer == b.v.integer;
	return a.v.instance == b.v.instance;
}
inline bool IsSameReference_(Value *a, Value *b)
{
	if (a->type != b->type)
		return false;
	// a->type == b->type at this point
	if (a->type == nullptr)
		return true;
	if (a->type->IsPrimitive())
		return a->v.integer == b->v.integer;
	return a->v.instance == b->v.instance;
}

} // namespace ovum
