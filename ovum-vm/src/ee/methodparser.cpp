#include "methodparser.h"
#include "methodinitexception.h"
#include "../debug/debugsymbols.h"
#include "../module/module.h"
#include "../object/member.h"
#include "../object/type.h"
#include "../object/method.h"
#include "../object/field.h"

namespace ovum
{

namespace instr
{
	void MethodParser::ParseInto(MethodOverload *method, MethodBuilder &builder)
	{
		MethodParser parser(method);

		// First we must populate the builder with instructions.
		while (!parser.IsAtEnd())
			parser.ParseInstruction(builder);

		// Then we transform offsets stored in the method (such as jump target,
		// try block locations, debug symbol offsets) into instruction indexes,
		// so we don't have to look up the original offset constantly.
		parser.InitOffsets(builder);
	}

	MethodParser::MethodParser(MethodOverload *method) :
		method(method),
		ip(reinterpret_cast<uintptr_t>(method->entry)),
		methodBodyStart(reinterpret_cast<uintptr_t>(method->entry)),
		methodBodyEnd(reinterpret_cast<uintptr_t>(method->entry + method->length)),
		methodRefSignature(method->refSignature, method->GetRefSignaturePool()),
		argRefOffset(method->group->IsStatic() ? 1 : 0),
		module(method->group->declModule)
	{ }

	void MethodParser::ParseInstruction(MethodBuilder &builder)
	{
		uintptr_t startOffset = ip;

		Opcode opc = Read<Opcode>();
		Box<Instruction> instruction = ParseInstructionArguments(opc, builder);

		uint32_t originalOffset = static_cast<uint32_t>(startOffset - methodBodyStart);
		size_t originalSize = static_cast<size_t>(ip - startOffset);
		builder.Append(
			originalOffset,
			originalSize,
			instruction.release()
		);
	}

	Box<Instruction> MethodParser::ParseInstructionArguments(Opcode opc, MethodBuilder &builder)
	{
		Box<Instruction> result;

		switch (opc)
		{
		case OPC_NOP:
			result.reset(new SimpleInstruction(OPI_NOP, StackChange::empty));
			break;
		case OPC_DUP:
			result.reset(new DupInstr());
			break;
		case OPC_POP:
			result.reset(new SimpleInstruction(OPI_POP, StackChange(1, 0)));
			break;
			// Arguments
		case OPC_LDARG_0:
		case OPC_LDARG_1:
		case OPC_LDARG_2:
		case OPC_LDARG_3:
			{
				ovlocals_t arg = opc - OPC_LDARG_0;
				result.reset(new LoadLocal(
					method->GetArgumentOffset(arg),
					methodRefSignature.IsParamRef(arg + argRefOffset)
				));
			}
			break;
		case OPC_LDARG_S: // ub:n
			{
				ovlocals_t arg = Read<uint8_t>();
				result.reset(new LoadLocal(
					method->GetArgumentOffset(arg),
					methodRefSignature.IsParamRef(arg + argRefOffset)
				));
			}
			break;
		case OPC_LDARG: // u2:n
			{
				ovlocals_t arg = Read<uint16_t>();
				result.reset(new LoadLocal(
					method->GetArgumentOffset(arg),
					methodRefSignature.IsParamRef(arg + argRefOffset)
				));
			}
			break;
		case OPC_STARG_S: // ub:n
			{
				ovlocals_t arg = Read<uint8_t>();
				result.reset(new StoreLocal(
					method->GetArgumentOffset(arg),
					methodRefSignature.IsParamRef(arg + argRefOffset)
				));
			}
			break;
		case OPC_STARG: // u2:n
			{
				ovlocals_t arg = Read<uint16_t>();
				result.reset(new StoreLocal(
					method->GetArgumentOffset(arg),
					methodRefSignature.IsParamRef(arg + argRefOffset)
				));
			}
			break;
		// Locals
		case OPC_LDLOC_0:
		case OPC_LDLOC_1:
		case OPC_LDLOC_2:
		case OPC_LDLOC_3:
			result.reset(new LoadLocal(
				method->GetLocalOffset(opc - OPC_LDLOC_0),
				false
			));
			break;
		case OPC_STLOC_0:
		case OPC_STLOC_1:
		case OPC_STLOC_2:
		case OPC_STLOC_3:
			result.reset(new StoreLocal(
				method->GetLocalOffset(opc - OPC_STLOC_0),
				false
			));
			break;
		case OPC_LDLOC_S: // ub:n
			{
				ovlocals_t local = Read<uint8_t>();
				result.reset(new LoadLocal(method->GetLocalOffset(local), false));
			}
			break;
		case OPC_LDLOC: // u2:n
			{
				ovlocals_t local = Read<uint16_t>();
				result.reset(new LoadLocal(method->GetLocalOffset(local), false));
			}
			break;
		case OPC_STLOC_S: // ub:n
			{
				ovlocals_t local = Read<uint8_t>();
				result.reset(new StoreLocal(method->GetLocalOffset(local), false));
			}
			break;
		case OPC_STLOC: // u2:n
			{
				ovlocals_t local = Read<uint16_t>();
				result.reset(new StoreLocal(method->GetLocalOffset(local), false));
			}
			break;
		// Values and object initialisation
		case OPC_LDNULL:
			result.reset(new LoadNull());
			break;
		case OPC_LDFALSE:
			result.reset(new LoadBoolean(false));
			break;
		case OPC_LDTRUE:
			result.reset(new LoadBoolean(true));
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
			// Note: we need to cast opc to something signed to ensure -1 doesn't
			// cause an overflow and all sorts of weirdness.
			result.reset(new LoadInt((int)opc - OPC_LDC_I_0));
			break;
		case OPC_LDC_I_S: // sb:value
			{
				int8_t value = Read<int8_t>();
				result.reset(new LoadInt(value));
			}
			break;
		case OPC_LDC_I_M: // i4:value
			{
				int32_t value = Read<int32_t>();
				result.reset(new LoadInt(value));
			}
			break;
		case OPC_LDC_I: // i8:value
			{
				int64_t value = Read<int64_t>();
				result.reset(new LoadInt(value));
			}
			break;
		case OPC_LDC_U: // u8:value
			{
				uint64_t value = Read<uint64_t>();
				result.reset(new LoadUInt(value));
			}
			break;
		case OPC_LDC_R: // r8:value
			{
				double value = Read<double>();
				result.reset(new LoadReal(value));
			}
			break;
		case OPC_LDSTR: // u4:str
			{
				String *str = StringFromToken(Read<uint32_t>());
				result.reset(new LoadString(str));
			}
			break;
		case OPC_LDARGC:
			result.reset(new LoadArgCount());
			break;
		case OPC_LDENUM_S: // u4:type  i4:value
			{
				Type *type = TypeFromToken(Read<uint32_t>());
				int32_t value = Read<int32_t>();
				result.reset(new LoadEnumValue(type, value));
			}
			break;
		case OPC_LDENUM: // u4:type  i8:value
			{
				Type *type = TypeFromToken(Read<uint32_t>());
				int64_t value = Read<int64_t>();
				result.reset(new LoadEnumValue(type, value));
			}
			break;
		case OPC_NEWOBJ_S: // u4:type, ub:argc
			{
				Type *type = TypeFromToken(Read<uint32_t>());
				ovlocals_t argCount = Read<uint8_t>();
				EnsureConstructible(type, argCount);
				result.reset(new NewObject(type, argCount));
			}
			break;
		case OPC_NEWOBJ: // u4:type, u2:argc
			{
				Type *type = TypeFromToken(Read<uint32_t>());
				ovlocals_t argCount = Read<uint16_t>();
				EnsureConstructible(type, argCount);
				result.reset(new NewObject(type, argCount));
			}
			break;
		// Invocation
		case OPC_CALL_0:
		case OPC_CALL_1:
		case OPC_CALL_2:
		case OPC_CALL_3:
			result.reset(new Call(opc - OPC_CALL_0));
			break;
		case OPC_CALL_S: // ub:argc
			{
				ovlocals_t argCount = Read<uint8_t>();
				result.reset(new Call(argCount));
			}
			break;
		case OPC_CALL: // u2:argc
			{
				ovlocals_t argCount = Read<uint16_t>();
				result.reset(new Call(argCount));
			}
			break;
		case OPC_SCALL_S: // u4:func  ub:argc
			{
				uint32_t funcToken = Read<uint32_t>();
				ovlocals_t argCount = Read<uint8_t>();
				MethodOverload *mo = MethodOverloadFromToken(funcToken, argCount);
				result.reset(new StaticCall(argCount - mo->InstanceOffset(), mo));
			}
			break;
		case OPC_SCALL: // u4:func  u2:argc
			{
				uint32_t funcToken = Read<uint32_t>();
				ovlocals_t argCount = Read<uint16_t>();
				MethodOverload *mo = MethodOverloadFromToken(funcToken, argCount);
				result.reset(new StaticCall(argCount - mo->InstanceOffset(), mo));
			}
			break;
		case OPC_APPLY:
			result.reset(new Apply());
			break;
		case OPC_SAPPLY: // u4:func
			{
				Method *func = MethodFromToken(Read<uint32_t>());
				result.reset(new StaticApply(func));
			}
			break;
		// Control flow
		case OPC_RETNULL:
			result.reset(new SimpleInstruction(OPI_RETNULL, StackChange::empty));
			break;
		case OPC_RET:
			result.reset(new SimpleInstruction(OPI_RET, StackChange(1, 0)));
			break;
		case OPC_BR_S: // sb:trg
			{
				JumpTarget target = JumpTarget::FromOffset(Read<int8_t>());
				result.reset(new Branch(target, /*isLeave:*/ false));
			}
			break;
		case OPC_BRNULL_S: // sb:trg
			{
				JumpTarget target = JumpTarget::FromOffset(Read<int8_t>());
				result.reset(new ConditionalBranch(target, ConditionalBranch::IF_NULL));
			}
			break;
		case OPC_BRINST_S: // sb:trg
			{
				JumpTarget target = JumpTarget::FromOffset(Read<int8_t>());
				result.reset(new ConditionalBranch(target, ConditionalBranch::NOT_NULL));
			}
			break;
		case OPC_BRFALSE_S: // sb:trg
			{
				JumpTarget target = JumpTarget::FromOffset(Read<int8_t>());
				result.reset(new ConditionalBranch(target, ConditionalBranch::IF_FALSE));
			}
			break;
		case OPC_BRTRUE_S: // sb:trg
			{
				JumpTarget target = JumpTarget::FromOffset(Read<int8_t>());
				result.reset(new ConditionalBranch(target, ConditionalBranch::IF_TRUE));
			}
			break;
		case OPC_BRREF_S:  // sb:trg	(even)
		case OPC_BRNREF_S: // sb:trg	(odd)
			{
				JumpTarget target = JumpTarget::FromOffset(Read<int8_t>());
				result.reset(new BranchIfReference(target, /*branchIfSame:*/ (opc & 1) == 0));
			}
			break;
		case OPC_BRTYPE_S: // u4:type  sb:trg
			{
				Type *type = TypeFromToken(Read<uint32_t>());
				JumpTarget target = JumpTarget::FromOffset(Read<int8_t>());
				result.reset(new BranchIfType(target, type));
			}
			break;
		case OPC_BR: // i4:trg
			{
				JumpTarget target = JumpTarget::FromOffset(Read<int32_t>());
				result.reset(new Branch(target, /*isLeave:*/ false));
			}
			break;
		case OPC_BRNULL: // i4:trg
			{
				JumpTarget target = JumpTarget::FromOffset(Read<int32_t>());
				result.reset(new ConditionalBranch(target, ConditionalBranch::IF_NULL));
			}
			break;
		case OPC_BRINST: // i4:trg
			{
				JumpTarget target = JumpTarget::FromOffset(Read<int32_t>());
				result.reset(new ConditionalBranch(target, ConditionalBranch::NOT_NULL));
			}
			break;
		case OPC_BRFALSE: // i4:trg
			{
				JumpTarget target = JumpTarget::FromOffset(Read<int32_t>());
				result.reset(new ConditionalBranch(target, ConditionalBranch::IF_FALSE));
			}
			break;
		case OPC_BRTRUE: // i4:trg
			{
				JumpTarget target = JumpTarget::FromOffset(Read<int32_t>());
				result.reset(new ConditionalBranch(target, ConditionalBranch::IF_TRUE));
			}
			break;
		case OPC_BRREF: // i4:trg		(even)
		case OPC_BRNREF: // i4:trg		(odd)
			{
				JumpTarget target = JumpTarget::FromOffset(Read<int32_t>());
				result.reset(new BranchIfReference(target, /*branchIfSame:*/ (opc & 1) == 0));
			}
			break;
		case OPC_BRTYPE: // u4:type  i4:trg
			{
				Type *type = TypeFromToken(Read<uint32_t>());
				JumpTarget target = JumpTarget::FromOffset(Read<int32_t>());
				result.reset(new BranchIfType(target, type));
			}
			break;
		case OPC_SWITCH_S: // u2:n  sb:targets...
			{
				size_t count = Read<uint16_t>();
				Box<JumpTarget[]> targets(new JumpTarget[count]);
				for (size_t i = 0; i < count; i++)
					targets[i] = JumpTarget::FromOffset(Read<int8_t>());
				result.reset(new Switch(count, std::move(targets)));
			}
			break;
		case OPC_SWITCH: // u2:n  i4:targets...
			{
				size_t count = Read<uint16_t>();
				Box<JumpTarget[]> targets(new JumpTarget[count]);
				for (size_t i = 0; i < count; i++)
					targets[i] = JumpTarget::FromOffset(Read<int32_t>());
				result.reset(new Switch(count, std::move(targets)));
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
		case OPC_PLUS:
		case OPC_NEG:
		case OPC_NOT:
			result.reset(new ExecOperator((Operator)(opc - OPC_ADD)));
			break;
		case OPC_EQ:
			result.reset(new ExecOperator(ExecOperator::EQ));
			break;
		case OPC_CMP:
			result.reset(new ExecOperator(ExecOperator::CMP));
			break;
		case OPC_LT:
			result.reset(new ExecOperator(ExecOperator::LT));
			break;
		case OPC_GT:
			result.reset(new ExecOperator(ExecOperator::GT));
			break;
		case OPC_LTE:
			result.reset(new ExecOperator(ExecOperator::LTE));
			break;
		case OPC_GTE:
			result.reset(new ExecOperator(ExecOperator::GTE));
			break;
		case OPC_CONCAT:
			result.reset(new ExecOperator(ExecOperator::CONCAT));
			break;
			// Misc. data
		case OPC_LIST_0:
			result.reset(new CreateList(0));
			break;
		case OPC_LIST_S: // ub:count
			result.reset(new CreateList(Read<uint8_t>()));
			break;
		case OPC_LIST: // u4:count
			result.reset(new CreateList(Read<uint32_t>()));
			break;
		case OPC_HASH_0:
			result.reset(new CreateHash(0));
			break;
		case OPC_HASH_S: // ub:count
			result.reset(new CreateHash(Read<uint8_t>()));
			break;
		case OPC_HASH: // u4:count
			result.reset(new CreateHash(Read<uint32_t>()));
			break;
		case OPC_LDITER:
			result.reset(new LoadIterator());
			break;
		case OPC_LDTYPE:
			result.reset(new LoadType());
			break;
			// Fields
		case OPC_LDFLD: // u4:fld
			{
				Field *field = FieldFromToken(Read<uint32_t>(), false);
				result.reset(new instr::LoadField(field));
			}
			break;
		case OPC_STFLD: // u4:fld
			{
				Field *field = FieldFromToken(Read<uint32_t>(), false);
				result.reset(new instr::StoreField(field));
			}
			break;
		case OPC_LDSFLD: // u4:fld
			{
				Field *field = FieldFromToken(Read<uint32_t>(), true);
				result.reset(new instr::LoadStaticField(field));
				builder.AddTypeToInitialize(field->declType);
			}
			break;
		case OPC_STSFLD: // u4:fld
			{
				Field *field = FieldFromToken(Read<uint32_t>(), true);
				result.reset(new instr::StoreStaticField(field));
				builder.AddTypeToInitialize(field->declType);
			}
			break;
		// Named member access
		case OPC_LDMEM: // u4:name
			{
				String *name = StringFromToken(Read<uint32_t>());
				result.reset(new instr::LoadMember(name));
			}
			break;
		case OPC_STMEM: // u4:name
			{
				String *name = StringFromToken(Read<uint32_t>());
				result.reset(new instr::StoreMember(name));
			}
			break;
		// Indexers
		case OPC_LDIDX_1:
			result.reset(new instr::LoadIndexer(1));
			break;
		case OPC_LDIDX_S: // ub:argc
			result.reset(new instr::LoadIndexer(Read<uint8_t>()));
			break;
		case OPC_LDIDX: // u2:argc
			result.reset(new instr::LoadIndexer(Read<uint16_t>()));
			break;
		case OPC_STIDX_1:
			result.reset(new instr::StoreIndexer(1));
			break;
		case OPC_STIDX_S: // ub:argc
			result.reset(new instr::StoreIndexer(Read<uint8_t>()));
			break;
		case OPC_STIDX: // u2:argc
			result.reset(new instr::StoreIndexer(Read<uint16_t>()));
			break;
		// Global/static functions
		case OPC_LDSFN: // u4:func
			{
				Method *func = MethodFromToken(Read<uint32_t>());
				result.reset(new LoadStaticFunction(func));
			}
			break;
		// Type tokens
		case OPC_LDTYPETKN: // u4:type
			{
				Type *type = TypeFromToken(Read<uint32_t>());
				result.reset(new LoadTypeToken(type));
			}
			break;
		// Exception handling
		case OPC_THROW:
			result.reset(new SimpleInstruction(OPI_THROW, StackChange(1, 0)));
			break;
		case OPC_RETHROW:
			result.reset(new SimpleInstruction(OPI_RETHROW, StackChange::empty));
			break;
		case OPC_LEAVE_S: // sb:trg
			{
				JumpTarget target = JumpTarget::FromOffset(Read<int8_t>());
				result.reset(new Branch(target, true));
			}
			break;
		case OPC_LEAVE: // i4:trg
			{
				JumpTarget target = JumpTarget::FromOffset(Read<int32_t>());
				result.reset(new Branch(target, true));
			}
			break;
		case OPC_ENDFINALLY:
			result.reset(new SimpleInstruction(OPI_ENDFINALLY, StackChange::empty));
			break;
			// Call member
		case OPC_CALLMEM_S: // u4:name  ub:argc
			{
				String *name = StringFromToken(Read<uint32_t>());
				ovlocals_t argCount = Read<uint8_t>();
				result.reset(new CallMember(name, argCount));
			}
			break;
		case OPC_CALLMEM: // u4:name  u2:argc
			{
				String *name = StringFromToken(Read<uint32_t>());
				ovlocals_t argCount = Read<uint16_t>();
				result.reset(new CallMember(name, argCount));
			}
			break;
		// References
		case OPC_LDMEMREF: // u4:name
			{
				String *name = StringFromToken(Read<uint32_t>());
				result.reset(new LoadMemberRef(name));
			}
			break;
		case OPC_LDARGREF_S: // ub:n
			{
				ovlocals_t arg = Read<uint8_t>();
				if (methodRefSignature.IsParamRef(arg + argRefOffset))
				{
					result.reset(new LoadLocal(method->GetArgumentOffset(arg), false));
					result->flags |= InstrFlags::PUSHES_REF;
				}
				else
				{
					result.reset(new LoadLocalRef(method->GetArgumentOffset(arg)));
				}
			}
			break;
		case OPC_LDARGREF: // u2:n
			{
				ovlocals_t arg = Read<uint16_t>();
				if (methodRefSignature.IsParamRef(arg + argRefOffset))
				{
					result.reset(new LoadLocal(method->GetArgumentOffset(arg), false));
					result->flags |= InstrFlags::PUSHES_REF;
				}
				else
				{
					result.reset(new LoadLocalRef(method->GetArgumentOffset(arg)));
				}
			}
			break;
		case OPC_LDLOCREF_S: // ub:n
			{
				ovlocals_t loc = Read<uint8_t>();
				result.reset(new LoadLocalRef(method->GetLocalOffset(loc)));
			}
			break;
		case OPC_LDLOCREF: // u2:n
			{
				ovlocals_t loc = Read<uint16_t>();
				result.reset(new LoadLocalRef(method->GetLocalOffset(loc)));
			}
			break;
		case OPC_LDFLDREF: // u4:field
			{
				Field *field = FieldFromToken(Read<uint32_t>(), false);
				result.reset(new LoadFieldRef(field));
			}
			break;
		case OPC_LDSFLDREF: // u4:field
			{
				Field *field = FieldFromToken(Read<uint32_t>(), true);
				result.reset(new LoadStaticFieldRef(field));
				builder.AddTypeToInitialize(field->declType);
			}
			break;
		default:
			throw MethodInitException::General("Invalid opcode encountered.", method);
		}

		return std::move(result);
	}

	void MethodParser::InitOffsets(MethodBuilder &builder)
	{
		if (builder.HasBranches())
			InitBranchOffsets(builder);

		if (method->tryBlockCount > 0)
			InitTryBlockOffsets(builder);

		if (method->debugSymbols)
			InitDebugSymbolOffsets(builder);
	}

	void MethodParser::InitBranchOffsets(MethodBuilder &builder)
	{
		for (size_t i = 0; i < builder.GetLength(); i++)
		{
			Instruction *instruction = builder[i];
			if (instruction->IsBranch())
			{
				Branch *br = static_cast<Branch*>(instruction);
				br->target = JumpTarget::FromIndex(
					builder.FindIndex(
						builder.GetOriginalOffset(i) +
						builder.GetOriginalSize(i) +
						br->target.offset
					)
				);
				builder[br->target.index]->AddIncomingBranch();
			}
			else if (instruction->IsSwitch())
			{
				Switch *sw = static_cast<Switch*>(instruction);
				for (size_t t = 0; t < sw->targetCount; t++)
				{
					auto &target = sw->targets[t];
					target = JumpTarget::FromIndex(
						builder.FindIndex(
							builder.GetOriginalOffset(i) +
							builder.GetOriginalSize(i) +
							target.offset
						)
					);
					builder[target.index]->AddIncomingBranch();
				}
			}
		}
	}

	void MethodParser::InitTryBlockOffsets(MethodBuilder &builder)
	{
		for (size_t i = 0; i < method->tryBlockCount; i++)
		{
			TryBlock &tryBlock = method->tryBlocks[i];
			tryBlock.tryStart = builder.FindIndex(tryBlock.tryStart);
			tryBlock.tryEnd = builder.FindIndex(tryBlock.tryEnd);

			switch (tryBlock.kind)
			{
			case TryKind::CATCH:
				for (size_t c = 0; c < tryBlock.catches.count; c++)
				{
					CatchBlock &catchBlock = tryBlock.catches.blocks[c];
					if (catchBlock.caughtType == nullptr)
						catchBlock.caughtType = TypeFromToken(catchBlock.caughtTypeId);
					catchBlock.catchStart = builder.FindIndex(catchBlock.catchStart);
					catchBlock.catchEnd = builder.FindIndex(catchBlock.catchEnd);
				}
				break;
			case TryKind::FINALLY:
			case TryKind::FAULT: // uses finallyBlock
			{
				auto &finallyBlock = tryBlock.finallyBlock;
				finallyBlock.finallyStart = builder.FindIndex(finallyBlock.finallyStart);
				finallyBlock.finallyEnd = builder.FindIndex(finallyBlock.finallyEnd);
			}
			break;
			}
		}
	}

	void MethodParser::InitDebugSymbolOffsets(MethodBuilder &builder)
	{
		debug::OverloadSymbols *debug = method->debugSymbols;
		size_t debugSymbolCount = debug->GetSymbolCount();
		for (size_t i = 0; i < debugSymbolCount; i++)
		{
			debug::DebugSymbol &sym = debug->GetSymbol(i);
			sym.startOffset = builder.FindIndex(sym.startOffset);
			sym.endOffset = builder.FindIndex(sym.endOffset);
		}
	}

	Type *MethodParser::TypeFromToken(uint32_t token) const
	{
		Type *result = module->FindType(token);
		if (!result)
			throw MethodInitException::UnresolvedToken(
				"Unresolved TypeDef or TypeRef token.",
				method,
				token
			);

		if (result->IsInternal() && result->module != module)
			throw MethodInitException::InaccessibleType(
				"The type is not accessible from outside its declaring module.",
				method,
				result
			);

		return result;
	}

	String *MethodParser::StringFromToken(uint32_t token) const
	{
		String *result = module->FindString(token);
		if (!result)
			throw MethodInitException::UnresolvedToken(
				"Unresolved String token.",
				method,
				token
			);

		return result;
	}

	Method *MethodParser::MethodFromToken(uint32_t token) const
	{
		Method *result = module->FindMethod(token);
		if (!result)
			throw MethodInitException::UnresolvedToken(
				"Unresolved MethodDef, MethodRef, FunctionDef or FunctionRef token.",
				method,
				token
			);

		if (result->IsStatic())
		{
			// Verify that the method is accessible from this location

			bool accessible = result->declType ?
				// If the method is declared in a type, use IsAccessible
				// Note: instType is only used by protected members. For static methods,
				// we pretend the method is being accessed through an instance of fromMethod->declType
				result->IsAccessible(method->declType, method) :
				// Otherwise, the method is accessible if it's public,
				// or internal and declared in the same module as fromMethod
				result->IsPublic() || result->declModule == module;
			if (!accessible)
				throw MethodInitException::InaccessibleMember(
					"The method is inaccessible from this location.",
					method,
					result
				);
		}

		return result;
	}

	MethodOverload *MethodParser::MethodOverloadFromToken(uint32_t token, ovlocals_t argCount) const
	{
		Method *method = MethodFromToken(token);

		if (!method->IsStatic())
			argCount--;

		MethodOverload *overload = method->ResolveOverload(argCount);
		if (!overload)
			throw MethodInitException::NoMatchingOverload(
				"Could not find a overload that takes the specified number of arguments.",
				this->method,
				method,
				argCount
			);

		return overload;
	}

	Field *MethodParser::FieldFromToken(uint32_t token, bool shouldBeStatic) const
	{
		Field *field = module->FindField(token);
		if (!field)
			throw MethodInitException::UnresolvedToken(
				"Unresolved FieldDef or FieldRef token.",
				method,
				token
			);

		if (field->IsStatic() && !field->IsAccessible(nullptr, method))
			throw MethodInitException::InaccessibleMember(
				"The field is inaccessible from this location.",
				method,
				field
			);

		if (shouldBeStatic != field->IsStatic())
			throw MethodInitException::FieldStaticMismatch(
				shouldBeStatic ? "The field must be static." : "The field must be an instance field.",
				method,
				field
			);

		return field;
	}

	void MethodParser::EnsureConstructible(Type *type, ovlocals_t argCount) const
	{
		if (type->IsAbstract() || type->IsStatic())
			throw MethodInitException::TypeNotConstructible(
				"Abstract and static types cannot be used with the newobj instruction.",
				method,
				type
			);

		if (type->instanceCtor == nullptr)
			throw MethodInitException::TypeNotConstructible(
				"The type does not declare an instance constructor.",
				method,
				type
			);

		if (!type->instanceCtor->IsAccessible(type, method))
			throw MethodInitException::TypeNotConstructible(
				"The instance constructor is not accessible from this location.",
				method,
				type
			);

		if (!type->instanceCtor->ResolveOverload(argCount))
			throw MethodInitException::NoMatchingOverload(
				"The instance constructor does not take the specified number of arguments.",
				method,
				type->instanceCtor,
				argCount
			);
	}
} // namespace instr

} // namespace ovum
