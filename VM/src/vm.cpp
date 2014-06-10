#include "ov_vm.internal.h"
#include "ov_module.internal.h"
#include "pathname.internal.h"
#include <fcntl.h>
#include <io.h>
#include <cstdio>
#include <memory>

#ifdef _MSC_VER
// Microsoft's C++ implementation uses %s to mean wchar_t* in wide functions,
// and requires the non-portable %S for char*s. Sigh.
#define CSTR  L"%S"
#else
#define CSTR  L"%s"
#endif

VM *VM::vm = nullptr;
FILE *VM::stdOut = nullptr;
FILE *VM::stdErr = nullptr;

OVUM_API int VM_Start(VMStartParams *params)
{
	int r;
	// VM::Init depends on both GC::Init and Module::Init,
	// so call those first.
	if ((r = GC::Init()) == OVUM_SUCCESS &&
		(r = Module::Init()) == OVUM_SUCCESS &&
		// VM::Init also takes care of loading modules
		(r = VM::Init(*params)) == OVUM_SUCCESS)
		r = VM::vm->Run();

	// done!
	// We have to unload the GC first, because the GC relies on data in
	// modules to perform cleanup, such as examining managed types and
	// calling finalizers in native types. If we clean up modules first,
	// then the GC will be very unhappy.
	//
	// Note that the Unload methods are safe to call even if the Init
	// method hasn't been called, e.g. if a previous Init call failed.
	GC::Unload();
	Module::Unload();
	VM::Unload();

#if EXIT_SUCCESS == 0
	// OVUM_SUCCESS == EXIT_SUCCESS, which also means
	// the system error codes are != OVUM_SUCCESS, so
	// let's just pass result on to the system.
	return r;
#else
	// Unlikely case - let's fall back to standard C
	// exit codes.
	return r == OVUM_SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE;
#endif
}

VM::VM(VMStartParams &params, int &status) :
	verbose(params.verbose),
	argCount(params.argc), argValues(nullptr),
	types(), functions(), mainThread(nullptr),
	startupPath(nullptr), startupPathLib(nullptr), modulePath(nullptr)
{
	mainThread = new(std::nothrow) Thread(status);
	if (mainThread == nullptr)
		status = OVUM_ERROR_NO_MEMORY;
}

VM::~VM()
{
	delete mainThread;
	delete startupPath;
	delete startupPathLib;
	delete modulePath;
	delete[] argValues;
}

int GetMainMethodOverload(VM *vm, Thread *const thread, Method *method,
                          unsigned int &argc, Method::Overload *&overload)
{
	overload = method->ResolveOverload(argc = 1);
	if (overload)
	{
		// If there is a one-argument overload, try to create an aves.List
		// and put the argument values in it.
		GCObject *listGco;
		int r = GC::gc->Alloc(thread, vm->types.List, vm->types.List->size, &listGco);
		if (r != OVUM_SUCCESS) return r;

		ListInst *argsList = reinterpret_cast<ListInst*>(listGco->InstanceBase());
		r = vm->functions.initListInstance(thread, argsList, vm->GetArgCount());
		if (r != OVUM_SUCCESS) return r;

		assert(argsList->capacity >= vm->GetArgCount());

		vm->GetArgValues(vm->GetArgCount(), argsList->values);
		argsList->length = vm->GetArgCount();

		Value argsValue;
		argsValue.type = vm->types.List;
		argsValue.instance = reinterpret_cast<uint8_t*>(argsList);
		thread->Push(&argsValue);
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

int VM::Run()
{
	int result;

	Method *main = startupModule->GetMainMethod();
	if (main == nullptr)
	{
		fwprintf(stdErr, L"Startup error: Startup module does not define a main method.\n");
		result = OVUM_ERROR_NO_MAIN_METHOD;
	}
	else
	{
		unsigned int argc;
		Method::Overload *mo;
		result = GetMainMethodOverload(this, mainThread, main, argc, mo);
		if (result != OVUM_SUCCESS)
			goto done;

		if (verbose)
			wprintf(L"<<< Begin program output >>>\n");

		Value returnValue;
		result = mainThread->Start(argc, mo, returnValue);

		if (result == OVUM_SUCCESS)
		{
			if (returnValue.type == types.Int ||
				returnValue.type == types.UInt)
				result = (int)returnValue.integer;
			else if (returnValue.type == types.Real)
				result = (int)returnValue.real;
		}
		else if (result == OVUM_ERROR_THROWN)
			PrintUnhandledError(mainThread);

		if (verbose)
			wprintf(L"<<< End program output >>>\n");
	}

done:
	return result;
}

int VM::Init(VMStartParams &params)
{
	VM::stdOut = stdout;
	VM::stdErr = stderr;

	_setmode(_fileno(stdOut), _O_U8TEXT);
	_setmode(_fileno(stdErr), _O_U8TEXT);
	_setmode(_fileno(stdin),  _O_U8TEXT);

	if (params.verbose)
	{
		wprintf(L"Module path:    %ls\n", params.modulePath);
		wprintf(L"Startup file:   %ls\n", params.startupFile);
		wprintf(L"Argument count: %d\n",  params.argc);
	}

	int r;
	vm = new(std::nothrow) VM(params, r);
	if (!vm)
		r = OVUM_ERROR_NO_MEMORY;
	else if (r == OVUM_SUCCESS)
	{
		r = vm->LoadModules(params);
		if (r == OVUM_SUCCESS)
			r = vm->InitArgs(params.argc, params.argv);
	}
	return r;
}

int VM::LoadModules(VMStartParams &params)
{
	try
	{
		// Set up some stuff first
		this->startupPath = new PathName(params.startupFile);
		this->startupPath->RemoveFileName();

		this->startupPathLib = new PathName(*this->startupPath);
		this->startupPathLib->Join(_Path("lib"));

		this->modulePath = new PathName(params.modulePath);

		// And now we can start opening modules! Hurrah!

		PathName startupFile(params.startupFile, std::nothrow);
		if (!startupFile.IsValid())
			return OVUM_ERROR_NO_MEMORY;
		this->startupModule = Module::Open(startupFile, nullptr);
	}
	catch (ModuleLoadException &e)
	{
		const PathName &fileName = e.GetFileName();
		if (fileName.GetLength() > 0)
			fwprintf(stderr, L"Error loading module '" PATHNWF L"': " CSTR L"\n", fileName.GetDataPointer(), e.what());
		else
			fwprintf(stderr, L"Error loading module: " CSTR L"\n", e.what());
		return OVUM_ERROR_MODULE_LOAD;
	}
	catch (std::bad_alloc&)
	{
		return OVUM_ERROR_NO_MEMORY;
	}

	for (unsigned int i = 0; i < std_type_names::StandardTypeCount; i++)
	{
		std_type_names::StdType type = std_type_names::Types[i];
		if (this->types.*(type.member) == nullptr)	
		{
			PrintInternal(stderr, L"Startup error: standard type not loaded: %ls\n", type.name);
			return OVUM_ERROR_MODULE_LOAD;
		}
	}

	RETURN_SUCCESS;
}

int VM::InitArgs(int argCount, const wchar_t *args[])
{
	// Convert command-line arguments to String*s.
	std::unique_ptr<Value*[]> argValues(new(std::nothrow) Value*[argCount]);
	if (!argValues.get()) return OVUM_ERROR_NO_MEMORY;

	for (int i = 0; i < argCount; i++)
	{
		String *argString = String_FromWString(nullptr, args[i]);
		if (!argString) return OVUM_ERROR_NO_MEMORY;

		Value argValue;
		argValue.type = types.String;
		argValue.common.string = argString;

		StaticRef *ref = GC::gc->AddStaticReference(nullptr, argValue);
		if (!ref) return OVUM_ERROR_NO_MEMORY;

		argValues[i] = ref->GetValuePointer();

		if (this->verbose)
		{
			wprintf(L"Argument %d: ", i);
			PrintLn(argValue.common.string);
		}
	}

	this->argValues = argValues.release();
	RETURN_SUCCESS;
}

void VM::Unload()
{
	VM::stdOut = nullptr;
	VM::stdErr = nullptr;
	delete VM::vm;
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
		std::unique_ptr<wchar_t[]> buffer(new wchar_t[length]);
		String_ToWString(buffer.get(), str);
		fwprintf(f, format, buffer.get());
	}
#else
#error Not supported
#endif
}

void VM::Print(String *str)
{
	PrintInternal(stdOut, L"%ls", str);
}
void VM::Printf(const wchar_t *format, String *str)
{
	PrintInternal(stdOut, format, str);
}
void VM::PrintLn(String *str)
{
	PrintInternal(stdOut, L"%ls\n", str);
}

void VM::PrintErr(String *str)
{
	PrintInternal(stdErr, L"%ls", str);
}
void VM::PrintfErr(const wchar_t *format, String *str)
{
	PrintInternal(stdErr, format, str);
}
void VM::PrintErrLn(String *str)
{
	PrintInternal(stdErr, L"%ls\n", str);
}

void VM::PrintUnhandledError(Thread *const thread)
{
	Value &error = thread->currentError;
	PrintInternal(stdErr, L"Unhandled error: %ls: ", error.type->fullName);

	String *message = nullptr;
	// If the member exists and is a readable instance property,
	// we can actually try to invoke the 'message' getter!
	Member *msgMember = error.type->FindMember(static_strings::message, nullptr);
	if (msgMember && !msgMember->IsStatic() &&
		(msgMember->flags & MemberFlags::KIND) == MemberFlags::PROPERTY)
	{
		Property *msgProp = static_cast<Property*>(msgMember);
		if (msgProp->getter != nullptr)
		{
			thread->Push(&error);

			Value result;
			int r = thread->InvokeMethod(msgProp->getter, 0, &result);
			if (r == OVUM_SUCCESS && result.type == vm->types.String)
				message = result.common.string;
		}
	}
	if (message == nullptr)
		message = error.common.error->message;
	if (message != nullptr)
		PrintErrLn(message);

	if (error.common.error->stackTrace)
		PrintErrLn(error.common.error->stackTrace);
}
void VM::PrintMethodInitException(MethodInitException &e)
{
	FILE *err = stdErr;

	fwprintf(err, L"An error occurred while initializing the method '");

	Method::Overload *method = e.GetMethod();
	if (method->declType)
		PrintInternal(err, L"%ls.", method->declType->fullName);
	PrintErr(method->group->name);

	PrintInternal(err, L"' from module %ls: ", method->group->declModule->name);
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
	case MethodInitException::UNRESOLVED_TOKEN_ID:
		fwprintf(err, L"Token ID: %08X\n", e.GetTokenId());
		break;
	case MethodInitException::NO_MATCHING_OVERLOAD:
		fwprintf(err, L"Method: '");
		{
			Method *method = e.GetMethodGroup();
			if (method->declType)
				PrintInternal(err, L"%ls.", method->declType->fullName);
			PrintErr(method->name);
			PrintInternal(err, L"' from module %ls\n", method->declModule->name);
		}
		fwprintf(err, L"Argument count: %u\n", e.GetArgumentCount());
		break;
	case MethodInitException::INACCESSIBLE_TYPE:
	case MethodInitException::TYPE_NOT_CONSTRUCTIBLE:
		PrintInternal(err, L"Type: '%ls' ", e.GetType()->fullName);
		PrintInternal(err, L"from module %ls\n", e.GetType()->module->name);
		break;
	}
}

int VM::GetArgs(int destLength, String *dest[])
{
	const int maxIndex = min(destLength, argCount);

	for (int i = 0; i < maxIndex; i++)
		dest[i] = argValues[i]->common.string;

	return maxIndex;
}
int VM::GetArgValues(int destLength, Value dest[])
{
	const int maxIndex = min(destLength, argCount);

	for (int i = 0; i < maxIndex; i++)
		dest[i] = *argValues[i];

	return maxIndex;
}


OVUM_API void VM_Print(String *str)
{
	VM::Print(str);
}
OVUM_API void VM_PrintLn(String *str)
{
	VM::PrintLn(str);
}

OVUM_API void VM_PrintErr(String *str)
{
	VM::PrintErr(str);
}
OVUM_API void VM_PrintErrLn(String *str)
{
	VM::PrintErrLn(str);
}

OVUM_API int VM_GetArgCount()
{
	return VM::vm->GetArgCount();
}
OVUM_API int VM_GetArgs(const int destLength, String *dest[])
{
	return VM::vm->GetArgs(destLength, dest);
}
OVUM_API int VM_GetArgValues(const int destLength, Value dest[])
{
	if (VM::vm == nullptr)
		return -1;
	return VM::vm->GetArgValues(destLength, dest);
}