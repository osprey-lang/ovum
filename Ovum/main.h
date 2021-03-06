#pragma once

#include "targetver.h"
// Exclude rarely-used stuff from Windows headers
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <stdio.h>
#include <Shlwapi.h>

#include <ovum.h>
#include <ovum_main.h>

// Arguments parsed by the command line parser. Whee!
typedef struct OvumArgs_S
{
	// The offset of the first "real" argument passed to the startup file.
	int argOffset;

	bool hasModulePath; // Is explicit module path specified?
	wchar_t *modulePath; // -L <path>: The directory from which to load modules

	wchar_t *startupFile; // The startup file

	bool verbose; // -v: Adds extra verbosity to the VM during startup and shutdown
} OvumArgs;

void ParseCommandLine(int argc, wchar_t *argv[], OvumArgs &args);

void CommandParseError(const char *message, const wchar_t *extra = nullptr);

void PrintUsageAndExit();

void GetStartupFile(const wchar_t *path, wchar_t *buf, size_t bufSize);
void GetModulePath(wchar_t *buf, size_t bufSize);
