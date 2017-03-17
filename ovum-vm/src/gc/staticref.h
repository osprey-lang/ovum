#pragma once

#include "../vm.h"
#include "../threading/sync.h"

namespace ovum
{

class StaticRef
{
public:
	// Note: no constructor. The type needs to be usable in an array.

	// Initializes the static reference to the specified value.
	// This should only be called ONCE per static reference.
	inline void Init(Value *value)
	{
		accessLock.SpinLock::SpinLock();
		this->value = *value;
	}

	// Atomically reads the value of the static reference.
	inline void Read(Value *target)
	{
		accessLock.Enter();
		*target = value;
		accessLock.Leave();
	}

	// Atomically updates the value of the static reference.
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

private:
	SpinLock accessLock;
	Value value;

	friend class GC;
};

class StaticRefBlock
{
public:
	static bool Extend(Box<StaticRefBlock> &other);

	inline bool IsFull() const
	{
		return count == BLOCK_SIZE;
	}

	StaticRef *Add(Value *value);

private:
	OVUM_DISABLE_COPY_AND_ASSIGN(StaticRefBlock);

	static const size_t BLOCK_SIZE = 128;

	Box<StaticRefBlock> next;
	// Only used during collection. Set to true if the block
	// contains any references to gen0 objects.
	bool hasGen0Refs;

	// Number of used slots
	size_t count;
	StaticRef values[BLOCK_SIZE];

	inline StaticRefBlock() :
		next(),
		hasGen0Refs(false),
		count(0)
	{ }

	friend class GC;
	friend class LiveObjectFinder;
	friend class MovedObjectUpdater;
	template<class Visitor>
	friend class RootSetWalker;
};

} // namespace ovum
