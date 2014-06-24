#pragma once

#ifndef VM__THREAD_INTERNAL_H
#define VM__THREAD_INTERNAL_H

#include <cassert>
#include <exception>
#include "ov_vm.internal.h"
#include "ov_stringbuffer.internal.h"
#include "sync.internal.h"

#ifdef THREADED_EVALUATION
#ifndef __GNUC__
#error You must compile with gcc for threaded evaluation
#endif
#endif

class StringBuffer;
namespace instr
{
	class MethodBuilder;
}


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

	inline void Push      (Value *value)
	{
		evalStack[stackCount++] = *value;
	}
	inline void PushBool  (bool value)
	{
		SetBool_(evalStack + stackCount++, value);
	}
	inline void PushInt   (int64_t value)
	{
		SetInt_(evalStack + stackCount++, value);
	}
	inline void PushUInt  (uint64_t value)
	{
		SetUInt_(evalStack + stackCount++, value);
	}
	inline void PushReal  (double value)
	{
		SetReal_(evalStack + stackCount++, value);
	}
	inline void PushString(String *value)
	{
		SetString_(evalStack + stackCount++, value);
	}
	inline void PushNull  ()
	{
		SetNull_(evalStack + stackCount++);
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
	inline Type *PeekType(unsigned int n = 0) const
	{
		assert(n <= stackCount);
		return evalStack[stackCount - n - 1].type;
	}
	inline String *PeekString(unsigned int n = 0) const
	{
		assert(n <= stackCount);
		return evalStack[stackCount - n - 1].common.string;
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

static const size_t STACK_FRAME_SIZE = ALIGN_TO(sizeof(::StackFrame), 8);

inline Value *StackFrame::Locals() const
{
	return reinterpret_cast<Value*>((char*)this + STACK_FRAME_SIZE);
}


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
ENUM_OPS(ThreadFlags, int);

class StackManager; // used by the method initializer

class MethodInitException : public std::exception
{
public:
	enum FailureKind
	{
		GENERAL = 0, // no extra information
		INCONSISTENT_STACK,
		INVALID_BRANCH_OFFSET,
		INSUFFICIENT_STACK_HEIGHT,
		STACK_HAS_REFS,
		INACCESSIBLE_MEMBER,
		FIELD_STATIC_MISMATCH,
		UNRESOLVED_TOKEN_ID,
		NO_MATCHING_OVERLOAD,
		INACCESSIBLE_TYPE,
		TYPE_NOT_CONSTRUCTIBLE,
	};

private:
	FailureKind kind;
	MethodOverload *method;

	union {
		int32_t instrIndex;
		Member *member;
		Type *type;
		uint32_t tokenId;
		struct {
			Method *methodGroup;
			uint32_t argCount;
		} noOverload;
	};

public:
	inline MethodInitException(const char *const message, MethodOverload *method) :
		exception(message), method(method), kind(GENERAL)
	{ }

	inline MethodInitException(const char *const message, MethodOverload *method, int32_t instrIndex, FailureKind kind) :
		exception(message), method(method), kind(kind), instrIndex(instrIndex)
	{ }

	inline MethodInitException(const char *const message, MethodOverload *method, Member *member, FailureKind kind) :
		exception(message), method(method), kind(kind), member(member)
	{ }

	inline MethodInitException(const char *const message, MethodOverload *method, Type *type, FailureKind kind) :
		exception(message), method(method), kind(kind), type(type)
	{ }

	inline MethodInitException(const char *const message, MethodOverload *method, uint32_t tokenId, FailureKind kind) :
		exception(message), method(method), kind(kind), tokenId(tokenId)
	{ }

	inline MethodInitException(const char *const message, MethodOverload *method,
		Method *methodGroup, uint32_t argCount, FailureKind kind) :
		exception(message), method(method), kind(kind)
	{
		noOverload.methodGroup = methodGroup;
		noOverload.argCount = argCount;
	}

	inline FailureKind GetFailureKind() const { return kind; }

	inline MethodOverload *GetMethod() const { return method; }

	inline int32_t GetInstructionIndex() const { return instrIndex; }
	inline Member *GetMember() const { return member; }
	inline Type *GetType() const { return type; }
	inline uint32_t GetTokenId() const { return tokenId; }
	inline Method *GetMethodGroup() const { return noOverload.methodGroup; }
	inline uint32_t GetArgumentCount() const { return noOverload.argCount; }
};

class Thread
{
private:
	// The size of the managed call stack
	static const size_t CALL_STACK_SIZE = 1024 * 1024;

public:
	Thread(int &status);
	~Thread();

	int Start(unsigned int argCount, MethodOverload *mo, Value &result);

private:
	// The current instruction pointer. This should always be the first field in the class.
	uint8_t *ip;

	// For obtaining the current frame from the call stack.
	// Stack frames grow up, towards higher addresses.
	// NOTE: This is relative to the base of the StackFrame*! The arguments precede
	// the base of said pointer.
	StackFrame *currentFrame;

	// Set to true if the GC has asked the thread to suspend itself.
	volatile bool shouldSuspendForGC;

	// The current state of the thread. And what a state it's in. Tsk tsk tsk.
	ThreadState state;

	// Various thread flags.
	ThreadFlags flags;

	// The call stack. This grows towards higher addresses.
	// Note: although this represents a bunch of unspecific bytes, giving the type
	// as unsigned char allows us to add and subtract from the address of this field.
	unsigned char *callStack;

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

public:
	inline void Push      (Value   *value) { currentFrame->Push(value);       }
	inline void PushBool  (bool     value) { currentFrame->PushBool(value);   }
	inline void PushInt   (int64_t  value) { currentFrame->PushInt(value);    }
	inline void PushUInt  (uint64_t value) { currentFrame->PushUInt(value);   }
	inline void PushReal  (double   value) { currentFrame->PushReal(value);   }
	inline void PushString(String  *value) { currentFrame->PushString(value); }
	inline void PushNull  ()               { currentFrame->PushNull();        }

	inline Value Pop() { return currentFrame->Pop(); }
	inline void Pop(unsigned int n) { currentFrame->Pop(n); }

	inline void Dup()
	{
		Value *ptr = currentFrame->evalStack + currentFrame->stackCount++;
		*(ptr + 1) = *ptr;
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

	// Note: argCount does NOT include the instance.
	int LoadIndexer(uint32_t argCount, Value *result);
	// Note: argCount does NOT include the instance or the value that's being stored.
	int StoreIndexer(uint32_t argCount);

	void LoadStaticField(Field *field, Value *result);
	void StoreStaticField(Field *field);

	int ToString(String **result);

	int Throw(bool rethrow = false);

	// Throw helpers!

	NOINLINE int ThrowError(String *message = nullptr);
	NOINLINE int ThrowTypeError(String *message = nullptr);
	NOINLINE int ThrowMemoryError(String *message = nullptr);
	NOINLINE int ThrowOverflowError(String *message = nullptr);
	NOINLINE int ThrowDivideByZeroError(String *message = nullptr);
	NOINLINE int ThrowNullReferenceError(String *message = nullptr);
	NOINLINE int ThrowMemberNotFoundError(String *member);
	NOINLINE int ThrowNoOverloadError(uint32_t argCount, String *message = nullptr);

	bool IsSuspendedForGC() const;

	void EnterUnmanagedRegion();
	void LeaveUnmanagedRegion();
	inline bool IsInUnmanagedRegion() const
	{
		return (flags & ThreadFlags::IN_UNMANAGED_REGION) == ThreadFlags::IN_UNMANAGED_REGION;
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

	// Tells the thread to suspend itself as soon as possible.
	// Thread::IsSuspendedForGC() returns true when this is done.
	NOINLINE void PleaseSuspendForGCAsap();
	// Tells the thread it doesn't have to suspend itself anymore,
	// because the GC cycle has ended. This happens when the thread
	// spends the entire GC cycle in a fully native section.
	NOINLINE void EndGCSuspension();
	NOINLINE void SuspendForGC();

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
	static void InitializeInstructions(instr::MethodBuilder &builder, MethodOverload *method);
	static void InitializeBranchOffsets(instr::MethodBuilder &builder, MethodOverload *method);
	static void CalculateStackHeights(instr::MethodBuilder &builder, MethodOverload *method, StackManager &stack);
	static void WriteInitializedBody(instr::MethodBuilder &builder, MethodOverload *method);
	int CallStaticConstructors(instr::MethodBuilder &builder);

	// These are used by the initializer
	static Type *TypeFromToken(MethodOverload *fromMethod, uint32_t token);
	static String *StringFromToken(MethodOverload *fromMethod, uint32_t token);
	static Method *MethodFromToken(MethodOverload *fromMethod, uint32_t token);
	static MethodOverload *MethodOverloadFromToken(MethodOverload *fromMethod, uint32_t token, uint32_t argCount);
	static Field *FieldFromToken(MethodOverload *fromMethod, uint32_t token, bool shouldBeStatic);
	static void EnsureConstructible(Type *type, uint32_t argCount, MethodOverload *fromMethod);

	static void GetHashIndexerSetter(MethodOverload **target);

	friend class GC;
	friend class VM;
	friend String *VM_GetStackTrace(ThreadHandle);
	friend int VM_GetStackDepth(ThreadHandle);
	friend OverloadHandle VM_GetCurrentOverload(ThreadHandle);
};

#endif // VM__THREAD_INTERNAL_H