#pragma once

#ifndef VM__THREAD_INTERNAL_H
#define VM__THREAD_INTERNAL_H

#include <cassert>
#include "ov_vm.internal.h"
#include "ov_stringbuffer.internal.h"

class StringBuffer;
namespace instr
{
	class MethodBuilder;
}

// The total size of a call stack.
#define CALL_STACK_SIZE    1024*1024
// The number of bytes that will always be available on the call stack.
// This is to allow the runtime to do things on the call stack, like
// throwing errors and whatnot, even during stack overflow conditions.
// It also ensures that very small manipulations of the call stack, such
// as shifting a method instance in before the arguments to the method,
// will always succeed.
#define CALL_STACK_BUFFER  1024


typedef struct StackFrame_S StackFrame;
typedef struct StackFrame_S
{
	// The current size of the evaluation stack.
	// This is the first field because it is the most frequently accessed;
	// therefore, no offset needs to be added to the stack frame pointer
	// to obtain the value of this field.
	uint16_t stackCount;
	// The number of arguments that were passed to the method, INCLUDING
	// the instance if the method is an instance method.
	// This is required by the ldargc instruction.
	uint16_t argc;
	// The address of the first argument.
	// Note that the GC uses this to examine the arguments passed to the
	// method. Also note that the parameter count is not necessarily the
	// same as the argument count.
	Value *arguments;
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
	Method::Overload *method;

	inline void Init(uint16_t stackCount, uint16_t argc,
		Value *arguments, Value *evalStack, uint8_t *prevInstr,
		StackFrame *prevFrame, Method::Overload *method)
	{
		this->stackCount = stackCount;
		this->argc       = argc;
		this->arguments  = arguments;
		this->evalStack  = evalStack;
		this->prevInstr  = prevInstr;
		this->prevFrame  = prevFrame;
		this->method     = method;
	}

	inline void Push(Value value)
	{
		evalStack[stackCount++] = value;
	}
	inline void PushBool(bool value)
	{
		SetBool_(evalStack + stackCount++, value);
	}
	inline void PushInt(int64_t value)
	{
		SetInt_(evalStack + stackCount++, value);
	}
	inline void PushUInt(uint64_t value)
	{
		SetUInt_(evalStack + stackCount++, value);
	}
	inline void PushReal(double value)
	{
		SetReal_(evalStack + stackCount++, value);
	}
	inline void PushString(String *value)
	{
		SetString_(evalStack + stackCount++, value);
	}

	inline Value Pop()
	{
		assert(stackCount > 0);
		return evalStack[--stackCount];
	}
	inline void Pop(unsigned int n)
	{
		assert(n <= stackCount);
		stackCount -= n;
	}

	inline Value Peek(unsigned int n = 0) const
	{
		assert(n <= stackCount);
		return evalStack[stackCount - n - 1];
	}

	inline void Shift(const uint16_t offset)
	{
		Value *stackPointer = evalStack + stackCount - offset - 1;
		for (int i = 0; i < offset; i++)
		{
			*stackPointer = *(stackPointer + 1);
			stackPointer++;
		}
		stackCount--;
	}
} StackFrame;

// The base of the locals array, relative to a stack frame base pointer.
#define LOCALS_OFFSET(sf)    reinterpret_cast<::Value*>((sf) + 1)
// Gets the total size of a specific stack frame. This is NOT equivalent to sizeof(StackFrame)!
#define STACK_FRAME_SIZE(sf) (sizeof(::StackFrame) + ((sf)->stackCount + (sf)->method->locals) * sizeof(::Value))


enum ThreadState
{
	// The thread has been created but not started.
	THREAD_CREATED   = 0x00,
	// The thread is running.
	THREAD_RUNNING   = 0x01,
	// The thread is suspended.
	THREAD_SUSPENDED = 0x02,
	// The thread has stopped, either from having its main method return, or from being killed.
	THREAD_STOPPED   = 0x03,
};


class Thread
{
public:
	Thread();
	~Thread();

	void Start(Method *method, Value &result);

private:
	// The current instruction pointer. This should always be the first field in the class.
	uint8_t *ip;

	// For obtaining the current frame from the call stack.
	// Stack frames grow up, towards higher addresses.
	// NOTE: This is relative to the base of the StackFrame*! The arguments precede
	// the base of said pointer.
	StackFrame *currentFrame;

	// The current state of the thread. And what a state it's in. Tsk tsk tsk.
	ThreadState state;

	// The current error.
	// If successfully caught, this is set to NULL_VALUE *after* the catch clause
	// has been exited. We need to do this after because the catch clause may be
	// generic (hence, the error is not in a local or on the stack) and the catch
	// clause may trigger a GC cycle or may want to rethrow the current error.
	Value currentError;

	// The call stack. This should be the last field in the class.
	// Note: although this represents a bunch of unspecific bytes, giving the type
	// as unsigned char allows us to add and subtract from the address of this field.
	unsigned char *callStack;

public:
	inline void Push      (Value    value) { currentFrame->Push(value);       }
	inline void PushBool  (bool     value) { currentFrame->PushBool(value);   }
	inline void PushInt   (int64_t  value) { currentFrame->PushInt(value);    }
	inline void PushUInt  (uint64_t value) { currentFrame->PushUInt(value);   }
	inline void PushReal  (double   value) { currentFrame->PushReal(value);   }
	inline void PushString(String  *value) { currentFrame->PushString(value); }

	inline void PushNull() { currentFrame->Push(NULL_VALUE);  }

	inline Value Pop() { return currentFrame->Pop(); }
	inline void Pop(unsigned int n) { currentFrame->Pop(n); }

	inline void Dup() { currentFrame->Push(currentFrame->Peek()); }

	inline Value *Local(const unsigned int n) { return LOCALS_OFFSET(currentFrame) + n; }

	// argCount does NOT include the instance.
	void Invoke(unsigned int argCount, Value *result);
	// argCount does NOT include the instance.
	void InvokeMember(String *name, unsigned int argCount, Value *result);
	void InvokeOperator(Operator op, Value *result);
	void InvokeApply(Value *result);
	void InvokeApplyMethod(Method *method, Value *result);

	bool Equals();
	int Compare();
	void Concat(Value *result);

	void LoadMember(String *member, Value *result);
	void StoreMember(String *member);

	// Note: argCount does NOT include the instance.
	void LoadIndexer(uint16_t argCount, Value *result);
	// Note: argCount does NOT include the instance or the value that's being stored.
	void StoreIndexer(uint16_t argCount);

	void LoadStaticField(Field *field, Value *result);
	void StoreStaticField(Field *field);

	void ToString(String **result);

#pragma warning(push)
#pragma warning(disable: 4290) // Ignoring C++ exception specification
	void Throw(bool rethrow = false) throw(OvumException);

	// Throw helpers!

	void ThrowError(String *message = nullptr) throw(OvumException);
	void ThrowTypeError(String *message = nullptr) throw(OvumException);
	void ThrowMemoryError(String *message = nullptr) throw(OvumException);
	void ThrowOverflowError(String *message = nullptr) throw(OvumException);
	void ThrowDivideByZeroError(String *message = nullptr) throw(OvumException);
	void ThrowNullReferenceError(String *message = nullptr) throw(OvumException);
#pragma warning(pop)

private:
	void InitCallStack();
	void DisposeCallStack();

	// Pushes a new stack frame onto the call stack representing a call
	// to the specified method.
	//   argCount:
	//     The number of arguments passed to the method, INCLUDING the instance.
	//   method:
	//     The overload that is being invoked in the stack frame.
	StackFrame *PushStackFrame(const uint16_t argCount, Value *args, Method::Overload *method);

	// Pushes the very first stack frame onto the call stack.
	StackFrame *PushFirstStackFrame(const uint16_t argCount, Value args[], Method::Overload *method);

	void PrepareArgs(const MethodFlags flags, const uint16_t argCount, const uint16_t paramCount, StackFrame *frame);

	// Resolves a method to an overload that accepts the specified number of arguments.
	//   argCount:
	//     The number of arguments that are being passed to the method, NOT including the instance.
	static inline Method::Overload *ResolveOverload(Method *method, const uint16_t argCount)
	{
		while (method)
		{
			for (int i = 0; i < method->overloadCount; i++)
			{
				Method::Overload *mo = method->overloads + i;
				if (mo->Accepts(argCount))
					return mo;
			}

			method = method->baseMethod;
		}
		return nullptr;
	}

	void Evaluate(StackFrame *frame, uint8_t *entryAddress);

	String *GetStackTrace();
	void AppendArgumentType(StringBuffer &buf, Value arg);

	// argCount DOES NOT include the instance.
	void InvokeLL(unsigned int argCount, Value *args, Value *result);
	// argCount and args DO NOT include the instance.
	void InvokeMethod(Method *method, unsigned int argCount, Value *args, Value *result);
	// argCount and args DO include the instance
	void InvokeMethodOverload(Method::Overload *mo, unsigned int argCount, Value *args, Value *result);

	void LoadMemberLL(Value *instance, String *member, Value *result);
	void StoreMemberLL(Value *instance, Value *value, String *member);

	void LoadIndexerLL(uint16_t argc, Value *args, Value *dest);

	void LoadIteratorLL(Value *inst, Value *dest);

	void InvokeOperatorLL(Value *args, Operator op, Value *result);
	bool EqualsLL(Value *args);
	int CompareLL(Value *args);
	void ConcatLL(Value *args, Value *result);

	typedef struct
	{
		enum Flags : uint8_t
		{
			// The slot is in use
			IN_USE   = 1,
			// The slot contains the 'this' argument
			THIS_ARG = 2,
		} flags;
	} StackEntry;

	uint8_t *InitializeMethod(Method::Overload *method);
	uint8_t *InitializeMethodInternal(StackEntry stack[], Method::Overload *method);
	void InitializeInstructions(instr::MethodBuilder &builder, Method::Overload *method);

	friend class GC;
	friend void VM_InvokeMethod(ThreadHandle, MethodHandle, const unsigned int, Value*);
};

// Converts a ThreadHandle to a real Thread.
//#define _Th(v)	reinterpret_cast<::Thread *const>(v)


#endif // VM__THREAD_INTERNAL_H