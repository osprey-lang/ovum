#ifndef OVUM__THREAD_H
#define OVUM__THREAD_H

#include "ov_vm.h"

// Returns a successful status code. Semicolon intentionally missing.
#define RETURN_SUCCESS             return OVUM_SUCCESS

/*
The macros BEGIN_NATIVE_FUNCTION, END_NATIVE_FUNCTION, CHECKED and CHECKED_MEM
are intended to be used in the development of native modules. They are used as
follows:

    BEGIN_NATIVE_FUNCTION(function_name)
    {
      // Body of the function goes here

      // If an error status is returned by VM_Invoke, CHECKED() will make sure it
      // is immediately returned from the native function.
      // Please note: if you have resources to clean up, you should either start
      // relying on C++ destructors, or check the status codes manually.
      CHECKED(VM_Invoke(...));

      // CHECKED_MEM is used for things that return a null pointer on failure:
      CHECKED_MEM(*value = GC_ConstructString(...));
      // If the expression returns a null pointer, CHECKED_MEM will return from
      // the native function with the status code OVUM_ERROR_NO_MEMORY.
    }
    END_NATIVE_FUNCTION

BEGIN_NATIVE_FUNCTION does the same thing as NATIVE_FUNCTION, and adds an opening
brace for the method body, followed by declaring an int variable named 'status__'.
END_NATIVE_FUNCTION outputs a 'status__ = OVUM_SUCCESS;' statement, followed by a
label named 'retStatus__', attached to a statement that returns 'status__'.
The variable and the label are used by CHECKED and CHECKED_MEM: if an error status
is returned by the checked expression, 'status__' is assigned an error code and a
goto jump to 'retStatus__'.

To make things clearer, the example above expands to code equivalent to:

    NATIVE_FUNCTION(function_name)
    {
      int status__; // Note: uninitialized! Only used in case of an error.
      {
        if ((status__ = VM_Invoke(...)) != OVUM_SUCCESS)
        {
          goto retStatus__;
        }

        if (!(*value = GC_ConstructString(...)))
        {
          status__ = OVUM_ERROR_NO_MEMORY;
          goto retStatus__;
        }
        // ...
      }
      status__ = OVUM_SUCCESS;
      retStatus__: return status__;
    }

If a native function uses BEGIN_NATIVE_FUNCTION, it should always end with an
END_NATIVE_FUNCITON. A semicolon should not be placed after either macro.

The assignment to 'status__' in END_NATIVE_FUNCTION is to ensure the compiler does
not produce an 'unused variable' warning if the function has no CHECKED statements.

Note that the names 'status__' and 'retStatus__' are *guaranteed* to be used by
BEGIN_ and END_NATIVE_FUNCTION. Code that wishes to make use of the CHECKED macros
can therefore declare their own 'status__' and 'retStatus__'.

It might be worth pointing out that CHECKED anc CHECKED_MEM cannot be used in an
expression context. They must enclose an expression, however.
*/

#define BEGIN_NATIVE_FUNCTION(name) \
	NATIVE_FUNCTION(name) { \
		int status__;
#define END_NATIVE_FUNCTION \
		status__ = OVUM_SUCCESS; \
		retStatus__: return status__; \
	}
#define CHECKED(expr) do { if ((status__ = (expr)) != OVUM_SUCCESS) goto retStatus__; } while (0)
#define CHECKED_MEM(expr) do { if (!(expr)) { status__ = OVUM_ERROR_NO_MEMORY; goto retStatus__; } } while (0)

OVUM_API void VM_Push(ThreadHandle thread, Value *value);

OVUM_API void VM_PushNull(ThreadHandle thread);
OVUM_API void VM_PushBool(ThreadHandle thread, bool value);
OVUM_API void VM_PushInt(ThreadHandle thread, int64_t value);
OVUM_API void VM_PushUInt(ThreadHandle thread, uint64_t value);
OVUM_API void VM_PushReal(ThreadHandle thread, double value);
OVUM_API void VM_PushString(ThreadHandle thread, String *str);

// Pops a single value off the top of the evaluation stack.
OVUM_API Value VM_Pop(ThreadHandle thread);
OVUM_API void VM_PopN(ThreadHandle thread, ovlocals_t n);

OVUM_API void VM_Dup(ThreadHandle thread);

OVUM_API Value *VM_Local(ThreadHandle thread, ovlocals_t n);

// Invokes a value on the evaluation stack.
// If S[0] is the top value on the stack, then S[argCount] is the value that is invoked.
//   thread:
//     The thread on which to invoke the value.
//   argCount:
//     The number of arguments passed in the invocation, NOT including the instance.
//   result:
//     The return value of the invocation. If null, the return value is pushed onto the stack.
OVUM_API int VM_Invoke(ThreadHandle thread, ovlocals_t argCount, Value *result);

// Invokes a member of a value on the evaluation stack.
// If S[0] is the top value on the stack, then S[argCount] is the value whose member is invoked.
//   thread:
//     The thread on which to invoke the method.
//   name:
//     The name of the member to look up. This member MUST be an instance method.
//     If it is not, an error is thrown.
//   argCount:
//     The number of arguments to pass to the method, NOT including the instance.
//   result:
//     The return value of the invocation. If null, the return value is pushed onto the stack.
OVUM_API int VM_InvokeMember(ThreadHandle thread, String *name, ovlocals_t argCount, Value *result);

// Invokes a specific method with arguments from the evaluation stack.
//   thread:
//     The thread on which to invoke the method.
//   method:
//     The method to invoke. Must not be nullptr.
//   argCount:
//     The number of arguments to pass to the method, NOT including the instance.
//   result:
//     The return value of the invocation. If null, the return value is pushed onto the stack.
OVUM_API int VM_InvokeMethod(ThreadHandle thread, MethodHandle method, ovlocals_t argCount, Value *result);
// Invokes an operator on one or two values on the evaluation stack.
//   thread:
//     The thread on which to invoke the operator.
//   op:
//     The operator to invoke. This determines the number of values that are popped from the stack:
//     binary operators use two operands; unary operators use one.
//   result:
//     The return value of the invocation. If null, the return value is pushed onto the stack.
OVUM_API int VM_InvokeOperator(ThreadHandle thread, Operator op, Value *result);
// Determines whether the top two values on the evaluation stack equal each other,
// by invoking the == operator.
//   thread:
//     The thread on which to perform the operator invocation.
// Notes:
//   If S[0] is the top of the stack, this method returns S[1] == S[0]; that is, S[1] is
//     the value whose == operator is invoked.
//   If no type in the hierarchy all the way up to aves.Object overloads the == operator,
//     then a reference equality comparison is performed instead (see "IsSameReference").
OVUM_API int VM_Equals(ThreadHandle thread, bool *result);
// Performs an ordinal comparison on two values on the stack, by invoking the <=> operator.
//   thread:
//     The thread on which to perform the operator invocation.
// Notes:
//   If S[0] is the top of the stack, this method returns S[1] <=> S[0]. That is, the <=>
//     operator is invoked on the type of S[1].
//   This method /requires/ the <=> operator to return an instance of aves.Int. If it does not,
//     a TypeError is thrown.
OVUM_API int VM_Compare(ThreadHandle thread, int64_t *result);

// Loads a member from the top value on the stack. Note that the instance is always popped.
//   thread:
//     The thread on which to load the member. If the member is a property with a getter,
//     then this is the thread on which the getter is executed.
//   member:
//     The name of the member to load.
//   result:
//     The result of loading the member. If null, this value is pushed onto the stack.
OVUM_API int VM_LoadMember(ThreadHandle thread, String *member, Value *result);
// Stores the top of the stack into a member of the second stack value.
//   thread:
//     The thread on which to load the member. If the member is a property with a setter,
//     then this is the thread on which the setter is executed.
//   member:
//     The name of the member to store.
OVUM_API int VM_StoreMember(ThreadHandle thread, String *member);

// Loads an instance field from the top value on the stack. Note that the instance is always popped.
//   thread:
//     The thread on which to load the member. If the member is a property with a getter,
//     then this is the thread on which the getter is executed.
//   field:
//     The instance field to load.
//   result:
//     Receives the value of the field. If null, this value is pushed onto the stack.
OVUM_API int VM_LoadField(ThreadHandle thread, FieldHandle field, Value *result);
// Stores the top of the stack into a field of the second stack value.
//   thread:
//     The thread on which to load the member. If the member is a property with a setter,
//     then this is the thread on which the setter is executed.
//   field:
//     The instance field to store.
OVUM_API int VM_StoreField(ThreadHandle thread, FieldHandle field);

// Loads the indexer from the top value on the stack.
//   thread:
//     The thread on which to execute the indexer getter.
//   argCount:
//     The number of arguments to invoke the indexer with, not including the instance.
//   result:
//     The result of loading the member. If null, this value is pushed onto the stack.
OVUM_API int VM_LoadIndexer(ThreadHandle thread, ovlocals_t argCount, Value *result);
// Stores the top value on the stack into the indexer of the second stack value.
//   thread:
//     The thread on which to execute the indexer setter.
//   argCount:
//     The number of arguments to invoke the indexer with, not including the instance or value.
OVUM_API int VM_StoreIndexer(ThreadHandle thread, ovlocals_t argCount);

// Loads the value of the specified static field.
//   thread:
//     The thread on which to perform the load.
//   field:
//     A handle to the field to load.
//   result:
//     The value in the field. If null, this value is pushed onto the stack.
// Note: If the type that the field belongs to has not had its static constructor run,
// this method will cause the static constructor to be run, except when called from
// within the static constructor.
OVUM_API int VM_LoadStaticField(ThreadHandle thread, FieldHandle field, Value *result);
// Stores the top value on the stack into the specified static field.
//   thread:
//     The thread on which to perform the store.
//   field:
//     A handle to the field to write into.
// Note: If the type that the field belongs to has not had its static constructor run,
// this method will cause the static constructor to be run, except when called from
// within the static constructor.
OVUM_API int VM_StoreStaticField(ThreadHandle thread, FieldHandle field);

// Stringifies the top value on the stack, by calling .toString on it. Additionally,
// this method makes sure that the return value is a indeed string, and throws a
// TypeConversionError if it is not.
//   thread:
//     The thread on which to call toString.
//   result:
//     The resulting string value. If null, the value ends up on the stack (as a Value).
OVUM_API int VM_ToString(ThreadHandle thread, String **result = nullptr);

OVUM_API int VM_Throw(ThreadHandle thread);
OVUM_API int VM_ThrowError(ThreadHandle thread, String *message = nullptr);
OVUM_API int VM_ThrowTypeError(ThreadHandle thread, String *message = nullptr);
OVUM_API int VM_ThrowMemoryError(ThreadHandle thread, String *message = nullptr);
OVUM_API int VM_ThrowOverflowError(ThreadHandle thread, String *message = nullptr);
OVUM_API int VM_ThrowDivideByZeroError(ThreadHandle thread, String *message = nullptr);
OVUM_API int VM_ThrowNullReferenceError(ThreadHandle thread, String *message = nullptr);
OVUM_API int VM_ThrowTypeConversionError(ThreadHandle thread, String *message = nullptr);
// Constructs and throws an error of the specified type. The caller
// pushes the constructor arguments onto the stack before calling
// this function.
//   thread:
//     The thread on which to throw the error.
//   type:
//     The error type. An instance of this type will be constructed
//     using arguments on the evaluation stack.
//   argc:
//     The total number of arguments that are to be passed to the
//     error type's constructor.
// Note that this function may not always return OVUM_ERROR_THROWN.
// If the VM runs out of memory while constructing the error object,
// OVUM_ERROR_NO_MEMORY is returned instead. Other error codes may
// also be returned.
// This function should be used by native code that need to throw
// an error of a type not covered by the helper functions above.
OVUM_API int VM_ThrowErrorOfType(ThreadHandle thread, TypeHandle type, ovlocals_t argc);

// Informs the thread that it is entering a section of native code
// which will not interact with the managed runtime in any way.
// If the GC is triggered (by another thread) when this thread is in
// the unmanaged region, garbage collection proceeds without suspending
// this thread.
// This should only be called by native code before performing a
// potentially lengthy operation that does not interact with the
// managed runtime, such as I/O or waiting for a synchronization
// object to be released. This will permit the thread to continue
// working in the background while the GC runs; otherwise, the GC
// would have to wait for the lengthy operation to finish.
OVUM_API void VM_EnterUnmanagedRegion(ThreadHandle thread);
// Informs the thread that it has left the unmanaged region.
// If the GC is running, the thread will now suspend itself; otherwise,
// the method returns immediately and the thread continues execution.
// See the documentation of VM_EnterUnmanagedRegion for details.
OVUM_API void VM_LeaveUnmanagedRegion(ThreadHandle thread);
// Determines whether the thread is in an unmanaged region.
// See the documentation of VM_EnterUnmanagedRegion for details.
OVUM_API bool VM_IsInUnmanagedRegion(ThreadHandle thread);

// Suspends the thread for the specified number of milliseconds.
// If milliseconds is 0, the thread gives up the remainder of its time
// slice to other threads that may be waiting for execution; if there
// are no such threads, this function returns immediately.
// The actual time spent sleeping may be more or less than the requested
// time depending on the system's time resolution.
OVUM_API void VM_Sleep(ThreadHandle thread, unsigned int milliseconds);

// Generates a stack trace for all the managed calls on the specified thread.
// This stack trace excludes the call to VM_GetStackTrace, as well as any invocations
// of natively functions called directly by other native functions. The only native
// functions that are included are those invoked by the VM.
OVUM_API String *VM_GetStackTrace(ThreadHandle thread);

// Gets the current depth of the call stack; that is, the number
// of stack frames currently on the call stack.
//
// This function runs in O(n) time, where n is the stack depth.
OVUM_API int VM_GetStackDepth(ThreadHandle thread);

// Gets a handle to the currently executing method overload.
OVUM_API OverloadHandle VM_GetCurrentOverload(ThreadHandle thread);

// In the functions below, stack frames are numbered such that the frame on top
// of the call stack is number 0, the previous frame is 1, and so on.
//
// These functions are provided for debugger support, and should be used cautiously.

// Gets the height of the evaluation stack of the specified stack frame, and
// optionally retrieves a pointer to the first value on the stack.
//
// If 'slots' is not null, it receives a pointer to the bottom of the evaluation
// stack in the specified stack frame. Do not attempt to modify the evaluation
// stack contents, as doing so will almost certainly corrupt the VM state. Also,
// do not read beyond the beginning or end of the evaluation stack; you may hit
// garbage, and may corrupt the VM.
//
// If stackFrame refers to an invalid stack frame, returns -1.
OVUM_API int VM_GetEvalStackHeight(ThreadHandle thread, int stackFrame, const Value **slots);

// Gets the number of locals in the specified stack frame, and optionally retrieves
// a pointer to the first local variable.
//
// If 'slots' is not null, it receives a pointer to the first local variable in
// the specified stack frame. Do not attempt to modify the local variable contents,
// as doing so will almost certainly corrupt the VM state. Also, do not read beyond
// the beginning or end of the local variable list; you may hit garbage, and may
// corrupt the VM.
//
// If stackFrame refers to an invalid stack frame, returns -1.
OVUM_API int VM_GetLocalCount(ThreadHandle thread, int stackFrame, const Value **slots);

// Gets the number of method arguments in the specified stack frame, and optionally
// retrieves a pointer to the first argument.
//
// If 'slots' is not null, it receives a pointer to the first method argument in
// the specified stack frame. Do not attempt to modify the method argument contents,
// as doing so will almost certainly corrupt the VM state. Also, do not read beyond
// the beginning or end of the method argument list; you may hit garbage, and may
// corrupt the VM.
//
// If stackFrame refers to an invalid stack frame, returns -1.
OVUM_API int VM_GetMethodArgCount(ThreadHandle thread, int stackFrame, const Value **slots);

// Gets a handle to the method overload executing in the specified stack frame.
//
// If stackFrame refers to an invalid stack frame, returns null.
OVUM_API OverloadHandle VM_GetExecutingOverload(ThreadHandle thread, int stackFrame);

// Gets the instruction pointer of the specified stack frame.
// Warning: for native methods, the instruction pointer is not useful.
// It may or may not point at the entry point of the method; this is not
// a guarantee.
//
// If stackFrame refers to an invalid stack frame, returns null.
OVUM_API const void *VM_GetInstructionPointer(ThreadHandle thread, int stackFrame);

typedef struct StackFrameInfo_S
{
	int stackHeight;
	const Value *stackPointer;

	int localCount;
	const Value *localPointer;

	int argumentCount;
	const Value *argumentPointer;

	OverloadHandle overload;
	const void *ip;
} StackFrameInfo;

// Gets information about the specified stack frame. Please read the documentation on
// VM_GetEvalStackHeight, VM_GetLocalCount, VM_GetMethodArgCount and VM_GetInstructionPointer
// to understand the various caveats of accessing this information.
//
// If stackFrame refers to an invalid stack frame, returns false.
OVUM_API bool VM_GetStackFrameInfo(ThreadHandle thread, int stackFrame, StackFrameInfo *dest);

#endif // OVUM__THREAD_H
