#include "ov_vm.internal.h"
#include "ov_thread.opcodes.h"
#include <vector>
#include <queue>
#include <memory>
#include <string>

#define I16_ARG(ip)  *reinterpret_cast<int16_t *>(ip)
#define I32_ARG(ip)  *reinterpret_cast<int32_t *>(ip)
#define I64_ARG(ip)  *reinterpret_cast<int64_t *>(ip)
#define U16_ARG(ip)  *reinterpret_cast<uint16_t*>(ip)
#define U32_ARG(ip)  *reinterpret_cast<uint32_t*>(ip)
#define U64_ARG(ip)  *reinterpret_cast<uint64_t*>(ip)

namespace instr
{
	const StackChange StackChange::empty = StackChange(0, 0);

	MethodBuilder::~MethodBuilder()
	{
		for (instr_iter i = instructions.begin(); i != instructions.end(); i++)
			delete i->instr;
	}

	void MethodBuilder::Append(const uint32_t originalOffset, const uint32_t originalSize, Instruction *instr)
	{
		instructions.push_back(InstrDesc(originalOffset, originalSize, instr));
		instr->offset = lastOffset;
		lastOffset += instr->GetSize();
		hasBranches = hasBranches || instr->IsBranch() || instr->IsSwitch();
	}

	void MethodBuilder::MarkForRemoval(const int32_t index)
	{
		assert(!instructions[index].instr->HasBranches());
		// Mark the instruction for removal by setting the known stack height
		// to a negative value. This will mark it as unreachable, and all such
		// instructions are removed, as they are unnecessary.
		// We use -2 rather than -1 as a way of distinguishing between unvisited
		// and explicitly removed. Particularly, if you try to set the stack
		// height of an explicitly removed instruction in debug mode, you will
		// get a failed assertion.
		instructions[index].stackHeight = -2;
	}

	void MethodBuilder::PerformRemovals(Method::Overload *method)
	{
		using namespace std;

		if (instructions.size() <= 32)
		{
			int32_t newIncides[32];
			PerformRemovalsInternal(newIncides, method);
		}
		else
		{
			unique_ptr<int32_t[]> newIndices(new int32_t[instructions.size()]);
			PerformRemovalsInternal(newIndices.get(), method);
		}
	}

	void MethodBuilder::PerformRemovalsInternal(int32_t newIndices[], Method::Overload *method)
	{
		typedef Method::TryBlock::TryKind TryKind;
		this->lastOffset = 0; // Must recalculate byte offsets as well

		int32_t oldIndex = 0, newIndex = 0;
		for (instr_iter i = instructions.begin(); i != instructions.end(); oldIndex++)
		{
			if (i->stackHeight < 0)
			{
				// This instruction may have been the first instruction in a protected region,
				// in which case the next following instruction becomes the first in that block.
				// Hence:
				newIndices[oldIndex] = newIndex;
				delete i->instr;
				i = instructions.erase(i);
			}
			else
			{
				i->instr->offset = lastOffset;
				lastOffset += i->instr->GetSize();
				newIndices[oldIndex] = newIndex;
				newIndex++;
				i++;
			}
		}

		if (hasBranches)
			for (instr_iter i = instructions.begin(); i != instructions.end(); i++)
				if (i->instr->IsBranch())
				{
					Branch *br = static_cast<Branch*>(i->instr);
					br->target = newIndices[br->target];
				}
				else if (i->instr->IsSwitch())
				{
					Switch *sw = static_cast<Switch*>(i->instr);
					for (int t = 0; t < sw->targetCount; t++)
						sw->targets[t] = newIndices[sw->targets[t]];
				}

		for (int32_t t = 0; t < method->tryBlockCount; t++)
		{
			Method::TryBlock *tryBlock = method->tryBlocks + t;
			tryBlock->tryStart = newIndices[tryBlock->tryStart];
			tryBlock->tryEnd = newIndices[tryBlock->tryEnd];

			if (tryBlock->kind == TryKind::CATCH)
			{
				for (int32_t c = 0; c < tryBlock->catches.count; c++)
				{
					Method::CatchBlock *catchBlock = tryBlock->catches.blocks + c;
					catchBlock->catchStart = newIndices[catchBlock->catchStart];
					catchBlock->catchEnd = newIndices[catchBlock->catchEnd];
				}
			}
			else if (tryBlock->kind == TryKind::FINALLY)
			{
				tryBlock->finallyBlock.finallyStart = newIndices[tryBlock->finallyBlock.finallyStart];
				tryBlock->finallyBlock.finallyEnd = newIndices[tryBlock->finallyBlock.finallyEnd];
			}
		}
	}

	int32_t MethodBuilder::GetNewOffset(const int32_t index, const Instruction *relativeTo) const
	{
		return instructions[index].instr->offset - relativeTo->offset - (int)relativeTo->GetSize();
	}

	void MethodBuilder::SetInstruction(int32_t index, Instruction *newInstr, bool deletePrev)
	{
		if (deletePrev)
			delete instructions[index].instr;
		instructions[index].instr = newInstr;
	}

	void MethodBuilder::AddTypeToInitialize(Type *type)
	{
		if ((type->flags & TypeFlags::STATIC_CTOR_RUN) != TypeFlags::NONE)
			return;

		for (type_iter i = typesToInitialize.begin(); i < typesToInitialize.end(); i++)
			if (*i == type)
				return;

		typesToInitialize.push_back(type);
	}
}

class StackManager
{
public:
	typedef struct StackEntry_S
	{
		enum StackEntryFlags : uint8_t
		{
			IN_USE   = 1,
			THIS_ARG = 2,
		} flags;
	} StackEntry;

	inline virtual ~StackManager() { }

	virtual uint32_t GetStackHeight() = 0;

	// Adds a branch to the end of the queue, with stack slots copied from the current branch.
	// All stack slots retain their flags.
	virtual void EnqueueBranch(int32_t firstInstr) = 0;
	// Adds a branch to the end of the queue, with the specified initial stack height.
	// The stack slots in the new branch have no special flags.
	virtual void EnqueueBranch(uint32_t stackHeight, int32_t firstInstr) = 0;

	// Moves to the next branch in the queue, and returns
	// the index of the first instruction in the branch.
	virtual int32_t DequeueBranch() = 0;

	virtual bool ApplyStackChange(instr::StackChange change) = 0;
};

class SmallStackManager : public StackManager
{
private:
	typedef struct Branch_S
	{
		int32_t firstInstr;
		uint32_t stackHeight;
		StackEntry stack[8];
	} Branch;

	std::queue<Branch> branches;

public:
	inline SmallStackManager()
	{
		// Push a fake branch onto the queue, so that the first call to DequeueBranch
		// will actually move to the first "real" branch.
		branches.push(Branch());
	}

	virtual uint32_t GetStackHeight()
	{
		return branches.front().stackHeight;
	}

	virtual void EnqueueBranch(int32_t firstInstr)
	{
		Branch &cur = branches.front();

		Branch br = { firstInstr, cur.stackHeight };
		for (uint32_t i = 0; i < cur.stackHeight; i++)
			br.stack[i] = cur.stack[i];

		branches.push(br);
	}
	virtual void EnqueueBranch(uint32_t stackHeight, int32_t firstInstr)
	{
		Branch br = { firstInstr, stackHeight };
		for (uint32_t i = 0; i < stackHeight; i++)
			br.stack[i].flags = StackEntry::IN_USE;
		branches.push(br);
	}

	virtual int32_t DequeueBranch()
	{
		branches.pop();
		if (branches.size() > 0)
			return branches.front().firstInstr;
		else
			return -1;
	}

	virtual bool ApplyStackChange(instr::StackChange change)
	{
		Branch &cur = branches.front();
		assert(cur.stackHeight - change.removed + change.added <= 8);
		if (cur.stackHeight < change.removed)
			return false; // Not enough values on stack

		cur.stackHeight -= change.removed;
		for (int i = 0; i < change.added; i++)
			cur.stack[cur.stackHeight + i].flags = StackEntry::IN_USE;
		cur.stackHeight += change.added;

		return true; // Yay!
	}

	static const int MaxStack = 8;
};

class LargeStackManager : public StackManager
{
private:
	class Branch
	{
	public:
		int32_t firstInstr;
		uint32_t maxStack;
		uint32_t stackHeight;
		StackEntry *stack;

		inline Branch() : firstInstr(-1), maxStack(0), stackHeight(0), stack(nullptr)
		{ }
		inline Branch(const int32_t firstInstr, uint32_t maxStack) :
			firstInstr(firstInstr), maxStack(maxStack),
			stackHeight(0), stack(new StackEntry[maxStack])
		{ }
		inline Branch(const int32_t firstInstr, const Branch &other) :
			firstInstr(firstInstr), maxStack(other.maxStack),
			stackHeight(other.stackHeight), stack(new StackEntry[maxStack])
		{
			CopyMemoryT(this->stack, other.stack, other.maxStack);
		}
		inline Branch(const Branch &other) :
			firstInstr(other.firstInstr), maxStack(other.maxStack),
			stackHeight(other.stackHeight), stack(new StackEntry[other.maxStack])
		{
			CopyMemoryT(this->stack, other.stack, other.maxStack);
		}

		inline ~Branch()
		{
			delete[] stack;
		}

		inline friend void swap(Branch &one, Branch &two)
		{
			using std::swap;

			swap(one.firstInstr, two.firstInstr);
			swap(one.maxStack, two.maxStack);
			swap(one.stackHeight, two.stackHeight);
		}

		inline Branch &operator=(Branch other)
		{
			swap(*this, other);
			return *this;
		}
	};

	uint32_t maxStack;
	std::queue<Branch> branches;

public:
	LargeStackManager(uint32_t maxStack) :
		maxStack(maxStack)
	{
		branches.push(Branch());
	}

	virtual uint32_t GetStackHeight()
	{
		return branches.front().stackHeight;
	}

	virtual void EnqueueBranch(int32_t firstInstr)
	{
		Branch br(branches.front()); // Use the copy constructor! :D
		branches.push(br);
	}
	virtual void EnqueueBranch(uint32_t stackHeight, int32_t firstInstr)
	{
		Branch br = Branch(firstInstr, maxStack);
		br.stackHeight = stackHeight;
		for (uint32_t i = 0; i < stackHeight; i++)
			br.stack[i].flags = StackEntry::IN_USE;
		branches.push(br);
	}

	virtual int32_t DequeueBranch()
	{
		branches.pop();
		if (branches.size() > 0)
			return branches.front().firstInstr;
		else
			return -1;
	}

	virtual bool ApplyStackChange(instr::StackChange change)
	{
		Branch &cur = branches.front();
		assert(cur.stackHeight - change.removed + change.added <= maxStack);
		if (cur.stackHeight < change.removed)
			return false; // Not enough values on stack

		cur.stackHeight -= change.removed;
		for (int i = 0; i < change.added; i++)
			cur.stack[cur.stackHeight + i].flags = StackEntry::IN_USE;
		cur.stackHeight += change.added;

		return true; // Yay!
	}
};

void Thread::InitializeMethod(Method::Overload *method)
{
	using namespace instr;

	assert(!method->IsInitialized());

	MethodBuilder builder;

	// First, initialize all the instructions based on the original bytecode
	InitializeInstructions(builder, method);

	// Then, let's find all branch and switch instructions, so we can
	// update their branch targets. During this step, we also mark said
	// targets as having incoming branches.
	InitializeBranchOffsets(builder, method);

	// And now, we assign each instruction input and output offsets,
	// as appropriate. This step may also rewrite the method somewhat,
	// removing instructions for optimisation purposes and changing
	// some LocalOffsets from stack offsets to locals.
	if (method->maxStack <= SmallStackManager::MaxStack)
	{
		SmallStackManager stack;
		CalculateStackHeights(builder, method, stack);
	}
	else
	{
		LargeStackManager stack(method->maxStack);
		CalculateStackHeights(builder, method, stack);
	}

	WriteInitializedBody(builder, method);


	if (builder.GetTypeCount() > 0)
		CallStaticConstructors(builder);
}

void Thread::InitializeBranchOffsets(instr::MethodBuilder &builder, Method::Overload *method)
{
	using namespace instr;
	typedef Method::TryBlock::TryKind TryKind;

	if (builder.HasBranches())
		for (int32_t i = 0; i < builder.GetLength(); i++)
		{
			Instruction *instruction = builder[i];
			if (instruction->IsBranch())
			{
				Branch *br = static_cast<Branch*>(instruction);
				br->target = builder.FindIndex(builder.GetOriginalOffset(i) + builder.GetOriginalSize(i) + br->target);
				if (br->target == -1)
					throw MethodInitException("Invalid branch offset.", method, i,
						MethodInitException::INVALID_BRANCH_OFFSET);
				builder[br->target]->AddBranch();
			}
			else if (instruction->IsSwitch())
			{
				Switch *sw = static_cast<Switch*>(instruction);
				for (int32_t t = 0; t < sw->targetCount; t++)
				{
					int32_t *target = sw->targets + t;
					*target = builder.FindIndex(builder.GetOriginalOffset(i) + builder.GetOriginalSize(i) + *target);
					if (*target == -1)
						throw MethodInitException("Invalid branch offset.", method, i,
							MethodInitException::INVALID_BRANCH_OFFSET);
					builder[*target]->AddBranch();
				}
			}
		}

		for (int32_t i = 0; i < method->tryBlockCount; i++)
		{
			Method::TryBlock *tryBlock = method->tryBlocks + i;
			tryBlock->tryStart = builder.FindIndex(tryBlock->tryStart);
			tryBlock->tryEnd = builder.FindIndex(tryBlock->tryEnd);

			if (tryBlock->kind == TryKind::CATCH)
			{
				for (int32_t c = 0; c < tryBlock->catches.count; c++)
				{
					Method::CatchBlock *catchBlock = tryBlock->catches.blocks + c;
					if (catchBlock->caughtType == nullptr)
						catchBlock->caughtType = TypeFromToken(method, catchBlock->caughtTypeId);
					catchBlock->catchStart = builder.FindIndex(catchBlock->catchStart);
					catchBlock->catchEnd = builder.FindIndex(catchBlock->catchEnd);
				}
			}
			else if (tryBlock->kind == TryKind::FINALLY)
			{
				tryBlock->finallyBlock.finallyStart = builder.FindIndex(tryBlock->finallyBlock.finallyStart);
				tryBlock->finallyBlock.finallyEnd = builder.FindIndex(tryBlock->finallyBlock.finallyEnd);
			}
		}
}

void Thread::CalculateStackHeights(instr::MethodBuilder &builder, Method::Overload *method, StackManager &stack)
{
	using namespace instr;
	typedef Method::TryBlock::TryKind TryKind;

	// The first instruction is always reachable
	stack.EnqueueBranch(0, 0);

	// If the method has any try blocks, we must add the first instruction
	// of each catch and finally as a branch, because they will never be
	// reached by fallthrough or branching.
	for (int32_t i = 0; i < method->tryBlockCount; i++)
	{
		Method::TryBlock &tryBlock = method->tryBlocks[i];
		if (tryBlock.kind == TryKind::CATCH)
		{
			for (int32_t c = 0; c < tryBlock.catches.count; c++)
				stack.EnqueueBranch(1, tryBlock.catches.blocks[c].catchStart);
		}
		else if (tryBlock.kind == TryKind::FINALLY)
			stack.EnqueueBranch(0, tryBlock.finallyBlock.finallyStart);
	}

	int32_t index;
	while ((index = stack.DequeueBranch()) != -1)
	{
		Instruction *prev = nullptr;
		while (true)
		{
			Instruction *instr = builder[index];
			if (builder.GetStackHeight(index) >= 0)
			{
				if (builder.GetStackHeight(index) != stack.GetStackHeight())
					throw MethodInitException("Instruction reached with different stack heights.",
						method, index, MethodInitException::INCONSISTENT_STACK_HEIGHT);
				break; // This branch has already been visited!
			}
			else
				builder.SetStackHeight(index, stack.GetStackHeight());

			{ // Update input/output
				StackChange sc = instr->GetStackChange();
				if (sc.removed > 0 || instr->HasInput())
				{
					// If:
					//   1. there is a previous instruction
					//   2. prev has an output
					//   3. prev added exactly one value to the stack, or is dup
					//   4. instr has no incoming branches
					//   5. instr is a StoreLocal
					// then we can update prev to point directly to the local variable,
					// thus avoiding the stack altogether.
					// If #1�4 are true, but instr is a pop, then we can similarly update
					// prev's output to discard the result.
					bool canUpdatePrev = prev != nullptr &&
						prev->HasOutput() &&
						(prev->GetStackChange().added == 1 || prev->IsDup()) &&
						!instr->HasBranches();

					if (canUpdatePrev && instr->IsStoreLocal())
					{
						prev->UpdateOutput(static_cast<StoreLocal*>(instr)->target, false);
						builder.MarkForRemoval(index);
					}
					else if (canUpdatePrev && instr->opcode == OPI_POP)
					{
						// Write the result to the stack, but pretend it's not on the stack.
						// (This won't increment the stack height)
						prev->UpdateOutput(method->GetStackOffset(stack.GetStackHeight() - 1), false);
						builder.MarkForRemoval(index);
					}
					else
					{
						// If:
						//   1. there is a previous instruction
						//   2. prev is a LoadLocal
						//   3. instr removes exactly one value from the stack
						//   4. instr has an input that is not required to be on the stack
						//   5. neither prev nor instr has incoming branches
						// then we can update instr to take the input directly from prev's local,
						// and remove prev.
						// Note: we don't have to test sc.removed == 1 here, because RequiresStackInput()
						// always returns true if the instruction uses more than one value.
						if (prev != nullptr && prev->IsLoadLocal() && !prev->HasBranches() &&
							instr->HasInput() && !instr->RequiresStackInput() && !instr->HasBranches())
						{
							instr->UpdateInput(static_cast<LoadLocal*>(prev)->source, false);
							// prev should be nulled after branching, even unconditionally,
							// so this is fine.
							builder.MarkForRemoval(index - 1);
						}
						else
							instr->UpdateInput(method->GetStackOffset(stack.GetStackHeight() - sc.removed), true);
					}
				}

				if (instr->HasOutput())
				{
					instr->UpdateOutput(method->GetStackOffset(stack.GetStackHeight() - sc.removed), true);
				}

				if (!stack.ApplyStackChange(sc))
					throw MethodInitException("There are not enough values on the stack.",
						method, index, MethodInitException::INSUFFICIENT_STACK_HEIGHT);
			} // End update input/output

			if (instr->IsBranch())
			{
				Branch *const br = static_cast<Branch*>(instr);
				if (br->IsConditional())
				{
					stack.EnqueueBranch(br->target); // Use the same stack
					if (prev && !br->HasBranches() &&
						// Is the previous instruction ==, <, >, <= or >=?
						((prev->opcode & ~1) == OPI_EQ_L ||
						prev->opcode >= OPI_LT_L && prev->opcode <= OPI_GTE_S) &&
						// And is this a brfalse or brtrue?
						br->opcode >= OPI_BRFALSE_L && br->opcode <= OPI_BRTRUE_S)
					{
						// Great! Then we can turn the previous instruction into a
						// brlt/brgt/brlte/brgte as required.
						IntermediateOpcode newOpcode;
						if (br->opcode == OPI_BRTRUE_L || br->opcode == OPI_BRTRUE_S)
						{
							// eq, brtrue  => breq
							// lt, brtrue  => brlt
							// gt, brtrue  => brgt
							// lte, brtrue => brlte
							// gte, brtrue => brgte
							// eq/lt/gt/lte/gte are in the same order both in the operator
							// section and the extended branch section, so we can just
							// subtract OPI_EQ_L from prev->opcode, divide the result by
							// two, and add it to OPI_BREQ.
							// There is a one-instruction gap between OPI_EQ_* and OPI_LT_*,
							// where OPI_CMP_* is, but fortunately that gap is covered by
							// OPI_BRNEQ, which is not produced by this code.
							newOpcode = (IntermediateOpcode)(OPI_BRLT + (prev->opcode - OPI_LT_L) / 2);
							switch (prev->opcode)
							{
							case OPI_EQ_L:  case OPI_EQ_S:  newOpcode = OPI_BREQ;  break;
							case OPI_LT_L:  case OPI_LT_S:  newOpcode = OPI_BRLT;  break;
							case OPI_GT_L:  case OPI_GT_S:  newOpcode = OPI_BRGT;  break;
							case OPI_LTE_L: case OPI_LTE_S: newOpcode = OPI_BRLTE; break;
							case OPI_GTE_L: case OPI_GTE_S: newOpcode = OPI_BRGTE; break;
							default: newOpcode = OPI_NOP; break; // Oh dear
							}
						}
						else
						{
							// lt, brfalse  => brgte
							// gt, brfalse  => brlte
							// lte, brfalse => brgt
							// gte, brfalse => brlt
							// For simplicity, we've defined some aliases for these:
							switch (prev->opcode)
							{
							case OPI_EQ_L:  case OPI_EQ_S:  newOpcode = OPI_BRNEQ;  break;
							case OPI_LT_L:  case OPI_LT_S:  newOpcode = OPI_BRNLT;  break;
							case OPI_GT_L:  case OPI_GT_S:  newOpcode = OPI_BRNGT;  break;
							case OPI_LTE_L: case OPI_LTE_S: newOpcode = OPI_BRNLTE; break;
							case OPI_GTE_L: case OPI_GTE_S: newOpcode = OPI_BRNGTE; break;
							default: newOpcode = OPI_NOP; break; // Oh dear
							}
						}
						assert(newOpcode != OPI_NOP);

						// Set the previous instruction to the new comparison thing
						// (This also deletes the Instruction*)
						builder.SetInstruction(index - 1,
							new BranchComparison(static_cast<ExecOperator*>(prev)->args,
								br->target, newOpcode),
							/*deletePrev:*/ true);
						// Mark this instruction for removal
						builder.MarkForRemoval(index);
					}
				}
				else
				{
					prev = nullptr;
					index = br->target; // Continue at the target instruction
					continue; // don't increment index
				}
			}
			else if (instr->IsSwitch())
			{
				Switch *sw = static_cast<Switch*>(instr);
				for (uint32_t i = 0; i < sw->targetCount; i++)
					stack.EnqueueBranch(sw->targets[i]); // Use the same stack
			}
			else if (instr->opcode == OPI_RET || instr->opcode == OPI_RETNULL ||
				instr->opcode == OPI_THROW || instr->opcode == OPI_RETHROW ||
				instr->opcode == OPI_ENDFINALLY)
				break; // This branch has terminated.

			prev = instr;
			index++;
		}
	}

	// Remove instructions that are now unnecessary! :D
	builder.PerformRemovals(method);
}

void Thread::WriteInitializedBody(instr::MethodBuilder &builder, Method::Overload *method)
{
	using namespace instr;
	typedef Method::TryBlock::TryKind TryKind;

	// Let's allocate a buffer for the output, yay!
	std::unique_ptr<uint8_t[]> buffer(new uint8_t[builder.GetByteSize()]);
	char *pbuffer = (char*)buffer.get();
	for (int32_t i = 0; i < builder.GetLength(); i++)
	{
		Instruction *instr = builder[i];
		instr->WriteBytes(pbuffer, builder);
		pbuffer += instr->GetSize();
	}

	for (int32_t t = 0; t < method->tryBlockCount; t++)
	{
		Method::TryBlock &tryBlock = method->tryBlocks[t];
		
		tryBlock.tryStart = builder[tryBlock.tryStart]->offset;
		tryBlock.tryEnd = builder[tryBlock.tryEnd]->offset;

		if (tryBlock.kind == TryKind::CATCH)
		{
			for (int32_t c = 0; c < tryBlock.catches.count; c++)
			{
				Method::CatchBlock &catchBlock = tryBlock.catches.blocks[c];
				catchBlock.catchStart = builder[catchBlock.catchStart]->offset;
				catchBlock.catchEnd = builder[catchBlock.catchEnd]->offset;
			}
		}
		else if (tryBlock.kind == TryKind::FINALLY)
		{
			tryBlock.finallyBlock.finallyStart = builder[tryBlock.finallyBlock.finallyStart]->offset;
			tryBlock.finallyBlock.finallyEnd = builder[tryBlock.finallyBlock.finallyEnd]->offset;
		}
	}

	delete[] method->entry;
	method->entry  = buffer.release();
	method->length = builder.GetByteSize();
	method->flags  = method->flags | MethodFlags::INITED;
}

void Thread::CallStaticConstructors(instr::MethodBuilder &builder)
{
	for (int32_t i = 0; i < builder.GetTypeCount(); i++)
	{
		Type *type = builder.GetType(i);
		// The static constructor may have been triggered by a previous type initialization,
		// so we must test the flag again
		if ((type->flags & TypeFlags::STATIC_CTOR_RUN) == TypeFlags::NONE)
		{
			type->flags |= TypeFlags::STATIC_CTOR_RUN; // prevent infinite recursion
			type->InitStaticFields(); // Get some storage locations for the static fields
			Member *member = type->GetMember(static_strings::_init);
			if (member)
			{
				// If there is a member '.init', it must be a method!
				assert((member->flags & MemberFlags::METHOD) == MemberFlags::METHOD);

				Method::Overload *mo = ((Method*)member)->ResolveOverload(0);
				if (!mo) ThrowNoOverloadError(0);

				Value ignore;
				InvokeMethodOverload(mo, 0,
					currentFrame->evalStack + currentFrame->stackCount,
					&ignore);
			}
		}
	}
}

void Thread::InitializeInstructions(instr::MethodBuilder &builder, Method::Overload *method)
{
	using namespace instr;

	register uint8_t *ip = method->entry;
	uint8_t *end = method->entry + method->length;

	Module *module = method->group->declModule;

	while (ip < end)
	{
		Opcode *opc = (Opcode*)ip;
		ip++; // Always skip opcode
		Instruction *instr = nullptr;
		switch (*opc)
		{
		case OPC_NOP:
			instr = new SimpleInstruction(OPI_NOP, StackChange::empty);
			break;
		case OPC_DUP:
			instr = new DupInstr();
			break;
		case OPC_POP:
			instr = new SimpleInstruction(OPI_POP, StackChange(1, 0)); 
			break;
		// Arguments
		case OPC_LDARG_0:
		case OPC_LDARG_1:
		case OPC_LDARG_2:
		case OPC_LDARG_3:
			instr = new LoadLocal(method->GetArgumentOffset(*opc - OPC_LDARG_0));
			break;
		case OPC_LDARG_S: // ub:n
			instr = new LoadLocal(method->GetArgumentOffset(*ip++));
			break;
		case OPC_LDARG: // u2:n
			{
				uint16_t arg = U16_ARG(ip);
				ip += sizeof(uint16_t);
				instr = new LoadLocal(method->GetArgumentOffset(arg));
			}
			break;
		case OPC_STARG_S: // ub:n
			instr = new StoreLocal(method->GetArgumentOffset(*ip++));
			break;
		case OPC_STARG: // u2:n
			{
				uint16_t arg = U16_ARG(ip);
				ip += sizeof(uint16_t);
				instr = new StoreLocal(method->GetArgumentOffset(arg));
			}
			break;
		// Locals
		case OPC_LDLOC_0:
		case OPC_LDLOC_1:
		case OPC_LDLOC_2:
		case OPC_LDLOC_3:
			instr = new LoadLocal(method->GetLocalOffset(*opc - OPC_LDLOC_0));
			break;
		case OPC_STLOC_0:
		case OPC_STLOC_1:
		case OPC_STLOC_2:
		case OPC_STLOC_3:
			instr = new StoreLocal(method->GetLocalOffset(*opc - OPC_STLOC_0));
			break;
		case OPC_LDLOC_S: // ub:n
			instr = new LoadLocal(method->GetLocalOffset(*ip++));
			break;
		case OPC_LDLOC: // u2:n
			{
				uint16_t loc = U16_ARG(ip);
				ip += sizeof(uint16_t);
				instr = new LoadLocal(method->GetLocalOffset(loc));
			}
			break;
		case OPC_STLOC_S: // ub:n
			instr = new StoreLocal(method->GetLocalOffset(*ip++));
			break;
		case OPC_STLOC: // u2:n
			{
				uint16_t loc = U16_ARG(ip);
				ip += sizeof(uint16_t);
				instr = new StoreLocal(method->GetLocalOffset(loc));
			}
			break;
		// Values and object initialisation
		case OPC_LDNULL:
			instr = new LoadNull();
			break;
		case OPC_LDFALSE:
			instr = new LoadBoolean(false);
			break;
		case OPC_LDTRUE:
			instr = new LoadBoolean(true);
			break;
		case OPC_LDC_I_M1:
		case OPC_LDC_I_0:
		case OPC_LDC_I_1:
		case OPC_LDC_I_2:
		case OPC_LDC_I_3:
		case OPC_LDC_I_4:
		case OPC_LDC_I_5:
		case OPC_LDC_I_6:
		case OPC_LDC_I_7:
		case OPC_LDC_I_8:
			instr = new LoadInt((int)*opc - OPC_LDC_I_0);
			break;
		case OPC_LDC_I_S: // sb:value
			instr = new LoadInt(*(int8_t*)ip);
			ip++;
			break;
		case OPC_LDC_I_M: // i4:value
			instr = new LoadInt(I32_ARG(ip));
			ip += sizeof(int32_t);
			break;
		case OPC_LDC_I: // i8:value
			instr = new LoadInt(I64_ARG(ip));
			ip += sizeof(int64_t);
			break;
		case OPC_LDC_U: // u8:value
			instr = new LoadUInt(U64_ARG(ip));
			ip += sizeof(uint64_t);
			break;
		case OPC_LDC_R: // r8:value
			instr = new LoadReal(*(double*)ip);
			ip += sizeof(double);
			break;
		case OPC_LDSTR: // u4:str
			{
				String *str = StringFromToken(method, U32_ARG(ip));
				ip += sizeof(uint32_t);

				instr = new LoadString(str);
			}
			break;
		case OPC_LDARGC:
			instr = new LoadArgCount();
			break;
		case OPC_LDENUM_S: // u4:type  i4:value
			{
				Type *type = TypeFromToken(method, U32_ARG(ip));
				ip += sizeof(uint32_t);

				int32_t value = I32_ARG(ip);
				ip += sizeof(int32_t);
				instr = new LoadEnumValue(type, value);
			}
			break;
		case OPC_LDENUM: // u4:type  i8:value
			{
				Type *type = TypeFromToken(method, U32_ARG(ip));
				ip += sizeof(uint32_t);

				int64_t value = I64_ARG(ip);
				ip += sizeof(int64_t);
				instr = new LoadEnumValue(type, value);
			}
			break;
		case OPC_NEWOBJ_S: // u4:type, ub:argc
			{
				Type *type = TypeFromToken(method, U32_ARG(ip));
				ip += sizeof(uint32_t);
				uint16_t argCount = *ip;
				ip++;
				instr = new NewObject(type, argCount);
			}
			break;
		case OPC_NEWOBJ: // u4:type, u2:argc
			{
				Type *type = TypeFromToken(method, U32_ARG(ip));
				ip += sizeof(uint32_t);
				uint16_t argCount = U16_ARG(ip);
				ip += sizeof(uint16_t);
				instr = new NewObject(type, argCount);
			}
			break;
		// Invocation
		case OPC_CALL_0:
		case OPC_CALL_1:
		case OPC_CALL_2:
		case OPC_CALL_3:
			instr = new Call(*opc - OPC_CALL_0);
			break;
		case OPC_CALL_S: // ub:argc
			instr = new Call(*ip++);
			break;
		case OPC_CALL: // u2:argc
			{
				uint16_t argCount = U16_ARG(ip);
				ip += sizeof(uint16_t);
				instr = new Call(argCount);
			}
			break;
		case OPC_SCALL_S: // u4:func  ub:argc
			{
				TokenId funcId = U32_ARG(ip);
				ip += sizeof(uint32_t);
				
				uint16_t argCount = *ip++;

				Method::Overload *mo = MethodOverloadFromToken(method, funcId, argCount);
				instr = new StaticCall(argCount - mo->InstanceOffset(), mo);
			}
			break;
		case OPC_SCALL: // u4:func  u2:argc
			{
				TokenId funcId = U32_ARG(ip);
				ip += sizeof(uint32_t);

				uint16_t argCount = U16_ARG(ip);
				ip += sizeof(uint16_t);

				Method::Overload *mo = MethodOverloadFromToken(method, funcId, argCount);
				instr = new StaticCall(argCount - mo->InstanceOffset(), mo);
			}
			break;
		case OPC_APPLY:
			instr = new Apply();
			break;
		case OPC_SAPPLY: // u4:func
			{
				Method *func = MethodFromToken(method, U32_ARG(ip));
				ip += sizeof(uint32_t);
				instr = new StaticApply(func);
			}
			break;
		// Control flow
		case OPC_RETNULL:
			instr = new SimpleInstruction(OPI_RETNULL, StackChange::empty);
			break;
		case OPC_RET:
			instr = new SimpleInstruction(OPI_RET, StackChange(1, 0));
			break;
		case OPC_BR_S: // sb:trg
			instr = new Branch(*(int8_t*)ip++, /*isLeave:*/ false);
			break;
		case OPC_BRNULL_S: // sb:trg
			instr = new ConditionalBranch(*(int8_t*)ip++, ConditionalBranch::IF_NULL);
			break;
		case OPC_BRINST_S: // sb:trg
			instr = new ConditionalBranch(*(int8_t*)ip++, ConditionalBranch::NOT_NULL);
			break;
		case OPC_BRFALSE_S: // sb:trg
			instr = new ConditionalBranch(*(int8_t*)ip++, ConditionalBranch::IF_FALSE);
			break;
		case OPC_BRTRUE_S: // sb:trg
			instr = new ConditionalBranch(*(int8_t*)ip++, ConditionalBranch::IF_TRUE);
			break;
		case OPC_BRREF_S: // sb:trg		(even)
		case OPC_BRNREF_S: // sb:trg	(odd)
			instr = new BranchIfReference(*(int8_t*)ip++, /*branchIfSame:*/ (*opc & 1) == 0);
			break;
		case OPC_BRTYPE_S: // u4:type  sb:trg
			{
				Type *type = TypeFromToken(method, U32_ARG(ip));
				ip += sizeof(uint32_t);
				instr = new BranchIfType(*(int8_t*)ip++, type);
			}
			break;
		case OPC_BR: // i4:trg
			{
				int32_t target = I32_ARG(ip);
				ip += sizeof(int32_t);
				instr = new Branch(target, /*isLeave:*/ false);
			}
			break;
		case OPC_BRNULL: // i4:trg
			{
				int32_t target = I32_ARG(ip);
				ip += sizeof(int32_t);
				instr = new ConditionalBranch(target, ConditionalBranch::IF_NULL);
			}
			break;
		case OPC_BRINST: // i4:trg
			{
				int32_t target = I32_ARG(ip);
				ip += sizeof(int32_t);
				instr = new ConditionalBranch(target, ConditionalBranch::NOT_NULL);
			}
			break;
		case OPC_BRFALSE: // i4:trg
			{
				int32_t target = I32_ARG(ip);
				ip += sizeof(int32_t);
				instr = new ConditionalBranch(target, ConditionalBranch::IF_FALSE);
			}
			break;
		case OPC_BRTRUE: // i4:trg
			{
				int32_t target = I32_ARG(ip);
				ip += sizeof(int32_t);
				instr = new ConditionalBranch(target, ConditionalBranch::IF_TRUE);
			}
			break;
		case OPC_BRREF: // i4:trg		(even)
		case OPC_BRNREF: // i4:trg		(odd)
			{
				int32_t target = I32_ARG(ip);
				ip += sizeof(int32_t);
				instr = new BranchIfReference(target, /*branchIfSame:*/ (*opc & 1) == 0);
			}
			break;
		case OPC_BRTYPE: // u4:type  i4:trg
			{
				Type *type = TypeFromToken(method, U32_ARG(ip));
				ip += sizeof(uint32_t);
				int32_t target = I32_ARG(ip);
				ip += sizeof(int32_t);
				instr = new BranchIfType(target, type);
			}
			break;
		case OPC_SWITCH_S: // u2:n  sb:targets...
			{
				uint16_t count = U16_ARG(ip);
				ip += sizeof(uint16_t);
				std::unique_ptr<int32_t[]> targets(new int32_t[count]);
				for (int i = 0; i < count; i++)
					targets[i] = *(int8_t*)ip++;
				instr = new Switch(count, targets.release());
			}
			break;
		case OPC_SWITCH: // u2:n  i4:targets...
			{
				uint16_t count = U16_ARG(ip);
				ip += sizeof(uint16_t);
				std::unique_ptr<int32_t[]> targets(new int32_t[count]);
				CopyMemoryT(targets.get(), (int32_t*)ip, count);
				instr = new Switch(count, targets.release());
			}
			break;
		// Operators
		case OPC_ADD:
		case OPC_SUB:
		case OPC_OR:
		case OPC_XOR:
		case OPC_MUL:
		case OPC_DIV:
		case OPC_MOD:
		case OPC_AND:
		case OPC_POW:
		case OPC_SHL:
		case OPC_SHR:
		case OPC_HASHOP:
		case OPC_DOLLAR:
		case OPC_PLUS:
		case OPC_NEG:
		case OPC_NOT:
		case OPC_EQ:
		case OPC_CMP:
			instr = new ExecOperator((Operator)(*opc - OPC_ADD));
			break;
		case OPC_LT:
			instr = new ExecOperator(ExecOperator::CMP_LT);
			break;
		case OPC_GT:
			instr = new ExecOperator(ExecOperator::CMP_GT);
			break;
		case OPC_LTE:
			instr = new ExecOperator(ExecOperator::CMP_LTE);
			break;
		case OPC_GTE:
			instr = new ExecOperator(ExecOperator::CMP_GTE);
			break;
		case OPC_CONCAT:
			instr = new ExecOperator(ExecOperator::CONCAT);
			break;
		// Misc. data
		case OPC_LIST_0:
			instr = new CreateList(0);
			break;
		case OPC_LIST_S: // ub:count
			instr = new CreateList(*ip);
			ip++;
			break;
		case OPC_LIST: // u4:count
			instr = new CreateList(U32_ARG(ip));
			ip += sizeof(uint32_t);
			break;
		case OPC_HASH_0:
			instr = new CreateHash(0);
			break;
		case OPC_HASH_S: // ub:count
			instr = new CreateHash(*ip);
			ip++;
			break;
		case OPC_HASH: // u4:count
			instr = new CreateHash(U32_ARG(ip));
			ip += sizeof(uint32_t);
			break;
		case OPC_LDITER:
			instr = new LoadIterator();
			break;
		case OPC_LDTYPE:
			instr = new LoadType();
			break;
		// Fields
		case OPC_LDFLD: // u4:fld
			{
				Field *field = FieldFromToken(method, U32_ARG(ip), false);
				ip += sizeof(uint32_t);
				instr = new LoadField(field);
			}
			break;
		case OPC_STFLD: // u4:fld
			{
				Field *field = FieldFromToken(method, U32_ARG(ip), false);
				ip += sizeof(uint32_t);
				instr = new StoreField(field);
			}
			break;
		case OPC_LDSFLD: // u4:fld
			{
				Field *field = FieldFromToken(method, U32_ARG(ip), true);
				ip += sizeof(uint32_t);
				instr = new instr::LoadStaticField(field);

				builder.AddTypeToInitialize(field->declType);
			}
			break;
		case OPC_STSFLD: // u4:fld
			{
				Field *field = FieldFromToken(method, U32_ARG(ip), true);
				ip += sizeof(uint32_t);
				instr = new instr::StoreStaticField(field);

				builder.AddTypeToInitialize(field->declType);
			}
			break;
		// Named member access
		case OPC_LDMEM: // u4:name
			{
				String *name = StringFromToken(method, U32_ARG(ip));
				ip += sizeof(uint32_t);
				instr = new instr::LoadMember(name);
			}
			break;
		case OPC_STMEM: // u4:name
			{
				String *name = StringFromToken(method, U32_ARG(ip));
				ip += sizeof(uint32_t);
				instr = new instr::StoreMember(name);
			}
			break;
		// Indexers
		case OPC_LDIDX_1:
			instr = new instr::LoadIndexer(1);
			break;
		case OPC_LDIDX_S: // ub:argc
			instr = new instr::LoadIndexer(*ip++);
			break;
		case OPC_LDIDX: // u2:argc
			instr = new instr::LoadIndexer(U16_ARG(ip));
			ip += sizeof(uint16_t);
			break;
		case OPC_STIDX_1:
			instr = new instr::StoreIndexer(1);
			break;
		case OPC_STIDX_S: // ub:argc
			instr = new instr::StoreIndexer(*ip++);
			break;
		case OPC_STIDX: // u2:argc
			instr = new instr::StoreIndexer(U16_ARG(ip));
			ip += sizeof(uint16_t);
			break;
		// Global/static functions
		case OPC_LDSFN: // u4:func
			{
				Method *func = MethodFromToken(method, U32_ARG(ip));
				ip += sizeof(uint32_t);
				instr = new LoadStaticFunction(func);
			}
			break;
		// Type tokens
		case OPC_LDTYPETKN: // u4:type
			{
				Type *type = TypeFromToken(method, U32_ARG(ip));
				ip += sizeof(uint32_t);
				instr = new LoadTypeToken(type);
			}
			break;
		// Exception handling
		case OPC_THROW:
			instr = new SimpleInstruction(OPI_THROW, StackChange(1, 0));
			break;
		case OPC_RETHROW:
			instr = new SimpleInstruction(OPI_RETHROW, StackChange::empty);
			break;
		case OPC_LEAVE_S: // sb:trg
			instr = new Branch(*(int8_t*)ip, true);
			ip++;
			break;
		case OPC_LEAVE: // i4:trg
			{
				int32_t target = I32_ARG(ip);
				ip += sizeof(int32_t);
				instr = new Branch(target, true);
			}
			break;
		case OPC_ENDFINALLY:
			instr = new SimpleInstruction(OPI_ENDFINALLY, StackChange::empty);
			break;
		// Call member
		case OPC_CALLMEM_S: // u4:name  ub:argc
			{
				String *name = StringFromToken(method, U32_ARG(ip));
				ip += sizeof(uint32_t);
				instr = new CallMember(name, *ip);
				ip++;
			}
			break;
		case OPC_CALLMEM: // u4:name  u2:argc
			{
				String *name = StringFromToken(method, U32_ARG(ip));
				ip += sizeof(uint32_t);
				instr = new CallMember(name, U16_ARG(ip));
				ip += sizeof(uint16_t);
			}
			break;
		default:
			throw L"Invalid opcode encountered";
		}
		builder.Append((uint32_t)((char*)opc - (char*)method->entry), (uint32_t)((char*)ip - (char*)opc), instr);
	}
}

Type *Thread::TypeFromToken(Method::Overload *fromMethod, uint32_t token)
{
	Type *result = fromMethod->group->declModule->FindType(token);
	if (!result)
		throw MethodInitException("Unresolved TypeDef or TypeRef token ID.",
			fromMethod, token, MethodInitException::UNRESOLVED_TOKEN_ID);

	if ((result->flags & TypeFlags::PROTECTION) == TypeFlags::PRIVATE &&
		result->module != fromMethod->group->declModule)
		throw MethodInitException("The type is not accessible from other modules.",
			fromMethod, result, MethodInitException::INACCESSIBLE_TYPE);

	return result;
}

String *Thread::StringFromToken(Method::Overload *fromMethod, uint32_t token)
{
	String *result = fromMethod->group->declModule->FindString(token);
	if (!result)
		throw MethodInitException("Unresolved String token ID.",
			fromMethod, token, MethodInitException::UNRESOLVED_TOKEN_ID);

	return result;
}

Method *Thread::MethodFromToken(Method::Overload *fromMethod, uint32_t token)
{
	Method *result = fromMethod->group->declModule->FindMethod(token);
	if (!result)
		throw MethodInitException("Unresolved MethodDef, MethodRef, FunctionDef or FunctionRef token ID.",
			fromMethod, token, MethodInitException::UNRESOLVED_TOKEN_ID);

	if (result->IsStatic())
		if (result->declType ?
			!result->IsAccessible(nullptr, fromMethod->declType) :
			(result->flags & MemberFlags::ACCESS_LEVEL) == MemberFlags::PRIVATE &&
			result->declModule != fromMethod->group->declModule)
			throw MethodInitException("The method is inaccessible from this location.",
				fromMethod, result, MethodInitException::INACCESSIBLE_MEMBER);

	return result;
}

Method::Overload *Thread::MethodOverloadFromToken(Method::Overload *fromMethod, uint32_t token, uint32_t argCount)
{
	Method *method = MethodFromToken(fromMethod, token);

	argCount -= (int)(method->flags & MemberFlags::INSTANCE) >> 10;

	Method::Overload *overload = method->ResolveOverload(argCount);
	if (!overload)
		throw MethodInitException("Could not find a overload that takes the specified number of arguments.",
			fromMethod, method, argCount, MethodInitException::NO_MATCHING_OVERLOAD);

	return overload;
}

Field *Thread::FieldFromToken(Method::Overload *fromMethod, uint32_t token, bool shouldBeStatic)
{
	Field *field = fromMethod->group->declModule->FindField(token);
	if (!field)
		throw MethodInitException("Unresolved FieldDef or FieldRef token ID.",
			fromMethod, token, MethodInitException::UNRESOLVED_TOKEN_ID);

	if (field->IsStatic() && !field->IsAccessible(nullptr, fromMethod->declType))
		throw MethodInitException("The field is inaccessible from this location.",
			fromMethod, field, MethodInitException::INACCESSIBLE_MEMBER);

	if (shouldBeStatic != field->IsStatic())
		throw MethodInitException(shouldBeStatic ? "The field must be static." : "The field must be an instance field.",
			fromMethod, field, MethodInitException::FIELD_STATIC_MISMATCH);

	return field;
}