#ifndef VM__STATICREF_H
#define VM__STATICREF_H

#include "../vm.h"
#include "../threading/sync.h"

namespace ovum
{

class StaticRef
{
private:
	SpinLock accessLock;
	Value value;

public:
	// Note: no constructor. The type needs to be usable in an array.

	// Initializes the static reference to the specified value.
	// This should only be called ONCE per static reference.
	inline void Init(Value value)
	{
		accessLock.SpinLock::SpinLock();
		this->value = value;
	}

	// Atomically reads the value of the static reference.
	inline Value Read()
	{
		accessLock.Enter();
		Value result = value;
		accessLock.Leave();
		return result;
	}
	inline void Read(Value *target)
	{
		accessLock.Enter();
		*target = value;
		accessLock.Leave();
	}

	// Atomically updates the value of the static reference.
	inline void Write(Value value)
	{
		accessLock.Enter();
		this->value = value;
		accessLock.Leave();
	}
	inline void Write(Value *value)
	{
		accessLock.Enter();
		this->value = *value;
		accessLock.Leave();
	}

	inline Value *GetValuePointer()
	{
		return &value;
	}

	friend class GC;
};

class StaticRefBlock
{
public:
	static const size_t BLOCK_SIZE = 64;

	StaticRefBlock *next;
	unsigned int count;
	// Only used during collection. Set to true if the block
	// contains any references to gen0 objects.
	bool hasGen0Refs;
	StaticRef values[BLOCK_SIZE];

	inline StaticRefBlock() :
		next(nullptr), count(0), hasGen0Refs(false)
	{ }
	inline StaticRefBlock(StaticRefBlock *next) :
		next(next), count(0), hasGen0Refs(false)
	{ }
};

} // namespace ovum

#endif // VM__STATICREF_H
