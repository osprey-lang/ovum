#pragma once

#ifndef VM__TYPE_INTERNAL_H
#define VM__TYPE_INTERNAL_H

#include "ov_vm.internal.h"
#include "ov_module.internal.h"

// forward declarations
class Module;
class Member;
class Method;

// Types, once initialized, are supposed to be (more or less) immutable.
// If you assign to any of the members in a Type, you have no one to blame but yourself.
// That said, the VM occasionally updates the flags.
class Type
{
public:
	Type(int32_t memberCount);

	Member *GetMember(String *name) const;
	Member *FindMember(String *name, const Type *fromType) const;
	Method *GetOperator(Operator op) const; // haha, 'const'. Liar. Potentially. Sometimes.

	// Flags associated with the type.
	TypeFlags flags;

	// The type from which this inherits (null only for Object).
	Type *baseType;
	// A type whose private and protected members this type has access to.
	// The shared type must be in the same module as this type.
	Type *sharedType;

	// The fully qualified name of the type, e.g. "aves.Object".
	String *fullName;

	// The offset (in bytes) of the first field in instances of this type.
	uint32_t fieldsOffset;
	// The total size (in bytes) of instances of this type.
	// Note: this is 0 for Object, and String is variable-size.
	size_t size;
	// The total number of instance Value fields in the type.
	int fieldCount;

	// Members! These allow us to look up members by name.
	StringHash<Member*> members;

	// Operator implementations. If an operator implementation is null,
	// then the type does not implement that operator.
	Method *operators[OPERATOR_COUNT];

	// The reference getter for the type. Is null unless the type has
	// the flag TYPE_CUSTOMPTR, in which case the GC uses this method
	// to obtain a list of Value references from instance of the type.
	ReferenceGetter getReferences;
	// The finalizer for the type. Only available to native-code types.
	Finalizer finalizer;

	// A handle to the module that declares the type.
	Module *module;

	// An instance of aves.Type that is bound to this type.
	// Use GetTypeToken() to retrieve this value; this starts
	// out as a NULL_VALUE and is only initialized on demand.
	Value typeToken;

	Value GetTypeToken() const;

	static inline const bool ValueIsType(Value value, const Type *const type)
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

private:
	void InitOperators();

	void LoadTypeToken();
};


TYPED_ENUM(MemberFlags, uint16_t)
{
	// The member has no flags.
	M_NONE      = 0x0000,
	// The member is a field.
	M_FIELD     = 0x0001,
	// The member is a method.
	M_METHOD    = 0x0002,
	// The member is a property.
	M_PROPERTY  = 0x0004,

	// The member is public.
	M_PUBLIC    = 0x0008,
	// The member is protected.
	M_PROTECTED = 0x0010,
	// The member is private.
	M_PRIVATE   = 0x0020,

	// The member is abstract.
	M_ABSTRACT  = 0x0080,
	// The member is virtual (overridable in Osprey).
	M_VIRTUAL   = 0x0100,
	// The member is sealed (no direct equivalent in Osprey).
	M_SEALED    = 0x0200,

	// The member is an instance member.
	M_INSTANCE  = 0x0400,

	// The member is used internally to implement some behaviour.
	// Primarily used by getters, setters, iterator accessors and
	// operator overloads.
	M_IMPL      = 0x0800,

	// A mask for extracting the access level of a member.
	M_ACCESS_LEVEL = M_PUBLIC | M_PROTECTED | M_PRIVATE,
	// A mask for extracting the kind of a member.
	M_KIND = M_FIELD | M_METHOD | M_PROPERTY,

	M_PUBLIC_INSTANCE    = M_PUBLIC    | M_INSTANCE,
	M_PROTECTED_INSTANCE = M_PROTECTED | M_INSTANCE,
	M_PRIVATE_INSTANCE   = M_PRIVATE   | M_INSTANCE,
};

class Member
{
public:
	MemberFlags flags;

	String *const name;
	Member *next;
	Type *const declType;
	Module *const declModule;

	inline Member(String *name, Type *declType, MemberFlags flags)
		: name(name), declType(declType), declModule(declType->module),
		next(nullptr), flags(flags)
	{ }

	const bool IsAccessible(const Type *instType, const Type *declType, const Type *fromType) const;
};

class Field : public Member
{
public:
	union
	{
		int32_t offset;
		Value *staticValue;
	};

	Value *const GetField(Thread *const thread, const Value instance) const;
	Value *const GetField(Thread *const thread, const Value *instance) const;

	Value *const GetFieldFast(Thread *const thread, const Value instance) const;
	Value *const GetFieldFast(Thread *const thread, const Value *instance) const;

	inline Value *const GetFieldUnchecked(Thread *const thread, const Value instance) const
	{
		return reinterpret_cast<Value*>(instance.instance + this->offset);
	}
	inline Value *const GetFieldUnchecked(Thread *const thread, const Value *instance) const
	{
		return reinterpret_cast<Value*>(instance->instance + this->offset);
	}
};
// Recovers a Field* from a FieldHandle
#define _Fld(fh)   reinterpret_cast<Field*>(fh)


TYPED_ENUM(MethodFlags, uint8_t)
{
	// No method flags.
	METHOD_NONE      = 0x00,
	// The method has a variadic parameter at the end.
	METHOD_VAR_END   = 0x01,
	// The method has a variadic parameter at the start.
	METHOD_VAR_START = 0x02,

	// The method has a native-code implementation.
	METHOD_NATIVE    = 0x04,

	// The method is an instance method. Without this flag,
	// methods are static.
	METHOD_INSTANCE  = 0x08,

	// The method is a constructor.
	METHOD_CTOR      = 0x10,

	// The method has been initialized. Used for bytecode methods only,
	// to indicate that the bytecode initializer has processed the method.
	METHOD_INITED    = 0x20,

	// A mask for extracting the variadic flags of a method.
	METHOD_VARIADIC = METHOD_VAR_END | METHOD_VAR_START,
};

class Method : public Member
{
public:
	typedef struct
	{
		// The number of parameters the method has, EXCLUDING the instance
		// if it is an instance method.
		uint16_t paramCount;
		// The number of optional parameters the method has.
		uint16_t optionalParamCount;
		// The number of local variables the method uses.
		uint16_t locals;
		// The maximum stack size to reserve for the method. If this requirement
		// cannot be met, a stack overflow condition occurs.
		uint16_t maxStack;
		// Flags associated with the method.
		MethodFlags flags;

		union
		{
			struct
			{
				uint8_t *entry;
				uint32_t length; // The length of the method body, in bytes.
			};
			NativeMethod nativeEntry;
		};

		// The group to which the overload belongs
		Method *group;
		// The type that declares the overload
		Type *declType;

		inline const bool Accepts(const uint16_t argc) const
		{
			if (flags & METHOD_VARIADIC)
				return argc >= paramCount - 1;
			else
				return argc >= paramCount - optionalParamCount &&
					argc <= paramCount;
		}
	} Overload;

	// The number of overloads in the method.
	int32_t overloadCount;
	// The overloads of the method.
	Overload **overloads;
	// If this method is not a global function and the base type declares
	// a method with the same name as this one, then this pointer refers
	// to that method, plus some rules about accessibility.
	Method *baseMethod;

	inline const bool Accepts(const uint16_t argc) const
	{
		const Method *m = this;
		do
		{
			for (int i = 0; i < m->overloadCount; i++)
				if (m->overloads[i]->Accepts(argc))
					return true;
		} while (m = m->baseMethod);
		return false;
	}
};
// Recovers a Method* from a MethodHandle.
#define _Mth(mh)    reinterpret_cast<Method*>(mh)


class Property : public Member
{
public:
	Method *getter;
	Method *setter;
};
// Recovers a Property* from a PropertyHandle
#define _Prop(ph)   reinterpret_cast<Property*>(ph)

#endif