#pragma once

#ifndef VM__VALUE_INTERNAL_H
#define VM__VALUE_INTERNAL_H

#include "ov_vm.internal.h"
#include "ov_type.internal.h"


#define VALUE_FIELDS(v,t)   reinterpret_cast<::Value*>((v).instance  + _Tp(t)->fieldsOffset)
#define VALUE_FIELDS_P(v,t) reinterpret_cast<::Value*>((v)->instance + _Tp(t)->fieldsOffset)

// These access stdTypes directly, instead of calling the various GetType_* methods.

inline void SetNull_(Value &target)
{
	target.type = nullptr;
	target.instance = nullptr;
}
inline void SetNull_(Value *target)
{
	target->type = nullptr;
	target->instance = nullptr;
}

inline void SetBool_(Value &target, const bool value)
{
	target.type = stdTypes.Boolean;
	target.integer = value;
}
inline void SetBool_(Value *target, const bool value)
{
	target->type = stdTypes.Boolean;
	target->integer = value;
}

inline void SetInt_(Value &target, const int64_t value)
{
	target.type = stdTypes.Int;
	target.integer = value;
}
inline void SetInt_(Value *target, const int64_t value)
{
	target->type = stdTypes.Int;
	target->integer = value;
}

inline void SetUInt_(Value &target, const uint64_t value)
{
	target.type = stdTypes.UInt;
	target.uinteger = value;
}
inline void SetUInt_(Value *target, const uint64_t value)
{
	target->type = stdTypes.UInt;
	target->uinteger = value;
}

inline void SetReal_(Value &target, const double value)
{
	target.type = stdTypes.Real;
	target.real = value;
}
inline void SetReal_(Value *target, const double value)
{
	target->type = stdTypes.Real;
	target->real = value;
}

inline void SetString_(Value &target, String *value)
{
	target.type = stdTypes.String;
	target.common.string = value;
}
inline void SetString_(Value *target, String *value)
{
	target->type = stdTypes.String;
	target->common.string = value;
}


// Similarly, these actually access the Type class directly.

inline bool IsTrue_(Value value)
{
	return value.type != nullptr &&
		(!(value.type->flags & TYPE_PRIMITIVE) ||
		value.integer != 0);
}

inline bool IsFalse_(Value value)
{
	return value.type == nullptr ||
		(value.type->flags & TYPE_PRIMITIVE) && value.integer == 0;
}

inline bool IsSameReference_(Value a, Value b)
{
	if (a.type != b.type)
		return false;
	// a.type == b.type at this point
	if (a.type == nullptr)
		return true; // both are null
	if (a.type->flags & TYPE_PRIMITIVE)
		return a.integer == b.integer;
	return a.instance == b.instance;
}

#endif // VM__VALUE_INTERNAL_H