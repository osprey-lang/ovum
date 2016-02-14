#include "refsignature.h"

namespace ovum
{

LongRefSignature::LongRefSignature(ovlocals_t paramCount)
{
	uint32_t maskCount = (paramCount + 31) / 32;

	this->paramCount = maskCount * 32;

	maskValues = new uint32_t[maskCount];
	for (uint32_t i = 0; i < maskCount; i++)
		maskValues[i] = 0;
}

LongRefSignature::~LongRefSignature()
{
	delete[] maskValues;
	maskValues = nullptr;
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

RefSignaturePool::~RefSignaturePool()
{
	typedef std::vector<LongRefSignature*>::iterator ref_iter;

	for (ref_iter i = signatures.begin(); i != signatures.end(); ++i)
	{
		LongRefSignature *signature = *i;
		delete signature;
	}
}

uint32_t RefSignaturePool::Add(LongRefSignature *signature, bool &isNew)
{
	uint32_t i = 0;
	while (i < (uint32_t)signatures.size())
	{
		LongRefSignature *item = signatures[i];
		if (item->Equals(*signature))
		{
			isNew = false;
			return i | RefSignature::SignatureKindMask;
		}
		i++;
	}
	signatures.push_back(signature);
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

}