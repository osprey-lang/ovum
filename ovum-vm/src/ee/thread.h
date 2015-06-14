#ifndef VM__THREAD_INTERNAL_H
#define VM__THREAD_INTERNAL_H

#include "../vm.h"
#include "vm.h"
#include "../threading/sync.h"
#include "../threading/tls.h"

namespace ovum
{

class StackFrame
{
public:
	// The current size of the evaluation stack.
	// This is the first field because it is the most frequently accessed;
	// therefore, no offset needs to be added to the stack frame pointer
	// to obtain the value of this field.
	uint32_t stackCount;
	// The number of arguments that were passed to the method, INCLUDING
	// the instance if the method is an instance method.
	// This is required by the ldargc instruction.
	uint32_t argc;
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
	inline void Pop(unsigned int n)
	{
		OVUM_ASSERT(n <= stackCount);
		stackCount -= n;
	}

	inline Value Peek(unsigned int n = 0) const
	{
		OVUM_ASSERT(n <= stackCount);
		return evalStack[stackCount - n - 1];
	}
	inline Type *PeekType(unsigned int n = 0) const
	{
		OVUM_ASSERT(n <= stackCount);
		return evalStack[stackCount - n - 1].type;
	}
	inline String *PeekString(unsigned int n = 0) const
	{
		OVUM_ASSERT(n <= stackCount);
		return evalStack[stackCount - n - 1].v.string;
	}

	inline void Shift(uint16_t offset)
	{
		Value *stackPointer = evalStack + stackCount - offset - 1;
		for (int i = 0; i < offset; i++)
		{
			*stackPointer = *(stackPointer + 1);
			stackPointer++;
		}
		stackCount--;
	}

	// The base of the locals array
	Value *Locals() const;
};

static const size_t STACK_FRAME_SIZE = OVUM_ALIGN_TO(sizeof(::ovum::StackFrame), 8);

inline Value *StackFrame::Locals() const
{
	return reinterpret_cast<Value*>((char*)this + STACK_FRAME_SIZE);
}

enum class ThreadRequest
{
	// The thread has no particular request associated with it.
	NONE = 0,
	// The thread should suspend for the GC as soon as it can.
	SUSPEND_FOR_GC = 1,
};

enum class ThreadState : int
{
	// The thread has been created but not started.
	CREATED         = 0x00,
	// The thread is running.
	RUNNING         = 0x01,
	// The thread is suspended by the GC.
	SUSPENDED_BY_GC = 0x02,
	// The thread has stopped, either from having its main method return, or from being killed.
	STOPPED         = 0x03,
};

enum class ThreadFlags : int
{
	NONE                = 0x00,
	// The thread is in an unmanaged region.
	// See VM_EnterUnmanagedRegion for details.
	IN_UNMANAGED_REGION = 0x01,
};
OVUM_ENUM_OPS(ThreadFlags, int);

class Thread
{
public:
	OVUM_NOINLINE static int Create(VM *owner, Thread *&result);

	Thread(VM *owner, int &status);
	~Thread();

	int Start(unsigned int argCount, MethodOverload *mo, Value &result);

private:
	// The size of the managed call stack
	static const size_t CALL_STACK_SIZE = 1024 * 1024;

	// The currently executing managed thread.
	static TlsEntry<Thread> threadKey;

	// The current instruction pointer. This should always be the first field in the class.
	uint8_t *ip;

	// For obtaining the current frame from the call stack.
	// Stack frames grow up, towards higher addresses.
	// NOTE: This is relative to the base of the StackFrame*! The arguments precede
	// the base of said pointer.
	StackFrame *currentFrame;

	// If another thread is waiting for this thread to perform a specific action, this field
	// is set to an appropriate request.
	// Note: Only one request can be active at a time. Currently this field is only used by
	// the GC, but that may change in the future.
	volatile ThreadRequest pendingRequest;

	os::ThreadId nativeId;

	// The current state of the thread. And what a state it's in. Tsk tsk tsk.
	ThreadState state;

	// Various thread flags.
	ThreadFlags flags;

	// The call stack. This grows towards higher addresses.
	// Note: although this represents a bunch of unspecific bytes, giving the type
	// as unsigned char allows us to add and subtract from the address of this field.
	unsigned char *callStack;

	// The VM instance that owns this thread.
	VM *vm;

	// Shared string data, accessed in a lot of places.
	StaticStrings *strings;

	// The current error.
	// If successfully caught, this is set to NULL_VALUE *after* the catch clause
	// has been exited. We need to do this after because the catch clause may be
	// generic (hence, the error is not in a local or on the stack) and the catch
	// clause may trigger a GC cycle or may want to rethrow the current error.
	Value currentError;

	// The critical section that the thread tries to enter when the GC is running
	// on another thread. The GC enters this section first, and leaves it only when
	// GC cycle is complete, thus blocking the current thread for the duration of
	// the cycle.
	CriticalSection gcCycleSection;

	// The one-argument indexer setter for aves.Hash, used by ConcatLL.
	// Starts out null; is initialized on demand. Access through GetHashIndexerSetter().
	MethodOverload *hashSetItem;

public:
	inline void Push(Value *value)
	{
		*currentFrame->NextStackSlot() = *value;
	}
	inline void PushBool(bool value)
	{
		Value *top = currentFrame->NextStackSlot();
		top->type = vm->types.Boolean;
		top->v.integer = value;
	}
	inline void PushInt(int64_t value)
	{
		Value *top = currentFrame->NextStackSlot();
		top->type = vm->types.Int;
		top->v.integer = value;
	}
	inline void PushUInt(uint64_t value)
	{
		Value *top = currentFrame->NextStackSlot();
		top->type = vm->types.UInt;
		top->v.uinteger = value;
	}
	inline void PushReal(double value)
	{
		Value *top = currentFrame->NextStackSlot();
		top->type = vm->types.Real;
		top->v.real = value;
	}
	inline void PushString(String *value)
	{
		Value *top = currentFrame->NextStackSlot();
		top->type = vm->types.String;
		top->v.string = value;
	}
	inline void PushNull()
	{
		currentFrame->NextStackSlot()->type = nullptr;
	}

	inline Value Pop() { return currentFrame->Pop(); }
	inline void Pop(unsigned int n) { currentFrame->Pop(n); }

	inline void Dup()
	{
		Value *top = currentFrame->evalStack + currentFrame->stackCount++;
		*(top + 1) = *top;
	}

	inline Value *Local(unsigned int n) { return currentFrame->Locals() + n; }

	// argCount does NOT include the instance.
	int Invoke(uint32_t argCount, Value *result);
	// argCount DOES NOT include the instance.
	int InvokeMethod(Method *method, uint32_t argCount, Value *result);
	// argCount does NOT include the instance.
	int InvokeMember(String *name, uint32_t argCount, Value *result);
	int InvokeOperator(Operator op, Value *result);
	int InvokeApply(Value *result);
	int InvokeApplyMethod(Method *method, Value *result);

	int Equals(bool *result);
	int Compare(int64_t *result);
	int Concat(Value *result);

	int LoadMember(String *member, Value *result);
	int StoreMember(String *member);

	int LoadField(Field *field, Value *result);
	int StoreField(Field *field);

	// Note: argCount does NOT include the instance.
	int LoadIndexer(uint32_t argCount, Value *result);
	// Note: argCount does NOT include the instance or the value that's being stored.
	int StoreIndexer(uint32_t argCount);

	int LoadStaticField(Field *field, Value *result);
	int StoreStaticField(Field *field);

	int ToString(String **result);

	int Throw(bool rethrow = false);

	// Throw helpers!

	OVUM_NOINLINE int ThrowError(String *message = nullptr);
	OVUM_NOINLINE int ThrowTypeError(String *message = nullptr);
	OVUM_NOINLINE int ThrowMemoryError(String *message = nullptr);
	OVUM_NOINLINE int ThrowOverflowError(String *message = nullptr);
	OVUM_NOINLINE int ThrowDivideByZeroError(String *message = nullptr);
	OVUM_NOINLINE int ThrowNullReferenceError(String *message = nullptr);
	OVUM_NOINLINE int ThrowMemberNotFoundError(String *member);
	OVUM_NOINLINE int ThrowNoOverloadError(uint32_t argCount, String *message = nullptr);

	bool IsSuspendedForGC() const;

	void EnterUnmanagedRegion();
	void LeaveUnmanagedRegion();
	inline bool IsInUnmanagedRegion() const
	{
		return (flags & ThreadFlags::IN_UNMANAGED_REGION) == ThreadFlags::IN_UNMANAGED_REGION;
	}

	inline const void *GetInstructionPointer() const
	{
		return ip;
	}
	inline const StackFrame *GetCurrentFrame() const
	{
		return currentFrame;
	}

	inline VM *GetVM() const
	{
		return vm;
	}
	inline GC *GetGC() const
	{
		return vm->GetGC();
	}
	inline StaticStrings *GetStrings() const
	{
		return strings;
	}

private:
	int InitCallStack();
	void DisposeCallStack();
	
	void PushFirstStackFrame();
	// Pushes a new stack frame onto the call stack representing a call
	// to the specified method.
	//   argCount:
	//     The number of arguments passed to the method, INCLUDING the instance.
	//   args:
	//     Pointer to the first argument, which must be on the stack.
	//   method:
	//     The overload that is being invoked in the stack frame.
	void PushStackFrame(uint32_t argCount, Value *args, MethodOverload *method);

	int PrepareVariadicArgs(MethodFlags flags, uint32_t argCount, uint32_t paramCount, StackFrame *frame);

	OVUM_NOINLINE void HandleRequest();

	// Tells the thread to suspend itself as soon as possible.
	// Thread::IsSuspendedForGC() returns true when this is done.
	OVUM_NOINLINE void PleaseSuspendForGCAsap();
	// Tells the thread it doesn't have to suspend itself anymore,
	// because the GC cycle has ended. This happens when the thread
	// spends the entire GC cycle in an unmanaged region.
	OVUM_NOINLINE void EndGCSuspension();
	OVUM_NOINLINE void SuspendForGC();

	int Evaluate();
	int FindErrorHandler(int32_t maxIndex);
	int EvaluateLeave(register StackFrame *frame, int32_t target);

	String *GetStackTrace();
	void AppendArgumentType(StringBuffer &buf, Value *arg);
	void AppendSourceLocation(StringBuffer &buf, MethodOverload *method, uint8_t *ip);

	// argCount DOES NOT include the value to be invoked, but value does.
	int InvokeLL(unsigned int argCount, Value *value, Value *result, uint32_t refSignature);
	// args DOES include the instance, argCount DOES NOT
	int InvokeMethodOverload(MethodOverload *mo, unsigned int argCount, Value *args, Value *result);

	int InvokeApplyLL(Value *args, Value *result);
	int InvokeApplyMethodLL(Method *method, Value *args, Value *result);

	int InvokeMemberLL(String *name, uint32_t argCount, Value *value, Value *result, uint32_t refSignature);

	int LoadMemberLL(Value *instance, String *member, Value *result);
	int StoreMemberLL(Value *instance, String *member);

	// argCount DOES NOT include the instance, but args DOES
	int LoadIndexerLL(uint32_t argCount, Value *args, Value *dest);
	// argCount DOES NOT include the instance or the value being assigned, but args DOES
	int StoreIndexerLL(uint32_t argCount, Value *args);

	int LoadFieldRefLL(Value *inst, Field *field);
	int LoadMemberRefLL(Value *inst, String *member);

	int InvokeOperatorLL(Value *args, Operator op, Value *result);
	int EqualsLL(Value *args, bool &result);
	int CompareLL(Value *args, Value *result);
	int ConcatLL(Value *args, Value *result);

	// Specialised comparers! For speed.
	int CompareLessThanLL(Value *args, bool &result);
	int CompareGreaterThanLL(Value *args, bool &result);
	int CompareLessEqualsLL(Value *args, bool &result);
	int CompareGreaterEqualsLL(Value *args, bool &result);

	int ThrowMissingOperatorError(Operator op);

	int InitializeMethod(MethodOverload *method);
	int CallStaticConstructors(instr::MethodBuilder &builder);

	MethodOverload *GetHashIndexerSetter();

	friend class GC;
	friend class VM;
	friend class Type;
	friend class MethodInitializer;
	friend String *::VM_GetStackTrace(ThreadHandle);
};

} // namespace ovum

#endif // VM__THREAD_INTERNAL_H