#include "method.h"
#include "../ee/refsignature.h"
#include "../module/module.h"
#include "../ee/thread.h"

namespace ovum
{

int32_t MethodOverload::GetLocalOffset(ovlocals_t local) const
{
	return (int32_t)(STACK_FRAME_SIZE + local * sizeof(Value));
}

int32_t MethodOverload::GetStackOffset(ovlocals_t stackSlot) const
{
	return (int32_t)(STACK_FRAME_SIZE + (locals + stackSlot) * sizeof(Value));
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

	ovlocals_t im = 1; // index into methodSignature
	ovlocals_t ia = 1; // and into argSignature

	ovlocals_t paramCount = this->GetEffectiveParamCount();
	if (this->IsVariadic())
	{
		// Test each required parameter against its argument
		while (im < paramCount - 1)
		{
			if (methodSignature.IsParamRef(im) != argSignature.IsParamRef(ia))
				return ia;
			im++;
			ia++;
		}
		// And then make sure every remaining argument is not by ref;
		// these will be packed into a list
		while (ia < argCount)
		{
			if (argSignature.IsParamRef(ia))
				return ia;
			ia++;
		}
	}
	else
	{
		// Test each parameter against its corresponding argument
		while (im < paramCount)
		{
			if (methodSignature.IsParamRef(im) != argSignature.IsParamRef(ia))
				return ia;
			im++;
			ia++;
		}
	}
	return -1;
}

inline bool Method::Accepts(ovlocals_t argCount) const
{
	const Method *m = this;
	do
	{
		for (int i = 0; i < m->overloadCount; i++)
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
		for (int i = 0; i < method->overloadCount; i++)
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
	for (int i = 0; i < overloadCount; i++)
		this->overloads[i].declType = type;
}

} // namespace ovum


OVUM_API bool Method_IsConstructor(MethodHandle method)
{
	return (method->flags & ovum::MemberFlags::CTOR) == ovum::MemberFlags::CTOR;
}
OVUM_API int32_t Method_GetOverloadCount(MethodHandle method)
{
	return method->overloadCount;
}
OVUM_API OverloadHandle Method_GetOverload(MethodHandle method, int32_t index)
{
	if (index < 0 || index >= method->overloadCount)
		return nullptr;

	return method->overloads + index;
}
OVUM_API int32_t Method_GetOverloads(MethodHandle method, int32_t destSize, OverloadHandle *dest)
{
	int32_t count = method->overloadCount;
	if (count > destSize)
		count = destSize;

	for (int32_t i = 0; i < count; i++)
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
	if (argc < 0 || argc > UINT16_MAX)
		return nullptr;
	return method->ResolveOverload(argc);
}


OVUM_API MethodFlags Overload_GetFlags(OverloadHandle overload)
{
	return overload->flags;
}
OVUM_API int32_t Overload_GetParamCount(OverloadHandle overload)
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
OVUM_API int32_t Overload_GetAllParameters(OverloadHandle overload, ovlocals_t destSize, ParamInfo *dest)
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
