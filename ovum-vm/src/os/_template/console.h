#ifndef VM__OS_CONSOLE_H
#define VM__OS_CONSOLE_H

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
		// Define OS-specific console data here, such as file handles
		// and any other information 
		...
	};

	// Initializes a ConsoleInfo with information about the current console.
	//   console:
	//     A pointer to the ConsoleInfo to initialize.
	// Returns:
	//   True if console was initialized correctly; otherwise, false. There
	//   is no extended error information for this call.
	bool ConsoleInit(ConsoleInfo *console);

	// Destroys the state (if any) associated with a ConsoleInfo. If the
	// particular implementation needs to do some cleanup, now is the time.
	//   console:
	//     A pointer to the ConsoleInfo to clean up.
	void ConsoleDestroy(ConsoleInfo *console);

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
	bool ConsoleWrite(ConsoleInfo *console, const ovchar_t *str, int32_t length);

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
		// Only change the implementation of this function if you absolutely have to.
		// You generally don't have to.
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
	bool ConsoleWriteError(ConsoleInfo *console, const ovchar_t *str, int32_t length);

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
		// Only change the implementation of this function if you absolutely have to.
		// You generally don't have to.
		return ConsoleWriteError(console, &str->firstChar, str->length);
	}

} // namespace os

} // namespace ovum

#endif // VM__OS_CONSOLE_H