#pragma once

#ifndef VM__TYPE_INTERNAL_H
#define VM__TYPE_INTERNAL_H

#include "ov_vm.internal.h"
#ifdef PRINT_DEBUG_INFO
#include <cstdio>
#endif
#include <cassert>

// forward declarations
class Module;
class Member;
class Method;

enum class MemberFlags : uint16_t
{
	// The member has no flags.
	NONE      = 0x0000,
	// The member is a field.
	FIELD     = 0x0001,
	// The member is a method.
	METHOD    = 0x0002,
	// The member is a property.
	PROPERTY  = 0x0004,

	// The member is public.
	PUBLIC    = 0x0008,
	// The member is protected.
	PROTECTED = 0x0010,
	// The member is private.
	PRIVATE   = 0x0020,

	//// The member is abstract.
	//ABSTRACT  = 0x0080,
	//// The member is virtual (overridable in Osprey).
	//VIRTUAL   = 0x0100,
	//// The member is sealed (no direct equivalent in Osprey).
	//SEALED    = 0x0200,

	// The member is an instance member.
	INSTANCE  = 0x0400,

	// The member is used internally to implement some behaviour.
	// Primarily used by getters, setters, iterator accessors and
	// operator overloads.
	IMPL      = 0x0800,

	// A mask for extracting the access level of a member.
	ACCESS_LEVEL = PUBLIC | PROTECTED | PRIVATE,
	// A mask for extracting the kind of a member.
	KIND = FIELD | METHOD | PROPERTY,
};
ENUM_OPS(MemberFlags, uint16_t);

class Member
{
public:
	MemberFlags flags;

	String *name;
	//Member *next;
	Type *declType;
	Module *declModule;

	Member(String *name, Type *declType, MemberFlags flags);
	inline Member(String *name, Module *declModule, MemberFlags flags) :
		name(name), declType(nullptr),
		declModule(declModule), flags(flags)
	{ }

	inline ~Member()
	{
#ifdef PRINT_DEBUG_INFO
		if ((flags & MemberFlags::FIELD) == MemberFlags::FIELD)
			wprintf(L"Releasing field: ");
		else if ((flags & MemberFlags::METHOD) == MemberFlags::METHOD)
			wprintf(L"Releasing method: ");
		else
			wprintf(L"Releasing property: ");
		if (declType)
		{
			VM::Print(declType->fullName);
			wprintf(L".");
		}
		VM::PrintLn(this->name);
#endif
	}

	inline bool IsStatic() const
	{
		return (flags & MemberFlags::INSTANCE) == MemberFlags::NONE;
	}

	// Determines whether a member is accessible from a given type.
	//   instType:
	//     The type of the instance that the member is being loaded from.
	//   fromType:
	//     The type which declares the method that is accessing the member.
	//     This is null for global functions.
	bool IsAccessible(const Type *instType, const Type *fromType) const;

private:
	// Gets the type that originally declared the member.
	// For virtual (overridable) protected methods, this is
	// the type that introduced the method. E.g.:
	//    class A {
	//        protected overridable f(); // introduces f
	//    }
	//    class B is A {
	//        override f(); // overrides A.f; originating type = A
	//    }
	Type *GetOriginatingType() const;

	bool IsAccessibleProtected(const Type *instType, const Type *fromType) const;
	bool IsAccessibleProtectedWithSharedType(const Type *instType, const Type *fromType) const;
};

class Field : public Member
{
public:
	union
	{
		int32_t offset;
		StaticRef *staticValue;
	};

	inline Field(String *name, Type *declType, MemberFlags flags) :
		Member(name, declType, flags | MemberFlags::FIELD)
	{ }

	void ReadField(Thread *const thread, Value *instance, Value *dest) const;
	void ReadFieldFast(Thread *const thread, Value *instance, Value *dest) const;
	void ReadFieldUnchecked(Value *instance, Value *dest) const;

	inline Value ReadField(Thread *const thread, Value *instance) const
	{
		Value result;
		ReadField(thread, instance, &result);
		return result;
	}
	inline Value ReadFieldFast(Thread *const thread, Value *instance) const
	{
		Value result;
		ReadFieldFast(thread, instance, &result);
		return result;
	}
	inline Value ReadFieldUnchecked(Value *instance) const
	{
		Value result;
		ReadFieldUnchecked(instance, &result);
		return result;
	}

	void WriteField(Thread *const thread, Value *instanceAndValue) const;
	void WriteFieldFast(Thread *const thread, Value *instanceAndValue) const;
	void WriteFieldUnchecked(Value *instanceAndValue) const;
};


enum class MethodFlags : int16_t
{
	// No method flags.
	NONE      = 0x0000,
	// The method has a variadic parameter at the end.
	VAR_END   = 0x0001,
	// The method has a variadic parameter at the start.
	VAR_START = 0x0002,

	// The method has a native-code implementation.
	NATIVE    = 0x0004,

	// The method is an instance method. Without this flag,
	// methods are static.
	INSTANCE  = 0x0008,

	// The method is virtual (overridable in Osprey).
	VIRTUAL   = 0x0010,
	// The method is abstract (it has no implementation).
	ABSTRACT  = 0x0020,

	// The method is a constructor.
	CTOR      = 0x0040,

	// The method has been initialized. Used for bytecode methods only,
	// to indicate that the bytecode initializer has processed the method.
	INITED    = 0x0080,

	// A mask for extracting the variadic flags of a method.
	VARIADIC = VAR_END | VAR_START,
};
ENUM_OPS(MethodFlags, int16_t);

typedef struct StackFrame_S StackFrame;
// Represents a local offset, that is, an offset that is relative
// to the base of the stack frame. This is negative for arguments.
// Use the overloaded + operator together with a StackFrame to get
// the local that it actually refers to.
class LocalOffset
{
private:
	int32_t offset;

public:
	inline LocalOffset(const int32_t offset) : offset(offset * sizeof(Value)) { }

	inline int32_t GetOffset() const { return offset / sizeof(Value); }

	inline Value *const operator+(const StackFrame *const frame) const
	{
		// Offsets 0 and 1 point directly into the stack frame;
		// they are never supposed to be reached.
		assert(offset != 0 && offset != 1);
		return (Value*)((char*)frame + offset);
	}
};

class Method : public Member
{
public:
	typedef struct
	{
		Type *caughtType;
		uint32_t caughtTypeId;
		uint32_t catchStart;
		uint32_t catchEnd;
	} CatchBlock;

	class TryBlock
	{
	public:
		// NOTE: These values must match those used in the module spec!
		enum class TryKind : uint8_t
		{
			CATCH   = 0x01,
			FINALLY = 0x02,
		};

		TryKind kind;
		uint32_t tryStart;
		uint32_t tryEnd;

		union
		{
			struct
			{
				int32_t count;
				CatchBlock *blocks;
			} catches;
			struct
			{
				uint32_t finallyStart;
				uint32_t finallyEnd;
			} finallyBlock;
		};

		inline TryBlock()
			: kind((TryKind)0)
		{ }
		inline TryBlock(TryKind kind, uint32_t tryStart, uint32_t tryEnd)
			: kind(kind), tryStart(tryStart), tryEnd(tryEnd)
		{
			if (kind == TryKind::CATCH)
				catches.blocks = nullptr;
		}

		inline ~TryBlock()
		{
			if (kind == TryKind::CATCH && catches.blocks)
			{
				delete[] catches.blocks;
				catches.blocks = nullptr;
				catches.count = 0;
			}
		}
	};

	class Overload
	{
	public:
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

		String **paramNames;

		int32_t tryBlockCount;
		TryBlock *tryBlocks;

		debug::DebugSymbols *debugSymbols;

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
			if ((flags & MethodFlags::VARIADIC) != MethodFlags::NONE)
				return argc >= paramCount - 1;
			else
				return argc >= paramCount - optionalParamCount &&
					argc <= paramCount;
		}

		inline int InstanceOffset() const
		{
			return ((int)(flags & MethodFlags::INSTANCE) >> 3);
		}

		// Gets the effective parameter count, which is paramCount + instance (if any).
		inline unsigned int GetEffectiveParamCount() const
		{
			return paramCount + InstanceOffset();
		}

		inline bool IsInstanceMethod() const
		{
			return (flags & MethodFlags::INSTANCE) == MethodFlags::INSTANCE;
		}

		inline bool IsInitialized() const
		{
			return (flags & MethodFlags::INITED) == MethodFlags::INITED;
		}

		inline LocalOffset GetArgumentOffset(uint16_t arg) const
		{
			return LocalOffset((int16_t)(arg - GetEffectiveParamCount()));
		}
		LocalOffset GetLocalOffset(uint16_t local) const;
		LocalOffset GetStackOffset(uint16_t stackSlot) const;

		inline ~Overload()
		{
			delete[] paramNames;

			if ((flags & (MethodFlags::NATIVE | MethodFlags::ABSTRACT)) == MethodFlags::NONE)
				delete[] entry;

			if (tryBlockCount > 0)
			{
				delete[] tryBlocks;
				tryBlocks = nullptr;
				tryBlockCount = 0;
			}
		}
	};

	// The number of overloads in the method.
	int32_t overloadCount;
	// The overloads of the method.
	Overload *overloads;
	// If this method is not a global function and the base type declares
	// a method with the same name as this one, then this pointer refers
	// to that method, plus some rules about accessibility.
	Method *baseMethod;

	inline Method(String *name, Module *declModule, MemberFlags flags) :
		Member(name, declModule, flags | MemberFlags::METHOD),
		overloadCount(0), overloads(nullptr), baseMethod(nullptr)
	{ }

	inline ~Method()
	{
		delete[] overloads;
	}

	inline const bool Accepts(const uint16_t argCount) const
	{
		const Method *m = this;
		do
		{
			for (int i = 0; i < m->overloadCount; i++)
				if (m->overloads[i].Accepts(argCount))
					return true;
		} while (m = m->baseMethod);
		return false;
	}

	inline Overload *ResolveOverload(const uint16_t argCount) const
	{
		const Method *method = this;
		do
		{
			for (int i = 0; i < method->overloadCount; i++)
			{
				Method::Overload *mo = method->overloads + i;
				if (mo->Accepts(argCount))
					return mo;
			}
		} while (method = method->baseMethod);
		return nullptr;
	}

	inline void SetDeclType(Type *type)
	{
		this->declType = type;
		for (int i = 0; i < overloadCount; i++)
			this->overloads[i].declType = type;
	}
};

class Property : public Member
{
public:
	Method *getter;
	Method *setter;

	inline Property(String *name, Type *declType, MemberFlags flags) :
		Member(name, declType, flags | MemberFlags::PROPERTY)
	{ }
};

// Types, once initialized, are supposed to be (more or less) immutable.
// If you assign to any of the members in a Type, you have no one to blame but yourself.
// That said, the VM occasionally updates the flags.
class Type
{
public:
	Type(int32_t memberCount);
	~Type();

	Member *GetMember(String *name) const;
	Member *FindMember(String *name, Type *fromType) const;
	//Method::Overload *GetOperator(Operator op);

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
	Method::Overload *operators[OPERATOR_COUNT];

	// The reference getter for the type. Is null unless the type has
	// TypeFlags::CUSTOMPTR, in which case the GC uses this method to
	// obtain a list of Value references from instance of the type.
	ReferenceGetter getReferences;
	// The finalizer for the type. Only available to native-code types.
	Finalizer finalizer;

	// A handle to the module that declares the type.
	Module *module;

	// An instance of aves.Type that is bound to this type.
	// Use GetTypeToken() to retrieve this value; this starts
	// out as a NULL_VALUE and is only initialized on demand.
	StaticRef *typeToken;

	Value GetTypeToken(Thread *const thread);

	inline bool HasFinalizer() const
	{
		return (flags & TypeFlags::HAS_FINALIZER) == TypeFlags::HAS_FINALIZER;
	}

	void InitOperators();
	void InitStaticFields();

	static inline const bool ValueIsType(Value *value, Type *const type)
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
	void LoadTypeToken(Thread *const thread);
};

inline Member::Member(String *name, Type *declType, MemberFlags flags) :
	name(name), declType(declType),
	declModule(declType->module), flags(flags)
{ }


namespace std_type_names
{
	typedef struct StdType_S
	{
		String *name;
		TypeHandle StandardTypes::*member;
		const char *const initerFunction;
	} StdType;

	extern const unsigned int StandardTypeCount;
	extern const StdType Types[];
}

#endif