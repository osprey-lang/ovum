#pragma once

#ifndef VM__METHODINITIALIZER_INTERNAL_H
#define VM__METHODINITIALIZER_INTERNAL_H

#include "ov_vm.internal.h"
#include "instructions.internal.h"
#include <vector>

namespace ovum
{

class MethodInitializer
{
private:
	VM *vm;
	MethodOverload *method;

	DISABLE_COPY_AND_ASSIGN(MethodInitializer);

public:
	inline MethodInitializer(VM *vm) : vm(vm), method(nullptr)
	{ }

	int Initialize(MethodOverload *method, Thread *const thread);

private:
	void ReadInstructions(instr::MethodBuilder &builder);

	void InitBranchOffsets(instr::MethodBuilder &builder);
	void InitTryBlockOffsets(instr::MethodBuilder &builder);
	void InitDebugSymbolOffsets(instr::MethodBuilder &builder);

	void CalculateStackHeights(instr::MethodBuilder &builder, StackManager &stack);

	void WriteInitializedBody(instr::MethodBuilder &builder);
	void FinalizeTryBlockOffsets(instr::MethodBuilder &builder);
	void FinalizeDebugSymbolOffsets(instr::MethodBuilder &builder);

	Type *TypeFromToken(uint32_t token);
	String *StringFromToken(uint32_t token);
	Method *MethodFromToken(uint32_t token);
	MethodOverload *MethodOverloadFromToken(uint32_t token, uint32_t argCount);
	Field *FieldFromToken(uint32_t token, bool shouldBeStatic);
	void EnsureConstructible(Type *type, uint32_t argCount);
};

class StackManager
{
public:
	typedef struct StackEntry_S
	{
		enum StackEntryFlags : uint8_t
		{
			IN_USE   = 1,
			THIS_ARG = 2,
			IS_REF   = 4,
		} flags;
	} StackEntry;

	inline StackManager(RefSignaturePool *refSignatures)
		: refSignatures(refSignatures)
	{ }

	inline virtual ~StackManager() { }

	virtual uint32_t GetStackHeight() const = 0;

	// Adds a branch to the end of the queue, with stack slots copied from the current branch.
	// All stack slots retain their flags.
	virtual void EnqueueBranch(int32_t firstInstr) = 0;
	// Adds a branch to the end of the queue, with the specified initial stack height.
	// The stack slots in the new branch have no special flags.
	virtual void EnqueueBranch(uint32_t stackHeight, int32_t firstInstr) = 0;

	// Moves to the next branch in the queue, and returns
	// the index of the first instruction in the branch.
	virtual int32_t DequeueBranch() = 0;

	virtual bool ApplyStackChange(instr::StackChange change, bool pushRef) = 0;

	virtual bool HasRefs(uint32_t argCount) const = 0;

	virtual bool IsRef(uint32_t stackSlot) const = 0;

	virtual uint32_t GetRefSignature(uint32_t argCount) const = 0;

	inline RefSignaturePool *GetRefSignaturePool() const
	{
		return refSignatures;
	}

protected:
	RefSignaturePool *refSignatures;
};

} // namespace ovum

#endif // VM__METHODINITIALIZER_INTERNAL_H