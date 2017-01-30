#pragma once

#include "../vm.h"

namespace ovum
{

class StackFrame
{
public:
	// The current size of the evaluation stack.
	// This is the first field because it is the most frequently accessed;
	// therefore, no offset needs to be added to the stack frame pointer
	// to obtain the value of this field.
	ovlocals_t stackCount;
	// The number of arguments that were passed to the method, INCLUDING
	// the instance if the method is an instance method.
	// This is required by the ldargc instruction.
	ovlocals_t argc;
	// The address at which the evaluation stack begins.
	Value *evalStack;
	// The previous IP.
	uint8_t *prevInstr;
	// The previous stack frame.
	StackFrame *prevFrame;
	// The method that the stack frame represents an invocation to.
	// This is used when accessing members by name, to determine
	// whether they are accessible, and when generating a stack trace,
	// to obtain the name of the method.
	MethodOverload *method;

	inline Value *NextStackSlot()
	{
		return evalStack + stackCount++;
	}

	inline Value Pop()
	{
		OVUM_ASSERT(stackCount > 0);
		return evalStack[--stackCount];
	}
	inline void Pop(ovlocals_t n)
	{
		OVUM_ASSERT(n <= stackCount);
		stackCount -= n;
	}

	inline Value Peek(ovlocals_t n = 0) const
	{
		OVUM_ASSERT(n <= stackCount);
		return evalStack[stackCount - n - 1];
	}

	inline Type *PeekType(ovlocals_t n = 0) const
	{
		OVUM_ASSERT(n <= stackCount);
		return evalStack[stackCount - n - 1].type;
	}

	inline String *PeekString(ovlocals_t n = 0) const
	{
		OVUM_ASSERT(n <= stackCount);
		return evalStack[stackCount - n - 1].v.string;
	}

	inline void Shift(ovlocals_t offset)
	{
		Value *stackPointer = evalStack + stackCount - offset - 1;
		for (ovlocals_t i = 0; i < offset; i++)
		{
			*stackPointer = *(stackPointer + 1);
			stackPointer++;
		}
		stackCount--;
	}

	// The base of the locals array
	Value *const Locals();
	const Value *const Locals() const;
};

static const size_t STACK_FRAME_SIZE = OVUM_ALIGN_TO(sizeof(::ovum::StackFrame), 8);

inline Value *const StackFrame::Locals()
{
	return reinterpret_cast<Value*>(
		reinterpret_cast<uintptr_t>(this) + STACK_FRAME_SIZE
	);
}

inline const Value *const StackFrame::Locals() const
{
	return reinterpret_cast<const Value*>(
		reinterpret_cast<uintptr_t>(this) + STACK_FRAME_SIZE
	);
}

} // namespace ovum
