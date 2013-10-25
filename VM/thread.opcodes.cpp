#include "ov_vm.internal.h"
#include "ov_thread.opcodes.h"
#include <memory>

#define VAL_ARG(ip)  *reinterpret_cast<Value  **>(ip)
#define I16_ARG(ip)  *reinterpret_cast<int16_t *>(ip)
#define I32_ARG(ip)  *reinterpret_cast<int32_t *>(ip)
#define I64_ARG(ip)  *reinterpret_cast<int64_t *>(ip)
#define U16_ARG(ip)  *reinterpret_cast<uint16_t*>(ip)
#define U32_ARG(ip)  *reinterpret_cast<uint32_t*>(ip)
#define U64_ARG(ip)  *reinterpret_cast<uint64_t*>(ip)
#define OFF_ARG(ip)  *reinterpret_cast<LocalOffset*>(ip)

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
				frame->stackCount--;
				break;
			case OPI_RET:
				assert(frame->stackCount == 1);
				goto done;
			case OPI_RETNULL:
				assert(frame->stackCount == 0);
				frame->evalStack[0] = NULL_VALUE;
				frame->stackCount++;
				goto done;
			case OPI_MVLOC_LL:
			case OPI_MVLOC_SL:
			case OPI_MVLOC_LS:
			case OPI_MVLOC_SS:
				// mvloc: LocalOffset source, LocalOffset destination
				{
					Value *const source = OFF_ARG(ip) + frame;
					Value *const dest = OFF_ARG(ip + sizeof(LocalOffset)) + frame;
					*dest = *source;
					// (Copied from ov_thread.opcodes.h)
					// mvloc encodes the stack change in its lowest two bits:
					// 0000 001ar
					//         a  = if set, one value was added
					//          r = if set, one value was removed
					frame->stackCount += ((opc & 2) >> 1) - (opc & 1);

					ip += 2 * sizeof(LocalOffset);
				}
				break;
			case OPI_LDNULL_L:
			case OPI_LDNULL_S:
				// ldnull: LocalOffset dest
				{
					*(OFF_ARG(ip) + frame) = NULL_VALUE;
					frame->stackCount += opc & 1;

					ip += sizeof(LocalOffset);
				}
				break;
			case OPI_LDFALSE_L:
			case OPI_LDFALSE_S:
			case OPI_LDTRUE_L:
			case OPI_LDTRUE_S:
				// ldfalse: LocalOffset dest
				// ldtrue: LocalOffset dest
				{
					SetBool_(OFF_ARG(ip) + frame, (opc >> 2) & 1);
					frame->stackCount += opc & 1;

					ip += sizeof(LocalOffset);
				}
				break;
			case OPI_LDC_I_L:
			case OPI_LDC_I_S:
				// ldc.i: LocalOffset dest, int64_t value
				{
					Value *const dest = OFF_ARG(ip) + frame;
					SetInt_(dest, I64_ARG(ip + sizeof(LocalOffset)));
					frame->stackCount += opc & 1;

					ip += sizeof(LocalOffset) + sizeof(int64_t);
				}
				break;
			case OPI_LDC_U_L:
			case OPI_LDC_U_S:
				// ldc.u: LocalOffset dest, uint64_t value
				{
					Value *const dest = OFF_ARG(ip) + frame;
					SetUInt_(dest, U64_ARG(ip + sizeof(LocalOffset)));
					frame->stackCount += opc & 1;

					ip += sizeof(LocalOffset) + sizeof(int64_t);
				}
				break;
			case OPI_LDC_R_L:
			case OPI_LDC_R_S:
				// ldc.r: LocalOffset dest, double value
				{
					Value *const dest = OFF_ARG(ip) + frame;
					double value = *reinterpret_cast<double*>(ip + sizeof(LocalOffset));
					SetReal_(dest, value);
					frame->stackCount += opc & 1;

					ip += sizeof(LocalOffset) + sizeof(int64_t);
				}
				break;
			case OPI_LDSTR_L:
			case OPI_LDSTR_S:
				// ldstr: LocalOffset dest, String *value
				{
					Value *const dest = OFF_ARG(ip) + frame;
					String *const value = *reinterpret_cast<String**>(ip + sizeof(LocalOffset));
					SetString_(dest, value);
					frame->stackCount += opc & 1;

					ip += sizeof(LocalOffset) + sizeof(String*);
				}
				break;
			case OPI_LDARGC_L:
			case OPI_LDARGC_S:
				// ldargc: LocalOffset dest
				{
					SetInt_(OFF_ARG(ip) + frame, frame->argc);
					frame->stackCount += opc & 1;

					ip += sizeof(LocalOffset);
				}
				break;
			case OPI_LDENUM_L:
			case OPI_LDENUM_S:
				// ldenum: LocalOffset dest, Type *type, int64_t value
				{
					Value *const dest = OFF_ARG(ip) + frame;
					ip += sizeof(LocalOffset);

					dest->type = *reinterpret_cast<Type**>(ip);
					ip += sizeof(Type*);

					dest->integer = I64_ARG(ip);
					ip += sizeof(int64_t);

					frame->stackCount += opc & 1;
				}
				break;
			case OPI_NEWOBJ_L:
			case OPI_NEWOBJ_S:
				// newobj: LocalOffset args, LocalOffset dest, Type *type, uint16_t argc
				{
					Value *const args = OFF_ARG(ip) + frame;
					Value *const dest = OFF_ARG(ip + sizeof(LocalOffset)) + frame;
					ip += 2 * sizeof(LocalOffset);

					Type *const type = *reinterpret_cast<Type**>(ip);
					ip += sizeof(Type*);

					uint16_t argc = U16_ARG(ip);

					GC::gc->ConstructLL(this, type, argc, args, dest);

					ip += sizeof(uint16_t);
					// ConstructLL pops the arguments
					frame->stackCount += opc & 1;
				}
				break;
			case OPI_LIST_L:
			case OPI_LIST_S:
				// list: LocalOffset dest, int32_t capacity
				{
					Value *const dest = OFF_ARG(ip) + frame;
					int32_t cap = I32_ARG(ip + sizeof(LocalOffset));

					Value result; // Can't put it in dest until it's fully initialized
					GC::gc->Alloc(this, VM::vm->types.List, sizeof(ListInst), &result);
					VM::vm->functions.initListInstance(this, result.common.list, cap);

					*dest = result;

					ip += sizeof(LocalOffset) + sizeof(int32_t);
					frame->stackCount += opc & 1;
				}
				break;
			case OPI_HASH_L:
			case OPI_HASH_S:
				// hash: LocalOffset dest, int32_t capacity
				{
					Value *const dest = OFF_ARG(ip) + frame;
					int32_t cap = I32_ARG(ip + sizeof(LocalOffset));

					Value result; // Can't put it in dest until it's fully initialized
					GC::gc->Alloc(this, VM::vm->types.Hash, sizeof(HashInst), &result);
					VM::vm->functions.initHashInstance(this, result.common.hash, cap);

					*dest = result;

					ip += sizeof(LocalOffset) + sizeof(int32_t);
					frame->stackCount += opc & 1;
				}
				break;
			case OPI_LDFLD_L:
			case OPI_LDFLD_S:
				// ldfld: LocalOffset instance, LocalOffset dest, Member *field
				{
					Value *const inst = OFF_ARG(ip) + frame;
					Value *const dest = OFF_ARG(ip + sizeof(LocalOffset)) + frame;
					ip += 2 * sizeof(LocalOffset);

					Field *const field = *reinterpret_cast<Field**>(ip);
						
					*dest = *field->GetField(this, inst);
						
					ip += sizeof(Field*);
					frame->stackCount += (opc & 1) - 1;
				}
				break;
			case OPI_LDSFLD_L:
			case OPI_LDSFLD_S:
				// ldsfld: LocalOffset dest, Field *field
				{
					Value *const dest = OFF_ARG(ip) + frame;
					ip += sizeof(LocalOffset);

					Field *const field = *reinterpret_cast<Field**>(ip);
					ip += sizeof(Field*);

					*dest = *field->staticValue;

					frame->stackCount += opc & 1;
				}
				break;
			case OPI_LDMEM_L:
			case OPI_LDMEM_S:
				// ldmem: LocalOffset instance, LocalOffset dest, String *name
				{
					Value *const inst = OFF_ARG(ip) + frame;
					Value *const dest = OFF_ARG(ip + sizeof(LocalOffset)) + frame;
					ip += 2 * sizeof(LocalOffset);

					String *const name = *reinterpret_cast<String**>(ip);
					LoadMemberLL(inst, name, dest);

					ip += sizeof(String*);
					// LoadMemberLL pops the instance
					frame->stackCount += opc & 1;
				}
				break;
			case OPI_LDITER_L:
			case OPI_LDITER_S:
				// lditer: LocalOffset instance, LocalOffest dest
				{
					Value *const inst = OFF_ARG(ip) + frame;
					Value *const dest = OFF_ARG(ip + sizeof(LocalOffset)) + frame;

					InvokeMemberLL(static_strings::_iter, 0, inst, dest);

					// InvokeMemberLL pops the instance and all 0 of the arguments
					ip += 2 * sizeof(LocalOffset);
					frame->stackCount += opc & 1;
				}
				break;
			case OPI_LDTYPE_L:
			case OPI_LDTYPE_S:
				// ldtype: LocalOffset instance, LocalOffset dest
				{
					Value *const inst = OFF_ARG(ip) + frame;
					Value *const dest = OFF_ARG(ip + sizeof(LocalOffset)) + frame;

					if (inst->type)
						*dest = inst->type->GetTypeToken(this);
					else
						*dest = NULL_VALUE;

					ip += 2 * sizeof(LocalOffset);
					frame->stackCount += (opc & 1) - 1;
				}
				break;
			case OPI_LDIDX_L:
			case OPI_LDIDX_S:
				// ldidx: LocalOffset args, LocalOffset dest, uint16_t argc
				// Note: argc does not include the instance
				{
					Value *const args = OFF_ARG(ip) + frame;
					Value *const dest = OFF_ARG(ip + sizeof(LocalOffset)) + frame;
					ip += 2 * sizeof(LocalOffset);
					uint16_t argc = U16_ARG(ip);

					LoadIndexerLL(argc, args, dest);

					// LoadIndexerLL decrements the stack height by the argument count + instance
					ip += sizeof(uint16_t);
					frame->stackCount += opc & 1;
				}
				break;
			case OPI_LDSFN_L:
			case OPI_LDSFN_S:
				// ldsfn: LocalOffset dest, Method *method
				{
					Value *const dest = OFF_ARG(ip) + frame;
					ip += sizeof(LocalOffset);

					Method *const method = *reinterpret_cast<Method**>(ip);
					ip += sizeof(Method*);

					// TODO: Load static function thing
					GC::gc->Alloc(this, VM::vm->types.Method, sizeof(MethodInst), dest);
					dest->common.method->method = method;

					frame->stackCount += opc & 1;
				}
				break;
			case OPI_LDTYPETKN_L:
			case OPI_LDTYPETKN_S:
				// ldtypetkn: LocalOffset dest, Type *type
				{
					Value *const dest = OFF_ARG(ip) + frame;
					ip += sizeof(LocalOffset);

					Type *const type = *reinterpret_cast<Type**>(ip);

					*dest = type->GetTypeToken(this);
						
					ip += sizeof(Type*);
					frame->stackCount += opc & 1;
				}
				break;
			case OPI_CALL_L:
			case OPI_CALL_S:
				// call: LocalOffset args, LocalOffset output, uint16_t argc
				{
					Value *const args   = OFF_ARG(ip) + frame;
					Value *const output = OFF_ARG(ip + sizeof(LocalOffset)) + frame;
					ip += 2 * sizeof(LocalOffset);

					const uint16_t argCount = U16_ARG(ip);

					InvokeLL(argCount, args, output);

					ip += sizeof(uint16_t);
					// InvokeLL pops the arguments
					frame->stackCount += opc & 1;
				}
				break;
			case OPI_SCALL_L:
			case OPI_SCALL_S:
				// scall: LocalOffset args, LocalOffset output, uint16_t argc, Method *method
				{
					Value *const args   = OFF_ARG(ip) + frame;
					Value *const output = OFF_ARG(ip + sizeof(LocalOffset)) + frame;
					ip += 2 * sizeof(LocalOffset);

					const uint16_t argCount = U16_ARG(ip);
					ip += sizeof(uint16_t);

					Method *const method = *reinterpret_cast<Method**>(ip);
					InvokeMethod(method, argCount, args, output);

					ip += sizeof(Method*);
					frame->stackCount += opc & 1;
				}
				break;
			case OPI_APPLY_L:
			case OPI_APPLY_S:
				// apply: LocalOffset args, LocalOffset output
				{
					Value *const args   = OFF_ARG(ip) + frame;
					Value *const output = OFF_ARG(ip + sizeof(LocalOffset)) + frame;
					ip += 2 * sizeof(LocalOffset);

					InvokeApplyLL(args, output);
					frame->stackCount += opc & 1;
				}
				break;
			case OPI_SAPPLY_L:
			case OPI_SAPPLY_S:
				// sapply: LocalOffset args, LocalOffset output, Method *method
				{
					Value *const args   = OFF_ARG(ip) + frame;
					Value *const output = OFF_ARG(ip + sizeof(LocalOffset)) + frame;
					ip += 2 * sizeof(LocalOffset);

					Method *const method = *reinterpret_cast<Method**>(ip);
					InvokeApplyMethodLL(method, args, output);

					ip += sizeof(Method*);
					frame->stackCount += opc & 1;
				}
				break;
			case OPI_BR:
				// br: int32_t offset
				ip += I32_ARG(ip);
				ip += sizeof(int32_t);
				break;
			case OPI_LEAVE:
				// leave: int32_t offset
				{
					const int32_t offset = I32_ARG(ip);
					ip += sizeof(int32_t);
					EvaluateLeave(frame, ip, offset);
					ip += offset;
				}
				break;
			case OPI_BRNULL_L:
			case OPI_BRNULL_S:
				// brnull: LocalOffset value, int32_t offset
				{
					Value *const value = OFF_ARG(ip) + frame;
					ip += sizeof(LocalOffset);
					if (value->type == nullptr)
						ip += I32_ARG(ip);
					ip += sizeof(int32_t);

					frame->stackCount -= opc & 1;
				}
				break;
			case OPI_BRINST_L:
			case OPI_BRINST_S:
				// brinst: LocalOffset value, int32_t offset
				{
					Value *const value = OFF_ARG(ip) + frame;
					ip += sizeof(LocalOffset);
					if (value->type != nullptr)
						ip += I32_ARG(ip);
					ip += sizeof(int32_t);

					frame->stackCount -= opc & 1;
				}
				break;
			case OPI_BRFALSE_L:
			case OPI_BRFALSE_S:
				// brfalse: LocalOffset value, int32_t offset
				{
					Value *const value = OFF_ARG(ip) + frame;
					ip += sizeof(LocalOffset);
					if (IsFalse_(*value))
						ip += I32_ARG(ip);
					ip += sizeof(int32_t);

					frame->stackCount -= opc & 1;
				}
				break;
			case OPI_BRTRUE_L:
			case OPI_BRTRUE_S:
				// brtrue: LocalOffset value, int32_t offset
				{
					Value *const value = OFF_ARG(ip) + frame;
					ip += sizeof(LocalOffset);
					if (IsTrue_(*value))
						ip += I32_ARG(ip);
					ip += sizeof(int32_t);

					frame->stackCount -= opc & 1;
				}
				break;
			case OPI_BRTYPE_L:
			case OPI_BRTYPE_S:
				// brtype: LocalOffset value, Type *type, int32_t offset
				{
					Value *const value = OFF_ARG(ip) + frame;
					ip += sizeof(LocalOffset);

					Type *const type = *reinterpret_cast<Type**>(ip);
					ip += sizeof(Type*);

					if (Type::ValueIsType(*value, type))
						ip += I32_ARG(ip);

					ip += sizeof(int32_t);

					frame->stackCount -= opc & 1;
				}
				break;
			case OPI_SWITCH_L:
			case OPI_SWITCH_S:
				// switch: LocalOffset value, uint16_t count, int32_t offsets[count]
				{
					Value *const value = OFF_ARG(ip) + frame;
					ip += sizeof(LocalOffset);

					if (value->type != VM::vm->types.Int)
						ThrowTypeError();

					uint16_t count = U16_ARG(ip);
					if (value->integer >= 0 || value->integer < count)
						ip += *((int32_t*)ip + (int32_t)value->integer);

					ip += count * sizeof(int32_t);

					frame->stackCount -= opc & 1;
				}
				break;
			case OPI_BRREF:
			case OPI_BRNREF:
				// brnref: LocalOffset (a, b), int32_t offset
				{
					Value *const args = OFF_ARG(ip) + frame;
					ip += sizeof(LocalOffset);

					if (IsSameReference_(args[0], args[1]) ^ (opc & 1))
						ip += I32_ARG(ip);
					ip += sizeof(int32_t);

					frame->stackCount -= 2;
				}
				break;
			case OPI_OPERATOR_L:
			case OPI_OPERATOR_S:
				// operator: LocalOffset args, LocalOffset dest, Operator op
				{
					Value *const args = OFF_ARG(ip) + frame;
					Value *const dest = OFF_ARG(ip + sizeof(LocalOffset)) + frame;
					ip += 2 * sizeof(LocalOffset);

					InvokeOperatorLL(args, *reinterpret_cast<Operator*>(ip), dest);
					ip += sizeof(Operator);

					// InvokeOperatorLL pops arguments off the stack
					frame->stackCount += opc & 1;
				}
				break;
			case OPI_EQ_L:
			case OPI_EQ_S:
				// eq: LocalOffset args, LocalOffset dest
				{
					Value *const args = OFF_ARG(ip) + frame;
					Value *const dest = OFF_ARG(ip + sizeof(LocalOffset)) + frame;
					ip += 2 * sizeof(LocalOffset);

					SetBool_(dest, EqualsLL(args));

					// EqualsLL pops arguments off the stack
					frame->stackCount += opc & 1;
				}
				break;
			case OPI_CMP_L:
			case OPI_CMP_S:
				// cmp: LocalOffset args, LocalOffset dest
				{
					Value *const args = OFF_ARG(ip) + frame;
					Value *const dest = OFF_ARG(ip + sizeof(LocalOffset)) + frame;
					ip += 2 * sizeof(LocalOffset);

					int result = CompareLL(args);
					SetInt_(dest, result);

					// CompareLL pops arguments off the stack
					frame->stackCount += opc & 1;
				}
				break;
			case OPI_LT_L:
			case OPI_LT_S:
				{
					Value *const args = OFF_ARG(ip) + frame;
					Value *const dest = OFF_ARG(ip + sizeof(LocalOffset)) + frame;
					ip += 2 * sizeof(LocalOffset);

					bool result = CompareLL(args) < 0;
					SetBool_(dest, result);

					// CompareLL pops arguments off the stack
					frame->stackCount += opc & 1;
				}
				break;
			case OPI_GT_L:
			case OPI_GT_S:
				{
					Value *const args = OFF_ARG(ip) + frame;
					Value *const dest = OFF_ARG(ip + sizeof(LocalOffset)) + frame;
					ip += 2 * sizeof(LocalOffset);

					bool result = CompareLL(args) > 0;
					SetBool_(dest, result);

					// CompareLL pops arguments off the stack
					frame->stackCount += opc & 1;
				}
				break;
			case OPI_LTE_L:
			case OPI_LTE_S:
				{
					Value *const args = OFF_ARG(ip) + frame;
					Value *const dest = OFF_ARG(ip + sizeof(LocalOffset)) + frame;
					ip += 2 * sizeof(LocalOffset);

					bool result = CompareLL(args) <= 0;
					SetBool_(dest, result);

					// CompareLL pops arguments off the stack
					frame->stackCount += opc & 1;
				}
				break;
			case OPI_GTE_L:
			case OPI_GTE_S:
				{
					Value *const args = OFF_ARG(ip) + frame;
					Value *const dest = OFF_ARG(ip + sizeof(LocalOffset)) + frame;
					ip += 2 * sizeof(LocalOffset);

					bool result = CompareLL(args) >= 0;
					SetBool_(dest, result);

					// CompareLL pops arguments off the stack
					frame->stackCount += opc & 1;
				}
				break;
			case OPI_CONCAT_L:
			case OPI_CONCAT_S:
				// concat: LocalOffset args, LocalOffset dest
				{
					Value *const args = OFF_ARG(ip) + frame;
					Value *const dest = OFF_ARG(ip + sizeof(LocalOffset)) + frame;
					ip += 2 * sizeof(LocalOffset);

					ConcatLL(args, dest);

					// ConcatLL pops arguments off stack
					frame->stackCount += opc & 1;
				}
				break;
			case OPI_CALLMEM_L:
			case OPI_CALLMEM_S:
				// callmem: LocalOffset args, LocalOffset dest, String *member, uint16_t argCount
				{
					Value *const args = OFF_ARG(ip) + frame;
					Value *const dest = OFF_ARG(ip + sizeof(LocalOffset)) + frame;
					ip += 2 * sizeof(LocalOffset);

					String *member = *reinterpret_cast<String**>(ip);
					ip += sizeof(String*);

					uint16_t argCount = U16_ARG(ip);

					InvokeMemberLL(member, argCount, args, dest);

					ip += sizeof(uint16_t);
					frame->stackCount += opc & 1;
				}
				break;
			case OPI_STSFLD_L:
			case OPI_STSFLD_S:
				// stsfld: LocalOffset value, Field *field
				{
					Value *const value = OFF_ARG(ip) + frame;
					Field *const field = *reinterpret_cast<Field**>(ip + sizeof(LocalOffset));

					*field->staticValue = *value;

					ip += sizeof(LocalOffset) + sizeof(Field*);
					frame->stackCount -= opc & 1;
				}
				break;
			case OPI_STFLD:
				// stfld: LocalOffset (instance, value), Field *field
				{
					Value *const values = OFF_ARG(ip) + frame;
					Field *const field = *reinterpret_cast<Field**>(ip + sizeof(LocalOffset));

					*field->GetField(this, values) = values[1];

					ip += sizeof(LocalOffset) + sizeof(Field*);
					frame->stackCount -= 2;
				}
				break;
			case OPI_STMEM:
				// stmem: LocalOffset (instance, value), String *name
				{
					Value *values = OFF_ARG(ip) + frame;
					String *name = *reinterpret_cast<String**>(ip + sizeof(LocalOffset));

					// StoreMemberLL performs a null check
					StoreMemberLL(values, values + 1, name);

					// It also pops the things off the stack
					ip += sizeof(LocalOffset) + sizeof(String*);
				}
				break;
			case OPI_STIDX:
				// stidx: LocalOffset args, uint16_t argCount
				{
					Value *args = OFF_ARG(ip) + frame;
					uint16_t argCount = U16_ARG(ip + sizeof(LocalOffset));

					// StoreIndexerLL performs a null check
					StoreIndexerLL(argCount, args);

					ip += sizeof(LocalOffset) + sizeof(uint16_t);
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
					Value *const inst = OFF_ARG(ip) + frame;
					Value *const dest = OFF_ARG(ip + sizeof(LocalOffset)) + frame;
					ip += 2 * sizeof(LocalOffset);

					Field *const field = *reinterpret_cast<Field**>(ip);

					*dest = *field->GetFieldFast(this, inst);

					ip += sizeof(Field*);
					frame->stackCount += (opc & 1) - 1;
				}
				break;
			case OPI_STFLDFAST:
				// stfldfast: LocalOffset (instance, value), Field *field
				// This is identical to stfld except that it does not perform a type check.
				{
					Value *const values = OFF_ARG(ip) + frame;
					Field *const field = *reinterpret_cast<Field**>(ip + sizeof(LocalOffset));
						
					*field->GetFieldFast(this, values[0]) = values[1];

					ip += sizeof(LocalOffset) + sizeof(Member*);
					frame->stackCount -= 2;
				}
				break;
			}
		}
		catch (OvumException &ex)
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
