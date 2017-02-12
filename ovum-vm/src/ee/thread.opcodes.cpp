#include "thread.opcodes.h"
#include "../object/type.h"
#include "../object/member.h"
#include "../object/field.h"
#include "../object/method.h"
#include "../object/value.h"
#include "../gc/gc.h"
#include "../gc/staticref.h"
#include "../res/staticstrings.h"

namespace ovum
{

#define OPC_ARGS(T) const T *const args = reinterpret_cast<const T*>(ip)

// Used in Thread::Evaluate. Semicolon intentionally missing.
#define CHK(expr) do { if ((retCode = (expr)) != OVUM_SUCCESS) goto exitMethod; } while (0)

#define TARGET(opc) case opc:
#define NEXT_INSTR() break

#define SET_BOOL(ptarg, bvalue) \
	{                                       \
		(ptarg)->type = vm->types.Boolean;  \
		(ptarg)->v.integer = bvalue;        \
	}
#define SET_INT(ptarg, ivalue) \
	{                                       \
		(ptarg)->type = vm->types.Int;      \
		(ptarg)->v.integer = ivalue;        \
	}
#define SET_UINT(ptarg, uvalue) \
	{                                       \
		(ptarg)->type = vm->types.UInt;     \
		(ptarg)->v.uinteger = uvalue;       \
	}
#define SET_REAL(ptarg, rvalue) \
	{                                       \
		(ptarg)->type = vm->types.Real;     \
		(ptarg)->v.real = rvalue;           \
	}
#define SET_STRING(ptarg, svalue) \
	{                                       \
		(ptarg)->type = vm->types.String;   \
		(ptarg)->v.string = svalue;         \
	}

int Thread::Evaluate()
{
	namespace oa = ovum::opcode_args; // For convenience

	if (pendingRequest != ThreadRequest::NONE)
		HandleRequest();

	int retCode;

	StackFrame *const f = currentFrame;
	// this->ip has been set to the entry address
	uint8_t *ip = this->ip;

	while (true)
	{
		this->ip = ip;
		IntermediateOpcode opc = static_cast<IntermediateOpcode>(*ip);
		// Always skip the opcode
		ip += OVUM_ALIGN_TO(sizeof(IntermediateOpcode), oa::ALIGNMENT);
		switch (opc)
		{
		TARGET(OPI_RET)
			{
				OVUM_ASSERT(f->stackCount == 1);
			}
			retCode = OVUM_SUCCESS;
			goto ret;

		TARGET(OPI_RETNULL)
			{
				OVUM_ASSERT(f->stackCount == 0);
				f->evalStack->type = nullptr;
				f->stackCount++;
			}
			retCode = OVUM_SUCCESS;
			goto ret;

		TARGET(OPI_NOP)
			// Really, do nothing!
			NEXT_INSTR();

		TARGET(OPI_POP)
			{
				// pop just decrements the stack height
				f->stackCount--;
			}
			NEXT_INSTR();

		// mvloc
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

		// ldnull
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

		// ldfalse
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

		// ldtrue
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

		// ldc.i
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

		// ldc.u
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

		// ldc.r
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

		// ldstr
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

		// ldargc
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

		// ldenum
		TARGET(OPI_LDENUM_L)
			{
				OPC_ARGS(oa::LoadEnum);
				Value *const dest = args->Dest(f);
				dest->type = args->type;
				dest->v.integer = args->value;
				ip += oa::LOAD_ENUM_SIZE;
			}
			NEXT_INSTR();
		TARGET(OPI_LDENUM_S)
			{
				OPC_ARGS(oa::LoadEnum);
				Value *const dest = args->Dest(f);
				dest->type = args->type;
				dest->v.integer = args->value;
				ip += oa::LOAD_ENUM_SIZE;
				f->stackCount++;
			}
			NEXT_INSTR();

		// newobj
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

		// list
		TARGET(OPI_LIST_L)
			{
				OPC_ARGS(oa::LocalAndValue<size_t>);
				// We unfortunately have to put the list in the destination local
				// during initialization, otherwise the GC won't be able to reach
				// it if initListInstance should happen to trigger a cycle.
				Value *result = args->Local(f);
				CHK(GetGC()->Alloc(this, vm->types.List, sizeof(ListInst), result));
				CHK(vm->functions.initListInstance(this, result->v.list, args->value));

				ip += oa::LOCAL_AND_VALUE<size_t>::SIZE;
			}
			NEXT_INSTR();
		TARGET(OPI_LIST_S)
			{
				OPC_ARGS(oa::LocalAndValue<size_t>);
				// We unfortunately have to put the list in the destination local
				// during initialization, otherwise the GC won't be able to reach
				// it if initListInstance should happen to trigger a cycle.
				Value *result = args->Local(f);
				CHK(GetGC()->Alloc(this, vm->types.List, sizeof(ListInst), result));
				f->stackCount++; // make GC-reachable
				CHK(vm->functions.initListInstance(this, result->v.list, args->value));

				ip += oa::LOCAL_AND_VALUE<size_t>::SIZE;
			}
			NEXT_INSTR();

		// hash
		TARGET(OPI_HASH_L)
			{
				OPC_ARGS(oa::LocalAndValue<size_t>);
				CHK(vm->functions.initHashInstance(this, args->value, args->Local(f)));

				ip += oa::LOCAL_AND_VALUE<size_t>::SIZE;
			}
			NEXT_INSTR();
		TARGET(OPI_HASH_S)
			{
				OPC_ARGS(oa::LocalAndValue<size_t>);
				CHK(vm->functions.initHashInstance(this, args->value, args->Local(f)));

				ip += oa::LOCAL_AND_VALUE<size_t>::SIZE;
				f->stackCount++;
			}
			NEXT_INSTR();

		// ldfld
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

		// ldsfld
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

		// ldmem
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

		// lditer
		TARGET(OPI_LDITER_L)
			{
				OPC_ARGS(oa::TwoLocals);
				CHK(InvokeMemberLL(strings->members.iter_, 0, args->Source(f), args->Dest(f), 0));
				// InvokeMemberLL pops the instance and all 0 of the arguments
				ip += oa::TWO_LOCALS_SIZE;
			}
			NEXT_INSTR();
		TARGET(OPI_LDITER_S)
			{
				OPC_ARGS(oa::TwoLocals);
				CHK(InvokeMemberLL(strings->members.iter_, 0, args->Source(f), args->Dest(f), 0));
				// InvokeMemberLL pops the instance and all 0 of the arguments
				ip += oa::TWO_LOCALS_SIZE;
				f->stackCount++;
			}
			NEXT_INSTR();

		// ldtype
		TARGET(OPI_LDTYPE_L)
			{
				OPC_ARGS(oa::TwoLocals);
				Value *const inst = args->Source(f);

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
				Value *const inst = args->Source(f);

				if (inst->type)
					CHK(inst->type->GetTypeToken(this, args->Dest(f)));
				else
					args->Dest(f)->type = nullptr;

				ip += oa::TWO_LOCALS_SIZE;
			}
			NEXT_INSTR();

		// ldidx
		// Note: arg count does not include the instance
		TARGET(OPI_LDIDX_L)
			{
				OPC_ARGS(oa::TwoLocalsAndValue<ovlocals_t>);

				CHK(LoadIndexerLL(args->value, args->Source(f), args->Dest(f)));

				// LoadIndexerLL decrements the stack height by the argument count + instance
				ip += oa::TWO_LOCALS_AND_VALUE<ovlocals_t>::SIZE;
			}
			NEXT_INSTR();
		TARGET(OPI_LDIDX_S)
			{
				OPC_ARGS(oa::TwoLocalsAndValue<ovlocals_t>);

				CHK(LoadIndexerLL(args->value, args->Source(f), args->Dest(f)));

				// LoadIndexerLL decrements the stack height by the argument count + instance
				ip += oa::TWO_LOCALS_AND_VALUE<ovlocals_t>::SIZE;
				f->stackCount++;
			}
			NEXT_INSTR();

		// ldsfn
		TARGET(OPI_LDSFN_L)
			{
				OPC_ARGS(oa::LocalAndValue<Method*>);
				Value *const dest = args->Local(f);
				CHK(GetGC()->Alloc(this, vm->types.Method, sizeof(MethodInst), dest));
				dest->v.method->method = args->value;

				ip += oa::LOCAL_AND_VALUE<Method*>::SIZE;
			}
			NEXT_INSTR();
		TARGET(OPI_LDSFN_S)
			{
				OPC_ARGS(oa::LocalAndValue<Method*>);
				Value *const dest = args->Local(f);
				CHK(GetGC()->Alloc(this, vm->types.Method, sizeof(MethodInst), dest));
				dest->v.method->method = args->value;

				ip += oa::LOCAL_AND_VALUE<Method*>::SIZE;
				f->stackCount++;
			}
			NEXT_INSTR();

		// ldtypetkn
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

		// call
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

		// scall
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

		// apply
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

		// sapply
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

		// br
		TARGET(OPI_BR)
			{
				OPC_ARGS(oa::Branch);
				ip += args->offset;
				ip += oa::BRANCH_SIZE;
			}
			NEXT_INSTR();

		// leave
		TARGET(OPI_LEAVE)
			{
				OPC_ARGS(oa::Branch);
				CHK(EvaluateLeave(f, args->offset));
				ip += args->offset;
				ip += oa::BRANCH_SIZE;
			}
			NEXT_INSTR();

		// brnull
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

		// brinst
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

		// brfalse
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

		// brtrue
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

		// brtype
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
				f->stackCount--;
			}
			NEXT_INSTR();

		// switch
		TARGET(OPI_SWITCH_L)
			{
				OPC_ARGS(oa::Switch);
				Value *const value = args->Value(f);
				if (value->type != vm->types.Int)
					return ThrowTypeError();

				if (value->v.integer >= 0 && value->v.integer < args->count)
					ip += (&args->firstOffset)[(size_t)value->v.integer];

				ip += oa::SWITCH_SIZE(args->count);
			}
			NEXT_INSTR();
		TARGET(OPI_SWITCH_S)
			{
				OPC_ARGS(oa::Switch);
				Value *const value = args->Value(f);
				if (value->type != vm->types.Int)
					return ThrowTypeError();

				if (value->v.integer >= 0 && value->v.integer < args->count)
					ip += (&args->firstOffset)[(size_t)value->v.integer];

				ip += oa::SWITCH_SIZE(args->count);
				f->stackCount--;
			}
			NEXT_INSTR();

		// brref
		TARGET(OPI_BRREF)
			{
				OPC_ARGS(oa::ConditionalBranch);
				Value *const ops = args->Value(f);

				if (IsSameReference_(ops + 0, ops + 1))
					ip += args->offset;

				ip += oa::CONDITIONAL_BRANCH_SIZE;
				f->stackCount -= 2;
			}
			NEXT_INSTR();

		// brnref
		TARGET(OPI_BRNREF)
			{
				OPC_ARGS(oa::ConditionalBranch);
				Value *const ops = args->Value(f);

				if (!IsSameReference_(ops + 0, ops + 1))
					ip += args->offset;

				ip += oa::CONDITIONAL_BRANCH_SIZE;
				f->stackCount -= 2;
			}
			NEXT_INSTR();

		// operator
		TARGET(OPI_OPERATOR_L)
			{
				OPC_ARGS(oa::TwoLocalsAndValue<Operator>);
				CHK(InvokeOperatorLL(args->Source(f), args->value, 2, args->Dest(f)));
				ip += oa::TWO_LOCALS_AND_VALUE<Operator>::SIZE;
				// InvokeOperatorLL pops arguments off the stack
			}
			NEXT_INSTR();
		TARGET(OPI_OPERATOR_S)
			{
				OPC_ARGS(oa::TwoLocalsAndValue<Operator>);
				CHK(InvokeOperatorLL(args->Source(f), args->value, 2, args->Dest(f)));
				ip += oa::TWO_LOCALS_AND_VALUE<Operator>::SIZE;
				// InvokeOperatorLL pops arguments off the stack
				f->stackCount++;
			}
			NEXT_INSTR();

		// eq
		TARGET(OPI_EQ_L)
			{
				OPC_ARGS(oa::TwoLocals);
				bool eq;
				CHK(EqualsLL(args->Source(f), eq));
				SET_BOOL(args->Dest(f), eq);
				ip += oa::TWO_LOCALS_SIZE;
				// EqualsLL pops arguments off the stack
			}
			NEXT_INSTR();
		TARGET(OPI_EQ_S)
			{
				OPC_ARGS(oa::TwoLocals);
				bool eq;
				CHK(EqualsLL(args->Source(f), eq));
				SET_BOOL(args->Dest(f), eq);
				ip += oa::TWO_LOCALS_SIZE;
				// EqualsLL pops arguments off the stack
				f->stackCount++;
			}
			NEXT_INSTR();

		// cmp
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

		// lt
		TARGET(OPI_LT_L)
			{
				OPC_ARGS(oa::TwoLocals);
				bool result;
				CHK(CompareLessThanLL(args->Source(f), result));
				SET_BOOL(args->Dest(f), result);
				ip += oa::TWO_LOCALS_SIZE;
				// CompareLL pops arguments off the stack
			}
			NEXT_INSTR();
		TARGET(OPI_LT_S)
			{
				OPC_ARGS(oa::TwoLocals);
				bool result;
				CHK(CompareLessThanLL(args->Source(f), result));
				SET_BOOL(args->Dest(f), result);
				ip += oa::TWO_LOCALS_SIZE;
				// CompareLL pops arguments off the stack
				f->stackCount++;
			}
			NEXT_INSTR();

		// gt
		TARGET(OPI_GT_L)
			{
				OPC_ARGS(oa::TwoLocals);
				bool result;
				CHK(CompareGreaterThanLL(args->Source(f), result));
				SET_BOOL(args->Dest(f), result);
				ip += oa::TWO_LOCALS_SIZE;
				// CompareLL pops arguments off the stack
			}
			NEXT_INSTR();
		TARGET(OPI_GT_S)
			{
				OPC_ARGS(oa::TwoLocals);
				bool result;
				CHK(CompareGreaterThanLL(args->Source(f), result));
				SET_BOOL(args->Dest(f), result);
				ip += oa::TWO_LOCALS_SIZE;
				// CompareLL pops arguments off the stack
				f->stackCount++;
			}
			NEXT_INSTR();

		// lte
		TARGET(OPI_LTE_L)
			{
				OPC_ARGS(oa::TwoLocals);
				bool result;
				CHK(CompareLessEqualsLL(args->Source(f), result));
				SET_BOOL(args->Dest(f), result);
				ip += oa::TWO_LOCALS_SIZE;
				// CompareLL pops arguments off the stack
			}
			NEXT_INSTR();
		TARGET(OPI_LTE_S)
			{
				OPC_ARGS(oa::TwoLocals);
				bool result;
				CHK(CompareLessEqualsLL(args->Source(f), result));
				SET_BOOL(args->Dest(f), result);
				ip += oa::TWO_LOCALS_SIZE;
				// CompareLL pops arguments off the stack
				f->stackCount++;
			}
			NEXT_INSTR();

		// gte
		TARGET(OPI_GTE_L)
			{
				OPC_ARGS(oa::TwoLocals);
				bool result;
				CHK(CompareGreaterEqualsLL(args->Source(f), result));
				SET_BOOL(args->Dest(f), result);
				ip += oa::TWO_LOCALS_SIZE;
				// CompareLL pops arguments off the stack
			}
			NEXT_INSTR();
		TARGET(OPI_GTE_S)
			{
				OPC_ARGS(oa::TwoLocals);
				bool result;
				CHK(CompareGreaterEqualsLL(args->Source(f), result));
				SET_BOOL(args->Dest(f), result);
				ip += oa::TWO_LOCALS_SIZE;
				// CompareLL pops arguments off the stack
				f->stackCount++;
			}
			NEXT_INSTR();

		// concat
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

		// callmem
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

		// stsfld
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

		// stfld
		TARGET(OPI_STFLD)
			{
				OPC_ARGS(oa::LocalAndValue<Field*>);
				CHK(args->value->WriteField(this, args->Local(f)));
				ip += oa::LOCAL_AND_VALUE<Field*>::SIZE;
				f->stackCount -= 2;
			}
			NEXT_INSTR();

		// stmem
		TARGET(OPI_STMEM)
			{
				OPC_ARGS(oa::LocalAndValue<String*>);
				// StoreMemberLL performs a null check
				CHK(StoreMemberLL(args->Local(f), args->value));
				// It also pops the things off the stack
				ip += oa::LOCAL_AND_VALUE<String*>::SIZE;
			}
			NEXT_INSTR();

		// stidx
		// Note: arg count does not include the instance, or the value being assigned
		TARGET(OPI_STIDX)
			{
				OPC_ARGS(oa::LocalAndValue<ovlocals_t>);
				// StoreIndexerLL performs a null check
				CHK(StoreIndexerLL(args->value, args->Local(f)));
				// It also pops things off the stack
				ip += oa::LOCAL_AND_VALUE<ovlocals_t>::SIZE;
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

		// ldfldfast
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

		// stfldfast
		// This is identical to stfld except that it does not perform a type check.
		TARGET(OPI_STFLDFAST)
			{
				OPC_ARGS(oa::LocalAndValue<Field*>);
				CHK(args->value->WriteFieldFast(this, args->Local(f)));
				ip += oa::LOCAL_AND_VALUE<Field*>::SIZE;
				f->stackCount -= 2;
			}
			NEXT_INSTR();

		// breq
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

		// brneq
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

		// brlt
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

		// brgt
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

		// brlte
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

		// brgte
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

		// ldlocref
		TARGET(OPI_LDLOCREF)
			{
				OPC_ARGS(oa::OneLocal);
				Value *const dest = f->evalStack + f->stackCount++;
				dest->type = (Type*)LOCAL_REFERENCE;
				dest->v.reference = args->Local(f);
				ip += oa::ONE_LOCAL_SIZE;
			}
			NEXT_INSTR();

		// ldmemref
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

		// ldfldref
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

		// ldsfldref
		TARGET(OPI_LDSFLDREF)
			{
				OPC_ARGS(oa::SingleValue<Field*>);
				Value *const dest = f->evalStack + f->stackCount++;
				dest->type = (Type*)STATIC_REFERENCE;
				dest->v.reference = args->value->staticValue;
				ip += oa::SINGLE_VALUE<Field*>::SIZE;
			}
			NEXT_INSTR();

		// mvloc_rr
		// Note: these are all subtly different in implementation. Do not attempt to abstract
		// them into a single macro without making note of the differences.
		TARGET(OPI_MVLOC_RL) // Reference -> local
			{
				OPC_ARGS(oa::TwoLocals);
				Value *const source = args->Source(f);

				uintptr_t sourceType = reinterpret_cast<uintptr_t>(source->type);
				if (sourceType == LOCAL_REFERENCE)
				{
					*args->Dest(f) = *reinterpret_cast<Value*>(source->v.reference);
				}
				else if (sourceType == STATIC_REFERENCE)
				{
					reinterpret_cast<StaticRef*>(source->v.reference)->Read(args->Dest(f));
				}
				else
				{
					GCObject *gco = reinterpret_cast<GCObject*>(source->v.reference);
					gco->fieldAccessLock.Enter();

					uintptr_t offset = ~sourceType;
					Value *field = reinterpret_cast<Value*>(
						reinterpret_cast<char*>(source->v.reference) + offset
					);
					*args->Dest(f) = *field;

					gco->fieldAccessLock.Leave();
				}

				ip += oa::TWO_LOCALS_SIZE;
			}
			NEXT_INSTR();
		TARGET(OPI_MVLOC_RS) // Reference -> stack
			{
				OPC_ARGS(oa::TwoLocals);
				Value *const source = args->Source(f);

				uintptr_t sourceType = reinterpret_cast<uintptr_t>(source->type);
				if (sourceType == LOCAL_REFERENCE)
				{
					*args->Dest(f) = *reinterpret_cast<Value*>(source->v.reference);
				}
				else if (sourceType == STATIC_REFERENCE)
				{
					reinterpret_cast<StaticRef*>(source->v.reference)->Read(args->Dest(f));
				}
				else
				{
					GCObject *gco = reinterpret_cast<GCObject*>(source->v.reference);
					gco->fieldAccessLock.Enter();

					uintptr_t offset = ~sourceType;
					Value *field = reinterpret_cast<Value*>(
						reinterpret_cast<char*>(source->v.reference) + offset
					);
					*args->Dest(f) = *field;

					gco->fieldAccessLock.Leave();
				}

				ip += oa::TWO_LOCALS_SIZE;
				f->stackCount++;
			}
			NEXT_INSTR();
		TARGET(OPI_MVLOC_LR) // Local -> reference
			{
				OPC_ARGS(oa::TwoLocals);
				Value *const dest = args->Dest(f);

				uintptr_t destType = reinterpret_cast<uintptr_t>(dest->type);
				if (destType == LOCAL_REFERENCE)
				{
					*reinterpret_cast<Value*>(dest->v.reference) = *args->Source(f);
				}
				else if (destType == STATIC_REFERENCE)
				{
					reinterpret_cast<StaticRef*>(dest->v.reference)->Write(args->Source(f));
				}
				else
				{
					GCObject *gco = reinterpret_cast<GCObject*>(dest->v.reference);
					gco->fieldAccessLock.Enter();

					uint32_t offset = ~destType;
					Value *field = reinterpret_cast<Value*>(
						reinterpret_cast<char*>(dest->v.reference) + offset
					);
					*field = *args->Source(f);

					gco->fieldAccessLock.Leave();
				}

				ip += oa::TWO_LOCALS_SIZE;
			}
			NEXT_INSTR();
		TARGET(OPI_MVLOC_SR) // Stack -> reference
			{
				OPC_ARGS(oa::TwoLocals);
				Value *const dest = args->Dest(f);

				uintptr_t destType = reinterpret_cast<uintptr_t>(dest->type);
				if (destType == LOCAL_REFERENCE)
				{
					*reinterpret_cast<Value*>(dest->v.reference) = *args->Source(f);
				}
				else if (destType == STATIC_REFERENCE)
				{
					reinterpret_cast<StaticRef*>(dest->v.reference)->Write(args->Source(f));
				}
				else
				{
					GCObject *gco = reinterpret_cast<GCObject*>(dest->v.reference);
					gco->fieldAccessLock.Enter();

					uintptr_t offset = ~destType;
					Value *field = reinterpret_cast<Value*>(
						reinterpret_cast<char*>(dest->v.reference) + offset
					);
					*field = *args->Source(f);

					gco->fieldAccessLock.Leave();
				}

				ip += oa::TWO_LOCALS_SIZE;
				f->stackCount--;
			}
			NEXT_INSTR();

		// callr
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

		// callmemr
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

		// unaryop
		TARGET(OPI_UNARYOP_L)
			{
				OPC_ARGS(oa::TwoLocalsAndValue<Operator>);
				CHK(InvokeOperatorLL(args->Source(f), args->value, 1, args->Dest(f)));
				ip += oa::TWO_LOCALS_AND_VALUE<Operator>::SIZE;
				// InvokeOperatorLL pops arguments off the stack
			}
			NEXT_INSTR();
		TARGET(OPI_UNARYOP_S)
			{
				OPC_ARGS(oa::TwoLocalsAndValue<Operator>);
				CHK(InvokeOperatorLL(args->Source(f), args->value, 1, args->Dest(f)));
				ip += oa::TWO_LOCALS_AND_VALUE<Operator>::SIZE;
				// InvokeOperatorLL pops arguments off the stack
				f->stackCount++;
			}
			NEXT_INSTR();

		default:
			OVUM_UNREACHABLE();
		}
	}

ret:
	OVUM_ASSERT(f->stackCount == 1);
	// And then we just fall through and return!
exitMethod:
	return retCode;
}

int Thread::FindErrorHandler(size_t maxIndex)
{
	StackFrame *frame = currentFrame;
	MethodOverload *method = frame->method;
	size_t offset = (size_t)(this->ip - method->entry);

	if (maxIndex == ALL_TRY_BLOCKS)
		maxIndex = method->tryBlockCount;

	for (size_t t = 0; t < maxIndex; t++)
	{
		TryBlock &tryBlock = method->tryBlocks[t];
		if (tryBlock.Contains(offset))
		{
			// The ip is inside a try block! Let's find a catch or finally.
			switch (tryBlock.kind)
			{
			case TryKind::CATCH:
				for (size_t c = 0; c < tryBlock.catches.count; c++)
				{
					CatchBlock &catchBlock = tryBlock.catches.blocks[c];
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
			case TryKind::FAULT: // When dealing with an error, behaves the same as a finally
				{
					frame->stackCount = 0;
					// See ErrorStack for more details on this
					ErrorStack savedError(this);

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
						this->errorStack = savedError.prev;
						return r;
					}
					this->ip = method->entry + offset;

					this->errorStack = savedError.prev;
					this->currentError = savedError.error;
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

int Thread::EvaluateLeave(StackFrame *frame, int32_t target)
{
	// Note: the IP currently points to the leave instruction. We must add the size
	// of the opcode and the instruction arguments to get the right target offset.
	const size_t LEAVE_SIZE =
		OVUM_ALIGN_TO(sizeof(IntermediateOpcode), opcode_args::ALIGNMENT) +
		opcode_args::BRANCH_SIZE;

	MethodOverload *method = frame->method;
	size_t ipOffset = (size_t)(this->ip - method->entry);
	size_t targetOffset = ipOffset + target + LEAVE_SIZE;
	for (size_t t = 0; t < method->tryBlockCount; t++)
	{
		TryBlock &tryBlock = method->tryBlocks[t];
		// We can evaluate a finally clause here if all of the following are true:
		//   1. tryBlock is a try-finally (i.e. there is a finally to evaluate)
		//   2. The instruction pointer is inside the try clause
		//   3. The branch target is outside of the try clause.
		// That means we're leaving the try clause of a try-finally, hence we have
		// to execute the finally.
		if (tryBlock.kind != TryKind::FINALLY ||
			!tryBlock.Contains(ipOffset) ||
			tryBlock.Contains(targetOffset))
			continue;

		// Let's evaluate the finally!

		uint8_t *const prevIp = this->ip;
		// See ErrorStack for more details on this
		ErrorStack savedError(this);

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
			this->errorStack = savedError.prev;
			return r;
		}
		this->ip = prevIp;

		this->errorStack = savedError.prev;
		this->currentError = savedError.error;
	}

	RETURN_SUCCESS;
}

} // namespace ovum
