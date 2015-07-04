#ifndef AVES__CONSOLE_H
#define AVES__CONSOLE_H

#include "../aves.h"

AVES_API void aves_Console_init(TypeHandle type);

AVES_API NATIVE_FUNCTION(aves_Console_write);
AVES_API NATIVE_FUNCTION(aves_Console_writeLine);
AVES_API NATIVE_FUNCTION(aves_Console_writeErr);
AVES_API NATIVE_FUNCTION(aves_Console_writeErrLine);

AVES_API NATIVE_FUNCTION(aves_Console_readKey);
AVES_API NATIVE_FUNCTION(aves_Console_readChar);
AVES_API NATIVE_FUNCTION(aves_Console_readLine);

AVES_API NATIVE_FUNCTION(aves_Console_clear);

AVES_API NATIVE_FUNCTION(aves_Console_get_textColor);
AVES_API NATIVE_FUNCTION(aves_Console_set_textColor);
AVES_API NATIVE_FUNCTION(aves_Console_get_backColor);
AVES_API NATIVE_FUNCTION(aves_Console_set_backColor);
AVES_API NATIVE_FUNCTION(aves_Console_setColors);
AVES_API NATIVE_FUNCTION(aves_Console_resetColors);

AVES_API NATIVE_FUNCTION(aves_Console_get_showCursor);
AVES_API NATIVE_FUNCTION(aves_Console_set_showCursor);

AVES_API NATIVE_FUNCTION(aves_Console_get_cursorX);
AVES_API NATIVE_FUNCTION(aves_Console_set_cursorX);

AVES_API NATIVE_FUNCTION(aves_Console_get_cursorY);
AVES_API NATIVE_FUNCTION(aves_Console_set_cursorY);

AVES_API NATIVE_FUNCTION(aves_Console_setCursorPosition);

AVES_API NATIVE_FUNCTION(aves_Console_get_bufferWidth);
AVES_API NATIVE_FUNCTION(aves_Console_get_bufferHeight);

AVES_API NATIVE_FUNCTION(aves_Console_get_windowWidth);
AVES_API NATIVE_FUNCTION(aves_Console_get_windowHeight);

AVES_API NATIVE_FUNCTION(aves_Console_setBufferSize);
AVES_API NATIVE_FUNCTION(aves_Console_setWindowSize);

#endif // AVES__CONSOLE_H
