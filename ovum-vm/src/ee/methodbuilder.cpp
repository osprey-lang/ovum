#include "methodbuilder.h"
#include "thread.opcodes.h"
#include "../object/type.h"
#include "../ee/instructions.h"
#include "../debug/debugsymbols.h"

namespace ovum
{

namespace instr
{
	MethodBuilder::~MethodBuilder()
	{
		for (instr_iter i = instructions.begin(); i != instructions.end(); i++)
			delete i->instr;
	}

	void MethodBuilder::Append(uint32_t originalOffset, uint32_t originalSize, Instruction *instr)
	{
		instructions.push_back(InstrDesc(originalOffset, originalSize, instr));
		instr->offset = lastOffset;
		lastOffset += (int32_t)instr->GetSize();
		hasBranches = hasBranches || instr->IsBranch() || instr->IsSwitch();
	}

	int32_t MethodBuilder::FindIndex(uint32_t originalOffset) const
	{
		int32_t iMin = 0; // inclusive
		int32_t iMax = (int32_t)instructions.size() - 1; // inclusive
		while (iMax >= iMin)
		{
			int32_t iMid = (iMin + iMax) / 2;
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
		return (int32_t)instructions.size(); // aw
	}

	void MethodBuilder::MarkForRemoval(int32_t index)
	{
		// Note: it is okay to remove instructions that have incoming branches;
		// the branch is simply forwarded to the next instruction.
		// Also note: previously removals were marked by setting stackHeight to -2.
		// This cannot be done, as we must preserve the known stack height in case
		// the instruction has incoming branches; otherwise we cannot verify that
		// it is reached with a consistent stack height on all branches.
		instructions[index].removed = true;
	}

	bool MethodBuilder::IsMarkedForRemoval(int32_t index) const
	{
		return instructions[index].removed;
	}

	void MethodBuilder::PerformRemovals(MethodOverload *method)
	{
		using namespace std;
		const int SmallBufferSize = 64;

		if (instructions.size() < SmallBufferSize)
		{
			int32_t newIncides[SmallBufferSize];
			PerformRemovalsInternal(newIncides, method);
		}
		else
		{
			unique_ptr<int32_t[]> newIndices(new int32_t[instructions.size() + 1]);
			PerformRemovalsInternal(newIndices.get(), method);
		}
	}

	void MethodBuilder::PerformRemovalsInternal(int32_t newIndices[], MethodOverload *method)
	{
		this->lastOffset = 0; // Must recalculate byte offsets as well

		int32_t oldIndex = 0, newIndex = 0;
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
				lastOffset += (int32_t)i->instr->GetSize();
				newIndices[oldIndex] = newIndex;
				newIndex++;
				i++;
			}
		}
		// try, catch and finally blocks may reference an index
		// beyond the last instruction
		newIndices[oldIndex] = newIndex;

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
					for (uint32_t t = 0; t < sw->targetCount; t++)
						sw->targets[t] = newIndices[sw->targets[t]];
				}

		for (int32_t t = 0; t < method->tryBlockCount; t++)
		{
			TryBlock *tryBlock = method->tryBlocks + t;
			tryBlock->tryStart = newIndices[tryBlock->tryStart];
			tryBlock->tryEnd = newIndices[tryBlock->tryEnd];

			switch (tryBlock->kind)
			{
			case TryKind::CATCH:
				for (int32_t c = 0; c < tryBlock->catches.count; c++)
				{
					CatchBlock *catchBlock = tryBlock->catches.blocks + c;
					catchBlock->catchStart = newIndices[catchBlock->catchStart];
					catchBlock->catchEnd = newIndices[catchBlock->catchEnd];
				}
				break;
			case TryKind::FINALLY:
				tryBlock->finallyBlock.finallyStart = newIndices[tryBlock->finallyBlock.finallyStart];
				tryBlock->finallyBlock.finallyEnd = newIndices[tryBlock->finallyBlock.finallyEnd];
				break;
			}
		}

		if (method->debugSymbols)
		{
			debug::OverloadSymbols *debug = method->debugSymbols;
			int32_t debugSymbolCount = debug->GetSymbolCount();
			for (int32_t i = 0; i < debugSymbolCount; i++)
			{
				debug::DebugSymbol &sym = debug->GetSymbol(i);
				sym.startOffset = newIndices[sym.startOffset];
				sym.endOffset = newIndices[sym.endOffset];
			}
		}
	}

	int32_t MethodBuilder::GetNewOffset(int32_t index) const
	{
		typedef std::vector<InstrDesc>::const_iterator const_iter;
		if (index >= (int32_t)instructions.size())
		{
			Instruction *end = (instructions.end() - 1)->instr;
			return end->offset + end->GetSize();
		}
		return instructions[index].instr->offset;
	}

	int32_t MethodBuilder::GetNewOffset(int32_t index, const Instruction *relativeTo) const
	{
		typedef std::vector<InstrDesc>::const_iterator const_iter;
		int32_t offset;
		if (index >= (int32_t)instructions.size())
		{
			Instruction *end = (instructions.end() - 1)->instr;
			offset = end->offset + end->GetSize();
		}
		else
			offset = instructions[index].instr->offset;
		return offset - relativeTo->offset - relativeTo->GetSize();
	}

	void MethodBuilder::SetInstruction(int32_t index, Instruction *newInstr, bool deletePrev)
	{
		if (deletePrev)
			delete instructions[index].instr;
		instructions[index].instr = newInstr;
	}

	void MethodBuilder::AddTypeToInitialize(Type *type)
	{
		if (type->HasStaticCtorRun())
			return;

		for (type_iter i = typesToInitialize.begin(); i < typesToInitialize.end(); i++)
			if (*i == type)
				return;

		typesToInitialize.push_back(type);
	}

	MethodBuffer::MethodBuffer(int32_t size) :
		current(nullptr), buffer()
	{
		buffer.reset(new uint8_t[size]);
		current = buffer.get();
	}

	MethodBuffer::~MethodBuffer()
	{ }
} // namespace instr

} // namespace ovum
