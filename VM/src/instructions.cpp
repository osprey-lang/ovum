#include "instructions.h"
#include "methodbuilder.h"
#include "methodinitializer.h"
#include "refsignature.h"

namespace ovum
{

namespace instr
{
	namespace oa = ovum::opcode_args;

#define ONE_LOCAL(local) oa::OneLocal args = { (local) }
#define TWO_LOCALS(source, dest) oa::TwoLocals args = { (source), (dest) }

	const StackChange StackChange::empty = StackChange(0, 0);

	int NewObject::SetReferenceSignature(const StackManager &stack)
	{
		// We have to treat the stack as if it contained an invisible extra
		// item before the first argument. That's where the instance will
		// go when the constructor is invoked.
		RefSignatureBuilder refBuilder(argCount + 1);

		for (int i = 1; i <= argCount; i++)
			if (stack.IsRef(argCount - i))
				refBuilder.SetParam(i, true);

		refSignature = refBuilder.Commit(stack.GetRefSignaturePool());

		MethodOverload *ctor = type->instanceCtor->ResolveOverload(argCount);
		if (this->refSignature != ctor->refSignature)
			// VerifyRefSignature does NOT include the instance in the argCount
			return ctor->VerifyRefSignature(this->refSignature, argCount);
		return -1;
	}
	
	int Call::SetReferenceSignature(const StackManager &stack)
	{
		refSignature = stack.GetRefSignature(argCount + 1);
		if (refSignature)
			opcode = (IntermediateOpcode)(OPI_CALLR_L | opcode & 1);
		return -1;
	}
	
	int CallMember::SetReferenceSignature(const StackManager &stack)
	{
		refSignature = stack.GetRefSignature(argCount + 1);
		if (refSignature)
			opcode = (IntermediateOpcode)(OPI_CALLMEMR_L | opcode & 1);
		return -1;
	}
	
	int StaticCall::SetReferenceSignature(const StackManager &stack)
	{
		if (method->group->IsStatic())
		{
			RefSignatureBuilder refBuilder(argCount + 1);

			for (int i = 1; i <= argCount; i++)
				if (stack.IsRef(argCount - i))
					refBuilder.SetParam(i, true);

			refSignature = refBuilder.Commit(stack.GetRefSignaturePool());
		}
		else
		{
			refSignature = stack.GetRefSignature(argCount + 1);
		}

		if (this->refSignature != method->refSignature)
			// VerifyRefSignature does NOT include the instance in the argCount
			return method->VerifyRefSignature(this->refSignature, argCount);
		return -1;
	}

	void Instruction::WriteBytes(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		buffer.Write(opcode, oa::ALIGNMENT);
		WriteArguments(buffer, builder);
	}

	void MoveLocal::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		TWO_LOCALS(source, target);
		buffer.Write(args, oa::TWO_LOCALS_SIZE);
	}

	void DupInstr::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		TWO_LOCALS(source, target);
		buffer.Write(args, oa::TWO_LOCALS_SIZE);
	}

	void LoadValue::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		ONE_LOCAL(target);
		buffer.Write(args, oa::ONE_LOCAL_SIZE);
	}

	void LoadInt::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		oa::LocalAndValue<int64_t> args = { target, value };
		buffer.Write(args, oa::LOCAL_AND_VALUE<int64_t>::SIZE);
	}

	void LoadUInt::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		oa::LocalAndValue<uint64_t> args = { target, value };
		buffer.Write(args, oa::LOCAL_AND_VALUE<uint64_t>::SIZE);
	}

	void LoadReal::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		oa::LocalAndValue<double> args = { target, value };
		buffer.Write(args, oa::LOCAL_AND_VALUE<double>::SIZE);
	}

	void LoadString::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		oa::LocalAndValue<String*> args = { target, value };
		buffer.Write(args, oa::LOCAL_AND_VALUE<String*>::SIZE);
	}

	void LoadEnumValue::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		oa::LoadEnum args = { target, type, value };
		buffer.Write(args, oa::LOAD_ENUM_SIZE);
	}

	void NewObject::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		oa::NewObject args = { this->args, target, argCount, type };
		buffer.Write(args, oa::NEW_OBJECT_SIZE);
	}

	void CreateList::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		oa::LocalAndValue<int32_t> args = { target, capacity };
		buffer.Write(args, oa::LOCAL_AND_VALUE<int32_t>::SIZE);
	}

	void CreateHash::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		oa::LocalAndValue<int32_t> args = { target, capacity };
		buffer.Write(args, oa::LOCAL_AND_VALUE<int32_t>::SIZE);
	}

	void LoadStaticFunction::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		oa::LocalAndValue<Method*> args = { target, method };
		buffer.Write(args, oa::LOCAL_AND_VALUE<Method*>::SIZE);
	}

	void LoadTypeToken::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		oa::LocalAndValue<Type*> args = { target, type };
		buffer.Write(args, oa::LOCAL_AND_VALUE<Type*>::SIZE);
	}

	void LoadMember::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		oa::TwoLocalsAndValue<String*> args = { instance, output, member };
		buffer.Write(args, oa::TWO_LOCALS_AND_VALUE<String*>::SIZE);
	}

	void StoreMember::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		oa::LocalAndValue<String*> args = { this->args, member };
		buffer.Write(args, oa::LOCAL_AND_VALUE<String*>::SIZE);
	}

	void LoadField::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		oa::TwoLocalsAndValue<Field*> args = { instance, output, field };
		buffer.Write(args, oa::TWO_LOCALS_AND_VALUE<Field*>::SIZE);
	}

	void StoreField::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		oa::LocalAndValue<Field*> args = { this->args, field };
		buffer.Write(args, oa::LOCAL_AND_VALUE<Field*>::SIZE);
	}

	void LoadStaticField::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		oa::LocalAndValue<Field*> args = { target, field };
		buffer.Write(args, oa::LOCAL_AND_VALUE<Field*>::SIZE);
	}

	void StoreStaticField::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		oa::LocalAndValue<Field*> args = { value, field };
		buffer.Write(args, oa::LOCAL_AND_VALUE<Field*>::SIZE);
	}

	void LoadIterator::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		TWO_LOCALS(value, output);
		buffer.Write(args, oa::TWO_LOCALS_SIZE);
	}

	void LoadType::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		TWO_LOCALS(source, target);
		buffer.Write(args, oa::TWO_LOCALS_SIZE);
	}

	void LoadIndexer::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		oa::TwoLocalsAndValue<uint32_t> args = { this->args, output, argCount };
		buffer.Write(args, oa::TWO_LOCALS_AND_VALUE<uint32_t>::SIZE);
	}

	void StoreIndexer::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		oa::LocalAndValue<uint32_t> args = { this->args, argCount };
		buffer.Write(args, oa::LOCAL_AND_VALUE<uint32_t>::SIZE);
	}

	void Call::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		// The final instruction DOES include the value to be invoked
		if (refSignature)
		{
			oa::CallRef args = { this->args, output, argCount, refSignature };
			buffer.Write(args, oa::CALL_REF_SIZE);
		}
		else
		{
			oa::Call args = { this->args, output, argCount };
			buffer.Write(args, oa::CALL_SIZE);
		}
	}

	void CallMember::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		if (refSignature)
		{
			oa::CallMemberRef args = {
				this->args,
				output,
				argCount,
				refSignature,
				member,
			};
			buffer.Write(args, oa::CALL_MEMBER_REF_SIZE);
		}
		else
		{
			oa::CallMember args = { this->args, output, argCount, member };
			buffer.Write(args, oa::CALL_MEMBER_SIZE);
		}
	}

	void StaticCall::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		// The scall instruction does NOT include the instance in its argCount.
		oa::StaticCall args = { this->args, output, argCount, method };
		buffer.Write(args, oa::STATIC_CALL_SIZE);
	}

	void Apply::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		TWO_LOCALS(this->args, output);
		buffer.Write(args, oa::TWO_LOCALS_SIZE);
	}

	void StaticApply::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		oa::TwoLocalsAndValue<Method*> args = { this->args, output, method };
		buffer.Write(args, oa::TWO_LOCALS_AND_VALUE<Method*>::SIZE);
	}

	void Branch::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		oa::Branch args = { builder.GetNewOffset(target, this) };
		buffer.Write(args, oa::BRANCH_SIZE);
	}

	void ConditionalBranch::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		oa::ConditionalBranch args = { value, builder.GetNewOffset(target, this) };
		buffer.Write(args, oa::CONDITIONAL_BRANCH_SIZE);
	}

	void BranchIfType::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		oa::BranchIfType args = { value, builder.GetNewOffset(target, this), type };
		buffer.Write(args, oa::BRANCH_IF_TYPE_SIZE);
	}

	void Switch::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		oa::Switch args = { value, targetCount, 0 };
		// Write the whole oa::Switch, but only increment it up to the first target
		buffer.Write(args, sizeof(oa::Switch) - sizeof(int32_t));

		// The buffer pointer should now be properly aligned for the first target,
		// so let's just write them all out, shall we?
		for (uint16_t i = 0; i < targetCount; i++)
		{
			buffer.Write(builder.GetNewOffset(targets[i], this), sizeof(int32_t));
		}

		// Make sure the buffer is properly aligned afterwards as well
		buffer.AlignTo(oa::ALIGNMENT);
	}

	void BranchIfReference::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		oa::ConditionalBranch args = { this->args, builder.GetNewOffset(target, this) };
		buffer.Write(args, oa::CONDITIONAL_BRANCH_SIZE);
	}

	void BranchComparison::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		oa::ConditionalBranch args = { this->args, builder.GetNewOffset(target, this) };
		buffer.Write(args, oa::CONDITIONAL_BRANCH_SIZE);
	}

	void ExecOperator::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		// When op is negative (or 0xff, rather), it means the operator has
		// a special opcode. This applies to: <  <=  >  >=  ::
		// The operators == and <=> also have their own opcodes.
		if ((uint8_t)op != 0xff && op != Operator::EQ && op != Operator::CMP)
		{
			// Gotta output the operator if it doesn't have its own opcode.
			oa::TwoLocalsAndValue<Operator> args = { this->args, output, op };
			buffer.Write(args, oa::TWO_LOCALS_AND_VALUE<Operator>::SIZE);
		}
		else
		{
			// Just the two local offsets are fine.
			TWO_LOCALS(this->args, output);
			buffer.Write(args, oa::TWO_LOCALS_SIZE);
		}
	}

	void LoadLocalRef::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		ONE_LOCAL(local);
		buffer.Write(args, oa::ONE_LOCAL_SIZE);
	}

	void LoadMemberRef::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		oa::LocalAndValue<String*> args = { instance, member };
		buffer.Write(args, oa::LOCAL_AND_VALUE<String*>::SIZE);
	}

	void LoadFieldRef::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		oa::LocalAndValue<Field*> args = { instance, field };
		buffer.Write(args, oa::LOCAL_AND_VALUE<Field*>::SIZE);
	}

	void LoadStaticFieldRef::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		oa::SingleValue<Field*> args = { field };
		buffer.Write(args, oa::SINGLE_VALUE<Field*>::SIZE);
	}
} // namespace instr

} // namespace ovum