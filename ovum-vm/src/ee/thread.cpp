#include "thread.h"
#include "vm.h"
#include "stacktraceformatter.h"
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
#include "../res/staticstrings.h"
#include "../config/defaults.h"

namespace ovum
{

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

Thread *Thread::GetCurrent()
{
	return threadKey.Get();
}

Thread::Thread(VM *owner, int &status) :
	ip(nullptr),
	currentFrame(nullptr),
	pendingRequest(ThreadRequest::NONE),
	state(ThreadState::CREATED),
	flags(ThreadFlags::NONE),
	callStack(nullptr),
	vm(owner),
	strings(owner->GetStrings()),
	currentError(NULL_VALUE),
	gcCycleSection(4000)
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

int Thread::Start(ovlocals_t argCount, MethodOverload *mo, Value &result)
{
	OVUM_ASSERT(mo != nullptr);
	OVUM_ASSERT(this->state == ThreadState::CREATED);
	OVUM_ASSERT(!mo->IsInstanceMethod());

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
	OVUM_ASSERT(pendingRequest == ThreadRequest::SUSPEND_FOR_GC);

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


int Thread::Invoke(ovlocals_t argCount, Value *result)
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
int Thread::InvokeLL(ovlocals_t argCount, Value *value, Value *result, uint32_t refSignature)
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
		Member *member = value->type->FindMember(strings->members.call_, currentFrame->method);
		if (member && member->IsMethod())
			mo = static_cast<Method*>(member)->ResolveOverload(argCount);
		else
			return ThrowTypeError(strings->error.MemberNotInvokable);
	}

	if (!mo)
		return ThrowNoOverloadError(argCount);
	
	if (refSignature != mo->refSignature &&
		mo->VerifyRefSignature(refSignature, argCount) != -1)
		return ThrowNoOverloadError(argCount, strings->error.IncorrectRefness);
	// We've now found a method overload to invoke, omg!
	// So let's just pass it into InvokeMethodOverload.
	return InvokeMethodOverload(mo, argCount, value, result);
}

int Thread::InvokeMethod(Method *method, ovlocals_t argCount, Value *result)
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

int Thread::InvokeMember(String *name, ovlocals_t argCount, Value *result)
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

int Thread::InvokeMemberLL(String *name, ovlocals_t argCount, Value *value, Value *result, uint32_t refSignature)
{
	if (IS_NULL(*value))
		return ThrowNullReferenceError();

	Member *member;
	if (member = value->type->FindMember(name, currentFrame->method))
	{
		if (member->IsStatic())
			return ThrowTypeError(strings->error.CannotAccessStaticMemberThroughInstance);

		switch (member->flags & MemberFlags::KIND_MASK)
		{
		case MemberFlags::FIELD:
			((Field*)member)->ReadFieldUnchecked(value, value);
			return InvokeLL(argCount, value, result, refSignature);
		case MemberFlags::PROPERTY:
			{
				if (((Property*)member)->getter == nullptr)
					return ThrowTypeError(strings->error.CannotGetWriteOnlyProperty);

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
		default: // method
			{
				MethodOverload *mo = ((Method*)member)->ResolveOverload(argCount);
				if (!mo)
					return ThrowNoOverloadError(argCount);
				if (refSignature != mo->refSignature &&
					mo->VerifyRefSignature(refSignature, argCount) != -1)
					return ThrowNoOverloadError(argCount, strings->error.IncorrectRefness);
				return InvokeMethodOverload(mo, argCount, value, result);
			}
		}
	}

	return ThrowMemberNotFoundError(name);
}

int Thread::InvokeMethodOverload(MethodOverload *mo, ovlocals_t argCount,
                                 Value *args, Value *result)
{
	int r;
	if (mo->IsVariadic())
	{
		r = PrepareVariadicArgs(argCount, mo->paramCount, currentFrame);
		if (r != OVUM_SUCCESS) return r;
		argCount = mo->paramCount;
	}

	argCount += mo->instanceCount;

	// And now we can push the new stack frame!
	// Note: this updates currentFrame
	PushStackFrame(argCount, args, mo);

	if (mo->IsNative())
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
#if OVUM_DEBUG
		else
		{
			// It should not be possible to return from a method with
			// anything other than exactly one value on the stack!
			OVUM_ASSERT(currentFrame->stackCount == 1);
		}
#endif
	}

	// restore previous stack frame
	restore:
	StackFrame *frame = currentFrame;
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
	ovlocals_t arity = (ovlocals_t) Arity(op);
	Value *args = currentFrame->evalStack + currentFrame->stackCount - arity;
	if (result != nullptr)
		r = InvokeOperatorLL(args, op, arity, result);
	else
	{
		r = InvokeOperatorLL(args, op, arity, args);
		if (r == OVUM_SUCCESS)
			currentFrame->stackCount++;
	}
	return r;
}

int Thread::InvokeOperatorLL(Value *args, Operator op, ovlocals_t arity, Value *result)
{
	if (IS_NULL(args[0]))
		return ThrowNullReferenceError();

	MethodOverload *method = args[0].type->operators[(int)op];
	if (method == nullptr)
		return ThrowMissingOperatorError(op);

	return InvokeMethodOverload(method, arity, args, result);
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
		return ThrowTypeError(strings->error.WrongApplyArgumentsType);
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
		return ThrowTypeError(strings->error.WrongApplyArgumentsType);

	OVUM_ASSERT(method->IsStatic());

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
	OVUM_ASSERT(method != nullptr); // okay, fine, but only when debugging

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
	Value *a = args;
	Value *b = args + 1;

	// string concatenation
	CHECKED(StringFromValue(this, a));
	CHECKED(StringFromValue(this, b));

	String *str;
	CHECKED_MEM(str = String_Concat(this, a->v.string, b->v.string));
	SetString_(vm, result, str);

	currentFrame->stackCount -= 2;
	RETURN_SUCCESS;

retStatus__:
	return status__;
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
		return ThrowTypeError(strings->error.ValueNotComparable); \
	\
	int r = InvokeMethodOverload(method, 2, args, (pResult)); \
	if (r != OVUM_SUCCESS) return r; \
	if ((pResult)->type != vm->types.Int) \
		return ThrowTypeError(strings->error.CompareOperatorWrongReturnType)

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

	const Member *m = instance->type->FindMember(member, currentFrame->method);
	if (m == nullptr)
		return ThrowMemberNotFoundError(member);
	if (m->IsStatic())
		return ThrowTypeError(strings->error.CannotAccessStaticMemberThroughInstance);

	int r = OVUM_SUCCESS;
	switch (m->flags & MemberFlags::KIND_MASK)
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
				r = ThrowTypeError(strings->error.CannotGetWriteOnlyProperty);
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

	Member *m = instance->type->FindMember(member, currentFrame->method);
	if (m == nullptr)
		return ThrowMemberNotFoundError(member);
	if (m->IsStatic())
		return ThrowTypeError(strings->error.CannotAccessStaticMemberThroughInstance);

	int r = OVUM_SUCCESS;
	switch (m->flags & MemberFlags::KIND_MASK)
	{
	case MemberFlags::FIELD:
		static_cast<Field*>(m)->WriteFieldUnchecked(instance);
		currentFrame->Pop(2); // Done with the instance and the value!
		break;
	case MemberFlags::METHOD:
		r = ThrowTypeError(strings->error.CannotAssignToMethod);
		break;
	case MemberFlags::PROPERTY:
		{
			Property *p = (Property*)m;
			if (!p->setter)
			{
				r = ThrowTypeError(strings->error.CannotSetReadOnlyProperty);
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
int Thread::LoadIndexer(ovlocals_t argCount, Value *result)
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
int Thread::LoadIndexerLL(ovlocals_t argCount, Value *args, Value *result)
{
	if (IS_NULL(args[0]))
		return ThrowNullReferenceError();

	Member *member = args[0].type->FindMember(strings->members.item_, currentFrame->method);
	if (!member)
		return ThrowTypeError(strings->error.IndexerNotFound);

	// The indexer, if present, MUST be an instance property.
	OVUM_ASSERT(!member->IsStatic() && member->IsProperty());

	if (((Property*)member)->getter == nullptr)
		return ThrowTypeError(strings->error.CannotGetWriteOnlyProperty);

	MethodOverload *method = ((Property*)member)->getter->ResolveOverload(argCount);
	if (!method)
		return ThrowNoOverloadError(argCount);
	return InvokeMethodOverload(method, argCount, args, result);
}

// Note: argCount DOES NOT include the instance or the value that's being stored.
int Thread::StoreIndexer(ovlocals_t argCount)
{
	Value *args = currentFrame->evalStack + currentFrame->stackCount - argCount - 2;
	return StoreIndexerLL(argCount, args);
}

// Note: argCount DOES NOT include the instance or the value that's being stored, but args DOES.
int Thread::StoreIndexerLL(ovlocals_t argCount, Value *args)
{
	if (IS_NULL(args[0]))
		return ThrowNullReferenceError();

	Member *member = args[0].type->FindMember(strings->members.item_, currentFrame->method);
	if (!member)
		return ThrowTypeError(strings->error.IndexerNotFound);

	// The indexer, if present, MUST be an instance property.
	OVUM_ASSERT(!member->IsStatic() && member->IsProperty());

	if (((Property*)member)->setter == nullptr)
		return ThrowTypeError(strings->error.CannotSetReadOnlyProperty);

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

	Member *m = inst->type->FindMember(member, currentFrame->method);
	if (m == nullptr)
		return ThrowMemberNotFoundError(member);
	if (m->IsStatic())
		return ThrowTypeError(strings->error.CannotAccessStaticMemberThroughInstance);
	if (!m->IsField())
		return ThrowTypeError(strings->error.MemberIsNotAField);

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
		int r = InvokeMember(strings->members.toString, 0, nullptr);
		if (r != OVUM_SUCCESS)
			return r;

		if (currentFrame->PeekType(0) != vm->types.String)
			return ThrowTypeConversionError(strings->error.ToStringWrongReturnType);
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
	OVUM_ASSERT(!IS_NULL(currentError));

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

int Thread::ThrowTypeConversionError(String *message)
{
	if (message == nullptr)
		PushNull();
	else
		PushString(message);
	int r = GetGC()->Construct(this, vm->types.TypeConversionError, 1, nullptr);
	if (r == OVUM_SUCCESS)
		r = Throw();
	return r;
}

int Thread::ThrowNoOverloadError(ovlocals_t argCount, String *message)
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
	auto &operators = this->strings->operators;
	String *operatorNames[] = {
		operators.add,        // Operator::ADD
		operators.subtract,   // Operator::SUB
		operators.or,         // Operator::OR
		operators.xor,        // Operator::XOR
		operators.multiply,   // Operator::MUL
		operators.divide,     // Operator::DIV
		operators.modulo,     // Operator::MOD
		operators.and,        // Operator::AND
		operators.power,      // Operator::POW
		operators.shiftLeft,  // Operator::SHL
		operators.shiftRight, // Operator::SHR
		operators.hash,       // Operator::HASHOP
		operators.dollar,     // Operator::DOLLAR
		operators.plus,       // Operator::PLUS
		operators.negate,     // Operator::NEG
		operators.not,        // Operator::NOT
		operators.or,         // Operator::EQ
		operators.compare,    // Operator::CMP
	};
	static const char *const baseMessage = "The type does not support the specified operator. (Operator: ";

	try
	{
		StringBuffer message;
		message.Append(strlen(baseMessage), baseMessage);
		message.Append(operatorNames[(int)op]);
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
		config::Defaults::CALL_STACK_SIZE + 256,
		os::VPROT_READ_WRITE
	);
	if (callStack == nullptr)
		return OVUM_ERROR_NO_MEMORY;

	// Make sure the page following the call stack will cause an instant segfault,
	// as a very dirty way of signalling a stack overflow.
	os::VirtualProtect(callStack + config::Defaults::CALL_STACK_SIZE, 256, os::VPROT_NO_ACCESS);

	// The call stack should never be swapped out.
	os::VirtualLock(callStack, config::Defaults::CALL_STACK_SIZE);

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
	StackFrame *frame = reinterpret_cast<StackFrame*>(callStack);
	frame->stackCount = 0;
	frame->argc       = 0;
	frame->evalStack  = reinterpret_cast<Value*>((char*)frame + STACK_FRAME_SIZE);
	frame->prevInstr  = nullptr;
	frame->prevFrame  = nullptr;
	frame->method     = nullptr;

	currentFrame = frame;
}

// Note: argCount and args DO include the instance here!
void Thread::PushStackFrame(ovlocals_t argCount, Value *args, MethodOverload *method)
{
	OVUM_ASSERT(currentFrame->stackCount >= argCount);
	currentFrame->stackCount -= argCount; // pop the arguments (including the instance) off the current frame

	uint32_t paramCount = method->GetEffectiveParamCount();
	uint32_t localCount = method->locals;
	StackFrame *newFrame = reinterpret_cast<StackFrame*>(args + paramCount);

	newFrame->stackCount = 0;
	newFrame->argc       = argCount;
	newFrame->evalStack  = newFrame->Locals() + localCount;
	newFrame->prevInstr  = ip;
	newFrame->prevFrame  = currentFrame;
	newFrame->method     = method;
	
	// initialize missing arguments to null
	if (argCount != paramCount)
	{
		Value *missing = args + argCount;
		while ((void*)missing != (void*)newFrame)
			(missing++)->type = nullptr;
	}

	// Also initialize all locals to null
	if (localCount)
	{
		Value *locals = newFrame->Locals();
		while (localCount--)
			(locals++)->type = nullptr;
	}

	currentFrame = newFrame;
}

int Thread::PrepareVariadicArgs(ovlocals_t argCount, ovlocals_t paramCount, StackFrame *frame)
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
		// Pointer to the first list item
		Value *valueBase = frame->evalStack + frame->stackCount - count;
		// Copy the values to the list
		CopyMemoryT(list->values, valueBase, count);
		// And update the stack slot!
		*valueBase = listValue;
		// Pop all but the last item
		frame->stackCount -= count;
		frame->stackCount++;
	}
	else // Let's push an empty list!
	{
		// Push list value onto the end
		frame->evalStack[frame->stackCount] = listValue;
		frame->stackCount++;
	}
	RETURN_SUCCESS;
}


String *Thread::GetStackTrace()
{
	return StackTraceFormatter::GetStackTrace(this);
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

OVUM_API int VM_Invoke(ThreadHandle thread, ovlocals_t argCount, Value *result)
{
	return thread->Invoke(argCount, result);
}
OVUM_API int VM_InvokeMember(ThreadHandle thread, String *name, ovlocals_t argCount, Value *result)
{
	return thread->InvokeMember(name, argCount, result);
}
OVUM_API int VM_InvokeMethod(ThreadHandle thread, MethodHandle method, ovlocals_t argCount, Value *result)
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

OVUM_API int VM_LoadIndexer(ThreadHandle thread, ovlocals_t argCount, Value *result)
{
	return thread->LoadIndexer(argCount, result);
}
OVUM_API int VM_StoreIndexer(ThreadHandle thread, ovlocals_t argCount)
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
OVUM_API int VM_ThrowTypeConversionError(ThreadHandle thread, String *message)
{
	return thread->ThrowTypeConversionError(message);
}
OVUM_API int VM_ThrowErrorOfType(ThreadHandle thread, TypeHandle type, ovlocals_t argc)
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
