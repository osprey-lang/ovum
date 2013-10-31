// ================================
//  THIS FILE IS NOT PART OF OVUM.
//  DO NOT COMPILE IT.
// ================================
// This was a test, where I had an additional ArgumentSizes array for skipping the
// arguments of certain opcodes. It turned out to be very slightly detrimental to
// performance, so the changes were reverted. I have kept it here in case I should
// choose to revisit this idea, in which case I won't have to rewrite /quite/ as
// much of the code.
#include "ov_vm.internal.h"
#include "ov_thread.opcodes.h"
#include <memory>

#define T_ARG(ip, T) (*reinterpret_cast<T*>(ip))
#define VAL_ARG(ip)  T_ARG(ip, Value*)
#define I16_ARG(ip)  T_ARG(ip, int16_t)
#define I32_ARG(ip)  T_ARG(ip, int32_t)
#define I64_ARG(ip)  T_ARG(ip, int64_t)
#define U16_ARG(ip)  T_ARG(ip, uint16_t)
#define U32_ARG(ip)  T_ARG(ip, uint32_t)
#define U64_ARG(ip)  T_ARG(ip, uint64_t)
#define OFF_ARG(ip, frame) (T_ARG(ip, LocalOffset) + (frame))
#define LOSZ         sizeof(LocalOffset) // For convenience, since it's used a LOT below

// Stack changes for the various instructions, indexed by opcode. (Actual values at the bottom)
// Note that if the instruction's stack change depends on one of its arguments, then we still
// have to apply that change under the case label. Note also, however, that most of those changes
// are performed by one of the methods that invoke stuff, as PushStackFrame decrements the
// stack height by argCount.
int8_t StackChanges[];
// Similarly, this is a list of the size of instructions minus sizeof(IntermediateOpcode).
// In other words, it's a list of the size of their arguments. We use this to increment the
// instruction pointer after each instruction.
int8_t ArgumentSizes[];

void Thread::Evaluate(StackFrame *frame, uint8_t *entryAddress)
{
#if NDEBUG
	uint8_t *ip = this->ip = entryAddress;
#else
	ip = entryAddress;
#endif

	while (true)
	{
		try
		{
			IntermediateOpcode opc = *reinterpret_cast<IntermediateOpcode*>(ip);
			ip++; // skip opcode, always

			switch (opc)
			{
			case OPI_NOP: break; // Really, do nothing!
			case OPI_POP:
				//frame->stackCount--; // pop just decrements the stack count, which we do below
				break;
			case OPI_RET:
				assert(frame->stackCount == 1);
				goto done;
			case OPI_RETNULL:
				assert(frame->stackCount == 0);
				frame->evalStack[0] = NULL_VALUE;
				frame->stackCount++; // We need to put this here, because we're skipping past the stack count change below the switch
				goto done;
			case OPI_MVLOC_LL:
			case OPI_MVLOC_SL:
			case OPI_MVLOC_LS:
			case OPI_MVLOC_SS:
				// mvloc: LocalOffset source, LocalOffset destination
				{
					*OFF_ARG(ip + LOSZ, frame) = *OFF_ARG(ip, frame);

					//ip += 2*LOSZ;
					// (Copied from ov_thread.opcodes.h)
					// mvloc encodes the stack change in its lowest two bits:
					// 0000 001ar
					//         a  = if set, one value was added
					//          r = if set, one value was removed
					//frame->stackCount += ((opc & 2) >> 1) - (opc & 1);
				}
				break;
			case OPI_LDNULL_L:
			case OPI_LDNULL_S:
				// ldnull: LocalOffset dest
				{
					*OFF_ARG(ip, frame) = NULL_VALUE;

					//ip += LOSZ;
					//frame->stackCount += opc & 1;
				}
				break;
			case OPI_LDFALSE_L:
			case OPI_LDFALSE_S:
			case OPI_LDTRUE_L:
			case OPI_LDTRUE_S:
				// ldfalse: LocalOffset dest
				// ldtrue: LocalOffset dest
				{
					SetBool_(OFF_ARG(ip, frame), (opc >> 2) & 1);

					//ip += LOSZ;
					//frame->stackCount += opc & 1;
				}
				break;
			case OPI_LDC_I_L:
			case OPI_LDC_I_S:
				// ldc.i: LocalOffset dest, int64_t value
				{
					SetInt_(OFF_ARG(ip, frame), I64_ARG(ip + LOSZ));

					//ip += LOSZ + sizeof(int64_t);
					//frame->stackCount += opc & 1;
				}
				break;
			case OPI_LDC_U_L:
			case OPI_LDC_U_S:
				// ldc.u: LocalOffset dest, uint64_t value
				{
					SetUInt_(OFF_ARG(ip, frame), U64_ARG(ip + LOSZ));

					//ip += LOSZ + sizeof(int64_t);
					//frame->stackCount += opc & 1;
				}
				break;
			case OPI_LDC_R_L:
			case OPI_LDC_R_S:
				// ldc.r: LocalOffset dest, double value
				{
					SetReal_(OFF_ARG(ip, frame), T_ARG(ip + LOSZ, double));

					//ip += LOSZ + sizeof(int64_t);
					//frame->stackCount += opc & 1;
				}
				break;
			case OPI_LDSTR_L:
			case OPI_LDSTR_S:
				// ldstr: LocalOffset dest, String *value
				{
					SetString_(OFF_ARG(ip, frame), T_ARG(ip + LOSZ, String*));

					//ip += LOSZ + sizeof(String*);
					//frame->stackCount += opc & 1;
				}
				break;
			case OPI_LDARGC_L:
			case OPI_LDARGC_S:
				// ldargc: LocalOffset dest
				{
					SetInt_(OFF_ARG(ip, frame), frame->argc);

					//ip += LOSZ;
					//frame->stackCount += opc & 1;
				}
				break;
			case OPI_LDENUM_L:
			case OPI_LDENUM_S:
				// ldenum: LocalOffset dest, Type *type, int64_t value
				{
					Value *const dest = OFF_ARG(ip, frame);

					dest->type = T_ARG(ip + LOSZ, Type*);
					dest->integer = I64_ARG(ip + LOSZ + sizeof(Type*));
					//ip += LOSZ + sizeof(Type*) + sizeof(int64_t);

					//frame->stackCount += opc & 1;
				}
				break;
			case OPI_NEWOBJ_L:
			case OPI_NEWOBJ_S:
				// newobj: LocalOffset args, LocalOffset dest, Type *type, uint16_t argc
				{
					GC::gc->ConstructLL(this,
						T_ARG(ip + 2*LOSZ, Type*), // type
						U16_ARG(ip + 2*LOSZ + sizeof(Type*)), // argCount
						OFF_ARG(ip, frame), // args
						OFF_ARG(ip + LOSZ, frame)); // dest

					ip += 2*LOSZ + sizeof(Type*) + sizeof(uint16_t);
					// ConstructLL pops the arguments
					//frame->stackCount += opc & 1;
				}
				break;
			case OPI_LIST_L:
			case OPI_LIST_S:
				// list: LocalOffset dest, int32_t capacity
				{
					Value result; // Can't put it in dest until it's fully initialized
					GC::gc->Alloc(this, VM::vm->types.List, sizeof(ListInst), &result);
					VM::vm->functions.initListInstance(this, result.common.list, I32_ARG(ip + LOSZ));

					*OFF_ARG(ip, frame) = result;

					//ip += LOSZ + sizeof(int32_t);
					//frame->stackCount += opc & 1;
				}
				break;
			case OPI_HASH_L:
			case OPI_HASH_S:
				// hash: LocalOffset dest, int32_t capacity
				{
					Value result; // Can't put it in dest until it's fully initialized
					GC::gc->Alloc(this, VM::vm->types.Hash, sizeof(HashInst), &result);
					VM::vm->functions.initHashInstance(this, result.common.hash, I32_ARG(ip + LOSZ));

					*OFF_ARG(ip, frame) = result;

					//ip += LOSZ + sizeof(int32_t);
					//frame->stackCount += opc & 1;
				}
				break;
			case OPI_LDFLD_L:
			case OPI_LDFLD_S:
				// ldfld: LocalOffset instance, LocalOffset dest, Field *field
				{
					*OFF_ARG(ip + LOSZ, frame) = *T_ARG(ip + 2*LOSZ, Field*)->GetField(this, OFF_ARG(ip, frame));
						
					//ip += 2*LOSZ + sizeof(Field*);
					//frame->stackCount += (opc & 1) - 1;
				}
				break;
			case OPI_LDSFLD_L:
			case OPI_LDSFLD_S:
				// ldsfld: LocalOffset dest, Field *field
				{
					*OFF_ARG(ip, frame) = *T_ARG(ip + LOSZ, Field*)->staticValue;

					//ip += LOSZ + sizeof(Field*);
					//frame->stackCount += opc & 1;
				}
				break;
			case OPI_LDMEM_L:
			case OPI_LDMEM_S:
				// ldmem: LocalOffset instance, LocalOffset dest, String *name
				{
					LoadMemberLL(OFF_ARG(ip, frame), // inst
						T_ARG(ip + 2*LOSZ, String*), // name
						OFF_ARG(ip + LOSZ, frame)); // dest

					//ip += 2*LOSZ + sizeof(String*);
					// LoadMemberLL pops the instance
					//frame->stackCount += opc & 1;
				}
				break;
			case OPI_LDITER_L:
			case OPI_LDITER_S:
				// lditer: LocalOffset instance, LocalOffest dest
				{
					InvokeMemberLL(static_strings::_iter, 0,
						OFF_ARG(ip, frame), // value
						OFF_ARG(ip + LOSZ, frame)); // result

					// InvokeMemberLL pops the instance and all 0 of the arguments
					//ip += 2*LOSZ;
					//frame->stackCount += opc & 1;
				}
				break;
			case OPI_LDTYPE_L:
			case OPI_LDTYPE_S:
				// ldtype: LocalOffset instance, LocalOffset dest
				{
					Value *const inst = OFF_ARG(ip, frame);
					Value *const dest = OFF_ARG(ip + LOSZ, frame);

					if (inst->type)
						*dest = inst->type->GetTypeToken(this);
					else
						*dest = NULL_VALUE;

					//ip += 2*LOSZ;
					//frame->stackCount += (opc & 1) - 1;
				}
				break;
			case OPI_LDIDX_L:
			case OPI_LDIDX_S:
				// ldidx: LocalOffset args, LocalOffset dest, uint16_t argc
				// Note: argc does not include the instance
				{
					LoadIndexerLL(U16_ARG(ip + 2*LOSZ), // argCount
						OFF_ARG(ip, frame), // args
						OFF_ARG(ip + LOSZ, frame)); // dest

					// LoadIndexerLL decrements the stack height by the argument count + instance
					//ip += 2*LOSZ + sizeof(uint16_t);
					//frame->stackCount += opc & 1;
				}
				break;
			case OPI_LDSFN_L:
			case OPI_LDSFN_S:
				// ldsfn: LocalOffset dest, Method *method
				{
					Value *const dest = OFF_ARG(ip, frame);
					GC::gc->Alloc(this, VM::vm->types.Method, sizeof(MethodInst), dest);

					dest->common.method->method = T_ARG(ip + LOSZ, Method*);
					//ip += LOSZ + sizeof(Method*);

					//frame->stackCount += opc & 1;
				}
				break;
			case OPI_LDTYPETKN_L:
			case OPI_LDTYPETKN_S:
				// ldtypetkn: LocalOffset dest, Type *type
				{
					*OFF_ARG(ip, frame) = T_ARG(ip + LOSZ, Type*)->GetTypeToken(this);
						
					//ip += LOSZ + sizeof(Type*);
					//frame->stackCount += opc & 1;
				}
				break;
			case OPI_CALL_L:
			case OPI_CALL_S:
				// call: LocalOffset args, LocalOffset output, uint16_t argc
				{
					InvokeLL(U16_ARG(ip + 2*LOSZ), // argCount
						OFF_ARG(ip, frame), // args
						OFF_ARG(ip + LOSZ, frame)); // output

					//ip += 2*LOSZ + sizeof(uint16_t);
					// InvokeLL pops the arguments
					//frame->stackCount += opc & 1;
				}
				break;
			case OPI_SCALL_L:
			case OPI_SCALL_S:
				// scall: LocalOffset args, LocalOffset output, uint16_t argc, Method::Overload *method
				{
					InvokeMethodOverload(T_ARG(ip + 2*LOSZ + sizeof(uint16_t), Method::Overload*),
						U16_ARG(ip + 2*LOSZ), // argCount
						OFF_ARG(ip, frame), // args
						OFF_ARG(ip + LOSZ, frame)); // output

					//ip += 2*LOSZ + sizeof(uint16_t) + sizeof(Method::Overload*);
					//frame->stackCount += opc & 1;
				}
				break;
			case OPI_APPLY_L:
			case OPI_APPLY_S:
				// apply: LocalOffset args, LocalOffset output
				{
					InvokeApplyLL(OFF_ARG(ip, frame), // args
						OFF_ARG(ip + LOSZ, frame)); // output

					//ip += 2*LOSZ;
					//frame->stackCount += opc & 1;
				}
				break;
			case OPI_SAPPLY_L:
			case OPI_SAPPLY_S:
				// sapply: LocalOffset args, LocalOffset output, Method *method
				{
					InvokeApplyMethodLL(T_ARG(ip + 2*LOSZ, Method*),
						OFF_ARG(ip, frame), // args
						OFF_ARG(ip + LOSZ, frame)); // output

					//ip += 2*LOSZ + sizeof(Method*);
					//frame->stackCount += opc & 1;
				}
				break;
			case OPI_BR:
				// br: int32_t offset
				ip += I32_ARG(ip);
				//ip += sizeof(int32_t);
				break;
			case OPI_LEAVE:
				// leave: int32_t offset
				{
					const int32_t offset = I32_ARG(ip);
					EvaluateLeave(frame, ip, offset);
					ip += /*sizeof(int32_t)*/ + offset;
				}
				break;
			case OPI_BRNULL_L:
			case OPI_BRNULL_S:
				// brnull: LocalOffset value, int32_t offset
				{
					if (OFF_ARG(ip, frame)->type == nullptr)
						ip += I32_ARG(ip + LOSZ);
					//ip += LOSZ + sizeof(int32_t);

					//frame->stackCount -= opc & 1;
				}
				break;
			case OPI_BRINST_L:
			case OPI_BRINST_S:
				// brinst: LocalOffset value, int32_t offset
				{
					if (OFF_ARG(ip, frame)->type != nullptr)
						ip += I32_ARG(ip + LOSZ);
					//ip += LOSZ + sizeof(int32_t);

					//frame->stackCount -= opc & 1;
				}
				break;
			case OPI_BRFALSE_L:
			case OPI_BRFALSE_S:
				// brfalse: LocalOffset value, int32_t offset
				{
					if (IsFalse_(OFF_ARG(ip, frame)))
						ip += I32_ARG(ip + LOSZ);
					//ip += LOSZ + sizeof(int32_t);

					//frame->stackCount -= opc & 1;
				}
				break;
			case OPI_BRTRUE_L:
			case OPI_BRTRUE_S:
				// brtrue: LocalOffset value, int32_t offset
				{
					if (IsTrue_(OFF_ARG(ip, frame)))
						ip += I32_ARG(ip + LOSZ);
					//ip += LOSZ + sizeof(int32_t);

					//frame->stackCount -= opc & 1;
				}
				break;
			case OPI_BRTYPE_L:
			case OPI_BRTYPE_S:
				// brtype: LocalOffset value, Type *type, int32_t offset
				{
					if (Type::ValueIsType(*OFF_ARG(ip, frame), T_ARG(ip + LOSZ, Type*)))
						ip += I32_ARG(ip + LOSZ + sizeof(Type*));

					//ip += LOSZ + sizeof(Type*);
					ip += sizeof(int32_t);

					//frame->stackCount -= opc & 1;
				}
				break;
			case OPI_SWITCH_L:
			case OPI_SWITCH_S:
				// switch: LocalOffset value, uint16_t count, int32_t offsets[count]
				{
					Value *const value = OFF_ARG(ip, frame);

					if (value->type != VM::vm->types.Int)
						ThrowTypeError();

					uint16_t count = U16_ARG(ip + LOSZ);
					//ip += LOSZ + sizeof(uint16_t);

					if (value->integer >= 0 && value->integer < count)
						ip += *(reinterpret_cast<int32_t*>(ip + LOSZ + sizeof(uint16_t)) + (int32_t)value->integer);

					// Note: we have to skip the jump table, because its size
					// depends on 'count'.
					ip += count * sizeof(int32_t);

					//frame->stackCount -= opc & 1;
				}
				break;
			case OPI_BRREF:
			case OPI_BRNREF:
				// brnref: LocalOffset (a, b), int32_t offset
				{
					Value *const args = OFF_ARG(ip, frame);
					//ip += LOSZ;

					if (IsSameReference_(args[0], args[1]) ^ (opc & 1))
						ip += I32_ARG(ip + LOSZ);
					//ip += sizeof(int32_t);

					//frame->stackCount -= 2;
				}
				break;
			case OPI_OPERATOR_L:
			case OPI_OPERATOR_S:
				// operator: LocalOffset args, LocalOffset dest, Operator op
				{
					Value *const args = OFF_ARG(ip, frame);
					Value *const dest = OFF_ARG(ip + LOSZ, frame);
					//ip += 2*LOSZ;

					InvokeOperatorLL(args, T_ARG(ip + 2*LOSZ, Operator), dest);
					//ip += sizeof(Operator);

					// InvokeOperatorLL pops arguments off the stack
					//frame->stackCount += opc & 1;
				}
				break;
			case OPI_EQ_L:
			case OPI_EQ_S:
				// eq: LocalOffset args, LocalOffset dest
				{
					SetBool_(OFF_ARG(ip + LOSZ, frame),
						EqualsLL(OFF_ARG(ip, frame)));

					//ip += 2*LOSZ;
					// EqualsLL pops arguments off the stack
					//frame->stackCount += opc & 1;
				}
				break;
			case OPI_CMP_L:
			case OPI_CMP_S:
				// cmp: LocalOffset args, LocalOffset dest
				{
					int result = CompareLL(OFF_ARG(ip, frame));
					SetInt_(OFF_ARG(ip + LOSZ, frame), result);

					//ip += 2*LOSZ;
					// CompareLL pops arguments off the stack
					//frame->stackCount += opc & 1;
				}
				break;
			case OPI_LT_L:
			case OPI_LT_S:
				// lt: LocalOffset args, LocalOffset dest
				{
					bool result = CompareLL(OFF_ARG(ip, frame)) < 0;
					SetBool_(OFF_ARG(ip + LOSZ, frame), result);

					//ip += 2*LOSZ;
					// CompareLL pops arguments off the stack
					//frame->stackCount += opc & 1;
				}
				break;
			case OPI_GT_L:
			case OPI_GT_S:
				// gt: LocalOffset args, LocalOffset dest
				{
					bool result = CompareLL(OFF_ARG(ip, frame)) > 0;
					SetBool_(OFF_ARG(ip + LOSZ, frame), result);

					//ip += 2*LOSZ;
					// CompareLL pops arguments off the stack
					//frame->stackCount += opc & 1;
				}
				break;
			case OPI_LTE_L:
			case OPI_LTE_S:
				// lte: LocalOffset args, LocalOffset dest
				{
					bool result = CompareLL(OFF_ARG(ip, frame)) <= 0;
					SetBool_(OFF_ARG(ip + LOSZ, frame), result);

					//ip += 2*LOSZ;
					// CompareLL pops arguments off the stack
					//frame->stackCount += opc & 1;
				}
				break;
			case OPI_GTE_L:
			case OPI_GTE_S:
				// gte: LocalOffset args, LocalOffset dest
				{
					bool result = CompareLL(OFF_ARG(ip, frame)) >= 0;
					SetBool_(OFF_ARG(ip + LOSZ, frame), result);

					//ip += 2*LOSZ;
					// CompareLL pops arguments off the stack
					//frame->stackCount += opc & 1;
				}
				break;
			case OPI_CONCAT_L:
			case OPI_CONCAT_S:
				// concat: LocalOffset args, LocalOffset dest
				{
					ConcatLL(OFF_ARG(ip, frame), OFF_ARG(ip + LOSZ, frame));

					//ip += 2*LOSZ;
					// ConcatLL pops arguments off stack
					//frame->stackCount += opc & 1;
				}
				break;
			case OPI_CALLMEM_L:
			case OPI_CALLMEM_S:
				// callmem: LocalOffset args, LocalOffset dest, String *member, uint16_t argCount
				{
					Value *const args = OFF_ARG(ip, frame);
					Value *const dest = OFF_ARG(ip + LOSZ, frame);
					//ip += 2*LOSZ;

					String *member = T_ARG(ip + 2*LOSZ, String*);
					//ip += sizeof(String*);

					uint16_t argCount = U16_ARG(ip + 2*LOSZ + sizeof(String*));

					InvokeMemberLL(member, argCount, args, dest);

					//ip += sizeof(uint16_t);
					//frame->stackCount += opc & 1;
				}
				break;
			case OPI_STSFLD_L:
			case OPI_STSFLD_S:
				// stsfld: LocalOffset value, Field *field
				{
					*T_ARG(ip + LOSZ, Field*)->staticValue = *OFF_ARG(ip, frame);

					//ip += LOSZ + sizeof(Field*);
					//frame->stackCount -= opc & 1;
				}
				break;
			case OPI_STFLD:
				// stfld: LocalOffset (instance, value), Field *field
				{
					Value *const values = OFF_ARG(ip, frame);
					*T_ARG(ip + LOSZ, Field*)->GetField(this, values) = values[1];

					//ip += LOSZ + sizeof(Field*);
					//frame->stackCount -= 2;
				}
				break;
			case OPI_STMEM:
				// stmem: LocalOffset (instance, value), String *name
				{
					// StoreMemberLL performs a null check
					StoreMemberLL(OFF_ARG(ip, frame), T_ARG(ip + LOSZ, String*));

					// It also pops the things off the stack
					//ip += LOSZ + sizeof(String*);
				}
				break;
			case OPI_STIDX:
				// stidx: LocalOffset args, uint16_t argCount
				{
					// StoreIndexerLL performs a null check
					StoreIndexerLL(U16_ARG(ip + LOSZ), // argCount
						OFF_ARG(ip, frame)); // args

					//ip += LOSZ + sizeof(uint16_t);
				}
				break;
			case OPI_THROW: // odd
			case OPI_RETHROW: // even
				Throw(/*rethrow:*/ (opc & 1) == 0);
				break;
			case OPI_ENDFINALLY:
				// This Evaluate call was reached through FindErrorHandlers,
				// so we return here and let the thing continue with its search
				// for more error handlers.
				return;
			case OPI_LDFLDFAST_L:
			case OPI_LDFLDFAST_S:
				// ldfldfast: LocalOffset instance, LocalOffset dest, Field *field
				// This is identical to ldfld except that it does not perform a type check.
				{
					*OFF_ARG(ip + LOSZ, frame) = *T_ARG(ip + 2*LOSZ, Field*)
						->GetFieldFast(this, OFF_ARG(ip, frame));

					//ip += 2*LOSZ + sizeof(Field*);
					//frame->stackCount += (opc & 1) - 1;
				}
				break;
			case OPI_STFLDFAST:
				// stfldfast: LocalOffset (instance, value), Field *field
				// This is identical to stfld except that it does not perform a type check.
				{
					Value *const values = OFF_ARG(ip, frame);
					*T_ARG(ip + LOSZ, Field*)->GetFieldFast(this, values) = values[1];

					//ip += LOSZ + sizeof(Member*);
					//frame->stackCount -= 2;
				}
				break;
			}
			frame->stackCount += StackChanges[opc];
			ip += ArgumentSizes[opc];
		}
		catch (OvumException&)
		{
			if (!FindErrorHandler(frame, ip))
			{
#if !NDEBUG
				this->ip = frame->prevInstr;
#endif
				this->currentFrame = frame->prevFrame;
				throw;
			}
		}
	}

	done: assert(frame->stackCount == 1);
	// And then we just fall through and return!
}

bool Thread::FindErrorHandler(StackFrame *frame, uint8_t * &ip)
{
	typedef Method::TryBlock::TryKind TryKind;

	Method::Overload *method = frame->method;
	uint32_t offset = (uint32_t)(ip - method->entry);
	for (int32_t t = 0; t < method->tryBlockCount; t++)
	{
		Method::TryBlock &tryBlock = method->tryBlocks[t];
		if (offset >= tryBlock.tryStart && offset <= tryBlock.tryEnd)
		{
			// The ip is inside a try block! Let's find a catch or finally.
			if (tryBlock.kind == TryKind::CATCH)
			{
				for (int32_t c = 0; c < tryBlock.catches.count; c++)
				{
					Method::CatchBlock &catchBlock = tryBlock.catches.blocks[c];
					if (Type::ValueIsType(currentError, catchBlock.caughtType))
					{
						frame->stackCount = 0;
						frame->Push(currentError);
						ip = method->entry + catchBlock.catchStart;
						return true;
					}
				}
			}
			else if (tryBlock.kind == TryKind::FINALLY)
			{
				frame->stackCount = 0;
				// Continue evaluation inside the finally block. When the inner
				// Evaluate returns, we continue searching through the try blocks,
				// until we've exhausted all of them.
#if !NDEBUG
				uint8_t *const prevIp = ip;
#endif
				Evaluate(frame, method->entry + tryBlock.finallyBlock.finallyStart);
#if !NDEBUG
				this->ip = prevIp;
#endif
			}
			// We can't stop enumerating the blocks just yet.
			// There may be another try block that actually handles the error.
		}
	}
	return false;
}

void Thread::EvaluateLeave(StackFrame *frame, uint8_t *ip, const int32_t target)
{
	typedef Method::TryBlock::TryKind TryKind;

	Method::Overload *method = frame->method;
	const uint32_t ipOffset = (uint32_t)(ip - method->entry);
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
			Evaluate(frame, method->entry + tryBlock.finallyBlock.finallyStart);
#if !NDEBUG
			this->ip = ip;
#endif
		}
	}
}

int8_t StackChanges[] = {
	// Note: it is vital here that there are no gap.
	// In the unlikely case that there should arise non-contiguous
	// opcodes, you must pad the gaps with something!
	/* OPI_NOP         0x00 */ 0,
	/* OPI_POP         0x01 */ -1,
	/* OPI_RET         0x02 */ 0,
	/* OPI_RETNULL     0x03 */ 1,
	/* OPI_MVLOC_LL    0x04 */ 0, // Local -> local, no stack change
	/* OPI_MVLOC_SL    0x05 */ -1, // Stack -> local, one value was removed
	/* OPI_MVLOC_LS    0x06 */ 1, // Local -> stack, one value was added
	/* OPI_MVLOC_SS    0x07 */ 0, // Stack -> stack, not used (what would this even mean?)
	/* OPI_LDNULL_L    0x08 */ 0, // -> local
	/* OPI_LDNULL_S    0x09 */ 1, // -> stack
	/* OPI_LDFALSE_L   0x0a */ 0,
	/* OPI_LDFALSE_S   0x0b */ 1,
	/* OPI_LDTRUE_L    0x0c */ 0,
	/* OPI_LDTRUE_S    0x0d */ 1,
	/* OPI_LDC_I_L     0x0e */ 0,
	/* OPI_LDC_I_S     0x0f */ 1,
	/* OPI_LDC_U_L     0x10 */ 0,
	/* OPI_LDC_U_S     0x11 */ 1,
	/* OPI_LDC_R_L     0x12 */ 0,
	/* OPI_LDC_R_S     0x13 */ 1,
	/* OPI_LDSTR_L     0x14 */ 0,
	/* OPI_LDSTR_S     0x15 */ 1,
	/* OPI_LDARGC_L    0x16 */ 0,
	/* OPI_LDARGC_S    0x17 */ 1,
	/* OPI_LDENUM_L    0x18 */ 0,
	/* OPI_LDENUM_S    0x19 */ 1,
	/* OPI_NEWOBJ_L    0x1a */ 0, // Store new object in local
	/* OPI_NEWOBJ_S    0x1b */ 1, // Store new object on stack
	/* OPI_LIST_L      0x1c */ 0,
	/* OPI_LIST_S      0x1d */ 1,
	/* OPI_HASH_L      0x1e */ 0,
	/* OPI_HASH_S      0x1f */ 1,
	/* OPI_LDFLD_L     0x20 */ 0,
	/* OPI_LDFLD_S     0x21 */ 1,
	/* OPI_LDSFLD_L    0x22 */ 0,
	/* OPI_LDSFLD_S    0x23 */ 1,
	/* OPI_LDMEM_L     0x24 */ 0,
	/* OPI_LDMEM_S     0x25 */ 1,
	/* OPI_LDITER_L    0x26 */ 0,
	/* OPI_LDITER_S    0x27 */ 1,
	/* OPI_LDTYPE_L    0x28 */ 0,
	/* OPI_LDTYPE_S    0x29 */ 1,
	/* OPI_LDIDX_L     0x2a */ 0,
	/* OPI_LDIDX_S     0x2b */ 1,
	/* OPI_LDSFN_L     0x2c */ 0,
	/* OPI_LDSFN_S     0x2d */ 1,
	/* OPI_LDTYPETKN_L 0x2e */ 0,
	/* OPI_LDTYPETKN_S 0x2f */ 1,
	/* OPI_CALL_L      0x30 */ 0, // Store return value in local
	/* OPI_CALL_S      0x31 */ 1, // Store return value on stack
	/* OPI_SCALL_L     0x32 */ 0, // Store return value in local
	/* OPI_SCALL_S     0x33 */ 1, // Store return value on stack
	/* OPI_APPLY_L     0x34 */ 0, // Store return value in local
	/* OPI_APPLY_S     0x35 */ 1, // Store return value on stack
	/* OPI_SAPPLY_L    0x36 */ 0, // Store return value in local
	/* OPI_SAPPLY_S    0x37 */ 1, // Store return value on stack 
	/* OPI_BR          0x38 */ 0,
	/* OPI_LEAVE       0x39 */ 0,
	/* OPI_BRNULL_L    0x3a */ 0,
	/* OPI_BRNULL_S    0x3b */ -1,
	/* OPI_BRINST_L    0x3c */ 0,
	/* OPI_BRINST_S    0x3d */ -1,
	/* OPI_BRFALSE_L   0x3e */ 0,
	/* OPI_BRFALSE_S   0x3f */ -1,
	/* OPI_BRTRUE_L    0x40 */ 0,
	/* OPI_BRTRUE_S    0x41 */ -1,
	/* OPI_BRTYPE_L    0x42 */ 0,
	/* OPI_BRTYPE_S    0x43 */ -1,
	/* OPI_SWITCH_L    0x44 */ 0,
	/* OPI_SWITCH_S    0x45 */ -1,
	/* OPI_BRREF       0x46 */ -2,
	/* OPI_BRNREF      0x47 */ -2,
	/* OPI_OPERATOR_L  0x48 */ 0, // Store result in local
	/* OPI_OPERATOR_S  0x49 */ 1, // Store result on stack
	/* OPI_EQ_L        0x4a */ 0, // Store result in local
	/* OPI_EQ_S        0x4b */ 1, // Store result on stack
	/* OPI_CMP_L       0x4c */ 0, // <=>
	/* OPI_CMP_S       0x4d */ 1,
	/* OPI_LT_L        0x4e */ 0, // <
	/* OPI_LT_S        0x4f */ 1,
	/* OPI_GT_L        0x50 */ 0, // >
	/* OPI_GT_S        0x51 */ 1,
	/* OPI_LTE_L       0x52 */ 0, // <=
	/* OPI_LTE_S       0x53 */ 1,
	/* OPI_GTE_L       0x54 */ 0, // >=
	/* OPI_GTE_S       0x55 */ 1,
	/* OPI_CONCAT_L    0x56 */ 0, // Store result in local
	/* OPI_CONCAT_S    0x57 */ 1, // Stack on result store
	/* OPI_CALLMEM_L   0x58 */ 0, // Store result in local
	/* OPI_CALLMEM_S   0x59 */ 1, // Store result on stack
	/* OPI_STSFLD_L    0x5a */ 0, // Get value from local
	/* OPI_STSFLD_S    0x5b */ -1, // Get value from stack
	/* OPI_STFLD       0x5c */ -2, // Always get values from stack
	/* OPI_STMEM       0x5d */ 0, // Same here
	/* OPI_STIDX       0x5e */ 0, // And here
	/* OPI_THROW       0x5f */ -1,
	/* OPI_RETHROW     0x60 */ 0,
	/* OPI_ENDFINALLY  0x61 */ 0,
	/* OPI_LDFLDFAST_L 0x62 */ 0,
	/* OPI_LDFLDFAST_S 0x63 */ 1,
	/* OPI_STFLDFAST   0x64 */ -2,
};

int8_t ArgumentSizes[] = {
	// Note: it is vital here that there are no gap.
	// In the unlikely case that there should arise non-contiguous
	// opcodes, you must pad the gaps with something!
	/* OPI_NOP         0x00 */ 0,
	/* OPI_POP         0x01 */ 0,
	/* OPI_RET         0x02 */ 0,
	/* OPI_RETNULL     0x03 */ 0,
	/* OPI_MVLOC_LL    0x04 */ 2*LOSZ,                    // mvloc: LocalOffset source, LocalOffset dest
	/* OPI_MVLOC_SL    0x05 */ 2*LOSZ,
	/* OPI_MVLOC_LS    0x06 */ 2*LOSZ,
	/* OPI_MVLOC_SS    0x07 */ 2*LOSZ,
	/* OPI_LDNULL_L    0x08 */ LOSZ,                      // ldnull: LocalOffset dest
	/* OPI_LDNULL_S    0x09 */ LOSZ,
	/* OPI_LDFALSE_L   0x0a */ LOSZ,                      // ldfalse: LocalOffset dest
	/* OPI_LDFALSE_S   0x0b */ LOSZ,
	/* OPI_LDTRUE_L    0x0c */ LOSZ,                      // ldtrue: LocalOffset
	/* OPI_LDTRUE_S    0x0d */ LOSZ,
	/* OPI_LDC_I_L     0x0e */ LOSZ + sizeof(int64_t),    // ldc.i: LocalOffset dest, int64_t value
	/* OPI_LDC_I_S     0x0f */ LOSZ + sizeof(int64_t),
	/* OPI_LDC_U_L     0x10 */ LOSZ + sizeof(uint64_t),   // ldc.u: LocalOffest dest, uint64_t value
	/* OPI_LDC_U_S     0x11 */ LOSZ + sizeof(uint64_t),
	/* OPI_LDC_R_L     0x12 */ LOSZ + sizeof(double),     // ldc.r: LocalOffset dest, double value
	/* OPI_LDC_R_S     0x13 */ LOSZ + sizeof(double),
	/* OPI_LDSTR_L     0x14 */ LOSZ + sizeof(String*),    // ldstr: LocalOffset dest, String *value
	/* OPI_LDSTR_S     0x15 */ LOSZ + sizeof(String*),
	/* OPI_LDARGC_L    0x16 */ LOSZ,                      // ldargc: LocalOffest dest
	/* OPI_LDARGC_S    0x17 */ LOSZ,
	/* OPI_LDENUM_L    0x18 */ LOSZ + sizeof(Type*) + sizeof(int64_t), // ldenum: LocalOffset dest, Type *type, int64_t value
	/* OPI_LDENUM_S    0x19 */ LOSZ + sizeof(Type*) + sizeof(int64_t),
	/* OPI_NEWOBJ_L    0x1a */ 2*LOSZ + sizeof(Type*) + sizeof(uint16_t), // newobj: LocalOffset args, LocalOffset dest, Type *type, uint16_t argc
	/* OPI_NEWOBJ_S    0x1b */ 2*LOSZ + sizeof(Type*) + sizeof(uint16_t),
	/* OPI_LIST_L      0x1c */ LOSZ + sizeof(int32_t),    // list: LocalOffset dest, int32_t capacity
	/* OPI_LIST_S      0x1d */ LOSZ + sizeof(int32_t),
	/* OPI_HASH_L      0x1e */ LOSZ + sizeof(int32_t),    // hash: LocalOffset dest, int32_t capacity
	/* OPI_HASH_S      0x1f */ LOSZ + sizeof(int32_t),
	/* OPI_LDFLD_L     0x20 */ 2*LOSZ + sizeof(Field*),   // ldfld: LocalOffset inst, LocalOffset dest, Field *field
	/* OPI_LDFLD_S     0x21 */ 2*LOSZ + sizeof(Field*),
	/* OPI_LDSFLD_L    0x22 */ LOSZ + sizeof(Field*),     // ldsfld: LocalOffset dest, Field *field
	/* OPI_LDSFLD_S    0x23 */ LOSZ + sizeof(Field*),
	/* OPI_LDMEM_L     0x24 */ 2*LOSZ + sizeof(String*),  // ldmem: LocalOffset inst, LocalOffset dest, String *member
	/* OPI_LDMEM_S     0x25 */ 2*LOSZ + sizeof(String*),
	/* OPI_LDITER_L    0x26 */ 2*LOSZ,                    // lditer: LocalOffset inst, LocalOffset dest
	/* OPI_LDITER_S    0x27 */ 2*LOSZ,
	/* OPI_LDTYPE_L    0x28 */ 2*LOSZ,                    // lditer: LocalOffset inst, LocalOffset dest
	/* OPI_LDTYPE_S    0x29 */ 2*LOSZ,
	/* OPI_LDIDX_L     0x2a */ 2*LOSZ + sizeof(uint16_t), // ldidx: LocalOffset inst, LocalOffset dest, uint16_t argc
	/* OPI_LDIDX_S     0x2b */ 2*LOSZ + sizeof(uint16_t),
	/* OPI_LDSFN_L     0x2c */ LOSZ + sizeof(Method*),    // ldsfn: LocalOffest dest, Method *method
	/* OPI_LDSFN_S     0x2d */ LOSZ + sizeof(Method*),
	/* OPI_LDTYPETKN_L 0x2e */ LOSZ + sizeof(Type*),      // ldtypetkn: LocalOffest dest, Type *type
	/* OPI_LDTYPETKN_S 0x2f */ LOSZ + sizeof(Type*),
	/* OPI_CALL_L      0x30 */ 2*LOSZ + sizeof(uint16_t), // call: LocalOffset args, LocalOffset dest, uint16_t argc
	/* OPI_CALL_S      0x31 */ 2*LOSZ + sizeof(uint16_t),
	/* OPI_SCALL_L     0x32 */ 2*LOSZ + sizeof(uint16_t) + sizeof(Method::Overload*), // scall: LocalOffset args, LocalOffest dest, uint16_t argc, Method::Overload *method
	/* OPI_SCALL_S     0x33 */ 2*LOSZ + sizeof(uint16_t) + sizeof(Method::Overload*),
	/* OPI_APPLY_L     0x34 */ 2*LOSZ,                    // apply: LocalOffset args, LocalOffset dest
	/* OPI_APPLY_S     0x35 */ 2*LOSZ,
	/* OPI_SAPPLY_L    0x36 */ 2*LOSZ + sizeof(Method*),  // sapply: LocalOffset args, LocalOffset dest, Method *method
	/* OPI_SAPPLY_S    0x37 */ 2*LOSZ + sizeof(Method*), 
	/* OPI_BR          0x38 */ sizeof(int32_t),           // br: int32_t offset
	/* OPI_LEAVE       0x39 */ sizeof(int32_t),           // leave: int32_t offset
	/* OPI_BRNULL_L    0x3a */ LOSZ + sizeof(int32_t),    // brnull: LocalOffset value, int32_t offset
	/* OPI_BRNULL_S    0x3b */ LOSZ + sizeof(int32_t),
	/* OPI_BRINST_L    0x3c */ LOSZ + sizeof(int32_t),    // brinst: LocalOffset value, int32_t offset
	/* OPI_BRINST_S    0x3d */ LOSZ + sizeof(int32_t),
	/* OPI_BRFALSE_L   0x3e */ LOSZ + sizeof(int32_t),    // brfalse: LocalOffset value, int32_t offset
	/* OPI_BRFALSE_S   0x3f */ LOSZ + sizeof(int32_t),
	/* OPI_BRTRUE_L    0x40 */ LOSZ + sizeof(int32_t),    // brtrue: LocalOffset value, int32_t offset
	/* OPI_BRTRUE_S    0x41 */ LOSZ + sizeof(int32_t),
	/* OPI_BRTYPE_L    0x42 */ LOSZ + sizeof(Type*) + sizeof(int32_t), // brtype: LocalOffset value, Type *type, int32_t offset
	/* OPI_BRTYPE_S    0x43 */ LOSZ + sizeof(Type*) + sizeof(int32_t),
	/* OPI_SWITCH_L    0x44 */ LOSZ + sizeof(uint16_t),   // switch: LocalOffset value, uint16_t offc, int32_t offset[offc]
	/* OPI_SWITCH_S    0x45 */ LOSZ + sizeof(uint16_t),   // (Since the size depends on the offset array, we skip it in the instruction)
	/* OPI_BRREF       0x46 */ LOSZ + sizeof(int32_t),    // brref: LocalOffset (a, b), int32_t offset
	/* OPI_BRNREF      0x47 */ LOSZ + sizeof(int32_t),    // brnref: LocalOffset (a, b), int32_t offset
	/* OPI_OPERATOR_L  0x48 */ 2*LOSZ + sizeof(Operator), // operator: LocalOffset args, LocalOffset dest, Operator op
	/* OPI_OPERATOR_S  0x49 */ 2*LOSZ + sizeof(Operator),
	/* OPI_EQ_L        0x4a */ 2*LOSZ,                    // eq: LocalOffset args, LocalOffset dest
	/* OPI_EQ_S        0x4b */ 2*LOSZ,
	/* OPI_CMP_L       0x4c */ 2*LOSZ,                    // cmp: LocalOffset args, LocalOffset dest
	/* OPI_CMP_S       0x4d */ 2*LOSZ,
	/* OPI_LT_L        0x4e */ 2*LOSZ,                    // lt: LocalOffset args, LocalOffset dest
	/* OPI_LT_S        0x4f */ 2*LOSZ,
	/* OPI_GT_L        0x50 */ 2*LOSZ,                    // gt: LocalOffset args, LocalOffset dest
	/* OPI_GT_S        0x51 */ 2*LOSZ,
	/* OPI_LTE_L       0x52 */ 2*LOSZ,                    // lte: LocalOffset args, LocalOffset dest
	/* OPI_LTE_S       0x53 */ 2*LOSZ,
	/* OPI_GTE_L       0x54 */ 2*LOSZ,                    // gte: LocalOffset args, LocalOffset dest
	/* OPI_GTE_S       0x55 */ 2*LOSZ,
	/* OPI_CONCAT_L    0x56 */ 2*LOSZ,                    // concat: LocalOffset args, LocalOffset dest
	/* OPI_CONCAT_S    0x57 */ 2*LOSZ,
	/* OPI_CALLMEM_L   0x58 */ 2*LOSZ + sizeof(String*) + sizeof(uint16_t), // callmem: LocalOffset args, LocalOffset dest, String *member, uint16_t argCount
	/* OPI_CALLMEM_S   0x59 */ 2*LOSZ + sizeof(String*) + sizeof(uint16_t),
	/* OPI_STSFLD_L    0x5a */ LOSZ + sizeof(Field*),     // stsfld: LocalOffset value, Field *field
	/* OPI_STSFLD_S    0x5b */ LOSZ + sizeof(Field*),
	/* OPI_STFLD       0x5c */ LOSZ + sizeof(Field*),     // stfld: LocalOffset (inst, value), Field *field
	/* OPI_STMEM       0x5d */ LOSZ + sizeof(String*),    // stmem: LocalOffset (inst, value), String *member
	/* OPI_STIDX       0x5e */ LOSZ + sizeof(uint16_t),   // stidx: LocalOffset args, uint16_t argCount
	/* OPI_THROW       0x5f */ 0,
	/* OPI_RETHROW     0x60 */ 0,
	/* OPI_ENDFINALLY  0x61 */ 0,
	/* OPI_LDFLDFAST_L 0x62 */ 2*LOSZ + sizeof(Field*),   // ldfldfast: LocalOffset inst, LocalOffset dest, Field *field
	/* OPI_LDFLDFAST_S 0x63 */ 2*LOSZ + sizeof(Field*),
	/* OPI_STFLDFAST   0x64 */ LOSZ + sizeof(Field*),     // stfldfast: LocalOffset (inst, value), Field *field
};