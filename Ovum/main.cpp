#include <iostream>
#include "main.h"

using namespace std;

//typedef struct Console_S
//{
//	HANDLE stdIn;
//	HANDLE stdOut;
//} Console;
//Console console;

#define CNSL_BLACK        0
#define CNSL_DARKGRAY     FOREGROUND_INTENSITY
#define CNSL_GRAY         (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)
#define CNSL_WHITE        (CNSL_GRAY | FOREGROUND_INTENSITY)
#define CNSL_DARKRED      FOREGROUND_RED
#define CNSL_RED          (FOREGROUND_RED | FOREGROUND_INTENSITY)
#define CNSL_DARKYELLOW   (FOREGROUND_RED | FOREGROUND_GREEN)
#define CNSL_YELLOW       (CNSL_DARKYELLOW | FOREGROUND_INTENSITY)
#define CNSL_DARKGREEN    FOREGROUND_GREEN
#define	CNSL_GREEN        (FOREGROUND_GREEN | FOREGROUND_INTENSITY)
#define CNSL_DARKCYAN     (FOREGROUND_GREEN | FOREGROUND_BLUE)
#define CNSL_CYAN         (CNSL_DARKCYAN | FOREGROUND_INTENSITY)
#define CNSL_DARKBLUE     FOREGROUND_BLUE
#define CNSL_BLUE         (FOREGROUND_BLUE | FOREGROUND_INTENSITY)
#define CNSL_DARKMAGENTA  (FOREGROUND_BLUE | FOREGROUND_RED)
#define CNSL_MAGENTA      (CNSL_DARKMAGENTA | FOREGROUND_INTENSITY)
#define CNSL_FG(c)        c
#define CNSL_BG(c)        (c << 4)

int wmain(int argc, wchar_t *argv[])
{
	if (argc == 1)
		PrintUsageAndExit();

	OvumArgs args;
	memset(&args, 0, sizeof(OvumArgs)); // initialize to zero
	ParseCommandLine(argc - 1, argv + 1, args);

	VMStartParams vm;
	vm.argc = argc - args.argOffset - 1;
	vm.argv = (const wchar_t**)(argv + args.argOffset + 1);
	vm.modulePath  = args.modulePath;
	vm.startupFile = args.startupFile;
	vm.verbose     = args.verbose;

	return VM_Start(vm);
}

void ParseCommandLine(int argc, wchar_t *argv[], OvumArgs &args)
{
	for (int i = 0; i < argc; i++)
	{
		wchar_t *arg = argv[i];
		size_t len = wcslen(arg);

		if (len > 0 && (*arg == L'-' || *arg == L'/')) // switch, it's a switch! get the switch!
		{
			if (wcscmp(arg + 1, L"L") == 0)
			{
				if (i >= argc - 1) // must be at least one more argument
					CommandParseError("/L must be followed by the name of a directory");
				if (args.hasModulePath)
					CommandParseError("/L can only occur once");
				args.hasModulePath = true;
				args.modulePath = argv[++i];
			}
			else if (wcscmp(arg + 1, L"v") == 0)
			{
				if (args.verbose)
					CommandParseError("/v can only occur once");
				args.verbose = true;
			}
			else
				CommandParseError("Invalid argument: ", arg);
		}
		else
		{
			// This must be the name of the startup file! omg!
			// Because of issues with shared libraries and possibly multithreading, we need
			// to resolve this to an absolute path before passing it into the virtual machine.
			wchar_t *startup = new wchar_t[MAX_PATH];
			GetStartupFile(arg, startup, MAX_PATH);
			args.startupFile = startup;
			args.argOffset = i + 1;
			break;
		}
	}

	if (args.startupFile == nullptr)
		CommandParseError("Startup file is missing.");

	if (!args.hasModulePath)
	{
		// Use the default
		wchar_t *libPath = new wchar_t[MAX_PATH];
		GetModulePath(libPath, MAX_PATH);
		args.modulePath = libPath;
	}
}

void CommandParseError(const char *message, wchar_t *extra)
{
	HANDLE stdOut = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO cbuf;
	GetConsoleScreenBufferInfo(stdOut, &cbuf);

	SetConsoleTextAttribute(stdOut, CNSL_RED);
	cout << "Could not start Ovum: " << message;
	if (extra)
		_tcout << extra;
	cout << endl << endl;

	SetConsoleTextAttribute(stdOut, cbuf.wAttributes);
	PrintUsageAndExit();
}

void PrintUsageAndExit()
{
	HANDLE stdOut = GetStdHandle(STD_OUTPUT_HANDLE);
	CONSOLE_SCREEN_BUFFER_INFO cbuf;
	GetConsoleScreenBufferInfo(stdOut, &cbuf);

	SetConsoleTextAttribute(stdOut, CNSL_GRAY); // Set to gray on black in case the default is something else
	cout
		<< "Usage:" << endl << endl;
	SetConsoleTextAttribute(stdOut, CNSL_WHITE);
	cout
		<< "    Ovum.exe ";
	SetConsoleTextAttribute(stdOut, CNSL_YELLOW);
	cout << "[VM args...] ";
	SetConsoleTextAttribute(stdOut, CNSL_GREEN);
	cout << "<startup file> ";
	SetConsoleTextAttribute(stdOut, CNSL_CYAN);
	cout << "[program args...]" << endl << endl;
	SetConsoleTextAttribute(stdOut, CNSL_GRAY);
	cout
		<< "The ";
		SetConsoleTextAttribute(stdOut, CNSL_GREEN);
		cout << "startup file";
		SetConsoleTextAttribute(stdOut, CNSL_GRAY);
	cout << " is the compiled Ovum program to be executed by the VM." << endl << endl
		<< "The ";
		SetConsoleTextAttribute(stdOut, CNSL_CYAN);
		cout << "program args";
		SetConsoleTextAttribute(stdOut, CNSL_GRAY);
	cout << " are passed to the program hosted by the VM. See the program's documentation for those." << endl << endl
		<< "The ";
		SetConsoleTextAttribute(stdOut, CNSL_YELLOW);
		cout << "VM args";
		SetConsoleTextAttribute(stdOut, CNSL_GRAY);
	cout << " are used by Ovum.exe. The following are available (order does not matter):" << endl;
	SetConsoleTextAttribute(stdOut, CNSL_YELLOW);
	cout << "    /L <directory>" << endl;
	SetConsoleTextAttribute(stdOut, CNSL_GRAY);
	cout
		<< "        Specifies the directory that modules are loaded from. Mnemonic: L for Library." << endl
		<< "        By default, they are loaded from the 'lib' directory in Ovum.exe's containing folder." << endl;
	SetConsoleTextAttribute(stdOut, CNSL_YELLOW);
	cout << "    /v" << endl;
	SetConsoleTextAttribute(stdOut, CNSL_GRAY);
	cout
		<< "        If present, the VM prints additional information during startup." << endl
		<< "        Hosted program output begins after '<<< Begin program output >>>'." << endl
		<< "        Mnemonic: v for verbose." << endl;

	//SetConsoleTextAttribute(stdOut, cbuf.wAttributes);
	exit(0);
}

void GetStartupFile(const wchar_t *path, wchar_t *buf, size_t bufSize)
{
	GetFullPathNameW(path, bufSize, buf, nullptr);
	// And that's it, I think!
}

void GetModulePath(wchar_t *buf, size_t bufSize)
{
	// The default module path is relative to to Ovum.exe.
	GetModuleFileNameW(nullptr, buf, bufSize);
	PathRemoveFileSpecW(buf); // removes \Ovum.exe
	PathAppendW(buf, L"lib"); // PathAppend automatically inserts backspace
}