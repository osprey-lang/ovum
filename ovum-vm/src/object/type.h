#ifndef VM__TYPE_INTERNAL_H
#define VM__TYPE_INTERNAL_H

#include "../vm.h"
#include "../ee/vm.h"
#include "../threading/sync.h"
#include "../util/stringhash.h"

namespace ovum
{

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
	Member *FindMember(String *name, Type *fromType) const;

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
	static const int OPERATOR_COUNT = 18;
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

	inline bool IsPrimitive() const
	{
		return (flags & TypeFlags::PRIMITIVE) == TypeFlags::PRIMITIVE;
	}
	inline bool HasFinalizer() const
	{
		return (flags & TypeFlags::HAS_FINALIZER) == TypeFlags::HAS_FINALIZER;
	}
	inline bool HasStaticCtorRun() const
	{
		return (flags & TypeFlags::STATIC_CTOR_RUN) == TypeFlags::STATIC_CTOR_RUN;
	}
	inline bool IsStaticCtorRunning() const
	{
		return (flags & TypeFlags::STATIC_CTOR_RUNNING) == TypeFlags::STATIC_CTOR_RUNNING;
	}

	void InitOperators();
	bool InitStaticFields(Thread *const thread);
	int RunStaticCtor(Thread *const thread);

	void AddNativeField(size_t offset, NativeFieldType fieldType);

	static inline bool ValueIsType(Value *value, Type *const type)
	{
		Type *valtype = value->type;
		while (valtype)
		{
			if (valtype == type)
				return true;
			valtype = valtype->baseType;
		}
		return false;
	}

private:
	int LoadTypeToken(Thread *const thread);
};

} // namespace ovum

#endif
