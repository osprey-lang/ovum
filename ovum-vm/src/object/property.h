#pragma once

#include "../vm.h"
#include "member.h"

namespace ovum
{

class Property : public Member
{
public:
	Method *getter;
	Method *setter;

	// Getters and setters without additional arguments.
	// For the getter, this means the parameterless overload (plusminus instance).
	// For the setter, the overload that accepts only one argument (plusminus instance).
	// These are used by various methods that load and store members, for performance
	// reasons. Method::ResolveOverload() is not exactly slow, but it's nice to avoid
	// calling it all the time.

	MethodOverload *defaultGetter;
	MethodOverload *defaultSetter;

	inline Property(String *name, Type *declType, MemberFlags flags) :
		Member(name, declType, flags | MemberFlags::PROPERTY)
	{ }
};

} // namespace ovum
