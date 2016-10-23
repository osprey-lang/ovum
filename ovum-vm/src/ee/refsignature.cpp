#include "refsignature.h"

namespace ovum
{

RefSignature::RefSignature(uint32_t mask, RefSignaturePool *pool)
{
	if (mask & SignatureKindMask)
	{
		const LongRefSignature *signature = pool->Get(mask & SignatureDataMask);
		paramCount = signature->paramCount;
		longMask = signature->maskValues.get();
	}
	else
	{
		paramCount = MaxShortParamCount;
		shortMask = mask & SignatureDataMask;
	}
}

bool RefSignature::IsParamRef(ovlocals_t index) const
{
	if (paramCount > MaxShortParamCount)
	{
		uint32_t mask = longMask[index / LongRefSignature::ParamsPerMask];
		return ((mask >> index % LongRefSignature::ParamsPerMask) & 1) == 1;
	}
	else
	{
		if (index > MaxShortParamCount)
			return false;
		return ((shortMask >> index) & 1) == 1;
	}
}

LongRefSignature::LongRefSignature(ovlocals_t paramCount)
{
	uint32_t maskCount = (paramCount + ParamsPerMask - 1) / ParamsPerMask;

	this->paramCount = maskCount * ParamsPerMask;

	maskValues = Box<uint32_t[]>(new uint32_t[maskCount]);
	for (uint32_t i = 0; i < maskCount; i++)
		maskValues[i] = 0;
}

bool LongRefSignature::IsParamRef(ovlocals_t index) const
{
	uint32_t mask = maskValues[index / ParamsPerMask];
	return ((mask >> index % ParamsPerMask) & 1) == 1;
}

void LongRefSignature::SetParam(ovlocals_t index, bool isRef)
{
	uint32_t *mask = maskValues.get() + index / ParamsPerMask;
	index %= ParamsPerMask;
	if (isRef)
		*mask |= 1 << index;
	else
		*mask &= ~(1 << index);
}

bool LongRefSignature::HasRefs() const
{
	for (ovlocals_t i = 0; i < paramCount / ParamsPerMask; i++)
		if (maskValues[i] != 0)
			return true;
	return false;
}

bool LongRefSignature::Equals(const LongRefSignature &other) const
{
	if (this->paramCount != other.paramCount)
		return false;

	for (ovlocals_t i = 0; i < this->paramCount; i++)
		if (this->maskValues[i] != other.maskValues[i])
			return false;

	return true;
}

uint32_t RefSignaturePool::Add(LongRefSignature *signature, bool &isNew)
{
	uint32_t i = 0;
	while (i < (uint32_t)signatures.size())
	{
		auto &item = signatures[i];
		if (item->Equals(*signature))
		{
			isNew = false;
			return i | RefSignature::SignatureKindMask;
		}
		i++;
	}

	// Take ownership of the signature
	signatures.push_back(Box<LongRefSignature>(signature));
	isNew = true;
	return i | RefSignature::SignatureKindMask;
}

RefSignatureBuilder::RefSignatureBuilder(ovlocals_t paramCount) :
	isLong(paramCount > RefSignature::MaxShortParamCount),
	isCommitted(false)
{
	if (isLong)
		longSignature = new LongRefSignature(paramCount);
	else
		shortMask = 0;
}

RefSignatureBuilder::~RefSignatureBuilder()
{
	// When a long signature is committed, it can either be added to the pool (if
	// there is no other identical long signature), or the index of an existing
	// signature is returned. When the signature is added to the pool, the pool
	// becomes the owner of longSignature, so don't delete it.
	// isCommitted is true if the longSignature was added to the pool.
	if (isLong && !isCommitted)
	{
		delete longSignature;
		longSignature = nullptr;
	}
}

bool RefSignatureBuilder::IsParamRef(ovlocals_t index) const
{
	if (isLong)
		return longSignature->IsParamRef(index);
	else
		return ((shortMask >> index) & 1) == 1;
}

void RefSignatureBuilder::SetParam(ovlocals_t index, bool isRef)
{
	if (isLong)
	{
		longSignature->SetParam(index, isRef);
	}
	else
	{
		if (isRef)
			shortMask |= 1 << index;
		else
			shortMask &= ~(1 << index);
	}
}

uint32_t RefSignatureBuilder::Commit(RefSignaturePool *pool)
{
	if (isLong)
	{
		// 0 is reserved for "nothing by ref", even for long signatures
		if (!longSignature->HasRefs())
			return 0;
		return pool->Add(longSignature, isCommitted);
	}
	else
	{
		return shortMask;
	}
}

} // namespace ovum
