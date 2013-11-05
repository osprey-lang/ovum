#include <wchar.h>
#include <fcntl.h>
#include <io.h>
#include <cstdio>
#include <memory>
#include <Shlwapi.h>
#include "ov_vm.internal.h"

#ifdef _MSC_VER
// Microsoft's C++ implementation uses %s to mean wchar_t* in wide functions,
// and requires the non-portable %S for char*s. Sigh.
#define CSTR  L"%S"
#else
#define CSTR  L"%s"
#endif

VM *VM::vm;

wchar_t *CloneWString(const wchar_t *source)
{
	size_t outputLength = wcslen(source);
	std::unique_ptr<wchar_t[]> output(new wchar_t[outputLength + 1]); // +1 for the \0
	memcpy(output.get(), source, outputLength * sizeof(wchar_t));
	output[outputLength] = L'\0';
	return output.release();
}

OVUM_API int VM_Start(VMStartParams params)
{
	return VM::Run(params);
}

VM::VM(VMStartParams &params) :
	argCount(params.argc), verbose(params.verbose),
	types(), functions(), mainThread(new Thread())
{ }

VM::~VM()
{
	delete mainThread;
	delete[] argValues;
}

int VM::Run(VMStartParams &params)
{
	using namespace std;

	_setmode(_fileno(stdout), _O_WTEXT);
	_setmode(_fileno(stderr), _O_WTEXT);
	_setmode(_fileno(stdin),  _O_WTEXT);

	if (params.verbose)
	{
		wprintf(L"Module path:    %ls\n", params.modulePath);
		wprintf(L"Startup file:   %ls\n", params.startupFile);
		wprintf(L"Argument count: %d\n",  params.argc);
	}

	GC::Init(); // We must call this before VM::Init(), because VM::Init relies on the GC
	Module::Init();
	VM::Init(params); // Also takes care of loading modules

	int result = 0;

	try
	{
		Method *main = vm->startupModule->GetMainMethod();
		if (main == nullptr)
		{
			fwprintf(stderr, L"Startup error: Startup module does not define a main method.\n");
			result = EXIT_FAILURE;
		}
		else
		{
			if (vm->verbose)
				wprintf(L"<<< Begin program output >>>\n");

			Value returnValue;
			vm->mainThread->Start(main, returnValue);

			if (returnValue.type == VM::vm->types.Int ||
				returnValue.type == VM::vm->types.UInt)
				result = (int)returnValue.integer;
			else if (returnValue.type == VM::vm->types.Real)
				result = (int)returnValue.real;

			if (vm->verbose)
				wprintf(L"<<< End program output >>>\n");
		}
	}
	catch (OvumException &e)
	{
		PrintOvumException(e);
		result = EXIT_FAILURE;
	}
	catch (MethodInitException &e)
	{
		PrintMethodInitException(e);
		result = EXIT_FAILURE;
	}

	// done!
	// Note: unload the GC first, as it may use the main thread.
	GC::Unload();
	Module::Unload();
	VM::Unload();

	return result;
}

void VM::Init(VMStartParams &params)
{
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
	GC::gc->MakeImmortal(GCO_FROM_INST(this->startupPath));
	delete[] startupPath;

	this->modulePath = String_FromWString(nullptr, params.modulePath);
	GC::gc->MakeImmortal(GCO_FROM_INST(this->modulePath));

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
		exit(EXIT_FAILURE);
	}

	for (unsigned int i = 0; i < std_type_names::StandardTypeCount; i++)
	{
		std_type_names::StdType type = std_type_names::Types[i];
		if (this->types.*(type.member) == nullptr)	
		{
			PrintInternal(stderr, L"Startup error: standard type not loaded: %ls\n", type.name);
			exit(EXIT_FAILURE);
		}
	}
}

void VM::InitArgs(int argCount, const wchar_t *args[])
{
	// Convert command-line arguments to String*s.
	Value **argValues = new Value*[argCount];
	for (int i = 0; i < argCount; i++)
	{
		const wchar_t *arg = args[i];
		Value argValue;
		SetString_(argValue, String_FromWString(nullptr, args[i]));
		argValues[i] = GC::gc->AddStaticReference(argValue);

		if (this->verbose)
		{
			wprintf(L"Argument %d: ", i);
			PrintLn(argValue.common.string);
		}
	}

	this->argValues = argValues;
}

void VM::Unload()
{
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

void VM::PrintOvumException(OvumException &e)
{
	using namespace std;

	Value error = e.GetError();
	PrintInternal(stderr, L"Unhandled error: %ls: ", error.type->fullName);
	PrintErrLn(error.common.error->message);
	PrintErrLn(error.common.error->stackTrace);
}
void VM::PrintMethodInitException(MethodInitException &e)
{
	using namespace std;

	FILE *err = stderr;

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
		if (e.GetMethodGroup()->declType)
			PrintInternal(err, L"%ls.", e.GetMethodGroup()->declType->fullName);
		PrintErr(e.GetMethodGroup()->name);
		PrintInternal(err, L"' from module %ls\n", e.GetMethodGroup()->declModule->name);

		fwprintf(err, L"Argument count: %u\n", e.GetArgumentCount());
		break;
	case MethodInitException::INACCESSIBLE_TYPE:
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
	return VM::vm->GetArgValues(destLength, dest);
}