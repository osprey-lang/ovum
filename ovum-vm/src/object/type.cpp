#include "type.h"
#include "member.h"
#include "field.h"
#include "method.h"
#include "../module/module.h"
#include "../../inc/ov_string.h"
#include "../ee/thread.h"
#include "../ee/refsignature.h"
#include "../gc/gc.h"
#include "../gc/staticref.h"

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
		{_Object.AsString(),              &StandardTypes::Object,              nullptr},
		{_Boolean.AsString(),             &StandardTypes::Boolean,             nullptr},
		{_Int.AsString(),                 &StandardTypes::Int,                 nullptr},
		{_UInt.AsString(),                &StandardTypes::UInt,                nullptr},
		{_Real.AsString(),                &StandardTypes::Real,                nullptr},
		{_String.AsString(),              &StandardTypes::String,              nullptr},
		{_List.AsString(),                &StandardTypes::List,                "InitListInstance"},
		{_Hash.AsString(),                &StandardTypes::Hash,                "InitHashInstance"},
		{_Method.AsString(),              &StandardTypes::Method,              nullptr},
		{_Iterator.AsString(),            &StandardTypes::Iterator,            nullptr},
		{_Type.AsString(),                &StandardTypes::Type,                "InitTypeToken"},
		{_Error.AsString(),               &StandardTypes::Error,               nullptr},
		{_TypeError.AsString(),           &StandardTypes::TypeError,           nullptr},
		{_MemoryError.AsString(),         &StandardTypes::MemoryError,         nullptr},
		{_OverflowError.AsString(),       &StandardTypes::OverflowError,       nullptr},
		{_NoOverloadError.AsString(),     &StandardTypes::NoOverloadError,     nullptr},
		{_DivideByZeroError.AsString(),   &StandardTypes::DivideByZeroError,   nullptr},
		{_NullReferenceError.AsString(),  &StandardTypes::NullReferenceError,  nullptr},
		{_MemberNotFoundError.AsString(), &StandardTypes::MemberNotFoundError, nullptr},
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
	int r = GetGC()->Alloc(thread, vm->types.Type,
		vm->types.Type->GetTotalSize(),
		typeTkn->GetValuePointer());
	if (r != OVUM_SUCCESS) return r;

	// Call the type token initializer with this type and the brand
	// new allocated instance data thing. Hurrah.
	r = vm->functions.initTypeToken(thread, typeTkn->GetValuePointer()->v.instance, this);
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
OVUM_API size_t Type_GetInstanceSize(TypeHandle type)
{
	return type->size;
}
OVUM_API size_t Type_GetTotalSize(TypeHandle type)
{
	return type->GetTotalSize();
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
OVUM_API void Type_SetInstanceSize(TypeHandle type, size_t size)
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