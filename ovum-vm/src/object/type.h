#pragma once

#include "../vm.h"
#include "../ee/vm.h"
#include "../threading/sync.h"
#include "../util/stringhash.h"

namespace ovum
{

// NOTE: The TypeFlags enum members are meant to be synchronised with those
//       declared in the module specification, but also includes a variety
//       of internal implementation details.
//
//       To prevent problems, the TypeFlags enum is not exposed verbatim to
//       the outside world; this means we can change it whenever we like.
//       In addition to this, to avoid collisions with values defined in the
//       module specification, we only use the two most significant bytes
//       for internal flags. If the module spec changes to occupy those bytes,
//       we'll have to change our approach.
enum class TypeFlags : uint32_t
{
	NONE            = 0x00000000,
	// Mask of flags to be exposed by API function Type_GetFlags().
	VISIBLE_MASK    = 0x0000ffff,

	ACCESSIBILITY   = 0x000000ff,
	PUBLIC          = 0x00000001,
	INTERNAL        = 0x00000002,

	ABSTRACT        = 0x00000100,
	SEALED          = 0x00000200,
	STATIC          = ABSTRACT | SEALED,
	IMPL            = 0x00001000,
	PRIMITIVE       = 0x00002000,

	// Non-standard/internal flags follow

	// The type does not use a standard Value array for its fields.
	// This is used only by the GC during collection.
	CUSTOMPTR           = 0x00010000,
	// The type's constructor also takes care of allocation. Only available
	// for types with native implementations.
	ALLOCATOR_CTOR      = 0x00020000,
	// The type's operators have been initialized.
	OPS_INITED          = 0x00040000,
	// The type has been initialised.
	INITED              = 0x00080000,
	// The static constructor for the type has been run.
	STATIC_CTOR_HAS_RUN = 0x00100000,
	// The static constructor is currently running.
	STATIC_CTOR_RUNNING = 0x00200000,
	// The type or any of its base types has a finalizer,
	// which must be run before the value is collected.
	HAS_FINALIZER       = 0x00400000,
};
OVUM_ENUM_OPS(TypeFlags, uint32_t);

// Types, once initialized, are supposed to be (more or less) immutable.
// If you assign to any of the members in a Type, you have no one to blame but yourself.
// That said, the VM occasionally updates the flags.
class Type
{
public:
	struct NativeField
	{
		size_t offset;
		NativeFieldType type;
	};

	Type(Module *module, int32_t memberCount);
	~Type();

	Member *GetMember(String *name) const;
	Member *FindMember(String *name, MethodOverload *fromMethod) const;

	// Flags associated with the type.
	TypeFlags flags;

	// The offset (in bytes) of the first field in instances of this type.
	uint32_t fieldsOffset;
	// The total size (in bytes) of instances of this type, not including
	// the size consumed by the base type.
	// Note: this is 0 for Object and String, the latter of which is variable-size.
	size_t size;
	// The total number of instance fields in the type. If the flag CUSTOMPTR
	// is set, this contains the number of native fields; otherwise, this is
	// the number of Value fields.
	int fieldCount;

	// Members! These allow us to look up members by name.
	StringHash<Member*> members;

	// The type from which this inherits (null only for Object).
	Type *baseType;
	// A type whose private and protected members this type has access to.
	// The shared type must be in the same module as this type.
	Type *sharedType;
	// The module that declares the type.
	Module *module;
	// The VM instance that the type belongs to.
	VM *vm;

	// The fully qualified name of the type, e.g. "aves.Object".
	String *fullName;

	// The instance constructor of the type, or null if there is none.
	Method *instanceCtor;

	// The reference getter for the type. Is null unless the type has
	// TypeFlags::CUSTOMPTR, in which case the GC uses this method to
	// obtain a list of Value references from instance of the type.
	ReferenceGetter getReferences;
	// The finalizer for the type. Only available to native-code types.
	Finalizer finalizer;
	// The number of native fields that can be defined before the array
	// must be resized.
	uint32_t nativeFieldCapacity;
	// Native fields defined on the type
	NativeField *nativeFields;

	// An instance of aves.Type that is bound to this type.
	// Use GetTypeToken() to retrieve this value; this starts
	// out as a NULL_VALUE and is only initialized on demand.
	StaticRef *typeToken;

	// The number of overloadable operators.
	// If you change Operator and/or Opcode without changing this,
	// you have no one to blame but yourself.
	static const int OPERATOR_COUNT = 16;
	// Operator implementations. If an operator implementation is null,
	// then the type does not implement that operator.
	MethodOverload *operators[OPERATOR_COUNT];

	CriticalSection staticCtorLock;

	// Gets the total number of bytes required to construct an instance
	// of this type. This includes the size of the base type.
	inline size_t GetTotalSize() const
	{
		return fieldsOffset + size;
	}

	int GetTypeToken(Thread *const thread, Value *result);

	inline VM *GetVM() const
	{
		return vm;
	}

	inline GC *GetGC() const
	{
		return vm->GetGC();
	}

	inline bool IsPublic() const
	{
		return (flags & TypeFlags::PUBLIC) == TypeFlags::PUBLIC;
	}

	inline bool IsInternal() const
	{
		return (flags & TypeFlags::INTERNAL) == TypeFlags::INTERNAL;
	}

	inline bool IsAbstract() const
	{
		return (flags & TypeFlags::ABSTRACT) == TypeFlags::ABSTRACT;
	}

	inline bool IsSealed() const
	{
		return (flags & TypeFlags::SEALED) == TypeFlags::SEALED;
	}

	inline bool IsStatic() const
	{
		return (flags & TypeFlags::STATIC) == TypeFlags::STATIC;
	}

	inline bool IsImpl() const
	{
		return (flags & TypeFlags::IMPL) == TypeFlags::IMPL;
	}

	inline bool IsPrimitive() const
	{
		return (flags & TypeFlags::PRIMITIVE) == TypeFlags::PRIMITIVE;
	}

	inline bool IsCustomPtr() const
	{
		return (flags & TypeFlags::CUSTOMPTR) == TypeFlags::CUSTOMPTR;
	}

	inline bool ConstructorIsAllocator() const
	{
		return (flags & TypeFlags::ALLOCATOR_CTOR) == TypeFlags::ALLOCATOR_CTOR;
	}

	inline bool AreOpsInited() const
	{
		return (flags & TypeFlags::OPS_INITED) == TypeFlags::OPS_INITED;
	}

	inline bool IsInited() const
	{
		return (flags & TypeFlags::INITED) == TypeFlags::INITED;
	}

	inline bool HasStaticCtorRun() const
	{
		return (flags & TypeFlags::STATIC_CTOR_HAS_RUN) == TypeFlags::STATIC_CTOR_HAS_RUN;
	}

	inline bool IsStaticCtorRunning() const
	{
		return (flags & TypeFlags::STATIC_CTOR_RUNNING) == TypeFlags::STATIC_CTOR_RUNNING;
	}

	inline bool HasFinalizer() const
	{
		return (flags & TypeFlags::HAS_FINALIZER) == TypeFlags::HAS_FINALIZER;
	}

	void InitOperators();
	bool InitStaticFields(Thread *const thread);
	int RunStaticCtor(Thread *const thread);

	int AddNativeField(size_t offset, NativeFieldType fieldType);

	// Determines whether the specified type equals or inherits from another
	// specified type.
	// Parameters:
	//   self:
	//     The type whose inheritance hierarchy to examine. This value can
	//     be null.
	//   base:
	//     The type to test whether 'self' inherits from. This value cannot
	//     be null.
	// Returns:
	//   True if 'self' equals or inherits from 'base'. Otherwise, false.
	static inline bool InheritsFrom(const Type *self, const Type *base)
	{
		// In general we avoid inline methods that contain loops. However,
		// this method is extremely small and simple, and performance is of
		// high importance. Therefore, this is an acceptable exception.

		while (self && self != base)
			self = self->baseType;

		// When we get this far, either self == base (if the current type
		// inherits from 'base'), or self == nullptr (if we've walked up
		// the entire inheritance hierarchy without a match).

		return self != nullptr;
	}

	static inline bool ValueIsType(Value *value, Type *const type)
	{
		return InheritsFrom(value->type, type);
	}

private:
	OVUM_DISABLE_COPY_AND_ASSIGN(Type);

	int LoadTypeToken(Thread *const thread);
};

} // namespace ovum
