#include <wchar.h>
#include <iostream>
#include <Shlwapi.h>
#include "ov_vm.internal.h"

VMState vmState;
StandardTypes stdTypes;
GlobalFunctions globalFunctions;

void CopyString(String *dest, const wchar_t *source)
{
	MutableString *mdest = reinterpret_cast<MutableString*>(dest);
	uchar *dp = &mdest->firstChar;
	const wchar_t *sp = source;

	int32_t len = 0;
	while (*dp++ = *sp++)
		len++;

	mdest->length = len;
	mdest->flags = STR_STATIC;
}

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
	using namespace std;

	if (params.verbose)
	{
		wcout << L"Module path:  " << params.modulePath << endl;
		wcout << L"Startup file: " << params.startupFile << endl;
		wcout << L"Argument count: " << params.argc << endl;
	}

	GC::Init(); // We must call this before VM_Init(), because VM_Init relies on the GC
	VM_Init(params);

	int result = 0;
	try
	{
		if (vmState.startupModule->GetMainMethod() == nullptr)
		{
			wcerr << "Startup error: Startup module does not define a main method." << endl;
			result = -1;
		}
		else
		{
			if (vmState.verbose)
				wcout << L"<<< Begin program output >>>" << endl;

			Value returnValue;
			vmState.mainThread->Start(vmState.startupModule->GetMainMethod(), returnValue);

			if (returnValue.type == stdTypes.Int ||
				returnValue.type == stdTypes.UInt)
				result = (int)returnValue.integer;
			else if (returnValue.type == stdTypes.Real)
				result = (int)returnValue.real;
		}
	}
	catch (OvumException &e)
	{
		result = -1;
	}

	// done!
	delete GC::gc;
	delete vmState.mainThread;
	Module::UnloadAll();

	return result;
}

void VM_Init(VMStartParams params)
{
	using namespace std;

	VMState state;

	// Convert command-line arguments to String*s.
	String **args = new String*[params.argc];
	for (int i = 0; i < params.argc; i++)
	{
		const wchar_t *arg = params.argv[i];
		String *str = (String*)malloc(sizeof(String) + sizeof(uchar) * wcslen(arg));
		CopyString(str, arg);
		args[i] = str;

		if (params.verbose)
		{
			wcout << "Argument " << i << ": ";
			VM_PrintLn(str);
		}
	}

	state.argCount = params.argc;
	state.argValues = args;

	state.mainThread = new Thread();

	wchar_t *startupPath = CloneWString(params.startupFile);
	PathRemoveFileSpecW(startupPath);
	state.startupPath = String_FromWString(nullptr, startupPath);
	state.startupPath->flags = STR_STATIC; // make sure the GC doesn't touch it, even though it's technically owned by the GC
	delete[] startupPath;

	state.modulePath = String_FromWString(nullptr, params.modulePath);
	state.modulePath->flags = STR_STATIC; // same here

	state.verbose = params.verbose;

	vmState = state;

	// And NOW we can start opening modules! Hurrah!

	Module::Init();
	try
	{
		vmState.startupModule = Module::Open(params.startupFile);
	}
	catch (ModuleLoadException &e)
	{
		const wchar_t *fileName = e.GetFileName();
		if (fileName)
			wcerr << "Error loading module '" << fileName << "': " << e.what() << endl;
		else
			wcerr << "Error loading module: " << e.what() << endl;
		exit(-1);
	}
}

OVUM_API void VM_Print(String *str)
{
	using namespace std;

	if (sizeof(uchar) == sizeof(wchar_t))
	{
		// Assume wchar_t is UTF-16, or at least USC-2, and just cast it.
		wcout << (const wchar_t*)&str->firstChar;
	}
	else if (sizeof(wchar_t) == sizeof(uint32_t))
	{
		// Assume wchar_t is UTF-32, and use our own conversion function to convert
		const int length = String_ToWString(nullptr, str);
		wchar_t *buffer = new wchar_t[length];
		String_ToWString(buffer, str);

		wcout << buffer;

		delete[] buffer;
	}
	else
	{
		throw L"Can't print! Implement a fallback, dammit.";
	}
}

OVUM_API void VM_PrintLn(String *str)
{
	VM_Print(str);
	std::wcout << std::endl;
}


OVUM_API int VM_GetArgCount()
{
	return vmState.argCount;
}

OVUM_API int VM_GetArgs(String *dest[], const int destLength)
{
	const int maxIndex = min(destLength, vmState.argCount);

	for (int i = 0; i < maxIndex; i++)
		dest[i] = vmState.argValues[i];

	return maxIndex;
}

OVUM_API int VM_GetArgValues(Value dest[], const int destLength)
{
	const int maxIndex = min(destLength, vmState.argCount);

	for (int i = 0; i < maxIndex; i++)
		SetString_(dest + i, vmState.argValues[i]);

	return maxIndex;
}