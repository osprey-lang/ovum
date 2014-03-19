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


OVUM_API String *Member_GetName(const MemberHandle member);

enum class MemberKind
{
	INVALID  = 0,
	METHOD   = 1,
	FIELD    = 2,
	PROPERTY = 3,
};
enum class MemberAccess
{
	INVALID   = -1,
	PUBLIC    = 0,
	PROTECTED = 1,
	PRIVATE   = 2,
};

OVUM_API MemberKind Member_GetKind(const MemberHandle member);
OVUM_API MemberAccess Member_GetAccessLevel(const MemberHandle member);
OVUM_API TypeHandle Member_GetDeclType(const MemberHandle member);

OVUM_API bool Member_IsStatic(const MemberHandle member);
OVUM_API bool Member_IsImpl(const MemberHandle member);
OVUM_API bool Member_IsAccessible(const MemberHandle member, TypeHandle instType, TypeHandle fromType);

OVUM_API MethodHandle Member_ToMethod(const MemberHandle member);
OVUM_API FieldHandle Member_ToField(const MemberHandle member);
OVUM_API PropertyHandle Member_ToProperty(const MemberHandle member);


enum class MethodFlags : int32_t
{
	NONE      = 0x0000,
	// The method has a variadic parameter at the end.
	VAR_END   = 0x0001,
	// The method has a variadic parameter at the start.
	VAR_START = 0x0002,

	// The method has a native-code implementation.
	NATIVE    = 0x0004,

	// The method is an instance method. Without this flag,
	// methods are static.
	INSTANCE  = 0x0008,

	// The method is virtual (overridable in Osprey).
	VIRTUAL   = 0x0010,
	// The method is abstract (it has no implementation).
	ABSTRACT  = 0x0020,

	// The method is a constructor.
	CTOR      = 0x0040,

	// The method has been initialized. Used for bytecode methods only,
	// to indicate that the bytecode initializer has processed the method.
	INITED    = 0x0080,

	// A mask for extracting the variadic flags of a method.
	VARIADIC = VAR_END | VAR_START,
};
ENUM_OPS(MethodFlags, int32_t);

OVUM_API int32_t Method_GetOverloadCount(const MethodHandle method);
OVUM_API MethodFlags Method_GetFlags(const MethodHandle method, int overloadIndex);
OVUM_API MethodHandle Method_GetBaseMethod(const MethodHandle method);

// Determines whether any overload in the method accepts the given number of arguments.
// For instance methods, this does NOT include the instance.
OVUM_API bool Method_Accepts(const MethodHandle method, int argc);


OVUM_API uint32_t Field_GetOffset(const FieldHandle field);
OVUM_API bool Field_GetStaticValue(const FieldHandle field, Value &result);
OVUM_API bool Field_SetStaticValue(const FieldHandle field, Value value);


OVUM_API MethodHandle Property_GetGetter(const PropertyHandle prop);
OVUM_API MethodHandle Property_GetSetter(const PropertyHandle prop);


class OvumException: public std::exception
{
private:
	Value errorValue;

public:
	inline OvumException(Value value) :
		exception("A managed error was thrown. Use GetManagedMessage to retrieve the full error message."),
		errorValue(value)
	{ }

	inline Value GetError() const
	{
		return errorValue;
	}

	inline String *GetManagedMessage() const
	{
		return errorValue.common.error->message;
	}
};


// It is VITAL that these are in the same order as the opcodes.
// See ov_thread.opcodes.h/Opcode
enum class Operator : uint8_t
{
	ADD,    // The binary + operator.
	SUB,    // The binary - operator.
	OR,     // The | operator.
	XOR,    // The ^ operator.
	MUL,    // The * operator.
	DIV,    // The / operator.
	MOD,    // The % operator.
	AND,    // The & operator.
	POW,    // The ** operator.
	SHL,    // The << operator.
	SHR,    // The >> operator.
	HASHOP, // The # operator.
	DOLLAR, // The $ operator.
	PLUS,   // The unary + operator.
	NEG,    // The unary - operator.
	NOT,    // The ~ operator.
	EQ,     // The == operator.
	CMP,    // The <=> operator.
};
// The number of overloadable operators.
// If you change Operator and/or Opcode without changing this,
// you have no one to blame but yourself.
#define OPERATOR_COUNT 18

inline unsigned int Arity(Operator op)
{
	switch (op)
	{
		case Operator::PLUS:
		case Operator::NEG:
		case Operator::NOT:
			return 1;
		default:
			return 2;
	}
}


// NOTE: This TypeFlags enum has exactly the same member values as
//       those in the module format specification. Please make sure
//       that they are synchronised!
//       However, the following flags are implementation details:
//         CUSTOMPTR
//         OPS_INITED
//         INITED
//         STATIC_CTOR_RUN
//         HAS_FINALIZER
enum class TypeFlags : uint32_t
{
	NONE            = 0x0000,

	PROTECTION      = 0x0003,
	PUBLIC          = 0x0001,
	PRIVATE         = 0x0002,

	ABSTRACT        = 0x0004,
	SEALED          = 0x0008,
	// The type is static; that is, instances of it cannot be created.
	STATIC          = ABSTRACT | SEALED,

	// The type is a value type; that is, it does not have an instance pointer.
	// Value types are always implicitly sealed, hence the TYPE_SEALED flag.
	// TYPES USING THIS FLAG WILL NOT BE ELIGIBLE FOR GARBAGE COLLECTION.
	// If you use this flag and still store a pointer in the Value, you are an
	// evil, wicked, truly malevolent person who deserves to be punished.
	// Unless there's a good reason to do so.
	PRIMITIVE       = 0x0010 | SEALED,
	// The type does not use a standard Value array for its fields.
	// This is used only by the GC during collection.
	CUSTOMPTR       = 0x0020,
	// Internal use only. If set, the type's operators have been initialized.
	OPS_INITED      = 0x0040,
	// Internal use only. If set, the type has been initialised.
	INITED          = 0x0080,
	// Internal use only. If set, the static constructor for the type has been run.
	STATIC_CTOR_RUN = 0x0100,
	// Internal use only. If set, the type or any of its base types has a finalizer,
	// which must be run before the value is collected.
	HAS_FINALIZER   = 0x0200,
};
ENUM_OPS(TypeFlags, uint32_t);

// A ReferenceGetter produces an array of Values from a basePtr. This function
// is called repeatedly for the same object until false is returned.
//
// The value of 'state' is preserved across calls to the same reference getter
// on the same object during the same GC cycle, and starts out at zero. This is
// to permit implementations of native types to containnon-adjacent Values. For
// example, aves.Set keeps its values in a special native struct alongside other
// data, and the state is used as a numeric index into the entries.
//
// Parameters:
//   basePtr:
//     The base of the fields for a value of the type that
//     implements the ReferenceGetter.
//   valc:
//     Receives the total number of Values the instance
//     of the type references.
//   target:
//     Receives a pointer to the first Value in an array of values.
//   state:
//     Keeps track of the enumeration state. Types that do not need a state
//     are free to ignore this value.
//
// If your type has both Value fields and non-Value fields, consider making
// the Values adjacent in memory. That way, you can just give target the
// address of the first Value and set valc to the appropriate length, thus
// removing the need for repeatedly calling the reference getter.
//
// NOTENOTENOTE: basePtr is NOT relative to where the instance begins
// in memory, but is rather instancePtr + type->fieldsOffset.
typedef bool (*ReferenceGetter)(void *basePtr, unsigned int *valc, Value **target, int32_t *state);

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
// NOTE: Finalizers do not have access to the managed runtime. Do not
// attempt to access the managed runtime from a finalizer. Do not try
// to allocate any managed memory during a finalizer. Doing either
// results in undefined and probably very undesirable behavior.
//
// NOTENOTENOTE: If the finalizer adds any references to the object
// that is about to be deleted, the GC WILL NOT CARE and will delete
// the object anyway. Malicious native-code modules may freely insert
// memory leaks here.
typedef void (*Finalizer)(void *basePtr);

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
typedef void (*TypeTokenInitializer)(ThreadHandle thread, void *basePtr, TypeHandle type);

OVUM_API TypeFlags Type_GetFlags(TypeHandle type);
OVUM_API String *Type_GetFullName(TypeHandle type);
OVUM_API TypeHandle Type_GetBaseType(TypeHandle type);
OVUM_API ModuleHandle Type_GetDeclModule(TypeHandle type);

OVUM_API MemberHandle Type_GetMember(TypeHandle type, String *name);
OVUM_API MemberHandle Type_FindMember(TypeHandle type, String *name, TypeHandle fromType);

OVUM_API int32_t Type_GetMemberCount(TypeHandle type);
OVUM_API MemberHandle Type_GetMemberByIndex(TypeHandle type, const int32_t index);

OVUM_API MethodHandle Type_GetOperator(TypeHandle type, Operator op);
OVUM_API Value Type_GetTypeToken(ThreadHandle thread, TypeHandle type);

OVUM_API uint32_t Type_GetFieldOffset(TypeHandle type);
OVUM_API uint32_t Type_GetInstanceSize(TypeHandle type);
OVUM_API void Type_SetFinalizer(TypeHandle type, Finalizer finalizer);
OVUM_API void Type_SetInstanceSize(TypeHandle type, uint32_t size);
OVUM_API void Type_SetReferenceGetter(TypeHandle type, ReferenceGetter getter);


// Standard types are required by the VM (because they implement special
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
	TypeHandle List;
	TypeHandle Hash;
	TypeHandle Method;
	TypeHandle Iterator;
	TypeHandle Type; //\\ ;epyT eldnaHepyT
	TypeHandle Error;
	TypeHandle TypeError;
	TypeHandle MemoryError;
	TypeHandle OverflowError;
	TypeHandle NoOverloadError;
	TypeHandle DivideByZeroError;
	TypeHandle NullReferenceError;
	TypeHandle MemberNotFoundError;
} StandardTypes;

OVUM_API void GetStandardTypes(StandardTypes *target, size_t targetSize);
OVUM_API TypeHandle GetType_Object();
OVUM_API TypeHandle GetType_Boolean();
OVUM_API TypeHandle GetType_Int();
OVUM_API TypeHandle GetType_UInt();
OVUM_API TypeHandle GetType_Real();
OVUM_API TypeHandle GetType_String();
OVUM_API TypeHandle GetType_List();
OVUM_API TypeHandle GetType_Hash();
OVUM_API TypeHandle GetType_Method();
OVUM_API TypeHandle GetType_Iterator();
OVUM_API TypeHandle GetType_Type();
OVUM_API TypeHandle GetType_Error();
OVUM_API TypeHandle GetType_TypeError();
OVUM_API TypeHandle GetType_MemoryError();
OVUM_API TypeHandle GetType_OverflowError();
OVUM_API TypeHandle GetType_NoOverloadError();
OVUM_API TypeHandle GetType_DivideByZeroError();
OVUM_API TypeHandle GetType_NullReferenceError();
OVUM_API TypeHandle GetType_MemberNotFoundError();

class TypeMemberIterator
{
private:
	TypeHandle type;
	int32_t index;
	bool includeInherited;

public:
	inline TypeMemberIterator(TypeHandle type)
		: type(type), index(-1), includeInherited(false)
	{ }
	inline TypeMemberIterator(TypeHandle type, bool includeInherited)
		: type(type), index(-1), includeInherited(includeInherited)
	{ }

	inline bool MoveNext()
	{
		while (type)
		{
			if (index < Type_GetMemberCount(type) - 1)
			{
				index++;
				return true;
			}

			// Try the base type, unless includeInherited is false,
			// in which case we stop.
			type = includeInherited ? Type_GetBaseType(type) : nullptr;
			index = -1;
		}

		return false;
	}

	inline MemberHandle Current()
	{
		return Type_GetMemberByIndex(type, index);
	}
};

#endif // VM__TYPE_H