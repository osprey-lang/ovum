#pragma once

#ifndef VM__THREAD_H
#define VM__THREAD_H

#include "ov_vm.h"

OVUM_API void VM_Push(ThreadHandle thread, Value value);

OVUM_API void VM_PushNull(ThreadHandle thread);
OVUM_API void VM_PushBool(ThreadHandle thread, const bool value);
OVUM_API void VM_PushInt(ThreadHandle thread, const int64_t value);
OVUM_API void VM_PushUInt(ThreadHandle thread, const uint64_t value);
OVUM_API void VM_PushReal(ThreadHandle thread, const double value);
OVUM_API void VM_PushString(ThreadHandle thread, String *str);

// Pops a single value off the top of the evaluation stack.
OVUM_API Value VM_Pop(ThreadHandle thread);
OVUM_API void VM_PopN(ThreadHandle thread, const uint32_t n);

OVUM_API void VM_Dup(ThreadHandle thread);

OVUM_API Value *VM_Local(ThreadHandle thread, const uint32_t n);

// Invokes a value on the evaluation stack.
// If S[0] is the top value on the stack, then S[argCount] is the value that is invoked.
//   thread:
//     The thread on which to invoke the value.
//   argCount:
//     The number of arguments passed in the invocation, NOT including the instance.
//   result:
//     The return value of the invocation. If null, the return value is pushed onto the stack.
OVUM_API void VM_Invoke(ThreadHandle thread, const uint32_t argCount, Value *result);

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
OVUM_API void VM_InvokeMember(ThreadHandle thread, String *name, const uint32_t argCount, Value *result);

// Invokes a specific method with arguments from the evaluation stack.
//   thread:
//     The thread on which to invoke the method.
//   method:
//     The method to invoke. Must not be nullptr.
//   argCount:
//     The number of arguments to pass to the method, NOT including the instance.
//   result:
//     The return value of the invocation. If null, the return value is pushed onto the stack.
OVUM_API void VM_InvokeMethod(ThreadHandle thread, MethodHandle method, const uint32_t argCount, Value *result);
// Invokes an operator on one or two values on the evaluation stack.
//   thread:
//     The thread on which to invoke the operator.
//   op:
//     The operator to invoke. This determines the number of values that are popped from the stack:
//     binary operators use two operands; unary operators use one.
//   result:
//     The return value of the invocation. If null, the return value is pushed onto the stack.
OVUM_API void VM_InvokeOperator(ThreadHandle thread, Operator op, Value *result);
// Determines whether the top two values on the evaluation stack equal each other,
// by invoking the == operator.
//   thread:
//     The thread on which to perform the operator invocation.
// Notes:
//   If S[0] is the top of the stack, this method returns S[1] == S[0]; that is, S[1] is
//     the value whose == operator is invoked.
//   If no type in the hierarchy all the way up to aves.Object overloads the == operator,
//     then a reference equality comparison is performed instead (see "IsSameReference").
OVUM_API bool VM_Equals(ThreadHandle thread);
// Performs an ordinal comparison on two values on the stack, by invoking the <=> operator.
//   thread:
//     The thread on which to perform the operator invocation.
// Notes:
//   If S[0] is the top of the stack, this method returns S[1] <=> S[0]. That is, the <=>
//     operator is invoked on the type of S[1].
//   This method /requires/ the <=> operator to return an instance of aves.Int. If it does not,
//     a TypeError is thrown.
OVUM_API int VM_Compare(ThreadHandle thread);

// Loads a member from the top value on the stack. Note that the instance is always popped.
//   thread:
//     The thread on which to load the member. If the member is a property with a getter,
//     then this is the thread on which the getter is executed.
//   member:
//     The name of the member to load.
//   result:
//     The result of loading the member. If null, this value is pushed onto the stack.
OVUM_API void VM_LoadMember(ThreadHandle thread, String *member, Value *result);
// Stores a member from the top of the stack to the second stack value.
//   thread:
//     The thread on which to load the member. If the member is a property with a setter,
//     then this is the thread on which the setter is executed.
//   member:
//     The name of the member to store.
OVUM_API void VM_StoreMember(ThreadHandle thread, String *member);

// Loads the indexer from the top value on the stack.
//   thread:
//     The thread on which to execute the indexer getter.
//   argCount:
//     The number of arguments to invoke the indexer with, not including the instance.
//   result:
//     The result of loading the member. If null, this value is pushed onto the stack.
OVUM_API void VM_LoadIndexer(ThreadHandle thread, const uint32_t argCount, Value *result);
// Stores the top value on the stack into the indexer of the second stack value.
//   thread:
//     The thread on which to execute the indexer setter.
//   argCount:
//     The number of arguments to invoke the indexer with, not including the instance or value.
OVUM_API void VM_StoreIndexer(ThreadHandle thread, const uint32_t argCount);

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
OVUM_API void VM_LoadStaticField(ThreadHandle thread, FieldHandle field, Value *result);
// Stores the top value on the stack into the specified static field.
//   thread:
//     The thread on which to perform the store.
//   field:
//     A handle to the field to write into.
// Note: If the type that the field belongs to has not had its static constructor run,
// this method will cause the static constructor to be run, except when called from
// within the static constructor.
OVUM_API void VM_StoreStaticField(ThreadHandle thread, FieldHandle field);

// Stringifies the top value on the stack, by calling .toString on it. Additionally,
// this method makes sure that the return value is indeed a string, and throws a TypeError
// if it is not.
//   thread:
//     The thread on which to call toString.
//   result:
//     The resulting string value. If null, the value ends up on the stack (as a Value).
OVUM_API void VM_ToString(ThreadHandle thread, String **result = nullptr);

OVUM_API void VM_Throw(ThreadHandle thread);
OVUM_API void VM_ThrowError(ThreadHandle thread, String *message = nullptr);
OVUM_API void VM_ThrowTypeError(ThreadHandle thread, String *message = nullptr);
OVUM_API void VM_ThrowMemoryError(ThreadHandle thread, String *message = nullptr);
OVUM_API void VM_ThrowOverflowError(ThreadHandle thread, String *message = nullptr);
OVUM_API void VM_ThrowDivideByZeroError(ThreadHandle thread, String *message = nullptr);
OVUM_API void VM_ThrowNullReferenceError(ThreadHandle thread, String *message = nullptr);

// Generates a stack trace for all the managed calls on the specified thread.
// This stack trace excludes the call to VM_GetStackTrace, as well as any invocations
// of natively functions called directly by other native functions. The only native
// functions that are included are those invoked by the VM.
OVUM_API String *VM_GetStackTrace(ThreadHandle thread);

#endif // VM__THREAD_H