#include "ov_vm.internal.h"
#include "ov_thread.opcodes.h"
#include <memory>

#define T_ARG(ip, T)   (*reinterpret_cast<T*>(ip))
#define VAL_ARG(ip)    T_ARG(ip, Value*)
#define I16_ARG(ip)    T_ARG(ip, int16_t)
#define I32_ARG(ip)    T_ARG(ip, int32_t)
#define I64_ARG(ip)    T_ARG(ip, int64_t)
#define U16_ARG(ip)    T_ARG(ip, uint16_t)
#define U32_ARG(ip)    T_ARG(ip, uint32_t)
#define U64_ARG(ip)    T_ARG(ip, uint64_t)
#define OFF_ARG(ip,f)  (T_ARG(ip, LocalOffset) + (f))
#define LOSZ           sizeof(LocalOffset) // For convenience, since it's used a LOT below

#ifdef THREADED_EVALUATION

#define TARGET(opc)	case opc: TARGET_##opc:
#define NEXT_INSTR() \
	{ \
		this->ip = ip; \
		goto *opcodeTargets[*ip++]; \
	}

#else // THREADED_EVALUATION

#define TARGET(opc) case opc:
#define NEXT_INSTR() break

#endif // THREADED_EVALUATION

#define SET_BOOL(ptarg, bvalue) \
	{ \
		(ptarg)->type = VM::vm->types.Boolean; \
		(ptarg)->integer = bvalue; \
	}
#define SET_INT(ptarg, ivalue) \
	{ \
		(ptarg)->type = VM::vm->types.Int; \
		(ptarg)->integer = ivalue; \
	}
#define SET_UINT(ptarg, uvalue) \
	{ \
		(ptarg)->type = VM::vm->types.UInt; \
		(ptarg)->uinteger = uvalue; \
	}
#define SET_REAL(ptarg, rvalue) \
	{ \
		(ptarg)->type = VM::vm->types.Real; \
		(ptarg)->real = rvalue; \
	}
#define SET_STRING(ptarg, svalue) \
	{ \
		(ptarg)->type = VM::vm->types.String; \
		(ptarg)->common.string = svalue; \
	}

void Thread::Evaluate()
{
#ifdef THREADED_EVALUATION
	static void *opcodeTargets[256] = {
#	include "thread.opcode_targets.h"
	};
#endif

	register StackFrame *const f = currentFrame;
	// this->ip has been set to the entry address
	register uint8_t *ip = this->ip;

	while (true)
	{
		if (shouldSuspendForGC)
			SuspendForGC();

		this->ip = ip;
		switch (*ip++) // always skip opcode
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
			goto ret;

		TARGET(OPI_RETNULL)
			{
				assert(f->stackCount == 0);
				f->evalStack->type = nullptr;
				f->stackCount++;
			}
			goto ret;

		// mvloc: LocalOffset source, LocalOffset destination
		TARGET(OPI_MVLOC_LL) // local to local
			{
				*OFF_ARG(ip + LOSZ, f) = *OFF_ARG(ip, f);
				ip += 2*LOSZ;
			}
			NEXT_INSTR();
		TARGET(OPI_MVLOC_SL) // stack to local
			{
				*OFF_ARG(ip + LOSZ, f) = *OFF_ARG(ip, f);
				ip += 2*LOSZ;
				f->stackCount--;
			}
			NEXT_INSTR();
		TARGET(OPI_MVLOC_LS) // local to stack
			{
				*OFF_ARG(ip + LOSZ, f) = *OFF_ARG(ip, f);
				ip += 2*LOSZ;
				f->stackCount++;
			}
			NEXT_INSTR();
		TARGET(OPI_MVLOC_SS) // stack to stack (shouldn't really be used!)
			{
				*OFF_ARG(ip + LOSZ, f) = *OFF_ARG(ip, f);
				ip += 2*LOSZ;
			}
			NEXT_INSTR();

		// ldnull: LocalOffset dest
		TARGET(OPI_LDNULL_L)
			{
				OFF_ARG(ip, f)->type = nullptr;
				ip += LOSZ;
			}
			NEXT_INSTR();
		TARGET(OPI_LDNULL_S)
			{
				OFF_ARG(ip, f)->type = nullptr;
				ip += LOSZ;
				f->stackCount++;
			}
			NEXT_INSTR();

		// ldfalse: LocalOffset dest
		TARGET(OPI_LDFALSE_L)
			{
				SET_BOOL(OFF_ARG(ip, f), false);
				ip += LOSZ;
			}
			NEXT_INSTR();
		TARGET(OPI_LDFALSE_S)
			{
				SET_BOOL(OFF_ARG(ip, f), false);
				ip += LOSZ;
				f->stackCount++;
			}
			NEXT_INSTR();

		// ldtrue: LocalOffset dest
		TARGET(OPI_LDTRUE_L)
			{
				SET_BOOL(OFF_ARG(ip, f), true);
				ip += LOSZ;
			}
			NEXT_INSTR();
		TARGET(OPI_LDTRUE_S)
			{
				SET_BOOL(OFF_ARG(ip, f), true);
				ip += LOSZ;
				f->stackCount++;
			}
			NEXT_INSTR();

		// ldc.i: LocalOffset dest, int64_t value
		TARGET(OPI_LDC_I_L)
			{
				SET_INT(OFF_ARG(ip, f), I64_ARG(ip + LOSZ));
				ip += LOSZ + sizeof(int64_t);
			}
			NEXT_INSTR();
		TARGET(OPI_LDC_I_S)
			{
				SET_INT(OFF_ARG(ip, f), I64_ARG(ip + LOSZ));
				ip += LOSZ + sizeof(int64_t);
				f->stackCount++;
			}
			NEXT_INSTR();

		// ldc.u: LocalOffset dest, uint64_t value
		TARGET(OPI_LDC_U_L)
			{
				SET_UINT(OFF_ARG(ip, f), U64_ARG(ip + LOSZ));
				ip += LOSZ + sizeof(uint64_t);
			}
			NEXT_INSTR();
		TARGET(OPI_LDC_U_S)
			{
				SET_UINT(OFF_ARG(ip, f), U64_ARG(ip + LOSZ));
				ip += LOSZ + sizeof(uint64_t);
				f->stackCount++;
			}
			NEXT_INSTR();

		// ldc.r: LocalOffset dest, double value
		TARGET(OPI_LDC_R_L)
			{
				SET_REAL(OFF_ARG(ip, f), T_ARG(ip + LOSZ, double));
				ip += LOSZ + sizeof(double);
			}
			NEXT_INSTR();
		TARGET(OPI_LDC_R_S)
			{
				SET_REAL(OFF_ARG(ip, f), T_ARG(ip + LOSZ, double));
				ip += LOSZ + sizeof(int64_t);
				f->stackCount++;
			}
			NEXT_INSTR();

		// ldstr: LocalOffset dest, String *value
		TARGET(OPI_LDSTR_L)
			{
				SET_STRING(OFF_ARG(ip, f), T_ARG(ip + LOSZ, String*));
				ip += LOSZ + sizeof(String*);
			}
			NEXT_INSTR();
		TARGET(OPI_LDSTR_S)
			{
				SET_STRING(OFF_ARG(ip, f), T_ARG(ip + LOSZ, String*));
				ip += LOSZ + sizeof(String*);
				f->stackCount++;
			}
			NEXT_INSTR();

		// ldargc: LocalOffset dest
		TARGET(OPI_LDARGC_L)
			{
				SET_INT(OFF_ARG(ip, f), f->argc);
				ip += LOSZ;
			}
			NEXT_INSTR();
		TARGET(OPI_LDARGC_S)
			{
				SET_INT(OFF_ARG(ip, f), f->argc);
				ip += LOSZ;
				f->stackCount++;
			}
			NEXT_INSTR();

		// ldenum: LocalOffset dest, Type *type, int64_t value
		TARGET(OPI_LDENUM_L)
			{
				register Value *const dest = OFF_ARG(ip, f);
				ip += LOSZ;

				dest->type = T_ARG(ip, Type*);
				dest->integer = I64_ARG(ip + sizeof(Type*));
				ip += sizeof(Type*) + sizeof(int64_t);
			}
			NEXT_INSTR();
		TARGET(OPI_LDENUM_S)
			{
				register Value *const dest = OFF_ARG(ip, f);
				ip += LOSZ;

				dest->type = T_ARG(ip, Type*);
				dest->integer = I64_ARG(ip + sizeof(Type*));
				ip += sizeof(Type*) + sizeof(int64_t);

				f->stackCount++;
			}
			NEXT_INSTR();

		// newobj: LocalOffset args, LocalOffset dest, Type *type, uint16_t argc
		TARGET(OPI_NEWOBJ_L)
			{
				register Value *const args = OFF_ARG(ip, f);
				register Value *const dest = OFF_ARG(ip + LOSZ, f);
				ip += 2*LOSZ;

				register Type *const type = T_ARG(ip, Type*);
				ip += sizeof(Type*);

				GC::gc->ConstructLL(this, type, U16_ARG(ip), args, dest);

				// ConstructLL pops the arguments
				ip += sizeof(uint16_t);
			}
			NEXT_INSTR();
		TARGET(OPI_NEWOBJ_S)
			{
				register Value *const args = OFF_ARG(ip, f);
				register Value *const dest = OFF_ARG(ip + LOSZ, f);
				ip += 2*LOSZ;

				register Type *const type = T_ARG(ip, Type*);
				ip += sizeof(Type*);

				GC::gc->ConstructLL(this, type, U16_ARG(ip), args, dest);

				ip += sizeof(uint16_t);
				// ConstructLL pops the arguments
				f->stackCount++;
			}
			NEXT_INSTR();

		// list: LocalOffset dest, int32_t capacity
		TARGET(OPI_LIST_L)
			{
				Value result;
				GC::gc->Alloc(this, VM::vm->types.List, sizeof(ListInst), &result);
				VM::vm->functions.initListInstance(this, result.common.list, I32_ARG(ip + LOSZ));

				*OFF_ARG(ip, f) = result;
				ip += LOSZ + sizeof(int32_t);
			}
			NEXT_INSTR();
		TARGET(OPI_LIST_S)
			{
				Value result; // Can't put it in dest until it's fully initialized
				GC::gc->Alloc(this, VM::vm->types.List, sizeof(ListInst), &result);
				VM::vm->functions.initListInstance(this, result.common.list, I32_ARG(ip + LOSZ));

				*OFF_ARG(ip, f) = result;
				ip += LOSZ + sizeof(int32_t);

				f->stackCount++;
			}
			NEXT_INSTR();

		// hash: LocalOffset dest, int32_t capacity
		TARGET(OPI_HASH_L)
			{
				Value result; // Can't put it in dest until it's fully initialized
				GC::gc->Alloc(this, VM::vm->types.Hash, sizeof(HashInst), &result);
				VM::vm->functions.initHashInstance(this, result.common.hash, I32_ARG(ip + LOSZ));

				*OFF_ARG(ip, f) = result;

				ip += LOSZ + sizeof(int32_t);
			}
			NEXT_INSTR();
		TARGET(OPI_HASH_S)
			{
				Value result; // Can't put it in dest until it's fully initialized
				GC::gc->Alloc(this, VM::vm->types.Hash, sizeof(HashInst), &result);
				VM::vm->functions.initHashInstance(this, result.common.hash, I32_ARG(ip + LOSZ));

				*OFF_ARG(ip, f) = result;

				ip += LOSZ + sizeof(int32_t);
				f->stackCount++;
			}
			NEXT_INSTR();

		// ldfld: LocalOffset instance, LocalOffset dest, Field *field
		TARGET(OPI_LDFLD_L)
			{
				register Value *const inst = OFF_ARG(ip, f);
				register Value *const dest = OFF_ARG(ip + LOSZ, f);
				ip += 2*LOSZ;

				T_ARG(ip, Field*)->ReadField(this, inst, dest);
				ip += sizeof(Field*);
				f->stackCount--;
			}
			NEXT_INSTR();
		TARGET(OPI_LDFLD_S)
			{
				register Value *const inst = OFF_ARG(ip, f);
				register Value *const dest = OFF_ARG(ip + LOSZ, f);
				ip += 2*LOSZ;
											
				T_ARG(ip, Field*)->ReadField(this, inst, dest);
				ip += sizeof(Field*);
			}
			NEXT_INSTR();

		// ldsfld: LocalOffset dest, Field *field
		TARGET(OPI_LDSFLD_L)
			{
				*OFF_ARG(ip, f) = T_ARG(ip + LOSZ, Field*)->staticValue->Read();
				ip += LOSZ + sizeof(Field*);
			}
			NEXT_INSTR();
		TARGET(OPI_LDSFLD_S)
			{
				*OFF_ARG(ip, f) = T_ARG(ip + LOSZ, Field*)->staticValue->Read();
				ip += LOSZ + sizeof(Field*);
				f->stackCount++;
			}
			NEXT_INSTR();

		// ldmem: LocalOffset instance, LocalOffset dest, String *name
		TARGET(OPI_LDMEM_L)
			{
				LoadMemberLL(OFF_ARG(ip, f), // inst
					T_ARG(ip + 2*LOSZ, String*), // name
					OFF_ARG(ip + LOSZ, f)); // dest
				ip += 2*LOSZ + sizeof(String*);
				// LoadMemberLL pops the instance
			}
			NEXT_INSTR();
		TARGET(OPI_LDMEM_S)
			{
				LoadMemberLL(OFF_ARG(ip, f), // inst
					T_ARG(ip + 2*LOSZ, String*), // name
					OFF_ARG(ip + LOSZ, f)); // dest
				ip += 2*LOSZ + sizeof(String*);
				// LoadMemberLL pops the instance
				f->stackCount++;
			}
			NEXT_INSTR();

		// lditer: LocalOffset instance, LocalOffest dest
		TARGET(OPI_LDITER_L)
			{
				InvokeMemberLL(static_strings::_iter, 0,
					OFF_ARG(ip, f), // value
					OFF_ARG(ip + LOSZ, f)); // result
				// InvokeMemberLL pops the instance and all 0 of the arguments
				ip += 2*LOSZ;
			}
			NEXT_INSTR();
		TARGET(OPI_LDITER_S)
			{
				InvokeMemberLL(static_strings::_iter, 0,
					OFF_ARG(ip, f), // value
					OFF_ARG(ip + LOSZ, f)); // result
				// InvokeMemberLL pops the instance and all 0 of the arguments
				ip += 2*LOSZ;
				f->stackCount++;
			}
			NEXT_INSTR();

		// ldtype: LocalOffset instance, LocalOffset dest
		TARGET(OPI_LDTYPE_L)
			{
				register Value *const inst = OFF_ARG(ip, f);
				register Value *const dest = OFF_ARG(ip + LOSZ, f);

				if (inst->type)
					*dest = inst->type->GetTypeToken(this);
				else
					dest->type = nullptr;

				ip += 2*LOSZ;
				f->stackCount--;
			}
			NEXT_INSTR();
		TARGET(OPI_LDTYPE_S)
			{
				register Value *const inst = OFF_ARG(ip, f);
				register Value *const dest = OFF_ARG(ip + LOSZ, f);

				if (inst->type)
					*dest = inst->type->GetTypeToken(this);
				else
					dest->type = nullptr;

				ip += 2*LOSZ;
			}
			NEXT_INSTR();

		// ldidx: LocalOffset args, LocalOffset dest, uint16_t argc
		// Note: argc does not include the instance
		TARGET(OPI_LDIDX_L)
			{
				register Value *const args = OFF_ARG(ip, f);
				register Value *const dest = OFF_ARG(ip + LOSZ, f);
				ip += 2*LOSZ;

				LoadIndexerLL(U16_ARG(ip), args, dest);

				// LoadIndexerLL decrements the stack height by the argument count + instance
				ip += sizeof(uint16_t);
			}
			NEXT_INSTR();
		TARGET(OPI_LDIDX_S)
			{
				register Value *const args = OFF_ARG(ip, f);
				register Value *const dest = OFF_ARG(ip + LOSZ, f);
				ip += 2*LOSZ;

				LoadIndexerLL(U16_ARG(ip), args, dest);

				// LoadIndexerLL decrements the stack height by the argument count + instance
				ip += sizeof(uint16_t);
				f->stackCount++;
			}
			NEXT_INSTR();

		// ldsfn: LocalOffset dest, Method *method
		TARGET(OPI_LDSFN_L)
			{
				register Value *const dest = OFF_ARG(ip, f);
				GC::gc->Alloc(this, VM::vm->types.Method, sizeof(MethodInst), dest);
				ip += LOSZ;

				dest->common.method->method = T_ARG(ip, Method*);
				ip += sizeof(Method*);
			}
			NEXT_INSTR();
		TARGET(OPI_LDSFN_S)
			{
				register Value *const dest = OFF_ARG(ip, f);
				GC::gc->Alloc(this, VM::vm->types.Method, sizeof(MethodInst), dest);
				ip += LOSZ;

				dest->common.method->method = T_ARG(ip, Method*);
				ip += sizeof(Method*);

				f->stackCount++;
			}
			NEXT_INSTR();

		// ldtypetkn: LocalOffset dest, Type *type
		TARGET(OPI_LDTYPETKN_L)
			{
				*OFF_ARG(ip, f) = T_ARG(ip + LOSZ, Type*)->GetTypeToken(this);
				ip += LOSZ + sizeof(Type*);
			}
			NEXT_INSTR();
		TARGET(OPI_LDTYPETKN_S)
			{
				*OFF_ARG(ip, f) = T_ARG(ip + LOSZ, Type*)->GetTypeToken(this);
				ip += LOSZ + sizeof(Type*);
				f->stackCount++;
			}
			NEXT_INSTR();

		// call: LocalOffset args, LocalOffset output, uint16_t argc
		TARGET(OPI_CALL_L)
			{
				register Value *const args   = OFF_ARG(ip, f);
				register Value *const output = OFF_ARG(ip + LOSZ, f);
				ip += 2*LOSZ;

				InvokeLL(U16_ARG(ip), args, output);

				ip += sizeof(uint16_t);
				// InvokeLL pops the arguments
			}
			NEXT_INSTR();
		TARGET(OPI_CALL_S)
			{
				register Value *const args   = OFF_ARG(ip, f);
				register Value *const output = OFF_ARG(ip + LOSZ, f);
				ip += 2*LOSZ;

				InvokeLL(U16_ARG(ip), args, output);

				ip += sizeof(uint16_t);
				// InvokeLL pops the arguments
				f->stackCount++;
			}
			NEXT_INSTR();

		// scall: LocalOffset args, LocalOffset output, uint16_t argc, Method::Overload *method
		TARGET(OPI_SCALL_L)
			{
				register Value *const args   = OFF_ARG(ip, f);
				register Value *const output = OFF_ARG(ip + LOSZ, f);
				ip += 2*LOSZ;
				
				InvokeMethodOverload(T_ARG(ip + sizeof(uint16_t), Method::Overload*),
					U16_ARG(ip), args, output);

				ip += sizeof(uint16_t) + sizeof(Method::Overload*);
			}
			NEXT_INSTR();
		TARGET(OPI_SCALL_S)
			{
				register Value *const args   = OFF_ARG(ip, f);
				register Value *const output = OFF_ARG(ip + LOSZ, f);
				ip += 2*LOSZ;
				
				InvokeMethodOverload(T_ARG(ip + sizeof(uint16_t), Method::Overload*),
					U16_ARG(ip), args, output);

				ip += sizeof(uint16_t) + sizeof(Method::Overload*);
				f->stackCount++;
			}
			NEXT_INSTR();

		// apply: LocalOffset args, LocalOffset output
		TARGET(OPI_APPLY_L)
			{
				InvokeApplyLL(OFF_ARG(ip, f), // args
					OFF_ARG(ip + LOSZ, f)); // output

				ip += 2*LOSZ;
			}
			NEXT_INSTR();
		TARGET(OPI_APPLY_S)
			{
				InvokeApplyLL(OFF_ARG(ip, f), // args
					OFF_ARG(ip + LOSZ, f)); // output

				ip += 2*LOSZ;
				f->stackCount++;
			}
			NEXT_INSTR();

		// sapply: LocalOffset args, LocalOffset output, Method *method
		TARGET(OPI_SAPPLY_L)
			{
				InvokeApplyMethodLL(T_ARG(ip + 2*LOSZ, Method*),
					OFF_ARG(ip, f), // args
					OFF_ARG(ip + LOSZ, f)); // output

				ip += 2*LOSZ + sizeof(Method*);
			}
			NEXT_INSTR();
		TARGET(OPI_SAPPLY_S)
			{
				InvokeApplyMethodLL(T_ARG(ip + 2*LOSZ, Method*),
					OFF_ARG(ip, f), // args
					OFF_ARG(ip + LOSZ, f)); // output

				ip += 2*LOSZ + sizeof(Method*);
				f->stackCount++;
			}
			NEXT_INSTR();

		// br: int32_t offset
		TARGET(OPI_BR)
			{
				ip += I32_ARG(ip);
				ip += sizeof(int32_t);
			}
			NEXT_INSTR();

		// leave: int32_t offset
		TARGET(OPI_LEAVE)
			{
				register const int32_t offset = I32_ARG(ip);
				EvaluateLeave(f, offset);
				ip += sizeof(int32_t) + offset;
			}
			NEXT_INSTR();

		// brnull: LocalOffset value, int32_t offset
		TARGET(OPI_BRNULL_L)
			{
				if (OFF_ARG(ip, f)->type == nullptr)
					ip += I32_ARG(ip + LOSZ);
				ip += LOSZ + sizeof(int32_t);
			}
			NEXT_INSTR();
		TARGET(OPI_BRNULL_S)
			{
				if (OFF_ARG(ip, f)->type == nullptr)
					ip += I32_ARG(ip + LOSZ);
				ip += LOSZ + sizeof(int32_t);
				f->stackCount--;
			}
			NEXT_INSTR();

		// brinst: LocalOffset value, int32_t offset
		TARGET(OPI_BRINST_L)
			{
				if (OFF_ARG(ip, f)->type != nullptr)
					ip += I32_ARG(ip + LOSZ);
				ip += LOSZ + sizeof(int32_t);
			}
			NEXT_INSTR();
		TARGET(OPI_BRINST_S)
			{
				if (OFF_ARG(ip, f)->type != nullptr)
					ip += I32_ARG(ip + LOSZ);
				ip += LOSZ + sizeof(int32_t);
				f->stackCount--;
			}
			NEXT_INSTR();

		// brfalse: LocalOffset value, int32_t offset
		TARGET(OPI_BRFALSE_L)
			{
				if (IsFalse_(OFF_ARG(ip, f)))
					ip += I32_ARG(ip + LOSZ);
				ip += LOSZ + sizeof(int32_t);
			}
			NEXT_INSTR();
		TARGET(OPI_BRFALSE_S)
			{
				if (IsFalse_(OFF_ARG(ip, f)))
					ip += I32_ARG(ip + LOSZ);
				ip += LOSZ + sizeof(int32_t);
				f->stackCount--;
			}
			NEXT_INSTR();

		// brtrue: LocalOffset value, int32_t offset
		TARGET(OPI_BRTRUE_L)
			{
				if (IsTrue_(OFF_ARG(ip, f)))
					ip += I32_ARG(ip + LOSZ);
				ip += LOSZ + sizeof(int32_t);
			}
			NEXT_INSTR();
		TARGET(OPI_BRTRUE_S)
			{
				if (IsTrue_(OFF_ARG(ip, f)))
					ip += I32_ARG(ip + LOSZ);
				ip += LOSZ + sizeof(int32_t);
				f->stackCount--;
			}
			NEXT_INSTR();

		// brtype: LocalOffset value, Type *type, int32_t offset
		TARGET(OPI_BRTYPE_L)
			{
				if (Type::ValueIsType(OFF_ARG(ip, f), T_ARG(ip + LOSZ, Type*)))
					ip += I32_ARG(ip + LOSZ + sizeof(Type*));

				ip += LOSZ + sizeof(Type*) + sizeof(int32_t);
			}
			NEXT_INSTR();
		TARGET(OPI_BRTYPE_S)
			{
				if (Type::ValueIsType(OFF_ARG(ip, f), T_ARG(ip + LOSZ, Type*)))
					ip += I32_ARG(ip + LOSZ + sizeof(Type*));

				ip += LOSZ + sizeof(Type*) + sizeof(int32_t);
				f->stackCount--;
			}
			NEXT_INSTR();

		// switch: LocalOffset value, uint16_t count, int32_t offsets[count]
		TARGET(OPI_SWITCH_L)
			{
				register Value *const value = OFF_ARG(ip, f);
				if (value->type != VM::vm->types.Int)
					ThrowTypeError();

				register int32_t count = U16_ARG(ip + LOSZ);
				ip += LOSZ + sizeof(uint16_t);

				if (value->integer >= 0 && value->integer < count)
					ip += *(reinterpret_cast<int32_t*>(ip) + (int32_t)value->integer);

				ip += count * sizeof(int32_t);
			}
			NEXT_INSTR();
		TARGET(OPI_SWITCH_S)
			{
				register Value *const value = OFF_ARG(ip, f);
				if (value->type != VM::vm->types.Int)
					ThrowTypeError();

				register int32_t count = U16_ARG(ip + LOSZ);
				ip += LOSZ + sizeof(uint16_t);

				if (value->integer >= 0 && value->integer < count)
					ip += *(reinterpret_cast<int32_t*>(ip) + (int32_t)value->integer);

				ip += count * sizeof(int32_t);

				f->stackCount--;
			}
			NEXT_INSTR();

		// brref: LocalOffset (a, b), int32_t offset
		TARGET(OPI_BRREF)
			{
				register Value *const args = OFF_ARG(ip, f);
				ip += LOSZ;

				if (IsSameReference_(args + 0, args + 1))
					ip += I32_ARG(ip);
				ip += sizeof(int32_t);

				f->stackCount -= 2;
			}
			NEXT_INSTR();

		// brnref: LocalOffset (a, b), int32_t offset
		TARGET(OPI_BRNREF)
			{
				register Value *const args = OFF_ARG(ip, f);
				ip += LOSZ;

				if (!IsSameReference_(args + 0, args + 1))
					ip += I32_ARG(ip);
				ip += sizeof(int32_t);

				f->stackCount -= 2;
			}
			NEXT_INSTR();

		// operator: LocalOffset args, LocalOffset dest, Operator op
		TARGET(OPI_OPERATOR_L)
			{
				register Value *const args = OFF_ARG(ip, f);
				register Value *const dest = OFF_ARG(ip + LOSZ, f);
				ip += 2*LOSZ;

				InvokeOperatorLL(args, T_ARG(ip, Operator), dest);
				ip += sizeof(Operator);

				// InvokeOperatorLL pops arguments off the stack
			}
			NEXT_INSTR();
		TARGET(OPI_OPERATOR_S)
			{
				register Value *const args = OFF_ARG(ip, f);
				register Value *const dest = OFF_ARG(ip + LOSZ, f);
				ip += 2*LOSZ;

				InvokeOperatorLL(args, T_ARG(ip, Operator), dest);
				ip += sizeof(Operator);

				// InvokeOperatorLL pops arguments off the stack
				f->stackCount++;
			}
			NEXT_INSTR();

		// eq: LocalOffset args, LocalOffset dest
		TARGET(OPI_EQ_L)
			{
				SetBool_(OFF_ARG(ip + LOSZ, f), EqualsLL(OFF_ARG(ip, f)));
				ip += 2*LOSZ;
				// EqualsLL pops arguments off the stack
			}
			NEXT_INSTR();
		TARGET(OPI_EQ_S)
			{
				SetBool_(OFF_ARG(ip + LOSZ, f), EqualsLL(OFF_ARG(ip, f)));
				ip += 2*LOSZ;
				// EqualsLL pops arguments off the stack
				f->stackCount++;
			}
			NEXT_INSTR();

		// cmp: LocalOffset args, LocalOffset dest
		TARGET(OPI_CMP_L)
			{
				CompareLL(OFF_ARG(ip, f), OFF_ARG(ip + LOSZ, f));
				ip += 2*LOSZ;
				// CompareLL pops arguments off the stack
			}
			NEXT_INSTR();
		TARGET(OPI_CMP_S)
			{
				CompareLL(OFF_ARG(ip, f), OFF_ARG(ip + LOSZ, f));
				ip += 2*LOSZ;
				// CompareLL pops arguments off the stack
				f->stackCount++;
			}
			NEXT_INSTR();

		// lt: LocalOffset args, LocalOffset dest
		TARGET(OPI_LT_L)
			{
				register bool result = CompareLessThanLL(OFF_ARG(ip, f));
				SetBool_(OFF_ARG(ip + LOSZ, f), result);
				ip += 2*LOSZ;
				// CompareLL pops arguments off the stack
			}
			NEXT_INSTR();
		TARGET(OPI_LT_S)
			{
				register bool result = CompareLessThanLL(OFF_ARG(ip, f));
				SetBool_(OFF_ARG(ip + LOSZ, f), result);
				ip += 2*LOSZ;
				// CompareLL pops arguments off the stack
				f->stackCount++;
			}
			NEXT_INSTR();

		// gt: LocalOffset args, LocalOffset dest
		TARGET(OPI_GT_L)
			{
				register bool result = CompareGreaterThanLL(OFF_ARG(ip, f));
				SetBool_(OFF_ARG(ip + LOSZ, f), result);
				ip += 2*LOSZ;
				// CompareLL pops arguments off the stack
			}
			NEXT_INSTR();
		TARGET(OPI_GT_S)
			{
				register bool result = CompareGreaterThanLL(OFF_ARG(ip, f));
				SetBool_(OFF_ARG(ip + LOSZ, f), result);
				ip += 2*LOSZ;
				// CompareLL pops arguments off the stack
				f->stackCount++;
			}
			NEXT_INSTR();

		// lte: LocalOffset args, LocalOffset dest
		TARGET(OPI_LTE_L)
			{
				register bool result = CompareLessEqualsLL(OFF_ARG(ip, f));
				SetBool_(OFF_ARG(ip + LOSZ, f), result);
				ip += 2*LOSZ;
				// CompareLL pops arguments off the stack
			}
			NEXT_INSTR();
		TARGET(OPI_LTE_S)
			{
				register bool result = CompareLessEqualsLL(OFF_ARG(ip, f));
				SetBool_(OFF_ARG(ip + LOSZ, f), result);
				ip += 2*LOSZ;
				// CompareLL pops arguments off the stack
				f->stackCount++;
			}
			NEXT_INSTR();

		// gte: LocalOffset args, LocalOffset dest
		TARGET(OPI_GTE_L)
			{
				register bool result = CompareGreaterEqualsLL(OFF_ARG(ip, f));
				SetBool_(OFF_ARG(ip + LOSZ, f), result);
				ip += 2*LOSZ;
				// CompareLL pops arguments off the stack
			}
			NEXT_INSTR();
		TARGET(OPI_GTE_S)
			{
				register bool result = CompareGreaterEqualsLL(OFF_ARG(ip, f));
				SetBool_(OFF_ARG(ip + LOSZ, f), result);
				ip += 2*LOSZ;
				// CompareLL pops arguments off the stack
				f->stackCount++;
			}
			NEXT_INSTR();

		// concat: LocalOffset args, LocalOffset dest
		TARGET(OPI_CONCAT_L)
			{
				ConcatLL(OFF_ARG(ip, f), OFF_ARG(ip + LOSZ, f));
				ip += 2*LOSZ;
				// ConcatLL pops arguments off stack
			}
			NEXT_INSTR();
		TARGET(OPI_CONCAT_S)
			{
				ConcatLL(OFF_ARG(ip, f), OFF_ARG(ip + LOSZ, f));
				ip += 2*LOSZ;
				// ConcatLL pops arguments off stack
				f->stackCount++;
			}
			NEXT_INSTR();

		// callmem: LocalOffset args, LocalOffset dest, String *member, uint16_t argCount
		TARGET(OPI_CALLMEM_L)
			{
				register Value *const args = OFF_ARG(ip, f);
				register Value *const dest = OFF_ARG(ip + LOSZ, f);
				ip += 2*LOSZ;

				InvokeMemberLL(T_ARG(ip, String*), U16_ARG(ip + sizeof(String*)), args, dest);

				ip += sizeof(String*) + sizeof(uint16_t);
			}
			NEXT_INSTR();
		TARGET(OPI_CALLMEM_S)
			{
				register Value *const args = OFF_ARG(ip, f);
				register Value *const dest = OFF_ARG(ip + LOSZ, f);
				ip += 2*LOSZ;

				InvokeMemberLL(T_ARG(ip, String*), U16_ARG(ip + sizeof(String*)), args, dest);

				ip += sizeof(String*) + sizeof(uint16_t);
				f->stackCount++;
			}
			NEXT_INSTR();

		// stsfld: LocalOffset value, Field *field
		TARGET(OPI_STSFLD_L)
			{
				T_ARG(ip + LOSZ, Field*)->staticValue->Write(OFF_ARG(ip, f));
				ip += LOSZ + sizeof(Field*);
			}
			NEXT_INSTR();
		TARGET(OPI_STSFLD_S)
			{
				T_ARG(ip + LOSZ, Field*)->staticValue->Write(OFF_ARG(ip, f));
				ip += LOSZ + sizeof(Field*);
				f->stackCount--;
			}
			NEXT_INSTR();

		// stfld: LocalOffset (instance, value), Field *field
		TARGET(OPI_STFLD)
			{
				register Value *const values = OFF_ARG(ip, f);
				T_ARG(ip + LOSZ, Field*)->WriteField(this, values);

				ip += LOSZ + sizeof(Field*);
				f->stackCount -= 2;
			}
			NEXT_INSTR();

		// stmem: LocalOffset (instance, value), String *name
		TARGET(OPI_STMEM)
			{
				// StoreMemberLL performs a null check
				StoreMemberLL(OFF_ARG(ip, f), T_ARG(ip + LOSZ, String*));

				// It also pops the things off the stack
				ip += LOSZ + sizeof(String*);
			}
			NEXT_INSTR();

		// stidx: LocalOffset args, uint16_t argCount
		// Note: argCount does not include the instance, or the value being assigned
		TARGET(OPI_STIDX)
			{
				// StoreIndexerLL performs a null check
				StoreIndexerLL(U16_ARG(ip + LOSZ), // argCount
					OFF_ARG(ip, f)); // args

				// It also pops things off the stack
				ip += LOSZ + sizeof(uint16_t);
			}
			NEXT_INSTR();

		TARGET(OPI_THROW)
			{
				Throw(/*rethrow:*/ false);
			}
			NEXT_INSTR();

		TARGET(OPI_RETHROW)
			{
				Throw(/*rethrow:*/ true);
			}
			NEXT_INSTR();

		TARGET(OPI_ENDFINALLY)
			// This Evaluate call was reached through FindErrorHandlers or
			// EvaluateLeave, so we return here and let the thing continue
			// with its search for more error handlers.
			goto endfinally;

		// ldfldfast: LocalOffset instance, LocalOffset dest, Field *field
		// This is identical to ldfld except that it does not perform a type check.
		TARGET(OPI_LDFLDFAST_L)
			{
				register Value *const inst = OFF_ARG(ip, f);
				register Value *const dest = OFF_ARG(ip + LOSZ, f);
				ip += 2*LOSZ;
				
				T_ARG(ip, Field*)->ReadFieldFast(this, inst, dest);
				ip += sizeof(Field*);
				f->stackCount--;
			}
			NEXT_INSTR();
		TARGET(OPI_LDFLDFAST_S)
			{
				register Value *const inst = OFF_ARG(ip, f);
				register Value *const dest = OFF_ARG(ip + LOSZ, f);
				ip += 2*LOSZ;
											
				T_ARG(ip, Field*)->ReadFieldFast(this, inst, dest);
				ip += sizeof(Field*);
			}
			NEXT_INSTR();

		// stfldfast: LocalOffset (instance, value), Field *field
		// This is identical to stfld except that it does not perform a type check.
		TARGET(OPI_STFLDFAST)
			{
				register Value *const values = OFF_ARG(ip, f);
				T_ARG(ip + LOSZ, Field*)->WriteFieldFast(this, values);

				ip += LOSZ + sizeof(Field*);
				f->stackCount -= 2;
			}
			NEXT_INSTR();

		// breq: LocalOffset args, int32_t offset
		TARGET(OPI_BREQ)
			{
				if (EqualsLL(OFF_ARG(ip, f)))
					ip += I32_ARG(ip + LOSZ);
				ip += LOSZ + sizeof(int32_t);
			}
			NEXT_INSTR();

		// brneq: LocalOffset args, int32_t offset
		TARGET(OPI_BRNEQ)
			{
				if (!EqualsLL(OFF_ARG(ip, f)))
					ip += I32_ARG(ip + LOSZ);
				ip += LOSZ + sizeof(int32_t);
			}
			NEXT_INSTR();

		// brlt: LocalOffset args, int32_t offset
		TARGET(OPI_BRLT)
			{
				register bool result = CompareLessThanLL(OFF_ARG(ip, f));
				if (result)
					ip += I32_ARG(ip + LOSZ);
				ip += LOSZ + sizeof(int32_t);
			}
			NEXT_INSTR();

		// brgt: LocalOffset args, int32_t offset
		TARGET(OPI_BRGT)
			{
				register bool result = CompareGreaterThanLL(OFF_ARG(ip, f));
				if (result)
					ip += I32_ARG(ip + LOSZ);
				ip += LOSZ + sizeof(int32_t);
			}
			NEXT_INSTR();

		// brlte: LocalOffset args, int32_t offset
		TARGET(OPI_BRLTE)
			{
				register bool result = CompareLessEqualsLL(OFF_ARG(ip, f));
				if (result)
					ip += I32_ARG(ip + LOSZ);
				ip += LOSZ + sizeof(int32_t);
			}
			NEXT_INSTR();

		// brgte: LocalOffset args, int32_t offset
		TARGET(OPI_BRGTE)
			{
				register bool result = CompareGreaterEqualsLL(OFF_ARG(ip, f));
				if (result)
					ip += I32_ARG(ip + LOSZ);
				ip += LOSZ + sizeof(int32_t);
			}
			NEXT_INSTR();
		}

#ifdef THREADED_EVALUATION
		throw L"Evaluate fell through the switch.";
#endif
	}

ret:
	assert(f->stackCount == 1);
	// And then we just fall through and return!
endfinally:
	return;
}

bool Thread::FindErrorHandler()
{
	typedef Method::TryBlock::TryKind TryKind;

	register StackFrame *frame = currentFrame;
	Method::Overload *method = frame->method;
	uint32_t offset = (uint32_t)(this->ip - method->entry);
	for (int32_t t = 0; t < method->tryBlockCount; t++)
	{
		Method::TryBlock &tryBlock = method->tryBlocks[t];
		if (offset >= tryBlock.tryStart && offset <= tryBlock.tryEnd)
		{
			// The ip is inside a try block! Let's find a catch or finally.
			switch (tryBlock.kind)
			{
			case TryKind::CATCH:
				for (int32_t c = 0; c < tryBlock.catches.count; c++)
				{
					Method::CatchBlock &catchBlock = tryBlock.catches.blocks[c];
					if (Type::ValueIsType(&currentError, catchBlock.caughtType))
					{
						frame->stackCount = 0;
						frame->Push(currentError);
						this->ip = method->entry + catchBlock.catchStart;
						return true;
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
					try { Evaluate(); }
					catch (OvumException&)
					{
						if (FindErrorHandler())
							goto enter;
						throw;
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
	return false;
}

void Thread::EvaluateLeave(register StackFrame *frame, const int32_t target)
{
	typedef Method::TryBlock::TryKind TryKind;

	// Note: the IP currently points to the leave instruction.
	// We must add sizeof(IntermediateOpcode) + sizeof(int32_t) to get
	// the right ipOffset and tOffset.

	Method::Overload *method = frame->method;
	const uint32_t ipOffset = (uint32_t)(this->ip + sizeof(IntermediateOpcode) + sizeof(int32_t) - method->entry);
	const uint32_t tOffset  = ipOffset + target;
	for (int32_t t = 0; t < method->tryBlockCount; t++)
	{
		Method::TryBlock &tryBlock = method->tryBlocks[t];
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
			try { Evaluate(); }
			catch (OvumException&)
			{
				if (FindErrorHandler())
					goto enter;
				throw;
			}
			this->ip = prevIp;

			this->currentError = prevError;
		}
	}
}