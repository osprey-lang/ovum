#ifndef VM__METHOD_H
#define VM__METHOD_H

#include "../vm.h"
#include "member.h"

namespace ovum
{

struct CatchBlock
{
	Type *caughtType;
	uint32_t caughtTypeId;
	uint32_t catchStart;
	uint32_t catchEnd;
};

struct CatchBlocks
{
	int32_t count;
	CatchBlock *blocks;
};

struct FinallyBlock
{
	uint32_t finallyStart;
	uint32_t finallyEnd;
};

class TryBlock
{
public:
	// NOTE: These values must match those used in the module spec!
	enum class TryKind : uint8_t
	{
		CATCH = 0x01,
		FINALLY = 0x02,
	};

	TryKind kind;
	uint32_t tryStart;
	uint32_t tryEnd;

	union
	{
		CatchBlocks catches;
		FinallyBlock finallyBlock;
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

class MethodOverload
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
	uint32_t refSignature;

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

	// Simply initializes all members to their default values.
	// We need a default constructor so we can use the type in
	// an array.
	inline MethodOverload() :
		paramCount(0), optionalParamCount(0),
		locals(0), maxStack(0),
		flags(MethodFlags::NONE),
		paramNames(nullptr),
		refSignature(0),
		tryBlockCount(0),
		tryBlocks(nullptr),
		debugSymbols(nullptr),
		group(nullptr),
		declType(nullptr)
	{ }

	inline ~MethodOverload()
	{
		delete[] paramNames;

		if ((flags & (MethodFlags::NATIVE | MethodFlags::ABSTRACT)) == MethodFlags::NONE)
			delete[] entry;

		delete[] tryBlocks;
	}

	inline bool Accepts(uint16_t argc) const
	{
		if (IsVariadic())
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

	inline bool IsVariadic() const
	{
		return (flags & MethodFlags::VARIADIC) != MethodFlags::NONE;
	}

	inline bool IsInstanceMethod() const
	{
		return (flags & MethodFlags::INSTANCE) == MethodFlags::INSTANCE;
	}

	inline bool IsInitialized() const
	{
		return (flags & MethodFlags::INITED) == MethodFlags::INITED;
	}

	inline int32_t GetArgumentOffset(uint16_t arg) const
	{
		return (int32_t)(arg - GetEffectiveParamCount()) * sizeof(Value);
	}
	int32_t GetLocalOffset(uint16_t local) const;
	int32_t GetStackOffset(uint16_t stackSlot) const;

	RefSignaturePool *GetRefSignaturePool() const;

	// argCount does NOT include the instance.
	int VerifyRefSignature(uint32_t signature, uint16_t argCount) const;
};

class Method : public Member
{
public:
	// The number of overloads in the method.
	int32_t overloadCount;
	// The overloads of the method.
	MethodOverload *overloads;
	// If this method is not a global function and the base type declares
	// a method with the same name as this one, then this pointer refers
	// to that method, plus some rules about accessibility.
	Method *baseMethod;

	inline Method(String *name, Module *declModule, MemberFlags flags) :
		Member(name, declModule, flags | MemberFlags::METHOD),
		overloadCount(0), overloads(nullptr), baseMethod(nullptr)
	{ }

	inline virtual ~Method()
	{
		delete[] overloads;
	}

	bool Accepts(uint16_t argCount) const;

	MethodOverload *ResolveOverload(uint16_t argCount) const;

	void SetDeclType(Type *type);
};

} // namespace ovum

#endif // VM__METHOD_H