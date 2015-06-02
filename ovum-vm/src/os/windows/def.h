#ifndef VM__OS_DEF_H
#define VM__OS_DEF_H

#include "../windows.h"

// File system
#include "filesystem.h"

// Memory management
#include "mem.h"

// (Multi)threading and related
#include "threading.h"

// Memory-mapped files (relies on types from filesystem.h)
#include "mmf.h"

// Dynamic/shared libraries
#include "dl.h"

#endif // VM__OS_DEF_H