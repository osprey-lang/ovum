#pragma once

#include "../vm.h"
#include "vm.h"
#include "stackframe.h"
#include "../threading/sync.h"
#include "../threading/tls.h"

namespace ovum
{

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
	// The thread has stopped, either from having its startup method return,
	// or from being killed.
	STOPPED         = 0x03,
};

enum class ThreadFlags : int
{
	NONE                = 0x00,
	// The thread is in an unmanaged region.
	// See Thread::EnterUnmanagedRegion() for details.
	IN_UNMANAGED_REGION = 0x01,
};
OVUM_ENUM_OPS(ThreadFlags, int);

class Thread
{
public:
	OVUM_NOINLINE static int New(VM *owner, Box<Thread> &result);

	// Gets the currently executing managed thread. If no managed thread is associated with
	// the current native thread, returns null.
	static Thread *GetCurrent();

	~Thread();

	// Gets the current instruction pointer.
	inline const void *GetInstructionPointer() const
	{
		return ip;
	}

	// Gets the current stack frame.
	inline const StackFrame *GetCurrentFrame() const
	{
		return currentFrame;
	}

	// Determines whether the thread is currently in an unmanaged region.
	//
	// See EnterUnmanagedRegion() for details.
	inline bool IsInUnmanagedRegion() const
	{
		return (flags & ThreadFlags::IN_UNMANAGED_REGION) == ThreadFlags::IN_UNMANAGED_REGION;
	}

	// Determines whether the thread is currently in a state that allows the GC
	// to proceed with garbage collection.
	inline bool IsSuspendedForGC() const
	{
		return state == ThreadState::SUSPENDED_BY_GC || IsInUnmanagedRegion();
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

	// Pushes a value onto the current evaluation stack.
	//   value:
	//     The value to push. This pointer MUST NOT be null.
	// Stack change:
	//   +1: Pushes the value.
	inline void Push(Value *value)
	{
		*currentFrame->NextStackSlot() = *value;
	}

	// Pushes a Boolean value onto the current evaluation stack.
	//   value:
	//     The raw boolean value.
	// Stack change:
	//   +1: Pushes the value.
	inline void PushBool(bool value)
	{
		Value *top = currentFrame->NextStackSlot();
		top->type = vm->types.Boolean;
		top->v.integer = value;
	}

	// Pushes an Int value onto the current evaluation stack.
	//   value:
	//     The raw 64-bit int value.
	// Stack change:
	//   +1: Pushes the value.
	inline void PushInt(int64_t value)
	{
		Value *top = currentFrame->NextStackSlot();
		top->type = vm->types.Int;
		top->v.integer = value;
	}

	// Pushes a UInt value onto the current evaluation stack.
	//   value:
	//     The raw 64-bit uint value.
	// Stack change:
	//   +1: Pushes the value.
	inline void PushUInt(uint64_t value)
	{
		Value *top = currentFrame->NextStackSlot();
		top->type = vm->types.UInt;
		top->v.uinteger = value;
	}

	// Pushes a Real value onto the current evaluation stack.
	//   value:
	//     The raw 64-bit floating-point value.
	// Stack change:
	//   +1: Pushes the value.
	inline void PushReal(double value)
	{
		Value *top = currentFrame->NextStackSlot();
		top->type = vm->types.Real;
		top->v.real = value;
	}

	// Pushes a String value onto the current evaluation stack.
	//   value:
	//     The raw string value. This pointer MUST NOT be null.
	// Stack change:
	//   +1: Pushes the value.
	inline void PushString(String *value)
	{
		Value *top = currentFrame->NextStackSlot();
		top->type = vm->types.String;
		top->v.string = value;
	}

	// Pushes the null value onto the current evaluation stack.
	//
	// Stack change:
	//   +1: Pushes the null value.
	inline void PushNull()
	{
		currentFrame->NextStackSlot()->type = nullptr;
	}

	// Pops a single value off the current evaluation stack and returns it. This
	// method must not be called with an empty evaluation stack.
	//
	// Stack change:
	//   -1: Pops a single value.
	inline Value Pop()
	{
		return currentFrame->Pop();
	}
	// Pops an arbitrary number of values off the current evaluation stack. None
	// of the values are returned.
	//   n:
	//     The number of values to pop. This value must not be greater than the
	//     stack height.
	// Stack change:
	//   -n: Pops n values.
	inline void Pop(ovlocals_t n)
	{
		currentFrame->Pop(n);
	}

	// Duplicates the value on top of the current evaluation stack.
	//
	// Stack change:
	//   +1: Pops the value, then pushes it twice. For performance reasons, the
	//       method actually just pushes the top value.
	inline void Dup()
	{
		Value *top = currentFrame->NextStackSlot();
		*top = *(top - 1);
	}

	// Gets a reference to the specified local value in the current stack frame.
	//   n:
	//     The local variable index, starting at 0. This value must be less than
	//     the number of local variables in the current method, otherwise the
	//     returned pointer will point into the evaluation stack, or possibly
	//     at garbage.
	inline Value *Local(ovlocals_t n)
	{
		return currentFrame->Locals() + n;
	}
	
	// Starts the thread. The thread's state must be ThreadState::CREATED; that
	// is, it must not be running or have finished execution.
	//
	// If the startup method takes any arguments, they must be pushed onto the
	// evaluation stack before calling this method.
	//   argCount:
	//     The number of arguments to call the startup method with. The startup
	//     method cannot be an instance method, so this value never includes an
	//     instance.
	//   mo:
	//     The method overload to invoke. The caller is responsible for method
	//     overload resolution, as well as verifying that the method is not an
	//     instance method and does not have any ref parameters.
	//   result:
	//     A location that receives the return value of the call.
	int Start(ovlocals_t argCount, MethodOverload *mo, Value &result);

	// Invokes a value on the stack with arguments from the stack. The value to
	// be invoked can be any invokable value, but typically an aves.Method
	// instance.
	//   argCount:
	//     The number of arguments to invoke the value with, EXCLUDING the value
	//     itself.
	//   result:
	//     A location that receives the return value of the call. If null, the
	//     return value is pushed onto the current evaluation stack.
	// Stack change:
	//   Pops the arguments, including the value, then possibly pushes the return
	//   value.
	int Invoke(ovlocals_t argCount, Value *result);

	// Invokes the specified method with arguments from the stack.
	//   method:
	//     The method to invoke.
	//   argCount:
	//     The number of arguments to invoke the method with, EXCLUDING the
	//     instance (if any).
	//   result:
	//     A location that receives the return value of the call. If null, the
	//     return value is pushed onto the current evaluation stack.
	// Stack change:
	//   Pops the arguments, including the instance (if any), then possibly pushes
	//   the return value.
	int InvokeMethod(Method *method, ovlocals_t argCount, Value *result);

	// Invokes the specified member of a value on the stack, with arguments from
	// the stack. If the member is not a method, the value of the member will
	// first be loaded. As a result, a property getter may be invoked.
	//
	// This method cannot be used to invoke a static member.
	//   name:
	//     The name of the member to invoke.
	//   argCount:
	//     The number of arguments to invoke the member with, 
	//   result:
	//     A location that receives the return value of the call. If null, the
	//     return value is pushed onto the current evaluation stack.
	// Stack change:
	//   Pops the arguments, including the instance, then possibly pushes the
	//   return value.
	int InvokeMember(String *name, ovlocals_t argCount, Value *result);

	// Invokes the specified operator with arguments from the stack.
	//   op:
	//     The operator to invoke. The argument count is determined by the
	//     arity of the operator. As all operators are either unary or binary,
	//     that means 1 or 2 arguments. The operator's implementing method is
	//     always loaded from the first argument.
	//   result:
	//     A location that receives the return value of the call. If null, the
	//     return value is pushed onto the current evaluation stack.
	// Stack change:
	//   Pops the arguments (-1 or -2), then possibly pushes the result.
	int InvokeOperator(Operator op, Value *result);

	// Applies a List of arguments on the stack to a value. The value to be
	// invoked can be any invokable value, but typically an aves.Method instance.
	//   result:
	//     A location that receives the return value of the call. If null, the
	//     return value is pushed onto the current evaluation stack.
	// Stack change:
	//   Pops the value and list (-2), then possibly pushes the result.
	int InvokeApply(Value *result);

	// Applies a List of arguments on the stack to the specified method.
	//   method:
	//     The method to be invoked. InvokeApplyMethod() performs method overload
	//     resolution based on the number of values in the arguments list.
	//   result:
	//     A location that receives the return value of the call. If null, the
	//     return value is pushed onto the current evaluation stack.
	// Stack change:
	//   Pops the list (-1), then possibly pushes the result.
	int InvokeApplyMethod(Method *method, Value *result);

	// Determines whether two values on the stack are equal according to the '=='
	// operator (Operator::EQ).
	//   result:
	//     A location that receives the result of the comparison. This pointer
	//     MUST NOT be null.
	// Stack change:
	//   -2: Pops the two arguments.
	int Equals(bool *result);

	// Compares two values on the stack for ordering, using the '<=>' operator
	// (Operator::CMP).
	//   result:
	//     A location that receives the result of the comparison. This pointer
	//     MUST NOT be null.
	// Stack change:
	//   -2: Pops the two arguments.
	int Compare(int64_t *result);

	// Concatenates two values on the stack together into a string. Equivalent
	// to Osprey's '::' operator.
	//   result:
	//     A location that receives the concatenated string. If null, the value
	//     is pushed onto the current evaluation stack.
	// Stack change:
	//   -2: Pops the two values.
	int Concat(Value *result);

	// Loads the member with the specified name from a value on the stack. Any
	// kind of member can be loaded. If the member is a property, the getter
	// will be invoked with zero arguments. If the member is a method, an
	// aves.Method object will be constructed.
	//
	// This method cannot be used to load a static member.
	//   member:
	//     The name of the member to load.
	//   result:
	//     A location that receives the member value. If null, the value is pushed
	//     onto the current evaluation stack.
	// Stack change:
	//   Pops the instance, then possibly pushes the member value.
	int LoadMember(String *member, Value *result);

	// Stores a value on the stack in the member with the specified name. Only
	// fields and writable properties can be written to. If the member is a
	// property, its setter will be invoked with one argument (the value). The
	// return value of such a call is discarded.
	//
	// This method cannot be used to write to a static member.
	//   member:
	//     The name of the member to write to.
	// Stack change:
	//   -2: Pops the instance and the stored value.
	int StoreMember(String *member);

	// Loads the specified field from a value on the stack.
	//
	// This method cannot be used to load a static field; see LoadStaticField().
	//   field:
	//     The field whose value to load.
	//   result:
	//     A location that receives the field value. If null, the value is pushed
	//     onto the current evaluation stack.
	// Stack change:
	//   Pops the instance, then possibly pushes the field value.
	int LoadField(Field *field, Value *result);

	// Stores a value on the stack into the specified field.
	//
	// This method cannot be used to write to a static field; see StoreStaticField().
	//   field:
	//     The field to write to.
	// Stack change:
	//   -2: Pops the instance and the stored value.
	int StoreField(Field *field);

	// Invokes the indexer getter of a value on the stack.
	//   argCount:
	//     The number of arguments to pass to the indexer, EXCLUDING the instance.
	//   result:
	//     A location that receives the return value of the indexer access. If
	//     null, the result is pushed onto the current evaluation stack.
	// Stack change:
	//   <0: Pops the arguments to the indexer, including the instance, then
	//       possibly pushes the result.
	int LoadIndexer(ovlocals_t argCount, Value *result);

	// Invokes the indexer setter of a value on the stack.
	//   argCount:
	//     The number of arguments to pass to the indexer, EXCLUDING both the
	//     instance and the value to be stored.
	// Stack change:
	//   <0: Pops the arguments to the indexer, including the instance and the
	//       stored value.
	int StoreIndexer(ovlocals_t argCount);

	// Loads the value of the specified static field.
	//
	// This method cannot be used to load an instance field.
	//   field:
	//     The field whose value to load.
	//   result:
	//     A location that receives the field value. If null, the value is pushed
	//     onto the current evaluation stack.
	// Stack change:
	//   Possibly pushes the field value.
	int LoadStaticField(Field *field, Value *result);

	// Stores a value on the stack into the specified static field.
	//
	// This method cannot be used to write to an instance field.
	//   field:
	//     The field to write to.
	// Stack change:
	//   -1: Pops the stored value.
	int StoreStaticField(Field *field);

	// Converts the value on top of the stack to a string, by calling 'toString'
	// with zero arguments.
	//   result:
	//     A location that receives the string value. If null, the value is pushed
	//     onto the current evaluation stack.
	// Stack change:
	//   Pops the value, then possibly pushes the resulting string.
	int ToString(String **result);

	// Gets a string with the current stack trace. See StackTraceFormatter for
	// details on the format of the result.
	String *GetStackTrace();

	// Throws the value on top of the stack. If the value is not an aves.Error,
	// an Error is first created, and the thrown value put in its 'data' member.
	//
	// The thrown Error's stack trace is initialized to the current stack trace,
	// as returned by GetStackTrace(), unless 'rethrow' is true.
	//   rethrow:
	//     (optional) If true, rethrows 'currentError' instead of taking an error
	//     from the evaluation stack. Rethrowing can only be performed inside a
	//     catch clause.
	OVUM_NOINLINE int Throw(bool rethrow = false);

	// Throw helpers!

	// Constructs and throws an error of type aves.Error.
	//   message:
	//     (optional) The error message. Pass null to use the default message.
	OVUM_NOINLINE int ThrowError(String *message = nullptr);

	// Constructs and throws an error of type aves.TypeError.
	//   message:
	//     (optional) The error message. Pass null to use the default message.
	OVUM_NOINLINE int ThrowTypeError(String *message = nullptr);

	// Constructs and throws an error of type aves.MemoryError.
	//   message:
	//     (optional) The error message. Pass null to use the default message.
	OVUM_NOINLINE int ThrowMemoryError(String *message = nullptr);

	// Constructs and throws an error of type aves.OverflowError.
	//   message:
	//     (optional) The error message. Pass null to use the default message.
	OVUM_NOINLINE int ThrowOverflowError(String *message = nullptr);

	// Constructs and throws an error of type aves.DivideByZeroError.
	//   message:
	//     (optional) The error message. Pass null to use the default message.
	OVUM_NOINLINE int ThrowDivideByZeroError(String *message = nullptr);

	// Constructs and throws an error of type aves.NullReferenceError.
	//   message:
	//     (optional) The error message. Pass null to use the default message.
	OVUM_NOINLINE int ThrowNullReferenceError(String *message = nullptr);

	// Constructs and throws an error of type aves.TypeConversionError.
	//   message:
	//     (optional) The error message. Pass null to use the default message.
	OVUM_NOINLINE int ThrowTypeConversionError(String *message = nullptr);

	// Constructs and throws an error of type aves.MemberNotFoundError.
	//   message:
	//     (optional) The error message. Pass null to use the default message.
	OVUM_NOINLINE int ThrowMemberNotFoundError(String *member);

	// Constructs and throws an error of type aves.NoOverloadError.
	//   argCount:
	//     The number of arguments the overload was attempted to be invoked with,
	//     excluding the instance (if any).
	//   message:
	//     (optional) The error message. Pass null to use the default message.
	OVUM_NOINLINE int ThrowNoOverloadError(ovlocals_t argCount, String *message = nullptr);

	// Informs the thread that native code is about to stop interacting with the
	// managed runtime in any way. This is typically done before performing some
	// possibly lengthy operation, such as opening a file or receiving from a
	// socket.
	//
	// While a thread is in an unmanaged region, the GC assumes that the thread
	// will not interact with the managed runtime. Hence, it will not affect the
	// lifetime of managed objects. The GC proceeds with garbage collection even
	// though the thread is not actually suspended. This means the GC doesn't
	// have to block all the whole system until one slow thread finishes.
	//
	// Since threads inside an unmanaged region aren't supposed to interact with
	// the managed runtime, unmanaged regions cannot be entered recursively.
	void EnterUnmanagedRegion();

	// Leaves the current unmanaged region. If the GC is running, the thread will
	// suspend itself until the GC is finished. Otherwise, this method returns
	// immediately.
	void LeaveUnmanagedRegion();

private:
	struct ErrorStack;

	// The currently executing managed thread.
	static TlsEntry<Thread> threadKey;

	// The current instruction pointer. This should always be the first field in
	// the class.
	uint8_t *ip;

	// The current managed stack frame.
	//
	// NOTE: The arguments to the method precede the stack frame pointer.
	StackFrame *currentFrame;

	// If another thread is waiting for this thread to perform a specific action, this field
	// is set to an appropriate request.
	//
	// NOTE: Only one request can be active at a time. Currently this field is only used by
	// the GC, but that may change in the future.
	volatile ThreadRequest pendingRequest;

	// The managed call stack. This grows towards higher addresses as stack frames
	// are pushed onto it.
	//
	// Each stack frame consists of zero or more Values, laid out consecutively,
	// containing the arguments to the method; followed by a StackFrame; followed
	// by the local variables and evaluation stack, all laid out as consecutive
	// Values.
	//
	// This value has the type uint8_t* only so that byte offsets can be added and
	// subtracted without excessive casting.
	uint8_t *callStack;

	// The native ID of this thread.
	os::ThreadId nativeId;

	// The current execution state of the thread.
	//
	// A thread starts out in the CREATED state. Upon entering the startup method
	// (through a call to Thread::Start()), the thread becomes RUNNING. Once the
	// startup method returns, the state goes to STOPPED.
	//
	// While the thread is RUNNING, the GC can suspend the thread to perform a
	// GC cycle. The thread state then goes to SUSPENDED_BY_GC. Threads in this
	// state are still considered alive and running, as they will resume normal
	// operations as soon as the GC is finished.
	ThreadState state;

	// Various thread flags, separate from the execution state.
	ThreadFlags flags;

	// The VM instance that owns this thread.
	VM *vm;

	// Shared string data, accessed in a lot of places.
	StaticStrings *strings;

	// The current error.
	//
	// If successfully caught, this is set to NULL_VALUE *after* the catch clause
	// has been exited. We need to do this after because the catch clause may be
	// discard the error and then later wish to rethrow it. Also, the GC examines
	// this field during a cycle, to mark the error contained within as live.
	Value currentError;
	// The current stack of saved error values.
	//
	// See ErrorStack for a detailed explanation of how this is used and why it is
	// necessary.
	//
	// NOTE: This field contains a pointer directly into the native call stack. It
	// is absolutely vital that it is restored before exiting the owning method.
	ErrorStack *errorStack;

	// The critical section that the thread tries to enter when the GC is running
	// on another thread. The GC enters this section first, and leaves it only when
	// GC cycle is complete, thus blocking the current thread for the duration of
	// the cycle.
	CriticalSection gcCycleSection;

	Thread(VM *owner, int &status);

	// Initializes the managed call stack. Returns OVUM_SUCCESS on success.
	int InitCallStack();

	// Destroys the managed call stack, called from the destructor.
	void DisposeCallStack();

	// If the thread has a pending request (see pendingRequest), handles it.
	OVUM_NOINLINE void HandleRequest();

	// Tells the thread to suspend itself as soon as possible. When the thread
	// is in a state that permits the GC to continue, Thread::IsSuspendedForGC()
	// will return true.
	//
	// This method is called by the GC.
	OVUM_NOINLINE void PleaseSuspendForGCAsap();

	// Tells the thread it doesn't have to suspend itself anymore, because the
	// GC cycle has ended. This can happen when the thread spends the entire GC
	// cycle in an unmanaged region.
	//
	// This method is called by the GC.
	OVUM_NOINLINE void EndGCSuspension();

	// Suspends the thread for the GC.
	//
	// The thread suspends itself by attempting to enter gcCycleSection. The GC
	// will have entered it first (it does so before attempting to suspend the
	// thread), which will cause this thread to wait for the GC cycle to finish,
	// at which point the GC leaves the section, allowing this thread to enter.
	OVUM_NOINLINE void SuspendForGC();

	// Pushes the first stack frame onto the call stack.
	//
	// The first stack frame has no method overload (StackFrame::method is null),
	// and is only used for its evaluation stack: the arguments to the thread's
	// startup method are pushed onto this first stack frame.
	void PushFirstStackFrame();

	// Pushes a new stack frame onto the call stack representing a call to the
	// specified method.
	//   argCount:
	//     The number of arguments passed to the method, INCLUDING the instance.
	//   args:
	//     Pointer to the first argument, which must be on the evaluation stack.
	//   method:
	//     The overload that is being invoked in the stack frame.
	void PushStackFrame(ovlocals_t argCount, Value *args, MethodOverload *method);

	// When invoking a method with a variadic parameter, packs arguments from
	// the evaluation stack into a List instance for the variadic parameter.
	//   argCount:
	//     The number of arguments passed to the method, EXCLUDING the instance.
	//   paramCount:
	//     The number of parameters the method expects, INCLUDING the variadic
	//     parameter.
	//   frame:
	//     The stack frame that contains the method arguments.
	int PrepareVariadicArgs(ovlocals_t argCount, ovlocals_t paramCount, StackFrame *frame);

	// Evaluates managed bytecode at the current instruction pointer, in the
	// current stack frame.
	//
	// This method assumes the instruction pointer is pointing at the starting
	// address of a valid intermediate bytecode instruction, as well as that a
	// suitable stack frame exists on the call stack. This method does NOT pop
	// the stack frame upon returning, nor does it guarantee any particular state
	// of the evaluation stack.
	//
	// This method is used when entering a managed call, to execute the method.
	// It is also used to evaluate finally blocks, which are effectively executed
	// in their own isolated context.
	int Evaluate();

	// Attempts to locate an error handler (catch clause) for a managed error that
	// has been thrown on the thread. The error is read from the 'currentError'
	// field of the current thread.
	//
	// This method assumes that the instruction pointer is pointing to the start
	// or middle of an instruction. If the instruction pointer goes past the end
	// of the instruction that caused the error, and that instruction happens to
	// be the last instruction in its containing try block, the IP will not be
	// considered to be inside the try block. Hence, that block's catch clauses
	// will not be found, nor will its finally block be executed.
	//   maxIndex:
	//     The maximum try block index to look at, exclusive. Pass -1 to examine
	//     all try blocks in the method.
	//
	//     Try blocks are stored inside out (most deeply nested try blocks first,
	//     followed by their parents). When an error is thrown inside a finally
	//     or fault clause, the index of that clause is passed in here, to ensure
	//     the containing try clause (if any) is not examined for catch clauses.
	//
	// Returns:
	//   Catch, finally and fault clauses can cause errors to be thrown. This
	//   method returns OVUM_SUCCESS if an error handler was successfully found
	//   and executed.
	int FindErrorHandler(int32_t maxIndex);

	// Attempts to evaluate a 'leave' instruction. This will execute any finally
	// and fault clauses that lie between the 'leave' instruction and the specified
	// target.
	//
	// This method assumes the instruction pointer is at the 'leave' instruction,
	// and will always add the size of the instruction's arguments to calculate
	// the actual target offset.
	//   frame:
	//     The current stack frame.
	//   target:
	//     The branch target, relative to the end of the 'leave' instruction.
	//
	// Returns:
	//   Finally and fault clauses can cause errors to be thrown and propagated
	//   past the protected region. This method returns OVUM_SUCCESS if all finally
	//   and fault clauses were successfully executed.
	int EvaluateLeave(StackFrame *frame, int32_t target);

	// Among the methods below, those suffixed with "LL" are "low-level" versions
	// of their non-LL brethren. These LL methods have special requirements and
	// usually make various assumptions, such as that certain pointers point to
	// something on the current stack frame's evaluation stack. They are mainly
	// used inside Evaluate(), where the aforementioned requirements are certain
	// to be met. A small number are called from friend classes, chiefly the GC.
	//
	// Note all public methods have LL equivalents. For example, LoadField() is
	// implemented in Evaluate() as a call directly to Field::ReadField(). All
	// LL methods have non-LL versions, however; call those as much as possible.
	//
	// All LL methods with "Invoke" in the name, and many without it, ultimately
	// call down to InvokeMethodOverload() at some stage, usually as the very
	// last step.

	// Invokes the specified method overload.
	//
	// This is the lowest-level invocation method. Upon calling this method, the
	// caller is assumed to have performed the necessary validation, such as ref
	// signature matching and method overload resolution.
	//   mo:
	//     The method overload to invoke.
	//   argCount:
	//     The number of arguments to invoke the method overload with, EXCLUDING
	//     the instance (if any).
	//   args:
	//     The arguments to pass into the method, INCLUDING the instance. This is
	//     assumed to point to the evaluation stack. The instance precedes the
	//     "regular" arguments.
	//   result:
	//     A location that receives the return value of the call.
	// Stack change:
	//   <0: Pops all the arguments, including the instance (if any).
	int InvokeMethodOverload(MethodOverload *mo, ovlocals_t argCount, Value *args, Value *result);

	// Invokes the specified value. The value to be invoked can be any invokable
	// value, but typically an aves.Method instance.
	//   argCount:
	//     The total number of arguments to pass to the value, EXCLUDING the
	//     value itself.
	//   value:
	//     A pointer to the value to be invoked. This is assumed to point to the
	//     evaluation stack. The arguments, if any, immediately follow the value.
	//   result:
	//     A location that receives the return value of the call.
	//   refSignature:
	//     The ref signature of the arguments. See RefSignature for details.
	// Stack change:
	//   <0: Pops all the arguments, including the value.
	int InvokeLL(ovlocals_t argCount, Value *value, Value *result, uint32_t refSignature);

	// Applies a List of arguments to a value. The value to be invoked can be any
	// invokable value, but typically an aves.Method instance.
	//   args:
	//     A pointer to the evaluation stack, pointing to the value to be invoked
	//     immediately followed by a List containing the arguments. The type of
	//     the list is verified by this method. This is assumed to point to the
	//     evaluation stack.
	//   result:
	//     A location that receives the return value of the call.
	// Stack change:
	//   -2: Pops the invoked value and the List.
	int InvokeApplyLL(Value *args, Value *result);

	// Applies a List of arguments to the specified method.
	//   method:
	//     The method to be invoked. InvokeApplyMethodLL() performs method overload
	//     resolution based on the number of values in the arguments list.
	//   args:
	//     A pointer to a List containing the arguments. The type of the list is
	//     verified by this method. This is assumed to point to the evaluation
	//     stack.
	//   result:
	//     A location that receives the return value of the call.
	// Stack change:
	//   -1: Pops the List.
	int InvokeApplyMethodLL(Method *method, Value *args, Value *result);

	// Applies a List of arguments to the specified member of a value. If the
	// member is not a method, the value of the member will first be loaded. As
	// a result, a property getter may be invoked.
	//
	// This method cannot be used to invoke a static member.
	//   name:
	//     The name of the member to invoke.
	//   argCount:
	//     The number of arguments to invoke the member with, EXCLUDING the
	//     instance.
	//   value:
	//     The value whose member is to be invoked, immediately followed by the
	//     arguments. This is assumed to point to the evaluation stack.
	//   result:
	//     A location that receives the return value of the call.
	//   refSignature:
	//     The ref signature of the arguments. See RefSignature for details.
	// Stack change:
	//   <0: Pops the arguments, including the instance.
	int InvokeMemberLL(String *name, ovlocals_t argCount, Value *value, Value *result, uint32_t refSignature);

	// Loads the member with the specified name from a value. Any kind of member
	// can be loaded. If the member is a property, the getter will be invoked
	// with zero arguments. If the member is a method, an aves.Method object will
	// be constructed.
	//
	// This method cannot be used to load a static member.
	//   instance:
	//     The instance to load a member from. This is assumed to point to the
	//     evaluation stack.
	//   member:
	//     The name of the member to load.
	//   result:
	//     A location that receives the member value.
	// Stack change:
	//   -1: Pops the instance.
	int LoadMemberLL(Value *instance, String *member, Value *result);

	// Stores a value in the member with the specified name. Only fields and
	// writable properties can be written to. If the member is a property, its
	// setter will be invoked with one argument (the value). The return value
	// of such a call is discarded.
	//
	// This method cannot be used to write to a static member.
	//   instance:
	//     The instance whose member to write to, immediately followed by the
	//     value to be stored. This is assumed to point to the evaluation stack.
	//   member:
	//     The name of the member to write to.
	// Stack change:
	//   -2: Pops the instance and the stored value.
	int StoreMemberLL(Value *instance, String *member);

	// Invokes the indexer getter of a value.
	//   argCount:
	//     The number of arguments to pass to the indexer, EXCLUDING the instance.
	//   args:
	//     The arguments to pass to the indexer, INCLUDING the instance. This is
	//     assumed to point to the evaluation stack. The instance precedes the
	//     "regular" arguments.
	//   result:
	//     A location that receives the return value of the indexer access.
	// Stack change:
	//   <0: Pops the arguments to the indexer, including the instance.
	int LoadIndexerLL(ovlocals_t argCount, Value *args, Value *result);

	// Invokes the indexer setter of a value.
	//   argCount:
	//     The number of arguments to pass to the indexer, EXCLUDING both the
	//     instance and the value to be stored.
	//   args:
	//     The arguments to pass to the indexer, INCLUDING the instance. This is
	//     assumed to point to the evaluation stack. The instance precedes the
	//     "regular" arguments, which are immediately followed by the value to be
	//     stored.
	// Stack change:
	//   <0: Pops the arguments to the indexer, including the instance and the
	//       stored value.
	int StoreIndexerLL(ovlocals_t argCount, Value *args);

	// Loads a reference to the specified field. The resulting reference is pushed
	// onto the evaluation stack.
	//
	// This method cannot be used to load a reference to a static field.
	//   inst:
	//     The instance whose field to get a reference to. This is not required
	//     to point to the evaluation stack.
	//   field:
	//     The instance field to get a reference to.
	// Stack change:
	//   0: Pops the instance, pushes the field reference.
	int LoadFieldRefLL(Value *inst, Field *field);

	// Loads a reference to the specified member. The member must be a field. The
	// resulting reference is pushed onto the evaluation stack.
	//
	// This method cannot be used to load a reference to a static member.
	//   inst:
	//     The instance whose member to get a reference to. This is not required
	//     to point to the evaluation stack.
	//   member:
	//     The name of the member to load. The member must be an instance field.
	//     (If you have a Field* already, use LoadFieldRefLL().)
	// Stack change:
	//   0: Pops the instance, pushes the member reference.
	int LoadMemberRefLL(Value *inst, String *member);

	// Invokes the specified operator with the specified arguments.
	//   args:
	//     The arguments to pass to the operator. The number of arguments this
	//     points to must correspond with the operator's arity. This is assumed
	//     to point to the evaluation stack. The operator's implementing method
	//     is always loaded from the first argument.
	//   op:
	//     The operator to invoke.
	//   arity:
	//     The number of arguments to pass to the operator. This must correspond
	//     to the operator's arity. Since all operators are unary or binary, this
	//     value can only be 1 or 2.
	//   result:
	//     A location that receives the return value of the call.
	// Stack change:
	//   -arity (-1 or -2): Pops the arguments to the operator method.
	int InvokeOperatorLL(Value *args, Operator op, ovlocals_t arity, Value *result);

	// Determines whether two values are equal according to the '==' operator
	// (Operator::EQ).
	//   args:
	//     The two values to compare. This parameter has the same characteristics
	//     as the 'args' parameter of InvokeOperatorLL().
	//   result:
	//     True if args[0] == args[1].
	// Stack change:
	//   -2: Pops the arguments.
	int EqualsLL(Value *args, bool &result);

	// Compares two values for ordering, using the '<=>' operator (Operator::CMP).
	//   args:
	//     The two values to compare. This parameter has the same characteristics
	//     as the 'args' parameter of InvokeOperatorLL().
	//   result:
	//     A location that receives the return value of the operator. This method
	//     additionally verifies that the result is of type Int.
	// Stack change:
	//   -2: Pops the arguments.
	int CompareLL(Value *args, Value *result);

	// Concatenates two values together into a string. Equivalent to Osprey's
	// '::' operator.
	//   args:
	//     The two values to be concatenated. This is assumed to point to the
	//     evaluation stack. Each of these values will be converted to a string,
	//     if they aren't strings already.
	//   result:
	//     A location that receives the concatenated string.
	// Stack change:
	//   -2: Pops the two values.
	int ConcatLL(Value *args, Value *result);

	// Specialised comparers! For speed.

	// Determines whether one value is less than another, according to the '<=>'
	// operator (Operator::CMP).
	//   args:
	//     The two values to compare. This parameter has the same characteristics
	//     as the 'args' parameter of InvokeOperatorLL().
	//   result:
	//     True if args[0] < args[1].
	// Stack change:
	//   -2: Pops the arguments.
	int CompareLessThanLL(Value *args, bool &result);

	// Determines whether one value is greater than another, according to the '<=>'
	// operator (Operator::CMP).
	//   args:
	//     The two values to compare. This parameter has the same characteristics
	//     as the 'args' parameter of InvokeOperatorLL().
	//   result:
	//     True if args[0] > args[1].
	// Stack change:
	//   -2: Pops the arguments.
	int CompareGreaterThanLL(Value *args, bool &result);

	// Determines whether one value is less than or equal to another, according to
	// the '<=>' operator (Operator::CMP).
	//   args:
	//     The two values to compare. This parameter has the same characteristics
	//     as the 'args' parameter of InvokeOperatorLL().
	//   result:
	//     True if args[0] <= args[1].
	// Stack change:
	//   -2: Pops the arguments.
	int CompareLessEqualsLL(Value *args, bool &result);

	// Determines whether one value is greater than or equal to another, according
	// to the '<=>' operator (Operator::CMP).
	//   args:
	//     The two values to compare. This parameter has the same characteristics
	//     as the 'args' parameter of InvokeOperatorLL().
	//   result:
	//     True if args[0] >= args[1].
	// Stack change:
	//   -2: Pops the arguments.
	int CompareGreaterEqualsLL(Value *args, bool &result);

	// Throws a TypeError with a message about an unimplemented operator.
	//   op:
	//     The operator that is missing.
	int ThrowMissingOperatorError(Operator op);

	// Initializes the specified method.
	//
	// See MethodInitializer for details on what method initialization is.
	//   method:
	//     The method to initialize.
	int InitializeMethod(MethodOverload *method);

	// Calls static constructors for the types recorded in a MethodBuilder.
	//
	// This method is called during method initialization, to ensure that static
	// fields referred to inside the initializing method have backing storage
	// when the method is executed. See MethodInitializer for details on what
	// method initialization is.
	int CallStaticConstructors(instr::MethodBuilder &builder);

	// When looking for a catch clause for a thrown error, we may encounter one
	// or more finally or fault clauses. A finally or fault clause can contain
	// any code it likes, including nested try blocks. This means an error may
	// be thrown and caught in a finally or fault clause. The currentError field
	// contains the most recently thrown error, but upon exiting the finally or
	// fault clause, we must make sure currentError contains the same value as
	// when the clause was entered, so that we can continue handling that error.
	//
	// The obvious solution is to simply store the currentError value in a local
	// variable inside the finally/fault handler, but then the GC cannot reach
	// it. The error would be obliterated if a GC cycle was triggered inside the
	// finally or fault clause. Until we have a GC that can scan the native call
	// stack, we have to save the error in a location the GC can examine, hence
	// Thread gets a pointer into the native call stack.
	//
	// This solution is somewhat convoluted.
	//
	// NOTE: Thread::errorStack points directly into the native call stack. You
	// MUST restore it when exiting the scope containing the ErrorStack value.
	struct ErrorStack
	{
		ErrorStack *prev;
		Value error;

		// Initializes a new ErrorStack value for the specified thread and pushes
		// the new entry onto the stack. THIS CONSTRUCTOR MUTATES THE THREAD.
		inline ErrorStack(Thread *const thread)
		{
			prev = thread->errorStack;
			error = thread->currentError;
			thread->errorStack = this;
		}
	};

	friend class GC;
	friend class VM;
	friend class Type;
	friend class MethodInitializer;
	friend class RootSetWalker;
};

} // namespace ovum
