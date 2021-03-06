#pragma once

#include "../vm.h"
#include "thread.opcodes.h"
#include "../object/method.h"

namespace ovum
{

namespace instr
{
	namespace oa = ovum::opcode_args;

	enum class InstrFlags : uint32_t
	{
		NONE           = 0x0000,
		// The instruction has incoming branches - that is, another instruction
		// branches to this instruction.
		HAS_INCOMING_BRANCHES = 0x0001,

		// The instruction has a LocalOffset input.
		HAS_INPUT = 0x0002,
		// The instruction has a LocalOffset output.
		HAS_OUTPUT = 0x0004,
		HAS_INOUT = HAS_INPUT | HAS_OUTPUT,

		// The instruction requires the input to be on the stack.
		// When the instruction lacks this flag, the LocalOffset it gets for its
		// input may point to a local variable. That way, given a sequence of
		// instructions like
		//   ldloc.0
		//   stsfld   <some field>
		// can be rewritten so that stsfld reads directly from the local (and so
		// ldloc.0 can be removed altogether).
		INPUT_ON_STACK = 0x0008,

		// The instruction inherits from Branch.
		BRANCH = 0x0010,
		// The instruction inherits from Switch.
		SWITCH = 0x0020,
		// The instruction is a LoadLocal.
		LOAD_LOCAL = 0x0040,
		// The instruction is a StoreLocal.
		STORE_LOCAL = 0x0080,
		// The instruction is a DupInstr.
		DUP = 0x0100,

		// The instruction accepts references on the stack.
		ACCEPTS_REFS = 0x0200,
		// The instruction pushes a reference onto the stack.
		PUSHES_REF  = 0x0400,
	};
	OVUM_ENUM_OPS(InstrFlags, uint32_t);

	class StackChange
	{
	public:
		uint16_t removed;
		uint16_t added;

		inline StackChange(uint16_t removed, uint16_t added) :
			removed(removed),
			added(added)
		{ }

		static const StackChange empty;
	};

	// Represents the target of a branch instruction.
	//
	// During method initialization, jump targets are initially read as offsets
	// relative to the instruction. Since it is possible to jump backwards, the
	// offset is necessarily a signed integer. However, the jump target is then
	// resolved to the absolute index of the target instruction in the method.
	// When the intermediate bytecode is emitted, the target is once again made
	// into a signed relative offset.
	//
	// We don't want double fields in each branch instruction, so we encapsulate
	// the offset and the index in the same data type. Since size_t and int32_t
	// may have different sizes, and definitely have different signedness, but
	// an instruction can only contain one of the two at any given time, a union
	// is acceptable.
	//
	// NOTE: Do not add constructors to this type. It needs to be a trivially
	// copyable type.
	union JumpTarget
	{
		int32_t offset;
		size_t index;

		inline static JumpTarget FromOffset(int32_t offset)
		{
			JumpTarget result;
			result.offset = offset;
			return result;
		}

		inline static JumpTarget FromIndex(size_t index)
		{
			JumpTarget result;
			result.index = index;
			return result;
		}
	};

	// General abstract base class for all intermediate instructions.
	class Instruction
	{
	public:
		InstrFlags flags;
		size_t offset;
		IntermediateOpcode opcode;

		inline Instruction(InstrFlags flags, IntermediateOpcode opcode) :
			flags(flags),
			opcode(opcode)
		{ }

		inline virtual ~Instruction() { }

		inline size_t GetSize() const
		{
			return OVUM_ALIGN_TO(sizeof(IntermediateOpcode), oa::ALIGNMENT) + GetArgsSize();
		}

		inline virtual size_t GetArgsSize() const { return 0; }

		virtual StackChange GetStackChange() const = 0;

		inline bool HasInput() const
		{
			return HasFlag(InstrFlags::HAS_INPUT);
		}

		inline bool HasOutput() const
		{
			return HasFlag(InstrFlags::HAS_OUTPUT);
		}

		inline bool IsBranch() const
		{
			return HasFlag(InstrFlags::BRANCH);
		}

		inline bool IsSwitch() const
		{
			return HasFlag(InstrFlags::SWITCH);
		}

		inline bool IsLoadLocal() const
		{
			return HasFlag(InstrFlags::LOAD_LOCAL);
		}

		inline bool IsStoreLocal() const
		{
			return HasFlag(InstrFlags::STORE_LOCAL);
		}

		inline bool IsDup() const
		{
			return HasFlag(InstrFlags::DUP);
		}

		inline bool HasIncomingBranches() const
		{
			return HasFlag(InstrFlags::HAS_INCOMING_BRANCHES);
		}

		inline bool RequiresStackInput() const
		{
			return HasFlag(InstrFlags::INPUT_ON_STACK);
		}

		inline bool AcceptsRefs() const
		{
			return HasFlag(InstrFlags::ACCEPTS_REFS);
		}

		inline bool PushesRef() const
		{
			return HasFlag(InstrFlags::PUSHES_REF);
		}

		inline void AddIncomingBranch()
		{
			flags = flags | InstrFlags::HAS_INCOMING_BRANCHES;
		}

		inline virtual void UpdateInput(LocalOffset offset, bool isOnStack) { }

		inline virtual void UpdateOutput(LocalOffset offset, bool isOnStack) { }

		inline virtual uint32_t GetReferenceSignature() const { return 0; }

		inline virtual int SetReferenceSignature(const StackManager &stack) { return -1; }

		void WriteBytes(MethodBuffer &buffer, MethodBuilder &builder) const;

	protected:
		inline virtual void WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const { }

	private:
		inline bool HasFlag(InstrFlags flag) const
		{
			return (flags & flag) == flag;
		}
	};

	// For instructions that have no input, no output, no arguments,
	// no other special requirements, and a fixed stack change.
	class SimpleInstruction : public Instruction
	{
	public:
		StackChange stackChange;

		inline SimpleInstruction(IntermediateOpcode opcode, StackChange stackChange) :
			Instruction(InstrFlags::NONE, opcode),
			stackChange(stackChange)
		{ }

		inline virtual StackChange GetStackChange() const
		{
			return stackChange;
		}
	};

	// An instruction that loads a local value (argument, local variable or stack value)
	// into another local location. This instruction combines ldloc, ldarg, stloc and starg.
	// Just to reiterate from the IntermediateOpcode documentation:
	// mvloc encodes the stack change in its lowest two bits:
	// 0000 001ar
	//         a  = if set, one value was added
	//          r = if set, one value was removed
	class MoveLocal : public Instruction
	{
	public:
		LocalOffset source;
		LocalOffset target;

		inline MoveLocal() :
			Instruction(InstrFlags::HAS_INOUT, OPI_MVLOC_SS),
			source(),
			target()
		{ }
		inline MoveLocal(InstrFlags flags) :
			Instruction(flags, OPI_MVLOC_SS),
			source(),
			target()
		{ }
		inline MoveLocal(InstrFlags flags, IntermediateOpcode opc) :
			Instruction(flags, opc),
			source(),
			target()
		{ }

		inline virtual size_t GetArgsSize() const
		{
			return oa::TWO_LOCALS_SIZE;
		}

		inline virtual StackChange GetStackChange() const
		{
			// Lowest bit: if set, pops one from stack; if cleared, does not.
			// Second bit: if set, pushes one to stack; if cleared, does not.
			return StackChange(opcode & 1, (opcode & 2) >> 1);
		}

		inline virtual void UpdateInput(LocalOffset offset, bool isOnStack)
		{
			source = offset;
			// Set or clear lowest bit to indicate removal from stack (or lack thereof)
			opcode = (IntermediateOpcode)(isOnStack ? opcode | 1 : opcode & ~1);
		}

		inline virtual void UpdateOutput(LocalOffset offset, bool isOnStack)
		{
			target = offset;
			// Set or clear second lowest bit to indicate addition to stack (or lack thereof)
			opcode = (IntermediateOpcode)(isOnStack ? opcode | 2 : opcode & ~2);
		}

	protected:
		virtual void WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const;
	};

	class LoadLocal : public MoveLocal
	{
	public:
		bool sourceIsRef;

		inline LoadLocal(LocalOffset localSource, bool sourceIsRef) :
			MoveLocal(
				sourceIsRef ? InstrFlags::HAS_OUTPUT : InstrFlags::HAS_OUTPUT | InstrFlags::LOAD_LOCAL,
				sourceIsRef ? OPI_MVLOC_RS : OPI_MVLOC_LS
			),
			sourceIsRef(sourceIsRef)
		{
			source = localSource;
		}

		inline virtual StackChange GetStackChange() const
		{
			return StackChange(0, 1);
		}

		inline virtual void UpdateOutput(LocalOffset offset, bool isOnStack)
		{
			if (sourceIsRef)
			{
				target = offset;
				opcode = isOnStack ? OPI_MVLOC_RS : OPI_MVLOC_RL;
			}
			else
				MoveLocal::UpdateOutput(offset, isOnStack);
		}
	};

	class StoreLocal : public MoveLocal
	{
	public:
		bool targetIsRef;

		inline StoreLocal(LocalOffset localTarget, bool targetIsRef) :
			MoveLocal(
				targetIsRef ? InstrFlags::HAS_INPUT : InstrFlags::HAS_INPUT | InstrFlags::STORE_LOCAL,
				targetIsRef ? OPI_MVLOC_SR : OPI_MVLOC_SL
			),
			targetIsRef(targetIsRef)
		{
			target = localTarget;
		}

		inline virtual StackChange GetStackChange() const
		{
			return StackChange(1, 0);
		}

		inline virtual void UpdateInput(LocalOffset offset, bool isOnStack)
		{
			if (targetIsRef)
			{
				source = offset;
				opcode = isOnStack ? OPI_MVLOC_SR : OPI_MVLOC_LR;
			}
			else
				MoveLocal::UpdateInput(offset, isOnStack);
		}
	};

	class DupInstr : public Instruction
	{
	public:
		LocalOffset source;
		LocalOffset target;

		inline DupInstr() :
			Instruction(InstrFlags::HAS_INOUT | InstrFlags::INPUT_ON_STACK | InstrFlags::DUP, OPI_MVLOC_LS),
			source(),
			target()
		{ }

		inline virtual size_t GetArgsSize() const
		{
			return oa::TWO_LOCALS_SIZE;
		}

		inline virtual StackChange GetStackChange() const
		{
			return StackChange(1, 1 + ((opcode & 2) >> 1));
		}

		inline virtual void UpdateInput(LocalOffset offset, bool isOnStack)
		{
			OVUM_ASSERT(isOnStack);
			source = offset;
		}

		inline virtual void UpdateOutput(LocalOffset offset, bool isOnStack)
		{
			if (isOnStack)
			{
				// dup claims to add two values, but we're only interested in the second argument
				target = LocalOffset(offset.GetOffset() + sizeof(Value));
				opcode = (IntermediateOpcode)(opcode | 2);
			}
			else
			{
				// ... except if we're storing the value in a local. dup is kind of special like that.
				// "special".
				target = offset;
				opcode = (IntermediateOpcode)(opcode & ~2);
			}
		}

	protected:
		virtual void WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const;
	};

	// In all the load instructions that follow, the lowest bit is set
	// to indicate that the target is on the stack.

	class LoadValue : public Instruction
	{
	public:
		LocalOffset target;

		inline LoadValue(IntermediateOpcode opcode) :
			Instruction(InstrFlags::HAS_OUTPUT, opcode),
			target()
		{ }

		inline virtual size_t GetArgsSize() const
		{
			return oa::ONE_LOCAL_SIZE;
		}

		inline virtual StackChange GetStackChange() const
		{
			return StackChange(0, opcode & 1);
		}

		inline virtual void UpdateOutput(LocalOffset offset, bool isOnStack)
		{
			target = offset;
			opcode = (IntermediateOpcode)(isOnStack ? opcode | 1 : opcode & ~1);
		}

	protected:
		virtual void WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const;
	};

	class LoadNull : public LoadValue
	{
	public:
		inline LoadNull() : LoadValue(OPI_LDNULL_S) { }
	};

	class LoadBoolean : public LoadValue
	{
	public:
		bool value;

		inline LoadBoolean(bool value) :
			LoadValue(value ? OPI_LDTRUE_S : OPI_LDFALSE_S),
			value(value)
		{ }
	};

	class LoadInt : public LoadValue
	{
	public:
		int64_t value;

		inline LoadInt(int64_t value) :
			LoadValue(OPI_LDC_I_S),
			value(value)
		{ }

		inline virtual size_t GetArgsSize() const
		{
			return oa::LOCAL_AND_VALUE<int64_t>::SIZE;
		}

	protected:
		virtual void WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const;
	};

	class LoadUInt : public LoadValue
	{
	public:
		uint64_t value;

		inline LoadUInt(uint64_t value) :
			LoadValue(OPI_LDC_U_S),
			value(value)
		{ }

		inline virtual size_t GetArgsSize() const
		{
			return oa::LOCAL_AND_VALUE<uint64_t>::SIZE;
		}

	protected:
		virtual void WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const;
	};

	class LoadReal : public LoadValue
	{
	public:
		double value;

		inline LoadReal(double value) :
			LoadValue(OPI_LDC_R_S),
			value(value)
		{ }

		inline virtual size_t GetArgsSize() const
		{
			return oa::LOCAL_AND_VALUE<double>::SIZE;
		}

	protected:
		virtual void WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const;
	};

	class LoadString : public LoadValue
	{
	public:
		String *value;

		inline LoadString(String *value) :
			LoadValue(OPI_LDSTR_S),
			value(value)
		{ }

		inline virtual size_t GetArgsSize() const
		{
			return oa::LOCAL_AND_VALUE<String*>::SIZE;
		}

	protected:
		virtual void WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const;
	};

	class LoadArgCount : public LoadValue
	{
	public:
		inline LoadArgCount() : LoadValue(OPI_LDARGC_S) { }
	};

	class LoadEnumValue : public LoadValue
	{
	public:
		Type *type;
		int64_t value;

		inline LoadEnumValue(Type *type, int64_t value) :
			LoadValue(OPI_LDENUM_S),
			type(type),
			value(value)
		{ }

		inline virtual size_t GetArgsSize() const
		{
			return oa::LOAD_ENUM_SIZE;
		}

	protected:
		virtual void WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const;
	};

	class NewObject : public Instruction
	{
	public:
		LocalOffset args;
		LocalOffset target;
		Type *type;
		ovlocals_t argCount;
		uint32_t refSignature; // Not written to the instruction

		inline NewObject(Type *type, ovlocals_t argCount) :
			Instruction(InstrFlags::HAS_INOUT | InstrFlags::INPUT_ON_STACK | InstrFlags::ACCEPTS_REFS, OPI_NEWOBJ_S),
			type(type),
			argCount(argCount),
			args(),
			target(),
			refSignature(0)
		{ }

		inline virtual size_t GetArgsSize() const
		{
			return oa::NEW_OBJECT_SIZE;
		}

		inline virtual StackChange GetStackChange() const
		{
			return StackChange(argCount, opcode & 1);
		}

		inline virtual void UpdateInput(LocalOffset offset, bool isOnStack)
		{
			OVUM_ASSERT(isOnStack);
			this->args = offset;
		}

		inline virtual void UpdateOutput(LocalOffset offset, bool isOnStack)
		{
			this->target = offset;
			opcode = (IntermediateOpcode)(isOnStack ? opcode | 1 : opcode & ~1);
		}

		inline virtual uint32_t GetReferenceSignature() const
		{
			return refSignature;
		}

		virtual int SetReferenceSignature(const StackManager &stack);

	protected:
		virtual void WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const;
	};

	class CreateList : public LoadValue
	{
	public:
		size_t capacity;

		inline CreateList(size_t capacity) :
			LoadValue(OPI_LIST_S),
			capacity(capacity)
		{ }

		inline virtual size_t GetArgsSize() const
		{
			return oa::LOCAL_AND_VALUE<size_t>::SIZE;
		}

	protected:
		virtual void WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const;
	};

	class CreateHash : public LoadValue
	{
	public:
		size_t capacity;

		inline CreateHash(size_t capacity) :
			LoadValue(OPI_HASH_S),
			capacity(capacity)
		{ }

		inline virtual size_t GetArgsSize() const
		{
			return oa::LOCAL_AND_VALUE<int32_t>::SIZE;
		}

	protected:
		virtual void WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const;
	};

	class LoadStaticFunction : public LoadValue
	{
	public:
		Method *method;

		inline LoadStaticFunction(Method *method) :
			LoadValue(OPI_LDSFN_S),
			method(method)
		{ }

		inline virtual size_t GetArgsSize() const
		{
			return oa::LOCAL_AND_VALUE<Method*>::SIZE;
		}

	protected:
		virtual void WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const;
	};

	class LoadTypeToken : public LoadValue
	{
	public:
		Type *type;

		inline LoadTypeToken(Type *type) :
			LoadValue(OPI_LDTYPETKN_S),
			type(type)
		{ }

		inline virtual size_t GetArgsSize() const
		{
			return oa::LOCAL_AND_VALUE<Type*>::SIZE;
		}

	protected:
		virtual void WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const;
	};

	class LoadMember : public Instruction
	{
	public:
		LocalOffset instance; // must be on the stack
		LocalOffset output; // doesn't have to be on the stack
		String *member;

		inline LoadMember(String *member) :
			Instruction(InstrFlags::HAS_INOUT | InstrFlags::INPUT_ON_STACK, OPI_LDMEM_S),
			instance(),
			output(),
			member(member)
		{ }

		inline virtual size_t GetArgsSize() const
		{
			return oa::TWO_LOCALS_AND_VALUE<String*>::SIZE;
		}

		inline virtual StackChange GetStackChange() const
		{
			return StackChange(1, opcode & 1);
		}

		inline virtual void UpdateInput(LocalOffset offset, bool isOnStack)
		{
			OVUM_ASSERT(isOnStack);
			instance = offset;
		}

		inline virtual void UpdateOutput(LocalOffset offset, bool isOnStack)
		{
			output = offset;
			opcode = (IntermediateOpcode)(isOnStack ? opcode | 1 : opcode & ~1);
		}

	protected:
		virtual void WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const;
	};

	class StoreMember : public Instruction
	{
	public:
		LocalOffset args;
		String *member;

		inline StoreMember(String *member) :
			Instruction(InstrFlags::HAS_INPUT | InstrFlags::INPUT_ON_STACK, OPI_STMEM),
			args(),
			member(member)
		{ }

		inline virtual size_t GetArgsSize() const
		{
			return oa::LOCAL_AND_VALUE<String*>::SIZE;
		}

		inline virtual StackChange GetStackChange() const { return StackChange(2, 0); }

		inline virtual void UpdateInput(LocalOffset offset, bool isOnStack)
		{
			OVUM_ASSERT(isOnStack);
			args = offset;
		}

	protected:
		virtual void WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const;
	};

	class LoadField : public Instruction
	{
	public:
		LocalOffset instance; // must be on the stack
		LocalOffset output; // doesn't have to be on the stack
		Field *field;

		inline LoadField(Field *field) :
			Instruction(InstrFlags::HAS_INOUT | InstrFlags::INPUT_ON_STACK, OPI_LDFLD_S),
			instance(),
			output(),
			field(field)
		{ }

		inline virtual size_t GetArgsSize() const
		{
			return oa::TWO_LOCALS_AND_VALUE<Field*>::SIZE;
		}

		inline virtual StackChange GetStackChange() const
		{
			return StackChange(1, opcode & 1);
		}

		inline virtual void UpdateInput(LocalOffset offset, bool isOnStack)
		{
			OVUM_ASSERT(isOnStack);
			this->instance = offset;
		}

		inline virtual void UpdateOutput(LocalOffset offset, bool isOnStack)
		{
			output = offset;
			opcode = (IntermediateOpcode)(isOnStack ? opcode | 1 : opcode & ~1);
		}

	protected:
		virtual void WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const;
	};

	class StoreField : public Instruction
	{
	public:
		LocalOffset args;
		Field *field;

		inline StoreField(Field *field) :
			Instruction(InstrFlags::HAS_INPUT | InstrFlags::INPUT_ON_STACK, OPI_STFLD),
			args(),
			field(field)
		{ }

		inline virtual size_t GetArgsSize() const
		{
			return oa::LOCAL_AND_VALUE<Field*>::SIZE;
		}

		inline virtual StackChange GetStackChange() const
		{
			return StackChange(2, 0);
		}

		inline virtual void UpdateInput(LocalOffset offset, bool isOnStack)
		{
			OVUM_ASSERT(isOnStack);
			args = offset;
		}

	protected:
		virtual void WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const;
	};

	class LoadStaticField : public LoadValue
	{
	public:
		Field *field;

		inline LoadStaticField(Field *field) :
			LoadValue(OPI_LDSFLD_S),
			field(field)
		{ }

		inline virtual size_t GetArgsSize() const
		{
			return oa::LOCAL_AND_VALUE<Field*>::SIZE;
		}

	protected:
		virtual void WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const;
	};

	class StoreStaticField : public Instruction
	{
	public:
		LocalOffset value; // doesn't have to be on the stack!
		Field *field;

		inline StoreStaticField(Field *field) :
			Instruction(InstrFlags::HAS_INPUT, OPI_STSFLD_S),
			value(),
			field(field)
		{ }

		inline virtual size_t GetArgsSize() const
		{
			return oa::LOCAL_AND_VALUE<Field*>::SIZE;
		}

		inline virtual StackChange GetStackChange() const
		{
			return StackChange(opcode & 1, 0);
		}

		inline virtual void UpdateInput(LocalOffset offset, bool isOnStack)
		{
			value = offset;
			opcode = (IntermediateOpcode)(isOnStack ? opcode | 1 : opcode & ~1);
		}

	protected:
		virtual void WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const;
	};

	class LoadIterator : public Instruction
	{
	public:
		LocalOffset value; // must be on the stack
		LocalOffset output;

		inline LoadIterator() :
			Instruction(InstrFlags::HAS_INOUT | InstrFlags::INPUT_ON_STACK, OPI_LDITER_S),
			value(),
			output()
		{ }

		inline virtual size_t GetArgsSize() const
		{
			return oa::TWO_LOCALS_SIZE;
		}

		inline virtual StackChange GetStackChange() const
		{
			return StackChange(1, opcode & 1);
		}

		inline virtual void UpdateInput(LocalOffset offset, bool isOnStack)
		{
			OVUM_ASSERT(isOnStack);
			value = offset;
		}

		inline virtual void UpdateOutput(LocalOffset offset, bool isOnStack)
		{
			output = offset;
			opcode = (IntermediateOpcode)(isOnStack ? opcode | 1 : opcode & ~1);
		}

	protected:
		virtual void WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const;
	};

	class LoadType : public Instruction
	{
	public:
		LocalOffset source; // on stack
		LocalOffset target;

		inline LoadType() :
			Instruction(InstrFlags::HAS_INOUT | InstrFlags::INPUT_ON_STACK, OPI_LDTYPE_S),
			source(),
			target()
		{ }

		inline virtual size_t GetArgsSize() const
		{
			return oa::TWO_LOCALS_SIZE;
		}

		inline virtual StackChange GetStackChange() const
		{
			return StackChange(1, opcode & 1);
		}

		inline virtual void UpdateInput(LocalOffset offset, bool isOnStack)
		{
			OVUM_ASSERT(isOnStack);
			source = offset;
		}

		inline virtual void UpdateOutput(LocalOffset offset, bool isOnStack)
		{
			target = offset;
			opcode = (IntermediateOpcode)(isOnStack ? opcode | 1 : opcode & ~1);
		}

	protected:
		virtual void WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const;
	};

	class LoadIndexer : public Instruction
	{
	public:
		LocalOffset args; // must be on stack (does include instance)
		LocalOffset output;
		ovlocals_t argCount;

		inline LoadIndexer(ovlocals_t argCount) :
			Instruction(InstrFlags::HAS_INOUT | InstrFlags::INPUT_ON_STACK, OPI_LDIDX_S),
			args(),
			output(),
			argCount(argCount)
		{ }

		inline virtual size_t GetArgsSize() const
		{
			return oa::TWO_LOCALS_AND_VALUE<uint32_t>::SIZE;
		}

		inline virtual StackChange GetStackChange() const
		{
			return StackChange(argCount + 1, opcode & 1);
		}

		inline virtual void UpdateInput(LocalOffset offset, bool isOnStack)
		{
			OVUM_ASSERT(isOnStack);
			args = offset;
		}

		inline virtual void UpdateOutput(LocalOffset offset, bool isOnStack)
		{
			output = offset;
			opcode = (IntermediateOpcode)(isOnStack ? opcode | 1 : opcode & ~1);
		}

	protected:
		virtual void WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const;
	};

	class StoreIndexer : public Instruction
	{
	public:
		LocalOffset args; // must be on stack (does include instance)
		ovlocals_t argCount;

		inline StoreIndexer(ovlocals_t argCount) :
			Instruction(InstrFlags::HAS_INPUT | InstrFlags::INPUT_ON_STACK, OPI_STIDX),
			args(),
			argCount(argCount)
		{ }

		inline virtual size_t GetArgsSize() const
		{
			return oa::LOCAL_AND_VALUE<uint32_t>::SIZE;
		}

		inline virtual StackChange GetStackChange() const
		{
			return StackChange(argCount + 2, 0);
		}

		inline virtual void UpdateInput(LocalOffset offset, bool isOnStack)
		{
			OVUM_ASSERT(isOnStack);
			args = offset;
		}

	protected:
		virtual void WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const;
	};

	class Call : public Instruction
	{
	public:
		LocalOffset args; // must be on stack (includes value to be invoked)
		LocalOffset output;
		ovlocals_t argCount;
		uint32_t refSignature;

		inline Call(ovlocals_t argCount) :
			Instruction(InstrFlags::HAS_INOUT | InstrFlags::INPUT_ON_STACK | InstrFlags::ACCEPTS_REFS, OPI_CALL_S),
			args(),
			output(),
			argCount(argCount),
			refSignature(0)
		{ }

		inline virtual size_t GetArgsSize() const
		{
			return refSignature ? oa::CALL_REF_SIZE : oa::CALL_SIZE;
		}

		inline virtual StackChange GetStackChange() const
		{
			return StackChange(argCount + 1, opcode & 1);
		}

		inline virtual void UpdateInput(LocalOffset offset, bool isOnStack)
		{
			OVUM_ASSERT(isOnStack);
			args = offset;
		}

		inline virtual void UpdateOutput(LocalOffset offset, bool isOnStack)
		{
			output = offset;
			opcode = (IntermediateOpcode)(isOnStack ? opcode | 1 : opcode & ~1);
		}

		inline virtual uint32_t GetReferenceSignature() const
		{
			return refSignature;
		}

		virtual int SetReferenceSignature(const StackManager &stack);

	protected:
		virtual void WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const;
	};

	class CallMember : public Instruction
	{
	public:
		LocalOffset args; // on stack, always!
		LocalOffset output;
		String *member;
		ovlocals_t argCount;
		uint32_t refSignature;

		inline CallMember(String *member, ovlocals_t argCount) :
			Instruction(InstrFlags::HAS_INOUT | InstrFlags::INPUT_ON_STACK | InstrFlags::ACCEPTS_REFS, OPI_CALLMEM_S),
			args(),
			output(),
			member(member),
			argCount(argCount),
			refSignature(0)
		{ }

		inline virtual size_t GetArgsSize() const
		{
			return refSignature ? oa::CALL_MEMBER_REF_SIZE : oa::CALL_MEMBER_SIZE;
		}

		inline virtual StackChange GetStackChange() const
		{
			return StackChange(argCount + 1, opcode & 1);
		}

		inline virtual void UpdateInput(LocalOffset offset, bool isOnStack)
		{
			OVUM_ASSERT(isOnStack);
			args = offset;
		}

		inline virtual void UpdateOutput(LocalOffset offset, bool isOnStack)
		{
			output = offset;
			opcode = (IntermediateOpcode)(isOnStack ? opcode | 1 : opcode & ~1);
		}

		inline virtual uint32_t GetReferenceSignature() const
		{
			return refSignature;
		}

		virtual int SetReferenceSignature(const StackManager &stack);

	protected:
		virtual void WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const;
	};

	class StaticCall : public Instruction
	{
	public:
		LocalOffset args; // must be on stack
		LocalOffset output;
		ovlocals_t argCount;
		MethodOverload *method;
		uint32_t refSignature; // Not output

		inline StaticCall(ovlocals_t argCount, MethodOverload *method) :
			Instruction(InstrFlags::HAS_INOUT | InstrFlags::INPUT_ON_STACK | InstrFlags::ACCEPTS_REFS, OPI_SCALL_S),
			args(),
			output(),
			argCount(argCount),
			method(method),
			refSignature(0)
		{ }

		inline virtual size_t GetArgsSize() const
		{
			return oa::STATIC_CALL_SIZE;
		}

		inline virtual StackChange GetStackChange() const
		{
			return StackChange(argCount + method->InstanceOffset(), opcode & 1);
		}

		inline virtual void UpdateInput(LocalOffset offset, bool isOnStack)
		{
			OVUM_ASSERT(isOnStack);
			args = offset;
		}

		inline virtual void UpdateOutput(LocalOffset offset, bool isOnStack)
		{
			output = offset;
			opcode = (IntermediateOpcode)(isOnStack ? opcode | 1 : opcode & ~1);
		}

		inline virtual uint32_t GetReferenceSignature() const
		{
			return refSignature;
		}

		virtual int SetReferenceSignature(const StackManager &stack);

	protected:
		virtual void WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const;
	};

	class Apply : public Instruction
	{
	public:
		LocalOffset args; // includes the value to be invoked
		LocalOffset output;

		inline Apply() :
			Instruction(InstrFlags::HAS_INOUT | InstrFlags::INPUT_ON_STACK, OPI_APPLY_S),
			args(),
			output()
		{ }

		inline virtual size_t GetArgsSize() const
		{
			return oa::TWO_LOCALS_SIZE;
		}

		inline virtual StackChange GetStackChange() const
		{
			return StackChange(2, opcode & 1);
		}

		inline virtual void UpdateInput(LocalOffset offset, bool isOnStack)
		{
			OVUM_ASSERT(isOnStack);
			args = offset;
		}

		inline virtual void UpdateOutput(LocalOffset offset, bool isOnStack)
		{
			output = offset;
			opcode = (IntermediateOpcode)(isOnStack ? opcode | 1 : opcode & ~1);
		}

	protected:
		virtual void WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const;
	};

	class StaticApply : public Instruction
	{
	public:
		LocalOffset args; // includes the instance, if there is one
		LocalOffset output;
		Method *method;

		inline StaticApply(Method *method) :
			Instruction(InstrFlags::HAS_INOUT | InstrFlags::INPUT_ON_STACK, OPI_SAPPLY_S),
			args(),
			output(),
			method(method)
		{ }

		inline virtual size_t GetArgsSize() const
		{
			return oa::TWO_LOCALS_AND_VALUE<Method*>::SIZE;
		}

		inline virtual StackChange GetStackChange() const
		{
			return StackChange(1, opcode & 1);
		}

		inline virtual void UpdateInput(LocalOffset offset, bool isOnStack)
		{
			OVUM_ASSERT(isOnStack);
			args = offset;
		}

		inline virtual void UpdateOutput(LocalOffset offset, bool isOnStack)
		{
			output = offset;
			opcode = (IntermediateOpcode)(isOnStack ? opcode | 1 : opcode & ~1);
		}

	protected:
		virtual void WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const;
	};

	class Branch : public Instruction
	{
	public:
		JumpTarget target;

		inline Branch(JumpTarget target, bool isLeave) :
			Instruction(InstrFlags::BRANCH, isLeave ? OPI_LEAVE : OPI_BR),
			target(target)
		{ }

		inline virtual size_t GetArgsSize() const
		{
			return oa::BRANCH_SIZE;
		}

		inline virtual StackChange GetStackChange() const
		{
			return StackChange::empty;
		}

		inline virtual bool IsConditional() const
		{
			return false;
		}

	protected:
		virtual void WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const;

		inline Branch(JumpTarget target, InstrFlags flags, IntermediateOpcode opcode) :
			Instruction(InstrFlags::BRANCH | flags, opcode),
			target(target)
		{ }
	};

	class ConditionalBranch : public Branch
	{
	public:
		LocalOffset value;

		static const int IF_NULL  = 0;
		static const int NOT_NULL = 2;
		static const int IF_FALSE = 4;
		static const int IF_TRUE  = 6;
		static const int IF_TYPE  = 8;

		inline ConditionalBranch(JumpTarget target, int condition) :
			Branch(target, InstrFlags::HAS_INPUT, (IntermediateOpcode)(OPI_BRNULL_S + condition)),
			value()
		{ }

		inline virtual size_t GetArgsSize() const
		{
			return oa::CONDITIONAL_BRANCH_SIZE;
		}

		inline virtual StackChange GetStackChange() const
		{
			return StackChange(opcode & 1, 0);
		}

		inline virtual bool IsConditional() const
		{
			return true;
		}

		inline virtual void UpdateInput(LocalOffset offset, bool isOnStack)
		{
			value = offset;
			opcode = (IntermediateOpcode)(isOnStack ? opcode | 1 : opcode & ~1);
		}

	protected:
		virtual void WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const;
	};

	class BranchIfType : public ConditionalBranch
	{
	public:
		Type *type;

		inline BranchIfType(JumpTarget target, Type *type) :
			ConditionalBranch(target, IF_TYPE),
			type(type)
		{ }

		inline virtual size_t GetArgsSize() const
		{
			return oa::BRANCH_IF_TYPE_SIZE;
		}

		inline virtual bool IsConditional() const
		{
			return true;
		}

	protected:
		virtual void WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const;
	};

	class Switch : public Instruction
	{
	public:
		LocalOffset value;
		size_t targetCount;
		Box<JumpTarget[]> targets;

		inline Switch(size_t targetCount, Box<JumpTarget[]> targets) :
			Instruction(InstrFlags::HAS_INPUT | InstrFlags::SWITCH, OPI_SWITCH_S),
			value(),
			targetCount(targetCount),
			targets(std::move(targets))
		{ }

		inline virtual size_t GetArgsSize() const
		{
			return oa::SWITCH_SIZE(targetCount);
		}

		inline virtual StackChange GetStackChange() const
		{
			return StackChange(opcode & 1, 0);
		}

		inline virtual void UpdateInput(LocalOffset offset, bool isOnStack)
		{
			value = offset;
			opcode = (IntermediateOpcode)(isOnStack ? opcode | 1 : opcode & ~1);
		}

	protected:
		virtual void WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const;
	};

	class BranchIfReference : public Branch
	{
	public:
		LocalOffset args; // on stack

		inline BranchIfReference(JumpTarget target, bool branchIfSame) :
			Branch(target, InstrFlags::HAS_INPUT | InstrFlags::INPUT_ON_STACK, branchIfSame ? OPI_BRREF : OPI_BRNREF),
			args()
		{ }

		inline virtual size_t GetArgsSize() const
		{
			return oa::CONDITIONAL_BRANCH_SIZE;
		}

		inline virtual StackChange GetStackChange() const
		{
			return StackChange(2, 0);
		}

		inline virtual bool IsConditional() const
		{
			return true;
		}

		inline virtual void UpdateInput(LocalOffset offset, bool isOnStack)
		{
			OVUM_ASSERT(isOnStack);
			args = offset;
		}

	protected:
		virtual void WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const;
	};

	class BranchComparison : public Branch
	{
	public:
		LocalOffset args;

		inline BranchComparison(LocalOffset args, JumpTarget target, IntermediateOpcode opcode) :
			Branch(target, InstrFlags::HAS_INPUT | InstrFlags::INPUT_ON_STACK, opcode),
			args(args)
		{ }

		inline virtual size_t GetArgsSize() const
		{
			return oa::CONDITIONAL_BRANCH_SIZE;
		}

		inline virtual StackChange GetStackChange() const
		{
			return StackChange(2, 0);
		}

		inline virtual bool IsConditional() const
		{
			return true;
		}

		inline virtual void UpdateInput(LocalOffset offset, bool isOnStack)
		{
			OVUM_ASSERT(isOnStack);
			args = offset;
		}

	protected:
		virtual void WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const;
	};

	class ExecOperator : public Instruction
	{
	public:
		LocalOffset args;
		LocalOffset output;
		Operator op;

		static const IntermediateOpcode EQ  = OPI_EQ_S;
		static const IntermediateOpcode CMP = OPI_CMP_S;
		static const IntermediateOpcode LT  = OPI_LT_S;
		static const IntermediateOpcode LTE = OPI_LTE_S;
		static const IntermediateOpcode GT  = OPI_GT_S;
		static const IntermediateOpcode GTE = OPI_GTE_S;
		static const IntermediateOpcode CONCAT  = OPI_CONCAT_S;

		static const Operator SINGLE_INSTR_OP = (Operator)-1;

		inline ExecOperator(Operator op) :
			Instruction(
				InstrFlags::HAS_INOUT | InstrFlags::INPUT_ON_STACK,
				GetOpcode(op)
			),
			args(),
			output(),
			op(op)
		{ }
		inline ExecOperator(IntermediateOpcode specialOp) :
			Instruction(InstrFlags::HAS_INOUT | InstrFlags::INPUT_ON_STACK, specialOp),
			args(),
			output(),
			op(SINGLE_INSTR_OP)
		{ }

		inline bool IsUnary() const
		{
			return op == Operator::PLUS || op == Operator::NEG || op == Operator::NOT;
		}

		inline virtual size_t GetArgsSize() const
		{
			if (op == SINGLE_INSTR_OP)
				return oa::TWO_LOCALS_SIZE;
			else
				return oa::TWO_LOCALS_AND_VALUE<Operator>::SIZE;
		}

		inline virtual StackChange GetStackChange() const
		{
			return StackChange(IsUnary() ? 1 : 2, opcode & 1);
		}

		inline virtual void UpdateInput(LocalOffset offset, bool isOnStack)
		{
			OVUM_ASSERT(isOnStack);
			args = offset;
		}

		inline virtual void UpdateOutput(LocalOffset offset, bool isOnStack)
		{
			output = offset;
			opcode = (IntermediateOpcode)(isOnStack ? opcode | 1 : opcode & ~1);
		}

	protected:
		virtual void WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const;

	private:
		inline static IntermediateOpcode GetOpcode(Operator op)
		{
			if (op == Operator::PLUS || op == Operator::NEG || op == Operator::NOT)
				return OPI_UNARYOP_S;
			return OPI_OPERATOR_S;
		}
	};

	class LoadLocalRef : public Instruction
	{
	public:
		LocalOffset local;

		inline LoadLocalRef(LocalOffset local) :
			Instruction(InstrFlags::PUSHES_REF, OPI_LDLOCREF),
			local(local)
		{ }

		inline virtual size_t GetArgsSize() const
		{
			return oa::ONE_LOCAL_SIZE;
		}

		inline virtual StackChange GetStackChange() const
		{
			return StackChange(0, 1);
		}

	protected:
		virtual void WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const;
	};

	class LoadMemberRef : public Instruction
	{
	public:
		LocalOffset instance;
		String *member;

		inline LoadMemberRef(String *member) :
			Instruction(InstrFlags::HAS_INPUT | InstrFlags::PUSHES_REF, OPI_LDMEMREF_S),
			instance(),
			member(member)
		{ }

		inline virtual size_t GetArgsSize() const
		{
			return oa::LOCAL_AND_VALUE<String*>::SIZE;
		}

		inline virtual StackChange GetStackChange() const
		{
			return StackChange(1, 1);
		}

		inline virtual void UpdateInput(LocalOffset offset, bool isOnStack)
		{
			instance = offset;
			opcode = (IntermediateOpcode)(isOnStack ? opcode | 1 : opcode & ~1);
		}

	protected:
		virtual void WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const;
	};

	class LoadFieldRef : public Instruction
	{
	public:
		LocalOffset instance;
		Field *field;

		inline LoadFieldRef(Field *field) :
			Instruction(InstrFlags::HAS_INPUT | InstrFlags::PUSHES_REF, OPI_LDFLDREF_S),
			instance(),
			field(field)
		{ }

		inline virtual size_t GetArgsSize() const
		{
			return oa::LOCAL_AND_VALUE<Field*>::SIZE;
		}

		inline virtual StackChange GetStackChange() const
		{
			return StackChange(1, 1);
		}

		inline virtual void UpdateInput(LocalOffset offset, bool isOnStack)
		{
			instance = offset;
			opcode = (IntermediateOpcode)(isOnStack ? opcode | 1 : opcode & ~1);
		}

	protected:
		virtual void WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const;
	};

	class LoadStaticFieldRef : public Instruction
	{
	public:
		Field *field;

		inline LoadStaticFieldRef(Field *field) :
			Instruction(InstrFlags::PUSHES_REF, OPI_LDSFLDREF),
			field(field)
		{ }

		inline virtual size_t GetArgsSize() const
		{
			return oa::SINGLE_VALUE<Field*>::SIZE;
		}

		inline virtual StackChange GetStackChange() const
		{
			return StackChange(0, 1);
		}

	protected:
		virtual void WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const;
	};
} // namespace instr

} // namespace ovum
