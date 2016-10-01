#pragma once

#include "../vm.h"

namespace ovum
{

enum class MemberFlags : uint32_t
{
	NONE      = 0x0000,

	// The member is a field.
	FIELD     = 0x0001,
	// The member is a method.
	METHOD    = 0x0002,
	// The member is a property.
	PROPERTY  = 0x0004,

	// The member is public.
	PUBLIC    = 0x0010,
	// The member is protected.
	PROTECTED = 0x0020,
	// The member is private.
	PRIVATE   = 0x0040,

	// The member is a constructor.
	CTOR      = 0x0100,

	// The member is an instance member.
	INSTANCE  = 0x0400,

	// The member is used internally to implement some behaviour.
	// Primarily used by getters, setters, iterator accessors and
	// operator overloads.
	IMPL      = 0x0800,

	// A mask for extracting the access level of a member.
	ACCESS_LEVEL = 0x00f0,
	// A mask for extracting the kind of a member.
	KIND = 0x000f,
};
OVUM_ENUM_OPS(MemberFlags, uint32_t);

class Member
{
public:
	MemberFlags flags;

	String *name;
	Type *declType;
	Module *declModule;

	Member(String *name, Type *declType, MemberFlags flags);
	Member(String *name, Module *declModule, MemberFlags flags);

	inline virtual ~Member() { }

	inline bool IsField() const
	{
		return (flags & MemberFlags::FIELD) == MemberFlags::FIELD;
	}

	inline bool IsMethod() const
	{
		return (flags & MemberFlags::METHOD) == MemberFlags::METHOD;
	}

	inline bool IsProperty() const
	{
		return (flags & MemberFlags::PROPERTY) == MemberFlags::PROPERTY;
	}

	inline bool IsStatic() const
	{
		return (flags & MemberFlags::INSTANCE) == MemberFlags::NONE;
	}

	// Determines whether a member is accessible from a given type.
	//   instType:
	//     The type of the instance that the member is being loaded from.
	//   fromType:
	//     The type which declares the method that is accessing the member.
	//     This is null for global functions.
	bool IsAccessible(const Type *instType, const Type *fromType) const;

private:
	// Gets the type that originally declared the member.
	// For virtual (overridable) protected methods, this is
	// the type that introduced the method. E.g.:
	//    class A {
	//        protected overridable f(); // introduces f
	//    }
	//    class B is A {
	//        override f(); // overrides A.f; originating type = A
	//    }
	Type *GetOriginatingType() const;

	bool IsAccessibleProtected(const Type *instType, const Type *fromType) const;
	bool IsAccessibleProtectedWithSharedType(const Type *instType, const Type *fromType) const;
};

} // namespace ovum
