#include "method.h"
#include "../ee/refsignature.h"
#include "../module/module.h"
#include "../ee/thread.h"

namespace ovum
{

LocalOffset MethodOverload::GetArgumentOffset(ovlocals_t arg) const
{
	return LocalOffset(static_cast<int32_t>(
		(arg - GetEffectiveParamCount()) * sizeof(Value)
	));
}

LocalOffset MethodOverload::GetLocalOffset(ovlocals_t local) const
{
	return LocalOffset(static_cast<int32_t>(
		STACK_FRAME_SIZE + local * sizeof(Value)
	));
}

LocalOffset MethodOverload::GetStackOffset(ovlocals_t stackSlot) const
{
	return LocalOffset(static_cast<int32_t>(
		STACK_FRAME_SIZE + (locals + stackSlot) * sizeof(Value)
	));
}

RefSignaturePool *MethodOverload::GetRefSignaturePool() const
{
	return group->declModule->GetVM()->GetRefSignaturePool();
}

int MethodOverload::VerifyRefSignature(uint32_t signature, ovlocals_t argCount) const
{
	RefSignaturePool *refSigPool = GetRefSignaturePool();
	RefSignature methodSignature(refSignature, refSigPool);
	RefSignature argSignature(signature, refSigPool);

	// Signatures always include extra space for the instance, even if the method
	// is static. Argument 0 should never be by ref.
	if (argSignature.IsParamRef(0))
		return 0;

	// Since we always reserve space for the instance, even if there isn't one, we start
	// numbering named parameters (i.e. anything that isn't 'this') at 1.
	ovlocals_t methodIndex = 1; // index into methodSignature
	ovlocals_t argIndex = 1; // and into argSignature

	// Don't use GetEffectiveParamCount, as the instance is already accounted for (if any).
	ovlocals_t paramsToCheck = this->paramCount;

	// When the method is variadic, the last parameter is verified separately, as it may
	// be represented by zero or more arguments.
	if (this->IsVariadic())
		paramsToCheck--;

	// Test each parameter against its corresponding argument. When an optional parameter
	// is missing from the argument list, IsParamRef will return false for it. Optional
	// parameters can never be passed by reference, so the refness will match.
	while (methodIndex <= paramsToCheck) // numbering from 1
	{
		if (methodSignature.IsParamRef(methodIndex) != argSignature.IsParamRef(argIndex))
			return argIndex;
		methodIndex++;
		argIndex++;
	}

	// If the method is variadic, all remaining arguments will be packed into a list.
	// These are not allowed to be passed by reference.
	if (this->IsVariadic())
	{
		while (argIndex <= argCount) // numbering from 1
		{
			if (argSignature.IsParamRef(argIndex))
				return argIndex;
			argIndex++;
		}
	}

	// No mismatches
	return -1;
}

inline bool Method::Accepts(ovlocals_t argCount) const
{
	const Method *m = this;
	do
	{
		for (size_t i = 0; i < m->overloadCount; i++)
			if (m->overloads[i].Accepts(argCount))
				return true;
	} while (m = m->baseMethod);
	return false;
}

MethodOverload *Method::ResolveOverload(ovlocals_t argCount) const
{
	const Method *method = this;
	do
	{
		for (size_t i = 0; i < method->overloadCount; i++)
		{
			MethodOverload *mo = method->overloads + i;
			if (mo->Accepts(argCount))
				return mo;
		}
	} while (method = method->baseMethod);
	return nullptr;
}

void Method::SetDeclType(Type *type)
{
	this->declType = type;
	for (size_t i = 0; i < overloadCount; i++)
		this->overloads[i].declType = type;
}

} // namespace ovum


OVUM_API bool Method_IsConstructor(MethodHandle method)
{
	return method->IsCtor();
}
OVUM_API size_t Method_GetOverloadCount(MethodHandle method)
{
	return method->overloadCount;
}
OVUM_API OverloadHandle Method_GetOverload(MethodHandle method, size_t index)
{
	if (index >= method->overloadCount)
		return nullptr;

	return method->overloads + index;
}
OVUM_API size_t Method_GetOverloads(MethodHandle method, size_t destSize, OverloadHandle *dest)
{
	size_t count = method->overloadCount;
	if (count > destSize)
		count = destSize;

	for (size_t i = 0; i < count; i++)
		dest[i] = method->overloads + i;

	return count;
}
OVUM_API MethodHandle Method_GetBaseMethod(MethodHandle method)
{
	return method->baseMethod;
}

OVUM_API bool Method_Accepts(MethodHandle method, ovlocals_t argc)
{
	return method->Accepts(argc);
}
OVUM_API OverloadHandle Method_FindOverload(MethodHandle method, ovlocals_t argc)
{
	return method->ResolveOverload(argc);
}


OVUM_API uint32_t Overload_GetFlags(OverloadHandle overload)
{
	return static_cast<uint32_t>(overload->flags & ovum::OverloadFlags::VISIBLE_MASK);
}
OVUM_API ovlocals_t Overload_GetParamCount(OverloadHandle overload)
{
	return overload->paramCount;
}
OVUM_API bool Overload_GetParameter(OverloadHandle overload, ovlocals_t index, ParamInfo *dest)
{
	if (index < 0 || index >= overload->paramCount)
		return false;

	dest->name = overload->paramNames[index];

	dest->isOptional = index >= overload->paramCount - overload->optionalParamCount;
	// Only the last parameter of a variadic overload is variadic.
	if (overload->IsVariadic())
		dest->isVariadic = index == overload->paramCount - 1;
	else
		dest->isVariadic = false;
	ovum::RefSignature refs(overload->refSignature, overload->GetRefSignaturePool());
	// +1 because the reference signature always reserves the first
	// slot for the instance, even if the method is static.
	dest->isByRef = refs.IsParamRef(index + 1);

	return true;
}
OVUM_API ovlocals_t Overload_GetAllParameters(OverloadHandle overload, ovlocals_t destSize, ParamInfo *dest)
{
	ovlocals_t count = overload->paramCount;
	if (count > destSize)
		count = destSize;

	bool isVariadic = overload->IsVariadic();

	ovum::RefSignature refs(overload->refSignature, overload->GetRefSignaturePool());
	for (ovlocals_t i = 0; i < count; i++)
	{
		ParamInfo *pi = dest + i;
		pi->name = overload->paramNames[i];

		pi->isOptional = i >= count - overload->optionalParamCount;
		// Only the last parameter of a variadic overload is variadic.
		if (isVariadic)
			pi->isVariadic = i == count - 1;
		else
			pi->isVariadic = false;
		// +1 because the reference signature always reserves the first
		// slot for the instance, even if this method is static.
		pi->isByRef = refs.IsParamRef(i + 1);
	}

	return count;
}
OVUM_API MethodHandle Overload_GetMethod(OverloadHandle overload)
{
	return overload->group;
}
