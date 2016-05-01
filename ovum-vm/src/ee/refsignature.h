#pragma once

#include "../../inc/ov_vm.h"
#include <vector>

/*\
|*| When invoking a method, it is possible to pass arguments by reference. The "referenceness"
|*| of an argument must match the "referenceness" of the parameter it is assigned to, which
|*| means we need some way to verify that the arguments and the parameters actually have the
|*| same referenceness.
|*|
|*| To accomplish this, each method overload and each invocation is given a reference signature,
|*| which encodes the referenceness of each parameter/argument. The refness is encoded as a bit
|*| field, where set bits indicate "pass by reference" and cleared bits are "pass by value".
|*| Bit 0 is reserved for the instance, even if the overload or invocation doesn't have any,
|*| and since 'this' cannot be passed by reference, it is always set to 0. An example of a
|*| ref signature:
|*|
|*|   0 0 ... 0 0 1 1 0 0
|*|   |         | | | | `- Always zero ('this' parameter/argument, if present)
|*|    \       /  | | `--- First named parameter/argument not by ref
|*|     `--+--´   | `----- Second by ref
|*|        |      `------- Third by ref
|*|        `-------------- Rest either absent or not by ref
|*|
|*| There are actually two kinds of ref signatures: long and short. A short ref signature stores
|*| the bit field in the lowest 31 bits of a 32-bit unsigned integer, which limits it to 30
|*| actual parameters/arguments (excluding the instance, for which space is always reserved).
|*| Short signatures have the most significant bit cleared. When the MSB is set, the signature
|*| is long, and then the remaining 31 bits store an index into a table of bit fields, each
|*| of which is represented by an array of integers. Basically, long signatures are accessed
|*| through a lookup table. Long signatures with identical refness are stored under the same
|*| index in the lookup table.
|*|
|*| The value 0 is reserved for "nothing by reference", even when the method/invocation has
|*| more than 30 parameters/arguments.
|*|
|*| The use of 32-bit integers allows argument and parameter refness to be compared in a single
|*| instruction in the common case of every argument matching every parameter. The interning of
|*| long signatures also helps to increase performance in such cases.
|*|
|*| There are two cases where ref signatures may not match even though the argument refness is
|*| correct:
|*|   * The method or invocation has at least one parameter/argument by reference, and:
|*|   1. The method has more than 30 parameters, of which some are optional, and the invocation
|*|      has fewer than 31 arguments.
|*|   2. The method is variadic with fewer than 31 parameters, and is passsed more than 30
|*|      arguments.
|*| In both cases above, one signature will be long and the other short. Note that if neither
|*| method nor invocation has anything by reference, they will both have a signature of 0.
|*| Because of these ((extremely) rare) cases, it is necessary to check each parameter against
|*| each argument when signatures do not match; see ovum::MethodOverload::VerifyRefSignature.
\*/

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

	LongRefSignature(ovlocals_t paramCount);

	~LongRefSignature();

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

	bool Equals(const LongRefSignature &other) const;

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

	const LongRefSignature *Get(ovlocals_t index) const
	{
		return signatures[index];
	}

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
	RefSignatureBuilder(ovlocals_t paramCount);

	~RefSignatureBuilder();

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

	uint32_t Commit(RefSignaturePool *pool);
};

// Keep this inline, for performance reasons
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

} // namespace ovum
