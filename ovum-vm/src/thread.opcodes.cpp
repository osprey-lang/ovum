#include "vm.h"
#include "thread.opcodes.h"
#include <memory>

namespace ovum
{

#define OPC_ARGS(T) register const T *const args = reinterpret_cast<const T*>(ip)

// Used in Thread::Evaluate. Semicolon intentionally missing.
#define CHK(expr) do { if ((retCode = (expr)) != OVUM_SUCCESS) goto exitMethod; } while (0)

#define TARGET(opc) case opc:
#define NEXT_INSTR() break

#define SET_BOOL(ptarg, bvalue) \
	{                                       \
		(ptarg)->type = vm->types.Boolean;  \
		(ptarg)->integer = bvalue;          \
	}
#define SET_INT(ptarg, ivalue) \
	{                                       \
		(ptarg)->type = vm->types.Int;      \
		(ptarg)->integer = ivalue;          \
	}
#define SET_UINT(ptarg, uvalue) \
	{                                       \
		(ptarg)->type = vm->types.UInt;     \
		(ptarg)->uinteger = uvalue;         \
	}
#define SET_REAL(ptarg, rvalue) \
	{                                       \
		(ptarg)->type = vm->types.Real;     \
		(ptarg)->real = rvalue;             \
	}
#define SET_STRING(ptarg, svalue) \
	{                                       \
		(ptarg)->type = vm->types.String;   \
		(ptarg)->common.string = svalue;    \
	}

int Thread::Evaluate()
{
	namespace oa = ovum::opcode_args; // For convenience

	if (pendingRequest != ThreadRequest::NONE)
		HandleRequest();

	int retCode;

	register StackFrame *const f = currentFrame;
	// this->ip has been set to the entry address
	register uint8_t *ip = this->ip;

	while (true)
	{
		this->ip = ip;
		// Always skip the opcode
		ip += ALIGN_TO(sizeof(IntermediateOpcode), oa::ALIGNMENT);
		switch (*this->ip)
		{
		TARGET(OPI_NOP) NEXT_INSTR(); // Really, do nothing!

		TARGET(OPI_POP)
			{
				f->stackCount--; // pop just decrements the stack height
			}
			NEXT_INSTR();

		TARGET(OPI_RET)
			{
				assert(f->stackCount == 1);
			}
			retCode = OVUM_SUCCESS;
			goto ret;

		TARGET(OPI_RETNULL)
			{
				assert(f->stackCount == 0);
				f->evalStack->type = nullptr;
				f->stackCount++;
			}
			retCode = OVUM_SUCCESS;
			goto ret;

		// mvloc: LocalOffset source, LocalOffset destination
		TARGET(OPI_MVLOC_LL) // local to local
			{
				OPC_ARGS(oa::TwoLocals);
				*args->Dest(f) = *args->Source(f);
				ip += oa::TWO_LOCALS_SIZE;
			}
			NEXT_INSTR();
		TARGET(OPI_MVLOC_SL) // stack to local
			{
				OPC_ARGS(oa::TwoLocals);
				*args->Dest(f) = *args->Source(f);
				ip += oa::TWO_LOCALS_SIZE;
				f->stackCount--;
			}
			NEXT_INSTR();
		TARGET(OPI_MVLOC_LS) // local to stack
			{
				OPC_ARGS(oa::TwoLocals);
				*args->Dest(f) = *args->Source(f);
				ip += oa::TWO_LOCALS_SIZE;
				f->stackCount++;
			}
			NEXT_INSTR();
		TARGET(OPI_MVLOC_SS) // stack to stack (shouldn't really be used!)
			{
				OPC_ARGS(oa::TwoLocals);
				*args->Dest(f) = *args->Source(f);
				ip += oa::TWO_LOCALS_SIZE;
			}
			NEXT_INSTR();

		// ldnull: LocalOffset dest
		TARGET(OPI_LDNULL_L)
			{
				OPC_ARGS(oa::OneLocal);
				args->Local(f)->type = nullptr;
				ip += oa::ONE_LOCAL_SIZE;
			}
			NEXT_INSTR();
		TARGET(OPI_LDNULL_S)
			{
				OPC_ARGS(oa::OneLocal);
				args->Local(f)->type = nullptr;
				ip += oa::ONE_LOCAL_SIZE;
				f->stackCount++;
			}
			NEXT_INSTR();

		// ldfalse: LocalOffset dest
		TARGET(OPI_LDFALSE_L)
			{
				OPC_ARGS(oa::OneLocal);
				SET_BOOL(args->Local(f), false);
				ip += oa::ONE_LOCAL_SIZE;
			}
			NEXT_INSTR();
		TARGET(OPI_LDFALSE_S)
			{
				OPC_ARGS(oa::OneLocal);
				SET_BOOL(args->Local(f), false);
				ip += oa::ONE_LOCAL_SIZE;
				f->stackCount++;
			}
			NEXT_INSTR();

		// ldtrue: LocalOffset dest
		TARGET(OPI_LDTRUE_L)
			{
				OPC_ARGS(oa::OneLocal);
				SET_BOOL(args->Local(f), true);
				ip += oa::ONE_LOCAL_SIZE;
			}
			NEXT_INSTR();
		TARGET(OPI_LDTRUE_S)
			{
				OPC_ARGS(oa::OneLocal);
				SET_BOOL(args->Local(f), true);
				ip += oa::ONE_LOCAL_SIZE;
				f->stackCount++;
			}
			NEXT_INSTR();

		// ldc.i: LocalOffset dest, int64_t value
		TARGET(OPI_LDC_I_L)
			{
				OPC_ARGS(oa::LocalAndValue<int64_t>);
				SET_INT(args->Local(f), args->value);
				ip += oa::LOCAL_AND_VALUE<int64_t>::SIZE;
			}
			NEXT_INSTR();
		TARGET(OPI_LDC_I_S)
			{
				OPC_ARGS(oa::LocalAndValue<int64_t>);
				SET_INT(args->Local(f), args->value);
				ip += oa::LOCAL_AND_VALUE<int64_t>::SIZE;
				f->stackCount++;
			}
			NEXT_INSTR();

		// ldc.u: LocalOffset dest, uint64_t value
		TARGET(OPI_LDC_U_L)
			{
				OPC_ARGS(oa::LocalAndValue<uint64_t>);
				SET_UINT(args->Local(f), args->value);
				ip += oa::LOCAL_AND_VALUE<uint64_t>::SIZE;
			}
			NEXT_INSTR();
		TARGET(OPI_LDC_U_S)
			{
				OPC_ARGS(oa::LocalAndValue<uint64_t>);
				SET_UINT(args->Local(f), args->value);
				ip += oa::LOCAL_AND_VALUE<uint64_t>::SIZE;
				f->stackCount++;
			}
			NEXT_INSTR();

		// ldc.r: LocalOffset dest, double value
		TARGET(OPI_LDC_R_L)
			{
				OPC_ARGS(oa::LocalAndValue<double>);
				SET_REAL(args->Local(f), args->value);
				ip += oa::LOCAL_AND_VALUE<double>::SIZE;
			}
			NEXT_INSTR();
		TARGET(OPI_LDC_R_S)
			{
				OPC_ARGS(oa::LocalAndValue<double>);
				SET_REAL(args->Local(f), args->value);
				ip += oa::LOCAL_AND_VALUE<double>::SIZE;
				f->stackCount++;
			}
			NEXT_INSTR();

		// ldstr: LocalOffset dest, String *value
		TARGET(OPI_LDSTR_L)
			{
				OPC_ARGS(oa::LocalAndValue<String*>);
				SET_STRING(args->Local(f), args->value);
				ip += oa::LOCAL_AND_VALUE<String*>::SIZE;
			}
			NEXT_INSTR();
		TARGET(OPI_LDSTR_S)
			{
				OPC_ARGS(oa::LocalAndValue<String*>);
				SET_STRING(args->Local(f), args->value);
				ip += oa::LOCAL_AND_VALUE<String*>::SIZE;
				f->stackCount++;
			}
			NEXT_INSTR();

		// ldargc: LocalOffset dest
		TARGET(OPI_LDARGC_L)
			{
				OPC_ARGS(oa::OneLocal);
				SET_INT(args->Local(f), f->argc);
				ip += oa::ONE_LOCAL_SIZE;
			}
			NEXT_INSTR();
		TARGET(OPI_LDARGC_S)
			{
				OPC_ARGS(oa::OneLocal);
				SET_INT(args->Local(f), f->argc);
				ip += oa::ONE_LOCAL_SIZE;
				f->stackCount++;
			}
			NEXT_INSTR();

		// ldenum: LocalOffset dest, Type *type, int64_t value
		TARGET(OPI_LDENUM_L)
			{
				OPC_ARGS(oa::LoadEnum);
				register Value *const dest = args->Dest(f);
				dest->type = args->type;
				dest->integer = args->value;
				ip += oa::LOAD_ENUM_SIZE;
			}
			NEXT_INSTR();
		TARGET(OPI_LDENUM_S)
			{
				OPC_ARGS(oa::LoadEnum);
				register Value *const dest = args->Dest(f);
				dest->type = args->type;
				dest->integer = args->value;
				ip += oa::LOAD_ENUM_SIZE;
				f->stackCount++;
			}
			NEXT_INSTR();

		// newobj: LocalOffset args, LocalOffset dest, uint32_t argc, Type *type
		TARGET(OPI_NEWOBJ_L)
			{
				OPC_ARGS(oa::NewObject);
				CHK(GetGC()->ConstructLL(this, args->type, args->argc, args->Args(f), args->Dest(f)));
				ip += oa::NEW_OBJECT_SIZE;
				// ConstructLL pops the arguments
			}
			NEXT_INSTR();
		TARGET(OPI_NEWOBJ_S)
			{
				OPC_ARGS(oa::NewObject);
				CHK(GetGC()->ConstructLL(this, args->type, args->argc, args->Args(f), args->Dest(f)));
				ip += oa::NEW_OBJECT_SIZE;
				// ConstructLL pops the arguments
				f->stackCount++;
			}
			NEXT_INSTR();

		// list: LocalOffset dest, int32_t capacity
		TARGET(OPI_LIST_L)
			{
				OPC_ARGS(oa::LocalAndValue<int32_t>);
				Value result; // Can't put it in dest until it's fully initialized
				CHK(GetGC()->Alloc(this, vm->types.List, sizeof(ListInst), &result));
				CHK(vm->functions.initListInstance(this, result.common.list, args->value));
				*args->Local(f) = result;

				ip += oa::LOCAL_AND_VALUE<int32_t>::SIZE;
			}
			NEXT_INSTR();
		TARGET(OPI_LIST_S)
			{
				OPC_ARGS(oa::LocalAndValue<int32_t>);
				Value result; // Can't put it in dest until it's fully initialized
				CHK(GetGC()->Alloc(this, vm->types.List, sizeof(ListInst), &result));
				CHK(vm->functions.initListInstance(this, result.common.list, args->value));
				*args->Local(f) = result;

				ip += oa::LOCAL_AND_VALUE<int32_t>::SIZE;
				f->stackCount++;
			}
			NEXT_INSTR();

		// hash: LocalOffset dest, int32_t capacity
		TARGET(OPI_HASH_L)
			{
				OPC_ARGS(oa::LocalAndValue<int32_t>);
				Value result; // Can't put it in dest until it's fully initialized
				CHK(GetGC()->Alloc(this, vm->types.Hash, sizeof(HashInst), &result));
				CHK(vm->functions.initHashInstance(this, result.common.hash, args->value));
				*args->Local(f) = result;

				ip += oa::LOCAL_AND_VALUE<int32_t>::SIZE;
			}
			NEXT_INSTR();
		TARGET(OPI_HASH_S)
			{
				OPC_ARGS(oa::LocalAndValue<int32_t>);
				Value result; // Can't put it in dest until it's fully initialized
				CHK(GetGC()->Alloc(this, vm->types.Hash, sizeof(HashInst), &result));
				CHK(vm->functions.initHashInstance(this, result.common.hash, args->value));
				*args->Local(f) = result;

				ip += oa::LOCAL_AND_VALUE<int32_t>::SIZE;
				f->stackCount++;
			}
			NEXT_INSTR();

		// ldfld: LocalOffset instance, LocalOffset dest, Field *field
		TARGET(OPI_LDFLD_L)
			{
				OPC_ARGS(oa::TwoLocalsAndValue<Field*>);
				CHK(args->value->ReadField(this, args->Source(f), args->Dest(f)));
				ip += oa::TWO_LOCALS_AND_VALUE<Field*>::SIZE;
				// The instance is read from the stack, and the field
				// value is put in a local. One item removed.
				f->stackCount--;
			}
			NEXT_INSTR();
		TARGET(OPI_LDFLD_S)
			{
				OPC_ARGS(oa::TwoLocalsAndValue<Field*>);
				CHK(args->value->ReadField(this, args->Source(f), args->Dest(f)));
				ip += oa::TWO_LOCALS_AND_VALUE<Field*>::SIZE;
				// The instance is read from the stack, and the field
				// value is pushed right back onto it. No change.
			}
			NEXT_INSTR();

		// ldsfld: LocalOffset dest, Field *field
		TARGET(OPI_LDSFLD_L)
			{
				OPC_ARGS(oa::LocalAndValue<Field*>);
				args->value->staticValue->Read(args->Local(f));
				ip += oa::LOCAL_AND_VALUE<Field*>::SIZE;
			}
			NEXT_INSTR();
		TARGET(OPI_LDSFLD_S)
			{
				OPC_ARGS(oa::LocalAndValue<Field*>);
				args->value->staticValue->Read(args->Local(f));
				ip += oa::LOCAL_AND_VALUE<Field*>::SIZE;
				f->stackCount++;
			}
			NEXT_INSTR();

		// ldmem: LocalOffset instance, LocalOffset dest, String *name
		TARGET(OPI_LDMEM_L)
			{
				OPC_ARGS(oa::TwoLocalsAndValue<String*>);
				CHK(LoadMemberLL(args->Source(f), args->value, args->Dest(f)));
				ip += oa::TWO_LOCALS_AND_VALUE<String*>::SIZE;
				// LoadMemberLL pops the instance
			}
			NEXT_INSTR();
		TARGET(OPI_LDMEM_S)
			{
				OPC_ARGS(oa::TwoLocalsAndValue<String*>);
				CHK(LoadMemberLL(args->Source(f), args->value, args->Dest(f)));
				ip += oa::TWO_LOCALS_AND_VALUE<String*>::SIZE;
				// LoadMemberLL pops the instance
				f->stackCount++;
			}
			NEXT_INSTR();

		// lditer: LocalOffset instance, LocalOffest dest
		TARGET(OPI_LDITER_L)
			{
				OPC_ARGS(oa::TwoLocals);
				CHK(InvokeMemberLL(static_strings::_iter, 0, args->Source(f), args->Dest(f), 0));
				// InvokeMemberLL pops the instance and all 0 of the arguments
				ip += oa::TWO_LOCALS_SIZE;
			}
			NEXT_INSTR();
		TARGET(OPI_LDITER_S)
			{
				OPC_ARGS(oa::TwoLocals);
				CHK(InvokeMemberLL(static_strings::_iter, 0, args->Source(f), args->Dest(f), 0));
				// InvokeMemberLL pops the instance and all 0 of the arguments
				ip += oa::TWO_LOCALS_SIZE;
				f->stackCount++;
			}
			NEXT_INSTR();

		// ldtype: LocalOffset instance, LocalOffset dest
		TARGET(OPI_LDTYPE_L)
			{
				OPC_ARGS(oa::TwoLocals);
				register Value *const inst = args->Source(f);

				if (inst->type)
					CHK(inst->type->GetTypeToken(this, args->Dest(f)));
				else
					args->Dest(f)->type = nullptr;

				ip += oa::TWO_LOCALS_SIZE;
				f->stackCount--;
			}
			NEXT_INSTR();
		TARGET(OPI_LDTYPE_S)
			{
				OPC_ARGS(oa::TwoLocals);
				register Value *const inst = args->Source(f);

				if (inst->type)
					CHK(inst->type->GetTypeToken(this, args->Dest(f)));
				else
					args->Dest(f)->type = nullptr;

				ip += oa::TWO_LOCALS_SIZE;
			}
			NEXT_INSTR();

		// ldidx: LocalOffset args, LocalOffset dest, uint32_t argc
		// Note: argc does not include the instance
		TARGET(OPI_LDIDX_L)
			{
				OPC_ARGS(oa::TwoLocalsAndValue<uint32_t>);

				CHK(LoadIndexerLL(args->value, args->Source(f), args->Dest(f)));

				// LoadIndexerLL decrements the stack height by the argument count + instance
				ip += oa::TWO_LOCALS_AND_VALUE<uint32_t>::SIZE;
			}
			NEXT_INSTR();
		TARGET(OPI_LDIDX_S)
			{
				OPC_ARGS(oa::TwoLocalsAndValue<uint32_t>);

				CHK(LoadIndexerLL(args->value, args->Source(f), args->Dest(f)));

				// LoadIndexerLL decrements the stack height by the argument count + instance
				ip += oa::TWO_LOCALS_AND_VALUE<uint32_t>::SIZE;
				f->stackCount++;
			}
			NEXT_INSTR();

		// ldsfn: LocalOffset dest, Method *method
		TARGET(OPI_LDSFN_L)
			{
				OPC_ARGS(oa::LocalAndValue<Method*>);
				register Value *const dest = args->Local(f);
				CHK(GetGC()->Alloc(this, vm->types.Method, sizeof(MethodInst), dest));
				dest->common.method->method = args->value;

				ip += oa::LOCAL_AND_VALUE<Method*>::SIZE;
			}
			NEXT_INSTR();
		TARGET(OPI_LDSFN_S)
			{
				OPC_ARGS(oa::LocalAndValue<Method*>);
				register Value *const dest = args->Local(f);
				CHK(GetGC()->Alloc(this, vm->types.Method, sizeof(MethodInst), dest));
				dest->common.method->method = args->value;

				ip += oa::LOCAL_AND_VALUE<Method*>::SIZE;
				f->stackCount++;
			}
			NEXT_INSTR();

		// ldtypetkn: LocalOffset dest, Type *type
		TARGET(OPI_LDTYPETKN_L)
			{
				OPC_ARGS(oa::LocalAndValue<Type*>);
				CHK(args->value->GetTypeToken(this, args->Local(f)));
				ip += oa::LOCAL_AND_VALUE<Type*>::SIZE;
			}
			NEXT_INSTR();
		TARGET(OPI_LDTYPETKN_S)
			{
				OPC_ARGS(oa::LocalAndValue<Type*>);
				CHK(args->value->GetTypeToken(this, args->Local(f)));
				ip += oa::LOCAL_AND_VALUE<Type*>::SIZE;
				f->stackCount++;
			}
			NEXT_INSTR();

		// call: LocalOffset args, LocalOffset dest, uint32_t argc
		TARGET(OPI_CALL_L)
			{
				OPC_ARGS(oa::Call);
				CHK(InvokeLL(args->argc, args->Args(f), args->Dest(f), 0));
				ip += oa::CALL_SIZE;
				// InvokeLL pops the arguments
			}
			NEXT_INSTR();
		TARGET(OPI_CALL_S)
			{
				OPC_ARGS(oa::Call);
				CHK(InvokeLL(args->argc, args->Args(f), args->Dest(f), 0));
				ip += oa::CALL_SIZE;
				// InvokeLL pops the arguments
				f->stackCount++;
			}
			NEXT_INSTR();

		// scall: LocalOffset args, LocalOffset dest, uint32_t argc, MethodOverload *method
		TARGET(OPI_SCALL_L)
			{
				OPC_ARGS(oa::StaticCall);
				CHK(InvokeMethodOverload(args->method, args->argc, args->Args(f), args->Dest(f)));
				ip += oa::STATIC_CALL_SIZE;
			}
			NEXT_INSTR();
		TARGET(OPI_SCALL_S)
			{
				OPC_ARGS(oa::StaticCall);
				CHK(InvokeMethodOverload(args->method, args->argc, args->Args(f), args->Dest(f)));
				ip += oa::STATIC_CALL_SIZE;
				f->stackCount++;
			}
			NEXT_INSTR();

		// apply: LocalOffset args, LocalOffset dest
		TARGET(OPI_APPLY_L)
			{
				OPC_ARGS(oa::TwoLocals);
				CHK(InvokeApplyLL(args->Source(f), args->Dest(f)));
				ip += oa::TWO_LOCALS_SIZE;
				// InvokeApplyLL pops the arguments
			}
			NEXT_INSTR();
		TARGET(OPI_APPLY_S)
			{
				OPC_ARGS(oa::TwoLocals);
				CHK(InvokeApplyLL(args->Source(f), args->Dest(f)));
				ip += oa::TWO_LOCALS_SIZE;
				// InvokeApplyLL pops the arguments
				f->stackCount++;
			}
			NEXT_INSTR();

		// sapply: LocalOffset args, LocalOffset dest, Method *method
		TARGET(OPI_SAPPLY_L)
			{
				OPC_ARGS(oa::TwoLocalsAndValue<Method*>);
				CHK(InvokeApplyMethodLL(args->value, args->Source(f), args->Dest(f)));
				ip += oa::TWO_LOCALS_AND_VALUE<Method*>::SIZE;
				// InvokeApplyMethodLL pops the arguments
			}
			NEXT_INSTR();
		TARGET(OPI_SAPPLY_S)
			{
				OPC_ARGS(oa::TwoLocalsAndValue<Method*>);
				CHK(InvokeApplyMethodLL(args->value, args->Source(f), args->Dest(f)));
				ip += oa::TWO_LOCALS_AND_VALUE<Method*>::SIZE;
				// InvokeApplyMethodLL pops the arguments
				f->stackCount++;
			}
			NEXT_INSTR();

		// br: int32_t offset
		TARGET(OPI_BR)
			{
				OPC_ARGS(oa::Branch);
				ip += args->offset;
				ip += oa::BRANCH_SIZE;
			}
			NEXT_INSTR();

		// leave: int32_t offset
		TARGET(OPI_LEAVE)
			{
				OPC_ARGS(oa::Branch);
				CHK(EvaluateLeave(f, args->offset));
				ip += args->offset;
				ip += oa::BRANCH_SIZE;
			}
			NEXT_INSTR();

		// brnull: LocalOffset value, int32_t offset
		TARGET(OPI_BRNULL_L)
			{
				OPC_ARGS(oa::ConditionalBranch);
				if (args->Value(f)->type == nullptr)
					ip += args->offset;
				ip += oa::CONDITIONAL_BRANCH_SIZE;
			}
			NEXT_INSTR();
		TARGET(OPI_BRNULL_S)
			{
				OPC_ARGS(oa::ConditionalBranch);
				if (args->Value(f)->type == nullptr)
					ip += args->offset;
				ip += oa::CONDITIONAL_BRANCH_SIZE;
				f->stackCount--;
			}
			NEXT_INSTR();

		// brinst: LocalOffset value, int32_t offset
		TARGET(OPI_BRINST_L)
			{
				OPC_ARGS(oa::ConditionalBranch);
				if (args->Value(f)->type != nullptr)
					ip += args->offset;
				ip += oa::CONDITIONAL_BRANCH_SIZE;
			}
			NEXT_INSTR();
		TARGET(OPI_BRINST_S)
			{
				OPC_ARGS(oa::ConditionalBranch);
				if (args->Value(f)->type != nullptr)
					ip += args->offset;
				ip += oa::CONDITIONAL_BRANCH_SIZE;
				f->stackCount--;
			}
			NEXT_INSTR();

		// brfalse: LocalOffset value, int32_t offset
		TARGET(OPI_BRFALSE_L)
			{
				OPC_ARGS(oa::ConditionalBranch);
				if (IsFalse_(args->Value(f)))
					ip += args->offset;
				ip += oa::CONDITIONAL_BRANCH_SIZE;
			}
			NEXT_INSTR();
		TARGET(OPI_BRFALSE_S)
			{
				OPC_ARGS(oa::ConditionalBranch);
				if (IsFalse_(args->Value(f)))
					ip += args->offset;
				ip += oa::CONDITIONAL_BRANCH_SIZE;
				f->stackCount--;
			}
			NEXT_INSTR();

		// brtrue: LocalOffset value, int32_t offset
		TARGET(OPI_BRTRUE_L)
			{
				OPC_ARGS(oa::ConditionalBranch);
				if (IsTrue_(args->Value(f)))
					ip += args->offset;
				ip += oa::CONDITIONAL_BRANCH_SIZE;
			}
			NEXT_INSTR();
		TARGET(OPI_BRTRUE_S)
			{
				OPC_ARGS(oa::ConditionalBranch);
				if (IsTrue_(args->Value(f)))
					ip += args->offset;
				ip += oa::CONDITIONAL_BRANCH_SIZE;
				f->stackCount--;
			}
			NEXT_INSTR();

		// brtype: LocalOffset value, int32_t offset, Type *type
		TARGET(OPI_BRTYPE_L)
			{
				OPC_ARGS(oa::BranchIfType);
				if (Type::ValueIsType(args->Value(f), args->type))
					ip += args->offset;
				ip += oa::BRANCH_IF_TYPE_SIZE;
			}
			NEXT_INSTR();
		TARGET(OPI_BRTYPE_S)
			{
				OPC_ARGS(oa::BranchIfType);
				if (Type::ValueIsType(args->Value(f), args->type))
					ip += args->offset;
				ip += oa::BRANCH_IF_TYPE_SIZE;
			}
			NEXT_INSTR();

		// switch: LocalOffset value, uint16_t count, int32_t offsets[count]
		TARGET(OPI_SWITCH_L)
			{
				OPC_ARGS(oa::Switch);
				register Value *const value = args->Value(f);
				if (value->type != vm->types.Int)
					return ThrowTypeError();

				if (value->integer >= 0 && value->integer < args->count)
					ip += (&args->firstOffset)[(int32_t)value->integer];

				ip += oa::SWITCH_SIZE(args->count);
			}
			NEXT_INSTR();
		TARGET(OPI_SWITCH_S)
			{
				OPC_ARGS(oa::Switch);
				register Value *const value = args->Value(f);
				if (value->type != vm->types.Int)
					return ThrowTypeError();

				if (value->integer >= 0 && value->integer < args->count)
					ip += (&args->firstOffset)[(int32_t)value->integer];

				ip += oa::SWITCH_SIZE(args->count);
				f->stackCount--;
			}
			NEXT_INSTR();

		// brref: LocalOffset (a, b), int32_t offset
		TARGET(OPI_BRREF)
			{
				OPC_ARGS(oa::ConditionalBranch);
				register Value *const ops = args->Value(f);

				if (IsSameReference_(ops + 0, ops + 1))
					ip += args->offset;

				ip += oa::CONDITIONAL_BRANCH_SIZE;
				f->stackCount -= 2;
			}
			NEXT_INSTR();

		// brnref: LocalOffset (a, b), int32_t offset
		TARGET(OPI_BRNREF)
			{
				OPC_ARGS(oa::ConditionalBranch);
				register Value *const ops = args->Value(f);

				if (IsSameReference_(ops + 0, ops + 1))
					ip += args->offset;

				ip += oa::CONDITIONAL_BRANCH_SIZE;
				f->stackCount -= 2;
			}
			NEXT_INSTR();

		// operator: LocalOffset args, LocalOffset dest, Operator op
		TARGET(OPI_OPERATOR_L)
			{
				OPC_ARGS(oa::TwoLocalsAndValue<Operator>);
				CHK(InvokeOperatorLL(args->Source(f), args->value, args->Dest(f)));
				ip += oa::TWO_LOCALS_AND_VALUE<Operator>::SIZE;
				// InvokeOperatorLL pops arguments off the stack
			}
			NEXT_INSTR();
		TARGET(OPI_OPERATOR_S)
			{
				OPC_ARGS(oa::TwoLocalsAndValue<Operator>);
				CHK(InvokeOperatorLL(args->Source(f), args->value, args->Dest(f)));
				ip += oa::TWO_LOCALS_AND_VALUE<Operator>::SIZE;
				// InvokeOperatorLL pops arguments off the stack
				f->stackCount++;
			}
			NEXT_INSTR();

		// eq: LocalOffset args, LocalOffset dest
		TARGET(OPI_EQ_L)
			{
				OPC_ARGS(oa::TwoLocals);
				bool eq;
				CHK(EqualsLL(args->Source(f), eq));
				SetBool_(vm, args->Dest(f), eq);
				ip += oa::TWO_LOCALS_SIZE;
				// EqualsLL pops arguments off the stack
			}
			NEXT_INSTR();
		TARGET(OPI_EQ_S)
			{
				OPC_ARGS(oa::TwoLocals);
				bool eq;
				CHK(EqualsLL(args->Source(f), eq));
				SetBool_(vm, args->Dest(f), eq);
				ip += oa::TWO_LOCALS_SIZE;
				// EqualsLL pops arguments off the stack
				f->stackCount++;
			}
			NEXT_INSTR();

		// cmp: LocalOffset args, LocalOffset dest
		TARGET(OPI_CMP_L)
			{
				OPC_ARGS(oa::TwoLocals);
				CHK(CompareLL(args->Source(f), args->Dest(f)));
				ip += oa::TWO_LOCALS_SIZE;
				// CompareLL pops arguments off the stack
			}
			NEXT_INSTR();
		TARGET(OPI_CMP_S)
			{
				OPC_ARGS(oa::TwoLocals);
				CHK(CompareLL(args->Source(f), args->Dest(f)));
				ip += oa::TWO_LOCALS_SIZE;
				// CompareLL pops arguments off the stack
				f->stackCount++;
			}
			NEXT_INSTR();

		// lt: LocalOffset args, LocalOffset dest
		TARGET(OPI_LT_L)
			{
				OPC_ARGS(oa::TwoLocals);
				bool result;
				CHK(CompareLessThanLL(args->Source(f), result));
				SetBool_(vm, args->Dest(f), result);
				ip += oa::TWO_LOCALS_SIZE;
				// CompareLL pops arguments off the stack
			}
			NEXT_INSTR();
		TARGET(OPI_LT_S)
			{
				OPC_ARGS(oa::TwoLocals);
				bool result;
				CHK(CompareLessThanLL(args->Source(f), result));
				SetBool_(vm, args->Dest(f), result);
				ip += oa::TWO_LOCALS_SIZE;
				// CompareLL pops arguments off the stack
				f->stackCount++;
			}
			NEXT_INSTR();

		// gt: LocalOffset args, LocalOffset dest
		TARGET(OPI_GT_L)
			{
				OPC_ARGS(oa::TwoLocals);
				bool result;
				CHK(CompareGreaterThanLL(args->Source(f), result));
				SetBool_(vm, args->Dest(f), result);
				ip += oa::TWO_LOCALS_SIZE;
				// CompareLL pops arguments off the stack
			}
			NEXT_INSTR();
		TARGET(OPI_GT_S)
			{
				OPC_ARGS(oa::TwoLocals);
				bool result;
				CHK(CompareGreaterThanLL(args->Source(f), result));
				SetBool_(vm, args->Dest(f), result);
				ip += oa::TWO_LOCALS_SIZE;
				// CompareLL pops arguments off the stack
				f->stackCount++;
			}
			NEXT_INSTR();

		// lte: LocalOffset args, LocalOffset dest
		TARGET(OPI_LTE_L)
			{
				OPC_ARGS(oa::TwoLocals);
				bool result;
				CHK(CompareLessEqualsLL(args->Source(f), result));
				SetBool_(vm, args->Dest(f), result);
				ip += oa::TWO_LOCALS_SIZE;
				// CompareLL pops arguments off the stack
			}
			NEXT_INSTR();
		TARGET(OPI_LTE_S)
			{
				OPC_ARGS(oa::TwoLocals);
				bool result;
				CHK(CompareLessEqualsLL(args->Source(f), result));
				SetBool_(vm, args->Dest(f), result);
				ip += oa::TWO_LOCALS_SIZE;
				// CompareLL pops arguments off the stack
				f->stackCount++;
			}
			NEXT_INSTR();

		// gte: LocalOffset args, LocalOffset dest
		TARGET(OPI_GTE_L)
			{
				OPC_ARGS(oa::TwoLocals);
				bool result;
				CHK(CompareGreaterEqualsLL(args->Source(f), result));
				SetBool_(vm, args->Dest(f), result);
				ip += oa::TWO_LOCALS_SIZE;
				// CompareLL pops arguments off the stack
			}
			NEXT_INSTR();
		TARGET(OPI_GTE_S)
			{
				OPC_ARGS(oa::TwoLocals);
				bool result;
				CHK(CompareGreaterEqualsLL(args->Source(f), result));
				SetBool_(vm, args->Dest(f), result);
				ip += oa::TWO_LOCALS_SIZE;
				// CompareLL pops arguments off the stack
				f->stackCount++;
			}
			NEXT_INSTR();

		// concat: LocalOffset args, LocalOffset dest
		TARGET(OPI_CONCAT_L)
			{
				OPC_ARGS(oa::TwoLocals);
				CHK(ConcatLL(args->Source(f), args->Dest(f)));
				ip += oa::TWO_LOCALS_SIZE;
				// ConcatLL pops arguments off stack
			}
			NEXT_INSTR();
		TARGET(OPI_CONCAT_S)
			{
				OPC_ARGS(oa::TwoLocals);
				CHK(ConcatLL(args->Source(f), args->Dest(f)));
				ip += oa::TWO_LOCALS_SIZE;
				// ConcatLL pops arguments off stack
				f->stackCount++;
			}
			NEXT_INSTR();

		// callmem: LocalOffset args, LocalOffset dest, uint32_t argc, String *member
		TARGET(OPI_CALLMEM_L)
			{
				OPC_ARGS(oa::CallMember);
				CHK(InvokeMemberLL(args->member, args->argc, args->Args(f), args->Dest(f), 0));
				ip += oa::CALL_MEMBER_SIZE;
			}
			NEXT_INSTR();
		TARGET(OPI_CALLMEM_S)
			{
				OPC_ARGS(oa::CallMember);
				CHK(InvokeMemberLL(args->member, args->argc, args->Args(f), args->Dest(f), 0));
				ip += oa::CALL_MEMBER_SIZE;
				f->stackCount++;
			}
			NEXT_INSTR();

		// stsfld: LocalOffset value, Field *field
		TARGET(OPI_STSFLD_L)
			{
				OPC_ARGS(oa::LocalAndValue<Field*>);
				args->value->staticValue->Write(args->Local(f));
				ip += oa::LOCAL_AND_VALUE<Field*>::SIZE;
			}
			NEXT_INSTR();
		TARGET(OPI_STSFLD_S)
			{
				OPC_ARGS(oa::LocalAndValue<Field*>);
				args->value->staticValue->Write(args->Local(f));
				ip += oa::LOCAL_AND_VALUE<Field*>::SIZE;
				f->stackCount--;
			}
			NEXT_INSTR();

		// stfld: LocalOffset (instance, value), Field *field
		TARGET(OPI_STFLD)
			{
				OPC_ARGS(oa::LocalAndValue<Field*>);
				CHK(args->value->WriteField(this, args->Local(f)));
				ip += oa::LOCAL_AND_VALUE<Field*>::SIZE;
				f->stackCount -= 2;
			}
			NEXT_INSTR();

		// stmem: LocalOffset (instance, value), String *name
		TARGET(OPI_STMEM)
			{
				OPC_ARGS(oa::LocalAndValue<String*>);
				// StoreMemberLL performs a null check
				CHK(StoreMemberLL(args->Local(f), args->value));
				// It also pops the things off the stack
				ip += oa::LOCAL_AND_VALUE<String*>::SIZE;
			}
			NEXT_INSTR();

		// stidx: LocalOffset args, uint32_t argc
		// Note: argCount does not include the instance, or the value being assigned
		TARGET(OPI_STIDX)
			{
				OPC_ARGS(oa::LocalAndValue<uint32_t>);
				// StoreIndexerLL performs a null check
				CHK(StoreIndexerLL(args->value, args->Local(f)));
				// It also pops things off the stack
				ip += oa::LOCAL_AND_VALUE<uint32_t>::SIZE;
			}
			NEXT_INSTR();

		TARGET(OPI_THROW)
			{
				retCode = Throw(/*rethrow:*/ false);
			}
			goto exitMethod;

		TARGET(OPI_RETHROW)
			{
				retCode = Throw(/*rethrow:*/ true);
			}
			goto exitMethod;

		TARGET(OPI_ENDFINALLY)
			// This Evaluate call was reached through FindErrorHandlers or
			// EvaluateLeave, so we return here and let the thing continue
			// with its search for more error handlers.
			retCode = OVUM_SUCCESS;
			goto exitMethod;

		// ldfldfast: LocalOffset instance, LocalOffset dest, Field *field
		// This is identical to ldfld except that it does not perform a type check.
		TARGET(OPI_LDFLDFAST_L)
			{
				OPC_ARGS(oa::TwoLocalsAndValue<Field*>);
				CHK(args->value->ReadFieldFast(this, args->Source(f), args->Dest(f)));
				ip += oa::TWO_LOCALS_AND_VALUE<Field*>::SIZE;
				f->stackCount--;
			}
			NEXT_INSTR();
		TARGET(OPI_LDFLDFAST_S)
			{
				OPC_ARGS(oa::TwoLocalsAndValue<Field*>);
				CHK(args->value->ReadFieldFast(this, args->Source(f), args->Dest(f)));
				ip += oa::TWO_LOCALS_AND_VALUE<Field*>::SIZE;
			}
			NEXT_INSTR();

		// stfldfast: LocalOffset (instance, value), Field *field
		// This is identical to stfld except that it does not perform a type check.
		TARGET(OPI_STFLDFAST)
			{
				OPC_ARGS(oa::LocalAndValue<Field*>);
				CHK(args->value->WriteFieldFast(this, args->Local(f)));
				ip += oa::LOCAL_AND_VALUE<Field*>::SIZE;
				f->stackCount -= 2;
			}
			NEXT_INSTR();

		// breq: LocalOffset args, int32_t offset
		TARGET(OPI_BREQ)
			{
				OPC_ARGS(oa::ConditionalBranch);
				bool eq;
				CHK(EqualsLL(args->Value(f), eq));
				if (eq)
					ip += args->offset;
				ip += oa::CONDITIONAL_BRANCH_SIZE;
			}
			NEXT_INSTR();

		// brneq: LocalOffset args, int32_t offset
		TARGET(OPI_BRNEQ)
			{
				OPC_ARGS(oa::ConditionalBranch);
				bool eq;
				CHK(EqualsLL(args->Value(f), eq));
				if (!eq)
					ip += args->offset;
				ip += oa::CONDITIONAL_BRANCH_SIZE;
			}
			NEXT_INSTR();

		// brlt: LocalOffset args, int32_t offset
		TARGET(OPI_BRLT)
			{
				OPC_ARGS(oa::ConditionalBranch);
				bool result;
				CHK(CompareLessThanLL(args->Value(f), result));
				if (result)
					ip += args->offset;
				ip += oa::CONDITIONAL_BRANCH_SIZE;
			}
			NEXT_INSTR();

		// brgt: LocalOffset args, int32_t offset
		TARGET(OPI_BRGT)
			{
				OPC_ARGS(oa::ConditionalBranch);
				bool result;
				CHK(CompareGreaterThanLL(args->Value(f), result));
				if (result)
					ip += args->offset;
				ip += oa::CONDITIONAL_BRANCH_SIZE;
			}
			NEXT_INSTR();

		// brlte: LocalOffset args, int32_t offset
		TARGET(OPI_BRLTE)
			{
				OPC_ARGS(oa::ConditionalBranch);
				bool result;
				CHK(CompareLessEqualsLL(args->Value(f), result));
				if (result)
					ip += args->offset;
				ip += oa::CONDITIONAL_BRANCH_SIZE;
			}
			NEXT_INSTR();

		// brgte: LocalOffset args, int32_t offset
		TARGET(OPI_BRGTE)
			{
				OPC_ARGS(oa::ConditionalBranch);
				bool result;
				CHK(CompareGreaterEqualsLL(args->Value(f), result));
				if (result)
					ip += args->offset;
				ip += oa::CONDITIONAL_BRANCH_SIZE;
			}
			NEXT_INSTR();

		// ldlocref: LocalOffset local
		TARGET(OPI_LDLOCREF)
			{
				OPC_ARGS(oa::OneLocal);
				register Value *const dest = f->evalStack + f->stackCount++;
				dest->type = (Type*)LOCAL_REFERENCE;
				dest->reference = args->Local(f);
				ip += oa::ONE_LOCAL_SIZE;
			}
			NEXT_INSTR();

		// ldmemref: LocalOffset inst, String *member
		TARGET(OPI_LDMEMREF_L) // Instance in local
			{
				OPC_ARGS(oa::LocalAndValue<String*>);
				CHK(LoadMemberRefLL(args->Local(f), args->value));
				ip += oa::LOCAL_AND_VALUE<String*>::SIZE;
			}
			NEXT_INSTR();
		TARGET(OPI_LDMEMREF_S) // Instance on stack
			{
				OPC_ARGS(oa::LocalAndValue<String*>);
				f->stackCount--;
				CHK(LoadMemberRefLL(args->Local(f), args->value));
				ip += oa::LOCAL_AND_VALUE<String*>::SIZE;
			}
			NEXT_INSTR();

		// ldfldref: LocalOffset inst, Field *field
		TARGET(OPI_LDFLDREF_L) // Instance in local
			{
				OPC_ARGS(oa::LocalAndValue<Field*>);
				CHK(LoadFieldRefLL(args->Local(f), args->value));
				ip += oa::LOCAL_AND_VALUE<Field*>::SIZE;
			}
			NEXT_INSTR();
		TARGET(OPI_LDFLDREF_S) // Instance on stack
			{
				OPC_ARGS(oa::LocalAndValue<Field*>);
				f->stackCount--;
				CHK(LoadFieldRefLL(args->Local(f), args->value));
				ip += oa::LOCAL_AND_VALUE<Field*>::SIZE;
			}
			NEXT_INSTR();

		// ldsfldref: Field *field
		TARGET(OPI_LDSFLDREF)
			{
				OPC_ARGS(oa::SingleValue<Field*>);
				register Value *const dest = f->evalStack + f->stackCount++;
				dest->type = (Type*)STATIC_REFERENCE;
				dest->reference = args->value->staticValue;
				ip += oa::SINGLE_VALUE<Field*>::SIZE;
			}
			NEXT_INSTR();

		// mvloc_rr: LocalOffset source, LocalOffset dest
		// Note: these are all subtly different in implementation. Do not attempt to abstract
		// them into a single macro without making note of the differences.
		TARGET(OPI_MVLOC_RL) // Reference -> local
			{
				OPC_ARGS(oa::TwoLocals);
				register Value *const source = args->Source(f);

				if ((uintptr_t)source->type == LOCAL_REFERENCE)
					*args->Dest(f) = *reinterpret_cast<Value*>(source->reference);
				else if ((uintptr_t)source->type == STATIC_REFERENCE)
					reinterpret_cast<StaticRef*>(source->reference)->Read(args->Dest(f));
				else
				{
					uintptr_t offset = ~(uintptr_t)source->type;
					GCObject *gco = reinterpret_cast<GCObject*>((char*)source->reference - offset);
					gco->fieldAccessLock.Enter();
					*args->Dest(f) = *reinterpret_cast<Value*>(source->reference);
					gco->fieldAccessLock.Leave();
				}

				ip += oa::TWO_LOCALS_SIZE;
			}
			NEXT_INSTR();
		TARGET(OPI_MVLOC_RS) // Reference -> stack
			{
				OPC_ARGS(oa::TwoLocals);
				register Value *const source = args->Source(f);

				if ((uintptr_t)source->type == LOCAL_REFERENCE)
					*args->Dest(f) = *reinterpret_cast<Value*>(source->reference);
				else if ((uintptr_t)source->type == STATIC_REFERENCE)
					reinterpret_cast<StaticRef*>(source->reference)->Read(args->Dest(f));
				else
				{
					uintptr_t offset = ~(uintptr_t)source->type;
					GCObject *gco = reinterpret_cast<GCObject*>((char*)source->reference - offset);
					gco->fieldAccessLock.Enter();
					*args->Dest(f) = *reinterpret_cast<Value*>(source->reference);
					gco->fieldAccessLock.Leave();
				}

				ip += oa::TWO_LOCALS_SIZE;
				f->stackCount++;
			}
			NEXT_INSTR();
		TARGET(OPI_MVLOC_LR) // Local -> reference
			{
				OPC_ARGS(oa::TwoLocals);
				register Value *const dest = args->Dest(f);

				if ((uintptr_t)dest->type == LOCAL_REFERENCE)
					*reinterpret_cast<Value*>(dest->reference) = *args->Source(f);
				else if ((uintptr_t)dest->type == STATIC_REFERENCE)
					reinterpret_cast<StaticRef*>(dest->reference)->Write(args->Source(f));
				else
				{
					uint32_t offset = ~(uint32_t)dest->type;
					GCObject *gco = reinterpret_cast<GCObject*>(dest->instance - offset);
					gco->fieldAccessLock.Enter();
					*reinterpret_cast<Value*>(dest->instance) = *args->Source(f);
					gco->fieldAccessLock.Leave();
				}

				ip += oa::TWO_LOCALS_SIZE;
			}
			NEXT_INSTR();
		TARGET(OPI_MVLOC_SR) // Stack -> reference
			{
				OPC_ARGS(oa::TwoLocals);
				register Value *const dest = args->Dest(f);

				if ((uintptr_t)dest->type == LOCAL_REFERENCE)
					*reinterpret_cast<Value*>(dest->reference) = *args->Source(f);
				else if ((uintptr_t)dest->type == STATIC_REFERENCE)
					reinterpret_cast<StaticRef*>(dest->reference)->Write(args->Source(f));
				else
				{
					uintptr_t offset = ~(uintptr_t)dest->type;
					GCObject *gco = reinterpret_cast<GCObject*>((char*)dest->reference - offset);
					gco->fieldAccessLock.Enter();
					*reinterpret_cast<Value*>(dest->reference) = *args->Source(f);
					gco->fieldAccessLock.Leave();
				}

				ip += oa::TWO_LOCALS_SIZE;
				f->stackCount--;
			}
			NEXT_INSTR();

		// callr: LocalOffset args, LocalOffset output, uint32_t argc, uint32_t refSignature
		TARGET(OPI_CALLR_L)
			{
				OPC_ARGS(oa::CallRef);
				CHK(InvokeLL(args->argc, args->Args(f), args->Dest(f), args->refSignature));
				ip += oa::CALL_REF_SIZE;
				// InvokeLL pops the arguments
			}
			NEXT_INSTR();
		TARGET(OPI_CALLR_S)
			{
				OPC_ARGS(oa::CallRef);
				CHK(InvokeLL(args->argc, args->Args(f), args->Dest(f), args->refSignature));
				ip += oa::CALL_REF_SIZE;
				// InvokeLL pops the arguments
				f->stackCount++;
			}
			NEXT_INSTR();

		// callmemr: LocalOffset args, LocalOffset dest, uint32_t argc, uint32_t refSignature, String *member
		TARGET(OPI_CALLMEMR_L)
			{
				OPC_ARGS(oa::CallMemberRef);
				CHK(InvokeMemberLL(args->member, args->argc, args->Args(f), args->Dest(f), args->refSignature));
				ip += oa::CALL_MEMBER_REF_SIZE;
			}
			NEXT_INSTR();
		TARGET(OPI_CALLMEMR_S)
			{
				OPC_ARGS(oa::CallMemberRef);
				CHK(InvokeMemberLL(args->member, args->argc, args->Args(f), args->Dest(f), args->refSignature));
				ip += oa::CALL_MEMBER_REF_SIZE;
				f->stackCount++;
			}
			NEXT_INSTR();
		}
	}

ret:
	assert(f->stackCount == 1);
	// And then we just fall through and return!
exitMethod:
	return retCode;
}

int Thread::FindErrorHandler(int32_t maxIndex)
{
	typedef MethodOverload::TryBlock::TryKind TryKind;

	register StackFrame *frame = currentFrame;
	MethodOverload *method = frame->method;
	uint32_t offset = (uint32_t)(this->ip - method->entry);

	if (maxIndex == -1)
		maxIndex = method->tryBlockCount;

	for (int32_t t = 0; t < maxIndex; t++)
	{
		MethodOverload::TryBlock &tryBlock = method->tryBlocks[t];
		if (offset >= tryBlock.tryStart && offset <= tryBlock.tryEnd)
		{
			// The ip is inside a try block! Let's find a catch or finally.
			switch (tryBlock.kind)
			{
			case TryKind::CATCH:
				for (int32_t c = 0; c < tryBlock.catches.count; c++)
				{
					MethodOverload::CatchBlock &catchBlock = tryBlock.catches.blocks[c];
					if (Type::ValueIsType(&currentError, catchBlock.caughtType))
					{
						frame->stackCount = 1;
						frame->evalStack[0] = currentError;
						this->ip = method->entry + catchBlock.catchStart;
						RETURN_SUCCESS; // Got there!
					}
				}
				break;
			case TryKind::FINALLY:
				{
					frame->stackCount = 0;
					// We must save the current error, because if an error is thrown and
					// caught inside the finally, currentError will be updated to contain
					// that error. We will cause problems if we don't restore the old one.
					Value prevError = this->currentError;

					this->ip = method->entry + tryBlock.finallyBlock.finallyStart;
					enter:
					int r = Evaluate();
					if (r != OVUM_SUCCESS)
					{
						if (r == OVUM_ERROR_THROWN)
						{
							// The try blocks in the method are ordered from innermost to
							// outermost. By passing t as the maxIndex, we ensure that if
							// an error is thrown in the finally, we don't look for a catch
							// that is outside the finally. Instead, we simply return the
							// appropriate status code and let the caller deal with it.
							// If the caller is InvokeMethodOverload, it will look for an
							// error handler in the method.
							int r2 = FindErrorHandler(t);
							if (r2 == OVUM_SUCCESS)
								goto enter;
							r = r2;
						}
						return r;
					}
					this->ip = method->entry + offset;

					this->currentError = prevError;
				}
				break;
			}
			// We can't stop enumerating the blocks just yet.
			// There may be another try block that actually handles the error.
		}
	}
	// No error handler found
	return OVUM_ERROR_THROWN;
}

int Thread::EvaluateLeave(register StackFrame *frame, int32_t target)
{
	typedef MethodOverload::TryBlock::TryKind TryKind;

	// Note: the IP currently points to the leave instruction.
	// We must add the size of the instructions to get the right ipOffset and tOffset.
	const size_t TOTAL_INSTR_SIZE = // including the opcode, not just its args
		ALIGN_TO(sizeof(IntermediateOpcode), opcode_args::ALIGNMENT) +
		opcode_args::BRANCH_SIZE;

	MethodOverload *method = frame->method;
	const uint32_t ipOffset = (uint32_t)(this->ip + TOTAL_INSTR_SIZE - method->entry);
	const uint32_t tOffset  = ipOffset + target;
	for (int32_t t = 0; t < method->tryBlockCount; t++)
	{
		MethodOverload::TryBlock &tryBlock = method->tryBlocks[t];
		if (tryBlock.kind == TryKind::FINALLY &&
			ipOffset >= tryBlock.tryStart && ipOffset <= tryBlock.tryEnd &&
			(tOffset < tryBlock.tryStart || tOffset >= tryBlock.tryEnd) &&
			(tOffset < tryBlock.finallyBlock.finallyStart || tOffset >= tryBlock.finallyBlock.finallyEnd))
		{
			// Evaluate the finally!
			uint8_t *const prevIp = this->ip;
			// We must save the current error, because if an error is thrown and
			// caught inside the finally, currentError will be updated to contain
			// that error. We will cause problems if we don't restore the old one.
			Value prevError = this->currentError;

			this->ip = method->entry + tryBlock.finallyBlock.finallyStart;
			enter:
			int r = Evaluate();
			if (r != OVUM_SUCCESS)
			{
				if (r == OVUM_ERROR_THROWN)
				{
					int r2 = FindErrorHandler(t);
					if (r2 == OVUM_SUCCESS)
						goto enter;
					r = r2;
				}
				return r;
			}
			this->ip = prevIp;

			this->currentError = prevError;
		}
	}

	RETURN_SUCCESS;
}

} // namespace ovum