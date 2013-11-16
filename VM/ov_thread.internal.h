#pragma once

#ifndef VM__THREAD_INTERNAL_H
#define VM__THREAD_INTERNAL_H

#include <cassert>
#include "ov_vm.internal.h"
#include "ov_stringbuffer.internal.h"

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

// The total size of a call stack.
#define CALL_STACK_SIZE    1024*1024


typedef struct StackFrame_S StackFrame;
typedef struct StackFrame_S
{
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
	Method::Overload *method;

	inline void Init(uint32_t stackCount, uint32_t argc,
		Value *evalStack, uint8_t *prevInstr,
		StackFrame *prevFrame, Method::Overload *method)
	{
		this->stackCount = stackCount;
		this->argc       = argc;
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
	inline void PushNull()
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

#define STACK_FRAME_SIZE     ALIGN_TO(sizeof(::StackFrame), sizeof(::Value))
// The base of the locals array, relative to a stack frame base pointer.
#define LOCALS_OFFSET(sf)    reinterpret_cast<::Value*>((char*)(sf) + STACK_FRAME_SIZE)


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
	NONE              = 0x00,
	// The thread is in a fully native region.
	// See VM_EnterFullyNativeRegion for details.
	IN_NATIVE_REGION  = 0x01,
};
ENUM_OPS(ThreadFlags, int);

class StackManager; // used by the method initializer

class MethodInitException : public std::exception
{
public:
	enum FailureKind
	{
		GENERAL = 0, // no extra information
		INCONSISTENT_STACK_HEIGHT,
		INVALID_BRANCH_OFFSET,
		INSUFFICIENT_STACK_HEIGHT,
		INACCESSIBLE_MEMBER,
		FIELD_STATIC_MISMATCH,
		UNRESOLVED_TOKEN_ID,
		NO_MATCHING_OVERLOAD,
		INACCESSIBLE_TYPE,
	};

private:
	FailureKind kind;
	Method::Overload *method;

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
	inline MethodInitException(const char *const message, Method::Overload *method) :
		exception(message), method(method), kind(GENERAL)
	{ }

	inline MethodInitException(const char *const message, Method::Overload *method, int32_t instrIndex, FailureKind kind) :
		exception(message), method(method), kind(kind), instrIndex(instrIndex)
	{ }

	inline MethodInitException(const char *const message, Method::Overload *method, Member *member, FailureKind kind) :
		exception(message), method(method), kind(kind), member(member)
	{ }

	inline MethodInitException(const char *const message, Method::Overload *method, Type *type, FailureKind kind) :
		exception(message), method(method), kind(kind), type(type)
	{ }

	inline MethodInitException(const char *const message, Method::Overload *method, uint32_t tokenId, FailureKind kind) :
		exception(message), method(method), kind(kind), tokenId(tokenId)
	{ }

	inline MethodInitException(const char *const message, Method::Overload *method,
		Method *methodGroup, uint32_t argCount, FailureKind kind) :
		exception(message), method(method), kind(kind)
	{
		noOverload.methodGroup = methodGroup;
		noOverload.argCount = argCount;
	}

	inline FailureKind GetFailureKind() const { return kind; }

	inline Method::Overload *GetMethod() const { return method; }

	inline int32_t GetInstructionIndex() const { return instrIndex; }
	inline Member *GetMember() const { return member; }
	inline Type *GetType() const { return type; }
	inline uint32_t GetTokenId() const { return tokenId; }
	inline Method *GetMethodGroup() const { return noOverload.methodGroup; }
	inline uint32_t GetArgumentCount() const { return noOverload.argCount; }
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
	CRITICAL_SECTION gcCycleSection;

public:
	inline void Push      (Value    value) { currentFrame->Push(value);       }
	inline void PushBool  (bool     value) { currentFrame->PushBool(value);   }
	inline void PushInt   (int64_t  value) { currentFrame->PushInt(value);    }
	inline void PushUInt  (uint64_t value) { currentFrame->PushUInt(value);   }
	inline void PushReal  (double   value) { currentFrame->PushReal(value);   }
	inline void PushString(String  *value) { currentFrame->PushString(value); }
	inline void PushNull  ()               { currentFrame->PushNull();        }

	inline Value Pop() { return currentFrame->Pop(); }
	inline void Pop(unsigned int n) { currentFrame->Pop(n); }

	inline void Dup() { currentFrame->Push(currentFrame->Peek()); }

	inline Value *Local(const unsigned int n) { return LOCALS_OFFSET(currentFrame) + n; }

	// argCount does NOT include the instance.
	void Invoke(uint32_t argCount, Value *result);
	// argCount DOES NOT include the instance.
	void InvokeMethod(Method *method, uint32_t argCount, Value *result);
	// argCount does NOT include the instance.
	void InvokeMember(String *name, uint32_t argCount, Value *result);
	void InvokeOperator(Operator op, Value *result);
	void InvokeApply(Value *result);
	void InvokeApplyMethod(Method *method, Value *result);

	bool Equals();
	int64_t Compare();
	void Concat(Value *result);

	void LoadMember(String *member, Value *result);
	void StoreMember(String *member);

	// Note: argCount does NOT include the instance.
	void LoadIndexer(uint32_t argCount, Value *result);
	// Note: argCount does NOT include the instance or the value that's being stored.
	void StoreIndexer(uint32_t argCount);

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
	void ThrowNoOverloadError(const uint32_t argCount, String *message = nullptr) throw(OvumException);
#pragma warning(pop)

	bool IsSuspendedForGC() const;

	void EnterFullyNativeRegion();
	void LeaveFullyNativeRegion();
	inline bool IsInFullyNativeRegion() const
	{
		return (flags & ThreadFlags::IN_NATIVE_REGION) == ThreadFlags::IN_NATIVE_REGION;
	}

private:
	void InitCallStack();
	void DisposeCallStack();

	void InitGCLock();
	void DisposeGCLock();

	// Pushes a new stack frame onto the call stack representing a call
	// to the specified method.
	//   argCount:
	//     The number of arguments passed to the method, INCLUDING the instance.
	//   method:
	//     The overload that is being invoked in the stack frame.
	template<bool First>
	StackFrame *PushStackFrame(const uint32_t argCount, Value *args, Method::Overload *method);

	void PrepareVariadicArgs(const MethodFlags flags, const uint32_t argCount, const uint32_t paramCount, StackFrame *frame);

	// Tells the thread to suspend itself as soon as possible.
	// Thread::IsSuspendedForGC() returns true when this is done.
	void PleaseSuspendForGCAsap();
	// Tells the thread it doesn't have to suspend itself anymore,
	// because the GC cycle has ended. This happens when the thread
	// spends the entire GC cycle in a fully native section.
	void EndGCSuspension();
	void SuspendForGC();

	void Evaluate(StackFrame *frame);
	bool FindErrorHandler(StackFrame *frame);
	void EvaluateLeave(StackFrame *frame, const int32_t target);

	String *GetStackTrace();
	void AppendArgumentType(StringBuffer &buf, Value arg);

	// argCount DOES NOT include the value to be invoked, but value does.
	void InvokeLL(unsigned int argCount, Value *value, Value *result);
	// args DOES include the instance, argCount DOES NOT
	void InvokeMethodOverload(Method::Overload *mo, unsigned int argCount, Value *args, Value *result, const bool ignoreVariadic = false);

	void InvokeApplyLL(Value *args, Value *result);
	void InvokeApplyMethodLL(Method *method, Value *args, Value *result);

	void InvokeMemberLL(String *name, uint32_t argCount, Value *value, Value *result);

	void LoadMemberLL(Value *instance, String *member, Value *result);
	void StoreMemberLL(Value *instance, String *member);

	// argCount DOES NOT include the instance, but args DOES
	void LoadIndexerLL(uint32_t argCount, Value *args, Value *dest);
	// argCount DOES NOT include the instance or the value being assigned, but args DOES
	void StoreIndexerLL(uint32_t argCount, Value *args);

	void InvokeOperatorLL(Value *args, Operator op, Value *result);
	bool EqualsLL(Value *args);
	int64_t CompareLL(Value *args);
	void ConcatLL(Value *args, Value *result);

	// Specialised comparers! For speed.
	bool CompareLessThanLL(Value *args);
	bool CompareGreaterThanLL(Value *args);
	bool CompareLessEqualsLL(Value *args);
	bool CompareGreaterEqualsLL(Value *args);

	void ThrowMissingOperatorError(Operator op);

	void InitializeMethod(Method::Overload *method);
	void InitializeInstructions(instr::MethodBuilder &builder, Method::Overload *method);
	static void InitializeBranchOffsets(instr::MethodBuilder &builder, Method::Overload *method);
	static void CalculateStackHeights(instr::MethodBuilder &builder, Method::Overload *method, StackManager &stack);
	static void WriteInitializedBody(instr::MethodBuilder &builder, Method::Overload *method);
	void CallStaticConstructors(instr::MethodBuilder &builder);

	// These are used by the initializer
	static Type *TypeFromToken(Method::Overload *fromMethod, uint32_t token);
	static String *StringFromToken(Method::Overload *fromMethod, uint32_t token);
	static Method *MethodFromToken(Method::Overload *fromMethod, uint32_t token);
	static Method::Overload *MethodOverloadFromToken(Method::Overload *fromMethod, uint32_t token, uint32_t argCount);
	static Field *FieldFromToken(Method::Overload *fromMethod, uint32_t token, bool shouldBeStatic);

	friend class GC;
	friend void VM_InvokeMethod(ThreadHandle, MethodHandle, const uint32_t, Value*);
	friend String *VM_GetStackTrace(ThreadHandle);
};

#endif // VM__THREAD_INTERNAL_H