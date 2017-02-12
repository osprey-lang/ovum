#pragma once

#include "methodbuilder.h"
#include "instructions.h"
#include "refsignature.h"

namespace ovum
{

namespace instr
{
	// Primarily, the MethodParser class parses bytecode instructions belonging
	// to a bytecode method (MethodOverload*). The parsed instructions are put
	// into a MethodBuilder, not returned directly.
	//
	// Secondarily, this class also updates the offsets of try blocks and debug
	// symbols, changing them to instruction indexes into the builder, ensuring
	// they can be used without the need to translate offsets into indexes all
	// the time.
	class MethodParser
	{
	public:
		// Parses the specified method's bytecode instructions into the specified
		// builder. This method also updates the offsets of try blocks and debug
		// symbols, changing them to instruction indexes.
		//
		// The caller must ensures that the builder is empty, and that the method
		// is an uninitialized bytecode method.
		//
		// If an error occurs at any point while parsing the method or updating
		// offsets to indexes, a MethodInitException is thrown, and the builder
		// is left in a possibly inconsistent state. Do not use the builder if
		// this method throws an exception.
		static void ParseInto(MethodOverload *method, MethodBuilder &builder);

	private:
		OVUM_DISABLE_COPY_AND_ASSIGN(MethodParser);

		MethodOverload *method;
		// Current instruction pointer (location in the method).
		uintptr_t ip;
		// The start of the method body, used to calculate the offset of instructions
		// relative to the beginning.
		uintptr_t methodBodyStart;
		// The end of the method body (exclusive; i.e. one byte past the last).
		uintptr_t methodBodyEnd;

		RefSignature methodRefSignature;
		// An offset that is added to param/arg indexes when calling
		// methodRefSignature.IsParamRef().
		// The ref signature always reserves space for the instance at the very
		// beginning, so for static methods, we have to skip it.
		ovlocals_t argRefOffset;

		// The module that the method belongs to. Used for resolving tokens.
		Module *module;

		explicit MethodParser(MethodOverload *method);

		bool IsAtEnd() const
		{
			return ip == methodBodyEnd;
		}

		// Parses the instruction at the current instruction pointer, and places it
		// into the builder. The instruction pointer is advanced by the size of the
		// instruction.
		void ParseInstruction(MethodBuilder &builder);

		// Parses the arguments of the specified opcode, and creates a complete
		// instruction. The instruction pointer is expected to be one byte past the
		// opcode, and is advanced by the size of the instruction's arguments.
		//
		// Note: the instruction is returned, not placed in the builder. The builder
		// is passed into the method only for the AddTypeToInitialize() method.
		Box<Instruction> ParseInstructionArguments(Opcode opc, MethodBuilder &builder);

		// Reads a value of T from the current instruction pointer, and advances
		// the instruction pointer by the size of T.
		template<class T>
		inline T Read()
		{
			T result;
			memcpy(&result, reinterpret_cast<void*>(ip), sizeof(T));
			ip += sizeof(T);
			return result;
		}

		template<>
		inline int8_t Read<int8_t>()
		{
			return *reinterpret_cast<int8_t*>(ip++);
		}

		template<>
		inline uint8_t Read<uint8_t>()
		{
			return *reinterpret_cast<uint8_t*>(ip++);
		}

		template<>
		inline Opcode Read<Opcode>()
		{
			return static_cast<Opcode>(Read<uint8_t>());
		}

		// Updates the offsets of branches (that is, their jump targets), try blocks
		// (that is, start and end offsets for try, catch, finally and fault blocks),
		// and debug symbols (that is, the start and end offset of each symbol) to
		// instruction indexes into the builder.
		//
		// This ensures said offsets can be used in the MethodInitializer without
		// the need to constantly translate the original offsets into indexes.
		void InitOffsets(MethodBuilder &builder);

		// Updates branch offsets (i.e. jump targets) to instruction indexes.
		void InitBranchOffsets(MethodBuilder &builder);

		// Updates try block offsets (i.e. start and end offsets of try, catch,
		// finally and fault blocks) to instruction indexes.
		void MethodParser::InitTryBlockOffsets(MethodBuilder &builder);

		// Updates debug symbol offsets (i.e. start and end offset of each debug
		// symbol) to instruction indexes.
		void MethodParser::InitDebugSymbolOffsets(MethodBuilder &builder);

		// Resolves a typedef or typeref token to a Type*.
		//
		// This method verifies that:
		//   - the type exists (i.e. the token is valid);
		//   - the type is accessible from the method being initialized.
		Type *TypeFromToken(uint32_t token) const;

		// Resolves a string token to a String*.
		//
		// This method verifies that:
		//   - the string exists (i.e. the token is valid).
		String *StringFromToken(uint32_t token) const;

		// Resolves a functiondef, functionref, methoddef or methodref token to
		// a Method*.
		//
		// This method verifies that:
		//   - the method exists (i.e. the token is valid);
		//   - the method is accessible from the method being initialized, if
		//     the method is static (accessibility of instance methods requires
		//     knowing the instance type, because of how 'protected' works).
		Method *MethodFromToken(uint32_t token) const;

		// Resolves a functiondef, functionref, methoddef or methodref token to
		// a MethodOverload* accepting the specified argument count.
		//
		// This method verifies that:
		//   - the method exists (i.e. the token is valid);
		//   - the method is accessible from the method being initialized, if
		//     the method is static (accessibility of instance methods requires
		//     knowing the instance type, because of how 'protected' works);
		//   - the method contains an overload accepting argCount arguments.
		//
		// This method does NOT verify the reference signature. That's done by
		// MethodInitializer.
		MethodOverload *MethodOverloadFromToken(uint32_t token, ovlocals_t argCount) const;

		// Resolves a fielddef or fieldref token to a Field*.
		//
		// This method verifies that:
		//   - the field exists (i.e. the token is valid);
		//   - the field is accessible from the method being initialized, if the
		//     field is static (accessibility of instance fields requires knowing
		//     the instance type, because of how 'protected' works);
		//   - the field has the right 'staticness', acccording to shouldBeStatic.
		Field *FieldFromToken(uint32_t token, bool shouldBeStatic) const;

		// Ensures that the specified type can be constructed with the specified
		// number of arguments.
		//
		// This method does NOT verify the reference signature. That's done by
		// MethodInitializer.
		void EnsureConstructible(Type *type, ovlocals_t argCount) const;
	};
} // namespace instr

} // namespace ovum
