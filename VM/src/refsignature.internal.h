#pragma once

#ifndef VM__REFSIGNATURE_INTERNAL_H
#define VM__REFSIGNATURE_INTERNAL_H

#include <cstdint>
#include <vector>

class LongRefSignature;
class RefSignatureBuilder;

class RefSignature
{
private:
	unsigned int paramCount;
	union
	{
		uint32_t shortMask;
		const uint32_t *longMask;
	};

public:
	RefSignature(uint32_t mask);

	inline bool IsParamRef(unsigned int index) const
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

	static const unsigned int MaxShortParamCount = 31;
	static const uint32_t SignatureKindMask = 0x80000000u;
	static const uint32_t SignatureDataMask = 0x7fffffffu;

private:
	class Pool
	{
	private:
		std::vector<LongRefSignature*> signatures;

	public:
		inline Pool() { } // Do nothing; initialize signatures to empty vector
		~Pool();

		const LongRefSignature *Get(uint32_t index) const;
		uint32_t Add(LongRefSignature *signature, bool &isNew);
	};

	static Pool *pool; // Defined in thread.method_initializer.cpp

	friend class RefSignatureBuilder;
};

class LongRefSignature
{
public:
	unsigned int paramCount;
	uint32_t *maskValues;

	inline LongRefSignature(unsigned int paramCount)
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

	inline bool IsParamRef(unsigned int index) const
	{
		uint32_t mask = maskValues[index / 32];
		return ((mask >> index % 32) & 1) == 1;
	}

	inline void SetParam(unsigned int index, bool isRef)
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
		for (unsigned int i = 0; i < paramCount / 32; i++)
			if (maskValues[i] != 0)
				return true;
		return false;
	}

	inline bool Equals(const LongRefSignature &other) const
	{
		if (this->paramCount != other.paramCount)
			return false;

		for (unsigned int i = 0; i < this->paramCount; i++)
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

inline RefSignature::RefSignature(uint32_t mask)
{
	if (mask & SignatureKindMask)
	{
		const LongRefSignature *signature = pool->Get(mask & SignatureDataMask);
		paramCount = signature->paramCount;
		longMask = signature->maskValues;
	}
	else
	{
		paramCount = 31;
		shortMask = mask & SignatureDataMask;
	}
}

inline RefSignature::Pool::~Pool()
{
	typedef std::vector<LongRefSignature*>::iterator ref_iter;

	for (ref_iter i = signatures.begin(); i != signatures.end(); ++i)
	{
		LongRefSignature *signature = *i;
		delete signature;
	}
}

inline const LongRefSignature *RefSignature::Pool::Get(uint32_t index) const
{
	return signatures[index];
}

inline uint32_t RefSignature::Pool::Add(LongRefSignature *signature, bool &isNew)
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
	inline RefSignatureBuilder(unsigned int paramCount)
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

	inline bool IsParamRef(uint32_t index) const
	{
		if (isLong)
			return longSignature->IsParamRef(index);
		else
			return ((shortMask >> index) & 1) == 1;
	}

	inline void SetParam(uint32_t index, bool isRef)
	{
		if (isLong)
			longSignature->SetParam(index, isRef);
		else
			shortMask = (isRef ? shortMask | (1 << index) : shortMask & ~(1 << index));
	}

	inline uint32_t Commit()
	{
		if (isLong)
		{
			if (!longSignature->HasRefs())
				return 0;

			if (RefSignature::pool == nullptr)
				RefSignature::pool = new RefSignature::Pool();
			return RefSignature::pool->Add(longSignature, isCommitted);
		}
		else
			return shortMask;
	}
};

#endif // VM__REFSIGNATURE_INTERNAL_H