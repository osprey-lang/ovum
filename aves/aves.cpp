#include <string>
#include <memory>
#include "aves.h"
#include "ov_string.h"

LitString<8> _Int_Name =  { 8, 0, StringFlags::STATIC, 'a','v','e','s','.','I','n','t',0 };
LitString<9> _UInt_Name = { 9, 0, StringFlags::STATIC, 'a','v','e','s','.','U','I','n','t',0 };
LitString<9> _Real_Name = { 9, 0, StringFlags::STATIC, 'a','v','e','s','.','R','e','a','l',0 };

LitString<18> _ArgumentError_Name = { 18, 0, StringFlags::STATIC,
	'a','v','e','s','.','A','r','g','u','m','e','n','t','E','r','r','o','r',0 };
LitString<22> _ArgumentNullError_Name = { 22, 0, StringFlags::STATIC,
	'a','v','e','s','.','A','r','g','u','m','e','n','t','N','u','l','l','E','r','r','o','r',0 };
LitString<23> _ArgumentRangeError_Name = { 23, 0, StringFlags::STATIC,
	'a','v','e','s','.','A','r','g','u','m','e','n','t','R','a','n','g','e','E','r','r','o','r',0 };
LitString<22> _DuplicateKeyError_Name = { 22, 0, StringFlags::STATIC,
	'a','v','e','s','.','D','u','p','l','i','c','a','t','e','K','e','y','E','r','r','o','r',0 };
LitString<20> _UnicodeCategory_Name = { 20, 0, StringFlags::STATIC,
	'a','v','e','s','.','U','n','i','c','o','d','e','C','a','t','e','g','o','r','y',0 };
LitString<19> _BufferViewKind_Name = { 19, 0, StringFlags::STATIC,
	'a','v','e','s','.','B','u','f','f','e','r','V','i','e','w','K','i','n','d',0 };
LitString<14> _HashEntry_Name = { 14, 0, StringFlags::STATIC,
	'a','v','e','s','.','H','a','s','h','E','n','t','r','y',0 };
LitString<15> _ConsoleKey_Name = { 15, 0, StringFlags::STATIC,
	'a','v','e','s','.','C','o','n','s','o','l','e','K','e','y',0 };
LitString<19> _ConsoleKeyCode_Name = { 19, 0, StringFlags::STATIC,
	'a','v','e','s','.','C','o','n','s','o','l','e','K','e','y','C','o','d','e',0 };

LitString<6> _format = { 6, 0, StringFlags::STATIC, 'f','o','r','m','a','t' };

TypeHandle Types::Int;
TypeHandle Types::UInt;
TypeHandle Types::Real;
TypeHandle Types::ArgumentError;
TypeHandle Types::ArgumentNullError;
TypeHandle Types::ArgumentRangeError;
TypeHandle Types::DuplicateKeyError;
TypeHandle Types::UnicodeCategory;
TypeHandle Types::BufferViewKind;
TypeHandle Types::HashEntry;
TypeHandle Types::ConsoleKey;
TypeHandle Types::ConsoleKeyCode;
String *format = _S(_format);

static bool ConsoleInputEOF;


// Note: This is not declared in any header file. Only in this source file.
AVES_API void OvumModuleMain(ModuleHandle module)
{
	Types::Int                = Module_FindType(module, _S(_Int_Name),                true);
	Types::UInt               = Module_FindType(module, _S(_UInt_Name),               true);
	Types::Real               = Module_FindType(module, _S(_Real_Name),               true);
	Types::ArgumentError      = Module_FindType(module, _S(_ArgumentError_Name),      true);
	Types::ArgumentNullError  = Module_FindType(module, _S(_ArgumentNullError_Name),  true);
	Types::ArgumentRangeError = Module_FindType(module, _S(_ArgumentRangeError_Name), true);
	Types::DuplicateKeyError  = Module_FindType(module, _S(_DuplicateKeyError_Name),  true);
	Types::UnicodeCategory    = Module_FindType(module, _S(_UnicodeCategory_Name),    true);
	Types::BufferViewKind     = Module_FindType(module, _S(_BufferViewKind_Name),     true);
	Types::HashEntry          = Module_FindType(module, _S(_HashEntry_Name),          true);
	Types::ConsoleKey         = Module_FindType(module, _S(_ConsoleKey_Name),         true);
	Types::ConsoleKeyCode     = Module_FindType(module, _S(_ConsoleKeyCode_Name),     true);
	ConsoleInputEOF = false;
}


AVES_API NATIVE_FUNCTION(aves_print)
{
	if (IS_NULL(*args))
		SetString(args, strings::Empty); // null prints like empty string
	else if (!IsString(*args))
		*args = StringFromValue(thread, *args);

	VM_PrintLn(args->common.string);
}
AVES_API NATIVE_FUNCTION(aves_printErr)
{
	if (IS_NULL(*args))
		SetString(args, strings::Empty); // null prints like empty string
	else if (!IsString(*args))
		*args = StringFromValue(thread, *args);

	VM_PrintErrLn(args->common.string);
}
AVES_API NATIVE_FUNCTION(aves_writeOut)
{
	if (IS_NULL(*args))
		return;
	if (!IsString(*args))
		*args = StringFromValue(thread, *args);

	VM_Print(args->common.string);
}
AVES_API NATIVE_FUNCTION(aves_writeErr)
{
	if (IS_NULL(*args))
		return;
	if (!IsString(*args))
		*args = StringFromValue(thread, *args);

	VM_PrintErr(args->common.string);
}

bool IsKeyDownEvent(INPUT_RECORD &ir)
{
	return ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown;
}
bool IsModifierKey(INPUT_RECORD &ir)
{
	WORD keyCode = ir.Event.KeyEvent.wVirtualKeyCode;
	return keyCode >= VK_SHIFT && keyCode <= VK_MENU ||
		keyCode == VK_CAPITAL || keyCode == VK_NUMLOCK || keyCode == VK_SCROLL;
}
bool IsAltKeyDown(INPUT_RECORD &ir)
{
	return (ir.Event.KeyEvent.dwControlKeyState & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0;
}

AVES_API NATIVE_FUNCTION(aves_readKey)
{
	static INPUT_RECORD cachedInputRecord = { };

	INPUT_RECORD ir;

	if (cachedInputRecord.EventType == KEY_EVENT)
	{
		ir = cachedInputRecord;
		if (cachedInputRecord.Event.KeyEvent.wRepeatCount == 0)
			cachedInputRecord.EventType = -1;
		else
			cachedInputRecord.Event.KeyEvent.wRepeatCount--;
	}
	else
	{
		while (true)
		{
			DWORD numEventsRead = -1;
			BOOL r = ReadConsoleInputW(GetStdHandle(STD_INPUT_HANDLE), &ir, 1, &numEventsRead);
			if (!r || numEventsRead == 0)
			{
				VM_ThrowError(thread); // TODO: error message
			}

			WORD keyCode = ir.Event.KeyEvent.wVirtualKeyCode;

			if (!IsKeyDownEvent(ir))
				if (keyCode != VK_MENU)
					continue;

			uchar ch = (uchar)ir.Event.KeyEvent.uChar.UnicodeChar;

			if (ch == 0)
				if (IsModifierKey(ir))
					continue;

			if (IsAltKeyDown(ir) &&
				(keyCode >= VK_NUMPAD0 && keyCode <= VK_NUMPAD9 ||
				keyCode == VK_CLEAR || keyCode == VK_INSERT ||
				keyCode >= VK_PRIOR && keyCode <= VK_DOWN))
				continue;

			if (ir.Event.KeyEvent.wRepeatCount > 1)
			{
				ir.Event.KeyEvent.wRepeatCount--;
				cachedInputRecord = ir;
			}
			break;
		}
	}

	{
		Value keyCodeValue;
		keyCodeValue.type = Types::ConsoleKeyCode;
		keyCodeValue.integer = ir.Event.KeyEvent.wVirtualKeyCode;

		DWORD state = ir.Event.KeyEvent.dwControlKeyState;

		uchar ch = (uchar)ir.Event.KeyEvent.uChar.UnicodeChar;
		VM_PushInt(thread, ch);
		VM_Push(thread, keyCodeValue);
		VM_PushBool(thread, (state & SHIFT_PRESSED) != 0);
		VM_PushBool(thread, (state & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0);
		VM_PushBool(thread, (state & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0);
		GC_Construct(thread, Types::ConsoleKey, 5, nullptr);

		if (argc == 0 || IsFalse(args[0]))
		{
			LitString<1> str = { 1, 0, StringFlags::STATIC, ch, 0 };
			VM_Print(_S(str));
		}
	}
}
AVES_API NATIVE_FUNCTION(aves_readChar)
{
	int ch = getchar();
	VM_PushInt(thread, ch);
}
AVES_API NATIVE_FUNCTION(aves_readLine)
{
	if (ConsoleInputEOF)
	{
		VM_PushNull(thread);
		return;
	}

	const int StackBufferSize = 512;
	using namespace std;

	int bufferSize = StackBufferSize;
	int length = 0;
	wchar_t *heapBuffer = nullptr; // Only allocated when needed
	wchar_t buffer[StackBufferSize];
	wchar_t *bufp = buffer;

	// Note: this implementation does NOT append the newline to the result

	DWORD charsRead;
	wchar_t ch;
	while (ReadConsoleW(GetStdHandle(STD_INPUT_HANDLE),
		&ch, 1, &charsRead, nullptr) && charsRead != 0)
	{
		// Control+Z (\x1A) at the beginning of the line marks the end of the input
		// Break the loop and update the flags below
		if (ch == 0x1A && length == 0 || ch == '\n')
			break;
		if (ch != '\r') // Ignore \r, because it only really occurs in \r\n
		{
			*bufp++ = ch;
			length++;
		}

		if (length == bufferSize)
		{
			// Oops - we've filled up the buffer!
			// Time to allocate a bigger buffer.
			bufferSize *= 2; // Double the buffer size
			if (heapBuffer)
			{
				wchar_t *newHeapBuffer = (wchar_t*)realloc(heapBuffer, bufferSize * sizeof(wchar_t));
				if (!newHeapBuffer)
				{
					free(heapBuffer);
					VM_ThrowMemoryError(thread);
				}
				heapBuffer = newHeapBuffer;
			}
			else
			{
				// Move from the stack buffer to the heap buffer
				heapBuffer = (wchar_t*)malloc(bufferSize * sizeof(wchar_t));
				if (!heapBuffer)
					VM_ThrowMemoryError(thread);
				CopyMemoryT(heapBuffer, buffer, StackBufferSize);
			}
			bufp = heapBuffer + length;
		}
	}
	// length < bufferSize here, so this does not
	// overflow either of the buffers:
	*bufp = 0; // Always terminate

	if (length == 0 && ch == 0x1A)
	{
		// Reached end-of-file before reading any characters,
		// update EOF flag and return null.
		ConsoleInputEOF = true;
		VM_PushNull(thread);
		return;
	}

	// Convert input to a string, woohoo!
	String *str = String_FromWString(thread, heapBuffer ? heapBuffer : buffer);
	if (heapBuffer)
		free(heapBuffer);
	VM_PushString(thread, str);
}

AVES_API NATIVE_FUNCTION(aves_clearConsole)
{
	// http://support.microsoft.com/kb/99261

	HANDLE stdOut = GetStdHandle(STD_OUTPUT_HANDLE);
	COORD home = { 0, 0 };

	BOOL success;

	// get the number of character cells in the current buffer
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	success = GetConsoleScreenBufferInfo(stdOut, &csbi);
	if (!success) return; // TODO: Throw errors

	DWORD conSize = csbi.dwSize.X * csbi.dwSize.Y;

	// fill the entire screen with blanks
	DWORD charsWritten;
	success = FillConsoleOutputCharacterW(stdOut, L' ', conSize, home, &charsWritten);
	if (!success) return; // TODO: Throw errors

	// now set the buffer's attributes accordingly 
	charsWritten = 0;
	success = FillConsoleOutputAttribute(stdOut, csbi.wAttributes, conSize, home, &charsWritten);
	if (!success) return; // TODO: Throw errors

	// put the cursor at (0, 0)
	success = SetConsoleCursorPosition(stdOut, home);
	if (!success) return; // TODO: Throw errors
}

AVES_API NATIVE_FUNCTION(aves_exit)
{
	int exitCode;
	if (args[0].type == Types::Int)
		exitCode = (int)args[0].integer;
	else if (args[0].type == Types::UInt)
		exitCode = (int)args[0].uinteger;
	else if (args[0].type == Types::Real)
		exitCode = (int)args[0].real;
	else
		exitCode = 0;

	exit(exitCode);
}

AVES_API NATIVE_FUNCTION(aves_number_asInt)
{
	VM_PushInt(thread, THISV.integer);
}
AVES_API NATIVE_FUNCTION(aves_number_asUInt)
{
	VM_PushUInt(thread, THISV.uinteger);
}
AVES_API NATIVE_FUNCTION(aves_number_asReal)
{
	VM_PushReal(thread, THISV.real);
}