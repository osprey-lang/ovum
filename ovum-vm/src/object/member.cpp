#include "member.h"
#include "field.h"
#include "property.h"
#include "method.h"
#include "type.h"

namespace ovum
{

Member::Member(String *name, Module *declModule, MemberFlags flags) :
	name(name), declType(nullptr),
	declModule(declModule), flags(flags)
{ }

Member::Member(String *name, Type *declType, MemberFlags flags) :
	name(name), declType(declType),
	declModule(declType->module), flags(flags)
{ }

// Determines whether a member is accessible from a given type.
//   instType:
//     The type of the instance that the member is being loaded from.
//   fromType:
//     The type which declares the method that is accessing the member.
//     This is null for global functions.
bool Member::IsAccessible(const Type *instType, const Type *fromType) const
{
	if ((this->flags & MemberFlags::PRIVATE) != MemberFlags::NONE)
		return fromType && (declType == fromType || declType == fromType->sharedType);

	if ((this->flags & MemberFlags::PROTECTED) != MemberFlags::NONE)
	{
		if (!fromType)
			return false;

		return fromType->sharedType ?
			IsAccessibleProtectedWithSharedType(instType, fromType) :
			IsAccessibleProtected(instType, fromType);
	}

	return true; // MemberFlags::PUBLIC or accessible
}

bool Member::IsAccessibleProtected(const Type *instType, const Type *fromType) const
{
	while (instType && instType != fromType)
		instType = instType->baseType;

	if (!instType)
		return false; // instType does not inherit from fromType

	Type *originatingType = GetOriginatingType();
	while (fromType && fromType != originatingType)
		fromType = fromType->baseType;

	if (!fromType)
		return false; // fromType does not inherit from originatingType

	return true; // yay
}

bool Member::IsAccessibleProtectedWithSharedType(const Type *instType, const Type *fromType) const
{
	const Type *tempType = instType;
	while (tempType && tempType != fromType)
		tempType = tempType->baseType;

	if (!tempType)
	{
		const Type *sharedType = fromType->sharedType;
		while (instType && instType != sharedType)
			instType = instType->baseType;

		if (!instType)
			return false; // instType does not inherit from fromType or fromType->sharedType
	}

	Type *originatingType = GetOriginatingType();
	tempType = fromType;
	while (tempType && tempType != originatingType)
		tempType = tempType->baseType;

	if (!tempType)
	{
		const Type *sharedType = fromType->sharedType;
		while (sharedType && sharedType != originatingType)
			sharedType = sharedType->baseType;

		if (!sharedType)
			return false; // neither fromType nor fromType->sharedType inherits from originatingType
	}

	return true;
}

Type *Member::GetOriginatingType() const
{
	OVUM_ASSERT((flags & MemberFlags::ACCESS_LEVEL) == MemberFlags::PROTECTED);
	const Method *method = nullptr;

	if ((flags & MemberFlags::KIND) == MemberFlags::METHOD)
		method = static_cast<const Method*>(this);
	else if ((flags & MemberFlags::KIND) == MemberFlags::PROPERTY)
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
	switch (member->flags & MemberFlags::KIND)
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
	switch (member->flags & MemberFlags::ACCESS_LEVEL)
	{
	case MemberFlags::PUBLIC:
		return MemberAccess::PUBLIC;
	case MemberFlags::PRIVATE:
		return MemberAccess::PRIVATE;
	case MemberFlags::PROTECTED:
		return MemberAccess::PROTECTED;
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
OVUM_API bool Member_IsAccessible(MemberHandle member, TypeHandle instType, TypeHandle fromType)
{
	return member->IsAccessible(instType, fromType);
}

OVUM_API MethodHandle Member_ToMethod(MemberHandle member)
{
	if ((member->flags & ovum::MemberFlags::METHOD) == ovum::MemberFlags::METHOD)
		return (MethodHandle)member;
	return nullptr;
}
OVUM_API FieldHandle Member_ToField(MemberHandle member)
{
	if ((member->flags & ovum::MemberFlags::FIELD) == ovum::MemberFlags::FIELD)
		return (FieldHandle)member;
	return nullptr;
}
OVUM_API PropertyHandle Member_ToProperty(MemberHandle member)
{
	if ((member->flags & ovum::MemberFlags::PROPERTY) == ovum::MemberFlags::PROPERTY)
		return (PropertyHandle)member;
	return nullptr;
}
