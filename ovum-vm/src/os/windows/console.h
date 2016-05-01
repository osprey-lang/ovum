#pragma once

#include "def.h"

// Note: This header file only defines an interface for writing to the console,
// not reading. Ovum only uses the console to write diagnostic messages, such
// as when errors occur during startup. Everything else should go through the
// standard library.

namespace ovum
{

namespace os
{

	struct ConsoleInfo
	{
		HANDLE stdOut;
		HANDLE stdErr;
		// Ovum forces the code page to UTF-8, which must be restored
		// after Ovum closes.
		UINT previousCodePage;
	};

	bool ConsoleWrite_(HANDLE handle, const ovchar_t *str, int32_t strLength);

	// Initializes a ConsoleInfo with information about the current console.
	//   console:
	//     A pointer to the ConsoleInfo to initialize.
	// Returns:
	//   True if console was initialized correctly; otherwise, false. There
	//   is no extended error information for this call.
	inline bool ConsoleInit(ConsoleInfo *console)
	{
		console->stdOut = GetStdHandle(STD_OUTPUT_HANDLE);
		console->stdErr = GetStdHandle(STD_ERROR_HANDLE);
		console->previousCodePage = GetConsoleOutputCP();
	}

	// Destroys the state (if any) associated with a ConsoleInfo. If the
	// particular implementation needs to do some cleanup, now is the time.
	//   console:
	//     A pointer to the ConsoleInfo to clean up.
	inline void ConsoleDestroy(ConsoleInfo *console)
	{
		SetConsoleOutputCP(console->previousCodePage);
	}

	// Writes an Ovum string to the console, to stdout.
	//   console:
	//     The console to write to.
	//   str:
	//     The string data to write.
	//   length:
	//     The length of the string.
	// Returns:
	//   True if the string was fully written; false if an error occurred.
	//   There is no extended error information for this call.
	inline bool ConsoleWrite(ConsoleInfo *console, const ovchar_t *str, int32_t length)
	{
		return ConsoleWrite_(console->stdOut, str, length);
	}

	// Writes an Ovum string to the console, to stdout.
	//   console:
	//     The console to write to.
	//   str:
	//     The string to write.
	// Returns:
	//   True if the string was fully written; false if an error occurred.
	//   There is no extended error information for this call.
	inline bool ConsoleWrite(ConsoleInfo *console, String *str)
	{
		return ConsoleWrite(console, &str->firstChar, str->length);
	}

	// Writes an Ovum string to the console, to stderr.
	//   console:
	//     The console to write to.
	//   str:
	//     The string data to write.
	//   length:
	//     The length of the string.
	// Returns:
	//   True if the string was fully written; false if an error occurred.
	//   There is no extended error information for this call.
	inline bool ConsoleWriteError(ConsoleInfo *console, const ovchar_t *str, int32_t length)
	{
		return ConsoleWrite_(console->stdErr, str, length);
	}

	// Writes an Ovum string to the console, to stderr.
	//   console:
	//     The console to write to.
	//   str:
	//     The string to write.
	// Returns:
	//   True if the string was fully written; false if an error occurred.
	//   There is no extended error information for this call.
	inline bool ConsoleWriteError(ConsoleInfo *console, String *str)
	{
		return ConsoleWriteError(console, &str->firstChar, str->length);
	}

} // namespace os

} // namespace ovum
