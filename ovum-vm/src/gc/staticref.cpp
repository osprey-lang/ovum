#include "staticref.h"

namespace ovum
{

bool StaticRefBlock::Extend(Box<StaticRefBlock> &other)
{
	Box<StaticRefBlock> newBlock(new(std::nothrow) StaticRefBlock());
	if (!newBlock)
		return false;

	newBlock->next = std::move(other);
	other = std::move(newBlock);
	return true;
}

StaticRef *StaticRefBlock::Add(Value *value)
{
	// This block is full!
	if (IsFull())
		return nullptr;

	StaticRef *result = values + count;
	count++;
	result->Init(value);
	return result;
}

} // namespace ovum
