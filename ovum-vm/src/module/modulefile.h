#pragma once

#include "../vm.h"

namespace ovum
{

typedef uint32_t Token;

namespace module_file
{

	union MagicNumber;

	struct ModuleVersion;
	struct ModuleHeader;
	struct WideString;
	struct ByteString;
	struct StringTableHeader;
	struct RefTableHeader;
	struct ModuleRef;
	struct TypeRef;
	struct FieldRef;
	struct MethodRef;
	struct FunctionRef;
	struct StringMapHeader;
	struct StringMapEntry;
	struct TypeDef;
	struct PropertyDef;
	struct OperatorDef;
	struct FieldDef;
	struct MethodDef;
	struct OverloadDef;
	struct Parameter;
	struct ConstantDef;
	struct ConstantValue;
	struct MethodBody;
	struct MethodHeader;
	struct TryBlock;
	struct CatchClauses;
	struct CatchClause;
	struct FinallyClause;
	struct NativeMethodHeader;
	struct Annotations;
	struct Annotation;
	struct AnnotationArgument;
	struct AnnotationArgumentList;
	struct NamedAnnotationArgument;

	template<typename T>
	struct Rva
	{
		uint32_t address;

		inline bool IsNull() const
		{
			return address == 0;
		}
	};

	template<typename T>
	struct Rva<T[]>
	{
		uint32_t address;

		inline bool IsNull() const
		{
			return address == 0;
		}
	};

	template<typename T>
	struct InlineArray
	{
		// We need to put at least one field in here, to make sure
		// the array is aligned according to T.
		T first;

		inline const T* Get() const
		{
			return reinterpret_cast<const T*>(&first);
		}

		inline const T *operator[](int32_t index) const
		{
			return reinterpret_cast<const T*>(&first) + index;
		}
	};

	enum VersionConstraint : uint32_t
	{
		VERSION_EXACT       = 0x00000000,
		VERSION_FIXED_MINOR = 0x00000001,
		VERSION_FIXED_MAJOR = 0x00000002,
	};

	enum TypeRefFlags : uint32_t
	{
		TYPEREF_NONE = 0,
	};

	enum FieldRefFlags : uint32_t
	{
		FIELDREF_NONE = 0,
	};

	enum MethodRefFlags : uint32_t
	{
		METHODREF_NONE = 0,
	};

	enum FunctionRefFlags : uint32_t
	{
		FUNCTIONREF_NONE = 0,
	};

	enum TypeFlags : uint32_t
	{
		TYPE_PUBLIC    = 0x00000001,
		TYPE_INTERNAL  = 0x00000002,
		TYPE_ABSTRACT  = 0x00000100,
		TYPE_SEALED    = 0x00000200,
		TYPE_STATIC    = 0x00000300,
		TYPE_IMPL      = 0x00001000,
		TYPE_PRIMITIVE = 0x00002200,
	};

	enum Operator : uint32_t
	{
		OP_ADD         =  0, // +
		OP_SUBTRACT    =  1, // -
		OP_OR          =  2, // |
		OP_XOR         =  3, // ^
		OP_MULTIPLY    =  4, // *
		OP_DIVIDE      =  5, // /
		OP_MODULO      =  6, // %
		OP_AND         =  7, // &
		OP_POWER       =  8, // **
		OP_SHIFT_LEFT  =  9, // <<
		OP_SHIFT_RIGHT = 10, // >>
		OP_PLUS        = 11, // +x
		OP_NEGATE      = 12, // -x
		OP_NOT         = 13, // ~
		OP_EQUALS      = 14, // ==
		OP_COMPARE     = 15, // <=>
	};

	enum FieldFlags : uint32_t
	{
		FIELD_PUBLIC    = 0x00000001,
		FIELD_INTERNAL  = 0x00000002,
		FIELD_PROTECTED = 0x00000004,
		FIELD_PRIVATE   = 0x00000008,
		FIELD_INSTANCE  = 0x00000100,
		FIELD_HAS_VALUE = 0x00000200,
		FIELD_IMPL      = 0x00001000,
	};

	enum MethodFlags : uint32_t
	{
		METHOD_PUBLIC    = 0x00000001,
		METHOD_INTERNAL  = 0x00000002,
		METHOD_PROTECTED = 0x00000004,
		METHOD_PRIVATE   = 0x00000008,
		METHOD_INSTANCE  = 0x00000100,
		METHOD_CTOR      = 0x00000200,
		METHOD_IMPL      = 0x00001000,
	};

	enum OverloadFlags : uint32_t
	{
		OVERLOAD_VARIADIC     = 0x00000001,
		OVERLOAD_VIRTUAL      = 0x00000100,
		OVERLOAD_ABSTRACT     = 0x00000200,
		OVERLOAD_OVERRIDE     = 0x00000400,
		OVERLOAD_NATIVE       = 0x00001000,
		OVERLOAD_SHORT_HEADER = 0x00002000,
	};

	enum ParamFlags : uint32_t
	{
		PARAM_BY_REF   = 0x00000001,
		PARAM_OPTIONAL = 0x00000002,
	};

	enum ConstantFlags : uint32_t
	{
		CONSTANT_PUBLIC   = 0x00000001,
		CONSTANT_INTERNAL = 0x00000002,
	};

	enum TryKind : uint32_t
	{
		TRY_CATCH   = 0x00000001,
		TRY_FINALLY = 0x00000002,
		TRY_FAULT   = 0x00000003,
	};

	union MagicNumber
	{
		char chars[4];
		uint32_t number;
	};

	struct ModuleVersion
	{
		uint32_t major;
		uint32_t minor;
		uint32_t patch;

		inline bool Equals(const ModuleVersion &other) const
		{
			return
				this->major == other.major &&
				this->minor == other.minor &&
				this->patch == other.patch;
		}

		inline bool Equals(const ModuleVersion *other) const
		{
			return this->Equals(*other);
		}

		inline int CompareTo(const ModuleVersion &other) const
		{
			if (this->major != other.major)
				return this->major < other.major ? -1 : 1;

			if (this->minor != other.minor)
				return this->minor < other.minor ? -1 : 1;

			if (this->patch != other.patch)
				return this->patch < other.patch ? -1 : 1;

			return 0;
		}

		inline int CompareTo(const ModuleVersion *other) const
		{
			return this->CompareTo(*other);
		}
	};

	struct ModuleHeader
	{
		MagicNumber magic;
		uint32_t formatVersion;

		char padding_[8];

		ModuleVersion version;
		Rva<WideString> name;

		Rva<StringTableHeader> strings;

		Rva<WideString> nativeLib;
		Rva<RefTableHeader> references;

		Rva<StringMapHeader> metadata;
		Token mainMethod;

		int32_t typeCount;
		Rva<TypeDef[]> types;

		int32_t fieldCount;
		Rva<FieldDef[]> fields;

		int32_t methodCount;
		Rva<MethodDef[]> methods;

		int32_t functionCount;
		Rva<MethodDef[]> functions;

		int32_t constantCount;
		Rva<ConstantDef[]> constants;

		Rva<Annotations> annotations;
	};

	struct WideString
	{
		int32_t length;
		InlineArray<ovchar_t> chars;
	};

	struct ByteString
	{
		int32_t length;
		InlineArray<char> chars;
	};

	struct StringTableHeader
	{
		int32_t length;
		InlineArray<Rva<WideString>> strings;
	};

	struct RefTableHeader
	{
		int32_t moduleRefCount;
		Rva<ModuleRef[]> moduleRefs;

		int32_t typeRefCount;
		Rva<TypeRef[]> typeRefs;

		int32_t fieldRefCount;
		Rva<FieldRef[]> fieldRefs;

		int32_t methodRefCount;
		Rva<MethodRef[]> methodRefs;

		int32_t functionRefCount;
		Rva<FunctionRef[]> functionRefs;
	};

	struct ModuleRef
	{
		Token name;
		VersionConstraint versionConstraint;
		ModuleVersion version;
	};

	struct TypeRef
	{
		Token declModule;
		TypeRefFlags flags;
		Token name;
	};

	struct FieldRef
	{
		Token declType;
		FieldRefFlags flags;
		Token name;
	};

	struct MethodRef
	{
		Token declType;
		MethodRefFlags flags;
		Token name;
	};

	struct FunctionRef
	{
		Token declModule;
		FunctionRefFlags flags;
		Token name;
	};

	struct StringMapEntry
	{
		Rva<WideString> key;
		Rva<WideString> value;
	};

	struct StringMapHeader
	{
		int32_t length;
		InlineArray<StringMapEntry> entries;
	};

	struct TypeDef
	{
		TypeFlags flags;
		Token name;

		Token baseType;
		Token sharedType;
		Rva<Annotations> annotations;
		Rva<ByteString> initer;

		int32_t fieldCount;
		Token firstField;

		int32_t methodCount;
		Token firstMethod;

		int32_t propertyCount;
		Rva<PropertyDef[]> properties;

		int32_t operatorCount;
		Rva<OperatorDef[]> operators;
	};

	struct PropertyDef
	{
		Token name;
		Token getter;
		Token setter;
	};

	struct OperatorDef
	{
		Operator op;
		Token method;
	};

	struct FieldDef
	{
		FieldFlags flags;
		Token name;
		Token declType;
		Rva<Annotations> annotations;
		Rva<ConstantValue> value;
	};

	struct MethodDef
	{
		MethodFlags flags;
		Token name;
		Token declType;
		int32_t overloadCount;
		Rva<OverloadDef[]> overloads;
	};

	struct OverloadDef
	{
		OverloadFlags flags;
		Rva<Annotations> annotations;

		int32_t paramCount;
		Rva<Parameter[]> params;
		union
		{
			Rva<MethodBody> shortHeader;
			Rva<MethodHeader> longHeader;
			Rva<NativeMethodHeader> nativeHeader;
		} h;
	};

	struct Parameter
	{
		ParamFlags flags;
		Token name;
	};

	struct ConstantValue
	{
		Token type;
		char padding_[4];
		union
		{
			unsigned char rawValue[8];
			uint64_t uintValue;
			Token stringValue;
		} v;
	};

	struct ConstantDef
	{
		ConstantFlags flags;
		Token name;
		Rva<Annotations> annotations;

		Rva<ConstantValue> value;
	};

	struct MethodBody
	{
		uint32_t size;
		InlineArray<uint8_t> data;
	};

	struct MethodHeader
	{
		uint32_t localCount;
		uint32_t maxStack;
		int32_t tryBlockCount;
		Rva<TryBlock[]> tryBlocks;
		MethodBody body;
	};

	struct CatchClauses
	{
		int32_t count;
		Rva<CatchClause[]> clauses;
	};

	struct FinallyClause
	{
		uint32_t finallyStart;
		uint32_t finallyEnd;
	};

	struct TryBlock
	{
		TryKind kind;
		uint32_t tryStart;
		uint32_t tryEnd;

		union
		{
			CatchClauses catchClauses;
			FinallyClause finallyClause;
		} m;
	};

	struct CatchClause
	{
		Token caughtType;
		uint32_t catchStart;
		uint32_t catchEnd;
	};

	struct NativeMethodHeader
	{
		uint32_t localCount;
		ByteString entryPointName;
	};

	struct Annotation
	{
		Token type;

		int32_t positionalCount;
		Rva<AnnotationArgument[]> positionalArguments;

		int32_t namedCount;
		Rva<NamedAnnotationArgument[]> namedArguments;
	};

	struct Annotations
	{
		int32_t count;
		InlineArray<Annotation> members;
	};

	struct AnnotationArgumentList
	{
		int32_t length;
		Rva<AnnotationArgument[]> items;
	};

	struct AnnotationArgument
	{
		Token type;
		char padding_[4];
		union
		{
			unsigned char rawValue[8];
			uint64_t uintValue;
			Token stringValue;
			AnnotationArgumentList listValue;
			Token typeValue;
		} v;
	};

	struct NamedAnnotationArgument
	{
		Token member;
		char padding_[4];
		AnnotationArgument value;
	};

	// It is vital that the struct sizes are correct

	static_assert(sizeof(WideString) >= 4,               "Wrong size: WideString");
	static_assert(sizeof(ByteString) >= 4,               "Wrong size: ByteString");
	static_assert(sizeof(ModuleHeader) == 96,            "Wrong size: ModuleHeader");
	static_assert(sizeof(ModuleVersion) == 12,           "Wrong size: ModuleVersion");
	static_assert(sizeof(StringTableHeader) >= 4,        "Wrong size: StringTableHeader");
	static_assert(sizeof(StringMapHeader) >= 4,          "Wrong size: StringMapHeader");
	static_assert(sizeof(StringMapEntry) == 8,           "Wrong size: StringMapEntry");
	static_assert(sizeof(RefTableHeader) == 40,          "Wrong size: RefTableHeader");
	static_assert(sizeof(ModuleRef) == 20,               "Wrong size: ModuleRef");
	static_assert(sizeof(TypeRef) == 12,                 "Wrong size: TypeRef");
	static_assert(sizeof(FieldRef) == 12,                "Wrong size: FieldRef");
	static_assert(sizeof(MethodRef) == 12,               "Wrong size: MethodRef");
	static_assert(sizeof(FunctionRef) == 12,             "Wrong size: FunctionRef");
	static_assert(sizeof(TypeDef) == 56,                 "Wrong size: TypeDef");
	static_assert(sizeof(FieldDef) == 20,                "Wrong size: FieldDef");
	static_assert(sizeof(PropertyDef) == 12,             "Wrong size: PropertyDef");
	static_assert(sizeof(OperatorDef) == 8,              "Wrong size: OperatorDef");
	static_assert(sizeof(MethodDef) == 20,               "Wrong size: MethodDef");
	static_assert(sizeof(OverloadDef) == 20,             "Wrong size: OverloadDef");
	static_assert(sizeof(Parameter) == 8,                "Wrong size: Parameter");
	static_assert(sizeof(MethodHeader) >= 20,            "Wrong size: MethodHeader");
	static_assert(sizeof(MethodBody) >= 4,               "Wrong size: MethodBody");
	static_assert(sizeof(NativeMethodHeader) >= 8,       "Wrong size: NativeMethodHeader");
	static_assert(sizeof(TryBlock) == 20,                "Wrong size: TryBlock");
	static_assert(sizeof(CatchClauses) == 8,             "Wrong size: CatchClauses");
	static_assert(sizeof(CatchClause) == 12,             "Wrong size: CatchClause");
	static_assert(sizeof(FinallyClause) == 8,            "Wrong size: FinallyClause");
	static_assert(sizeof(ConstantDef) == 16,             "Wrong size: ConstantDef");
	static_assert(sizeof(ConstantValue) == 16,           "Wrong size: ConstantValue");
	static_assert(sizeof(Annotations) >= 24,             "Wrong size: Annotations");
	static_assert(sizeof(Annotation) == 20,              "Wrong size: Annotation");
	static_assert(sizeof(AnnotationArgument) == 16,      "Wrong size: AnnotationArgument");
	static_assert(sizeof(AnnotationArgumentList) == 8,   "Wrong size: AnnotationArgumentList");
	static_assert(sizeof(NamedAnnotationArgument) == 24, "Wrong size: NamedAnnotationArgument");

} // namespace module_file

} // namespace ovum
