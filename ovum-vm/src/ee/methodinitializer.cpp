#include "methodinitializer.h"
#include "instructions.h"
#include "methodbuilder.h"
#include "methodparser.h"
#include "methodinitexception.h"
#include "refsignature.h"
#include "../object/type.h"
#include "../object/field.h"
#include "../module/module.h"
#include "../debug/debugsymbols.h"
#include <queue>

namespace ovum
{

class SmallStackManager : public StackManager
{
public:
	static const ovlocals_t MaxStack = 8;

private:
	struct Branch
	{
		size_t firstInstr;
		ovlocals_t stackHeight;
		StackEntry stack[MaxStack];
	};

	std::queue<Branch> branches;

public:
	inline SmallStackManager(RefSignaturePool *refSignatures)
		: StackManager(refSignatures)
	{
		// Push a fake branch onto the queue, so that the first call to DequeueBranch
		// will actually move to the first "real" branch.
		branches.push(Branch());
	}

	virtual ovlocals_t GetStackHeight() const
	{
		return branches.front().stackHeight;
	}

	virtual void EnqueueBranch(size_t firstInstr)
	{
		Branch &cur = branches.front();

		Branch br = { firstInstr, cur.stackHeight };
		for (ovlocals_t i = 0; i < cur.stackHeight; i++)
			br.stack[i] = cur.stack[i];

		branches.push(br);
	}
	virtual void EnqueueBranch(ovlocals_t stackHeight, size_t firstInstr)
	{
		Branch br = { firstInstr, stackHeight };
		for (ovlocals_t i = 0; i < stackHeight; i++)
			br.stack[i].flags = StackEntry::IN_USE;
		branches.push(br);
	}

	virtual size_t DequeueBranch()
	{
		branches.pop();
		if (branches.size() > 0)
			return branches.front().firstInstr;
		else
			return NO_BRANCH;
	}

	virtual bool ApplyStackChange(instr::StackChange change, bool pushRef)
	{
		typedef StackEntry::StackEntryFlags EntryFlags;

		Branch &cur = branches.front();
		OVUM_ASSERT(cur.stackHeight - change.removed + change.added <= MaxStack);
		if (cur.stackHeight < change.removed)
			return false; // Not enough values on stack

		cur.stackHeight -= change.removed;
		EntryFlags newFlags = (EntryFlags)(StackEntry::IN_USE | (pushRef ? StackEntry::IS_REF : 0));
		for (int i = 0; i < change.added; i++)
			cur.stack[cur.stackHeight + i].flags = newFlags;
		cur.stackHeight += change.added;

		return true; // Yay!
	}

	virtual bool HasRefs(ovlocals_t argCount) const
	{
		const Branch &cur = branches.front();
		OVUM_ASSERT(cur.stackHeight >= argCount && argCount <= MaxStack);

		for (ovlocals_t i = 1; i <= argCount; i++)
			if (cur.stack[cur.stackHeight - i].flags & StackEntry::IS_REF)
				return true;

		return false;
	}

	virtual bool IsRef(ovlocals_t stackSlot) const
	{
		const Branch &cur = branches.front();
		OVUM_ASSERT(stackSlot < MaxStack);
		StackEntry::StackEntryFlags flags = cur.stack[cur.stackHeight - 1 - stackSlot].flags;
		return (flags & StackEntry::IS_REF) == StackEntry::IS_REF;
	}

	virtual uint32_t GetRefSignature(ovlocals_t argCount) const
	{
		const Branch &cur = branches.front();
		OVUM_ASSERT(cur.stackHeight >= argCount && argCount <= MaxStack);

		RefSignatureBuilder refBuilder(argCount);

		ovlocals_t origin = cur.stackHeight - argCount;
		for (ovlocals_t i = 0; i < argCount; i++)
			if (cur.stack[origin + i].flags & StackEntry::IS_REF)
				refBuilder.SetParam(i, true);

		return refBuilder.Commit(refSignatures);
	}
};

class LargeStackManager : public StackManager
{
private:
	class Branch
	{
	public:
		size_t firstInstr;
		ovlocals_t maxStack;
		ovlocals_t stackHeight;
		Box<StackEntry[]> stack;

		inline Branch() :
			firstInstr(-1),
			maxStack(0),
			stackHeight(0),
			stack()
		{ }
		inline Branch(size_t firstInstr, ovlocals_t maxStack) :
			firstInstr(firstInstr),
			maxStack(maxStack),
			stackHeight(0),
			stack(new StackEntry[maxStack])
		{ }
		inline Branch(size_t firstInstr, const Branch &other) :
			firstInstr(firstInstr),
			maxStack(other.maxStack),
			stackHeight(other.stackHeight),
			stack(new StackEntry[maxStack])
		{
			CopyMemoryT(this->stack.get(), other.stack.get(), other.maxStack);
		}
		inline Branch(const Branch &other) :
			firstInstr(other.firstInstr),
			maxStack(other.maxStack),
			stackHeight(other.stackHeight),
			stack(new StackEntry[other.maxStack])
		{
			CopyMemoryT(this->stack.get(), other.stack.get(), other.maxStack);
		}

		inline friend void swap(Branch &one, Branch &two)
		{
			using std::swap;

			swap(one.firstInstr, two.firstInstr);
			swap(one.maxStack, two.maxStack);
			swap(one.stackHeight, two.stackHeight);
			swap(one.stack, two.stack);
		}

		inline Branch &operator=(Branch other)
		{
			swap(*this, other);
			return *this;
		}
	};

	ovlocals_t maxStack;
	std::queue<Branch> branches;

public:
	LargeStackManager(ovlocals_t maxStack, RefSignaturePool *refSignatures)
		: StackManager(refSignatures), maxStack(maxStack)
	{
		branches.push(Branch());
	}

	virtual ovlocals_t GetStackHeight() const
	{
		return branches.front().stackHeight;
	}

	virtual void EnqueueBranch(size_t firstInstr)
	{
		Branch br(branches.front()); // Use the copy constructor! :D
		br.firstInstr = firstInstr;
		branches.push(br);
	}
	virtual void EnqueueBranch(ovlocals_t stackHeight, size_t firstInstr)
	{
		Branch br = Branch(firstInstr, maxStack);
		br.stackHeight = stackHeight;
		for (ovlocals_t i = 0; i < stackHeight; i++)
			br.stack[i].flags = StackEntry::IN_USE;
		branches.push(br);
	}

	virtual size_t DequeueBranch()
	{
		branches.pop();
		if (branches.size() > 0)
			return branches.front().firstInstr;
		else
			return NO_BRANCH;
	}

	virtual bool ApplyStackChange(instr::StackChange change, bool pushRef)
	{
		typedef StackEntry::StackEntryFlags EntryFlags;

		Branch &cur = branches.front();
		OVUM_ASSERT(cur.stackHeight - change.removed + change.added <= maxStack);
		if (cur.stackHeight < change.removed)
			return false; // Not enough values on stack

		cur.stackHeight -= change.removed;
		EntryFlags newFlags = (EntryFlags)(StackEntry::IN_USE | (pushRef ? StackEntry::IS_REF : 0));
		for (int i = 0; i < change.added; i++)
			cur.stack[cur.stackHeight + i].flags = newFlags;
		cur.stackHeight += change.added;

		return true; // Yay!
	}

	virtual bool HasRefs(ovlocals_t argCount) const
	{
		const Branch &cur = branches.front();

		for (ovlocals_t i = 1; i <= argCount; i++)
			if (cur.stack[cur.stackHeight - i].flags & StackEntry::IS_REF)
				return true;

		return false;
	}

	virtual bool IsRef(ovlocals_t stackSlot) const
	{
		const Branch &cur = branches.front();
		StackEntry::StackEntryFlags flags = cur.stack[cur.stackHeight - 1 - stackSlot].flags;
		return (flags & StackEntry::IS_REF) == StackEntry::IS_REF;
	}

	virtual uint32_t GetRefSignature(ovlocals_t argCount) const
	{
		const Branch &cur = branches.front();

		RefSignatureBuilder refBuilder(argCount);

		ovlocals_t origin = cur.stackHeight - argCount;
		for (ovlocals_t i = 0; i < argCount; i++)
			if (cur.stack[origin + i].flags & StackEntry::IS_REF)
				refBuilder.SetParam(i, true);

		return refBuilder.Commit(refSignatures);
	}
};

int MethodInitializer::Initialize(MethodOverload *method, Thread *const thread)
{
	using namespace instr;

	OVUM_ASSERT(!method->IsInitialized());
	this->method = method;

	MethodBuilder builder;
	try
	{
		// First, initialize all the instructions based on the original bytecode
		ReadInstructions(builder);

		// And now, we assign each instruction input and output offsets,
		// as appropriate. This step may also rewrite the method somewhat,
		// removing instructions for optimisation purposes and changing
		// some LocalOffsets from stack offsets to locals.
		if (method->maxStack <= SmallStackManager::MaxStack)
		{
			SmallStackManager stack(vm->GetRefSignaturePool());
			CalculateStackHeights(builder, stack);
		}
		else
		{
			LargeStackManager stack(method->maxStack, vm->GetRefSignaturePool());
			CalculateStackHeights(builder, stack);
		}

		WriteInitializedBody(builder);
		FinalizeTryBlockOffsets(builder);
		FinalizeDebugSymbolOffsets(builder);
	}
	catch (MethodInitException &e)
	{
		vm->PrintMethodInitException(e);
		abort();
	}

	int r = OVUM_SUCCESS;
	if (builder.GetTypeCount() > 0)
		r = thread->CallStaticConstructors(builder);
	return r;
}

/*** Step 1: Reading the instructions ***/

void MethodInitializer::ReadInstructions(instr::MethodBuilder &builder)
{
	instr::MethodParser::ParseInto(method, builder);
}

/*** Step 2: Stack height calculation & optimizations ***/

void MethodInitializer::CalculateStackHeights(instr::MethodBuilder &builder, StackManager &stack)
{
	using namespace instr;

	EnqueueInitialBranches(builder, stack);

	size_t index;
	while ((index = stack.DequeueBranch()) != StackManager::NO_BRANCH)
	{
		Instruction *prev = nullptr;
		while (true)
		{
			Instruction *instr = builder[index];
			if (builder.GetStackHeight(index) != MethodBuilder::UNVISITED)
			{
				VerifyStackHeight(builder, stack, index);
				break; // This branch has already been visited!
				// Note: the instruction may have been marked for removal. The branch is
				// still perfectly safe to skip, because the only way to get an instruction
				// considered for removal is to visit it.
			}
			else
			{
				ovlocals_t stackHeight = stack.GetStackHeight();
				builder.SetStackHeight(index, stackHeight);
				if (instr->HasIncomingBranches())
					// Only calculmacate this if necessary
					builder.SetRefSignature(index, stack.GetRefSignature(stackHeight));
			}

			TryUpdateInputOutput(builder, stack, prev, instr, index);

			if (instr->IsBranch())
			{
				Branch *br = static_cast<Branch*>(instr);
				if (br->IsConditional())
				{
					stack.EnqueueBranch(br->target.index); // Use the same stack
					// Note: If TryUpdateConditionalBranch actually updates anything,
					// 'prev' will be deleted. Do not attempt to use 'prev' after this
					// call.
					TryUpdateConditionalBranch(builder, prev, br, index);
				}
				else
				{
					prev = nullptr;
					index = br->target.index; // Continue at the target instruction
					continue; // don't increment index
				}
			}
			else if (instr->IsSwitch())
			{
				Switch *sw = static_cast<Switch*>(instr);
				for (size_t i = 0; i < sw->targetCount; i++)
					stack.EnqueueBranch(sw->targets[i].index); // Use the same stack
			}
			else if (instr->opcode == OPI_RET || instr->opcode == OPI_RETNULL ||
				instr->opcode == OPI_THROW || instr->opcode == OPI_RETHROW ||
				instr->opcode == OPI_ENDFINALLY)
			{
				break; // This branch has terminated.
			}

			prev = instr;
			index++;
		}
	}

	// Remove instructions that are now unnecessary! :D
	builder.PerformRemovals(method);
}

void MethodInitializer::EnqueueInitialBranches(instr::MethodBuilder &builder, StackManager &stack)
{
	// The first instruction is always reachable, and always with a stack
	// height of 0.
	stack.EnqueueBranch(0, 0);

	// If the method has any try blocks, we must add the first instruction
	// of each catch, finally and fault as a branch, because they will never
	// be reached by fallthrough or branching.
	for (size_t i = 0; i < method->tryBlockCount; i++)
	{
		TryBlock &tryBlock = method->tryBlocks[i];
		switch (tryBlock.kind)
		{
		case TryKind::CATCH:
			{
				// The initial stack height of a catch block is 1, because the
				// thrown error is on the stack.
				CatchBlocks &catches = tryBlock.catches;
				for (size_t c = 0; c < catches.count; c++)
					stack.EnqueueBranch(1, catches.blocks[c].catchStart);
			}
			break;
		case TryKind::FINALLY:
		case TryKind::FAULT: // uses finallyBlock
			stack.EnqueueBranch(0, tryBlock.finallyBlock.finallyStart);
			break;
		}
	}
}

void MethodInitializer::VerifyStackHeight(
	instr::MethodBuilder &builder,
	StackManager &stack,
	size_t index
)
{
	ovlocals_t stackHeight = stack.GetStackHeight();

	if (builder.GetStackHeight(index) != stackHeight)
		throw MethodInitException::InconsistentStack(
			"Instruction reached with different stack heights.",
			method,
			index
		);

	if (builder.GetRefSignature(index) != stack.GetRefSignature(stackHeight))
		throw MethodInitException::InconsistentStack(
			"Instruction reached with different referencenesses of stack slots.",
			method,
			index
		);
}

void MethodInitializer::TryUpdateInputOutput(
	instr::MethodBuilder &builder,
	StackManager &stack,
	instr::Instruction *prev,
	instr::Instruction *instr,
	size_t index
)
{
	using namespace instr;

	StackChange sc = instr->GetStackChange();
	if (sc.removed > 0 || instr->HasInput())
	{
		// We can perform a bunch of fun optimizations here if:
		//   1. there is a previous instruction, and
		//   2. the current instruction has no incoming branches.
		// If either is not true, we cannot optimize any local offsets here,
		// so we skip to the default input offset.
		if (prev == nullptr || instr->HasIncomingBranches())
			goto updateInputDefault;

		// First, let's see if we can update the output of the previous instruction.
		// If:
		//   1. prev has an output, and
		//   2. prev added exactly one value to the stack, or is dup
		// then, if instr is a StoreLocal, we can update prev to point directly
		// to the local variable, thus avoiding the stack altogether; otherwise,
		// if instr is a pop, we can similarly update prev's output to discard
		// the result.
		// If either condition is not true, we must try to update the input of the
		// current instruction.
		if (!prev->HasOutput())
			goto updateInput;
		if (prev->GetStackChange().added != 1 && !prev->IsDup())
			goto updateInput;

		if (instr->IsStoreLocal())
		{
			prev->UpdateOutput(static_cast<StoreLocal*>(instr)->target, false);
			builder.MarkForRemoval(index);
			goto updateDone;
		}
		if (instr->opcode == OPI_POP)
		{
			// Write the result to the stack, but pretend it's not on the stack.
			// (This won't increment the stack height)
			prev->UpdateOutput(method->GetStackOffset(stack.GetStackHeight() - 1), false);
			builder.MarkForRemoval(index);
			goto updateDone;
		}

		updateInput:
		{
			// If instr requires its input to be on the stack, or it has
			// incoming branches, then we can't optimize its input.
			// instr->HasBranches() is tested for above.
			if (instr->RequiresStackInput())
				goto updateInputDefault;

			if (prev->IsLoadLocal() && instr->HasInput())
			{
				// If prev is a LoadLocal, then we can update instr to take the input
				// directly from prev's local and remove prev.
				instr->UpdateInput(static_cast<LoadLocal*>(prev)->source, false);
				builder.MarkForRemoval(index - 1);
				goto updateDone;
			}
			if (prev->IsDup() && instr->IsBranch() && ((Branch*)instr)->IsConditional())
			{
				// dup followed by conditional branch: use the dup's input for the branch,
				// and pretend it's not on the stack.
				// For example, something like this:
				//     ldloc 0
				//     ldmem "value"
				//     dup
				//     brnull LABEL
				// gets turned into:
				//     ldloc 0
				//     ldmem "value" onto stack
				//     brnull LABEL with local condition
				instr->UpdateInput(static_cast<DupInstr*>(prev)->source, false);
				builder.MarkForRemoval(index - 1);
				goto updateDone;
			}
		}

		updateInputDefault:
		{
			instr->UpdateInput(method->GetStackOffset(stack.GetStackHeight() - sc.removed), true);
		}

		updateDone: ;
	}

	if (instr->HasOutput())
	{
		instr->UpdateOutput(method->GetStackOffset(stack.GetStackHeight() - sc.removed), true);
	}

	if (sc.removed > 0 && stack.GetStackHeight() >= sc.removed)
	{
		if (instr->AcceptsRefs())
		{
			if (instr->SetReferenceSignature(stack) != -1)
				throw MethodInitException::InconsistentStack(
					"Incorrect referenceness of stack arguments.",
					method,
					index
				);
		}
		else if (stack.HasRefs(sc.removed))
		{
			throw MethodInitException::StackHasRefs(
				"The instruction does not take references on the stack.",
				method,
				index
			);
		}
	}

	if (!stack.ApplyStackChange(sc, instr->PushesRef()))
		throw MethodInitException::InsufficientStackHeight(
			"There are not enough values on the stack.",
			method,
			index
		);
}

void MethodInitializer::TryUpdateConditionalBranch(
	instr::MethodBuilder &builder,
	instr::Instruction *prev,
	instr::Branch *branch,
	size_t index
)
{
	// If the previous instruction is one of the operators ==, <, >, <= or >=,
	// and the current instruction is a brtrue or brfalse, then we can transform
	// the sequence to a single, special operator:
	//   eq  + brtrue => breq
	//   lt  + brtrue => brlt
	//   gt  + brtrue => brgt
	//   lte + brtrue => brlte
	//   gte + brtrue => brgte
	// and
	//   eq  + brfalse => brneq
	//   lt  + brfalse => brgte
	//   gt  + brfalse => brlte
	//   lte + brfalse => brgt
	//   gte + brfalse => brlt
	// The previous instruction is replaced with the special branch instruction,
	// and the current instruction is deleted.

	// If there is no previous instruction, the current cannot possibly be preceded
	// by an operator. If the current instruction has incoming branches, we cannot
	// delete it.
	if (!prev || branch->HasIncomingBranches())
		return;

	if (!IsBranchComparisonOperator(prev->opcode))
		return;

	if (branch->opcode < OPI_BRFALSE_L || branch->opcode > OPI_BRTRUE_S)
		return;

	// If we get this far, we can update the branch instruction.

	// Let's figure out the new ocpode.
	IntermediateOpcode newOpcode = GetBranchComparisonOpcode(branch->opcode, prev->opcode);
	OVUM_ASSERT(newOpcode != OPI_NOP);

	// Set the previous instruction to the new comparison thing.
	// This also deletes the old Instruction.
	builder.SetInstruction(
		index - 1,
		Box<instr::Instruction>(new instr::BranchComparison(
			static_cast<instr::ExecOperator*>(prev)->args,
			branch->target,
			newOpcode
		))
	);
	// Mark the current instruction for removal
	builder.MarkForRemoval(index);
}

bool MethodInitializer::IsBranchComparisonOperator(IntermediateOpcode opc)
{
	switch (opc)
	{
	case OPI_EQ_L:
	case OPI_EQ_S:
	case OPI_LT_L:
	case OPI_LT_S:
	case OPI_GT_L:
	case OPI_GT_S:
	case OPI_LTE_L:
	case OPI_LTE_S:
	case OPI_GTE_L:
	case OPI_GTE_S:
		return true;
	default:
		return false;
	}
}

IntermediateOpcode MethodInitializer::GetBranchComparisonOpcode(
	IntermediateOpcode branchOpc,
	IntermediateOpcode comparisonOpc
)
{
	IntermediateOpcode result = OPI_NOP;
	if (branchOpc == OPI_BRTRUE_L || branchOpc == OPI_BRTRUE_S)
	{
		switch (comparisonOpc)
		{
		case OPI_EQ_L:  case OPI_EQ_S:  result = OPI_BREQ;  break;
		case OPI_LT_L:  case OPI_LT_S:  result = OPI_BRLT;  break;
		case OPI_GT_L:  case OPI_GT_S:  result = OPI_BRGT;  break;
		case OPI_LTE_L: case OPI_LTE_S: result = OPI_BRLTE; break;
		case OPI_GTE_L: case OPI_GTE_S: result = OPI_BRGTE; break;
		}
	}
	else
	{
		// For simplicity, we've defined some aliases for the negated cases
		switch (comparisonOpc)
		{
		case OPI_EQ_L:  case OPI_EQ_S:  result = OPI_BRNEQ;  break;
		case OPI_LT_L:  case OPI_LT_S:  result = OPI_BRNLT;  break;
		case OPI_GT_L:  case OPI_GT_S:  result = OPI_BRNGT;  break;
		case OPI_LTE_L: case OPI_LTE_S: result = OPI_BRNLTE; break;
		case OPI_GTE_L: case OPI_GTE_S: result = OPI_BRNGTE; break;
		}
	}
	return result;
}

/*** Step 3: Result writing & finalization ***/

void MethodInitializer::WriteInitializedBody(instr::MethodBuilder &builder)
{
	using namespace instr;

	// Let's allocate a buffer for the output, yay!
	MethodBuffer buffer(builder.GetByteSize());
	for (size_t i = 0; i < builder.GetLength(); i++)
	{
		Instruction *instr = builder[i];
		instr->WriteBytes(buffer, builder);

		// The buffer should be properly aligned after each instruction
		OVUM_ASSERT((buffer.GetCurrent() - buffer.GetBuffer()) % oa::ALIGNMENT == 0);
	}

	delete[] method->entry;
	method->entry  = buffer.Release();
	method->length = builder.GetByteSize();
	method->flags |= OverloadFlags::INITED;
}

void MethodInitializer::FinalizeTryBlockOffsets(instr::MethodBuilder &builder)
{
	for (size_t t = 0; t < method->tryBlockCount; t++)
	{
		TryBlock &tryBlock = method->tryBlocks[t];
		
		tryBlock.tryStart = builder.GetNewOffset(tryBlock.tryStart);
		tryBlock.tryEnd = builder.GetNewOffset(tryBlock.tryEnd);

		switch (tryBlock.kind)
		{
		case TryKind::CATCH:
			for (size_t c = 0; c < tryBlock.catches.count; c++)
			{
				CatchBlock &catchBlock = tryBlock.catches.blocks[c];
				catchBlock.catchStart = builder.GetNewOffset(catchBlock.catchStart);
				catchBlock.catchEnd = builder.GetNewOffset(catchBlock.catchEnd);
			}
			break;
		case TryKind::FINALLY:
		case TryKind::FAULT: // uses finallyBlock
			{
				FinallyBlock &finallyBlock = tryBlock.finallyBlock;
				finallyBlock.finallyStart = builder.GetNewOffset(finallyBlock.finallyStart);
				finallyBlock.finallyEnd = builder.GetNewOffset(finallyBlock.finallyEnd);
			}
			break;
		}
	}
}

void MethodInitializer::FinalizeDebugSymbolOffsets(instr::MethodBuilder &builder)
{
	if (!method->debugSymbols)
		return;

	debug::OverloadSymbols *debug = method->debugSymbols;
	size_t debugSymbolCount = debug->GetSymbolCount();
	for (size_t i = 0; i < debugSymbolCount; i++)
	{
		debug::DebugSymbol &sym = debug->GetSymbol(i);
		sym.startOffset = builder.GetNewOffset(sym.startOffset);
		sym.endOffset = builder.GetNewOffset(sym.endOffset);
	}
}

} // namespace ovum
