#include "ov_vm.internal.h"
#include "ov_thread.opcodes.h"
#include <vector>
#include <memory>

#define I16_ARG(ip)  *reinterpret_cast<int16_t *>(ip)
#define I32_ARG(ip)  *reinterpret_cast<int32_t *>(ip)
#define I64_ARG(ip)  *reinterpret_cast<int64_t *>(ip)
#define U16_ARG(ip)  *reinterpret_cast<uint16_t*>(ip)
#define U32_ARG(ip)  *reinterpret_cast<uint32_t*>(ip)
#define U64_ARG(ip)  *reinterpret_cast<uint64_t*>(ip)

uint8_t *Thread::InitializeMethod(Method::Overload *method)
{
	uint8_t *result;
	if (method->maxStack <= 16)
	{
		StackEntry stack[16];
		result = InitializeMethodInternal(stack, method);
	}
	else
	{
		std::unique_ptr<StackEntry[]> stack(new StackEntry[method->maxStack]);
		result = InitializeMethodInternal(stack.get(), method);
	}
}

uint8_t *Thread::InitializeMethodInternal(StackEntry stack[], Method::Overload *method)
{
	using namespace instr;

	MethodBuilder builder;
	InitializeInstructions(builder, method);
}

void Thread::InitializeInstructions(instr::MethodBuilder &builder, Method::Overload *method)
{
	using namespace instr;

	register uint8_t *ip = method->entry;
	uint8_t *end = method->entry + method->length;

	Module *module = method->group->declModule;

	while (ip < end)
	{
		Opcode *opc = (Opcode*)ip;
		ip++; // Always skip opcode
		Instruction *instr;
		switch (*opc)
		{
		case OPC_NOP:
			instr = new SimpleInstruction(OPI_NOP, StackChange::empty);
			break;
		case OPC_DUP:
			// TODO
			break;
		case OPC_POP:
			// TODO
			break;
		// Arguments
		case OPC_LDARG_0:
		case OPC_LDARG_1:
		case OPC_LDARG_2:
		case OPC_LDARG_3:
			// TODO
			break;
		case OPC_LDARG_S: // ub:n
			// TODO
			break;
		case OPC_LDARG: // u2:n
			// TODO
			break;
		case OPC_STARG_S: // ub:n
			// TODO
			break;
		case OPC_STARG: // u2:n
			// TODO
			break;
		// Locals
		case OPC_LDLOC_0:
		case OPC_LDLOC_1:
		case OPC_LDLOC_2:
		case OPC_LDLOC_3:
			// TODO
			break;
		case OPC_STLOC_0:
		case OPC_STLOC_1:
		case OPC_STLOC_2:
		case OPC_STLOC_3:
			// TODO
			break;
		case OPC_LDLOC_S: // ub:n
			// TODO
			break;
		case OPC_LDLOC: // u2:n
			// TODO
			break;
		case OPC_STLOC_S: // ub:n
			// TODO
			break;
		case OPC_STLOC: // u2:n
			// TODO
			break;
		// Values and object initialisation
		case OPC_LDNULL:
			instr = new LoadNull();
			break;
		case OPC_LDFALSE:
			instr = new LoadBoolean(false);
			break;
		case OPC_LDTRUE:
			instr = new LoadBoolean(true);
			break;
		case OPC_LDC_I_M1:
		case OPC_LDC_I_0:
		case OPC_LDC_I_1:
		case OPC_LDC_I_2:
		case OPC_LDC_I_3:
		case OPC_LDC_I_4:
		case OPC_LDC_I_5:
		case OPC_LDC_I_6:
		case OPC_LDC_I_7:
		case OPC_LDC_I_8:
			instr = new LoadInt((int)*opc - OPC_LDC_I_0);
			break;
		case OPC_LDC_I_S: // sb:value
			instr = new LoadInt(*(int8_t*)ip);
			ip++;
			break;
		case OPC_LDC_I_M: // i4:value
			instr = new LoadInt(I32_ARG(ip));
			ip += sizeof(int32_t);
			break;
		case OPC_LDC_I: // i8:value
			instr = new LoadInt(I64_ARG(ip));
			ip += sizeof(int64_t);
			break;
		case OPC_LDC_U: // u8:value
			instr = new LoadUInt(U64_ARG(ip));
			ip += sizeof(uint64_t);
			break;
		case OPC_LDC_R: // r8:value
			instr = new LoadReal(*(double*)ip);
			ip += sizeof(double);
			break;
		case OPC_LDSTR: // u4:str
			{
				TokenId stringId = U32_ARG(ip);
				ip += sizeof(uint32_t);

				instr = new LoadString(module->FindString(stringId));
			}
			break;
		case OPC_LDARGC:
			instr = new LoadArgCount();
			break;
		case OPC_LDENUM_S: // u4:type  i4:value
			{
				TokenId typeId = U32_ARG(ip);
				ip += sizeof(uint32_t);
				int32_t value = I32_ARG(ip);
				ip += sizeof(int32_t);
				instr = new LoadEnumValue(module->FindType(typeId), value);
			}
			break;
		case OPC_LDENUM: // u4:type  i8:value
			{
				TokenId typeId = U32_ARG(ip);
				ip += sizeof(uint32_t);
				int64_t value = I64_ARG(ip);
				ip += sizeof(int64_t);
				instr = new LoadEnumValue(module->FindType(typeId), value);
			}
			break;
		case OPC_NEWOBJ_S: // u4:type, ub:argc
			{
				TokenId typeId = U32_ARG(ip);
				ip += sizeof(uint32_t);
				uint16_t argCount = *ip;
				ip++;
				instr = new NewObject(module->FindType(typeId), argCount);
			}
			break;
		case OPC_NEWOBJ: // u4:type, u2:argc
			{
				TokenId typeId = U32_ARG(ip);
				ip += sizeof(uint32_t);
				uint16_t argCount = U16_ARG(ip);
				ip += sizeof(uint16_t);
				instr = new NewObject(module->FindType(typeId), argCount);
			}
			break;
		// Invocation
		case OPC_CALL_0:
		case OPC_CALL_1:
		case OPC_CALL_2:
		case OPC_CALL_3:
			instr = new Call(*opc - OPC_CALL_0);
			break;
		case OPC_CALL_S: // ub:argc
			instr = new Call(*ip++);
			break;
		case OPC_CALL: // u2:argc
			{
				uint16_t argCount = U16_ARG(ip);
				ip += sizeof(uint16_t);
				instr = new Call(argCount);
			}
			break;
		case OPC_SCALL_S: // u4:func  ub:argc
			{
				TokenId funcId = U32_ARG(ip);
				ip += sizeof(uint32_t);
				instr = new StaticCall(*ip++, module->FindMethod(funcId));
			}
			break;
		case OPC_SCALL: // u4:func  u2:argc
			{
				TokenId funcId = U32_ARG(ip);
				ip += sizeof(uint32_t);
				uint16_t argCount = U16_ARG(ip);
				ip += sizeof(uint16_t);
				instr = new StaticCall(argCount, module->FindMethod(funcId));
			}
			break;
		case OPC_APPLY:
			instr = new Apply();
			break;
		case OPC_SAPPLY: // u4:func
			{
				TokenId funcId = U32_ARG(ip);
				ip += sizeof(uint32_t);
				instr = new StaticApply(module->FindMethod(funcId));
			}
			break;
		// Control flow
		case OPC_RETNULL:
			instr = new SimpleInstruction(OPI_RETNULL, StackChange::empty);
			break;
		case OPC_RET:
			instr = new SimpleInstruction(OPI_RET, StackChange(1, 0));
			break;
		case OPC_BR_S: // sb:trg
			instr = new Branch(*(int8_t*)ip++, /*isLeave:*/ false);
			break;
		case OPC_BRNULL_S: // sb:trg
			instr = new ConditionalBranch(*(int8_t*)ip++, ConditionalBranch::IF_NULL);
			break;
		case OPC_BRINST_S: // sb:trg
			instr = new ConditionalBranch(*(int8_t*)ip++, ConditionalBranch::NOT_NULL);
			break;
		case OPC_BRFALSE_S: // sb:trg
			instr = new ConditionalBranch(*(int8_t*)ip++, ConditionalBranch::IF_FALSE);
			break;
		case OPC_BRTRUE_S: // sb:trg
			instr = new ConditionalBranch(*(int8_t*)ip++, ConditionalBranch::IF_TRUE);
			break;
		case OPC_BRREF_S: // sb:trg		(even)
		case OPC_BRNREF_S: // sb:trg	(odd)
			instr = new BranchIfReference(*(int8_t*)ip++, /*branchIfSame:*/ (*opc & 1) == 0);
			break;
		case OPC_BRTYPE_S: // u4:type  sb:trg
			{
				TokenId typeId = U32_ARG(ip);
				ip += sizeof(uint32_t);
				instr = new BranchIfType(*(int8_t*)ip++, module->FindType(typeId));
			}
			break;
		case OPC_BR: // i4:trg
			instr = new Branch(*(int8_t*)ip++, /*isLeave:*/ false);
			break;
		case OPC_BRNULL: // i4:trg
			{
				int32_t target = I32_ARG(ip);
				ip += sizeof(int32_t);
				instr = new ConditionalBranch(target, ConditionalBranch::IF_NULL);
			}
			break;
		case OPC_BRINST: // i4:trg
			{
				int32_t target = I32_ARG(ip);
				ip += sizeof(int32_t);
				instr = new ConditionalBranch(target, ConditionalBranch::NOT_NULL);
			}
			break;
		case OPC_BRFALSE: // i4:trg
			{
				int32_t target = I32_ARG(ip);
				ip += sizeof(int32_t);
				instr = new ConditionalBranch(target, ConditionalBranch::IF_FALSE);
			}
			break;
		case OPC_BRTRUE: // i4:trg
			{
				int32_t target = I32_ARG(ip);
				ip += sizeof(int32_t);
				instr = new ConditionalBranch(target, ConditionalBranch::IF_TRUE);
			}
			break;
		case OPC_BRREF: // i4:trg		(even)
		case OPC_BRNREF: // i4:trg		(odd)
			{
				int32_t target = I32_ARG(ip);
				ip += sizeof(int32_t);
				instr = new BranchIfReference(target, /*branchIfSame:*/ (*opc & 1) == 0);
			}
			break;
		case OPC_BRTYPE: // u4:type  i4:trg
			{
				TokenId typeId = U32_ARG(ip);
				ip += sizeof(uint32_t);
				int32_t target = I32_ARG(ip);
				ip += sizeof(int32_t);
				instr = new BranchIfType(target, module->FindType(typeId));
			}
			break;
		case OPC_SWITCH_S: // u2:n  sb:targets...
			{
				uint16_t count = U16_ARG(ip);
				ip += sizeof(uint16_t);
				int32_t *targets = new int32_t[count];
				for (int i = 0; i < count; i++)
					targets[i] = *(int8_t*)ip++;
				instr = new Switch(count, targets);
			}
			break;
		case OPC_SWITCH: // u2:n  i4:targets...
			{
				uint16_t count = U16_ARG(ip);
				ip += sizeof(uint16_t);
				int32_t *targets = new int32_t[count];
				CopyMemoryT(targets, (int32_t*)ip, count);
				instr = new Switch(count, targets);
			}
			break;
		// Operators
		case OPC_ADD:
		case OPC_SUB:
		case OPC_OR:
		case OPC_XOR:
		case OPC_MUL:
		case OPC_DIV:
		case OPC_MOD:
		case OPC_AND:
		case OPC_POW:
		case OPC_SHL:
		case OPC_SHR:
		case OPC_HASHOP:
		case OPC_DOLLAR:
		case OPC_PLUS:
		case OPC_NEG:
		case OPC_NOT:
		case OPC_EQ:
		case OPC_CMP:
			instr = new ExecOperator((Operator)(*opc - OPC_ADD));
			break;
		case OPC_LT:
			instr = new ExecOperator(ExecOperator::CMP_LT);
			break;
		case OPC_GT:
			instr = new ExecOperator(ExecOperator::CMP_GT);
			break;
		case OPC_LTE:
			instr = new ExecOperator(ExecOperator::CMP_LTE);
			break;
		case OPC_GTE:
			instr = new ExecOperator(ExecOperator::CMP_GTE);
			break;
		case OPC_CONCAT:
			instr = new ExecOperator(ExecOperator::CONCAT);
			break;
		// Misc. data
		case OPC_LIST_0:
			instr = new CreateList(0);
			break;
		case OPC_LIST_S: // ub:count
			instr = new CreateList(*ip);
			ip++;
			break;
		case OPC_LIST: // u4:count
			instr = new CreateList(U32_ARG(ip));
			ip += sizeof(uint32_t);
			break;
		case OPC_HASH_0:
			instr = new CreateHash(0);
			break;
		case OPC_HASH_S: // ub:count
			instr = new CreateHash(*ip);
			ip++;
			break;
		case OPC_HASH: // u4:count
			instr = new CreateHash(U32_ARG(ip));
			ip += sizeof(uint32_t);
			break;
		case OPC_LDITER:
			instr = new LoadIterator();
			break;
		case OPC_LDTYPE:
			instr = new LoadType();
			break;
		// Fields
		case OPC_LDFLD: // u4:fld
			{
				TokenId fieldId = U32_ARG(ip);
				ip += sizeof(uint32_t);
				instr = new LoadField(module->FindField(fieldId));
			}
			break;
		case OPC_STFLD: // u4:fld
			{
				TokenId fieldId = U32_ARG(ip);
				ip += sizeof(uint32_t);
				instr = new StoreField(module->FindField(fieldId));
			}
			break;
		case OPC_LDSFLD: // u4:fld
			{
				TokenId fieldId = U32_ARG(ip);
				ip += sizeof(uint32_t);
				instr = new instr::LoadStaticField(module->FindField(fieldId));
			}
			break;
		case OPC_STSFLD: // u4:fld
			{
				TokenId fieldId = U32_ARG(ip);
				ip += sizeof(uint32_t);
				instr = new instr::StoreStaticField(module->FindField(fieldId));
			}
			break;
		// Named member access
		case OPC_LDMEM: // u4:name
			{
				TokenId nameId = U32_ARG(ip);
				ip += sizeof(uint32_t);
				instr = new instr::LoadMember(module->FindString(nameId));
			}
			break;
		case OPC_STMEM: // u4:name
			{
				TokenId nameId = U32_ARG(ip);
				ip += sizeof(uint32_t);
				instr = new instr::StoreMember(module->FindString(nameId));
			}
			break;
		// Indexers
		case OPC_LDIDX_1:
			instr = new instr::LoadIndexer(1);
			break;
		case OPC_LDIDX_S: // ub:argc
			instr = new instr::LoadIndexer(*ip);
			ip++;
			break;
		case OPC_LDIDX: // u2:argc
			instr = new instr::LoadIndexer(U16_ARG(ip));
			ip += sizeof(uint16_t);
			break;
		case OPC_STIDX_1:
			instr = new instr::StoreIndexer(1);
			break;
		case OPC_STIDX_S: // ub:argc
			instr = new instr::StoreIndexer(*ip);
			ip++;
			break;
		case OPC_STIDX: // u2:argc
			instr = new instr::StoreIndexer(U16_ARG(ip));
			ip += sizeof(uint16_t);
			break;
		// Global/static functions
		case OPC_LDSFN: // u4:func
			{
				TokenId funcId = U32_ARG(ip);
				ip += sizeof(uint32_t);
				instr = new LoadStaticFunction(module->FindMethod(funcId));
			}
			break;
		// Type tokens
		case OPC_LDTYPETKN: // u4:type
			{
				TokenId typeId = U32_ARG(ip);
				ip += sizeof(uint32_t);
				instr = new LoadTypeToken(module->FindType(typeId));
			}
			break;
		// Exception handling
		case OPC_THROW:
			instr = new SimpleInstruction(OPI_THROW, StackChange(1, 0));
			break;
		case OPC_RETHROW:
			instr = new SimpleInstruction(OPI_RETHROW, StackChange::empty);
			break;
		case OPC_LEAVE_S: // sb:trg
			instr = new Branch(*(int8_t*)ip, true);
			ip++;
			break;
		case OPC_LEAVE: // i4:trg
			{
				int32_t target = I32_ARG(ip);
				ip += sizeof(int32_t);
				instr = new Branch(target, true);
			}
			break;
		case OPC_ENDFINALLY:
			instr = new SimpleInstruction(OPI_ENDFINALLY, StackChange::empty);
			break;
		// Call member
		case OPC_CALLMEM_S: // u4:name  ub:argc
			{
				TokenId nameId = U32_ARG(ip);
				ip += sizeof(uint32_t);
				instr = new CallMember(module->FindString(nameId), *ip);
				ip++;
			}
			break;
		case OPC_CALLMEM: // u4:name  u2:argc
			{
				TokenId nameId = U32_ARG(ip);
				ip += sizeof(uint32_t);
				instr = new CallMember(module->FindString(nameId), U16_ARG(ip));
				ip += sizeof(uint16_t);
			}
			break;
		}
		builder.Append((uint32_t)((char*)opc - (char*)method->entry), instr);
	}
}