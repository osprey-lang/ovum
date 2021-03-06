#ifndef OVUM__TYPE_H
#define OVUM__TYPE_H

#include "ovum.h"

typedef int (OVUM_CDECL *NativeMethod)(ThreadHandle thread, ovlocals_t argc, Value args[]);
// Adds the magic parameters 'ThreadHandle thread', 'int argc' and 'Value args[]' to a function definition.
#define NATIVE_FUNCTION(name)	int OVUM_CDECL name(::ThreadHandle thread, ovlocals_t argc, ::Value args[])
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
OVUM_API bool Member_IsAccessible(MemberHandle member, TypeHandle instType, OverloadHandle fromMethod);

OVUM_API MethodHandle Member_ToMethod(MemberHandle member);
OVUM_API FieldHandle Member_ToField(MemberHandle member);
OVUM_API PropertyHandle Member_ToProperty(MemberHandle member);


OVUM_API bool Method_IsConstructor(MethodHandle method);
OVUM_API size_t Method_GetOverloadCount(MethodHandle method);
OVUM_API OverloadHandle Method_GetOverload(MethodHandle method, size_t index);
OVUM_API size_t Method_GetOverloads(MethodHandle method, size_t destSize, OverloadHandle *dest);
OVUM_API MethodHandle Method_GetBaseMethod(MethodHandle method);

// Determines whether any overload in the method accepts the given number of arguments.
// For instance methods, this does NOT include the instance.
OVUM_API bool Method_Accepts(MethodHandle method, ovlocals_t argc);
OVUM_API OverloadHandle Method_FindOverload(MethodHandle method, ovlocals_t argc);

#define OVUM_OVERLOAD_VARIADIC      0x00000001
#define OVUM_OVERLOAD_VIRTUAL       0x00000100
#define OVUM_OVERLOAD_ABSTRACT      0x00000200
#define OVUM_OVERLOAD_OVERRIDE      0x00000400
#define OVUM_OVERLOAD_NATIVE        0x00001000
#define OVUM_OVERLOAD_SHORT_HEADER  0x00002000

OVUM_API uint32_t Overload_GetFlags(OverloadHandle overload);

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
OVUM_API ovlocals_t Overload_GetParamCount(OverloadHandle overload);
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
OVUM_API bool Overload_GetParameter(OverloadHandle overload, ovlocals_t index, ParamInfo *dest);
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
OVUM_API ovlocals_t Overload_GetAllParameters(OverloadHandle overload, ovlocals_t destSize, ParamInfo *dest);
// Gets a handle to an overload's containing method.
OVUM_API MethodHandle Overload_GetMethod(OverloadHandle overload);

OVUM_API size_t Field_GetOffset(FieldHandle field);

OVUM_API MethodHandle Property_GetGetter(PropertyHandle prop);
OVUM_API MethodHandle Property_GetSetter(PropertyHandle prop);

// It is VITAL that these are in the same order as the opcodes.
// See ov_thread.opcodes.h/Opcode
enum class Operator : uint8_t
{
	ADD  =  0,   // The binary + operator.
	SUB  =  1,   // The binary - operator.
	OR   =  2,   // The | operator.
	XOR  =  3,   // The ^ operator.
	MUL  =  4,   // The * operator.
	DIV  =  5,   // The / operator.
	MOD  =  6,   // The % operator.
	AND  =  7,   // The & operator.
	POW  =  8,   // The ** operator.
	SHL  =  9,   // The << operator.
	SHR  = 10,   // The >> operator.
	PLUS = 11,   // The unary + operator.
	NEG  = 12,   // The unary - operator.
	NOT  = 13,   // The ~ operator.
	EQ   = 14,   // The == operator.
	CMP  = 15,   // The <=> operator.
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

// Type flags

#define OVUM_TYPE_PUBLIC    0x00000001
#define OVUM_TYPE_INTERNAL  0x00000002
#define OVUM_TYPE_ABSTRACT  0x00000100
#define OVUM_TYPE_SEALED    0x00000200
#define OVUM_TYPE_STATIC    0x00000300
#define OVUM_TYPE_IMPL      0x00001000
#define OVUM_TYPE_PRIMITIVE 0x00002000

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
typedef int (OVUM_CDECL *ReferenceVisitor)(void *cbState, size_t count, Value *values);

// A ReferenceWalker produces an array of Values from a basePtr. This function
// is called by the GC for two reasons:
//   * To mark referenced objects as alive;
//   * To update references to objects that may have moved.
//
// A method that implements ReferenceWalker must call the given ReferenceVisitor
// for each available set of managed references in the object, and MUST pass the
// value of the 'cbState' as the first argument to 'callback'.
//
// If 'callback' returns any value other than OVUM_SUCCESS, it must be returned
// from the ReferenceWalker, and the callback must not be called again. If the
// ReferenceWalker call succeeds, it must return OVUM_SUCCESS.
//
// Parameters:
//   basePtr:
//     The base of the fields for a value of the type that implements
//     the ReferenceWalker. This is the instance pointer plus the field
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
typedef int (OVUM_CDECL *ReferenceWalker)(void *basePtr, ReferenceVisitor callback, void *cbState);

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
//
// Type initializers return a status code, to indicate whether everything
// went okay. Use OVUM_SUCCESS for success, and an error code otherwise.
//
// Parameters:
//   type:
//     The type to initialize.
typedef int (OVUM_CDECL *TypeInitializer)(TypeHandle type);

// Initializes a ListInst* to a specific capacity.
// This method is provided to avoid making any assumptions about the
// underlying implementation of the aves.List class, and is taken from
// the main module's exported method "InitListInstance".
// When called, 'list' is guaranteed to refer to a valid ListInst*.
typedef int (OVUM_CDECL *ListInitializer)(ThreadHandle thread, ListInst *list, size_t capacity);

// Initializes a Value* with an aves.Hash instance of the specified capacity.
// This method is provided to avoid making any assumptions about the underlying
// implementation of the aves.Hash class. The native library of the module that
// declares aves.Hash must export a function called "InitHashInstance", which
// is called when the runtime needs to construct a hash table.
//
// Parameters:
//   thread:
//     The current managed thread.
//   capacity:
//     The required minimal capacity of the hash table. This value is guaranteed
//     to be greater than or equal to zero.
//   result:
//     A storage location that receives the initialized hash table.
typedef int (OVUM_CDECL *HashInitializer)(ThreadHandle thread, size_t capacity, Value *result);

// Initializes a value of the aves.reflection.Type class for a specific
// underlying TypeHandle. The standard module must expose a method with
// the name "InitTypeToken", with this signature, so that the VM can create
// type tokens when they are requested.
typedef int (OVUM_CDECL *TypeTokenInitializer)(ThreadHandle thread, void *basePtr, TypeHandle type);

OVUM_API uint32_t Type_GetFlags(TypeHandle type);
OVUM_API String *Type_GetFullName(TypeHandle type);
OVUM_API TypeHandle Type_GetBaseType(TypeHandle type);
OVUM_API ModuleHandle Type_GetDeclModule(TypeHandle type);

OVUM_API MemberHandle Type_GetMember(TypeHandle type, String *name);
OVUM_API MemberHandle Type_FindMember(TypeHandle type, String *name, OverloadHandle fromMethod);

OVUM_API size_t Type_GetMemberCount(TypeHandle type);
OVUM_API MemberHandle Type_GetMemberByIndex(TypeHandle type, size_t index);

OVUM_API MethodHandle Type_GetOperator(TypeHandle type, Operator op);
OVUM_API int Type_GetTypeToken(ThreadHandle thread, TypeHandle type, Value *result);

OVUM_API uint32_t Type_GetFieldOffset(TypeHandle type);
OVUM_API size_t Type_GetInstanceSize(TypeHandle type);
OVUM_API size_t Type_GetTotalSize(TypeHandle type);
OVUM_API void Type_SetFinalizer(TypeHandle type, Finalizer finalizer);
OVUM_API void Type_SetInstanceSize(TypeHandle type, size_t size);
OVUM_API void Type_SetReferenceWalker(TypeHandle type, ReferenceWalker getter);
OVUM_API void Type_SetConstructorIsAllocator(TypeHandle type, bool isAllocator);

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
OVUM_API int Type_AddNativeField(TypeHandle type, size_t offset, NativeFieldType fieldType);

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
	TypeHandle TypeConversionError;
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
OVUM_API TypeHandle GetType_TypeConversionError(ThreadHandle thread);

class TypeMemberIterator
{
private:
	bool includeInherited;
	TypeHandle type;
	size_t index;
	MemberHandle current;

public:
	inline TypeMemberIterator(TypeHandle type) :
		includeInherited(false),
		type(type),
		index(0),
		current(nullptr)
	{ }
	inline TypeMemberIterator(TypeHandle type, bool includeInherited) :
		includeInherited(includeInherited),
		type(type),
		index(0),
		current(nullptr)
	{ }

	inline bool MoveNext()
	{
		while (type)
		{
			if (index < Type_GetMemberCount(type))
			{
				current = Type_GetMemberByIndex(type, index);
				index++;
				return true;
			}

			// Try the base type, unless includeInherited is false,
			// in which case we stop.
			type = includeInherited ? Type_GetBaseType(type) : nullptr;
			index = 0;
		}

		return false;
	}

	inline MemberHandle Current()
	{
		return current;
	}
};

#endif // OVUM__TYPE_H
