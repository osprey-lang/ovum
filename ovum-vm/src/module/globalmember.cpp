#include "globalmember.h"
#include "../object/type.h"
#include "../object/method.h"

namespace ovum
{

GlobalMember GlobalMember::FromType(Type *type)
{
	GlobalMember result(
		GlobalMemberFlags::TYPE,
		type->IsInternal() ? GlobalMemberFlags::INTERNAL : GlobalMemberFlags::PUBLIC,
		type->fullName
	);
	result.m.type = type;
	return std::move(result);
}

GlobalMember GlobalMember::FromFunction(Method *function)
{
	GlobalMember result(
		GlobalMemberFlags::FUNCTION,
		function->IsInternal() ? GlobalMemberFlags::INTERNAL : GlobalMemberFlags::PUBLIC,
		function->name
	);
	result.m.function = function;
	return std::move(result);
}

GlobalMember GlobalMember::FromConstant(String *name, Value *value, bool isInternal)
{
	GlobalMember result(
		GlobalMemberFlags::CONSTANT,
		isInternal ? GlobalMemberFlags::INTERNAL : GlobalMemberFlags::PUBLIC,
		name
	);
	result.m.constant = *value;
	return std::move(result);
}

void GlobalMember::ToPublicGlobalMember(::GlobalMember *result) const
{
	// GlobalMemberFlags are synchronized with publicly visible flags.
	result->flags = static_cast<uint32_t>(this->flags);
	result->name = this->name;
	switch (this->flags & GlobalMemberFlags::KIND_MASK)
	{
	case GlobalMemberFlags::TYPE:
		result->m.type = this->m.type;
		break;
	case GlobalMemberFlags::FUNCTION:
		result->m.function = this->m.function;
		break;
	case GlobalMemberFlags::CONSTANT:
		result->m.constant = this->m.constant;
		break;
	}
}

} // namespace ovum
