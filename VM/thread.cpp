#include "ov_vm.internal.h"
#include <comdef.h>

namespace thread_errors
{
	namespace
	{
		LitString<92> _ConcatTypes                 = LitString<92>::FromCString("The concatenation operator requires two Lists, two Hashes, or two values of any other types.");
		LitString<43> _CompareType                 = LitString<43>::FromCString("The comparison operator must return an Int.");
		LitString<27> _NotInvokable                = LitString<27>::FromCString("The value is not invokable.");
		LitString<30> _MemberNotFound              = LitString<30>::FromCString("The member could not be found.");
		LitString<28> _MemberNotInvokable          = LitString<28>::FromCString("The member is not invokable.");
		LitString<26> _AssigningToMethod           = LitString<26>::FromCString("Cannot assign to a method.");
		LitString<50> _StaticMemberThroughInstance = LitString<50>::FromCString("Cannot access a static member through an instance.");
		LitString<31> _GettingWriteonlyProperty    = LitString<31>::FromCString("Cannot get write-only property.");
		LitString<38> _SettingReadonlyProperty     = LitString<38>::FromCString("Cannot assign to a read-only property.");
	}

	String *ConcatTypes                 = _S(_ConcatTypes);
	String *CompareType                 = _S(_CompareType);
	String *NotInvokable                = _S(_NotInvokable);
	String *MemberNotFound              = _S(_MemberNotFound);
	String *MemberNotInvokable          = _S(_MemberNotInvokable);
	String *AssigningToMethod           = _S(_AssigningToMethod);
	String *StaticMemberThroughInstance = _S(_StaticMemberThroughInstance);
	String *GettingWriteonlyProperty    = _S(_GettingWriteonlyProperty);
	String *SettingReadonlyProperty     = _S(_SettingReadonlyProperty);
}


Thread::Thread() :
	currentFrame(nullptr), state(THREAD_CREATED),
	currentError(NULL_VALUE), ip(nullptr)
{
	InitCallStack();
}

Thread::~Thread()
{
	// You really should not dispose of a thread that is still running, so:
	assert(state == THREAD_CREATED || state == THREAD_STOPPED);

	DisposeCallStack();
}

void Thread::Start(Method *method, Value &result)
{
	;
}


void Thread::Invoke(unsigned int argCount, Value *result)
{
	Value *value = currentFrame->evalStack + currentFrame->stackCount - argCount - 1;
	if (result)
		InvokeLL(argCount, value + 1, result);
	else
	{
		Value output;
		InvokeLL(argCount, value, &output);
		currentFrame->Push(output);
	}
}

#pragma warning(push)
#pragma warning(disable: 4703) // Potentially uninitialized local variable 'method'
                               // The compiler can't figure out that ThrowTypeError always throws.
// Note: argCount does NOT include the instance
void Thread::InvokeLL(unsigned int argCount, Value *value, Value *result)
{
	Method::Overload *mo;

	// If the value is a Method instance, we use that instance's details.
	// Otherwise, we load the default invocator from the value.

	const Type *type = value->type;

	if (type == VM::vm->types.Method)
	{
		MethodInst *methodInst = value->common.method;
		mo = ResolveOverload(methodInst->method, argCount);

		if ((mo->flags & MethodFlags::INSTANCE) != MethodFlags::NONE)
			// Overwrite the Method with the instance
			*value = methodInst->instance;
		else
			// Shift the Method off the stack
			currentFrame->Shift(argCount);
	}
	else
	{
		Member *member;
		if ((member = type->FindMember(static_strings::_call, currentFrame->method->declType)) &&
			(member->flags & MemberFlags::METHOD) != MemberFlags::NONE)
			mo = ResolveOverload((Method*)member, argCount);
		else
			ThrowTypeError(thread_errors::NotInvokable);
	}

	// We've now found a method overload to invoke, omg!
	// So let's just pass it into InvokeMethodOverload.
	InvokeMethodOverload(mo, argCount, value + 1, result);
}
#pragma warning(pop)

void Thread::InvokeMember(String *name, unsigned int argCount, Value *result)
{
	Value *value = currentFrame->evalStack + currentFrame->stackCount - argCount - 1;

	Member *member;
	if (member = value->type->FindMember(name, currentFrame->method->declType))
	{
		if ((member->flags & MemberFlags::METHOD) == MemberFlags::NONE)
			ThrowTypeError(thread_errors::MemberNotInvokable);

		InvokeMethod((Method*)member, argCount, value + 1, result);
	}
	else
		ThrowTypeError(thread_errors::MemberNotFound);
}

void Thread::InvokeMethod(Method *method, unsigned int argCount, Value *args, Value *result)
{
	Method::Overload *mo = ResolveOverload(method, argCount);
	InvokeMethodOverload(mo, argCount, args, result);
}

void Thread::InvokeMethodOverload(Method::Overload *mo, unsigned int argCount, Value *args, Value *result)
{
	if (!mo->IsInitialized())
		InitializeMethod(mo);

	MethodFlags flags = mo->flags; // used several times below!
	uint16_t finalArgCount = argCount;

	if ((flags & MethodFlags::VARIADIC) != MethodFlags::NONE)
	{
		PrepareArgs(flags, argCount, mo->paramCount, currentFrame);
		finalArgCount = mo->paramCount;
	}

	finalArgCount += (int)(flags & MethodFlags::INSTANCE) >> 3;

	// And now we can push the new stack frame!
	// Note: this updates currentFrame and maybe the ip
	StackFrame *frame = PushStackFrame(finalArgCount, args, mo);

	if ((flags & MethodFlags::NATIVE) != MethodFlags::NONE)
	{
		mo->nativeEntry(this, finalArgCount, frame->arguments);
		// Native methods are not required to return with one value on the stack, but if
		// they have more than one, only the lowest one is used.
		if (frame->stackCount)
			*result = frame->evalStack[0];
		else
			*result = NULL_VALUE;
	}
	else
	{
		Evaluate(frame, mo->entry);
		// It should not be possible to return from a method with
		// anything other than exactly one value on the stack!
		assert(frame->stackCount == 1);
		*result = frame->evalStack[0];
	}

	// restore previous stack frame
	currentFrame = frame->prevFrame;
	ip = frame->prevInstr;

	// Done!
}

void Thread::InvokeOperator(Operator op, Value *result)
{
	throw L"Not implemented";
}

void Thread::InvokeOperatorLL(Value *args, Operator op, Value *result)
{
	throw L"Not implemented";
}

void Thread::InvokeApply(Value *result)
{
	throw L"Not implemented";
}
void Thread::InvokeApplyMethod(Method *method, Value *result)
{
	throw L"Not implemented";
}


bool Thread::Equals()
{
	Value *args = currentFrame->evalStack + currentFrame->stackCount - 2;
	return EqualsLL(args);
}

bool Thread::EqualsLL(Value *args)
{
	Value result;
	InvokeOperatorLL(args, Operator::EQ, &result);
	return IsTrue_(result);
}

int Thread::Compare()
{
	Value *args = currentFrame->evalStack + currentFrame->stackCount - 2;
	return CompareLL(args);
}

int Thread::CompareLL(Value *args)
{
	Value result;
	InvokeOperatorLL(args, Operator::CMP, &result);

	if (result.type != VM::vm->types.Int)
		ThrowTypeError(thread_errors::CompareType);

	int64_t cmpValue = result.integer;
	return cmpValue < 0 ? -1 :
		cmpValue > 0 ? 1 :
		0;
}

void Thread::Concat(Value *result)
{
	ConcatLL(currentFrame->evalStack + currentFrame->stackCount - 2, result);
}

void Thread::ConcatLL(Value *args, Value *result)
{
	Value *a = args;
	Value *b = args + 1;
	if (a->type == VM::vm->types.List || b->type == VM::vm->types.List)
	{
		// list concatenation
		if (a->type != b->type)
			ThrowTypeError(thread_errors::ConcatTypes);

		ListInst *aList = a->common.list;
		ListInst *bList = b->common.list;
		Value output;
		GC::gc->Alloc(this, VM::vm->types.List, sizeof(ListInst), &output);

		int32_t length = aList->length + bList->length;
		VM::vm->functions.initListInstance(this, output.common.list, length);
		if (length > 0)
		{
			CopyMemoryT(output.common.list->values, aList->values, aList->length);
			CopyMemoryT(output.common.list->values + aList->length, bList->values, bList->length);
		}

		*result = output;
	}
	else if (a->type == VM::vm->types.Hash || b->type == VM::vm->types.Hash)
	{
		// hash concatenation
		if (a->type != b->type)
			ThrowTypeError(thread_errors::ConcatTypes);

		// TODO
	}
	else
	{
		// string concatenation
		*a = StringFromValue(this, *a);
		*b = StringFromValue(this, *b);

		Value output;
		SetString_(output, String_Concat(this, a->common.string, b->common.string));
		*result = output;
	}
}


void Thread::LoadMember(String *member, Value *result)
{
	Value *inst = currentFrame->evalStack + currentFrame->stackCount - 1;
	if (result)
		Thread::LoadMemberLL(inst, member, result);
	else
	{
		Value output;
		LoadMemberLL(inst, member, &output);
		currentFrame->Push(output);
	}
}

void Thread::LoadMemberLL(Value *instance, String *member, Value *result)
{
	if (instance->type == nullptr)
		ThrowNullReferenceError();

	const Member *m = instance->type->FindMember(member, currentFrame->method->declType);
	if ((m->flags & MemberFlags::INSTANCE) == MemberFlags::NONE)
		ThrowTypeError(thread_errors::StaticMemberThroughInstance);

	if ((m->flags & MemberFlags::FIELD) != MemberFlags::NONE)
	{
		*result = *reinterpret_cast<const Field*>(m)->GetFieldUnchecked(instance);
		currentFrame->Pop(1); // Done with the instance!
	}
	else if ((m->flags & MemberFlags::METHOD) != MemberFlags::NONE)
	{
		GC::gc->Alloc(this, VM::vm->types.Method, sizeof(MethodInst), result);
		result->common.method->instance = *instance;
		result->common.method->method = (Method*)m;
		currentFrame->Pop(1); // Done with the instance!
	}
	else // MemberFlags::PROPERTY
	{
		const Property *p = (Property*)m;
		if (!p->getter)
			ThrowTypeError(thread_errors::GettingWriteonlyProperty);

		// Remember: the instance is already on the stack!
		InvokeMethod(p->getter, 0, instance + 1, result);
	}
}

void Thread::StoreMember(String *member)
{
	Value *args = currentFrame->evalStack + currentFrame->stackCount - 2;
	StoreMemberLL(args, args + 1, member);
}

void Thread::StoreMemberLL(Value *instance, Value *value, String *member)
{
	if (instance->type == nullptr)
		ThrowNullReferenceError();

	Member *m = instance->type->FindMember(member, currentFrame->method->declType);
	if ((m->flags & MemberFlags::INSTANCE) == MemberFlags::NONE)
		ThrowTypeError(thread_errors::StaticMemberThroughInstance);
	if ((m->flags & MemberFlags::METHOD) != MemberFlags::NONE)
		ThrowTypeError(thread_errors::AssigningToMethod);

	if ((m->flags & MemberFlags::FIELD) != MemberFlags::NONE)
		*reinterpret_cast<Field*>(m)->GetFieldUnchecked(instance) = *value;
	else // MemberFlags::PROPERTY
	{
		Property *p = (Property*)m;
		if (!p->setter)
			ThrowTypeError(thread_errors::SettingReadonlyProperty);

		// Remember: the instance and value are already on the stack!
		InvokeMethod(p->setter, 1, value, nullptr);
	}

	currentFrame->Pop(2); // Done with the instance and the value!
}

// Note: argCount does NOT include the instance.
void Thread::LoadIndexer(uint16_t argCount, Value *result)
{
	throw L"Not implemented";
}
// Note: argc does NOT include the instance.
void Thread::LoadIndexerLL(uint16_t argc, Value *args, Value *dest)
{
	throw L"Not implemented";
}

// Note: argCount does NOT include the instance or the value that's being stored.
void Thread::StoreIndexer(uint16_t argCount)
{
	throw L"Not implemented";
}

void Thread::LoadIteratorLL(Value *inst, Value *dest)
{
	throw L"Not implemented";
}

void Thread::LoadStaticField(Field *field, Value *result)
{
	throw L"Not implemented";
}
void Thread::StoreStaticField(Field *field)
{
	throw L"Not implemented";
}

void Thread::ToString(String **result)
{
	LoadMember(static_strings::toString, nullptr);
	Invoke(0, nullptr);

	if (currentFrame->Peek(0).type != VM::vm->types.String)
		ThrowTypeError(static_strings::errors::ToStringWrongType);

	if (result != nullptr)
	{
		*result = currentFrame->Peek(0).common.string;
		currentFrame->stackCount--;
	}
	// else, leave it on the stack!
}


void Thread::Throw(bool rethrow)
{
	if (!rethrow)
	{
		currentError = currentFrame->Peek(0);
		currentError.common.error->stackTrace = GetStackTrace();
	}
	assert(!IS_NULL(currentError));

	OvumException ex(currentError);
	throw ex;
}

void Thread::ThrowError(String *message)
{
	if (message == nullptr)
		currentFrame->Push(NULL_VALUE);
	else
		currentFrame->PushString(message);
	GC::gc->Construct(this, VM::vm->types.Error, 1, nullptr);
	Throw();
}

void Thread::ThrowTypeError(String *message)
{
	if (message == nullptr)
		currentFrame->Push(NULL_VALUE);
	else
		currentFrame->PushString(message);
	GC::gc->Construct(this, VM::vm->types.TypeError, 1, nullptr);
	Throw();
}

void Thread::ThrowMemoryError(String *message)
{
	if (message == nullptr)
		currentFrame->Push(NULL_VALUE);
	else
		currentFrame->PushString(message);
	GC::gc->Construct(this, VM::vm->types.MemoryError, 1, nullptr);
	Throw();
}

void Thread::ThrowOverflowError(String *message)
{
	if (message == nullptr)
		currentFrame->Push(NULL_VALUE);
	else
		currentFrame->PushString(message);
	GC::gc->Construct(this, VM::vm->types.OverflowError, 1, nullptr);
	Throw();
}

void Thread::ThrowDivideByZeroError(String *message)
{
	if (message == nullptr)
		currentFrame->Push(NULL_VALUE);
	else
		currentFrame->PushString(message);
	GC::gc->Construct(this, VM::vm->types.DivideByZeroError, 1, nullptr);
	Throw();
}

void Thread::ThrowNullReferenceError(String *message)
{
	if (message == nullptr)
		currentFrame->Push(NULL_VALUE);
	else
		currentFrame->PushString(message);
	GC::gc->Construct(this, VM::vm->types.NullReferenceError, 1, nullptr);
	Throw();
}

void Thread::InitCallStack()
{
	callStack = (unsigned char*)VirtualAlloc(nullptr,
		CALL_STACK_SIZE + 256,
		MEM_RESERVE | MEM_COMMIT,
		PAGE_READWRITE);

	// Make sure the page following the call stack will cause an instant segfault,
	// as a very dirty way of signalling a stack overflow.
	DWORD ignore;
	VirtualProtect(callStack + CALL_STACK_SIZE, 256, PAGE_NOACCESS, &ignore);

	// The call stack should never be swapped out.
	VirtualLock(callStack, CALL_STACK_SIZE);
}

void Thread::DisposeCallStack()
{
	VirtualFree(callStack, 0, MEM_RELEASE);
}


// Note: argCount and args DO include the instance here!
StackFrame *Thread::PushStackFrame(const uint16_t argCount, Value *args, Method::Overload *method)
{
	StackFrame *current = currentFrame;

	assert(current->stackCount >= argCount);

	uint16_t paramCount = method->paramCount;
	uint16_t localCount = method->locals;
	StackFrame *newFrame = reinterpret_cast<StackFrame*>(args + paramCount);

	newFrame->Init(
		0,                                   // stackCount
		argCount,                            // argCount
		(Value*)newFrame - paramCount,       // arguments pointer
		(Value*)(newFrame + 1) + localCount, // evalStack pointer
		ip,                                  // prevInstr
		current,                             // prevFrame
		method                               // method
	);

	// initialize missing arguments to zeroes
	if (argCount < paramCount)
		memset(newFrame->arguments + argCount, 0, (paramCount - argCount) * sizeof(Value));

	// Also initialize all locals to 0.
	if (localCount)
		memset(LOCALS_OFFSET(newFrame), 0, localCount * sizeof(Value));

	currentFrame = newFrame;
	current->stackCount -= argCount; // pop the arguments (including the instance) off the current frame
	return newFrame;
}

StackFrame *Thread::PushFirstStackFrame(const uint16_t argCount, Value args[], Method::Overload *method)
{
	assert(currentFrame == nullptr); // <-- !

	// Copy the arguments onto the call stack
	if (argCount)
		CopyMemoryT(reinterpret_cast<Value*>(callStack), args, argCount);

	uint16_t paramCount = method->paramCount;
	uint16_t localCount = method->locals;
	StackFrame *newFrame = reinterpret_cast<StackFrame*>(callStack + paramCount * sizeof(Value));

	StackFrame newFrameValues = {
		0,                                   // stackCount
		argCount,                            // argCount
		(Value*)newFrame - paramCount,       // arguments pointer
		(Value*)(newFrame + 1) + localCount, // evalStack pointer
		nullptr,                                // prevInstr
		nullptr,                                // prevFrame
		method,                              // method
	};
	*newFrame = newFrameValues;

	// initialize missing arguments to zeroes
	if (argCount < paramCount)
		memset(newFrame->arguments + argCount, 0, (paramCount - argCount) * sizeof(Value));

	// Also initialize all locals to 0.
	if (localCount)
		memset(LOCALS_OFFSET(newFrame), 0, localCount * sizeof(Value));

	return currentFrame = newFrame;
}

void Thread::PrepareArgs(const MethodFlags flags, const uint16_t argCount, const uint16_t paramCount, StackFrame *frame)
{
	int32_t count = argCount >= paramCount ? argCount - paramCount : 0;

	Value listValue;
	// Construct the list!
	// We cannot really make any assumptions about the List constructor,
	// so we can't call it here. Instead, we "manually" allocate a ListInst,
	// set its type to List, and initialize its fields.
	GC::gc->Alloc(this, VM::vm->types.List, sizeof(ListInst), &listValue);
	ListInst *list = listValue.common.list;
	VM::vm->functions.initListInstance(this, list, count);

	if (count) // There are items to pack into a list
	{
		Value *valueBase;
		if ((flags & MethodFlags::VAR_END) != MethodFlags::NONE) // Copy from end
		{
			valueBase = frame->evalStack + frame->stackCount - count; // the first list item
			CopyMemoryT(list->values, valueBase, count); // copy values to list
			count--; // we want to remove all but the last item later
		}
		else // Copy from beginning!
		{
			Value *firstArg = frame->evalStack + frame->stackCount - argCount;
			CopyMemoryT(list->values, firstArg, count); // copy values to list
			valueBase = firstArg + 1; // the second argument

			// Shift all the other arguments down by count - 1.
			//   a, b, c, d, e, f	arguments
			//  [a, b, c] = L		pack into list
			//   L, d, e, f			result
			//   0  1  2  3			argument index
			count--; // same here!
			for (int i = 0; i < argCount; i++)
			{
				*valueBase = *(valueBase + count);
				valueBase++;
			}
			valueBase = firstArg; // the first argument receives the list
		}
		*valueBase = listValue;
		frame->stackCount -= count;
	}
	else // Let's push an empty list!
	{
		// Note: if argCount == 0, then push is equivalent to unshift.
		if ((flags & MethodFlags::VAR_END) != MethodFlags::NONE || argCount == 0)
			// Push list value onto the end
			frame->evalStack[frame->stackCount] = listValue;
		else
		{
			// Unshift list value onto the beginning!
			Value *valueBase = frame->evalStack + frame->stackCount;
			for (int i = 0; i < argCount; i++)
			{
				*valueBase = *(valueBase - 1);
				valueBase--;
			}
			*valueBase = listValue;
		}
		frame->stackCount++;
	}
}


String *Thread::GetStackTrace()
{
	StringBuffer buf(this, 1024);

	// General formats:
	//   Instance method call:
	//     methodName(this: thisType, arguments...)
	//   Static method call:
	//     methodName(arguments...)
	//   Arguments:
	//     arg0Type, arg1Type, arg2Type, ...
	//   aves.Method formatting:
	//     aves.Method(this: thisType, methodName)

	StackFrame *frame = currentFrame;
	while (frame)
	{
		Method *group = frame->method->group;

		buf.Append(this, ' ');

		// method name, which consists of:
		// fully.qualified.type
		// .
		// methodName
		// For global methods, group->name is already the fully qualified name.
		if (group->declType)
		{
			buf.Append(this, group->declType->fullName);
			buf.Append(this, '.');
		}
		buf.Append(this, group->name);
		buf.Append(this, '(');

		uint16_t paramCount = frame->method->paramCount;
		for (int i = 0; i < paramCount; i++)
		{
			if (i > 0)
				buf.Append(this, 2, ", ");
			AppendArgumentType(buf, frame->arguments[i]);
		}

		buf.Append(this, ')');
		buf.Append(this, '\n');
	}

	return buf.ToString(this);
}

void Thread::AppendArgumentType(StringBuffer &buf, Value arg)
{
	const Type *type = arg.type;
	if (type == nullptr)
		buf.Append(this, 4, "null");
	else
	{
		buf.Append(this, type->fullName);

		if (type == VM::vm->types.Method)
		{
			// Append some information about the instance and method group, too.
			MethodInst *method = arg.common.method;
			buf.Append(this, 7, "(this: ");
			AppendArgumentType(buf, method->instance);
			buf.Append(this, 2, ", ");

			Method *mgroup = method->method;
			if (mgroup->declType)
			{
				buf.Append(this, mgroup->declType->fullName);
				buf.Append(this, '.');
			}
			buf.Append(this, mgroup->name);

			buf.Append(this, ')');
		}
	}
}


// API functions, which are really just wrappers for the fun stuff.

OVUM_API void VM_Push(ThreadHandle thread, Value value)
{
	thread->Push(value);
}

OVUM_API void VM_PushNull(ThreadHandle thread)
{
	thread->Push(NULL_VALUE);
}

OVUM_API void VM_PushBool(ThreadHandle thread, const bool value)
{
	thread->PushBool(value);
}
OVUM_API void VM_PushInt(ThreadHandle thread, const int64_t value)
{
	thread->PushInt(value);
}
OVUM_API void VM_PushUInt(ThreadHandle thread, const uint64_t value)
{
	thread->PushUInt(value);
}
OVUM_API void VM_PushReal(ThreadHandle thread, const double value)
{
	thread->PushReal(value);
}
OVUM_API void VM_PushString(ThreadHandle thread, String *str)
{
	thread->PushString(str);
}

OVUM_API Value VM_Pop(ThreadHandle thread)
{
	return thread->Pop();
}
OVUM_API void VM_PopN(ThreadHandle thread, const unsigned int n)
{
	thread->Pop(n);
}

OVUM_API void VM_Dup(ThreadHandle thread)
{
	thread->Dup();
}

OVUM_API Value *VM_Local(ThreadHandle thread, const unsigned int n)
{
	return thread->Local(n);
}

OVUM_API void VM_Invoke(ThreadHandle thread, const unsigned int argCount, Value *result)
{
	thread->Invoke(argCount, result);
}
OVUM_API void VM_InvokeMember(ThreadHandle thread, String *name, const unsigned int argCount, Value *result)
{
	thread->InvokeMember(name, argCount, result);
}
OVUM_API void VM_InvokeMethod(ThreadHandle thread, MethodHandle method, const unsigned int argCount, Value *result)
{
	StackFrame *const frame = thread->currentFrame;
	thread->InvokeMethod(method, argCount, frame->evalStack + frame->stackCount - 1 - argCount, result);
}
OVUM_API void VM_InvokeOperator(ThreadHandle thread, Operator op, Value *result)
{
	thread->InvokeOperator(op, result);
}
OVUM_API bool VM_Equals(ThreadHandle thread)
{
	return thread->Equals();
}
OVUM_API int VM_Compare(ThreadHandle thread)
{
	return thread->Compare();
}

OVUM_API void VM_LoadMember(ThreadHandle thread, String *member, Value *result)
{
	thread->LoadMember(member, result);
}
OVUM_API void VM_StoreMember(ThreadHandle thread, String *member)
{
	thread->StoreMember(member);
}

OVUM_API void VM_LoadIndexer(ThreadHandle thread, const uint16_t argCount, Value *result)
{
	thread->LoadIndexer(argCount, result);
}
OVUM_API void VM_StoreIndexer(ThreadHandle thread, const uint16_t argCount)
{
	thread->StoreIndexer(argCount);
}

OVUM_API void VM_LoadStaticField(ThreadHandle thread, FieldHandle field, Value *result)
{
	thread->LoadStaticField(field, result);
}
OVUM_API void VM_StoreStaticField(ThreadHandle thread, FieldHandle field)
{
	thread->StoreStaticField(field);
}

OVUM_API void VM_ToString(ThreadHandle thread, String **result)
{
	thread->ToString(result);
}

OVUM_API void VM_Throw(ThreadHandle thread)
{
	thread->Throw();
}
OVUM_API void VM_ThrowError(ThreadHandle thread, String *message)
{
	thread->ThrowError(message);
}
OVUM_API void VM_ThrowTypeError(ThreadHandle thread, String *message)
{
	thread->ThrowTypeError(message);
}
OVUM_API void VM_ThrowMemoryError(ThreadHandle thread, String *message)
{
	thread->ThrowMemoryError(message);
}
OVUM_API void VM_ThrowOverflowError(ThreadHandle thread, String *message)
{
	thread->ThrowOverflowError(message);
}
OVUM_API void VM_ThrowDivideByZeroError(ThreadHandle thread, String *message)
{
	thread->ThrowDivideByZeroError(message);
}
OVUM_API void VM_ThrowNullReferenceError(ThreadHandle thread, String *message)
{
	thread->ThrowNullReferenceError(message);
}