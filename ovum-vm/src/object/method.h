#pragma once

#include "../vm.h"
#include "member.h"
#include "../ee/thread.opcodes.h"

namespace ovum
{

// NOTE: The OverloadFlags enum members are meant to be synchronised with those
//       declared in the module specification, but also includes a small number
//       of internal implementation details.
//
//       To prevent problems, the OverloadFlags enum is not exposed verbatim to
//       the outside world; this means we can change it whenever we like. In
//       addition to this, to avoid collisions with values defined in the module
//       specification, we only use the two most significant bytes for internal
//       flags. When the module spec changes to occupy those bytes, we'll have
//       to change our approach.
enum class OverloadFlags : int32_t
{
	NONE         = 0x00000000,
	// Mask of flags to be exposed by API function Overload_GetFlags().
	VISIBLE_MASK = 0x0000ffff,

	// The method signature is variadic. Only the last parameter is
	// allowed to be variadic.
	VARIADIC     = 0x00000001,
	// The method is virtual (overridable in Osprey).
	VIRTUAL      = 0x00000100,
	// The method is abstract (it has no body).
	ABSTRACT     = 0x00000200,
	// The method overrides an inherited method.
	OVERRIDE     = 0x00000400,
	// The method has a native-code implementation.
	NATIVE       = 0x00001000,
	// The method uses the short header format (used when reading the method).
	SHORT_HEADER = 0x00002000,

	// Non-standard/internal flags follow

	// The method is an instance method. Without this flag, methods are static.
	// Note: The containing Method has an INSTANCE flag too. It is present here
	// for convenience.
	INSTANCE    = 0x00010000,
	// The method is a constructor.
	// Note: The containing Method has an CTOR flag too. It is present here for
	// convenience.
	CTOR        = 0x00020000,
	// The method has been initialized. Used for bytecode methods only, to
	// indicate that the bytecode initializer has processed the method.
	INITED      = 0x00040000,
};
OVUM_ENUM_OPS(OverloadFlags, int32_t);

struct CatchBlock
{
	Type *caughtType;
	uint32_t caughtTypeId;
	size_t catchStart;
	size_t catchEnd;

	inline bool Contains(size_t offset) const
	{
		return catchStart <= offset && offset < catchEnd;
	}
};

struct CatchBlocks
{
	size_t count;
	CatchBlock *blocks;
};

struct FinallyBlock
{
	size_t finallyStart;
	size_t finallyEnd;

	inline bool Contains(size_t offset) const
	{
		return finallyStart <= offset && offset < finallyEnd;
	}
};

enum class TryKind
{
	CATCH = 0x01,
	FINALLY = 0x02,
	FAULT = 0x03,
};

class TryBlock
{
public:

	TryKind kind;
	size_t tryStart;
	size_t tryEnd;

	union
	{
		CatchBlocks catches;
		FinallyBlock finallyBlock;
	};

	inline TryBlock() :
		kind((TryKind)0)
	{ }
	inline TryBlock(TryKind kind, size_t tryStart, size_t tryEnd) :
		kind(kind),
		tryStart(tryStart),
		tryEnd(tryEnd)
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

	inline bool Contains(size_t offset) const
	{
		return tryStart <= offset && offset < tryEnd;
	}
};

class MethodOverload
{
public:
	// The number of parameters the method has, EXCLUDING the instance
	// if it is an instance method.
	ovlocals_t paramCount;
	// The number of optional parameters the method has.
	ovlocals_t optionalParamCount;
	// The number of local variables the method uses.
	ovlocals_t locals;
	// The number of instance arguments this method takes (currently
	// always either 0 or 1). This value is stored here, rather than
	// calculated on demand, because it is needed every single time
	// the method is invoked.
	ovlocals_t instanceCount;
	// Flags associated with the method.
	OverloadFlags flags;

	String **paramNames;
	uint32_t refSignature;

	size_t tryBlockCount;
	TryBlock *tryBlocks;

	// The maximum number of stack slots the method uses. This value is
	// used only during method initialization, to allocate an appropriate
	// number of stack slots.
	ovlocals_t maxStack;

	debug::OverloadSymbols *debugSymbols;

	union
	{
		struct
		{
			uint8_t *entry;
			size_t length; // The length of the method body, in bytes.
		};
		NativeMethod nativeEntry;
	};

	// The group to which the overload belongs
	Method *group;
	// The type that declares the overload
	Type *declType;

	// Simply initializes all members to their default values.
	// We need a default constructor so we can use the type in
	// an array.
	inline MethodOverload() :
		paramCount(0),
		optionalParamCount(0),
		locals(0),
		instanceCount(0),
		flags(OverloadFlags::NONE),
		paramNames(nullptr),
		refSignature(0),
		tryBlockCount(0),
		tryBlocks(nullptr),
		maxStack(0),
		debugSymbols(nullptr),
		group(nullptr),
		declType(nullptr)
	{ }

	inline ~MethodOverload()
	{
		delete[] paramNames;

		if (!IsAbstract() && !IsNative())
			delete[] entry;

		delete[] tryBlocks;
	}

	inline bool IsVariadic() const
	{
		return (flags & OverloadFlags::VARIADIC) == OverloadFlags::VARIADIC;
	}

	inline bool IsVirtual() const
	{
		return (flags & OverloadFlags::VIRTUAL) == OverloadFlags::VIRTUAL;
	}

	inline bool IsAbstract() const
	{
		return (flags & OverloadFlags::ABSTRACT) == OverloadFlags::ABSTRACT;
	}

	inline bool IsOverride() const
	{
		return (flags & OverloadFlags::OVERRIDE) == OverloadFlags::OVERRIDE;
	}

	inline bool IsNative() const
	{
		return (flags & OverloadFlags::NATIVE) == OverloadFlags::NATIVE;
	}

	inline bool HasShortHeader() const
	{
		return (flags & OverloadFlags::SHORT_HEADER) == OverloadFlags::SHORT_HEADER;
	}

	inline bool IsInstanceMethod() const
	{
		return (flags & OverloadFlags::INSTANCE) == OverloadFlags::INSTANCE;
	}

	inline bool IsCtor() const
	{
		return (flags & OverloadFlags::CTOR) == OverloadFlags::CTOR;
	}

	inline bool IsInitialized() const
	{
		return (flags & OverloadFlags::INITED) == OverloadFlags::INITED;
	}

	inline bool Accepts(ovlocals_t argc) const
	{
		if (IsVariadic())
			return argc >= paramCount - 1;
		else
			return argc >= paramCount - optionalParamCount &&
				argc <= paramCount;
	}

	inline ovlocals_t InstanceOffset() const
	{
		return instanceCount;
	}

	// Gets the effective parameter count, which is paramCount + instance (if any).
	inline ovlocals_t GetEffectiveParamCount() const
	{
		return paramCount + instanceCount;
	}

	LocalOffset GetArgumentOffset(ovlocals_t arg) const;

	LocalOffset GetLocalOffset(ovlocals_t local) const;

	LocalOffset GetStackOffset(ovlocals_t stackSlot) const;

	RefSignaturePool *GetRefSignaturePool() const;

	// Verifies the ref signature of an invocation against the overload's ref signature,
	// by comparing each argument against the referenceness expected by each corresponding
	// parameter.
	//
	// Parameters:
	//   signature:
	//     The ref signature of an invocation.
	//   argCount:
	//     The number of arguments passed to the overload, NOT including the instance.
	// Returns:
	//   -1 if there is no refness mismatch. If there is a mismatch, returns the index of
	//   the first argument with incorrect refness. Argument 0 is reserved for the instance;
	//   hence named argument numbering starts at 1.
	int VerifyRefSignature(uint32_t signature, ovlocals_t argCount) const;
};

class Method : public Member
{
public:
	// The number of overloads in the method.
	size_t overloadCount;
	// The overloads of the method.
	MethodOverload *overloads;
	// If this method is not a global function and the base type declares
	// a method with the same name as this one, then this pointer refers
	// to that method, plus some rules about accessibility.
	Method *baseMethod;

	inline Method(String *name, Module *declModule, MemberFlags flags) :
		Member(name, declModule, flags | MemberFlags::METHOD),
		overloadCount(0),
		overloads(nullptr),
		baseMethod(nullptr)
	{ }

	inline virtual ~Method()
	{
		delete[] overloads;
	}

	bool Accepts(ovlocals_t argCount) const;

	MethodOverload *ResolveOverload(ovlocals_t argCount) const;

	void SetDeclType(Type *type);
};

} // namespace ovum
