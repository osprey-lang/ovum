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
#include "../res/staticstrings.h"

namespace ovum
{

Type::Type(Module *module, int32_t memberCount) :
	members(memberCount),
	typeToken(nullptr),
	size(0),
	fieldCount(0),
	walkReferences(nullptr),
	finalizer(nullptr),
	nativeFieldCapacity(0),
	nativeFields(nullptr),
	module(module),
	vm(module->GetVM()),
	staticCtorLock(8000)
{
	memset(operators, 0, sizeof(MethodOverload*) * OPERATOR_COUNT);
}

Type::~Type()
{
	// If there are any native fields, destroy them
	// (Allocated with realloc)
	free(nativeFields);
}

void Type::InitOperators()
{
	this->flags |= TypeFlags::OPS_INITED;
	if (!baseType)
		return;

	OVUM_ASSERT(baseType->AreOpsInited());
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

Member *Type::FindMember(String *name, MethodOverload *fromMethod) const
{
	const Type *type = this;
	do {
		Member *m;
		if (type->members.Get(name, m) && m->IsAccessible(this, fromMethod))
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
	Value nullValue;
	nullValue.type = nullptr;

	// Type tokens can never be destroyed, so let's create a static
	// reference to it.
	StaticRef *typeTkn = GetGC()->AddStaticReference(thread, &nullValue);
	if (typeTkn == nullptr)
		return thread->ThrowMemoryError();

	// Note: use GC::Alloc because the aves.Type type may not have
	// a public constructor. GC::Construct will fail if it doesn't.
	int r = GetGC()->Alloc(
		thread,
		vm->types.Type,
		vm->types.Type->GetTotalSize(),
		typeTkn->GetValuePointer()
	);
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
	Value nullValue;
	nullValue.type = nullptr;

	for (int32_t i = 0; i < members.count; i++)
	{
		Member *m = members.entries[i].value;
		if (m->IsField() &&
			m->IsStatic() &&
			static_cast<Field*>(m)->staticValue == nullptr)
		{
			Field *f = static_cast<Field*>(m);
			f->staticValue = GetGC()->AddStaticReference(thread, &nullValue);
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
		Member *member = GetMember(thread->GetStrings()->members.init_);
		if (member)
		{
			// If there is a member '.init', it must be a method!
			OVUM_ASSERT(member->IsMethod());

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
		flags |= TypeFlags::STATIC_CTOR_HAS_RUN;
	}
	r = OVUM_SUCCESS;
leave:
	staticCtorLock.Leave();
	return r;
}

int Type::AddNativeField(size_t offset, NativeFieldType fieldType)
{
	if (fieldCount == nativeFieldCapacity)
	{
		uint32_t newCap = nativeFieldCapacity ? 2 * nativeFieldCapacity : 4;
		NativeField *newFields = reinterpret_cast<NativeField*>(realloc(nativeFields, sizeof(NativeField) * newCap));
		if (newFields == nullptr)
			return OVUM_ERROR_NO_MEMORY;

		nativeFields = newFields;
		nativeFieldCapacity = newCap;
	}

	NativeField *field = nativeFields + fieldCount++;
	field->offset = offset;
	field->type = fieldType;
	RETURN_SUCCESS;
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
OVUM_API TypeHandle GetType_TypeConversionError(ThreadHandle thread) { return thread->GetVM()->types.TypeConversionError; }

OVUM_API uint32_t Type_GetFlags(TypeHandle type)
{
	return static_cast<uint32_t>(type->flags & ovum::TypeFlags::VISIBLE_MASK);
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
OVUM_API MemberHandle Type_FindMember(TypeHandle type, String *name, OverloadHandle fromMethod)
{
	return type->FindMember(name, fromMethod);
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
	if (!type->IsInited())
	{
		type->finalizer = finalizer;
		if (finalizer)
			type->flags |= ovum::TypeFlags::HAS_FINALIZER;
		else if (type->baseType)
			type->flags |= type->baseType->flags & ovum::TypeFlags::HAS_FINALIZER;
		else
			type->flags &= ~ovum::TypeFlags::HAS_FINALIZER;
	}
}

OVUM_API void Type_SetInstanceSize(TypeHandle type, size_t size)
{
	if (!type->IsInited())
	{
		// Ensure the effective size is a multiple of 8
		type->size = OVUM_ALIGN_TO(size, 8);
		type->flags |= ovum::TypeFlags::CUSTOMPTR;
	}
}

OVUM_API void Type_SetReferenceWalker(TypeHandle type, ReferenceWalker getter)
{
	if (!type->IsInited())
	{
		type->walkReferences = getter;
	}
}

OVUM_API void Type_SetConstructorIsAllocator(TypeHandle type, bool isAllocator)
{
	if (!type->IsInited())
	{
		if (isAllocator)
			type->flags |= ovum::TypeFlags::ALLOCATOR_CTOR;
		else
			type->flags &= ~ovum::TypeFlags::ALLOCATOR_CTOR;
	}
}

OVUM_API int Type_AddNativeField(TypeHandle type, size_t offset, NativeFieldType fieldType)
{
	if (!type->IsInited())
	{
		return type->AddNativeField(offset, fieldType);
	}
	return OVUM_ERROR_UNSPECIFIED;
}
