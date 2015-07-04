#ifndef VM__PROPERTY_H
#define VM__PROPERTY_H

#include "../vm.h"
#include "member.h"

namespace ovum
{

class Property : public Member
{
public:
	Method *getter;
	Method *setter;

	inline Property(String *name, Type *declType, MemberFlags flags) :
		Member(name, declType, flags | MemberFlags::PROPERTY)
	{ }
};

} // namespace ovum

#endif // VM__PROPERTY_H
