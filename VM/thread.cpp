#include "ov_vm.internal.h"

namespace thread_errors
{
	namespace
	{
		LitString<92> _ConcatTypes                 = LitString<92>::FromCString("The concatenation operator requires two Lists, two Hashes, or two values of any other types.");
		LitString<43> _CompareType                 = LitString<43>::FromCString("The comparison operator must return an Int.");
		LitString<27> _NotInvokable                = LitString<27>::FromCString("The value is not invokable.");
		LitString<28> _NotComparable               = LitString<28>::FromCString("The value is not comparable.");
		LitString<30> _MemberNotFound              = LitString<30>::FromCString("The member could not be found.");
		LitString<28> _MemberNotInvokable          = LitString<28>::FromCString("The member is not invokable.");
		LitString<26> _AssigningToMethod           = LitString<26>::FromCString("Cannot assign to a method.");
		LitString<50> _StaticMemberThroughInstance = LitString<50>::FromCString("Cannot access a static member through an instance.");
		LitString<31> _GettingWriteonlyProperty    = LitString<31>::FromCString("Cannot get write-only property.");
		LitString<38> _SettingReadonlyProperty     = LitString<38>::FromCString("Cannot assign to a read-only property.");
		LitString<71> _WrongApplyArgsType          = LitString<71>::FromCString("The arguments list in a function application must be of type aves.List.");
		LitString<62> _NoIndexerFound              = LitString<62>::FromCString("The type does not contain an indexer, or it is not accessible.");
	}

	String *ConcatTypes                 = _S(_ConcatTypes);
	String *CompareType                 = _S(_CompareType);
	String *NotInvokable                = _S(_NotInvokable);
	String *NotComparable               = _S(_NotComparable);
	String *MemberNotFound              = _S(_MemberNotFound);
	String *MemberNotInvokable          = _S(_MemberNotInvokable);
	String *AssigningToMethod           = _S(_AssigningToMethod);
	String *StaticMemberThroughInstance = _S(_StaticMemberThroughInstance);
	String *GettingWriteonlyProperty    = _S(_GettingWriteonlyProperty);
	String *SettingReadonlyProperty     = _S(_SettingReadonlyProperty);
	String *WrongApplyArgsType          = _S(_WrongApplyArgsType);
	String *NoIndexerFound              = _S(_NoIndexerFound);
}

Thread::Thread() :
	currentFrame(nullptr), state(ThreadState::CREATED),
	currentError(NULL_VALUE), ip(nullptr),
	shouldSuspendForGC(false),
	flags(ThreadFlags::NONE)
{
	InitCallStack();
	InitGCLock();
}

Thread::~Thread()
{
	DisposeCallStack();
	DisposeGCLock();
}

void Thread::Start(Method *method, Value &result)
{
	assert(this->state == ThreadState::CREATED);
	assert((method->flags & MemberFlags::INSTANCE) == MemberFlags::NONE);

	state = ThreadState::RUNNING;
	Method::Overload *mo = method->ResolveOverload(0);
	assert(mo != nullptr);
	assert((mo->flags & MethodFlags::VARIADIC) == MethodFlags::NONE);

	StackFrame *frame = PushStackFrame<true>(0, nullptr, mo);

	if ((mo->flags & MethodFlags::NATIVE) == MethodFlags::NATIVE)
	{
		mo->nativeEntry(this, 0, (Value*)frame);
		if (frame->stackCount == 0)
			frame->evalStack[0].type = nullptr;
	}
	else
	{
		if (!mo->IsInitialized())
			InitializeMethod(mo);
		this->ip = mo->entry;
		entry:
		try
		{
			Evaluate(/*frame*/);
		}
		catch (OvumException&)
		{
			if (FindErrorHandler(frame))
				goto entry;
			throw;
		}
		assert(frame->stackCount == 1);
	}

	result = frame->evalStack[0];
	currentFrame = nullptr;
	ip = nullptr;

	state = ThreadState::STOPPED;

	// Done! Hopefully.
}


void Thread::PleaseSuspendForGCAsap()
{
	shouldSuspendForGC = true;
}

void Thread::EndGCSuspension()
{
	shouldSuspendForGC = false;
}

void Thread::SuspendForGC()
{
	assert(shouldSuspendForGC == true);

	state = ThreadState::SUSPENDED_BY_GC;
	// Do nothing here. Just wait for the GC to finish.
	EnterCriticalSection(&gcCycleSection);

	state = ThreadState::RUNNING;
	shouldSuspendForGC = false;
	// Resume normal operations!
	LeaveCriticalSection(&gcCycleSection);
}


void Thread::EnterFullyNativeRegion()
{
	flags |= ThreadFlags::IN_NATIVE_REGION;
}

void Thread::LeaveFullyNativeRegion()
{
	flags &= ~ThreadFlags::IN_NATIVE_REGION;
	if (shouldSuspendForGC)
		SuspendForGC();
}

bool Thread::IsSuspendedForGC() const
{
	return state == ThreadState::SUSPENDED_BY_GC ||
		(flags & ThreadFlags::IN_NATIVE_REGION) == ThreadFlags::IN_NATIVE_REGION;
}


void Thread::Invoke(unsigned int argCount, Value *result)
{
	Value *value = currentFrame->evalStack + currentFrame->stackCount - argCount - 1;
	if (result)
		InvokeLL(argCount, value, result);
	else
	{
		Value output;
		InvokeLL(argCount, value, &output);
		currentFrame->Push(output);
	}
}

// Note: argCount does NOT include the instance, but value does
void Thread::InvokeLL(unsigned int argCount, Value *value, Value *result)
{
	if (IS_NULL(*value))
		ThrowNullReferenceError();

	Method::Overload *mo = nullptr;

	// If the value is a Method instance, we use that instance's details.
	// Otherwise, we load the default invocator from the value.

	if (value->type == VM::vm->types.Method)
	{
		MethodInst *methodInst = value->common.method;
		if (mo = methodInst->method->ResolveOverload(argCount))
		{
			if (!IS_NULL(methodInst->instance))
				// Overwrite the Method with the instance
				*value = methodInst->instance;
			else
				// Shift the Method off the stack
				currentFrame->Shift(argCount);
		}
	}
	else
	{
		Member *member;
		if ((member = value->type->FindMember(static_strings::_call, currentFrame->method->declType)) &&
			(member->flags & MemberFlags::METHOD) != MemberFlags::NONE)
			mo = ((Method*)member)->ResolveOverload(argCount);
		else
			ThrowTypeError(thread_errors::NotInvokable);
	}

	if (!mo)
		ThrowNoOverloadError(argCount);

	// We've now found a method overload to invoke, omg!
	// So let's just pass it into InvokeMethodOverload.
	InvokeMethodOverload(mo, argCount, value, result);
}

void Thread::InvokeMethod(Method *method, unsigned int argCount, Value *result)
{
	Method::Overload *mo = method->ResolveOverload(argCount);
	if (mo == nullptr)
		ThrowNoOverloadError(argCount);

	Value *args = currentFrame->evalStack + currentFrame->stackCount - argCount - mo->InstanceOffset();
	if (result)
		InvokeMethodOverload(mo, argCount, args, result);
	else
	{
		Value output;
		InvokeMethodOverload(mo, argCount, args, &output);
		currentFrame->Push(output);
	}
}

void Thread::InvokeMember(String *name, unsigned int argCount, Value *result)
{
	Value *value = currentFrame->evalStack + currentFrame->stackCount - argCount - 1;
	if (result)
		InvokeMemberLL(name, argCount, value, result);
	else
	{
		Value output;
		InvokeMemberLL(name, argCount, value, &output);
		currentFrame->Push(output);
	}
}

void Thread::InvokeMemberLL(String *name, uint32_t argCount, Value *value, Value *result)
{
	if (IS_NULL(*value))
		ThrowNullReferenceError();

	Member *member;
	if (member = value->type->FindMember(name, currentFrame->method->declType))
	{
		if ((member->flags & MemberFlags::INSTANCE) == MemberFlags::NONE)
			ThrowTypeError(thread_errors::StaticMemberThroughInstance);

		switch (member->flags & MemberFlags::KIND)
		{
		case MemberFlags::FIELD:
			((Field*)member)->ReadFieldUnchecked(value, value);
			InvokeLL(argCount, value, result);
			break;
		case MemberFlags::PROPERTY:
			{
				if (((Property*)member)->getter == nullptr)
					ThrowTypeError(thread_errors::GettingWriteonlyProperty);

				Method::Overload *mo = ((Property*)member)->getter->ResolveOverload(0);
				if (!mo) ThrowNoOverloadError(0);
				// Call the property getter!
				// We do need to copy the instance, because the property getter
				// would otherwise overwrite the arguments already on the stack.
				currentFrame->Push(*value);
				InvokeMethodOverload(mo, 0,
					currentFrame->evalStack + currentFrame->stackCount - 1,
					value);
				// And then invoke the result of that call (which is in 'value')
				InvokeLL(argCount, value, result);
			}
			break;
		default: // method
			{
				Method::Overload *mo = ((Method*)member)->ResolveOverload(argCount);
				if (!mo) ThrowNoOverloadError(argCount);
				InvokeMethodOverload(mo, argCount, value, result);
			}
			break;
		}
	}
	else
		ThrowTypeError(thread_errors::MemberNotFound);
}

void Thread::InvokeMethodOverload(Method::Overload *mo, unsigned int argCount,
								  Value *args, Value *result)
{
	register MethodFlags flags = mo->flags; // used several times below!

	if ((flags & MethodFlags::VARIADIC) != MethodFlags::NONE)
	{
		PrepareVariadicArgs(flags, argCount, mo->paramCount, currentFrame);
		argCount = mo->paramCount;
	}

	argCount += (int)(flags & MethodFlags::INSTANCE) >> 3;

	// And now we can push the new stack frame!
	// Note: this updates currentFrame
	StackFrame *frame = PushStackFrame<false>(argCount, args, mo);

	if ((flags & MethodFlags::NATIVE) == MethodFlags::NATIVE)
	{
		try
		{
			mo->nativeEntry(this, argCount, args);
		}
		catch (OvumException&)
		{
			// Native methods have no handlers for OvumExceptions.
			// Hence, all we do is restore the previous stack frame and IP,
			// then rethrow.
			currentFrame = frame->prevFrame;
			this->ip = frame->prevInstr;
			throw;
		}
		// Native methods are not required to return with one value on the stack, but if
		// they have more than one, only the lowest one is used.
		if (frame->stackCount == 0)
			frame->evalStack[0].type = nullptr;
	}
	else
	{
		if (!mo->IsInitialized())
			InitializeMethod(mo);

		this->ip = mo->entry;
		entry:
		try
		{
			Evaluate(/*frame*/);
		}
		catch (OvumException&)
		{
			if (FindErrorHandler(frame))
				// IP is now at the catch handler's offset, so let's
				// re-enter the method!
				goto entry;

			// Restore previous stack frame and IP, and rethrow
			currentFrame = frame->prevFrame;
			this->ip = frame->prevInstr;
			throw;
		}
		// It should not be possible to return from a method with
		// anything other than exactly one value on the stack!
		assert(frame->stackCount == 1);
	}

	// restore previous stack frame
	currentFrame = frame->prevFrame;
	this->ip = frame->prevInstr;
	// Note: If the method has 0 parameters and the result is on the
	// caller's eval stack, then it may very well point directly into
	// the frame we have here. Hence, we must assign this /after/
	// restoring to the previous stack frame, otherwise we may
	// overwrite frame->prevFrame and/or frame->prevInstr
	*result = frame->evalStack[0];

	// Done!
}

void Thread::InvokeOperator(Operator op, Value *result)
{
	InvokeOperatorLL(currentFrame->evalStack + currentFrame->stackCount - Arity(op), op, result);
}

void Thread::InvokeOperatorLL(Value *args, Operator op, Value *result)
{
	if (IS_NULL(args[0]))
		ThrowNullReferenceError();

	Method::Overload *method = args[0].type->operators[(int)op];
	if (method == nullptr)
		ThrowMissingOperatorError(op);

	InvokeMethodOverload(method, Arity(op), args, result);
}

void Thread::InvokeApply(Value *result)
{
	Value *args = currentFrame->evalStack + currentFrame->stackCount - 2;
	if (result != nullptr)
		InvokeApplyLL(args, result);
	else
	{
		Value output;
		InvokeApplyLL(args, &output);
		currentFrame->Push(output);
	}
}

void Thread::InvokeApplyLL(Value *args, Value *result)
{
	// First, ensure that args[1] is a List.
	if (!Type::ValueIsType(args + 1, VM::vm->types.List))
		ThrowTypeError(thread_errors::WrongApplyArgsType);

	// Then, unpack it onto the evaluation stack!
	ListInst *argsList = args[1].common.list;
	currentFrame->stackCount--;
	CopyMemoryT(currentFrame->evalStack + currentFrame->stackCount, argsList->values, argsList->length);
	currentFrame->stackCount += argsList->length;

	InvokeLL(argsList->length, args, result);
}

void Thread::InvokeApplyMethod(Method *method, Value *result)
{
	Value *args = currentFrame->evalStack + currentFrame->stackCount - 1;
	if (result != nullptr)
		InvokeApplyMethodLL(method, args, result);
	else
	{
		Value output;
		InvokeApplyMethodLL(method, args, &output);
		currentFrame->Push(output);
	}
}

void Thread::InvokeApplyMethodLL(Method *method, Value *args, Value *result)
{
	// First, ensure that args[0] is a List
	if (!Type::ValueIsType(args, VM::vm->types.List))
		ThrowTypeError(thread_errors::WrongApplyArgsType);

	assert((method->flags & MemberFlags::INSTANCE) == MemberFlags::NONE);

	ListInst *argsList = args->common.list;

	// Then, find an appropriate overload!
	Method::Overload *mo = nullptr;
	if (args->common.list->length <= UINT16_MAX)
		mo = method->ResolveOverload(argsList->length);
	if (mo == nullptr)
		ThrowNoOverloadError(argsList->length);

	// Only now that we've found an overload do we start unpacking values and stuff.
	currentFrame->stackCount--;
	CopyMemoryT(currentFrame->evalStack + currentFrame->stackCount, argsList->values, argsList->length);
	currentFrame->stackCount += argsList->length;

	InvokeMethodOverload(mo, argsList->length, args, result);
}


bool Thread::Equals()
{
	Value *args = currentFrame->evalStack + currentFrame->stackCount - 2;
	return EqualsLL(args);
}

bool Thread::EqualsLL(Value *args)
{
	if (IS_NULL(args[0]) || IS_NULL(args[1]))
	{
		currentFrame->stackCount -= 2;
		return args[0].type == args[1].type;
	}

	// Some code here is duplicated from InvokeOperatorLL, which we
	// don't call directly; we want to avoid the null check.

	Method::Overload *method = args[0].type->operators[(int)Operator::EQ];
	// Don't need to test method for nullness: every type supports ==,
	// because Object supports ==.
	assert(method != nullptr); // okay, fine, but only when debugging

	Value result;
	InvokeMethodOverload(method, 2, args, &result);

	return IsTrue_(result);
}

int64_t Thread::Compare()
{
	Value *args = currentFrame->evalStack + currentFrame->stackCount - 2;
	Value result;
	CompareLL(args, &result);
	return result.integer;
}

void Thread::Concat(Value *result)
{
	ConcatLL(currentFrame->evalStack + currentFrame->stackCount - 2, result);
}

void Thread::ConcatLL(Value *args, Value *result)
{
	register Value *a = args;
	register Value *b = args + 1;
	if (a->type == VM::vm->types.List || b->type == VM::vm->types.List)
	{
		// list concatenation
		if (a->type != b->type)
			ThrowTypeError(thread_errors::ConcatTypes);

		Value output;
		GC::gc->Alloc(this, VM::vm->types.List, sizeof(ListInst), &output);

		int32_t length = a->common.list->length + b->common.list->length;
		VM::vm->functions.initListInstance(this, output.common.list, length);
		if (length > 0)
		{
			CopyMemoryT(output.common.list->values, a->common.list->values, a->common.list->length);
			CopyMemoryT(output.common.list->values + a->common.list->length, b->common.list->values, b->common.list->length);
		}
		output.common.list->length = length;

		*result = output;
	}
	else if (a->type == VM::vm->types.Hash || b->type == VM::vm->types.Hash)
	{
		// hash concatenation
		if (a->type != b->type)
			ThrowTypeError(thread_errors::ConcatTypes);

		static Method::Overload *hashSetItem = nullptr;
		if (!hashSetItem) GetHashIndexerSetter(&hashSetItem);
		assert(hashSetItem != nullptr);

		register Value *hash = args + 2; // Put the hash on the stack for extra GC reachability!

		GC::gc->Alloc(this, VM::vm->types.Hash, sizeof(HashInst), hash);
		VM::vm->functions.initHashInstance(this, hash->common.hash, max(a->common.hash->count, b->common.hash->count));

		register StackFrame *f = currentFrame;
		f->stackCount++;

		Value ignore;
		do
		{
			for (int32_t i = 0; i < a->common.hash->count; i++)
			{
				HashEntry *e = &a->common.hash->entries[i];
				hash[1] = hash[0]; // dup the hash
				hash[2] = e->key;
				hash[3] = e->value;
				f->stackCount += 3;
				// InvokeMethodOverload pops the three effective arguments
				InvokeMethodOverload(hashSetItem, 2, hash + 1, &ignore);
			}
			a++;
		} while (a == b);

		*result = *hash;
		f->stackCount--; // Pop the hash off the stack again
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
	currentFrame->stackCount -= 2;
}

void Thread::GetHashIndexerSetter(Method::Overload **target)
{
	Member *m = VM::vm->types.Hash->GetMember(static_strings::_item);

	assert((m->flags & MemberFlags::KIND) == MemberFlags::PROPERTY);
	assert(((Property*)m)->setter != nullptr);

	*target = ((Property*)m)->setter->ResolveOverload(2);
}


// Base implementation of the various comparison methods
// This duplicates a lot of code from InvokeOperatorLL
// (Semicolon intentionally missing from the last statement)
#define COMPARE_BASE(pResult) \
	if (IS_NULL(args[0])) \
		ThrowNullReferenceError(); \
	\
	Method::Overload *method = args[0].type->operators[(int)Operator::CMP]; \
	if (method == nullptr) \
		ThrowTypeError(thread_errors::NotComparable); \
	\
	InvokeMethodOverload(method, 2, args, (pResult)); \
	if ((pResult)->type != VM::vm->types.Int) \
		ThrowTypeError(thread_errors::CompareType)

void Thread::CompareLL(Value *args, Value *result)
{
	COMPARE_BASE(result);
}

bool Thread::CompareLessThanLL(Value *args)
{
	Value result;
	COMPARE_BASE(&result);
	return result.integer < 0;
}

bool Thread::CompareGreaterThanLL(Value *args)
{
	Value result;
	COMPARE_BASE(&result);
	return result.integer > 0;
}

bool Thread::CompareLessEqualsLL(Value *args)
{
	Value result;
	COMPARE_BASE(&result);
	return result.integer <= 0;
}

bool Thread::CompareGreaterEqualsLL(Value *args)
{
	Value result;
	COMPARE_BASE(&result);
	return result.integer >= 0;
}

#undef COMPARE_BASE


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
	if (IS_NULL(*instance))
		ThrowNullReferenceError();

	const Member *m = instance->type->FindMember(member, currentFrame->method->declType);
	if (m == nullptr)
		ThrowTypeError(thread_errors::MemberNotFound);
	if ((m->flags & MemberFlags::INSTANCE) == MemberFlags::NONE)
		ThrowTypeError(thread_errors::StaticMemberThroughInstance);

	if ((m->flags & MemberFlags::FIELD) != MemberFlags::NONE)
	{
		reinterpret_cast<const Field*>(m)->ReadFieldUnchecked(instance, result);
		currentFrame->Pop(1); // Done with the instance!
	}
	else if ((m->flags & MemberFlags::METHOD) != MemberFlags::NONE)
	{
		Value output;
		GC::gc->Alloc(this, VM::vm->types.Method, sizeof(MethodInst), &output);
		output.common.method->instance = *instance;
		output.common.method->method = (Method*)m;
		*result = output;
		currentFrame->Pop(1); // Done with the instance!
	}
	else // MemberFlags::PROPERTY
	{
		const Property *p = (Property*)m;
		if (!p->getter)
			ThrowTypeError(thread_errors::GettingWriteonlyProperty);

		Method::Overload *mo = p->getter->ResolveOverload(0);
		if (!mo) ThrowNoOverloadError(0);

		// Remember: the instance is already on the stack!
		InvokeMethodOverload(mo, 0, instance, result);
	}
}

void Thread::StoreMember(String *member)
{
	Value *args = currentFrame->evalStack + currentFrame->stackCount - 2;
	StoreMemberLL(args, member);
}

void Thread::StoreMemberLL(Value *instance, String *member)
{
	if (IS_NULL(*instance))
		ThrowNullReferenceError();

	Member *m = instance->type->FindMember(member, currentFrame->method->declType);
	if ((m->flags & MemberFlags::INSTANCE) == MemberFlags::NONE)
		ThrowTypeError(thread_errors::StaticMemberThroughInstance);
	if ((m->flags & MemberFlags::METHOD) != MemberFlags::NONE)
		ThrowTypeError(thread_errors::AssigningToMethod);

	if ((m->flags & MemberFlags::FIELD) != MemberFlags::NONE)
		reinterpret_cast<Field*>(m)->WriteFieldUnchecked(instance);
	else // MemberFlags::PROPERTY
	{
		Property *p = (Property*)m;
		if (!p->setter)
			ThrowTypeError(thread_errors::SettingReadonlyProperty);

		Method::Overload *mo = p->getter->ResolveOverload(1);
		if (!mo) ThrowNoOverloadError(1);

		// Remember: the instance and value are already on the stack!
		InvokeMethodOverload(mo, 1, instance, nullptr);
	}

	currentFrame->Pop(2); // Done with the instance and the value!
}

// Note: argCount does NOT include the instance.
void Thread::LoadIndexer(uint32_t argCount, Value *result)
{
	Value *args = currentFrame->evalStack + currentFrame->stackCount - argCount - 1;
	if (result)
		LoadIndexerLL(argCount, args, result);
	else
	{
		Value output;
		LoadIndexerLL(argCount, args, &output);
		currentFrame->Push(output);
	}
}
// Note: argc DOES NOT include the instance, but args DOES.
void Thread::LoadIndexerLL(uint32_t argCount, Value *args, Value *result)
{
	if (IS_NULL(args[0]))
		ThrowNullReferenceError();

	Member *member = args[0].type->FindMember(static_strings::_item, currentFrame->method->declType);
	if (!member)
		ThrowTypeError(thread_errors::NoIndexerFound);

	// The indexer, if present, MUST be an instance property.
	assert((member->flags & MemberFlags::INSTANCE) == MemberFlags::INSTANCE);
	assert((member->flags & MemberFlags::PROPERTY) == MemberFlags::PROPERTY);

	if (((Property*)member)->getter == nullptr)
		ThrowTypeError(thread_errors::GettingWriteonlyProperty);

	Method::Overload *method = ((Property*)member)->getter->ResolveOverload(argCount);
	if (!method)
		ThrowNoOverloadError(argCount);
	InvokeMethodOverload(method, argCount, args, result);
}

// Note: argCount DOES NOT include the instance or the value that's being stored.
void Thread::StoreIndexer(uint32_t argCount)
{
	Value *args = currentFrame->evalStack + currentFrame->stackCount - argCount - 2;
	StoreIndexerLL(argCount, args);
}

// Note: argCount DOES NOT include the instance or the value that's being stored, but args DOES.
void Thread::StoreIndexerLL(uint32_t argCount, Value *args)
{
	if (IS_NULL(args[0]))
		ThrowNullReferenceError();

	Member *member = args[0].type->FindMember(static_strings::_item, currentFrame->method->declType);
	if (!member)
		ThrowTypeError(thread_errors::NoIndexerFound);

	// The indexer, if present, MUST be an instance property.
	assert((member->flags & MemberFlags::INSTANCE) == MemberFlags::INSTANCE);
	assert((member->flags & MemberFlags::PROPERTY) == MemberFlags::PROPERTY);

	if (((Property*)member)->setter == nullptr)
		ThrowTypeError(thread_errors::SettingReadonlyProperty);

	Method::Overload *method = ((Property*)member)->setter->ResolveOverload(argCount + 1);
	if (!method)
		ThrowNoOverloadError(argCount + 1);
	Value ignore;
	InvokeMethodOverload(method, argCount + 1, args, &ignore);
}

void Thread::LoadStaticField(Field *field, Value *result)
{
	if (result)
		*result = field->staticValue->Read();
	else
		currentFrame->Push(field->staticValue->Read());
}

void Thread::StoreStaticField(Field *field)
{
	field->staticValue->Write(currentFrame->Pop());
}

void Thread::ToString(String **result)
{
	LoadMember(static_strings::toString, nullptr);
	Invoke(0, nullptr);

	if (currentFrame->PeekType(0) != VM::vm->types.String)
		ThrowTypeError(static_strings::errors::ToStringWrongType);

	if (result != nullptr)
	{
		*result = currentFrame->PeekString(0);
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
		currentFrame->PushNull();
	else
		currentFrame->PushString(message);
	GC::gc->Construct(this, VM::vm->types.Error, 1, nullptr);
	Throw();
}

void Thread::ThrowTypeError(String *message)
{
	if (message == nullptr)
		currentFrame->PushNull();
	else
		currentFrame->PushString(message);
	GC::gc->Construct(this, VM::vm->types.TypeError, 1, nullptr);
	Throw();
}

void Thread::ThrowMemoryError(String *message)
{
	if (message == nullptr)
		currentFrame->PushNull();
	else
		currentFrame->PushString(message);
	GC::gc->Construct(this, VM::vm->types.MemoryError, 1, nullptr);
	Throw();
}

void Thread::ThrowOverflowError(String *message)
{
	if (message == nullptr)
		currentFrame->PushNull();
	else
		currentFrame->PushString(message);
	GC::gc->Construct(this, VM::vm->types.OverflowError, 1, nullptr);
	Throw();
}

void Thread::ThrowDivideByZeroError(String *message)
{
	if (message == nullptr)
		currentFrame->PushNull();
	else
		currentFrame->PushString(message);
	GC::gc->Construct(this, VM::vm->types.DivideByZeroError, 1, nullptr);
	Throw();
}

void Thread::ThrowNullReferenceError(String *message)
{
	if (message == nullptr)
		currentFrame->PushNull();
	else
		currentFrame->PushString(message);
	GC::gc->Construct(this, VM::vm->types.NullReferenceError, 1, nullptr);
	Throw();
}

void Thread::ThrowNoOverloadError(const uint32_t argCount, String *message)
{
	currentFrame->PushInt(argCount);
	if (message == nullptr)
		currentFrame->PushNull();
	else
		currentFrame->PushString(message);
	GC::gc->Construct(this, VM::vm->types.NoOverloadError, 2, nullptr);
	Throw();
}

void Thread::ThrowMissingOperatorError(Operator op)
{
	static LitString<3> operatorNames[] = {
		{ 1, 0, StringFlags::STATIC, '+',0         }, // Operator::ADD
		{ 1, 0, StringFlags::STATIC, '-',0         }, // Operator::SUB
		{ 1, 0, StringFlags::STATIC, '|',0         }, // Operator::OR
		{ 1, 0, StringFlags::STATIC, '^',0         }, // Operator::XOR
		{ 1, 0, StringFlags::STATIC, '*',0         }, // Operator::MUL
		{ 1, 0, StringFlags::STATIC, '/',0         }, // Operator::DIV
		{ 1, 0, StringFlags::STATIC, '%',0         }, // Operator::MOD
		{ 1, 0, StringFlags::STATIC, '&',0         }, // Operator::AND
		{ 2, 0, StringFlags::STATIC, '*','*',0     }, // Operator::POW
		{ 2, 0, StringFlags::STATIC, '<','<',0     }, // Operator::SHL
		{ 2, 0, StringFlags::STATIC, '>','>',0     }, // Operator::SHR
		{ 1, 0, StringFlags::STATIC, '#',0         }, // Operator::HASHOP
		{ 1, 0, StringFlags::STATIC, '$',0         }, // Operator::DOLLAR
		{ 1, 0, StringFlags::STATIC, '+',0         }, // Operator::PLUS
		{ 1, 0, StringFlags::STATIC, '-',0         }, // Operator::NEG
		{ 1, 0, StringFlags::STATIC, '~',0         }, // Operator::NOT
		{ 2, 0, StringFlags::STATIC, '=','=',0     }, // Operator::EQ
		{ 3, 0, StringFlags::STATIC, '<','=','>',0 }, // Operator::CMP
	};
	static const wchar_t *const baseMessage = L"The type does not support the specified operator. (Operator: ";

	{
		StringBuffer message(this);
		message.Append(this, wcslen(baseMessage), baseMessage);
		message.Append(this, _S(operatorNames[(int)op]));
		message.Append(this, ')');
		PushString(message.ToString(this));
	}
	GC::gc->Construct(this, VM::vm->types.TypeError, 1, nullptr);
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

void Thread::InitGCLock()
{
	InitializeCriticalSection(&gcCycleSection);
}

void Thread::DisposeGCLock()
{
	DeleteCriticalSection(&gcCycleSection);
}


// Note: argCount and args DO include the instance here!
template<bool First>
StackFrame *Thread::PushStackFrame(const uint32_t argCount, Value *args, Method::Overload *method)
{
	if (First)
	{
		assert(currentFrame == nullptr);
		if (argCount)
			CopyMemoryT(reinterpret_cast<Value*>(callStack), args, argCount);
	}
	else
	{
		assert(currentFrame->stackCount >= argCount);
		currentFrame->stackCount -= argCount; // pop the arguments (including the instance) off the current frame
	}

	register uint32_t paramCount = method->GetEffectiveParamCount();
	register uint32_t localCount = method->locals;
	register StackFrame *newFrame = reinterpret_cast<StackFrame*>((First ? (Value*)callStack : args) + paramCount);

	newFrame->stackCount = 0;
	newFrame->argc = argCount;
	newFrame->evalStack = (Value*)((char*)newFrame + STACK_FRAME_SIZE) + localCount;
	newFrame->prevInstr = First ? nullptr : ip;
	newFrame->prevFrame = First ? nullptr : currentFrame;
	newFrame->method = method;
	
	// initialize missing arguments to null
	if (argCount != paramCount)
	{
		register Value *missing = args + argCount;
		while ((void*)missing != (void*)newFrame)
			(missing++)->type = nullptr;
	}

	// Also initialize all locals to null
	if (localCount)
	{
		register Value *locals = LOCALS_OFFSET(newFrame);
		while (localCount--)
			(locals++)->type = nullptr;
	}

	return currentFrame = newFrame;
}

void Thread::PrepareVariadicArgs(const MethodFlags flags, const uint32_t argCount, const uint32_t paramCount, StackFrame *frame)
{
	int32_t count = argCount >= paramCount - 1 ? argCount - paramCount + 1 : 0;

	Value listValue;
	// Construct the list!
	// We cannot really make any assumptions about the List constructor,
	// so we can't call it here. Instead, we "manually" allocate a ListInst,
	// set its type to List, and initialize its fields.
	GC::gc->Alloc(this, VM::vm->types.List, sizeof(ListInst), &listValue);
	ListInst *list = listValue.common.list;
	VM::vm->functions.initListInstance(this, list, count);
	list->length = count;

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

		buf.Append(this, 2, ' ');

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

		uint16_t paramCount = frame->method->GetEffectiveParamCount();

		for (int i = 0; i < paramCount; i++)
		{
			if (i > 0)
				buf.Append(this, 2, ", ");
			else if (i == 0 && frame->method->IsInstanceMethod())
				buf.Append(this, 6, "this: ");
			AppendArgumentType(buf, ((Value*)frame - paramCount)[i]);
		}

		buf.Append(this, ')');
		buf.Append(this, '\n');

		frame = frame->prevFrame;
	}

	return buf.ToString(this);
}

void Thread::AppendArgumentType(StringBuffer &buf, Value arg)
{
	Type *type = arg.type;
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
	thread->PushNull();
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
OVUM_API void VM_PopN(ThreadHandle thread, const uint32_t n)
{
	thread->Pop(n);
}

OVUM_API void VM_Dup(ThreadHandle thread)
{
	thread->Dup();
}

OVUM_API Value *VM_Local(ThreadHandle thread, const uint32_t n)
{
	return thread->Local(n);
}

OVUM_API void VM_Invoke(ThreadHandle thread, const uint32_t argCount, Value *result)
{
	thread->Invoke(argCount, result);
}
OVUM_API void VM_InvokeMember(ThreadHandle thread, String *name, const uint32_t argCount, Value *result)
{
	thread->InvokeMember(name, argCount, result);
}
OVUM_API void VM_InvokeMethod(ThreadHandle thread, MethodHandle method, const uint32_t argCount, Value *result)
{
	thread->InvokeMethod(method, argCount, result);
}
OVUM_API void VM_InvokeOperator(ThreadHandle thread, Operator op, Value *result)
{
	thread->InvokeOperator(op, result);
}
OVUM_API bool VM_Equals(ThreadHandle thread)
{
	return thread->Equals();
}
OVUM_API int64_t VM_Compare(ThreadHandle thread)
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

OVUM_API void VM_LoadIndexer(ThreadHandle thread, const uint32_t argCount, Value *result)
{
	thread->LoadIndexer(argCount, result);
}
OVUM_API void VM_StoreIndexer(ThreadHandle thread, const uint32_t argCount)
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

OVUM_API void VM_EnterFullyNativeRegion(ThreadHandle thread)
{
	thread->EnterFullyNativeRegion();
}
OVUM_API void VM_LeaveFullyNativeRegion(ThreadHandle thread)
{
	thread->LeaveFullyNativeRegion();
}
OVUM_API bool VM_IsInFullyNativeRegion(ThreadHandle thread)
{
	return thread->IsInFullyNativeRegion();
}

OVUM_API void VM_Sleep(ThreadHandle thread, unsigned int milliseconds)
{
	thread->EnterFullyNativeRegion();

	Sleep(milliseconds);

	thread->LeaveFullyNativeRegion();
}

OVUM_API String *VM_GetStackTrace(ThreadHandle thread)
{
	return thread->GetStackTrace();
}