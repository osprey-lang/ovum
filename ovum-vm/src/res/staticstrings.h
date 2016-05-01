#pragma once

/*********************************************************/
/*                                                       */
/*                 DO NOT EDIT THIS FILE                 */
/*              THIS FILE IS AUTO-GENERATED              */
/*                                                       */
/*  To change this file, edit staticstrings.json and/or  */
/*  staticstrigs.template.h, and run staticstrings.py    */
/*                                                       */
/*********************************************************/

#include "../vm.h"

namespace ovum
{

struct StaticStringData
{
	LitString<0> empty;
	LitString<5> members_call_;
	LitString<5> members_init_;
	LitString<5> members_item_;
	LitString<5> members_iter_;
	LitString<7> members_message;
	LitString<4> members_new_;
	LitString<8> members_toString;
	LitString<12> types_aves_Boolean;
	LitString<22> types_aves_DivideByZeroError;
	LitString<9> types_aves_Enum;
	LitString<10> types_aves_Error;
	LitString<9> types_aves_Hash;
	LitString<8> types_aves_Int;
	LitString<13> types_aves_Iterator;
	LitString<9> types_aves_List;
	LitString<24> types_aves_MemberNotFoundError;
	LitString<16> types_aves_MemoryError;
	LitString<11> types_aves_Method;
	LitString<20> types_aves_NoOverloadError;
	LitString<23> types_aves_NullReferenceError;
	LitString<11> types_aves_Object;
	LitString<18> types_aves_OverflowError;
	LitString<9> types_aves_Real;
	LitString<11> types_aves_String;
	LitString<24> types_aves_TypeConversionError;
	LitString<14> types_aves_TypeError;
	LitString<9> types_aves_UInt;
	LitString<20> types_aves_reflection_Type;
	LitString<1> operators_add;
	LitString<1> operators_subtract;
	LitString<1> operators_or;
	LitString<1> operators_xor;
	LitString<1> operators_multiply;
	LitString<1> operators_divide;
	LitString<1> operators_modulo;
	LitString<1> operators_and;
	LitString<2> operators_power;
	LitString<2> operators_shiftLeft;
	LitString<2> operators_shiftRight;
	LitString<1> operators_hash;
	LitString<1> operators_dollar;
	LitString<1> operators_plus;
	LitString<1> operators_negate;
	LitString<1> operators_not;
	LitString<2> operators_equal;
	LitString<3> operators_compare;
	LitString<50> error_CannotAccessStaticMemberThroughInstance;
	LitString<26> error_CannotAssignToMethod;
	LitString<33> error_CannotGetWriteOnlyProperty;
	LitString<38> error_CannotSetReadOnlyProperty;
	LitString<43> error_CompareOperatorWrongReturnType;
	LitString<93> error_IncorrectRefness;
	LitString<71> error_IndexerNotFound;
	LitString<36> error_MemberIsNotAField;
	LitString<30> error_MemberNotFound;
	LitString<28> error_MemberNotInvokable;
	LitString<42> error_ObjectTooLarge;
	LitString<43> error_ToIntFailed;
	LitString<43> error_ToRealFailed;
	LitString<50> error_ToStringWrongReturnType;
	LitString<43> error_ToUIntFailed;
	LitString<28> error_ValueNotComparable;
	LitString<27> error_ValueNotInvokable;
	LitString<71> error_WrongApplyArgumentsType;
};

class StaticStrings
{
public:
	OVUM_NOINLINE static int Create(StaticStrings *&result);
	~StaticStrings();

	::String *empty;
	struct membersStrings {
		::String *call_;
		::String *init_;
		::String *item_;
		::String *iter_;
		::String *message;
		::String *new_;
		::String *toString;
	} members;
	struct typesStrings {
		struct avesStrings {
			::String *Boolean;
			::String *DivideByZeroError;
			::String *Enum;
			::String *Error;
			::String *Hash;
			::String *Int;
			::String *Iterator;
			::String *List;
			::String *MemberNotFoundError;
			::String *MemoryError;
			::String *Method;
			::String *NoOverloadError;
			::String *NullReferenceError;
			::String *Object;
			::String *OverflowError;
			::String *Real;
			::String *String;
			::String *TypeConversionError;
			::String *TypeError;
			::String *UInt;
			struct reflectionStrings {
				::String *Type;
			} reflection;
		} aves;
	} types;
	struct operatorsStrings {
		::String *add;
		::String *subtract;
		::String *or;
		::String *xor;
		::String *multiply;
		::String *divide;
		::String *modulo;
		::String *and;
		::String *power;
		::String *shiftLeft;
		::String *shiftRight;
		::String *hash;
		::String *dollar;
		::String *plus;
		::String *negate;
		::String *not;
		::String *equal;
		::String *compare;
	} operators;
	struct errorStrings {
		::String *CannotAccessStaticMemberThroughInstance;
		::String *CannotAssignToMethod;
		::String *CannotGetWriteOnlyProperty;
		::String *CannotSetReadOnlyProperty;
		::String *CompareOperatorWrongReturnType;
		::String *IncorrectRefness;
		::String *IndexerNotFound;
		::String *MemberIsNotAField;
		::String *MemberNotFound;
		::String *MemberNotInvokable;
		::String *ObjectTooLarge;
		::String *ToIntFailed;
		::String *ToRealFailed;
		::String *ToStringWrongReturnType;
		::String *ToUIntFailed;
		::String *ValueNotComparable;
		::String *ValueNotInvokable;
		::String *WrongApplyArgumentsType;
	} error;

private:
	StaticStringData *data;

	StaticStrings();

	void InitData();
	void InitStrings();
};

} // namespace ovum
