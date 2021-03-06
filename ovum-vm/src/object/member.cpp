#include "member.h"
#include "field.h"
#include "property.h"
#include "method.h"
#include "type.h"

namespace ovum
{

Member::Member(String *name, Module *declModule, MemberFlags flags) :
	name(name),
	declType(nullptr),
	declModule(declModule),
	flags(flags)
{ }

Member::Member(String *name, Type *declType, MemberFlags flags) :
	name(name),
	declType(declType),
	declModule(declType->module),
	flags(flags)
{ }

// Determines whether a member is accessible from a given type.
//   instType:
//     The type of the instance that the member is being loaded from.
//   fromMethod:
//     The method in which the member access is occurring.
bool Member::IsAccessible(const Type *instType, const MethodOverload *fromMethod) const
{
	switch (this->flags & MemberFlags::ACCESSIBILITY)
	{
	case MemberFlags::PUBLIC:
		return true;
	case MemberFlags::INTERNAL:
		return this->declModule == fromMethod->group->declModule;
	case MemberFlags::PROTECTED:
		{
			Type *fromType = fromMethod ? fromMethod->declType : nullptr;
			if (!fromType)
				return false;

			return fromType->sharedType ?
				IsAccessibleProtectedWithSharedType(instType, fromType) :
				IsAccessibleProtected(instType, fromType);
		}
	case MemberFlags::PRIVATE:
		{
			Type *fromType = fromMethod ? fromMethod->declType : nullptr;
			return fromType && (declType == fromType || declType == fromType->sharedType);
		}
	default:
		return false;
	}
}

bool Member::IsAccessibleProtected(const Type *instType, const Type *fromType) const
{
	if (!Type::InheritsFrom(instType, fromType))
		return false; // instType does not inherit from fromType

	if (!Type::InheritsFrom(fromType, GetOriginatingType()))
		return false; // fromType does not inherit from originatingType

	return true; // yay
}

bool Member::IsAccessibleProtectedWithSharedType(const Type *instType, const Type *fromType) const
{
	if (!Type::InheritsFrom(instType, fromType) &&
		!Type::InheritsFrom(instType, fromType->sharedType))
		return false; // instType does not inherit from fromType or fromType->sharedType

	Type *originatingType = GetOriginatingType();
	if (!Type::InheritsFrom(fromType, originatingType) &&
		!Type::InheritsFrom(fromType->sharedType, originatingType))
		return false; // neither fromType nor fromType->sharedType inherits from originatingType

	return true;
}

Type *Member::GetOriginatingType() const
{
	OVUM_ASSERT(IsProtected());
	const Method *method = nullptr;

	if (IsMethod())
		method = static_cast<const Method*>(this);
	else if (IsProperty())
	{
		const Property *prop = static_cast<const Property*>(this);
		method = prop->getter ? prop->getter : prop->setter;
	}
	else // Field
		return declType;

	while (method->baseMethod)
		method = method->baseMethod;
	return method->declType;
}

} // namespace ovum

OVUM_API String *Member_GetName(MemberHandle member)
{
	return member->name;
}

OVUM_API MemberKind Member_GetKind(MemberHandle member)
{
	using namespace ovum;
	switch (member->flags & MemberFlags::KIND_MASK)
	{
	case MemberFlags::METHOD:   return MemberKind::METHOD;
	case MemberFlags::FIELD:    return MemberKind::FIELD;
	case MemberFlags::PROPERTY: return MemberKind::PROPERTY;
	default:                    return MemberKind::INVALID;
	}
}
OVUM_API MemberAccess Member_GetAccessLevel(MemberHandle member)
{
	using namespace ovum;
	switch (member->flags & MemberFlags::ACCESSIBILITY)
	{
	case MemberFlags::PUBLIC:
		return MemberAccess::PUBLIC;
	case MemberFlags::INTERNAL:
		// TODO: internal accessibility
		return MemberAccess::INVALID;
	case MemberFlags::PROTECTED:
		return MemberAccess::PROTECTED;
	case MemberFlags::PRIVATE:
		return MemberAccess::PRIVATE;
	default:
		return MemberAccess::INVALID;
	}
}
OVUM_API TypeHandle Member_GetDeclType(MemberHandle member)
{
	return member->declType;
}
OVUM_API ModuleHandle Member_GetDeclModule(MemberHandle member)
{
	return member->declModule;
}

OVUM_API bool Member_IsStatic(MemberHandle member)
{
	return member->IsStatic();
}
OVUM_API bool Member_IsImpl(MemberHandle member)
{
	return (member->flags & ovum::MemberFlags::IMPL) == ovum::MemberFlags::IMPL;
}
OVUM_API bool Member_IsAccessible(MemberHandle member, TypeHandle instType, OverloadHandle fromMethod)
{
	return member->IsAccessible(instType, fromMethod);
}

OVUM_API MethodHandle Member_ToMethod(MemberHandle member)
{
	if (member->IsMethod())
		return (MethodHandle)member;
	return nullptr;
}
OVUM_API FieldHandle Member_ToField(MemberHandle member)
{
	if (member->IsField())
		return (FieldHandle)member;
	return nullptr;
}
OVUM_API PropertyHandle Member_ToProperty(MemberHandle member)
{
	if (member->IsProperty())
		return (PropertyHandle)member;
	return nullptr;
}
