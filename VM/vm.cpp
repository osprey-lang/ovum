#include <wchar.h>
#include <fcntl.h>
#include <io.h>
#include <iostream>
#include <Shlwapi.h>
#include "ov_vm.internal.h"

VM *VM::vm;

wchar_t *CloneWString(const wchar_t *source)
{
	size_t outputLength = wcslen(source);
	wchar_t *output = new wchar_t[outputLength + 1]; // +1 for the \0
	memcpy(output, source, outputLength * sizeof(wchar_t));
	output[outputLength] = L'\0';
	return output;
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

	_setmode(_fileno(stdout), _O_U16TEXT);

	if (params.verbose)
	{
		wcout << L"Module path:    " << params.modulePath  << endl;
		wcout << L"Startup file:   " << params.startupFile << endl;
		wcout << L"Argument count: " << params.argc        << endl;
	}

	GC::Init(); // We must call this before VM::Init(), because VM::Init relies on the GC
	Module::Init();
	VM::Init(params);

	int result = 0;

	//VM::vm->mainThread->InitializeMethod(vm->startupModule->GetMainMethod()->overloads);
	try
	{
		Method *main = vm->startupModule->GetMainMethod();
		if (main == nullptr)
		{
			wcerr << L"Startup error: Startup module does not define a main method." << endl;
			result = -1;
		}
		else
		{
			if (vm->verbose)
				wcout << L"<<< Begin program output >>>" << endl;

			Value returnValue;
			vm->mainThread->Start(main, returnValue);

			if (returnValue.type == VM::vm->types.Int ||
				returnValue.type == VM::vm->types.UInt)
				result = (int)returnValue.integer;
			else if (returnValue.type == VM::vm->types.Real)
				result = (int)returnValue.real;

			if (vm->verbose)
				wcout << L"<<< End program output >>>" << endl;
		}
	}
	catch (OvumException &e)
	{
		wcerr << L"Unhandled error: ";
		Value error = e.GetError();
		PrintErr(error.type->fullName);
		wcerr << L": ";
		PrintErrLn(error.common.error->message);
		PrintErrLn(error.common.error->stackTrace);

		result = -1;
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
	using namespace std;

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
		const wstring &fileName = e.GetFileName();
		if (!fileName.empty())
			wcerr << "Error loading module '" << fileName.c_str() << "': " << e.what() << endl;
		else
			wcerr << "Error loading module: " << e.what() << endl;
		exit(EXIT_FAILURE);
	}

	for (unsigned int i = 0; i < std_type_names::StandardTypeCount; i++)
	{
		std_type_names::StdType type = std_type_names::Types[i];
		if (this->types.*(type.member) == nullptr)	
		{
			wcerr << "Startup error: standard type not loaded: ";
			PrintErrLn(type.name);
			exit(EXIT_FAILURE);
		}
	}
}

void VM::InitArgs(int argCount, const wchar_t *args[])
{
	using namespace std;

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
			wcout << "Argument " << i << ": ";
			PrintLn(argValue.common.string);
		}
	}

	this->argValues = argValues;
}

void VM::Unload()
{
	delete VM::vm;
}

void VM::PrintInternal(std::wostream &stream, String *str)
{
	if (sizeof(uchar) == sizeof(wchar_t))
	{
		// Assume wchar_t is UTF-16, or at least USC-2, and just cast it.
		stream << (const wchar_t*)&str->firstChar;
	}
	else if (sizeof(wchar_t) == sizeof(uint32_t))
	{
		// Assume wchar_t is UTF-32, and use our own conversion function to convert
		const int length = String_ToWString(nullptr, str);

		if (length <= 128)
		{
			wchar_t buffer[128];
			String_ToWString(buffer, str);
			stream << buffer;
		}
		else
		{
			wchar_t *buffer = new wchar_t[length];
			String_ToWString(buffer, str);

			stream << buffer;

			delete[] buffer;
		}
	}
	else
	{
		throw L"Can't print! Implement a fallback, dammit.";
	}
}

void VM::Print(String *str)
{
	PrintInternal(std::wcout, str);
}
void VM::PrintLn(String *str)
{
	PrintInternal(std::wcout, str);	
	std::wcout << std::endl;
}

void VM::PrintErr(String *str)
{
	PrintInternal(std::wcerr, str);
}
void VM::PrintErrLn(String *str)
{
	PrintInternal(std::wcerr, str);
	std::wcerr << std::endl;
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