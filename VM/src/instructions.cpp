#include "instructions.internal.h"
#include "methodbuilder.internal.h"
#include "methodinitializer.internal.h"
#include "refsignature.internal.h"

namespace ovum
{

namespace instr
{
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
		buffer.Write(opcode);
		WriteArguments(buffer, builder);
	}

	void MoveLocal::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		buffer.Write(source);
		buffer.Write(target);
	}

	void DupInstr::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		buffer.Write(source);
		buffer.Write(target);
	}

	void LoadInt::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		WriteTarget(buffer);
		buffer.Write(value);
	}

	void LoadUInt::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		WriteTarget(buffer);
		buffer.Write(value);
	}

	void LoadReal::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		WriteTarget(buffer);
		buffer.Write(value);
	}

	void LoadString::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		WriteTarget(buffer);
		buffer.Write(value);
	}

	void LoadEnumValue::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		WriteTarget(buffer);
		buffer.Write(type);
		buffer.Write(value);
	}

	void NewObject::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		buffer.Write(args);
		buffer.Write(target);
		buffer.Write(type);
		buffer.Write(argCount);
	}

	void CreateList::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		WriteTarget(buffer);
		buffer.Write(capacity);
	}

	void CreateHash::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		WriteTarget(buffer);
		buffer.Write(capacity);
	}

	void LoadStaticFunction::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		WriteTarget(buffer);
		buffer.Write(method);
	}

	void LoadTypeToken::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		WriteTarget(buffer);
		buffer.Write(type);
	}

	void LoadMember::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		buffer.Write(instance);
		buffer.Write(output);
		buffer.Write(member);
	}

	void StoreMember::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		buffer.Write(args);
		buffer.Write(member);
	}

	void LoadField::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		buffer.Write(instance);
		buffer.Write(output);
		buffer.Write(field);
	}

	void StoreField::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		buffer.Write(args);
		buffer.Write(field);
	}

	void LoadStaticField::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		WriteTarget(buffer);
		buffer.Write(field);
	}

	void StoreStaticField::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		buffer.Write(value);
		buffer.Write(field);
	}

	void LoadIterator::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		buffer.Write(value);
		buffer.Write(output);
	}

	void LoadType::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		buffer.Write(source);
		buffer.Write(target);
	}

	void LoadIndexer::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		buffer.Write(args);
		buffer.Write(output);
		buffer.Write(argCount);
	}

	void StoreIndexer::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		buffer.Write(args);
		buffer.Write(argCount);
	}

	void Call::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		// The final instruction DOES include the value to be invoked
		buffer.Write(args);
		buffer.Write(output);
		buffer.Write(argCount);
		if (refSignature)
		{
			buffer.Write(refSignature);
		}
	}

	void CallMember::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		buffer.Write(args);
		buffer.Write(output);
		buffer.Write(member);
		buffer.Write(argCount);
		if (refSignature)
		{
			buffer.Write(refSignature);
		}
	}

	void StaticCall::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		// The scall instruction does NOT include the instance in its argCount.
		buffer.Write(args);
		buffer.Write(output);
		buffer.Write(argCount);
		buffer.Write(method);
	}

	void Apply::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		buffer.Write(args);
		buffer.Write(output);
	}

	void StaticApply::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		buffer.Write(args);
		buffer.Write(output);
		buffer.Write(method);
	}

	void Branch::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		buffer.Write(builder.GetNewOffset(target, this));
	}

	void ConditionalBranch::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		buffer.Write(value);
		buffer.Write(builder.GetNewOffset(target, this));
	}

	void BranchIfType::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		buffer.Write(value);
		buffer.Write(type);
		buffer.Write(builder.GetNewOffset(target, this));
	}

	void Switch::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		buffer.Write(value);
		buffer.Write(targetCount);

		for (uint16_t i = 0; i < targetCount; i++)
		{
			buffer.Write(builder.GetNewOffset(targets[i], this));
		}
	}

	void BranchIfReference::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		buffer.Write(args);
		buffer.Write(builder.GetNewOffset(target, this));
	}

	void BranchComparison::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		buffer.Write(args);
		buffer.Write(builder.GetNewOffset(target, this));
	}

	void ExecOperator::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		buffer.Write(args);
		buffer.Write(output);

		// The op is negative (or 0xff, rather) when the operator
		// is one of: <  <=  >  >=  ::
		// (Because there are specialised opcodes for those)
		// Similarly, there are specialised opcodes for == and <=>
		if ((uint8_t)op != 0xff && op != Operator::EQ && op != Operator::CMP)
		{
			buffer.Write(op);
		}
	}

	void LoadLocalRef::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		buffer.Write(local);
	}

	void LoadMemberRef::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		buffer.Write(instance);
		buffer.Write(member);
	}

	void LoadFieldRef::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		buffer.Write(instance);
		buffer.Write(field);
	}

	void LoadStaticFieldRef::WriteArguments(MethodBuffer &buffer, MethodBuilder &builder) const
	{
		buffer.Write(field);
	}
} // namespace instr

} // namespace ovum