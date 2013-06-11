#include "ov_vm.internal.h"

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
				assert(frame->stackCount == 0);
				goto done;
			case OPI_RETNULL:
				assert(frame->stackCount == 0);
				frame->evalStack[0] = NULL_VALUE;
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
				// ldfalse: LocalOffset dest
				{
					SetBool_(OFF_ARG(ip) + frame, false);
					frame->stackCount += opc & 1;

					ip += sizeof(LocalOffset);
				}
				break;
			case OPI_LDTRUE_L:
			case OPI_LDTRUE_S:
				// ldtrue: LocalOffset dest
				{
					SetBool_(OFF_ARG(ip) + frame, true);
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
					frame->stackCount += -argc + (opc & 1);
				}
				break;
			case OPI_LIST_L:
			case OPI_LIST_S:
				// list: LocalOffset dest, int32_t capacity
				{
					Value *const dest = OFF_ARG(ip) + frame;
					int32_t cap = I32_ARG(ip + sizeof(LocalOffset));

					Value result; // Can't put it in dest until it's fully initialized
					GC::gc->Alloc(this, _Tp(stdTypes.List), sizeof(ListInst), &result);
					globalFunctions.initListInstance(this, result.common.list, cap);

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
					GC::gc->Alloc(this, _Tp(stdTypes.Hash), sizeof(HashInst), &result);
					globalFunctions.initHashInstance(this, result.common.hash, cap);

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
				// ldsfld: LocalOffset dest, Value *field
				{
					Value *const dest = OFF_ARG(ip) + frame;
					ip += sizeof(LocalOffset);

					*dest = *VAL_ARG(ip);
					ip += sizeof(Value*);

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
					frame->stackCount += (opc & 1) - 1;
				}
				break;
			case OPI_LDITER_L:
			case OPI_LDITER_S:
				// lditer: LocalOffset instance, LocalOffest dest
				{
					Value *const inst = OFF_ARG(ip) + frame;
					Value *const dest = OFF_ARG(ip + sizeof(LocalOffset)) + frame;

					LoadIteratorLL(inst, dest);

					// LoadIteratorLL pops the instance
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

					*dest = _Tp(inst->type)->GetTypeToken();

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

					*dest = type->GetTypeToken();
						
					ip += sizeof(Type*);
					frame->stackCount += opc & 1;
				}
				break;
			case OPI_CALL_L:
			case OPI_CALL_S:
				{
				}
				break;
			case OPI_SCALL_L:
			case OPI_SCALL_S:
				{
				}
				break;
			case OPI_APPLY_L:
			case OPI_APPLY_S:
				{
				}
				break;
			case OPI_SAPPLY_L:
			case OPI_SAPPLY_S:
				{
				}
				break;
			case OPI_BR:
				// br: int32_t offset
				ip += I32_ARG(ip);
				ip += sizeof(int32_t);
				break;
			case OPI_LEAVE:
				{
					// TODO: OPI_LEAVE
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

					if (value->type != stdTypes.Int)
						ThrowTypeError();

					uint16_t count = U16_ARG(ip);
					if (value->integer >= 0 || value->integer < count)
						ip += I32_ARG(ip + (int32_t)value->integer);

					ip += count * sizeof(int32_t);

					frame->stackCount -= opc & 1;
				}
				break;
			case OPI_BRREF:
				// brref: LocalOffset (a, b), int32_t offset
				{
					Value *const args = OFF_ARG(ip) + frame;
					ip += sizeof(LocalOffset);

					if (IsSameReference_(args[0], args[1]))
						ip += I32_ARG(ip);
					ip += sizeof(int32_t);

					frame->stackCount -= 2;
				}
				break;
			case OPI_BRNREF:
				// brnref: LocalOffset (a, b), int32_t offset
				{
					Value *const args = OFF_ARG(ip) + frame;
					ip += sizeof(LocalOffset);

					if (!IsSameReference_(args[0], args[1]))
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

					SetInt_(dest, CompareLL(args));

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

					SetInt_(dest, CompareLL(args) < 0);

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

					SetInt_(dest, CompareLL(args) > 0);

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

					SetInt_(dest, CompareLL(args) <= 0);

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

					SetInt_(dest, CompareLL(args) >= 0);

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
			case OPI_STSFLD_L:
			case OPI_STSFLD_S:
				// stsfld: LocalOffset value, Member *field
				{
					Value *const value = OFF_ARG(ip) + frame;
					Field *const field = *reinterpret_cast<Field**>(ip + sizeof(LocalOffset));

					*field->staticValue = *value;

					ip += sizeof(LocalOffset) + sizeof(Member*);
					frame->stackCount -= opc & 1;
				}
				break;
			case OPI_STFLD:
				// stfld: LocalOffset (instance, value), Member *field
				{
					Value *const values = OFF_ARG(ip) + frame;
					Field *const field = *reinterpret_cast<Field**>(ip + sizeof(LocalOffset));

					*field->GetField(this, values) = values[1];

					ip += sizeof(LocalOffset) + sizeof(Member*);
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
				{
					;
				}
				break;
			case OPI_THROW:
				Throw(/*rethrow:*/ false);
				break;
			case OPI_RETHROW:
				Throw(/*rethrow:*/ true);
				break;
			case OPI_ENDFINALLY:
				{
				}
				break;
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
			// TODO: Evaluate - catch; find appropriate handlers.

			throw;
		}
	}

	done: assert(frame->stackCount == 1);
	// And then we just fall through and return!
}