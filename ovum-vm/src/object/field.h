#pragma once

#include "../vm.h"
#include "member.h"

namespace ovum
{

class Field : public Member
{
public:
	union
	{
		int32_t offset;
		StaticRef *staticValue;
	};

	inline Field(String *name, Type *declType, MemberFlags flags) :
		Member(name, declType, flags | MemberFlags::FIELD)
	{ }

	int ReadField(Thread *const thread, Value *instance, Value *dest) const;
	int ReadFieldFast(Thread *const thread, Value *instance, Value *dest) const;
	void ReadFieldUnchecked(Value *instance, Value *dest) const;

	int WriteField(Thread *const thread, Value *instanceAndValue) const;
	int WriteFieldFast(Thread *const thread, Value *instanceAndValue) const;
	void WriteFieldUnchecked(Value *instanceAndValue) const;
};

} // namespace ovum
