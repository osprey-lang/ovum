#pragma once

#ifndef VM__TYPE_H
#define VM__TYPE_H

#include <exception>
#include "ov_vm.h"


typedef void (__cdecl *NativeMethod)(ThreadHandle thread, const int argc, Value args[]);
// Adds the magic parameters 'ThreadHandle thread', 'int argc' and 'Value args[]' to a function definition.
#define NATIVE_FUNCTION(name)	void __cdecl name(::ThreadHandle thread, const int argc, ::Value args[])
// The 'this' in a NATIVE_FUNCTION, which is always argument 0.
#define THISV	(args[0])


// Determines whether any overload in the method accepts the given number of arguments.
// For instance methods, this does NOT include the instance.
OVUM_API bool Method_Accepts(const MethodHandle m, int argc);

OVUM_API bool Member_IsAccessible(const MemberHandle member, TypeHandle instType, TypeHandle declType, TypeHandle fromType);


//typedef void *ErrorHandle; // I cannot remember what I was planning on using this for.

class OvumException: public std::exception
{
private:
	Value errorValue;

public:
	inline OvumException(Value value) :
		errorValue(value)
	{ }

	inline Value GetError()
	{
		return errorValue;
	}

	inline virtual const char *what() const throw()
	{
		return "An error occurred. Use Error_GetMessage to get the full error message.";
	}
};

OVUM_API String *Error_GetMessage(Value error);
OVUM_API String *Error_GetStackTrace(Value error);


// It is VITAL that these are in the same order as the opcodes.
// See vm.opcodes.h/Opcode
TYPED_ENUM(Operator, uint8_t)
{
	OP_ADD,    // The binary + operator.
	OP_SUB,    // The binary - operator.
	OP_OR,     // The | operator.
	OP_XOR,    // The ^ operator.
	OP_MUL,    // The * operator.
	OP_DIV,    // The / operator.
	OP_MOD,    // The % operator.
	OP_AND,    // The & operator.
	OP_POW,    // The ** operator.
	OP_SHL,    // The << operator.
	OP_SHR,    // The >> operator.
	OP_HASHOP, // The # operator.
	OP_DOLLAR, // The $ operator.
	OP_PLUS,   // The unary + operator.
	OP_NEG,    // The unary - operator.
	OP_NOT,    // The ~ operator.
	OP_EQ,     // The == operator.
	OP_CMP,    // The <=> operator.
};
// The number of overloadable operators.
// If you change Operator and/or Opcode without changing this,
// you have no one to blame but yourself.
#define OPERATOR_COUNT 18


// NOTE: This TypeFlags enum has exactly the same member values as
//       those in the module format specification. Please make sure
//       that they are synchronised!
//       However, the following flags are implementation details:
//         TYPE_CUSTOMPTR
//         TYPE_OPS_INITED
//         TYPE_INITED
//         TYPE_STATICCTORRUN
TYPED_ENUM(TypeFlags, uint32_t)
{
	TYPE_NONE          = 0x0000,

	TYPE_PROTECTION    = 0x0003,
	TYPE_PUBLIC        = 0x0001,
	TYPE_PRIVATE       = 0x0002,

	TYPE_ABSTRACT      = 0x0004,
	TYPE_SEALED        = 0x0008,
	// The type is static; that is, instances of it cannot be created.
	TYPE_STATIC        = TYPE_ABSTRACT | TYPE_SEALED,

	// The type is a value type; that is, it does not have an instance pointer.
	// Value types are always implicitly sealed, hence the TYPE_SEALED flag.
	// TYPES USING THIS FLAG WILL NOT BE ELIGIBLE FOR GARBAGE COLLECTION.
	// If you use this flag and still store a pointer in the VALUE, you are an
	// evil, wicked, truly malevolent person who deserves to be punished.
	TYPE_PRIMITIVE     = 0x0010 | TYPE_SEALED,
	// The type does not use a standard VALUE array for its fields.
	// This is used only by the GC during collection.
	// If the type is NOT List or Hash, any type using this flag MUST
	// set its Type::getReferences field to an appropriate value.
	// Failure to do so will crash the runtime when the GC runs a cycle.
	TYPE_CUSTOMPTR     = 0x0020,
	// Internal use only. If absent from a type's flags, the Type_InitOperators method
	// must be called on the type before invoking an operator on the type.
	TYPE_OPS_INITED    = 0x0040,
	// Internal use only. If set, the type has been initialised.
	TYPE_INITED        = 0x0080,
	// Internal use only. If set, the static constructor for the type has been run.
	TYPE_STATICCTORRUN = 0x0100,
};

// A ReferenceGetter produces an array of Values from a basePtr.
// The basePtr is the base of the fields for a value of the type
// that implements a ReferenceGetter.
// The valc argument contains the total number of Values the instance
// of the type references.
//
// If the Value array put in target needs to be deleted as soon as
// it has been used, return true. Otherwise, return false.
// Return true IF AND ONLY IF the array was specially created for the GC.
//
// If your type has both Value fields and non-Value fields, consider making
// the Values adjacent in memory. That way, you can just give target the
// address of the first Value and set valc to some appropriate value, which
// removes the need for allocating more data during a collection cycle,
// which may fail if the amount of available memory is very small.
//
// When called from the GC, the target argument is guaranteed to refer
// to a valid storage location.
//
// NOTENOTENOTE: basePtr is NOT relative to where the instance begins
// in memory, but is rather instancePtr + type->fieldsOffset.
typedef bool (*ReferenceGetter)(void *basePtr, unsigned int &valc, Value **target);

// A Finalizer is called when the object is about to be deleted.
// If the type has the flag TYPE_CUSTOMPTR, it may have to supply
// a finalizer, to ensure that things outside of the GC's supervision
// get properly released from memory.
//
// The finalizer also provides an ample opportunity to release file
// handles, to avoid locking the file longer than necessary.
//
// basePtr is a pointer to the base of the instance of the type that
// is being finalized. It is equal to the base instance pointer +
// the offset of the finalizing type, and may therefore differ from
// Value.instance.
//
// NOTENOTENOTE: If the finalizer adds any references to the object
// that is about to be deleted, the GC WILL NOT CARE and will delete
// the object anyway. Malicious native-code modules may freely insert
// memory leaks here.
typedef void (*Finalizer)(ThreadHandle thread, void *basePtr);

// Initializes a single type, which may involve setting flags or the size
// of the instance. Type initializers should only be used for types with
// native implementations.
typedef void (*TypeInitializer)(TypeHandle type);

// Initializes a ListInst* to a specific capacity.
// This method is provided to avoid making any assumptions about the
// underlying implementation of the aves.List class, and is taken from
// the main module's exported method "InitListInstance".
// When called, 'list' is guaranteed to refer to a valid ListInst*.
typedef void (*ListInitializer)(ThreadHandle thread, ListInst *list, int32_t capacity);

// Initializes a HashInst* to a specific capacity.
// This method is provided to avoid making any assumptions about the
// underlying implementation of the aves.Hash class, and is taken from
// the main module's exported method "InitHashInstance".
// When called, 'hash' is guaranteed to refer to a valid HashInst*.
typedef void (*HashInitializer)(ThreadHandle thread, HashInst *hash, int32_t capacity);

// Initializes a value of the aves.Type class for a specific underlying
// TypeHandle. The standard module must expose a method with the name
// "InitTypeToken", with this signature, so that the VM can create type
// tokens when they are requested.
typedef Value (*TypeTokenInitializer)(ThreadHandle thread, TypeHandle type);

OVUM_API TypeFlags Type_GetFlags(TypeHandle type);
OVUM_API String *Type_GetFullName(TypeHandle type);

OVUM_API MemberHandle Type_GetMember(TypeHandle type, String *name);
OVUM_API MemberHandle Type_FindMember(TypeHandle type, String *name, TypeHandle fromType);

OVUM_API MethodHandle Type_GetOperator(TypeHandle type, Operator op);
OVUM_API Value Type_GetTypeToken(TypeHandle type);

OVUM_API uint32_t Type_GetFieldOffset(TypeHandle type);
OVUM_API void Type_SetFinalizer(TypeHandle type, Finalizer finalizer);
OVUM_API void Type_SetInstanceSize(TypeHandle type, uint32_t size);
OVUM_API void Type_SetReferenceGetter(TypeHandle type, ReferenceGetter getter);


// Standard types are required by the VM (because the implement special
// behaviour or are needed by opcode instructions), but are implemented
// by the standard library, which is by default represented by the module
// aves.ovm (and its associated aves.dll).
typedef struct StandardTypes_S
{
	TypeHandle Object;
	TypeHandle Boolean;
	TypeHandle Int;
	TypeHandle UInt;
	TypeHandle Real;
	TypeHandle String;
	TypeHandle Enum;
	TypeHandle List;
	TypeHandle Hash;
	TypeHandle Method;
	TypeHandle Iterator;
	TypeHandle Type; // reflection!
	TypeHandle Error;
	TypeHandle TypeError;
	TypeHandle MemoryError;
	TypeHandle OverflowError;
	TypeHandle DivideByZeroError;
	TypeHandle NullReferenceError;
} StandardTypes;

OVUM_API const StandardTypes &GetStandardTypes();
OVUM_API TypeHandle GetType_Object();
OVUM_API TypeHandle GetType_Boolean();
OVUM_API TypeHandle GetType_Int();
OVUM_API TypeHandle GetType_UInt();
OVUM_API TypeHandle GetType_Real();
OVUM_API TypeHandle GetType_String();
OVUM_API TypeHandle GetType_Enum();
OVUM_API TypeHandle GetType_List();
OVUM_API TypeHandle GetType_Hash();
OVUM_API TypeHandle GetType_Method();
OVUM_API TypeHandle GetType_Iterator();
OVUM_API TypeHandle GetType_Type();
OVUM_API TypeHandle GetType_Error();
OVUM_API TypeHandle GetType_TypeError();
OVUM_API TypeHandle GetType_MemoryError();
OVUM_API TypeHandle GetType_OverflowError();
OVUM_API TypeHandle GetType_DivideByZeroError();
OVUM_API TypeHandle GetType_NullReferenceError();

#endif // VM__TYPE_H