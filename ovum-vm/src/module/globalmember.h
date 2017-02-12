#pragma once

#include "../vm.h"
#include "../../inc/ovum_module.h"

namespace ovum
{

// MAKE SURE TO SYNCHRONIZE WITH PUBLIC VALUES.
// See inc/ovum_module.h

enum class GlobalMemberFlags : uint32_t
{
	NONE          = 0x0000,

	// Mask for extracting the accessibility of a member (public or internal).
	ACCESSIBILITY = 0x00ff,

	PUBLIC        = 0x0001,
	INTERNAL      = 0x0002,

	// Mask for extracting the kind of member (type, function or constant).
	KIND_MASK     = 0x0f00,

	// The member is a type (Type*).
	TYPE          = 0x0100,
	// The member is a global function (Method*).
	FUNCTION      = 0x0200,
	// The member is a global constant (Value).
	CONSTANT      = 0x0400,
};
OVUM_ENUM_OPS(GlobalMemberFlags, uint32_t);

class GlobalMember
{
public:
	// Default constructor for use in arrays and similar
	inline GlobalMember() :
		flags(GlobalMemberFlags::NONE),
		name(nullptr)
	{ }

	inline bool IsPublic() const
	{
		return (flags & GlobalMemberFlags::ACCESSIBILITY) == GlobalMemberFlags::PUBLIC;
	}

	inline bool IsInternal() const
	{
		return (flags & GlobalMemberFlags::ACCESSIBILITY) == GlobalMemberFlags::INTERNAL;
	}

	inline bool IsType() const
	{
		return (flags & GlobalMemberFlags::KIND_MASK) == GlobalMemberFlags::TYPE;
	}

	inline bool IsFunction() const
	{
		return (flags & GlobalMemberFlags::KIND_MASK) == GlobalMemberFlags::FUNCTION;
	}

	inline bool IsConstant() const
	{
		return (flags & GlobalMemberFlags::KIND_MASK) == GlobalMemberFlags::CONSTANT;
	}

	inline String *GetName() const
	{
		return name;
	}

	inline Type *GetType() const
	{
		if (!IsType())
			return nullptr;
		return m.type;
	}

	inline Method *GetFunction() const
	{
		if (!IsFunction())
			return nullptr;
		return m.function;
	}

	inline const Value *GetConstant() const
	{
		if (!IsConstant())
			return nullptr;
		return &m.constant;
	}

	void ToPublicGlobalMember(::GlobalMember *result) const;

	static GlobalMember FromType(Type *type);

	static GlobalMember FromFunction(Method *function);

	static GlobalMember FromConstant(String *name, Value *value, bool isInternal);

private:
	GlobalMemberFlags flags;

	// Fully qualified name of the member.
	String *name;

	union
	{
		Type *type;
		Method *function;
		Value constant;
	} m;

	inline GlobalMember(GlobalMemberFlags kind, GlobalMemberFlags accessibility, String *name) :
		flags(kind | accessibility),
		name(name)
	{ }
};

} // namespace ovum
