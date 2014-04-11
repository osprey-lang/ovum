#include "ov_vm.internal.h"
#include "ov_module.internal.h"
#include <fcntl.h>
#include <io.h>
#include <cstdio>
#include <memory>
#include <Shlwapi.h>

#ifdef _MSC_VER
// Microsoft's C++ implementation uses %s to mean wchar_t* in wide functions,
// and requires the non-portable %S for char*s. Sigh.
#define CSTR  L"%S"
#else
#define CSTR  L"%s"
#endif

VM *VM::vm;
FILE *VM::stdOut;
FILE *VM::stdErr;

wchar_t *CloneWString(const wchar_t *source)
{
	size_t outputLength = wcslen(source);
	std::unique_ptr<wchar_t[]> output(new wchar_t[outputLength + 1]); // +1 for the \0
	memcpy(output.get(), source, outputLength * sizeof(wchar_t));
	output[outputLength] = L'\0';
	return output.release();
}

OVUM_API int VM_Start(VMStartParams *params)
{
	GC::Init(); // We must call this before VM::Init(), because VM::Init relies on the GC
	Module::Init();
	VM::Init(*params); // Also takes care of loading modules

	int result = VM::vm->Run(*params);

	// done!
	GC::Unload();
	Module::Unload();
	VM::Unload();

#if EXIT_SUCCESS == 0
	// OVUM_SUCCESS == EXIT_SUCCESS, which also means
	// the system error codes are != OVUM_SUCCESS, so
	// let's just pass result on to the system.
	return result;
#else
	// Unlikely case - let's fall back to standard C
	// exit codes.
	return result == OVUM_SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE;
#endif
}

VM::VM(VMStartParams &params) :
	argCount(params.argc), verbose(params.verbose),
	types(), functions(), mainThread(new Thread()),
	startupPath(nullptr), modulePath(nullptr)
{ }

VM::~VM()
{
	delete mainThread;
	delete[] argValues;
}

int VM::Run(VMStartParams &params)
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
		if (verbose)
			wprintf(L"<<< Begin program output >>>\n");

		Value returnValue;
		result = mainThread->Start(main, returnValue);

		if (result == OVUM_SUCCESS)
		{
			if (returnValue.type == types.Int ||
				returnValue.type == types.UInt)
				result = (int)returnValue.integer;
			else if (returnValue.type == types.Real)
				result = (int)returnValue.real;
		}
		else if (result == OVUM_ERROR_THROWN)
			PrintUnhandledError(mainThread->currentError);

		if (verbose)
			wprintf(L"<<< End program output >>>\n");
	}

	return result;
}

void VM::Init(VMStartParams &params)
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

	vm = new VM(params);
	vm->LoadModules(params);
	vm->InitArgs(params.argc, params.argv);
}

void VM::LoadModules(VMStartParams &params)
{
	// Set up some stuff first
	wchar_t *startupPath = CloneWString(params.startupFile);
	PathRemoveFileSpecW(startupPath);
	this->startupPath = String_FromWString(nullptr, startupPath);
	GCObject::FromInst(this->startupPath)->flags |= GCOFlags::EARLY_STRING;
	delete[] startupPath;

	this->modulePath = String_FromWString(nullptr, params.modulePath);
	GCObject::FromInst(this->modulePath)->flags |= GCOFlags::EARLY_STRING;

	// And now we can start opening modules! Hurrah!
	try
	{
		this->startupModule = Module::Open(params.startupFile);
	}
	catch (ModuleLoadException &e)
	{
		const std::wstring &fileName = e.GetFileName();
		if (!fileName.empty())
			fwprintf(stderr, L"Error loading module '%ls': " CSTR L"\n", fileName.c_str(), e.what());
		else
			fwprintf(stderr, L"Error loading module: " CSTR L"\n", e.what());
		exit(OVUM_ERROR_MODULE_LOAD);
	}

	for (unsigned int i = 0; i < std_type_names::StandardTypeCount; i++)
	{
		std_type_names::StdType type = std_type_names::Types[i];
		if (this->types.*(type.member) == nullptr)	
		{
			PrintInternal(stderr, L"Startup error: standard type not loaded: %ls\n", type.name);
			exit(OVUM_ERROR_MODULE_LOAD);
		}
	}
}

void VM::InitArgs(int argCount, const wchar_t *args[])
{
	// Convert command-line arguments to String*s.
	std::unique_ptr<Value*[]> argValues(new Value*[argCount]);
	for (int i = 0; i < argCount; i++)
	{
		const wchar_t *arg = args[i];
		Value argValue;
		SetString_(argValue, String_FromWString(nullptr, args[i]));
		argValues[i] = GC::gc->AddStaticReference(argValue)->GetValuePointer();

		if (this->verbose)
		{
			wprintf(L"Argument %d: ", i);
			PrintLn(argValue.common.string);
		}
	}

	this->argValues = argValues.release();
}

void VM::Unload()
{
	VM::stdOut = nullptr;
	VM::stdErr = nullptr;
	delete VM::vm;
}

void VM::PrintInternal(FILE *f, const wchar_t *format, String *str)
{
	using namespace std;

	if (sizeof(uchar) == sizeof(wchar_t))
	{
		// Assume wchar_t is UTF-16, or at least USC-2, and just cast it.
		fwprintf(f, format, (const wchar_t*)&str->firstChar);
	}
	else if (sizeof(wchar_t) == sizeof(uint32_t))
	{
		// Assume wchar_t is UTF-32, and use our own conversion function to convert
		const int length = String_ToWString(nullptr, str);

		if (length <= 128)
		{
			wchar_t buffer[128];
			String_ToWString(buffer, str);
			fwprintf(f, format, buffer);
		}
		else
		{
			unique_ptr<wchar_t[]> buffer(new wchar_t[length]);
			String_ToWString(buffer.get(), str);
			fwprintf(f, format, buffer.get());
		}
	}
	else
	{
		throw L"Can't print! Implement a fallback, dammit.";
	}
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

void VM::PrintUnhandledError(Value &error)
{
	PrintInternal(stdErr, L"Unhandled error: %ls: ", error.type->fullName);
	PrintErrLn(error.common.error->message);
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
	case MethodInitException::INCONSISTENT_STACK_HEIGHT:
	case MethodInitException::INVALID_BRANCH_OFFSET:
	case MethodInitException::INSUFFICIENT_STACK_HEIGHT:
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

int VM::GetArgs(const int destLength, String *dest[])
{
	const int maxIndex = min(destLength, argCount);

	for (int i = 0; i < maxIndex; i++)
		dest[i] = argValues[i]->common.string;

	return maxIndex;
}
int VM::GetArgValues(const int destLength, Value dest[])
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