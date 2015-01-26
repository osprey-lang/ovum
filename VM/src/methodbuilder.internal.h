#pragma once

#ifndef VM__METHODBUILDER_INTERNAL_H
#define VM__METHODBUILDER_INTERNAL_H

#include "ov_vm.internal.h"
#include <vector>

namespace ovum
{
namespace instr
{
	class Instruction;

	class MethodBuilder
	{
	private:
		class InstrDesc
		{
		public:
			uint32_t originalOffset;
			uint32_t originalSize;
			int32_t stackHeight;
			uint32_t refSignature;
			bool removed;
			Instruction *instr;

			inline InstrDesc(uint32_t originalOffset, uint32_t originalSize, Instruction *instr) :
				originalOffset(originalOffset), originalSize(originalSize),
				stackHeight(-1), refSignature(0), removed(false), instr(instr)
			{ }
		};

		typedef std::vector<InstrDesc>::iterator instr_iter;
		typedef std::vector<Type*>::iterator type_iter;

		int32_t lastOffset;
		bool hasBranches;
		std::vector<InstrDesc> instructions;
		std::vector<Type*> typesToInitialize;

		DISABLE_COPY_AND_ASSIGN(MethodBuilder);

	public:
		inline MethodBuilder() : lastOffset(0), hasBranches(false) { }
		~MethodBuilder();

		inline int32_t GetLength() const
		{
			return (int32_t)instructions.size();
		}
		inline int32_t GetByteSize() const
		{
			return lastOffset;
		}

		inline bool HasBranches() const
		{
			return hasBranches;
		}

		void Append(uint32_t originalOffset, uint32_t originalSize, Instruction *instr);

		int32_t FindIndex(uint32_t originalOffset) const;

		inline uint32_t GetOriginalOffset(int32_t index) const
		{
			if (index >= (int32_t)instructions.size())
				return instructions.end()->originalOffset +
					instructions.end()->originalSize;
			return instructions[index].originalOffset;
		}
		inline uint32_t GetOriginalSize(int32_t index) const
		{
			if (index >= (int32_t)instructions.size())
				return 0;
			return instructions[index].originalSize;
		}
		int32_t GetNewOffset(int32_t index) const;
		int32_t GetNewOffset(int32_t index, const Instruction *relativeTo) const;

		inline int32_t GetStackHeight(int32_t index) const
		{
			return instructions[index].stackHeight;
		}
		inline void SetStackHeight(int32_t index, const uint16_t stackHeight)
		{
			InstrDesc &instrDesc = instructions[index];
			assert(!instrDesc.removed);
			assert(instrDesc.stackHeight < 0);
			instrDesc.stackHeight = stackHeight;
		}

		inline uint32_t GetRefSignature(int32_t index) const
		{
			return instructions[index].refSignature;
		}
		inline void SetRefSignature(int32_t index, uint32_t refSignature)
		{
			instructions[index].refSignature = refSignature;
		}

		void MarkForRemoval(int32_t index);
		bool IsMarkedForRemoval(int32_t index) const;
		void PerformRemovals(MethodOverload *method);

		inline Instruction *operator[](int32_t index) const
		{
			return instructions[index].instr;
		}
		void SetInstruction(int32_t index, Instruction *newInstr, bool deletePrev);

		inline int32_t GetTypeCount() const
		{
			return (int32_t)typesToInitialize.size();
		}

		void AddTypeToInitialize(Type *type);

		inline Type *GetType(int32_t index) const
		{
			return typesToInitialize[index];
		}

	private:
		void PerformRemovalsInternal(int32_t newIndices[], MethodOverload *method);
	};

	class MethodBuffer
	{
	private:
		uint8_t *current;
		uint8_t *buffer;

		DISABLE_COPY_AND_ASSIGN(MethodBuffer);

	public:
		inline MethodBuffer(uint8_t *buffer) : current(buffer), buffer(buffer)
		{ }

		// Gets the current buffer pointer. Data is written at this offset.
		inline uint8_t *GetCurrent() const
		{
			return current;
		}

		// Gets a pointer to the start of the buffer.
		inline uint8_t *GetBuffer() const
		{
			return buffer;
		}

		// Writes a value of the specified type at the current buffer offset,
		// and increments the buffer offset by the size of the data.
		template<class T>
		inline void Write(T value)
		{
			*reinterpret_cast<T*>(current) = value;
			current += sizeof(T);
		}
	};
} // namespace instr

} // namespace ovum

#endif // VM__METHODBUILDER_INTERNAL_H