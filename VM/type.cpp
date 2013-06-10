#include "ov_vm.internal.h"
#include "ov_string.h"

namespace std_type_names
{
	// Fully qualified names of core types. If you change the fully
	// qualified names of these types and neglect to update this list,
	// do not be surprised if the VM crashes in your face!

	// This macro is a horrible, evil thing. But it works.
	#define AVES	'a','v','e','s','.'

	LitString<11> Object    = { 11, 0, STR_STATIC, AVES,'O','b','j','e','c','t',0 };
	LitString<12> Boolean   = { 12, 0, STR_STATIC, AVES,'B','o','o','l','e','a','n',0 };
	LitString<8>  Int       = {  8, 0, STR_STATIC, AVES,'I','n','t',0 };
	LitString<9>  UInt      = {  9, 0, STR_STATIC, AVES,'U','I','n','t',0 };
	LitString<9>  Real      = {  9, 0, STR_STATIC, AVES,'R','e','a','l',0 };
	LitString<11> String    = { 11, 0, STR_STATIC, AVES,'S','t','r','i','n','g',0 };
	LitString<9>  Enum      = {  9, 0, STR_STATIC, AVES,'E','n','u','m',0 };
	LitString<9>  List      = {  9, 0, STR_STATIC, AVES,'L','i','s','t',0 };
	LitString<9>  Hash      = {  9, 0, STR_STATIC, AVES,'H','a','s','h',0 };
	LitString<11> Method    = { 11, 0, STR_STATIC, AVES,'M','e','t','h','o','d',0 };
	LitString<13> Iterator  = { 13, 0, STR_STATIC, AVES,'I','t','e','r','a','t','o','r',0 };
	LitString<9>  Type      = {  9, 0, STR_STATIC, AVES,'T','y','p','e',0 };
	LitString<10> Error     = { 10, 0, STR_STATIC, AVES,'E','r','r','o','r',0 };
	LitString<14> TypeError = { 14, 0, STR_STATIC, AVES,'T','y','p','e','E','r','r','o','r',0 };
	LitString<16> MemoryError	     = { 16, 0, STR_STATIC, AVES,'M','e','m','o','r','y','E','r','r','o','r',0 };
	LitString<18> OverflowError	     = { 18, 0, STR_STATIC, AVES,'O','v','e','r','f','l','o','w','E','r','r','o','r',0 };
	LitString<23> NullReferenceError = { 23, 0, STR_STATIC, AVES,'N','u','l','l','R','e','f','e','r','e','n','c','e','E','r','r','o','r',0 };
}

Type::Type(int32_t memberCount) :
	members(memberCount)
{
	// chirp
}

void Type::InitOperators()
{
	for (int op = 0; op < OPERATOR_COUNT; op++) {
		if (this->operators[op])
			continue;

		const Type *type = this;

		Method *method;
		do {
			method = type->operators[op];
		} while (!method && (type = type->baseType));

		this->operators[op] = method; // null or an actual method.
	}

	this->flags = _E(TypeFlags, this->flags | TYPE_OPS_INITED);
}

Member *Type::GetMember(String *name) const
{
	Member *m;
	if (members.Get(name, m))
		return m;
	return nullptr;
}

Member *Type::FindMember(String *name, const Type *fromType) const
{
	const Type *type = this;
	do {
		Member *m;
		if (members.Get(name, m) && m->IsAccessible(this, type, fromType))
			return m;
	} while (type = type->baseType);

	return nullptr; // not found
}

Method *Type::GetOperator(Operator op) const
{
	if (!(this->flags & TYPE_OPS_INITED))
		// I'm probably a bad person for doing this. I don't care. <3
		const_cast<Type*>(this)->InitOperators();

	return this->operators[op];
}

Value Type::GetTypeToken() const
{
	if (IS_NULL(typeToken))
		const_cast<Type*>(this)->LoadTypeToken();

	return typeToken;
}

void Type::LoadTypeToken()
{
	throw L"Not implemented";
}

// Determines whether a member is accessible from a given type.
//   instType:
//     The type of the instance that the member belongs to.
//   declType:
//     The type that actually declares the member.
//   fromType:
//     The type which declares the method that is accessing the member.
//     This is null for global functions.
const bool Member::IsAccessible(const Type *instType, const Type *const declType, const Type *const fromType) const
{
	if (this->flags & M_PRIVATE)
		return fromType && (declType == fromType || declType == fromType->sharedType);

	if (this->flags & M_PROTECTED)
	{
		if (!fromType)
			return false;

		// WARNING: THIS ALGORITHM IS BROKEN
		// TODO: Member::IsAccessible

		const Type *tempType = instType;
		while (tempType && tempType != fromType)
			tempType = tempType->baseType;

		if (!tempType)
		{
			const Type *sharedType = fromType->sharedType;
			while (instType && instType != sharedType)
				instType = instType->baseType;

			if (!instType)
				return false; // instType does not inherit from fromType or fromType->sharedType
		}

		tempType = fromType;
		while (tempType && tempType != declType)
			tempType = tempType->baseType;

		if (!tempType)
		{
			const Type *sharedType = fromType->sharedType;
			while (sharedType && sharedType != declType)
				sharedType = sharedType->baseType;

			if (!sharedType)
				return false; // neither fromType nor fromType->sharedType inherits from declType
		}
	}

	return true; // M_PUBLIC or accessible
}

Value *const Field::GetField(Thread *const thread, const Value instance) const
{
	if (IS_NULL(instance))
		thread->ThrowNullReferenceError();
	if (!Type::ValueIsType(instance, this->declType))
		thread->ThrowTypeError();
	return reinterpret_cast<Value*>(instance.instance + this->offset);
}

Value *const Field::GetField(Thread *const thread, const Value *instance) const
{
	if (instance->type == nullptr)
		thread->ThrowNullReferenceError();
	if (!Type::ValueIsType(*instance, this->declType))
		thread->ThrowTypeError();
	return reinterpret_cast<Value*>(instance->instance + this->offset);
}

Value *const Field::GetFieldFast(Thread *const thread, const Value instance) const
{
	if (IS_NULL(instance))
		thread->ThrowNullReferenceError();
	return reinterpret_cast<Value*>(instance.instance + this->offset);
}

Value *const Field::GetFieldFast(Thread *const thread, const Value *instance) const
{
	if (instance->type == nullptr)
		thread->ThrowNullReferenceError();
	return reinterpret_cast<Value*>(instance->instance + this->offset);
}

OVUM_API const StandardTypes &GetStandardTypes() { return stdTypes; }
OVUM_API TypeHandle GetType_Object()             { return stdTypes.Object; }
OVUM_API TypeHandle GetType_Boolean()            { return stdTypes.Boolean; }
OVUM_API TypeHandle GetType_Int()                { return stdTypes.Int; }
OVUM_API TypeHandle GetType_UInt()               { return stdTypes.UInt; }
OVUM_API TypeHandle GetType_Real()               { return stdTypes.Real; }
OVUM_API TypeHandle GetType_String()             { return stdTypes.String; }
OVUM_API TypeHandle GetType_Enum()               { return stdTypes.Enum; }
OVUM_API TypeHandle GetType_List()               { return stdTypes.List; }
OVUM_API TypeHandle GetType_Hash()               { return stdTypes.Hash; }
OVUM_API TypeHandle GetType_Method()             { return stdTypes.Method; }
OVUM_API TypeHandle GetType_Iterator()           { return stdTypes.Iterator; }
OVUM_API TypeHandle GetType_Type()               { return stdTypes.Type; }
OVUM_API TypeHandle GetType_Error()              { return stdTypes.Error; }
OVUM_API TypeHandle GetType_TypeError()          { return stdTypes.TypeError; }
OVUM_API TypeHandle GetType_MemoryError()        { return stdTypes.MemoryError; }
OVUM_API TypeHandle GetType_OverflowError()      { return stdTypes.OverflowError; }
OVUM_API TypeHandle GetType_DivideByZeroError()  { return stdTypes.DivideByZeroError; }
OVUM_API TypeHandle GetType_NullReferenceError() { return stdTypes.NullReferenceError; }


OVUM_API bool Member_IsAccessible(const MemberHandle member, TypeHandle instType, TypeHandle declType, TypeHandle fromType)
{
	return ((Member*)member)->IsAccessible(_Tp(instType), _Tp(declType), _Tp(fromType));
}


OVUM_API TypeFlags Type_GetFlags(TypeHandle type)
{
	return _Tp(type)->flags;
}

OVUM_API String *Type_GetFullName(TypeHandle type)
{
	return _Tp(type)->fullName;
}

OVUM_API MemberHandle Type_GetMember(TypeHandle type, String *name)
{
	return _Tp(type)->GetMember(name);
}

OVUM_API MemberHandle Type_FindMember(TypeHandle type, String *name, TypeHandle fromType)
{
	return _Tp(type)->FindMember(name, _Tp(fromType));
}

OVUM_API MethodHandle Type_GetOperator(TypeHandle type, Operator op)
{
	return _Tp(type)->GetOperator(op);
}

OVUM_API Value Type_GetTypeToken(TypeHandle type)
{
	return _Tp(type)->GetTypeToken();
}

OVUM_API uint32_t Type_GetFieldOffset(TypeHandle type)
{
	return _Tp(type)->fieldsOffset;
}

OVUM_API void Type_SetFinalizer(TypeHandle type, Finalizer finalizer)
{
	Type *realType = const_cast<Type*>(_Tp(type));
	if ((realType->flags & TYPE_INITED) == TYPE_NONE)
		realType->finalizer = finalizer;
}

OVUM_API void Type_SetInstanceSize(TypeHandle type, uint32_t size)
{
	Type *realType = const_cast<Type*>(_Tp(type));
	if ((realType->flags & TYPE_INITED) == TYPE_NONE)
	{
		realType->size = size;
		realType->flags = _E(TypeFlags, realType->flags | TYPE_CUSTOMPTR);
	}
}

OVUM_API void Type_SetReferenceGetter(TypeHandle type, ReferenceGetter getter)
{
	Type *realType = const_cast<Type*>(_Tp(type));
	if ((realType->flags & TYPE_INITED) == TYPE_NONE)
		realType->getReferences = getter;
}

OVUM_API String *Error_GetMessage(Value error)
{
	if (!Type::ValueIsType(error, _Tp(stdTypes.Error)))
		return nullptr; // whoopsies!

	return error.common.error->message;
}

OVUM_API String *Error_GetStackTrace(Value error)
{
	if (!Type::ValueIsType(error, _Tp(stdTypes.Error)))
		return nullptr; // Type mismatch!

	return error.common.error->stackTrace;
}