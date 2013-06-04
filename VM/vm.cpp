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

	GC_Init(); // We must call this before VM_Init(), because VM_Init relies on the GC
	VM_Init(params);

	if (vmState.verbose)
		wcout << L"<<< Begin program output >>>" << endl;

	// ... run VM ...
	int result = 0;
	Value returnValue;
	vmState.mainThread->Start(NULL, returnValue);
	// blah blah

	// done!
	delete vmState.mainThread;

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
			cout << "Argument " << i << ": ";
			VM_PrintLn(str);
		}
	}

	state.argCount = params.argc;
	state.argValues = args;

	state.mainThread = new Thread();

	wchar_t *startupPath = CloneWString(params.startupFile);
	PathRemoveFileSpecW(startupPath);
	state.startupPath = String_FromWString(NULL, startupPath);
	state.startupPath->flags = STR_STATIC; // make sure the GC doesn't touch it, even though it's technically owned by the GC
	delete[] startupPath;

	state.modulePath = String_FromWString(NULL, params.modulePath);
	state.modulePath->flags = STR_STATIC; // same here

	state.verbose = params.verbose;

	vmState = state;

	// And NOW we can start opening modules! Hurrah!

	try
	{
		Module::OpenByName(stdModuleName);
	}
	catch (ModuleLoadException &e)
	{
		const wchar_t *fileName = e.GetFileName();
		if (fileName)
			wcout << "Error loading module '" << fileName << "': " << e.what() << endl;
		else
			wcout << "Error loading module: " << e.what() << endl;
		exit(-1);
	}
}

OVUM_API void VM_Print(String *str)
{
	using namespace std;
#ifdef UNICODE
	// If UNICODE, then Windows uses UTF-16 internally, so we can just cast to wchar_t*!
	wcout << (const wchar_t*)&str->firstChar;
#else
	// Let's use the Windows API to convert our text to UTF-8!
	int length = WideCharToMultiByte(CP_UTF8, 0, (LPWSTR)&str->firstChar, str->length, NULL, 0, NULL, NULL);
	CHAR *output = new CHAR[length + 1];
	length = WideCharToMultiByte(CP_UTF8, 0, (LPWSTR)&str->firstChar, str->length, output, length + 1, NULL, NULL);
	output[length] = '\0';

	cout << output;

	delete[] output;
#endif
}

OVUM_API void VM_PrintLn(String *str)
{
	VM_Print(str);
	std::cout << std::endl;
}


OVUM_API int VM_GetArgCount()
{
	return vmState.argCount;
}

OVUM_API int VM_GetArgs(String **dest, int destLength)
{
	int maxIndex = min(destLength, vmState.argCount);

	for (int i = 0; i < maxIndex; i++)
		dest[i] = vmState.argValues[i];

	return maxIndex;
}