#include "methodbuilder.h"
#include "thread.opcodes.h"
#include "../object/type.h"
#include "../ee/instructions.h"
#include "../debug/debugsymbols.h"

namespace ovum
{

namespace instr
{
	MethodBuilder::MethodBuilder() :
		lastOffset(0),
		hasBranches(false)
	{ }

	MethodBuilder::~MethodBuilder()
	{
		for (auto &i : instructions)
			delete i.instr;
	}

	uint32_t MethodBuilder::GetOriginalOffset(size_t index) const
	{
		if (index >= instructions.size())
			return instructions.back().originalOffset +
				(uint32_t)instructions.back().originalSize;
		return instructions[index].originalOffset;
	}

	size_t MethodBuilder::GetOriginalSize(size_t index) const
	{
		if (index >= instructions.size())
			return 0;
		return instructions[index].originalSize;
	}

	size_t MethodBuilder::FindIndex(uint32_t originalOffset) const
	{
		size_t iMin = 0; // inclusive
		size_t iMax = instructions.size() - 1; // inclusive
		while (iMax >= iMin)
		{
			size_t iMid = (iMin + iMax) / 2;
			uint32_t midOffset = instructions[iMid].originalOffset;
			if (originalOffset < midOffset)
				// Search lower half
				iMax = iMid - 1;
			else if (originalOffset > midOffset)
				iMin = iMid + 1;
			else
				return iMid;
		}

		// try, catch and finally blocks may reference an offset
		// beyond the last instruction.
		return instructions.size();
	}

	size_t MethodBuilder::GetNewOffset(size_t index) const
	{
		typedef std::vector<InstrDesc>::const_iterator const_iter;
		if (index >= instructions.size())
		{
			Instruction *end = instructions.back().instr;
			return end->offset + end->GetSize();
		}
		return instructions[index].instr->offset;
	}

	int32_t MethodBuilder::GetJumpOffset(size_t index, const Instruction *relativeTo) const
	{
		size_t offset;
		if (index >= instructions.size())
		{
			Instruction *end = instructions.back().instr;
			offset = end->offset + end->GetSize();
		}
		else
		{
			offset = instructions[index].instr->offset;
		}
		return static_cast<int32_t>(
			offset - relativeTo->offset - relativeTo->GetSize()
		);
	}

	ovlocals_t MethodBuilder::GetStackHeight(size_t index) const
	{
		return instructions[index].stackHeight;
	}

	void MethodBuilder::SetStackHeight(size_t index, ovlocals_t stackHeight)
	{
		InstrDesc &instrDesc = instructions[index];
		OVUM_ASSERT(!instrDesc.removed);
		OVUM_ASSERT(instrDesc.stackHeight == UNVISITED);
		instrDesc.stackHeight = stackHeight;
	}

	uint32_t MethodBuilder::GetRefSignature(size_t index) const
	{
		return instructions[index].refSignature;
	}

	void MethodBuilder::SetRefSignature(size_t index, uint32_t refSignature)
	{
		instructions[index].refSignature = refSignature;
	}

	void MethodBuilder::Append(uint32_t originalOffset, size_t originalSize, Instruction *instr)
	{
		instructions.push_back(InstrDesc(originalOffset, originalSize, instr));
		instr->offset = lastOffset;
		lastOffset += instr->GetSize();
		hasBranches = hasBranches || instr->IsBranch() || instr->IsSwitch();
	}

	void MethodBuilder::SetInstruction(size_t index, Instruction *newInstr, bool deletePrev)
	{
		if (deletePrev)
			delete instructions[index].instr;
		instructions[index].instr = newInstr;
	}

	void MethodBuilder::MarkForRemoval(size_t index)
	{
		// Note: it is okay to remove instructions that have incoming branches;
		// the branch is simply forwarded to the next instruction.
		// Also note: previously removals were marked by setting stackHeight to -2.
		// This cannot be done, as we must preserve the known stack height in case
		// the instruction has incoming branches; otherwise we cannot verify that
		// it is reached with a consistent stack height on all branches.
		instructions[index].removed = true;
	}

	bool MethodBuilder::IsMarkedForRemoval(size_t index) const
	{
		return instructions[index].removed;
	}

	void MethodBuilder::PerformRemovals(MethodOverload *method)
	{
		using namespace std;
		const size_t SmallBufferSize = 64;

		if (instructions.size() < SmallBufferSize)
		{
			size_t newIncides[SmallBufferSize];
			PerformRemovalsInternal(newIncides, method);
		}
		else
		{
			Box<size_t[]> newIndices(new size_t[instructions.size() + 1]);
			PerformRemovalsInternal(newIndices.get(), method);
		}
	}

	void MethodBuilder::PerformRemovalsInternal(size_t newIndices[], MethodOverload *method)
	{
		this->lastOffset = 0; // Must recalculate byte offsets as well

		size_t oldIndex = 0, newIndex = 0;
		for (instr_iter i = instructions.begin(); i != instructions.end(); oldIndex++)
		{
			if (i->stackHeight == UNVISITED || i->removed)
			{
				// This instruction may have been the first instruction in a protected region,
				// or the target of a branch, in which case the next following instruction
				// becomes the first in that block, or the target of the branch.
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
		// try, catch and finally blocks may reference an index
		// beyond the last instruction
		newIndices[oldIndex] = newIndex;

		// If there are any branching instructions in the method, we must
		// update their targets, which we can only do AFTER performing all
		// the removals, since branching instructions may reference any
		// instruction in the method.
		if (hasBranches)
		{
			for (auto &i : instructions)
			{
				Instruction *instr = i.instr;
				if (instr->IsBranch())
				{
					Branch *br = static_cast<Branch*>(instr);
					br->target = JumpTarget::FromIndex(newIndices[br->target.index]);
				}
				else if (instr->IsSwitch())
				{
					Switch *sw = static_cast<Switch*>(instr);
					for (size_t t = 0; t < sw->targetCount; t++)
						sw->targets[t] = JumpTarget::FromIndex(newIndices[sw->targets[t].index]);
				}
			}
		}

		for (size_t t = 0; t < method->tryBlockCount; t++)
		{
			TryBlock *tryBlock = method->tryBlocks + t;
			tryBlock->tryStart = newIndices[tryBlock->tryStart];
			tryBlock->tryEnd = newIndices[tryBlock->tryEnd];

			switch (tryBlock->kind)
			{
			case TryKind::CATCH:
				for (size_t c = 0; c < tryBlock->catches.count; c++)
				{
					CatchBlock *catchBlock = tryBlock->catches.blocks + c;
					catchBlock->catchStart = newIndices[catchBlock->catchStart];
					catchBlock->catchEnd = newIndices[catchBlock->catchEnd];
				}
				break;
			case TryKind::FINALLY:
			case TryKind::FAULT: // uses finallyBlock
				tryBlock->finallyBlock.finallyStart = newIndices[tryBlock->finallyBlock.finallyStart];
				tryBlock->finallyBlock.finallyEnd = newIndices[tryBlock->finallyBlock.finallyEnd];
				break;
			}
		}

		if (method->debugSymbols)
		{
			debug::OverloadSymbols *debug = method->debugSymbols;
			size_t debugSymbolCount = debug->GetSymbolCount();
			for (size_t i = 0; i < debugSymbolCount; i++)
			{
				debug::DebugSymbol &sym = debug->GetSymbol(i);
				sym.startOffset = newIndices[sym.startOffset];
				sym.endOffset = newIndices[sym.endOffset];
			}
		}
	}

	void MethodBuilder::AddTypeToInitialize(Type *type)
	{
		if (type->HasStaticCtorRun())
			return;

		for (Type *t : typesToInitialize)
			if (t == type)
				return;

		typesToInitialize.push_back(type);
	}

	MethodBuffer::MethodBuffer(size_t size) :
		current(nullptr),
		buffer()
	{
		buffer.reset(new uint8_t[size]);
		current = buffer.get();
	}

	MethodBuffer::~MethodBuffer()
	{ }
} // namespace instr

} // namespace ovum
