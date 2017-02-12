#pragma once

#include "../vm.h"
#include "instructions.h"
#include <vector>

namespace ovum
{

class MethodInitializer
{
public:
	inline MethodInitializer(VM *vm) :
		vm(vm),
		method(nullptr)
	{ }

	int Initialize(MethodOverload *method, Thread *const thread);

private:
	OVUM_DISABLE_COPY_AND_ASSIGN(MethodInitializer);

	VM *vm;
	MethodOverload *method;

	void ReadInstructions(instr::MethodBuilder &builder);

	void CalculateStackHeights(instr::MethodBuilder &builder, StackManager &stack);

	void EnqueueInitialBranches(instr::MethodBuilder &builder, StackManager &stack);

	void VerifyStackHeight(instr::MethodBuilder &builder, StackManager &stack, size_t index);

	void TryUpdateInputOutput(
		instr::MethodBuilder &builder,
		StackManager &stack,
		instr::Instruction *prev,
		instr::Instruction *instr,
		size_t index
	);

	void TryUpdateConditionalBranch(
		instr::MethodBuilder &builder,
		instr::Instruction *prev,
		instr::Branch *branch,
		size_t index
	);

	static bool IsBranchComparisonOperator(IntermediateOpcode opc);

	static IntermediateOpcode GetBranchComparisonOpcode(
		IntermediateOpcode branchOpc,
		IntermediateOpcode comparisonOpc
	);

	void WriteInitializedBody(instr::MethodBuilder &builder);

	void FinalizeTryBlockOffsets(instr::MethodBuilder &builder);

	void FinalizeDebugSymbolOffsets(instr::MethodBuilder &builder);
};

class StackManager
{
public:
	static const size_t NO_BRANCH = (size_t)-1;

	struct StackEntry
	{
		enum StackEntryFlags : uint8_t
		{
			IN_USE   = 1,
			THIS_ARG = 2,
			IS_REF   = 4,
		} flags;
	};

	inline StackManager(RefSignaturePool *refSignatures) :
		refSignatures(refSignatures)
	{ }

	inline virtual ~StackManager() { }

	virtual ovlocals_t GetStackHeight() const = 0;

	// Adds a branch to the end of the queue, with stack slots copied from the current branch.
	// All stack slots retain their flags.
	virtual void EnqueueBranch(size_t firstInstr) = 0;
	// Adds a branch to the end of the queue, with the specified initial stack height.
	// The stack slots in the new branch have no special flags.
	virtual void EnqueueBranch(ovlocals_t stackHeight, size_t firstInstr) = 0;

	// Moves to the next branch in the queue, and returns
	// the index of the first instruction in the branch.
	virtual size_t DequeueBranch() = 0;

	virtual bool ApplyStackChange(instr::StackChange change, bool pushRef) = 0;

	virtual bool HasRefs(ovlocals_t argCount) const = 0;

	virtual bool IsRef(ovlocals_t stackSlot) const = 0;

	virtual uint32_t GetRefSignature(ovlocals_t argCount) const = 0;

	inline RefSignaturePool *GetRefSignaturePool() const
	{
		return refSignatures;
	}

protected:
	RefSignaturePool *refSignatures;
};

} // namespace ovum
