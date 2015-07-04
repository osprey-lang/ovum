#ifndef VM__REFSIGNATURE_INTERNAL_H
#define VM__REFSIGNATURE_INTERNAL_H

#include <cstdint>
#include <vector>

namespace ovum
{

class RefSignature;
class LongRefSignature;
class RefSignatureBuilder;
class RefSignaturePool;

class RefSignature
{
private:
	ovlocals_t paramCount;
	union
	{
		uint32_t shortMask;
		const uint32_t *longMask;
	};

public:
	RefSignature(uint32_t mask, RefSignaturePool *pool);

	inline bool IsParamRef(ovlocals_t index) const
	{
		if (paramCount > MaxShortParamCount)
		{
			uint32_t mask = longMask[index / 32];
			return ((mask >> index % 32) & 1) == 1;
		}
		else
		{
			if (index > MaxShortParamCount)
				return false;
			return ((shortMask >> index) & 1) == 1;
		}
	}

	static const ovlocals_t MaxShortParamCount = 31;
	static const uint32_t SignatureKindMask = 0x80000000u;
	static const uint32_t SignatureDataMask = 0x7fffffffu;

private:
	friend class RefSignatureBuilder;
};

class LongRefSignature
{
public:
	ovlocals_t paramCount;
	uint32_t *maskValues;

	inline LongRefSignature(ovlocals_t paramCount)
	{
		uint32_t maskCount = (paramCount + 31) / 32;
		this->paramCount = maskCount * 32;
		maskValues = new uint32_t[maskCount];
		for (uint32_t i = 0; i < maskCount; i++)
			maskValues[i] = 0;
	}

	inline ~LongRefSignature()
	{
		delete[] maskValues;
		maskValues = nullptr;
	}

	inline bool IsParamRef(ovlocals_t index) const
	{
		uint32_t mask = maskValues[index / 32];
		return ((mask >> index % 32) & 1) == 1;
	}

	inline void SetParam(ovlocals_t index, bool isRef)
	{
		uint32_t *mask = maskValues + index / 32;
		index %= 32;
		if (isRef)
			*mask |= 1 << index;
		else
			*mask &= ~(1 << index);
	}

	inline bool HasRefs() const
	{
		for (ovlocals_t i = 0; i < paramCount / 32; i++)
			if (maskValues[i] != 0)
				return true;
		return false;
	}

	inline bool Equals(const LongRefSignature &other) const
	{
		if (this->paramCount != other.paramCount)
			return false;

		for (ovlocals_t i = 0; i < this->paramCount; i++)
			if (this->maskValues[i] != other.maskValues[i])
				return false;

		return true;
	}

	inline bool operator==(const LongRefSignature &other) const
	{
		return this->Equals(other);
	}

	inline bool operator!=(const LongRefSignature &other) const
	{
		return !this->Equals(other);
	}
};

class RefSignaturePool
{
private:
	std::vector<LongRefSignature*> signatures;

public:
	inline RefSignaturePool() { } // Do nothing; initialize signatures to empty vector
	~RefSignaturePool();

	const LongRefSignature *Get(ovlocals_t index) const;
	uint32_t Add(LongRefSignature *signature, bool &isNew);
};

class RefSignatureBuilder
{
private:
	bool isLong;
	bool isCommitted;
	union
	{
		uint32_t shortMask;
		LongRefSignature *longSignature;
	};

public:
	inline RefSignatureBuilder(ovlocals_t paramCount)
		: isLong(paramCount > RefSignature::MaxShortParamCount),
		isCommitted(false)
	{
		if (isLong)
			longSignature = new LongRefSignature(paramCount);
		else
			shortMask = 0;
	}

	inline ~RefSignatureBuilder()
	{
		if (isLong && !isCommitted)
		{
			delete longSignature;
			longSignature = nullptr;
		}
	}

	inline bool IsParamRef(ovlocals_t index) const
	{
		if (isLong)
			return longSignature->IsParamRef(index);
		else
			return ((shortMask >> index) & 1) == 1;
	}

	inline void SetParam(ovlocals_t index, bool isRef)
	{
		if (isLong)
			longSignature->SetParam(index, isRef);
		else
			shortMask = (isRef ? shortMask | (1 << index) : shortMask & ~(1 << index));
	}

	inline uint32_t Commit(RefSignaturePool *pool)
	{
		if (isLong)
		{
			if (!longSignature->HasRefs())
				return 0;

			return pool->Add(longSignature, isCommitted);
		}
		else
			return shortMask;
	}
};

inline RefSignature::RefSignature(uint32_t mask, RefSignaturePool *pool)
{
	if (mask & SignatureKindMask)
	{
		const LongRefSignature *signature = pool->Get(mask & SignatureDataMask);
		paramCount = signature->paramCount;
		longMask = signature->maskValues;
	}
	else
	{
		paramCount = MaxShortParamCount;
		shortMask = mask & SignatureDataMask;
	}
}

inline RefSignaturePool::~RefSignaturePool()
{
	typedef std::vector<LongRefSignature*>::iterator ref_iter;

	for (ref_iter i = signatures.begin(); i != signatures.end(); ++i)
	{
		LongRefSignature *signature = *i;
		delete signature;
	}
}

inline const LongRefSignature *RefSignaturePool::Get(ovlocals_t index) const
{
	return signatures[index];
}

inline uint32_t RefSignaturePool::Add(LongRefSignature *signature, bool &isNew)
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

} // namespace ovum

#endif // VM__REFSIGNATURE_INTERNAL_H
