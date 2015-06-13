#include "thread.h"
#include "vm.h"
#include "../object/type.h"
#include "../object/member.h"
#include "../object/field.h"
#include "../object/property.h"
#include "../object/method.h"
#include "../object/value.h"
#include "../gc/gc.h"
#include "../gc/staticref.h"
#include "../debug/debugsymbols.h"
#include "../util/stringbuffer.h"
#include <memory>

namespace ovum
{

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
		LitString<93> _IncorrectReferenceness      = LitString<93>::FromCString("One or more arguments has the wrong referenceness (should be a ref but isn't, or vice versa).");
		LitString<36> _MemberIsNotAField           = LitString<36>::FromCString("The specified member is not a field.");
	}

	String *ConcatTypes                 = _ConcatTypes.AsString();
	String *CompareType                 = _CompareType.AsString();
	String *NotInvokable                = _NotInvokable.AsString();
	String *NotComparable               = _NotComparable.AsString();
	String *MemberNotFound              = _MemberNotFound.AsString();
	String *MemberNotInvokable          = _MemberNotInvokable.AsString();
	String *AssigningToMethod           = _AssigningToMethod.AsString();
	String *StaticMemberThroughInstance = _StaticMemberThroughInstance.AsString();
	String *GettingWriteonlyProperty    = _GettingWriteonlyProperty.AsString();
	String *SettingReadonlyProperty     = _SettingReadonlyProperty.AsString();
	String *WrongApplyArgsType          = _WrongApplyArgsType.AsString();
	String *NoIndexerFound              = _NoIndexerFound.AsString();
	String *IncorrectReferenceness      = _IncorrectReferenceness.AsString();
	String *MemberIsNotAField           = _MemberIsNotAField.AsString();
}

TlsEntry<Thread> Thread::threadKey;

int Thread::Create(VM *owner, Thread *&result)
{
	// Try to allocate the TLS key first
	if (!threadKey.IsValid() && !threadKey.Alloc())
		return OVUM_ERROR_NO_MEMORY;

	// And now make the thread!
	int status;
	result = new(std::nothrow) Thread(owner, status);
	if (!result)
		status = OVUM_ERROR_NO_MEMORY;
	return status;
}

Thread::Thread(VM *owner, int &status) :
	ip(nullptr),
	currentFrame(nullptr),
	pendingRequest(ThreadRequest::NONE),
	state(ThreadState::CREATED),
	flags(ThreadFlags::NONE),
	callStack(nullptr),
	vm(owner),
	currentError(NULL_VALUE),
	gcCycleSection(4000),
	hashSetItem(nullptr)
{
	status = InitCallStack();
	if (status == OVUM_SUCCESS)
	{
		nativeId = os::GetCurrentThread();
		// Associate the VM with the native thread
		VM::vmKey.Set(owner);
		// And this managed thread, too
		threadKey.Set(this);
	}
}

Thread::~Thread()
{
	DisposeCallStack();
}

int Thread::Start(unsigned int argCount, MethodOverload *mo, Value &result)
{
	assert(mo != nullptr);
	assert(this->state == ThreadState::CREATED);
	assert((mo->flags & MethodFlags::INSTANCE) == MethodFlags::NONE);

	state = ThreadState::RUNNING;

	Value *args = currentFrame->evalStack + currentFrame->stackCount - argCount;
	int r = InvokeMethodOverload(mo, argCount, args, &result);

	state = ThreadState::STOPPED;

	// Done! Hopefully.
	return r;
}


void Thread::HandleRequest()
{
	switch (pendingRequest)
	{
	case ThreadRequest::SUSPEND_FOR_GC:
		SuspendForGC();
		break;
	}
}

void Thread::PleaseSuspendForGCAsap()
{
	pendingRequest = ThreadRequest::SUSPEND_FOR_GC;
}

void Thread::EndGCSuspension()
{
	pendingRequest = ThreadRequest::NONE;
}

void Thread::SuspendForGC()
{
	assert(pendingRequest == ThreadRequest::SUSPEND_FOR_GC);

	state = ThreadState::SUSPENDED_BY_GC;
	// Do nothing here. Just wait for the GC to finish.
	gcCycleSection.Enter();

	state = ThreadState::RUNNING;
	pendingRequest = ThreadRequest::NONE;
	// Resume normal operations!
	gcCycleSection.Leave();
}


void Thread::EnterUnmanagedRegion()
{
	flags |= ThreadFlags::IN_UNMANAGED_REGION;
}

void Thread::LeaveUnmanagedRegion()
{
	flags &= ~ThreadFlags::IN_UNMANAGED_REGION;
	if (pendingRequest != ThreadRequest::NONE)
		HandleRequest();
}

bool Thread::IsSuspendedForGC() const
{
	return state == ThreadState::SUSPENDED_BY_GC || IsInUnmanagedRegion();
}


int Thread::Invoke(unsigned int argCount, Value *result)
{
	int r;
	Value *value = currentFrame->evalStack + currentFrame->stackCount - argCount - 1;
	if (result != nullptr)
		r = InvokeLL(argCount, value, result, 0);
	else
	{
		r = InvokeLL(argCount, value, value, 0);
		if (r == OVUM_SUCCESS)
			currentFrame->stackCount++;
	}
	return r;
}

// Note: argCount does NOT include the instance, but value does
int Thread::InvokeLL(unsigned int argCount, Value *value, Value *result, uint32_t refSignature)
{
	if (IS_NULL(*value))
		return ThrowNullReferenceError();

	MethodOverload *mo = nullptr;

	// If the value is a Method instance, we use that instance's details.
	// Otherwise, we load the default invocator from the value.

	if (value->type == vm->types.Method)
	{
		MethodInst *methodInst = value->v.method;
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
			return ThrowTypeError(thread_errors::NotInvokable);
	}

	if (!mo)
		return ThrowNoOverloadError(argCount);
	
	if (refSignature != mo->refSignature &&
		mo->VerifyRefSignature(refSignature, argCount) != -1)
		return ThrowNoOverloadError(argCount, thread_errors::IncorrectReferenceness);
	// We've now found a method overload to invoke, omg!
	// So let's just pass it into InvokeMethodOverload.
	return InvokeMethodOverload(mo, argCount, value, result);
}

int Thread::InvokeMethod(Method *method, unsigned int argCount, Value *result)
{
	MethodOverload *mo = method->ResolveOverload(argCount);
	if (mo == nullptr)
		return ThrowNoOverloadError(argCount);

	int r;
	Value *args = currentFrame->evalStack + currentFrame->stackCount - argCount - mo->InstanceOffset();
	if (result)
		r = InvokeMethodOverload(mo, argCount, args, result);
	else
	{
		r = InvokeMethodOverload(mo, argCount, args, args);
		if (r == OVUM_SUCCESS)
			currentFrame->stackCount++;
	}
	return r;
}

int Thread::InvokeMember(String *name, unsigned int argCount, Value *result)
{
	int r;
	Value *value = currentFrame->evalStack + currentFrame->stackCount - argCount - 1;
	if (result)
		r = InvokeMemberLL(name, argCount, value, result, 0);
	else
	{
		r = InvokeMemberLL(name, argCount, value, value, 0);
		if (r == OVUM_SUCCESS)
			currentFrame->stackCount++;
	}
	return r;
}

int Thread::InvokeMemberLL(String *name, uint32_t argCount, Value *value, Value *result, uint32_t refSignature)
{
	if (IS_NULL(*value))
		return ThrowNullReferenceError();

	Member *member;
	if (member = value->type->FindMember(name, currentFrame->method->declType))
	{
		if ((member->flags & MemberFlags::INSTANCE) == MemberFlags::NONE)
			return ThrowTypeError(thread_errors::StaticMemberThroughInstance);

		switch (member->flags & MemberFlags::KIND)
		{
		case MemberFlags::FIELD:
			((Field*)member)->ReadFieldUnchecked(value, value);
			return InvokeLL(argCount, value, result, refSignature);
			break;
		case MemberFlags::PROPERTY:
			{
				if (((Property*)member)->getter == nullptr)
					return ThrowTypeError(thread_errors::GettingWriteonlyProperty);

				MethodOverload *mo = ((Property*)member)->getter->ResolveOverload(0);
				if (!mo) return ThrowNoOverloadError(0);
				// Call the property getter!
				// We do need to copy the instance, because the property getter
				// would otherwise overwrite the arguments already on the stack.
				Push(value);
				int r = InvokeMethodOverload(mo, 0,
					currentFrame->evalStack + currentFrame->stackCount - 1,
					value);
				if (r != OVUM_SUCCESS) return r;

				// And then invoke the result of that call (which is in 'value')
				return InvokeLL(argCount, value, result, refSignature);
			}
			break;
		default: // method
			{
				MethodOverload *mo = ((Method*)member)->ResolveOverload(argCount);
				if (!mo)
					return ThrowNoOverloadError(argCount);
				if (refSignature != mo->refSignature &&
					mo->VerifyRefSignature(refSignature, argCount) != -1)
					return ThrowNoOverloadError(argCount, thread_errors::IncorrectReferenceness);
				return InvokeMethodOverload(mo, argCount, value, result);
			}
		}
	}

	return ThrowMemberNotFoundError(name);
}

int Thread::InvokeMethodOverload(MethodOverload *mo, unsigned int argCount,
                                 Value *args, Value *result)
{
	register MethodFlags flags = mo->flags; // used several times below!

	int r;
	if ((flags & MethodFlags::VARIADIC) != MethodFlags::NONE)
	{
		r = PrepareVariadicArgs(flags, argCount, mo->paramCount, currentFrame);
		if (r != OVUM_SUCCESS) return r;
		argCount = mo->paramCount;
	}

	argCount += (int)(flags & MethodFlags::INSTANCE) >> 3;

	// And now we can push the new stack frame!
	// Note: this updates currentFrame
	PushStackFrame(argCount, args, mo);

	if ((flags & MethodFlags::NATIVE) == MethodFlags::NATIVE)
	{
		if (pendingRequest != ThreadRequest::NONE)
			HandleRequest();
		r = mo->nativeEntry(this, argCount, args);
		// Native methods are not required to return with one value on the stack, but if
		// they have more than one, only the lowest one is used.
		if (r == OVUM_SUCCESS && currentFrame->stackCount == 0)
			currentFrame->evalStack[0].type = nullptr;
	}
	else
	{
		if (!mo->IsInitialized())
		{
			// This calls abort() if the initialization fails,
			// but not if a static constructor call fails.
			r = InitializeMethod(mo);
			if (r != OVUM_SUCCESS) goto restore;
		}

		this->ip = mo->entry;
		entry:
		r = Evaluate();
		if (r != OVUM_SUCCESS)
		{
			if (r == OVUM_ERROR_THROWN)
			{
				int r2 = FindErrorHandler(-1);
				if (r2 == OVUM_SUCCESS)
					// Error handler found! IP is now at the catch
					// handler's offset, so let's re-enter the method.
					goto entry;
				r = r2; // overwrite previous error
			}
			// If we fail to locate an error handler, or if the
			// error is not one we can handle, fall through to
			// restore the previous stack frame, then return r.
		}
#ifndef NDEBUG
		else
		{
			// It should not be possible to return from a method with
			// anything other than exactly one value on the stack!
			assert(currentFrame->stackCount == 1);
		}
#endif
	}

	// restore previous stack frame
	restore:
	register StackFrame *frame = currentFrame;
	currentFrame = frame->prevFrame;
	this->ip = frame->prevInstr;
	if (r == OVUM_SUCCESS)
		// Note: If the method has 0 parameters and the result is on the
		// caller's eval stack, then it may very well point directly into
		// the frame we have here. Hence, we must assign this /after/
		// restoring to the previous stack frame, otherwise we may
		// overwrite frame->prevFrame and/or frame->prevInstr
		*result = frame->evalStack[0];

	// Done!
	return r;
}

int Thread::InvokeOperator(Operator op, Value *result)
{
	int r;
	Value *args = currentFrame->evalStack + currentFrame->stackCount - Arity(op);
	if (result != nullptr)
		r = InvokeOperatorLL(args, op, result);
	else
	{
		r = InvokeOperatorLL(args, op, args);
		if (r == OVUM_SUCCESS)
			currentFrame->stackCount++;
	}
	return r;
}

int Thread::InvokeOperatorLL(Value *args, Operator op, Value *result)
{
	if (IS_NULL(args[0]))
		return ThrowNullReferenceError();

	MethodOverload *method = args[0].type->operators[(int)op];
	if (method == nullptr)
		return ThrowMissingOperatorError(op);

	return InvokeMethodOverload(method, Arity(op), args, result);
}

int Thread::InvokeApply(Value *result)
{
	int r;
	Value *args = currentFrame->evalStack + currentFrame->stackCount - 2;
	if (result != nullptr)
		r = InvokeApplyLL(args, result);
	else
	{
		r = InvokeApplyLL(args, args);
		if (r == OVUM_SUCCESS)
			currentFrame->stackCount++;
	}
	return r;
}

int Thread::InvokeApplyLL(Value *args, Value *result)
{
	// First, ensure that args[1] is a List.
	if (!Type::ValueIsType(args + 1, vm->types.List))
		return ThrowTypeError(thread_errors::WrongApplyArgsType);
	// Second, ensure that args[0] is not null.
	if (IS_NULL(args[0]))
		return ThrowNullReferenceError();

	// Then, unpack it onto the evaluation stack!
	ListInst *argsList = args[1].v.list;
	currentFrame->stackCount--;
	CopyMemoryT(currentFrame->evalStack + currentFrame->stackCount, argsList->values, argsList->length);
	currentFrame->stackCount += argsList->length;

	return InvokeLL(argsList->length, args, result, 0);
}

int Thread::InvokeApplyMethod(Method *method, Value *result)
{
	int r;
	Value *args = currentFrame->evalStack + currentFrame->stackCount - 1;
	if (result != nullptr)
		r = InvokeApplyMethodLL(method, args, result);
	else
	{
		r = InvokeApplyMethodLL(method, args, args);
		if (r == OVUM_SUCCESS)
			currentFrame->stackCount++;
	}
	return r;
}

int Thread::InvokeApplyMethodLL(Method *method, Value *args, Value *result)
{
	// First, ensure that args[0] is a List
	if (!Type::ValueIsType(args, vm->types.List))
		return ThrowTypeError(thread_errors::WrongApplyArgsType);

	assert((method->flags & MemberFlags::INSTANCE) == MemberFlags::NONE);

	ListInst *argsList = args->v.list;

	// Then, find an appropriate overload!
	MethodOverload *mo = nullptr;
	if (args->v.list->length <= UINT16_MAX)
		mo = method->ResolveOverload(argsList->length);
	if (mo == nullptr)
		return ThrowNoOverloadError(argsList->length);

	// Only now that we've found an overload do we start unpacking values and stuff.
	currentFrame->stackCount--;
	CopyMemoryT(currentFrame->evalStack + currentFrame->stackCount, argsList->values, argsList->length);
	currentFrame->stackCount += argsList->length;

	return InvokeMethodOverload(mo, argsList->length, args, result);
}


int Thread::Equals(bool *result)
{
	Value *args = currentFrame->evalStack + currentFrame->stackCount - 2;
	return EqualsLL(args, *result);
}

int Thread::EqualsLL(Value *args, bool &result)
{
	if (IS_NULL(args[0]) || IS_NULL(args[1]))
	{
		currentFrame->stackCount -= 2;
		result = args[0].type == args[1].type;
		RETURN_SUCCESS;
	}

	// Some code here is duplicated from InvokeOperatorLL, which we
	// don't call directly; we want to avoid the null check.

	MethodOverload *method = args[0].type->operators[(int)Operator::EQ];
	// Don't need to test method for nullness: every type supports ==,
	// because Object supports ==.
	assert(method != nullptr); // okay, fine, but only when debugging

	// Save the result in the first argument
	int r = InvokeMethodOverload(method, 2, args, args);
	if (r == OVUM_SUCCESS)
		result = IsTrue_(args);

	return r;
}

int Thread::Compare(int64_t *result)
{
	Value *args = currentFrame->evalStack + currentFrame->stackCount - 2;
	int r = CompareLL(args, args);
	if (r == OVUM_SUCCESS)
		*result = args[0].v.integer;
	return r;
}

int Thread::Concat(Value *result)
{
	int r;
	Value *args = currentFrame->evalStack + currentFrame->stackCount - 2;
	if (result != nullptr)
		r = ConcatLL(args, result);
	else
	{
		r = ConcatLL(args, args);
		if (r == OVUM_SUCCESS)
			currentFrame->stackCount++;
	}
	return r;
}

int Thread::ConcatLL(Value *args, Value *result)
{
	// Note: result may overlap args, so we cannot assign to it
	//       until we are absolutely 100% done.

	int status__;
	register Value *a = args;
	register Value *b = args + 1;
	if (a->type == vm->types.List || b->type == vm->types.List)
	{
		// list concatenation
		if (a->type != b->type)
			return ThrowTypeError(thread_errors::ConcatTypes);

		Value output;
		CHECKED(GetGC()->Alloc(this, vm->types.List, sizeof(ListInst), &output));

		int32_t length = a->v.list->length + b->v.list->length;
		CHECKED(vm->functions.initListInstance(this,
			output.v.list, length));

		if (length > 0)
		{
			CopyMemoryT(output.v.list->values, a->v.list->values, a->v.list->length);
			CopyMemoryT(output.v.list->values + a->v.list->length, b->v.list->values, b->v.list->length);
		}
		output.v.list->length = length;

		*result = output;
	}
	else if (a->type == vm->types.Hash || b->type == vm->types.Hash)
	{
		// hash concatenation
		if (a->type != b->type)
			return ThrowTypeError(thread_errors::ConcatTypes);

		register StackFrame *f = currentFrame;
		// Put the hash on the stack for extra GC reachability!
		register Value *hash = args + 2;
		f->stackCount++;

		CHECKED(GetGC()->Alloc(this, vm->types.Hash, sizeof(HashInst), hash));
		CHECKED(vm->functions.initHashInstance(this,
			hash->v.hash,
			max(a->v.hash->count, b->v.hash->count)));

		MethodOverload *hashSetItem = GetHashIndexerSetter();
		do
		{
			for (int32_t i = 0; i < a->v.hash->count; i++)
			{
				HashEntry *e = &a->v.hash->entries[i];
				hash[1] = hash[0]; // dup the hash
				hash[2] = e->key;
				hash[3] = e->value;
				f->stackCount += 3;
				// InvokeMethodOverload pops the three effective arguments
				CHECKED(InvokeMethodOverload(hashSetItem, 2, hash + 1, hash + 1));
			}
			a++;
		} while (a == b);

		*result = *hash;
		f->stackCount--; // Pop the hash off the stack again
	}
	else
	{
		// string concatenation
		CHECKED(StringFromValue(this, a));
		CHECKED(StringFromValue(this, b));

		String *str;
		CHECKED_MEM(str = String_Concat(this, a->v.string, b->v.string));
		SetString_(vm, result, str);
	}
	currentFrame->stackCount -= 2;
	RETURN_SUCCESS;

retStatus__:
	return status__;
}

MethodOverload *Thread::GetHashIndexerSetter()
{
	if (!hashSetItem)
	{
		Member *m = vm->types.Hash->GetMember(static_strings::_item);

		assert((m->flags & MemberFlags::KIND) == MemberFlags::PROPERTY);
		assert(((Property*)m)->setter != nullptr);

		MethodOverload *result = ((Property*)m)->setter->ResolveOverload(2);
		assert(result != nullptr);
		hashSetItem = result;
	}
	return hashSetItem;
}


// Base implementation of the various comparison methods
// This duplicates a lot of code from InvokeOperatorLL
// (Semicolon intentionally missing from the last statement)
#define COMPARE_BASE(pResult) \
	if (IS_NULL(args[0])) \
		return ThrowNullReferenceError(); \
	\
	MethodOverload *method = args[0].type->operators[(int)Operator::CMP]; \
	if (method == nullptr) \
		return ThrowTypeError(thread_errors::NotComparable); \
	\
	int r = InvokeMethodOverload(method, 2, args, (pResult)); \
	if (r != OVUM_SUCCESS) return r; \
	if ((pResult)->type != vm->types.Int) \
		return ThrowTypeError(thread_errors::CompareType)

int Thread::CompareLL(Value *args, Value *result)
{
	COMPARE_BASE(result);
	RETURN_SUCCESS;
}

int Thread::CompareLessThanLL(Value *args, bool &result)
{
	COMPARE_BASE(args);
	result = args[0].v.integer < 0;
	RETURN_SUCCESS;
}

int Thread::CompareGreaterThanLL(Value *args, bool &result)
{
	COMPARE_BASE(args);
	result = args[0].v.integer > 0;
	RETURN_SUCCESS;
}

int Thread::CompareLessEqualsLL(Value *args, bool &result)
{
	COMPARE_BASE(args);
	result = args[0].v.integer <= 0;
	RETURN_SUCCESS;
}

int Thread::CompareGreaterEqualsLL(Value *args, bool &result)
{
	COMPARE_BASE(args);
	result = args[0].v.integer >= 0;
	RETURN_SUCCESS;
}

#undef COMPARE_BASE


int Thread::LoadMember(String *member, Value *result)
{
	int r;
	Value *inst = currentFrame->evalStack + currentFrame->stackCount - 1;
	if (result)
		r = LoadMemberLL(inst, member, result);
	else
	{
		r = LoadMemberLL(inst, member, inst);
		if (r == OVUM_SUCCESS)
			currentFrame->stackCount++;
	}
	return r;
}

int Thread::LoadMemberLL(Value *instance, String *member, Value *result)
{
	if (IS_NULL(*instance))
		return ThrowNullReferenceError();

	const Member *m = instance->type->FindMember(member, currentFrame->method->declType);
	if (m == nullptr)
		return ThrowMemberNotFoundError(member);
	if ((m->flags & MemberFlags::INSTANCE) == MemberFlags::NONE)
		return ThrowTypeError(thread_errors::StaticMemberThroughInstance);

	int r = OVUM_SUCCESS;
	switch (m->flags & MemberFlags::KIND)
	{
	case MemberFlags::FIELD:
		reinterpret_cast<const Field*>(m)->ReadFieldUnchecked(instance, result);
		currentFrame->Pop(1); // Done with the instance!
		break;
	case MemberFlags::METHOD:
		{
			Value output;
			r = GetGC()->Alloc(this, vm->types.Method, sizeof(MethodInst), &output);
			if (r != OVUM_SUCCESS)
				break;

			output.v.method->instance = *instance;
			output.v.method->method = (Method*)m;
			*result = output;
			currentFrame->Pop(1); // Done with the instance!
		}
		break;
	case MemberFlags::PROPERTY:
		{
			const Property *p = (Property*)m;
			if (!p->getter)
			{
				r = ThrowTypeError(thread_errors::GettingWriteonlyProperty);
				break;
			}

			MethodOverload *mo = p->getter->ResolveOverload(0);
			if (!mo)
			{
				r = ThrowNoOverloadError(0);
				break;
			}

			// Remember: the instance is already on the stack!
			r = InvokeMethodOverload(mo, 0, instance, result);
		}
		break;
	}
	return r;
}

int Thread::StoreMember(String *member)
{
	Value *args = currentFrame->evalStack + currentFrame->stackCount - 2;
	return StoreMemberLL(args, member);
}

int Thread::StoreMemberLL(Value *instance, String *member)
{
	if (IS_NULL(*instance))
		return ThrowNullReferenceError();

	Member *m = instance->type->FindMember(member, currentFrame->method->declType);
	if (m == nullptr)
		return ThrowMemberNotFoundError(member);
	if ((m->flags & MemberFlags::INSTANCE) == MemberFlags::NONE)
		return ThrowTypeError(thread_errors::StaticMemberThroughInstance);

	int r = OVUM_SUCCESS;
	switch (m->flags & MemberFlags::KIND)
	{
	case MemberFlags::FIELD:
		reinterpret_cast<Field*>(m)->WriteFieldUnchecked(instance);
		currentFrame->Pop(2); // Done with the instance and the value!
		break;
	case MemberFlags::METHOD:
		r = ThrowTypeError(thread_errors::AssigningToMethod);
		break;
	case MemberFlags::PROPERTY:
		{
			Property *p = (Property*)m;
			if (!p->setter)
			{
				r = ThrowTypeError(thread_errors::SettingReadonlyProperty);
				break;
			}

			MethodOverload *mo = p->setter->ResolveOverload(1);
			if (!mo)
			{
				r = ThrowNoOverloadError(1);
				break;
			}

			// Remember: the instance and value are already on the stack!
			r = InvokeMethodOverload(mo, 1, instance, instance);
		}
		break;
	}

	return r;
}

// Note: argCount does NOT include the instance.
int Thread::LoadIndexer(uint32_t argCount, Value *result)
{
	int r;
	Value *args = currentFrame->evalStack + currentFrame->stackCount - argCount - 1;
	if (result)
		r = LoadIndexerLL(argCount, args, result);
	else
	{
		r = LoadIndexerLL(argCount, args, args);
		if (r == OVUM_SUCCESS)
			currentFrame->stackCount++;
	}
	return r;
}
// Note: argc DOES NOT include the instance, but args DOES.
int Thread::LoadIndexerLL(uint32_t argCount, Value *args, Value *result)
{
	if (IS_NULL(args[0]))
		return ThrowNullReferenceError();

	Member *member = args[0].type->FindMember(static_strings::_item, currentFrame->method->declType);
	if (!member)
		return ThrowTypeError(thread_errors::NoIndexerFound);

	// The indexer, if present, MUST be an instance property.
	assert((member->flags & MemberFlags::INSTANCE) == MemberFlags::INSTANCE);
	assert((member->flags & MemberFlags::PROPERTY) == MemberFlags::PROPERTY);

	if (((Property*)member)->getter == nullptr)
		return ThrowTypeError(thread_errors::GettingWriteonlyProperty);

	MethodOverload *method = ((Property*)member)->getter->ResolveOverload(argCount);
	if (!method)
		return ThrowNoOverloadError(argCount);
	return InvokeMethodOverload(method, argCount, args, result);
}

// Note: argCount DOES NOT include the instance or the value that's being stored.
int Thread::StoreIndexer(uint32_t argCount)
{
	Value *args = currentFrame->evalStack + currentFrame->stackCount - argCount - 2;
	return StoreIndexerLL(argCount, args);
}

// Note: argCount DOES NOT include the instance or the value that's being stored, but args DOES.
int Thread::StoreIndexerLL(uint32_t argCount, Value *args)
{
	if (IS_NULL(args[0]))
		return ThrowNullReferenceError();

	Member *member = args[0].type->FindMember(static_strings::_item, currentFrame->method->declType);
	if (!member)
		return ThrowTypeError(thread_errors::NoIndexerFound);

	// The indexer, if present, MUST be an instance property.
	assert((member->flags & MemberFlags::INSTANCE) == MemberFlags::INSTANCE);
	assert((member->flags & MemberFlags::PROPERTY) == MemberFlags::PROPERTY);

	if (((Property*)member)->setter == nullptr)
		return ThrowTypeError(thread_errors::SettingReadonlyProperty);

	MethodOverload *method = ((Property*)member)->setter->ResolveOverload(argCount + 1);
	if (!method)
		return ThrowNoOverloadError(argCount + 1);

	return InvokeMethodOverload(method, argCount + 1, args, args);
}

int Thread::LoadFieldRefLL(Value *inst, Field *field)
{
	if (IS_NULL(*inst))
		return ThrowNullReferenceError();
	if (!Type::ValueIsType(inst, field->declType))
		return ThrowTypeError();

	Value fieldRef;
	fieldRef.type = (Type*)~(field->offset + GCO_SIZE);
	fieldRef.v.reference = inst->v.instance + field->offset;
	Push(&fieldRef);

	RETURN_SUCCESS;
}

int Thread::LoadMemberRefLL(Value *inst, String *member)
{
	if (IS_NULL(*inst))
		return ThrowNullReferenceError();

	Member *m = inst->type->FindMember(member, currentFrame->method->declType);
	if (m == nullptr)
		return ThrowMemberNotFoundError(member);
	if ((m->flags & MemberFlags::INSTANCE) == MemberFlags::NONE)
		return ThrowTypeError(thread_errors::StaticMemberThroughInstance);
	if ((m->flags & MemberFlags::FIELD) == MemberFlags::NONE)
		return ThrowTypeError(thread_errors::MemberIsNotAField);

	Field *field = static_cast<Field*>(m);
	Value fieldRef;
	fieldRef.type = (Type*)~(field->offset + GCO_SIZE);
	fieldRef.v.reference = inst->v.instance + field->offset;
	Push(&fieldRef);

	RETURN_SUCCESS;
}

int Thread::LoadField(Field *field, Value *result)
{
	Value *inst = currentFrame->evalStack + currentFrame->stackCount - 1;

	int r;
	if (result)
	{
		r = field->ReadField(this, inst, result);
		currentFrame->stackCount--;
	}
	else
	{
		Value value;
		r = field->ReadField(this, inst, &value);
		*inst = value;
	}
	return r;
}

int Thread::StoreField(Field *field)
{
	Value *args = currentFrame->evalStack + currentFrame->stackCount - 2;

	int r = field->WriteField(this, args);
	if (r == OVUM_SUCCESS)
		currentFrame->stackCount -= 2;
	return r;
}

int Thread::LoadStaticField(Field *field, Value *result)
{
	// Note: test against field->staticValue rather than
	// field->declType->HasStaticCtorRun(), because the
	// field may be a constant field and those don't trigger
	// the static constructor.
	if (!field->staticValue)
	{
		int r = field->declType->RunStaticCtor(this);
		if (r != OVUM_SUCCESS) return r; // Something went wrong!
	}
	if (result)
	{
		field->staticValue->Read(result);
	}
	else
	{
		Value value;
		field->staticValue->Read(&value);
		Push(&value);
	}
	RETURN_SUCCESS;
}

int Thread::StoreStaticField(Field *field)
{
	// Note: test against field->staticValue rather than
	// field->declType->HasStaticCtorRun(), because the
	// field may be a constant field and those don't trigger
	// the static constructor.
	if (!field->staticValue)
	{
		int r = field->declType->RunStaticCtor(this);
		if (r != OVUM_SUCCESS) return r; // Something went wrong!
	}
	field->staticValue->Write(currentFrame->Pop());
	currentFrame->stackCount--;
	RETURN_SUCCESS;
}


int Thread::ToString(String **result)
{
	if (currentFrame->PeekType(0) != vm->types.String)
	{
		int r = InvokeMember(static_strings::toString, 0, nullptr);
		if (r != OVUM_SUCCESS)
			return r;

		if (currentFrame->PeekType(0) != vm->types.String)
			return ThrowTypeError(static_strings::errors::ToStringWrongType);
	}

	if (result != nullptr)
	{
		*result = currentFrame->PeekString(0);
		currentFrame->stackCount--;
	}
	// else, leave it on the stack!
	RETURN_SUCCESS;
}


int Thread::Throw(bool rethrow)
{
	if (!rethrow)
	{
		currentError = currentFrame->Peek(0);
		if (!(currentError.v.error->stackTrace = GetStackTrace()))
			return OVUM_ERROR_NO_MEMORY;
	}
	assert(!IS_NULL(currentError));

	return OVUM_ERROR_THROWN;
}

int Thread::ThrowError(String *message)
{
	if (message == nullptr)
		PushNull();
	else
		PushString(message);
	int r = GetGC()->Construct(this, vm->types.Error, 1, nullptr);
	if (r == OVUM_SUCCESS)
		r = Throw();
	return r;
}

int Thread::ThrowTypeError(String *message)
{
	if (message == nullptr)
		PushNull();
	else
		PushString(message);
	int r = GetGC()->Construct(this, vm->types.TypeError, 1, nullptr);
	if (r == OVUM_SUCCESS)
		r = Throw();
	return r;
}

int Thread::ThrowMemoryError(String *message)
{
	if (message == nullptr)
		PushNull();
	else
		PushString(message);
	int r = GetGC()->Construct(this, vm->types.MemoryError, 1, nullptr);
	if (r == OVUM_SUCCESS)
		r = Throw();
	return r;
}

int Thread::ThrowOverflowError(String *message)
{
	if (message == nullptr)
		PushNull();
	else
		PushString(message);
	int r = GetGC()->Construct(this, vm->types.OverflowError, 1, nullptr);
	if (r == OVUM_SUCCESS)
		r = Throw();
	return r;
}

int Thread::ThrowDivideByZeroError(String *message)
{
	if (message == nullptr)
		PushNull();
	else
		PushString(message);
	int r = GetGC()->Construct(this, vm->types.DivideByZeroError, 1, nullptr);
	if (r == OVUM_SUCCESS)
		r = Throw();
	return r;
}

int Thread::ThrowNullReferenceError(String *message)
{
	if (message == nullptr)
		PushNull();
	else
		PushString(message);
	int r = GetGC()->Construct(this, vm->types.NullReferenceError, 1, nullptr);
	if (r == OVUM_SUCCESS)
		r = Throw();
	return r;
}

int Thread::ThrowNoOverloadError(uint32_t argCount, String *message)
{
	PushInt(argCount);
	if (message == nullptr)
		PushNull();
	else
		PushString(message);
	int r = GetGC()->Construct(this, vm->types.NoOverloadError, 2, nullptr);
	if (r == OVUM_SUCCESS)
		r = Throw();
	return r;
}

int Thread::ThrowMemberNotFoundError(String *member)
{
	PushString(member);
	int r = GetGC()->Construct(this, vm->types.MemberNotFoundError, 1, nullptr);
	if (r == OVUM_SUCCESS)
		r = Throw();
	return r;
}

int Thread::ThrowMissingOperatorError(Operator op)
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
	static const char *const baseMessage = "The type does not support the specified operator. (Operator: ";

	try
	{
		StringBuffer message;
		message.Append(strlen(baseMessage), baseMessage);
		message.Append(operatorNames[(int)op].AsString());
		message.Append(')');
		String *messageStr = message.ToString(this);
		if (messageStr == nullptr)
			return OVUM_ERROR_NO_MEMORY;
		PushString(messageStr);
	}
	catch (std::exception&)
	{
		return OVUM_ERROR_NO_MEMORY;
	}

	int r = GetGC()->Construct(this, vm->types.TypeError, 1, nullptr);
	if (r == OVUM_SUCCESS)
		r = Throw();

	return r;
}

int Thread::InitCallStack()
{
	callStack = (unsigned char*)os::VirtualAlloc(
		nullptr,
		CALL_STACK_SIZE + 256,
		os::VPROT_READ_WRITE
	);
	if (callStack == nullptr)
		return OVUM_ERROR_NO_MEMORY;

	// Make sure the page following the call stack will cause an instant segfault,
	// as a very dirty way of signalling a stack overflow.
	os::VirtualProtect(callStack + CALL_STACK_SIZE, 256, os::VPROT_NO_ACCESS);

	// The call stack should never be swapped out.
	os::VirtualLock(callStack, CALL_STACK_SIZE);

	// Push a "fake" stack frame onto the stack, so that we can
	// push values onto the evaluation stack before invoking the
	// main method of the thread.
	PushFirstStackFrame();

	RETURN_SUCCESS;
}

void Thread::DisposeCallStack()
{
	if (callStack)
		os::VirtualFree(callStack);
}


void Thread::PushFirstStackFrame()
{
	register StackFrame *frame = reinterpret_cast<StackFrame*>(callStack);
	frame->stackCount = 0;
	frame->argc       = 0;
	frame->evalStack  = reinterpret_cast<Value*>((char*)frame + STACK_FRAME_SIZE);
	frame->prevInstr  = nullptr;
	frame->prevFrame  = nullptr;
	frame->method     = nullptr;

	currentFrame = frame;
}

// Note: argCount and args DO include the instance here!
void Thread::PushStackFrame(uint32_t argCount, Value *args, MethodOverload *method)
{
	assert(currentFrame->stackCount >= argCount);
	currentFrame->stackCount -= argCount; // pop the arguments (including the instance) off the current frame

	register uint32_t paramCount = method->GetEffectiveParamCount();
	register uint32_t localCount = method->locals;
	register StackFrame *newFrame = reinterpret_cast<StackFrame*>(args + paramCount);

	newFrame->stackCount = 0;
	newFrame->argc       = argCount;
	newFrame->evalStack  = newFrame->Locals() + localCount;
	newFrame->prevInstr  = ip;
	newFrame->prevFrame  = currentFrame;
	newFrame->method     = method;
	
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
		register Value *locals = newFrame->Locals();
		while (localCount--)
			(locals++)->type = nullptr;
	}

	currentFrame = newFrame;
}

int Thread::PrepareVariadicArgs(MethodFlags flags, uint32_t argCount, uint32_t paramCount, StackFrame *frame)
{
	int32_t count = argCount >= paramCount - 1 ? argCount - paramCount + 1 : 0;

	Value listValue;
	// Construct the list!
	// We cannot really make any assumptions about the List constructor,
	// so we can't call it here. Instead, we "manually" allocate a ListInst,
	// set its type to List, and initialize its fields.
	int r = GetGC()->Alloc(this, vm->types.List, sizeof(ListInst), &listValue);
	if (r != OVUM_SUCCESS) return r;

	ListInst *list = listValue.v.list;
	r = vm->functions.initListInstance(this, list, count);
	if (r != OVUM_SUCCESS) return r;
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
			for (uint32_t i = 0; i < argCount; i++)
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
			for (uint32_t i = 0; i < argCount; i++)
			{
				*valueBase = *(valueBase - 1);
				valueBase--;
			}
			*valueBase = listValue;
		}
		frame->stackCount++;
	}
	RETURN_SUCCESS;
}


String *Thread::GetStackTrace()
{
	try
	{
		StringBuffer buf(1024);

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
		uint8_t *ip = this->ip;
		while (frame && frame->method)
		{
			MethodOverload *method = frame->method;
			Method *group = method->group;

			buf.Append(2, ' ');

			// method name, which consists of:
			// fully.qualified.type
			// .
			// methodName
			// For global methods, group->name is already the fully qualified name.
			if (group->declType)
			{
				buf.Append(group->declType->fullName);
				buf.Append('.');
			}
			buf.Append(group->name);
			buf.Append('(');

			unsigned int paramCount = method->GetEffectiveParamCount();

			for (unsigned int i = 0; i < paramCount; i++)
			{
				if (i > 0)
					buf.Append(2, ", ");

				if (i == 0 && method->IsInstanceMethod())
					buf.Append(4, "this");
				else
					buf.Append(method->paramNames[i - method->InstanceOffset()]);
				buf.Append('=');

				AppendArgumentType(buf, ((Value*)frame - paramCount) + i);
			}

			buf.Append(')');
			if (method->debugSymbols)
				AppendSourceLocation(buf, method, ip);
			buf.Append('\n');

			ip = frame->prevInstr;
			frame = frame->prevFrame;
		}

		return buf.ToString(this);
	}
	catch (std::exception&)
	{
		return nullptr;
	}
}

void Thread::AppendArgumentType(StringBuffer &buf, Value *arg)
{
	Type *type = arg->type;
	if ((uintptr_t)type & 1)
	{
		buf.Append(4, "ref ");
		Value refValue;
		ReadReference(arg, &refValue);
		type = refValue.type;
	}

	if (type == nullptr)
		buf.Append(4, "null");
	else
	{
		buf.Append(type->fullName);

		if (type == vm->types.Method)
		{
			// Append some information about the instance and method group, too.
			MethodInst *method = arg->v.method;
			buf.Append(6, "(this=");
			AppendArgumentType(buf, &method->instance);
			buf.Append(2, ", ");

			Method *mgroup = method->method;
			if (mgroup->declType)
			{
				buf.Append(mgroup->declType->fullName);
				buf.Append('.');
			}
			buf.Append(mgroup->name);

			buf.Append(')');
		}
	}
}

void Thread::AppendSourceLocation(StringBuffer &buf, MethodOverload *method, uint8_t *ip)
{
	uint32_t offset = (uint32_t)(ip - method->entry);

	debug::SourceLocation *loc = method->debugSymbols->FindSymbol(offset);
	if (loc)
	{
		buf.Append(9, " at line ");
		// Build a decimal string for the line number
		{
			const uchar NumberLength = 16; // 16 digits ought to be enough for anybody

			uchar lineNumberStr[NumberLength];
			uchar *chp = lineNumberStr + NumberLength;
			int32_t length = 0;

			int32_t lineNumber = loc->lineNumber;
			do
			{
				*--chp = (uchar)'0' + lineNumber % 10;
				length++;
			} while (lineNumber /= 10);
			buf.Append(length, chp);
		}
		buf.Append(5, " in \"");
		buf.Append(loc->file->fileName);
		buf.Append('"');
	}
}

} // namespace ovum

// API functions, which are really just wrappers for the fun stuff.

OVUM_API void VM_Push(ThreadHandle thread, Value *value)
{
	thread->Push(value);
}

OVUM_API void VM_PushNull(ThreadHandle thread)
{
	thread->PushNull();
}

OVUM_API void VM_PushBool(ThreadHandle thread, bool value)
{
	thread->PushBool(value);
}
OVUM_API void VM_PushInt(ThreadHandle thread, int64_t value)
{
	thread->PushInt(value);
}
OVUM_API void VM_PushUInt(ThreadHandle thread, uint64_t value)
{
	thread->PushUInt(value);
}
OVUM_API void VM_PushReal(ThreadHandle thread, double value)
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
OVUM_API void VM_PopN(ThreadHandle thread, uint32_t n)
{
	thread->Pop(n);
}

OVUM_API void VM_Dup(ThreadHandle thread)
{
	thread->Dup();
}

OVUM_API Value *VM_Local(ThreadHandle thread, uint32_t n)
{
	return thread->Local(n);
}

OVUM_API int VM_Invoke(ThreadHandle thread, uint32_t argCount, Value *result)
{
	return thread->Invoke(argCount, result);
}
OVUM_API int VM_InvokeMember(ThreadHandle thread, String *name, uint32_t argCount, Value *result)
{
	return thread->InvokeMember(name, argCount, result);
}
OVUM_API int VM_InvokeMethod(ThreadHandle thread, MethodHandle method, uint32_t argCount, Value *result)
{
	return thread->InvokeMethod(method, argCount, result);
}
OVUM_API int VM_InvokeOperator(ThreadHandle thread, Operator op, Value *result)
{
	return thread->InvokeOperator(op, result);
}
OVUM_API int VM_Equals(ThreadHandle thread, bool *result)
{
	return thread->Equals(result);
}
OVUM_API int VM_Compare(ThreadHandle thread, int64_t *result)
{
	return thread->Compare(result);
}

OVUM_API int VM_LoadMember(ThreadHandle thread, String *member, Value *result)
{
	return thread->LoadMember(member, result);
}
OVUM_API int VM_StoreMember(ThreadHandle thread, String *member)
{
	return thread->StoreMember(member);
}

OVUM_API int VM_LoadField(ThreadHandle thread, FieldHandle field, Value *result)
{
	return thread->LoadField(field, result);
}
OVUM_API int VM_StoreField(ThreadHandle thread, FieldHandle field)
{
	return thread->StoreField(field);
}

OVUM_API int VM_LoadStaticField(ThreadHandle thread, FieldHandle field, Value *result)
{
	return thread->LoadStaticField(field, result);
}
OVUM_API int VM_StoreStaticField(ThreadHandle thread, FieldHandle field)
{
	return thread->StoreStaticField(field);
}

OVUM_API int VM_LoadIndexer(ThreadHandle thread, uint32_t argCount, Value *result)
{
	return thread->LoadIndexer(argCount, result);
}
OVUM_API int VM_StoreIndexer(ThreadHandle thread, uint32_t argCount)
{
	return thread->StoreIndexer(argCount);
}

OVUM_API int VM_ToString(ThreadHandle thread, String **result)
{
	return thread->ToString(result);
}

OVUM_API int VM_Throw(ThreadHandle thread)
{
	return thread->Throw();
}
OVUM_API int VM_ThrowError(ThreadHandle thread, String *message)
{
	return thread->ThrowError(message);
}
OVUM_API int VM_ThrowTypeError(ThreadHandle thread, String *message)
{
	return thread->ThrowTypeError(message);
}
OVUM_API int VM_ThrowMemoryError(ThreadHandle thread, String *message)
{
	return thread->ThrowMemoryError(message);
}
OVUM_API int VM_ThrowOverflowError(ThreadHandle thread, String *message)
{
	return thread->ThrowOverflowError(message);
}
OVUM_API int VM_ThrowDivideByZeroError(ThreadHandle thread, String *message)
{
	return thread->ThrowDivideByZeroError(message);
}
OVUM_API int VM_ThrowNullReferenceError(ThreadHandle thread, String *message)
{
	return thread->ThrowNullReferenceError(message);
}
OVUM_API int VM_ThrowErrorOfType(ThreadHandle thread, TypeHandle type, int argc)
{
	int r = thread->GetGC()->Construct(thread, type, argc, nullptr);
	if (r == OVUM_SUCCESS)
		r = thread->Throw(false);
	return r;
}

OVUM_API void VM_EnterUnmanagedRegion(ThreadHandle thread)
{
	thread->EnterUnmanagedRegion();
}
OVUM_API void VM_LeaveUnmanagedRegion(ThreadHandle thread)
{
	thread->LeaveUnmanagedRegion();
}
OVUM_API bool VM_IsInUnmanagedRegion(ThreadHandle thread)
{
	return thread->IsInUnmanagedRegion();
}

OVUM_API void VM_Sleep(ThreadHandle thread, unsigned int milliseconds)
{
	thread->EnterUnmanagedRegion();

	ovum::os::Sleep(milliseconds);

	thread->LeaveUnmanagedRegion();
}

OVUM_API String *VM_GetStackTrace(ThreadHandle thread)
{
	return thread->GetStackTrace();
}

OVUM_API int VM_GetStackDepth(ThreadHandle thread)
{
	int depth = 0;

	const ovum::StackFrame *frame = thread->GetCurrentFrame();
	while (frame && frame->method)
	{
		depth++;
		frame = frame->prevFrame;
	}

	return depth;
}

OVUM_API OverloadHandle VM_GetCurrentOverload(ThreadHandle thread)
{
	const ovum::StackFrame *frame = thread->GetCurrentFrame();
	return frame ? frame->method : nullptr;
}

const ovum::StackFrame *VM_FindStackFrame(ThreadHandle thread, int stackFrame)
{
	if (stackFrame >= 0)
	{
		const ovum::StackFrame *frame = thread->GetCurrentFrame();
		while (frame && frame->method)
		{
			if (stackFrame-- == 0)
				return frame;
			frame = frame->prevFrame;
		}
	}
	return nullptr;
}

OVUM_API int VM_GetEvalStackHeight(ThreadHandle thread, int stackFrame, const Value **slots)
{
	const ovum::StackFrame *frame = VM_FindStackFrame(thread, stackFrame);
	if (frame)
	{
		if (slots)
			*slots = frame->evalStack;
		return (int)frame->stackCount;
	}
	return -1;
}
OVUM_API int VM_GetLocalCount(ThreadHandle thread, int stackFrame, const Value **slots)
{
	const ovum::StackFrame *frame = VM_FindStackFrame(thread, stackFrame);
	if (frame)
	{
		if (slots)
			*slots = frame->Locals();
		return (int)frame->method->locals;
	}
	return -1;
}
OVUM_API int VM_GetMethodArgCount(ThreadHandle thread, int stackFrame, const Value **slots)
{
	const ovum::StackFrame *frame = VM_FindStackFrame(thread, stackFrame);
	if (frame)
	{
		int argCount = (int)frame->method->GetEffectiveParamCount();
		if (slots)
			*slots = reinterpret_cast<const Value*>(frame) - argCount;
		return argCount;
	}
	return -1;
}
OVUM_API OverloadHandle VM_GetExecutingOverload(ThreadHandle thread, int stackFrame)
{
	const ovum::StackFrame *frame = VM_FindStackFrame(thread, stackFrame);
	if (frame)
		return frame->method;
	return nullptr;
}
OVUM_API const void *VM_GetInstructionPointer(ThreadHandle thread, int stackFrame)
{
	if (stackFrame >= 0)
	{
		const ovum::StackFrame *frame = thread->GetCurrentFrame();
		if (frame)
		{
			const void *ip = thread->GetInstructionPointer();
			while (frame && frame->method)
			{
				if (stackFrame-- == 0)
					return ip;
				ip = frame->prevInstr;
				frame = frame->prevFrame;
			}
		}
	}
	return nullptr;
}
OVUM_API bool VM_GetStackFrameInfo(ThreadHandle thread, int stackFrame, StackFrameInfo *dest)
{
	if (stackFrame >= 0)
	{
		const ovum::StackFrame *frame = thread->GetCurrentFrame();
		if (frame)
		{
			const void *ip = thread->GetInstructionPointer();
			while (frame && frame->method)
			{
				if (stackFrame-- == 0)
				{
					dest->stackHeight = frame->stackCount;
					dest->stackPointer = frame->evalStack;
					dest->localCount = frame->method->locals;
					dest->localPointer = frame->Locals();
					dest->argumentCount = frame->method->GetEffectiveParamCount();
					dest->argumentPointer = reinterpret_cast<const Value*>(frame) - dest->argumentCount;
					dest->overload = frame->method;
					dest->ip = ip;
					return true;
				}
				ip = frame->prevInstr;
				frame = frame->prevFrame;
			}
		}
	}
	return false;
}