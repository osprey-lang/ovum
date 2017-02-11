#pragma once

#include "../vm.h"
#include <vector>

namespace ovum
{

namespace instr
{
	class MethodBuilder
	{
	public:
		// When used as a stack height, indicates that the instruction has not
		// yet been visited by any branch of evaluation.
		static const ovlocals_t UNVISITED = (ovlocals_t)-1;

		MethodBuilder();
		~MethodBuilder();

		inline Instruction *operator[](size_t index) const
		{
			return instructions[index].instr;
		}

		inline size_t GetLength() const
		{
			return instructions.size();
		}

		inline size_t GetByteSize() const
		{
			return lastOffset;
		}

		inline bool HasBranches() const
		{
			return hasBranches;
		}

		inline size_t GetTypeCount() const
		{
			return (int32_t)typesToInitialize.size();
		}

		inline Type *GetType(size_t index) const
		{
			return typesToInitialize[index];
		}

		uint32_t GetOriginalOffset(size_t index) const;

		size_t GetOriginalSize(size_t index) const;

		size_t FindIndex(uint32_t originalOffset) const;

		size_t GetNewOffset(size_t index) const;

		int32_t GetJumpOffset(size_t index, const Instruction *relativeTo) const;

		ovlocals_t GetStackHeight(size_t index) const;

		void SetStackHeight(size_t index, ovlocals_t stackHeight);

		uint32_t GetRefSignature(size_t index) const;

		void SetRefSignature(size_t index, uint32_t refSignature);

		bool IsMarkedForRemoval(size_t index) const;

		void MarkForRemoval(size_t index);

		void Append(uint32_t originalOffset, size_t originalSize, Instruction *instr);

		void SetInstruction(size_t index, Instruction *newInstr, bool deletePrev);

		void PerformRemovals(MethodOverload *method);

		void AddTypeToInitialize(Type *type);

	private:
		OVUM_DISABLE_COPY_AND_ASSIGN(MethodBuilder);

		class InstrDesc
		{
		public:
			uint32_t originalOffset;
			size_t originalSize;
			ovlocals_t stackHeight;
			uint32_t refSignature;
			bool removed;
			Instruction *instr;

			inline InstrDesc(uint32_t originalOffset, size_t originalSize, Instruction *instr) :
				originalOffset(originalOffset),
				originalSize(originalSize),
				stackHeight(UNVISITED),
				refSignature(0),
				removed(false),
				instr(instr)
			{ }
		};

		typedef std::vector<InstrDesc>::iterator instr_iter;
		typedef std::vector<Type*>::iterator type_iter;

		size_t lastOffset;
		bool hasBranches;
		std::vector<InstrDesc> instructions;
		std::vector<Type*> typesToInitialize;

		void PerformRemovalsInternal(size_t newIndices[], MethodOverload *method);
	};

	class MethodBuffer
	{
	public:
		explicit MethodBuffer(size_t size);

		~MethodBuffer();

		// Gets the current buffer pointer. Data is written at this offset.
		inline uint8_t *GetCurrent() const
		{
			return current;
		}

		// Gets a pointer to the start of the buffer.
		inline uint8_t *GetBuffer() const
		{
			return buffer.get();
		}

		// Claims the fully initialized buffer, which prevents the MethodBuffer
		// from deallocating it in the destructor.
		inline uint8_t *Release()
		{
			return buffer.release();
		}

		// Writes a value of the specified type at the current buffer offset,
		// and increments the buffer offset by the specified amount.
		template<class T>
		inline void Write(T value, size_t size)
		{
			*reinterpret_cast<T*>(current) = value;
			current += size;
		}

		inline void AlignTo(size_t alignment)
		{
			uintptr_t offset = (uintptr_t)current % (uintptr_t)alignment;
			if (offset != 0)
				current += alignment - offset;
		}

	private:
		OVUM_DISABLE_COPY_AND_ASSIGN(MethodBuffer);

		uint8_t *current;
		Box<uint8_t[]> buffer;
	};
} // namespace instr

} // namespace ovum
