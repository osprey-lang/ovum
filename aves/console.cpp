#include <cstdio>
#include <memory>
#include "aves_console.h"
#include "aves_ns.h"
#include "ov_string.h"

LitString<39> ConsoleIOError = LitString<39>::FromCString("An I/O error occurred with the console.");

class Console
{
public:
	static bool InputEOF;
	static bool HaveDefaultColors;
	static WORD DefaultColors;
	static HANDLE StdOut;

	static int GetBufferInfo(ThreadHandle thread, CONSOLE_SCREEN_BUFFER_INFO &csbi);
	static int GetDefaultColors(ThreadHandle thread);
	static int GetCurrentAttrs(ThreadHandle thread, WORD &attrs);
	static int GetCursorPosition(ThreadHandle thread, COORD &pos);
	static void SetCursorPosition(ThreadHandle thread, SHORT x, SHORT y);

	static int ThrowConsoleError(ThreadHandle thread);
};

bool Console::InputEOF;
bool Console::HaveDefaultColors;
WORD Console::DefaultColors;
HANDLE Console::StdOut;

int Console::GetBufferInfo(ThreadHandle thread, CONSOLE_SCREEN_BUFFER_INFO &csbi)
{
	BOOL r = GetConsoleScreenBufferInfo(Console::StdOut, &csbi);
	if (!r) return ThrowConsoleError(thread);

	RETURN_SUCCESS;
}

int Console::GetDefaultColors(ThreadHandle thread)
{
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	int r = GetBufferInfo(thread, csbi);
	if (r == OVUM_SUCCESS)
	{
		DefaultColors = csbi.wAttributes & 0xff;
		HaveDefaultColors = true;
	}
	return r;
}
int Console::GetCurrentAttrs(ThreadHandle thread, WORD &attrs)
{
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	int r = GetBufferInfo(thread, csbi);
	if (r == OVUM_SUCCESS)
	{
		// Text color occupies the lowest byte:
		attrs = csbi.wAttributes & 0xff;
	}
	return r;
}

AVES_API void aves_Console_init(TypeHandle type)
{
	Console::InputEOF = false;
	Console::HaveDefaultColors = false;
	Console::DefaultColors = 0;
	Console::StdOut = GetStdHandle(STD_OUTPUT_HANDLE);
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_Console_write)
{
	if (IS_NULL(*args))
		RETURN_SUCCESS;
	if (!IsString(thread, args))
		CHECKED(StringFromValue(thread, args));

	VM_Print(args->v.string);
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_Console_writeErr)
{
	if (IS_NULL(*args))
		RETURN_SUCCESS;
	if (!IsString(thread, args))
		CHECKED(StringFromValue(thread, args));

	VM_PrintErr(args->v.string);
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_Console_writeLineErr)
{
	if (IS_NULL(*args))
		SetString(thread, args, strings::Empty); // null prints like empty string
	else if (!IsString(thread, args))
		CHECKED(StringFromValue(thread, args));

	VM_PrintErrLn(args->v.string);
}
END_NATIVE_FUNCTION

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

AVES_API BEGIN_NATIVE_FUNCTION(aves_Console_readKey)
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
		VM_EnterUnmanagedRegion(thread);

		while (true)
		{
			DWORD numEventsRead = -1;
			BOOL r = ReadConsoleInputW(GetStdHandle(STD_INPUT_HANDLE), &ir, 1, &numEventsRead);
			if (!r || numEventsRead == 0)
				Console::ThrowConsoleError(thread);

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

		VM_LeaveUnmanagedRegion(thread);
	}

	{
		Value charValue;
		charValue.type = Types::Char;
		charValue.v.integer = (uint32_t)ir.Event.KeyEvent.uChar.UnicodeChar;
		VM_Push(thread, &charValue);

		Value keyCodeValue;
		keyCodeValue.type = Types::ConsoleKeyCode;
		keyCodeValue.v.integer = ir.Event.KeyEvent.wVirtualKeyCode;
		VM_Push(thread, &keyCodeValue);

		DWORD state = ir.Event.KeyEvent.dwControlKeyState;
		VM_PushBool(thread, (state & SHIFT_PRESSED) != 0);
		VM_PushBool(thread, (state & (LEFT_ALT_PRESSED | RIGHT_ALT_PRESSED)) != 0);
		VM_PushBool(thread, (state & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0);
		CHECKED(GC_Construct(thread, Types::ConsoleKey, 5, nullptr));

		if (argc == 0 || IsFalse(args + 0))
		{
			LitString<1> str = { 1, 0, StringFlags::STATIC, (uchar)ir.Event.KeyEvent.uChar.UnicodeChar, 0 };
			VM_Print(str.AsString());
		}
	}
}
END_NATIVE_FUNCTION
AVES_API NATIVE_FUNCTION(aves_Console_readChar)
{
	VM_EnterUnmanagedRegion(thread);
	wint_t ch = getwchar();
	VM_LeaveUnmanagedRegion(thread);

	VM_PushInt(thread, ch);
	RETURN_SUCCESS;
}
AVES_API BEGIN_NATIVE_FUNCTION(aves_Console_readLine)
{
	if (Console::InputEOF)
	{
		VM_PushNull(thread);
		RETURN_SUCCESS;
	}

	VM_EnterUnmanagedRegion(thread);

	const int StackBufferSize = 256;
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
					VM_LeaveUnmanagedRegion(thread);
					return VM_ThrowMemoryError(thread);
				}
				heapBuffer = newHeapBuffer;
			}
			else
			{
				// Move from the stack buffer to the heap buffer
				heapBuffer = (wchar_t*)malloc(bufferSize * sizeof(wchar_t));
				if (!heapBuffer)
				{
					VM_LeaveUnmanagedRegion(thread);
					return VM_ThrowMemoryError(thread);
				}
				CopyMemoryT(heapBuffer, buffer, StackBufferSize);
			}
			bufp = heapBuffer + length;
		}
	}
	// length < bufferSize here, so this does not
	// overflow either of the buffers:
	*bufp = 0; // Always terminate

	VM_LeaveUnmanagedRegion(thread);

	if (length == 0 && ch == 0x1A)
	{
		// Reached end-of-file before reading any characters,
		// update EOF flag and return null.
		Console::InputEOF = true;
		VM_PushNull(thread);
		RETURN_SUCCESS;
	}

	// Convert input to a string, woohoo!
	String *str;
	CHECKED_MEM(str = String_FromWString(thread, heapBuffer ? heapBuffer : buffer));
	if (heapBuffer)
		free(heapBuffer);
	VM_PushString(thread, str);
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_Console_clear)
{
	VM_EnterUnmanagedRegion(thread);

	// http://support.microsoft.com/kb/99261

	HANDLE stdOut = GetStdHandle(STD_OUTPUT_HANDLE);
	COORD home = { 0, 0 };

	BOOL success;

	// get the number of character cells in the current buffer
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	success = GetConsoleScreenBufferInfo(stdOut, &csbi);
	if (!success) return Console::ThrowConsoleError(thread);

	DWORD conSize = csbi.dwSize.X * csbi.dwSize.Y;

	// fill the entire screen with blanks
	DWORD charsWritten;
	success = FillConsoleOutputCharacterW(stdOut, L' ', conSize, home, &charsWritten);
	if (!success) return Console::ThrowConsoleError(thread);

	// now set the buffer's attributes accordingly 
	charsWritten = 0;
	success = FillConsoleOutputAttribute(stdOut, csbi.wAttributes, conSize, home, &charsWritten);
	if (!success) return Console::ThrowConsoleError(thread);

	// put the cursor at (0, 0)
	success = SetConsoleCursorPosition(stdOut, home);
	if (!success) return Console::ThrowConsoleError(thread);

	VM_LeaveUnmanagedRegion(thread);

	RETURN_SUCCESS;
}

int AssertIsConsoleColor(ThreadHandle thread, Value *arg)
{
	if (arg->type != Types::ConsoleColor)
		return VM_ThrowTypeError(thread);
	RETURN_SUCCESS;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_Console_get_textColor)
{
	WORD currentAttrs;
	CHECKED(Console::GetCurrentAttrs(thread, currentAttrs));

	Value result;
	result.type = Types::ConsoleColor;
	// Foreground color occupies the lowest 4 bits
	result.v.integer = currentAttrs & 0x0f;
	VM_Push(thread, &result);
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_Console_set_textColor)
{
	CHECKED(AssertIsConsoleColor(thread, args));

	if (!Console::HaveDefaultColors)
		CHECKED(Console::GetDefaultColors(thread));

	WORD currentAttrs;
	CHECKED(Console::GetCurrentAttrs(thread, currentAttrs));
	SetConsoleTextAttribute(Console::StdOut, (currentAttrs & ~0x0f) | (WORD)args->v.integer);
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_Console_get_backColor)
{
	WORD currentAttrs;
	CHECKED(Console::GetCurrentAttrs(thread, currentAttrs));

	Value result;
	result.type = Types::ConsoleColor;
	// Background color occupies bits 4–7
	result.v.integer = (currentAttrs & 0xf0) >> 4;
	VM_Push(thread, &result);
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_Console_set_backColor)
{
	CHECKED(AssertIsConsoleColor(thread, args));

	if (!Console::HaveDefaultColors)
		CHECKED(Console::GetDefaultColors(thread));

	WORD currentAttrs;
	CHECKED(Console::GetCurrentAttrs(thread, currentAttrs));
	SetConsoleTextAttribute(Console::StdOut, (currentAttrs & ~0xf0) | ((WORD)args->v.integer << 4));
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_Console_setColors)
{
	CHECKED(AssertIsConsoleColor(thread, args + 0));
	CHECKED(AssertIsConsoleColor(thread, args + 1));

	if (!Console::HaveDefaultColors)
		CHECKED(Console::GetDefaultColors(thread));

	WORD currentAttrs;
	CHECKED(Console::GetCurrentAttrs(thread, currentAttrs));
	SetConsoleTextAttribute(Console::StdOut, (currentAttrs & ~0xff) |
		((WORD)args[0].v.integer & 0x0f) | // foreground
		(((WORD)args[1].v.integer & 0x0f) << 4)); // background
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_Console_resetColors)
{
	if (Console::HaveDefaultColors)
	{
		CONSOLE_SCREEN_BUFFER_INFO csbi;
		CHECKED(Console::GetBufferInfo(thread, csbi));

		SetConsoleTextAttribute(Console::StdOut,
			Console::DefaultColors | (csbi.wAttributes & ~0xff));
	}
}
END_NATIVE_FUNCTION

AVES_API NATIVE_FUNCTION(aves_Console_get_showCursor)
{
	CONSOLE_CURSOR_INFO cci;
	BOOL success = GetConsoleCursorInfo(Console::StdOut, &cci);
	if (!success)
		return Console::ThrowConsoleError(thread);

	VM_PushBool(thread, cci.bVisible ? true : false);
	RETURN_SUCCESS;
}
AVES_API NATIVE_FUNCTION(aves_Console_set_showCursor)
{
	CONSOLE_CURSOR_INFO cci;
	BOOL success = GetConsoleCursorInfo(Console::StdOut, &cci);
	if (!success)
		return Console::ThrowConsoleError(thread);

	cci.bVisible = IsTrue(args + 0);
	SetConsoleCursorInfo(Console::StdOut, &cci);
	RETURN_SUCCESS;
}

int Console::GetCursorPosition(ThreadHandle thread, COORD &pos)
{
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	int r = GetBufferInfo(thread, csbi);
	if (r == OVUM_SUCCESS)
		pos = csbi.dwCursorPosition;
	return r;
}
void Console::SetCursorPosition(ThreadHandle thread, SHORT x, SHORT y)
{
	COORD pos = { x, y };
	SetConsoleCursorPosition(StdOut, pos);
}

int AssertValidCoord(ThreadHandle thread, Value *v, String *paramName)
{
	int r = IntFromValue(thread, v);
	if (r == OVUM_SUCCESS)
	{
		if (v->v.integer < 0 || v->v.integer > SHRT_MAX)
		{
			VM_PushString(thread, paramName);
			r = VM_ThrowErrorOfType(thread, Types::ArgumentRangeError, 1);
		}
	}
	return r;
}

AVES_API BEGIN_NATIVE_FUNCTION(aves_Console_get_cursorX)
{
	COORD pos;
	CHECKED(Console::GetCursorPosition(thread, pos));
	VM_PushInt(thread, pos.X);
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_Console_set_cursorX)
{
	CHECKED(AssertValidCoord(thread, args, strings::value));

	COORD pos;
	CHECKED(Console::GetCursorPosition(thread, pos));
	Console::SetCursorPosition(thread,
		(SHORT)args[0].v.integer, pos.Y);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_Console_get_cursorY)
{
	COORD pos;
	CHECKED(Console::GetCursorPosition(thread, pos));
	VM_PushInt(thread, pos.Y);
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_Console_set_cursorY)
{
	CHECKED(AssertValidCoord(thread, args, strings::value));

	COORD pos;
	CHECKED(Console::GetCursorPosition(thread, pos));
	Console::SetCursorPosition(thread,
		pos.X, (SHORT)args[0].v.integer);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_Console_setCursorPosition)
{
	CHECKED(AssertValidCoord(thread, args + 0, strings::x));
	CHECKED(AssertValidCoord(thread, args + 1, strings::y));

	Console::SetCursorPosition(thread, (SHORT)args[0].v.integer, (SHORT)args[1].v.integer);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_Console_get_bufferWidth)
{
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	CHECKED(Console::GetBufferInfo(thread, csbi));
	VM_PushInt(thread, csbi.dwSize.X);
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_Console_get_bufferHeight)
{
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	CHECKED(Console::GetBufferInfo(thread, csbi));
	VM_PushInt(thread, csbi.dwSize.Y);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_Console_get_windowWidth)
{
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	CHECKED(Console::GetBufferInfo(thread, csbi));
	VM_PushInt(thread, csbi.srWindow.Right - csbi.srWindow.Left + 1);
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_Console_get_windowHeight)
{
	CONSOLE_SCREEN_BUFFER_INFO csbi;
	CHECKED(Console::GetBufferInfo(thread, csbi));
	VM_PushInt(thread, csbi.srWindow.Bottom - csbi.srWindow.Top + 1);
}
END_NATIVE_FUNCTION

AVES_API BEGIN_NATIVE_FUNCTION(aves_Console_setBufferSize)
{
	CHECKED(IntFromValue(thread, args + 0));
	CHECKED(IntFromValue(thread, args + 1));
	int64_t width  = args[0].v.integer;
	int64_t height = args[1].v.integer;

	CONSOLE_SCREEN_BUFFER_INFO csbi;
	CHECKED(Console::GetBufferInfo(thread, csbi));
	// Make sure the new buffer is not smaller than the window
	if (width < csbi.srWindow.Right + 1 || width >= SHRT_MAX)
	{
		VM_PushString(thread, strings::width);
		return VM_ThrowErrorOfType(thread, Types::ArgumentRangeError, 1);
	}
	if (height < csbi.srWindow.Bottom + 1 || height >= SHRT_MAX)
	{
		VM_PushString(thread, strings::height);
		return VM_ThrowErrorOfType(thread, Types::ArgumentRangeError, 1);
	}

	COORD size = { (SHORT)width, (SHORT)height };
	BOOL success = SetConsoleScreenBufferSize(Console::StdOut, size);
	if (!success)
		return Console::ThrowConsoleError(thread);
}
END_NATIVE_FUNCTION
AVES_API BEGIN_NATIVE_FUNCTION(aves_Console_setWindowSize)
{
	CHECKED(IntFromValue(thread, args + 0));
	CHECKED(IntFromValue(thread, args + 1));
	int64_t width64  = args[0].v.integer;
	int64_t height64 = args[1].v.integer;

	if (width64 < 0 || width64 > INT32_MAX)
	{
		VM_PushString(thread, strings::width);
		return VM_ThrowErrorOfType(thread, Types::ArgumentRangeError, 1);
	}
	if (height64 < 0 || height64 > INT32_MAX)
	{
		VM_PushString(thread, strings::height);
		return VM_ThrowErrorOfType(thread, Types::ArgumentRangeError, 1);
	}

	int32_t width  = (int32_t)width64;
	int32_t height = (int32_t)height64;

	CONSOLE_SCREEN_BUFFER_INFO csbi;
	CHECKED(Console::GetBufferInfo(thread, csbi));
	BOOL success;

	// If the new window size is too small for the buffer,
	// resize the buffer. Maintain relative window position.
	bool resizeBuffer = false;
	COORD newBufferSize = csbi.dwSize;
	if (csbi.srWindow.Left + width >= csbi.dwSize.X)
	{
		if (csbi.srWindow.Left >= SHRT_MAX - width)
		{
			VM_PushString(thread, strings::width);
			return VM_ThrowErrorOfType(thread, Types::ArgumentRangeError, 1);
		}
		newBufferSize.X = csbi.srWindow.Left + width;
		resizeBuffer = true;
	}
	if (csbi.srWindow.Top + height >= csbi.dwSize.Y)
	{
		if (csbi.srWindow.Top >= SHRT_MAX - height)
		{
			VM_PushString(thread, strings::height);
			return VM_ThrowErrorOfType(thread, Types::ArgumentRangeError, 1);
		}
		newBufferSize.Y = csbi.srWindow.Top + height;
		resizeBuffer = true;
	}

	if (resizeBuffer)
	{
		success = SetConsoleScreenBufferSize(Console::StdOut, newBufferSize);
		if (!success)
			return Console::ThrowConsoleError(thread);
	}

	SMALL_RECT window = csbi.srWindow;
	window.Right = window.Left + width - 1;
	window.Bottom = window.Top + height - 1;

	success = SetConsoleWindowInfo(Console::StdOut, true, &window);
	if (!success)
	{
		// Restore previous buffer size!
		if (resizeBuffer)
			SetConsoleScreenBufferSize(Console::StdOut, csbi.dwSize);
		return Console::ThrowConsoleError(thread);
	}
}
END_NATIVE_FUNCTION

int Console::ThrowConsoleError(ThreadHandle thread)
{
	if (VM_IsInUnmanagedRegion(thread))
		VM_LeaveUnmanagedRegion(thread);
	return VM_ThrowError(thread, ConsoleIOError.AsString());
}