#include "ov_vm.internal.h"
#include "ov_module.internal.h"
#include "../inc/ov_string.h"
#include "refsignature.internal.h"

namespace ovum
{

namespace std_type_names
{
	// Fully qualified names of core types. If you change the fully
	// qualified names of these types and neglect to update this list,
	// do not be surprised if the VM crashes in your face!

	// This macro is a horrible, evil thing. But it works.
	#define AVES	'a','v','e','s','.'
	#define SFS     ::StringFlags::STATIC

	LitString<11> _Object              = { 11, 0, SFS, AVES,'O','b','j','e','c','t',0 };
	LitString<12> _Boolean             = { 12, 0, SFS, AVES,'B','o','o','l','e','a','n',0 };
	LitString<8>  _Int                 = {  8, 0, SFS, AVES,'I','n','t',0 };
	LitString<9>  _UInt                = {  9, 0, SFS, AVES,'U','I','n','t',0 };
	LitString<9>  _Real                = {  9, 0, SFS, AVES,'R','e','a','l',0 };
	LitString<11> _String              = { 11, 0, SFS, AVES,'S','t','r','i','n','g',0 };
	LitString<9>  _Enum                = {  9, 0, SFS, AVES,'E','n','u','m',0 };
	LitString<9>  _List                = {  9, 0, SFS, AVES,'L','i','s','t',0 };
	LitString<9>  _Hash                = {  9, 0, SFS, AVES,'H','a','s','h',0 };
	LitString<11> _Method              = { 11, 0, SFS, AVES,'M','e','t','h','o','d',0 };
	LitString<13> _Iterator            = { 13, 0, SFS, AVES,'I','t','e','r','a','t','o','r',0 };
	LitString<20> _Type                = { 20, 0, SFS, AVES,'r','e','f','l','e','c','t','i','o','n','.','T','y','p','e',0 };
	LitString<10> _Error               = { 10, 0, SFS, AVES,'E','r','r','o','r',0 };
	LitString<14> _TypeError           = { 14, 0, SFS, AVES,'T','y','p','e','E','r','r','o','r',0 };
	LitString<16> _MemoryError         = { 16, 0, SFS, AVES,'M','e','m','o','r','y','E','r','r','o','r',0 };
	LitString<18> _OverflowError       = { 18, 0, SFS, AVES,'O','v','e','r','f','l','o','w','E','r','r','o','r',0 };
	LitString<20> _NoOverloadError     = { 20, 0, SFS, AVES,'N','o','O','v','e','r','l','o','a','d','E','r','r','o','r',0 };
	LitString<22> _DivideByZeroError   = { 22, 0, SFS, AVES,'D','i','v','i','d','e','B','y','Z','e','r','o','E','r','r','o','r',0 };
	LitString<23> _NullReferenceError  = { 23, 0, SFS, AVES,'N','u','l','l','R','e','f','e','r','e','n','c','e','E','r','r','o','r',0 };
	LitString<24> _MemberNotFoundError = { 24, 0, SFS, AVES,'M','e','m','b','e','r','N','o','t','F','o','u','n','d','E','r','r','o','r',0 };

	const unsigned int StandardTypeCount = 19;
	const StdType Types[] = {
		{_S(_Object),	           &StandardTypes::Object,              nullptr},
		{_S(_Boolean),	           &StandardTypes::Boolean,             nullptr},
		{_S(_Int),		           &StandardTypes::Int,                 nullptr},
		{_S(_UInt),		           &StandardTypes::UInt,                nullptr},
		{_S(_Real),		           &StandardTypes::Real,                nullptr},
		{_S(_String),	           &StandardTypes::String,              nullptr},
		{_S(_List),		           &StandardTypes::List,                "InitListInstance"},
		{_S(_Hash),		           &StandardTypes::Hash,                "InitHashInstance"},
		{_S(_Method),	           &StandardTypes::Method,              nullptr},
		{_S(_Iterator),	           &StandardTypes::Iterator,            nullptr},
		{_S(_Type),		           &StandardTypes::Type,                "InitTypeToken"},
		{_S(_Error),	           &StandardTypes::Error,               nullptr},
		{_S(_TypeError),           &StandardTypes::TypeError,           nullptr},
		{_S(_MemoryError),		   &StandardTypes::MemoryError,         nullptr},
		{_S(_OverflowError),	   &StandardTypes::OverflowError,       nullptr},
		{_S(_NoOverloadError),     &StandardTypes::NoOverloadError,     nullptr},
		{_S(_DivideByZeroError),   &StandardTypes::DivideByZeroError,   nullptr},
		{_S(_NullReferenceError),  &StandardTypes::NullReferenceError,  nullptr},
		{_S(_MemberNotFoundError), &StandardTypes::MemberNotFoundError, nullptr},
	};
}

Type::Type(Module *module, int32_t memberCount) :
	members(memberCount), typeToken(nullptr),
	size(0), fieldCount(0),
	getReferences(nullptr), finalizer(nullptr),
	nativeFieldCapacity(0), nativeFields(nullptr),
	module(module), vm(module->GetVM()),
	staticCtorLock(8000)
{
	memset(operators, 0, sizeof(MethodOverload*) * OPERATOR_COUNT);
}

Type::~Type()
{
	// If this is a standard type, unregister it
	for (unsigned int i = 0; i < std_type_names::StandardTypeCount; i++)
	{
		std_type_names::StdType type = std_type_names::Types[i];
		if (vm->types.*(type.member) == this)
			vm->types.*(type.member) = nullptr;
	}

	// If there are any native fields, destroy them
	// (Allocated with realloc)
	free(nativeFields);
}

void Type::InitOperators()
{
	this->flags |= TypeFlags::OPS_INITED;
	if (!baseType)
		return;

	assert((baseType->flags & TypeFlags::OPS_INITED) == TypeFlags::OPS_INITED);
	for (int op = 0; op < OPERATOR_COUNT; op++)
	{
		if (!this->operators[op])
			this->operators[op] = baseType->operators[op];
	}
}

Member *Type::GetMember(String *name) const
{
	Member *m;
	if (members.Get(name, m))
		return m;
	return nullptr;
}

Member *Type::FindMember(String *name, Type *fromType) const
{
	const Type *type = this;
	do {
		Member *m;
		if (type->members.Get(name, m) && m->IsAccessible(this, fromType))
			return m;
	} while (type = type->baseType);

	return nullptr; // not found
}

int Type::GetTypeToken(Thread *const thread, Value *result)
{
	int r = OVUM_SUCCESS;
	if (typeToken == nullptr)
		r = this->LoadTypeToken(thread);

	if (r == OVUM_SUCCESS)
		typeToken->Read(result);
	return r;
}

int Type::LoadTypeToken(Thread *const thread)
{
	// Type tokens can never be destroyed, so let's create a static
	// reference to it.
	StaticRef *typeTkn = GetGC()->AddStaticReference(thread, NULL_VALUE);
	if (typeTkn == nullptr)
		return thread->ThrowMemoryError();

	// Note: use GC::Alloc because the aves.Type type may not have
	// a public constructor. GC::Construct would fail if it didn't.
	int r = GetGC()->Alloc(thread, vm->types.Type, vm->types.Type->size, typeTkn->GetValuePointer());
	if (r != OVUM_SUCCESS) return r;

	// Call the type token initializer with this type and the brand
	// new allocated instance data thing. Hurrah.
	r = vm->functions.initTypeToken(thread, typeTkn->GetValuePointer()->instance, this);
	if (r == OVUM_SUCCESS)
		typeToken = typeTkn;

	return r;
}

bool Type::InitStaticFields(Thread *const thread)
{
	for (int32_t i = 0; i < members.count; i++)
	{
		Member *m = members.entries[i].value;
		if ((m->flags & MemberFlags::FIELD) == MemberFlags::FIELD &&
			(m->flags & MemberFlags::INSTANCE) == MemberFlags::NONE &&
			static_cast<Field*>(m)->staticValue == nullptr)
		{
			Field *f = static_cast<Field*>(m);
			f->staticValue = GetGC()->AddStaticReference(thread, NULL_VALUE);
			if (f->staticValue == nullptr)
				return false;
		}
	}

	return true;
}

int Type::RunStaticCtor(Thread *const thread)
{
	int r;
	staticCtorLock.Enter();
	// If we've entered this critcal section while the static ctor is running, it can
	// only mean it's running on this thread, since all other threads are locked out.
	// This call must have been triggered by one of these conditions:
	//  1. The static constructor is being initialized (it will likely reference static
	//     fields of the type).
	//  2. The static constructor of this type called a method that depends on a static
	//     field of this type, such as another type's static constructor. In this case,
	//     the other method will see null fields, which is acceptable; you should never
	//     expose static fields directly anyway, and generally should avoid cross-deps
	//     between static members of different types.
	// In both cases, it's safe to return immediately.
	if (!HasStaticCtorRun() && !IsStaticCtorRunning())
	{
		flags |= TypeFlags::STATIC_CTOR_RUNNING; // prevent infinite recursion
		if (!InitStaticFields(thread)) // Get some storage locations for the static fields
		{
			r = thread->ThrowMemoryError();
			flags &= ~TypeFlags::STATIC_CTOR_RUNNING;
			goto leave;
		}
		Member *member = GetMember(static_strings::_init);
		if (member)
		{
			// If there is a member '.init', it must be a method!
			assert((member->flags & MemberFlags::METHOD) == MemberFlags::METHOD);

			MethodOverload *mo = ((Method*)member)->ResolveOverload(0);
			if (!mo)
			{
				r = thread->ThrowNoOverloadError(0);
				flags &= ~TypeFlags::STATIC_CTOR_RUNNING;
				goto leave;
			}

			Value ignore;
			r = thread->InvokeMethodOverload(mo, 0,
				thread->currentFrame->evalStack + thread->currentFrame->stackCount,
				&ignore);
			if (r != OVUM_SUCCESS)
			{
				flags &= ~TypeFlags::STATIC_CTOR_RUNNING;
				goto leave;
			}
		}
		flags &= ~TypeFlags::STATIC_CTOR_RUNNING;
		flags |= TypeFlags::STATIC_CTOR_RUN;
	}
	r = OVUM_SUCCESS;
leave:
	staticCtorLock.Leave();
	return r;
}

void Type::AddNativeField(size_t offset, NativeFieldType fieldType)
{
	if (fieldCount == nativeFieldCapacity)
	{
		uint32_t newCap = nativeFieldCapacity ? 2 * nativeFieldCapacity : 4;
		nativeFields = reinterpret_cast<NativeField*>(realloc(nativeFields, sizeof(NativeField) * newCap));
		nativeFieldCapacity = newCap;
	}

	NativeField *field = nativeFields + fieldCount++;
	field->offset = offset;
	field->type = fieldType;
}

// Determines whether a member is accessible from a given type.
//   instType:
//     The type of the instance that the member is being loaded from.
//   fromType:
//     The type which declares the method that is accessing the member.
//     This is null for global functions.
bool Member::IsAccessible(const Type *instType, const Type *fromType) const
{
	if ((this->flags & MemberFlags::PRIVATE) != MemberFlags::NONE)
		return fromType && (declType == fromType || declType == fromType->sharedType);

	if ((this->flags & MemberFlags::PROTECTED) != MemberFlags::NONE)
	{
		if (!fromType)
			return false;

		return fromType->sharedType ?
			IsAccessibleProtectedWithSharedType(instType, fromType) :
			IsAccessibleProtected(instType, fromType);
	}

	return true; // MemberFlags::PUBLIC or accessible
}

bool Member::IsAccessibleProtected(const Type *instType, const Type *fromType) const
{
	while (instType && instType != fromType)
		instType = instType->baseType;

	if (!instType)
		return false; // instType does not inherit from fromType

	Type *originatingType = GetOriginatingType();
	while (fromType && fromType != originatingType)
		fromType = fromType->baseType;

	if (!fromType)
		return false; // fromType does not inherit from originatingType

	return true; // yay
}

bool Member::IsAccessibleProtectedWithSharedType(const Type *instType, const Type *fromType) const
{
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

	Type *originatingType = GetOriginatingType();
	tempType = fromType;
	while (tempType && tempType != originatingType)
		tempType = tempType->baseType;

	if (!tempType)
	{
		const Type *sharedType = fromType->sharedType;
		while (sharedType && sharedType != originatingType)
			sharedType = sharedType->baseType;

		if (!sharedType)
			return false; // neither fromType nor fromType->sharedType inherits from originatingType
	}

	return true;
}

Type *Member::GetOriginatingType() const
{
	assert((flags & MemberFlags::ACCESS_LEVEL) == MemberFlags::PROTECTED);
	const Method *method = nullptr;

	if ((flags & MemberFlags::KIND) == MemberFlags::METHOD)
		method = static_cast<const Method*>(this);
	else if ((flags & MemberFlags::KIND) == MemberFlags::PROPERTY)
	{
		const Property *prop = static_cast<const Property*>(this);
		method = prop->getter ? prop->getter : prop->setter;
	}
	else // Field
		return declType;

	while (method->baseMethod)
		method = method->baseMethod;
	return method->declType;
}

int Field::ReadField(Thread *const thread, Value *instance, Value *dest) const
{
	if (instance->type == nullptr)
		return thread->ThrowNullReferenceError();
	if (!Type::ValueIsType(instance, this->declType))
		return thread->ThrowTypeError();

	GCObject *gco = GCObject::FromInst(instance->instance);
	gco->fieldAccessLock.Enter();
	*dest = *reinterpret_cast<Value*>(instance->instance + this->offset);
	gco->fieldAccessLock.Leave();

	RETURN_SUCCESS;
}

int Field::ReadFieldFast(Thread *const thread, Value *instance, Value *dest) const
{
	if (instance->type == nullptr)
		return thread->ThrowNullReferenceError();

	GCObject *gco = GCObject::FromInst(instance->instance);
	gco->fieldAccessLock.Enter();
	*dest = *reinterpret_cast<Value*>(instance->instance + this->offset);
	gco->fieldAccessLock.Leave();

	RETURN_SUCCESS;
}

void Field::ReadFieldUnchecked(Value *instance, Value *dest) const
{
	GCObject *gco = GCObject::FromInst(instance->instance);
	gco->fieldAccessLock.Enter();
	*dest = *reinterpret_cast<Value*>(instance->instance + this->offset);
	gco->fieldAccessLock.Leave();
}

int Field::WriteField(Thread *const thread, Value *instanceAndValue) const
{
	if (instanceAndValue[0].type == nullptr)
		return thread->ThrowNullReferenceError();
	if (!Type::ValueIsType(instanceAndValue, this->declType))
		return thread->ThrowTypeError();

	GCObject *gco = GCObject::FromInst(instanceAndValue[0].instance);
	gco->fieldAccessLock.Enter();
	*reinterpret_cast<Value*>(instanceAndValue[0].instance + this->offset) = instanceAndValue[1];
	gco->fieldAccessLock.Leave();

	RETURN_SUCCESS;
}

int Field::WriteFieldFast(Thread *const thread, Value *instanceAndValue) const
{
	if (instanceAndValue[0].type == nullptr)
		return thread->ThrowNullReferenceError();

	GCObject *gco = GCObject::FromInst(instanceAndValue[0].instance);
	gco->fieldAccessLock.Enter();
	*reinterpret_cast<Value*>(instanceAndValue[0].instance + this->offset) = instanceAndValue[1];
	gco->fieldAccessLock.Leave();

	RETURN_SUCCESS;
}

void Field::WriteFieldUnchecked(Value *instanceAndValue) const
{
	GCObject *gco = GCObject::FromInst(instanceAndValue[0].instance);
	gco->fieldAccessLock.Enter();
	*reinterpret_cast<Value*>(instanceAndValue[0].instance + this->offset) = instanceAndValue[1];
	gco->fieldAccessLock.Leave();
}

int32_t MethodOverload::GetLocalOffset(uint16_t local) const
{
	return (int32_t)(STACK_FRAME_SIZE + local * sizeof(Value));
}

int32_t MethodOverload::GetStackOffset(uint16_t stackSlot) const
{
	return (int32_t)(STACK_FRAME_SIZE + (locals + stackSlot) * sizeof(Value));
}

RefSignaturePool *MethodOverload::GetRefSignaturePool() const
{
	return group->declModule->GetVM()->GetRefSignaturePool();
}

int MethodOverload::VerifyRefSignature(uint32_t signature, uint16_t argCount) const
{
	RefSignaturePool *refSigPool = GetRefSignaturePool();
	RefSignature methodSignature(refSignature, refSigPool);
	RefSignature argSignature(signature, refSigPool);

	// Signatures always include extra space for the instance, even if the method
	// is static. Argument 0 should never be by ref.
	if (argSignature.IsParamRef(0))
		return 0;

	int im = 1, // index into methodSignature
		ia = 1; // and into argSignature

	int paramCount = (int)this->GetEffectiveParamCount();
	if (this->IsVariadic())
	{
		if ((this->flags & MethodFlags::VAR_START) != MethodFlags::NONE)
		{
			// Test each argument to be packed, making sure none of them are by ref
			int packed = argCount - this->paramCount + 1;
			while (packed-- > 0)
			{
				if (argSignature.IsParamRef(ia))
					return ia;
				ia++;
			}
			im++; // Skip the first parameter (it's variadic)
			// And then test each required parameter against its argument
			while (im < paramCount)
			{
				if (methodSignature.IsParamRef(im) != argSignature.IsParamRef(ia))
					return ia;
				im++;
				ia++;
			}
		}
		else
		{
			// Test each required parameter against its argument
			while (im < paramCount - 1)
			{
				if (methodSignature.IsParamRef(im) != argSignature.IsParamRef(ia))
					return ia;
				im++;
				ia++;
			}
			// And then make sure every remaining argument is not by ref;
			// these will be packed into a list
			while (ia < argCount)
			{
				if (argSignature.IsParamRef(ia))
					return ia;
				ia++;
			}
		}
	}
	else
	{
		// Test each parameter against its corresponding argument
		while (im < paramCount)
		{
			if (methodSignature.IsParamRef(im) != argSignature.IsParamRef(ia))
				return ia;
			im++;
			ia++;
		}
	}
	return -1;
}

} // namespace ovum

OVUM_API void GetStandardTypes(ThreadHandle thread, StandardTypes *target, size_t targetSize)
{
	// Never copy more than sizeof(StandardTypes) bytes,
	// but potentially copy less.
	targetSize = min(targetSize, sizeof(StandardTypes));
	memcpy(target, &thread->GetVM()->types, targetSize);
}
OVUM_API TypeHandle GetType_Object(ThreadHandle thread)              { return thread->GetVM()->types.Object; }
OVUM_API TypeHandle GetType_Boolean(ThreadHandle thread)             { return thread->GetVM()->types.Boolean; }
OVUM_API TypeHandle GetType_Int(ThreadHandle thread)                 { return thread->GetVM()->types.Int; }
OVUM_API TypeHandle GetType_UInt(ThreadHandle thread)                { return thread->GetVM()->types.UInt; }
OVUM_API TypeHandle GetType_Real(ThreadHandle thread)                { return thread->GetVM()->types.Real; }
OVUM_API TypeHandle GetType_String(ThreadHandle thread)              { return thread->GetVM()->types.String; }
OVUM_API TypeHandle GetType_List(ThreadHandle thread)                { return thread->GetVM()->types.List; }
OVUM_API TypeHandle GetType_Hash(ThreadHandle thread)                { return thread->GetVM()->types.Hash; }
OVUM_API TypeHandle GetType_Method(ThreadHandle thread)              { return thread->GetVM()->types.Method; }
OVUM_API TypeHandle GetType_Iterator(ThreadHandle thread)            { return thread->GetVM()->types.Iterator; }
OVUM_API TypeHandle GetType_Type(ThreadHandle thread)                { return thread->GetVM()->types.Type; }
OVUM_API TypeHandle GetType_Error(ThreadHandle thread)               { return thread->GetVM()->types.Error; }
OVUM_API TypeHandle GetType_TypeError(ThreadHandle thread)           { return thread->GetVM()->types.TypeError; }
OVUM_API TypeHandle GetType_MemoryError(ThreadHandle thread)         { return thread->GetVM()->types.MemoryError; }
OVUM_API TypeHandle GetType_OverflowError(ThreadHandle thread)       { return thread->GetVM()->types.OverflowError; }
OVUM_API TypeHandle GetType_NoOverloadError(ThreadHandle thread)     { return thread->GetVM()->types.NoOverloadError; }
OVUM_API TypeHandle GetType_DivideByZeroError(ThreadHandle thread)   { return thread->GetVM()->types.DivideByZeroError; }
OVUM_API TypeHandle GetType_NullReferenceError(ThreadHandle thread)  { return thread->GetVM()->types.NullReferenceError; }
OVUM_API TypeHandle GetType_MemberNotFoundError(ThreadHandle thread) { return thread->GetVM()->types.MemberNotFoundError; }

OVUM_API String *Member_GetName(MemberHandle member)
{
	return member->name;
}

OVUM_API MemberKind Member_GetKind(MemberHandle member)
{
	using namespace ovum;
	switch (member->flags & MemberFlags::KIND)
	{
		case MemberFlags::METHOD:   return MemberKind::METHOD;
		case MemberFlags::FIELD:    return MemberKind::FIELD;
		case MemberFlags::PROPERTY: return MemberKind::PROPERTY;
		default:                    return MemberKind::INVALID;
	}
}
OVUM_API MemberAccess Member_GetAccessLevel(MemberHandle member)
{
	using namespace ovum;
	switch (member->flags & MemberFlags::ACCESS_LEVEL)
	{
	case MemberFlags::PUBLIC:
		return MemberAccess::PUBLIC;
	case MemberFlags::PRIVATE:
		return MemberAccess::PRIVATE;
	case MemberFlags::PROTECTED:
		return MemberAccess::PROTECTED;
	default:
		return MemberAccess::INVALID;
	}
}
OVUM_API TypeHandle Member_GetDeclType(MemberHandle member)
{
	return member->declType;
}
OVUM_API ModuleHandle Member_GetDeclModule(MemberHandle member)
{
	return member->declModule;
}

OVUM_API bool Member_IsStatic(MemberHandle member)
{
	return member->IsStatic();
}
OVUM_API bool Member_IsImpl(MemberHandle member)
{
	return (member->flags & ovum::MemberFlags::IMPL) == ovum::MemberFlags::IMPL;
}
OVUM_API bool Member_IsAccessible(MemberHandle member, TypeHandle instType, TypeHandle fromType)
{
	return member->IsAccessible(instType, fromType);
}

OVUM_API MethodHandle Member_ToMethod(MemberHandle member)
{
	if ((member->flags & ovum::MemberFlags::METHOD) == ovum::MemberFlags::METHOD)
		return (MethodHandle)member;
	return nullptr;
}
OVUM_API FieldHandle Member_ToField(MemberHandle member)
{
	if ((member->flags & ovum::MemberFlags::FIELD) == ovum::MemberFlags::FIELD)
		return (FieldHandle)member;
	return nullptr;
}
OVUM_API PropertyHandle Member_ToProperty(MemberHandle member)
{
	if ((member->flags & ovum::MemberFlags::PROPERTY) == ovum::MemberFlags::PROPERTY)
		return (PropertyHandle)member;
	return nullptr;
}


OVUM_API bool Method_IsConstructor(MethodHandle method)
{
	return (method->flags & ovum::MemberFlags::CTOR) == ovum::MemberFlags::CTOR;
}
OVUM_API int32_t Method_GetOverloadCount(MethodHandle method)
{
	return method->overloadCount;
}
OVUM_API OverloadHandle Method_GetOverload(MethodHandle method, int32_t index)
{
	if (index < 0 || index >= method->overloadCount)
		return nullptr;

	return method->overloads + index;
}
OVUM_API int32_t Method_GetOverloads(MethodHandle method, int32_t destSize, OverloadHandle *dest)
{
	int32_t count = method->overloadCount;
	if (count > destSize)
		count = destSize;

	for (int32_t i = 0; i < count; i++)
		dest[i] = method->overloads + i;

	return count;
}
OVUM_API MethodHandle Method_GetBaseMethod(MethodHandle method)
{
	return method->baseMethod;
}

OVUM_API bool Method_Accepts(MethodHandle method, int argc)
{
	return method->Accepts(argc);
}
OVUM_API OverloadHandle Method_FindOverload(MethodHandle method, int argc)
{
	if (argc < 0 || argc > UINT16_MAX)
		return nullptr;
	return method->ResolveOverload((uint16_t)argc);
}


OVUM_API MethodFlags Overload_GetFlags(OverloadHandle overload)
{
	return overload->flags;
}
OVUM_API int32_t Overload_GetParamCount(OverloadHandle overload)
{
	return overload->paramCount;
}
OVUM_API bool Overload_GetParameter(OverloadHandle overload, int32_t index, ParamInfo *dest)
{
	if (index < 0 || index >= overload->paramCount)
		return false;

	dest->name = overload->paramNames[index];

	dest->isOptional = index >= overload->paramCount - overload->optionalParamCount;
	if (overload->IsVariadic())
		dest->isVariadic = (overload->flags & MethodFlags::VAR_START) != MethodFlags::NONE ?
			index == 0 :
			index == overload->paramCount - 1;
	else
		dest->isVariadic = false;
	ovum::RefSignature refs(overload->refSignature, overload->GetRefSignaturePool());
	// +1 because the reference signature always reserves the first
	// slot for the instance, even if the method is static.
	dest->isByRef = refs.IsParamRef(index + 1);

	return true;
}
OVUM_API int32_t Overload_GetAllParameters(OverloadHandle overload, int32_t destSize, ParamInfo *dest)
{
	int32_t count = overload->paramCount;
	if (count > destSize)
		count = destSize;

	bool isVariadic = overload->IsVariadic();

	ovum::RefSignature refs(overload->refSignature, overload->GetRefSignaturePool());
	for (int32_t i = 0; i < count; i++)
	{
		ParamInfo *pi = dest + i;
		pi->name = overload->paramNames[i];

		pi->isOptional = i >= count - overload->optionalParamCount;
		if (isVariadic)
			pi->isVariadic = (overload->flags & MethodFlags::VAR_START) != MethodFlags::NONE ?
				i == 0 :
				i == count - 1;
		else
			pi->isVariadic = false;
		// +1 because the reference signature always reserves the first
		// slot for the instance, even if this method is static.
		pi->isByRef = refs.IsParamRef(i + 1);
	}

	return count;
}
OVUM_API MethodHandle Overload_GetMethod(OverloadHandle overload)
{
	return overload->group;
}


OVUM_API uint32_t Field_GetOffset(FieldHandle field)
{
	return field->offset;
}


OVUM_API MethodHandle Property_GetGetter(PropertyHandle prop)
{
	return prop->getter;
}
OVUM_API MethodHandle Property_GetSetter(PropertyHandle prop)
{
	return prop->setter;
}


OVUM_API TypeFlags Type_GetFlags(TypeHandle type)
{
	return type->flags;
}
OVUM_API String *Type_GetFullName(TypeHandle type)
{
	return type->fullName;
}
OVUM_API TypeHandle Type_GetBaseType(TypeHandle type)
{
	return type->baseType;
}
OVUM_API ModuleHandle Type_GetDeclModule(TypeHandle type)
{
	return type->module;
}

OVUM_API MemberHandle Type_GetMember(TypeHandle type, String *name)
{
	return type->GetMember(name);
}
OVUM_API MemberHandle Type_FindMember(TypeHandle type, String *name, TypeHandle fromType)
{
	return type->FindMember(name, fromType);
}

OVUM_API int32_t Type_GetMemberCount(TypeHandle type)
{
	return type->members.GetCount();
}
OVUM_API MemberHandle Type_GetMemberByIndex(TypeHandle type, const int32_t index)
{
	ovum::Member *result;
	if (type->members.GetByIndex(index, result))
		return result;
	return nullptr;
}

OVUM_API MethodHandle Type_GetOperator(TypeHandle type, Operator op)
{
	return type->operators[(int)op]->group;
}
OVUM_API int Type_GetTypeToken(ThreadHandle thread, TypeHandle type, Value *result)
{
	return type->GetTypeToken(thread, result);
}

OVUM_API uint32_t Type_GetFieldOffset(TypeHandle type)
{
	return type->fieldsOffset;
}
OVUM_API uint32_t Type_GetInstanceSize(TypeHandle type)
{
	return type->size;
}

OVUM_API void Type_SetFinalizer(TypeHandle type, Finalizer finalizer)
{
	if ((type->flags & TypeFlags::INITED) == TypeFlags::NONE)
	{
		type->finalizer = finalizer;
		if (finalizer)
			type->flags |= TypeFlags::HAS_FINALIZER;
		else if (type->baseType)
			type->flags |= type->baseType->flags & TypeFlags::HAS_FINALIZER;
		else
			type->flags &= ~TypeFlags::HAS_FINALIZER;
	}
}
OVUM_API void Type_SetInstanceSize(TypeHandle type, uint32_t size)
{
	if ((type->flags & TypeFlags::INITED) == TypeFlags::NONE)
	{
		// Ensure the effective size is a multiple of 8
		type->size = ALIGN_TO(size, 8);
		type->flags |= TypeFlags::CUSTOMPTR;
	}
}
OVUM_API void Type_SetReferenceGetter(TypeHandle type, ReferenceGetter getter)
{
	if ((type->flags & TypeFlags::INITED) == TypeFlags::NONE)
		type->getReferences = getter;
}

OVUM_API void Type_AddNativeField(TypeHandle type, size_t offset, NativeFieldType fieldType)
{
	if ((type->flags & TypeFlags::INITED) == TypeFlags::NONE)
		type->AddNativeField(offset, fieldType);
}