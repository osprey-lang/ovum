﻿#include "vm.h"
#include "thread.h"
#include "refsignature.h"
#include "methodinitexception.h"
#include "../gc/gc.h"
#include "../gc/staticref.h"
#include "../object/type.h"
#include "../object/property.h"
#include "../object/method.h"
#include "../object/standardtypeinfo.h"
#include "../module/module.h"
#include "../module/modulepool.h"
#include "../util/pathname.h"
#include "../res/staticstrings.h"
#include <fcntl.h>
#include <io.h>
#include <cstdio>

#ifdef _MSC_VER
// Microsoft's C++ implementation uses %s to mean wchar_t* in wide functions,
// and requires the non-portable %S for char*s. Sigh.
#define CSTR  L"%S"
#else
#define CSTR  L"%s"
#endif

namespace ovum
{

TlsEntry<VM> VM::vmKey;

VM::VM(VMStartParams &params) :
	verbose(params.verbose),
	argCount(params.argc),
	argValues(),
	types(),
	functions(),
	startupPath(),
	startupPathLib(),
	modulePath(),
	mainThread(),
	gc(),
	modules(),
	refSignatures()
{ }

VM::~VM()
{
	// We have to unload the GC first, because the GC relies on data in
	// modules to perform cleanup, such as examining managed types and
	// calling finalizers in native types. If we clean up modules first,
	// then the GC will be very unhappy.
	gc.reset();
}

int VM::Run()
{
	int r;

	Method *main = startupModule->GetMainMethod();
	if (main == nullptr)
	{
		fwprintf(stderr, L"Startup error: Startup module does not define a main method.\n");
		r = OVUM_ERROR_NO_MAIN_METHOD;
	}
	else
	{
		ovlocals_t argc;
		MethodOverload *mo;
		r = GetMainMethodOverload(main, argc, mo);
		if (r != OVUM_SUCCESS) return r;

		if (verbose)
			wprintf(L"<<< Begin program output >>>\n");

		Value returnValue;
		r = mainThread->Start(argc, mo, returnValue);

		if (r == OVUM_SUCCESS)
		{
			if (returnValue.type == types.Int ||
				returnValue.type == types.UInt)
				r = (int)returnValue.v.integer;
			else if (returnValue.type == types.Real)
				r = (int)returnValue.v.real;
		}
		else if (r == OVUM_ERROR_THROWN)
			PrintUnhandledError(mainThread.get());

		if (verbose)
			wprintf(L"<<< End program output >>>\n");
	}

	return r;
}

int VM::New(VMStartParams &params, Box<VM> &result)
{
	int status__;
	{
		if (!vmKey.IsValid())
			CHECKED_MEM(vmKey.Alloc());

		Box<VM> vm(new(std::nothrow) VM(params));
		CHECKED_MEM(vm.get());

		// Most things rely on static strings, so initialize them first.
		CHECKED_MEM(vm->strings = StaticStrings::New());

		CHECKED_MEM(vm->mainThread = Thread::New(vm.get()));
		CHECKED_MEM(vm->gc = GC::New(vm.get()));
		CHECKED_MEM(vm->standardTypeCollection = StandardTypeCollection::New(vm.get()));
		CHECKED_MEM(vm->modules = ModulePool::New(10));
		CHECKED_MEM(vm->refSignatures = Box<RefSignaturePool>(new(std::nothrow) RefSignaturePool()));

		CHECKED(vm->LoadModules(params));
		CHECKED(vm->InitArgs(params.argc, params.argv));

		result = std::move(vm);
	}

	status__ = OVUM_SUCCESS;
retStatus__:
	return status__;
}

int VM::LoadModules(VMStartParams &params)
{
	try
	{
		// Set up some stuff first
		this->startupPath = Box<PathName>(new PathName(params.startupFile));
		this->startupPath->RemoveFileName();

		this->startupPathLib = Box<PathName>(new PathName(*this->startupPath));
		this->startupPathLib->Join(OVUM_PATH("lib"));

		this->modulePath = Box<PathName>(new PathName(params.modulePath));

		// And now we can start opening modules! Hurrah!

		PathName startupFile(params.startupFile, std::nothrow);
		if (!startupFile.IsValid())
			return OVUM_ERROR_NO_MEMORY;

		PartiallyOpenedModulesList partiallyOpenedModules;
		this->startupModule = Module::Open(
			this,
			startupFile,
			nullptr,
			partiallyOpenedModules
		);
	}
	catch (ModuleLoadException &e)
	{
		const PathName &fileName = e.GetFileName();
		if (fileName.GetLength() > 0)
			fwprintf(stderr, L"Error loading module '" OVUM_PATHNWF L"': " CSTR L"\n", fileName.GetDataPointer(), e.what());
		else
			fwprintf(stderr, L"Error loading module: " CSTR L"\n", e.what());
		return OVUM_ERROR_MODULE_LOAD;
	}
	catch (std::bad_alloc&)
	{
		return OVUM_ERROR_NO_MEMORY;
	}

	size_t stdTypeCount = standardTypeCollection->GetCount();
	for (size_t i = 0; i < stdTypeCount; i++)
	{
		StandardTypeInfo type;
		standardTypeCollection->GetByIndex(i, type);
		if (this->types.*(type.member) == nullptr)	
		{
			PrintInternal(stderr, L"Startup error: standard type not loaded: %ls\n", type.name);
			return OVUM_ERROR_MODULE_LOAD;
		}
	}

	RETURN_SUCCESS;
}

int VM::InitArgs(size_t argCount, const wchar_t *args[])
{
	// Convert command-line arguments to String*s.
	Box<Value*[]> argValues(new(std::nothrow) Value*[argCount]);
	if (!argValues)
		return OVUM_ERROR_NO_MEMORY;

	for (size_t i = 0; i < argCount; i++)
	{
		String *argString = String_FromWString(mainThread.get(), args[i]);
		if (!argString)
			return OVUM_ERROR_NO_MEMORY;

		Value argValue;
		argValue.type = types.String;
		argValue.v.string = argString;

		StaticRef *ref = gc->AddStaticReference(nullptr, &argValue);
		if (!ref)
			return OVUM_ERROR_NO_MEMORY;

		argValues[i] = ref->GetValuePointer();

		if (this->verbose)
		{
			wprintf(L"Argument %d: ", i);
			PrintLn(argValue.v.string);
		}
	}

	this->argValues = std::move(argValues);
	RETURN_SUCCESS;
}

int VM::GetMainMethodOverload(Method *method, ovlocals_t &argc, MethodOverload *&overload)
{
	overload = method->ResolveOverload(argc = 1);
	if (overload)
	{
		// If there is a one-argument overload, try to create an aves.List
		// and put the argument values in it.
		GCObject *listGco;
		int r = gc->Alloc(mainThread.get(), types.List, types.List->size, &listGco);
		if (r != OVUM_SUCCESS)
			return r;

		ListInst *argsList = reinterpret_cast<ListInst*>(listGco->InstanceBase());
		r = functions.initListInstance(mainThread.get(), argsList, this->argCount);
		if (r != OVUM_SUCCESS)
			return r;

		OVUM_ASSERT(argsList->capacity >= this->argCount);

		GetArgValues(this->argCount, argsList->values);
		argsList->length = this->argCount;

		Value argsValue;
		argsValue.type = types.List;
		argsValue.v.instance = reinterpret_cast<uint8_t*>(argsList);
		mainThread->Push(&argsValue);
	}
	else
	{
		overload = method->ResolveOverload(argc = 0);
	}

	if (overload == nullptr || overload->IsInstanceMethod())
	{
		fwprintf(stderr, L"Startup error: Main method must take 1 or 0 arguments, and cannot be an instance method.\n");
		return OVUM_ERROR_NO_MAIN_METHOD;
	}

	RETURN_SUCCESS;
}

void VM::PrintInternal(FILE *f, const wchar_t *format, String *str)
{
#if OVUM_WCHAR_SIZE == 2
	// UTF-16, or at least USC-2.
	fwprintf(f, format, &str->firstChar);
#elif OVUM_WCHAR_SIZE == 4
	// UTF-32, use our own conversion function to convert
	const int length = String_ToWString(nullptr, str);

	if (length <= 128)
	{
		wchar_t buffer[128];
		String_ToWString(buffer, str);
		fwprintf(f, format, buffer);
	}
	else
	{
		Box<wchar_t[]> buffer(new wchar_t[length]);
		String_ToWString(buffer.get(), str);
		fwprintf(f, format, buffer.get());
	}
#else
#error Not supported
#endif
}

void VM::Print(String *str)
{
	PrintInternal(stdout, L"%ls", str);
}
void VM::Printf(const wchar_t *format, String *str)
{
	PrintInternal(stdout, format, str);
}
void VM::PrintLn(String *str)
{
	PrintInternal(stdout, L"%ls\n", str);
}

void VM::PrintErr(String *str)
{
	PrintInternal(stderr, L"%ls", str);
}
void VM::PrintfErr(const wchar_t *format, String *str)
{
	PrintInternal(stderr, format, str);
}
void VM::PrintErrLn(String *str)
{
	PrintInternal(stderr, L"%ls\n", str);
}

void VM::PrintUnhandledError(Thread *const thread)
{
	Value &error = thread->currentError;
	PrintInternal(stderr, L"Unhandled error: %ls: ", error.type->fullName);

	String *message = nullptr;
	// If the member exists and is a readable instance property,
	// we can actually try to invoke the 'message' getter!
	Member *msgMember = error.type->FindMember(strings->members.message, nullptr);
	if (msgMember && !msgMember->IsStatic() && msgMember->IsProperty())
	{
		Property *msgProp = static_cast<Property*>(msgMember);
		if (msgProp->getter != nullptr)
		{
			thread->Push(&error);

			Value result;
			int r = thread->InvokeMethod(msgProp->getter, 0, &result);
			if (r == OVUM_SUCCESS && result.type == types.String)
				message = result.v.string;
		}
	}
	if (message == nullptr)
		message = error.v.error->message;
	if (message != nullptr)
		PrintErrLn(message);

	if (error.v.error->stackTrace)
		PrintErrLn(error.v.error->stackTrace);
}
void VM::PrintMethodInitException(MethodInitException &e)
{
	FILE *err = stderr;

	fwprintf(err, L"An error occurred while initializing the method '");

	MethodOverload *method = e.GetMethod();
	if (method->declType)
		PrintInternal(err, L"%ls.", method->declType->fullName);
	PrintErr(method->group->name);

	PrintInternal(err, L"' from module %ls: ", method->group->declModule->GetName());
	fwprintf(err, CSTR L"\n", e.what());

	switch (e.GetFailureKind())
	{
	case MethodInitException::INCONSISTENT_STACK:
	case MethodInitException::INVALID_BRANCH_OFFSET:
	case MethodInitException::INSUFFICIENT_STACK_HEIGHT:
	case MethodInitException::STACK_HAS_REFS:
		fwprintf(err, L"Instruction index: %d\n", e.GetInstructionIndex());
		break;
	case MethodInitException::INACCESSIBLE_MEMBER:
	case MethodInitException::FIELD_STATIC_MISMATCH:
		fwprintf(err, L"Member: ");
		{
			Member *member = e.GetMember();
			if (member->declType)
				PrintInternal(err, L"%ls.", member->declType->fullName);
			PrintInternal(err, L"%ls\n", member->name);
		}
		break;
	case MethodInitException::UNRESOLVED_TOKEN:
		fwprintf(err, L"Token: %08X\n", e.GetToken());
		break;
	case MethodInitException::NO_MATCHING_OVERLOAD:
		fwprintf(err, L"Method: '");
		{
			Method *method = e.GetMethodGroup();
			if (method->declType)
				PrintInternal(err, L"%ls.", method->declType->fullName);
			PrintErr(method->name);
			PrintInternal(err, L"' from module %ls\n", method->declModule->GetName());
		}
		fwprintf(err, L"Argument count: %u\n", e.GetArgumentCount());
		break;
	case MethodInitException::INACCESSIBLE_TYPE:
	case MethodInitException::TYPE_NOT_CONSTRUCTIBLE:
		PrintInternal(err, L"Type: '%ls' ", e.GetType()->fullName);
		PrintInternal(err, L"from module %ls\n", e.GetType()->module->GetName());
		break;
	}
}

size_t VM::GetArgs(size_t destLength, String *dest[])
{
	size_t maxIndex = min(destLength, argCount);

	for (size_t i = 0; i < maxIndex; i++)
		dest[i] = argValues[i]->v.string;

	return maxIndex;
}
size_t VM::GetArgValues(size_t destLength, Value dest[])
{
	size_t maxIndex = min(destLength, argCount);

	for (size_t i = 0; i < maxIndex; i++)
		dest[i] = *argValues[i];

	return maxIndex;
}

} // namespace ovum

OVUM_API int VM_Start(VMStartParams *params)
{
	using namespace ovum;

	_setmode(_fileno(stdout), _O_U8TEXT);
	_setmode(_fileno(stderr), _O_U8TEXT);
	_setmode(_fileno(stdin),  _O_U8TEXT);

	if (params->verbose)
	{
		wprintf(L"Module path:    %ls\n", params->modulePath);
		wprintf(L"Startup file:   %ls\n", params->startupFile);
		wprintf(L"Argument count: %d\n",  params->argc);
	}

	Box<VM> vm;
	int r = VM::New(*params, vm);
	if (r == OVUM_SUCCESS)
	{
		r = vm->Run();
	}

#if EXIT_SUCCESS == 0
	// OVUM_SUCCESS == EXIT_SUCCESS, which also means
	// Ovum's error codes are != EXIT_SUCCESS, so let's
	// just pass the result on to the system.
	return r;
#else
	// Unlikely case - let's fall back to standard C
	// exit codes.
	return r == OVUM_SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE;
#endif
}

OVUM_API void VM_Print(String *str)
{
	ovum::VM::Print(str);
}
OVUM_API void VM_PrintLn(String *str)
{
	ovum::VM::PrintLn(str);
}

OVUM_API void VM_PrintErr(String *str)
{
	ovum::VM::PrintErr(str);
}
OVUM_API void VM_PrintErrLn(String *str)
{
	ovum::VM::PrintErrLn(str);
}

OVUM_API size_t VM_GetArgCount(ThreadHandle thread)
{
	return thread->GetVM()->GetArgCount();
}
OVUM_API size_t VM_GetArgs(ThreadHandle thread, size_t destLength, String *dest[])
{
	return thread->GetVM()->GetArgs(destLength, dest);
}
OVUM_API size_t VM_GetArgValues(ThreadHandle thread, size_t destLength, Value dest[])
{
	return thread->GetVM()->GetArgValues(destLength, dest);
}
