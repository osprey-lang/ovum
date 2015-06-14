#ifndef VM__TYPE_H
#define VM__TYPE_H

#include "ov_vm.h"

typedef int (OVUM_CDECL *NativeMethod)(ThreadHandle thread, const int argc, Value args[]);
// Adds the magic parameters 'ThreadHandle thread', 'int argc' and 'Value args[]' to a function definition.
#define NATIVE_FUNCTION(name)	int OVUM_CDECL name(::ThreadHandle thread, const int argc, ::Value args[])
// The 'this' in a NATIVE_FUNCTION, which is always argument 0.
#define THISV	(args[0])
#define THISP   (args + 0)

OVUM_API String *Member_GetName(MemberHandle member);

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

OVUM_API MemberKind Member_GetKind(MemberHandle member);
OVUM_API MemberAccess Member_GetAccessLevel(MemberHandle member);
OVUM_API TypeHandle Member_GetDeclType(MemberHandle member);
OVUM_API ModuleHandle Member_GetDeclModule(MemberHandle member);

OVUM_API bool Member_IsStatic(MemberHandle member);
OVUM_API bool Member_IsImpl(MemberHandle member);
OVUM_API bool Member_IsAccessible(MemberHandle member, TypeHandle instType, TypeHandle fromType);

OVUM_API MethodHandle Member_ToMethod(MemberHandle member);
OVUM_API FieldHandle Member_ToField(MemberHandle member);
OVUM_API PropertyHandle Member_ToProperty(MemberHandle member);


OVUM_API bool Method_IsConstructor(MethodHandle method);
OVUM_API int32_t Method_GetOverloadCount(MethodHandle method);
OVUM_API OverloadHandle Method_GetOverload(MethodHandle method, int32_t index);
OVUM_API int32_t Method_GetOverloads(MethodHandle method, int32_t destSize, OverloadHandle *dest);
OVUM_API MethodHandle Method_GetBaseMethod(MethodHandle method);

// Determines whether any overload in the method accepts the given number of arguments.
// For instance methods, this does NOT include the instance.
OVUM_API bool Method_Accepts(MethodHandle method, int argc);
OVUM_API OverloadHandle Method_FindOverload(MethodHandle method, int argc);

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
OVUM_ENUM_OPS(MethodFlags, int32_t);

OVUM_API MethodFlags Overload_GetFlags(OverloadHandle overload);

typedef struct ParamInfo_S
{
	String *name;
	bool isOptional;
	bool isVariadic;
	bool isByRef;
} ParamInfo;

// Gets the total number of named parameters the overload has.
// The count does not include the 'this' parameter if the overload
// is in an instance method.
OVUM_API int32_t Overload_GetParamCount(OverloadHandle overload);
// Gets metadata about a specific parameter in the specified overload.
//
// Returns true if there is a parameter at the specified index; otherwise, false.
//
// Parameters:
//   overload:
//     The method overload to get parameter info from.
//   index:
//     The index of the parameter to get metadata for.
//   dest:
//     The destination buffer.
OVUM_API bool Overload_GetParameter(OverloadHandle overload, int32_t index, ParamInfo *dest);
// Gets metadata about all the parameters in the specified overload.
//
// Returns the number of ParamInfo items that were written into 'dest'.
//
// Parameters:
//   overload:
//     The method overload to get parameter info from.
//   destSize:
//     The size of the destination buffer, in number of ParamInfo items.
//   dest:
//     The destination buffer.
OVUM_API int32_t Overload_GetAllParameters(OverloadHandle overload, int32_t destSize, ParamInfo *dest);
// Gets a handle to an overload's containing method.
OVUM_API MethodHandle Overload_GetMethod(OverloadHandle overload);

OVUM_API uint32_t Field_GetOffset(FieldHandle field);

OVUM_API MethodHandle Property_GetGetter(PropertyHandle prop);
OVUM_API MethodHandle Property_GetSetter(PropertyHandle prop);

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
//         STATIC_CTOR_RUNNING
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
	OPS_INITED          = 0x0040,
	// Internal use only. If set, the type has been initialised.
	INITED              = 0x0080,
	// Internal use only. If set, the static constructor for the type has been run.
	STATIC_CTOR_RUN     = 0x0100,
	// Internal use only. If set, the static constructor is currently running.
	STATIC_CTOR_RUNNING = 0x0200,
	// Internal use only. If set, the type or any of its base types has a finalizer,
	// which must be run before the value is collected.
	HAS_FINALIZER       = 0x0400,
};
OVUM_ENUM_OPS(TypeFlags, uint32_t);

// A ReferenceVisitor receives a set of zero or more managed references stored
// in an object with a native implementation.
//
// If a ReferenceVisitor returns a value other than OVUM_SUCCESS, the ReferenceGetter
// that invoked the callback must return that value and not call the callback
// again.
//
// Parameters:
//   cbState:
//     The state paseed into the ReferenceGetter. This must not be modified
//     in any way.
//   count:
//     The number of values contained in 'values'. May be zero.
//   values:
//     An array of zero or more managed references.
typedef int (OVUM_CDECL *ReferenceVisitor)(void *cbState, unsigned int count, Value *values);

// A ReferenceGetter produces an array of Values from a basePtr. This function
// is called by the GC for two reasons:
//   * To mark referenced objects as alive;
//   * To update references to objects that may have moved.
//
// A method that implements ReferenceGetter must call the given ReferenceVisitor
// for each available set of managed references in the object, and MUST pass the
// value of the 'cbState' as the first argument to 'callback'.
//
// If 'callback' returns any value other than OVUM_SUCCESS, it must be returned
// from the ReferenceGetter, and the callback must not be called again. If the
// ReferenceGetter call succeeds, it must return OVUM_SUCCESS.
//
// Parameters:
//   basePtr:
//     The base of the fields for a value of the type that implements
//     the ReferenceGetter. This is the instance pointer plus the field
//     offset of the type.
//   callback:
//     A function that is called for each set of managed references in
//     the object. See the documentation of ReferenceVisitor.
//   cbState:
//     A value that must be passed entirely unmodified as the first argument
//     to the callback.
//
// If your type has both Value fields and non-Value fields, consider making
// the Values adjacent in memory. That way, you can just give 'callback' the
// address of the first Value and pass an appropriate length into 'count',
// thus removing the need for repeatedly calling the callback.
//
// Also, whenever possible, use native fields (see Type_AddNativeField) and
// GC-allocated arrays (GC_AllocArray, GC_AllocValueArray).
//
// NOTENOTENOTE: basePtr is NOT relative to where the instance begins
// in memory, but is rather instancePtr + type->fieldsOffset.
typedef int (OVUM_CDECL *ReferenceGetter)(void *basePtr, ReferenceVisitor callback, void *cbState);

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
typedef void (OVUM_CDECL *Finalizer)(void *basePtr);

// Initializes a single type, which may involve setting flags or the size
// of the instance. Type initializers should only be used for types with
// native implementations.
typedef void (OVUM_CDECL *TypeInitializer)(TypeHandle type);

// Initializes a ListInst* to a specific capacity.
// This method is provided to avoid making any assumptions about the
// underlying implementation of the aves.List class, and is taken from
// the main module's exported method "InitListInstance".
// When called, 'list' is guaranteed to refer to a valid ListInst*.
typedef int (OVUM_CDECL *ListInitializer)(ThreadHandle thread, ListInst *list, int32_t capacity);

// Initializes a HashInst* to a specific capacity.
// This method is provided to avoid making any assumptions about the
// underlying implementation of the aves.Hash class, and is taken from
// the main module's exported method "InitHashInstance".
// When called, 'hash' is guaranteed to refer to a valid HashInst*.
typedef int (OVUM_CDECL *HashInitializer)(ThreadHandle thread, HashInst *hash, int32_t capacity);

// Initializes a value of the aves.reflection.Type class for a specific
// underlying TypeHandle. The standard module must expose a method with
// the name "InitTypeToken", with this signature, so that the VM can create
// type tokens when they are requested.
typedef int (OVUM_CDECL *TypeTokenInitializer)(ThreadHandle thread, void *basePtr, TypeHandle type);

OVUM_API TypeFlags Type_GetFlags(TypeHandle type);
OVUM_API String *Type_GetFullName(TypeHandle type);
OVUM_API TypeHandle Type_GetBaseType(TypeHandle type);
OVUM_API ModuleHandle Type_GetDeclModule(TypeHandle type);

OVUM_API MemberHandle Type_GetMember(TypeHandle type, String *name);
OVUM_API MemberHandle Type_FindMember(TypeHandle type, String *name, TypeHandle fromType);

OVUM_API int32_t Type_GetMemberCount(TypeHandle type);
OVUM_API MemberHandle Type_GetMemberByIndex(TypeHandle type, const int32_t index);

OVUM_API MethodHandle Type_GetOperator(TypeHandle type, Operator op);
OVUM_API int Type_GetTypeToken(ThreadHandle thread, TypeHandle type, Value *result);

OVUM_API uint32_t Type_GetFieldOffset(TypeHandle type);
OVUM_API size_t Type_GetInstanceSize(TypeHandle type);
OVUM_API size_t Type_GetTotalSize(TypeHandle type);
OVUM_API void Type_SetFinalizer(TypeHandle type, Finalizer finalizer);
OVUM_API void Type_SetInstanceSize(TypeHandle type, size_t size);
OVUM_API void Type_SetReferenceGetter(TypeHandle type, ReferenceGetter getter);

enum class NativeFieldType : int
{
	// The native field is a single Value.
	VALUE = 0,
	// The native field is a single Value* or nullptr.
	VALUE_PTR = 1,
	// The native field contains a String* or nullptr.
	STRING = 2,
	// The native field contains an array of arbitrary values,
	// allocated by GC_AllocArray or GC_AllocValueArray.
	//
	// NOTE: Do not use this field type for arrays allocated in
	// any other way. The GC won't be able to examine a native
	// array's contents, as it has no way of knowing what it
	// contains, nor can it obtain the length of such an array.
	// If the array contains managed references, you generally
	// have to implement a ReferenceGetter in addition to adding
	// a native field of this type. The GC only uses this field
	// type to keep the array alive.
	GC_ARRAY = 3,
};
// Adds a native field to a type that does not use regular Ovum fields
// for its instance data. Native fields added through this method can
// only contain references to managed data, and are used by the GC during
// a cycle to mark those references as alive.
//
// In some cases, it may be preferable or necessary to implement a
// ReferenceGetter instead of or in addition to using native fields.
//
// Parameters:
//   type:
//     The type to add a native field to.
//   offset:
//     The offset of the field, in bytes, relative to the start of the instance.
//     If the field is contained in a native struct or class, consider using the
//     offsetof() macro to obtain its offset, e.g. offsetof(MyType, field).
//   fieldType:
//     The type of the data contained in the field. See the NativeFieldType enum
//     values for more information. The type must ensure that only data of the
//     given type is written to the native field, or the GC will probably crash
//     the program when it examines the field.
//
// NOTE: Ovum does not verify that your native fields are non-overlapping. It is
// entirely up to you to lay them out sensibly.
OVUM_API void Type_AddNativeField(TypeHandle type, size_t offset, NativeFieldType fieldType);

// Standard types are required by the VM (because they implement special
// behaviour or are needed by opcode instructions), but are implemented
// by the standard library, which is by default represented by the module
// aves.ovm (and its associated native library).
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

OVUM_API void GetStandardTypes(ThreadHandle thread, StandardTypes *target, size_t targetSize);
OVUM_API TypeHandle GetType_Object(ThreadHandle thread);
OVUM_API TypeHandle GetType_Boolean(ThreadHandle thread);
OVUM_API TypeHandle GetType_Int(ThreadHandle thread);
OVUM_API TypeHandle GetType_UInt(ThreadHandle thread);
OVUM_API TypeHandle GetType_Real(ThreadHandle thread);
OVUM_API TypeHandle GetType_String(ThreadHandle thread);
OVUM_API TypeHandle GetType_List(ThreadHandle thread);
OVUM_API TypeHandle GetType_Hash(ThreadHandle thread);
OVUM_API TypeHandle GetType_Method(ThreadHandle thread);
OVUM_API TypeHandle GetType_Iterator(ThreadHandle thread);
OVUM_API TypeHandle GetType_Type(ThreadHandle thread);
OVUM_API TypeHandle GetType_Error(ThreadHandle thread);
OVUM_API TypeHandle GetType_TypeError(ThreadHandle thread);
OVUM_API TypeHandle GetType_MemoryError(ThreadHandle thread);
OVUM_API TypeHandle GetType_OverflowError(ThreadHandle thread);
OVUM_API TypeHandle GetType_NoOverloadError(ThreadHandle thread);
OVUM_API TypeHandle GetType_DivideByZeroError(ThreadHandle thread);
OVUM_API TypeHandle GetType_NullReferenceError(ThreadHandle thread);
OVUM_API TypeHandle GetType_MemberNotFoundError(ThreadHandle thread);

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