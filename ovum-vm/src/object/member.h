#pragma once

#include "../vm.h"

namespace ovum
{

// The MemberFlags enum represents a kind of combination of the various
// member flags found in the raw module format. This enum contains less
// information than the corresponding flags enums in the module format;
// that missing information is stored elsewhere in the member.
//
// The least significant byte is the member's accessibility, which is
// made deliberately to match the module format's values.
enum class MemberFlags : uint32_t
{
	NONE      = 0x0000,

	ACCESSIBILITY = 0x000000ff,
	// The member is public.
	PUBLIC        = 0x00000001,
	// The member is internal.
	INTERNAL      = 0x00000002,
	// The member is protected.
	PROTECTED     = 0x00000004,
	// The member is private.
	PRIVATE       = 0x00000008,

	KIND_MASK     = 0x00000f00,
	// The member is a field.
	FIELD         = 0x00000100,
	// The member is a method.
	METHOD        = 0x00000200,
	// The member is a property.
	PROPERTY      = 0x00000400,

	// The member is an instance member.
	INSTANCE      = 0x00001000,

	// The member is a constructor.
	CTOR          = 0x00002000,

	// The member is used internally to implement some behaviour.
	// Primarily used by getters, setters, iterator accessors and
	// operator overloads.
	IMPL          = 0x00004000,
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

	inline bool IsPublic() const
	{
		return (flags & MemberFlags::PUBLIC) == MemberFlags::PUBLIC;
	}

	inline bool IsInternal() const
	{
		return (flags & MemberFlags::INTERNAL) == MemberFlags::INTERNAL;
	}

	inline bool IsProtected() const
	{
		return (flags & MemberFlags::PROTECTED) == MemberFlags::PROTECTED;
	}

	inline bool IsPrivate() const
	{
		return (flags & MemberFlags::PRIVATE) == MemberFlags::PRIVATE;
	}

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

	inline bool IsCtor() const
	{
		return (flags & MemberFlags::CTOR) == MemberFlags::CTOR;
	}

	inline bool IsImpl() const
	{
		return (flags & MemberFlags::IMPL) == MemberFlags::IMPL;
	}

	// Determines whether a member is accessible from a given type.
	//   instType:
	//     The type of the instance that the member is being loaded from.
	//   fromMethod:
	//     The method in which the member access is occurring.
	bool IsAccessible(const Type *instType, const MethodOverload *fromMethod) const;

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
