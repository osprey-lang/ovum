#pragma once

#include "def.h"
#include "../../vm.h"

// Note: The functions exported by this header file depend on pathchar_t,
// which is defined in Ovum's public headers. If you require a special
// definition of pathchar_t, modify ov_pathchar.h.

namespace ovum
{

namespace os
{

	typedef ... LibraryHandle;

	// OSes may not make all of these status codes available. Portable code
	// should always handle LIBRARY_ERROR as a fallback.
	enum LibraryStatus
	{
		// The library was loaded correctly. The handle contains a valid
		// and usable instance of the library.
		LIBRARY_OK = 0,
		// The library could not be opened or closed for some unknown reason.
		// This status code should only be used when no other is available.
		LIBRARY_ERROR = 1,
		// The library file could not be located.
		LIBRARY_FILE_NOT_FOUND = 2,
		// Access to the library file was denied.
		LIBRARY_ACCESS_DENIED = 3,
		// The library file contents could not be understood by the OS.
		LIBRARY_BAD_IMAGE = 4,
		// One or more dependencies could not be loaded.
		LIBRARY_MISSING_DEPENDENCY = 5,
	};

	// Opens a shared library from the given path. The path name should be
	// fully qualified, due to unfortunate differences between OSes. If the
	// library is already open, this function will generally not reopen it,
	// but simply increment the reference count. Typically, the only effect
	// of this is that static initializers are not re-run.
	//
	// This function always opens a library with the purpose of executing
	// code in it, never for loading resources or the like. As a result, if
	// the library has dependencies, they will be loaded as well.
	//
	//   path:
	//     The path name of the library file to open.
	//   output:
	//     Receives a handle to the newly opened library.
	// Returns:
	//   A status code which indicates whether an error occurred. If the
	//   library was opened successfully, LIBRARY_OK is returned.
	LibraryStatus OpenLibrary(const pathchar_t *path, LibraryHandle *output);

	// Closes a shared library handle. The handle should not be used after
	// closing it. If the specified handle contains the last reference to the
	// library, the OS will usually unload it; opening the library again will
	// cause static initializers to be re-run.
	//   library:
	//     The library to close.
	// Returns:
	//   A status code which indicates whether an error occurred. If the
	//   library was closed successfully, LIBRARY_OK is returned.
	LibraryStatus CloseLibrary(LibraryHandle *library);]

	// Determines whether the specified library handle is (probably) valid;
	// that is, it refers to an open library. This function is essentially
	// used to test whether a library handle still has the default value.
	//   library:
	//     The handle to test.
	// Returns:
	//   True if the handle appears to be an open library; otherwise, false.
	bool LibraryHandleIsValid(LibraryHandle *library);

	// Locates the address of a function (entry point) in the library. If
	// no entry point with the specified name is exported, this function
	// returns null.
	//
	// Note: The encoding of the name is not specified. These APIs are
	// intended to be used by the module loader, which reads native entry
	// point names directly from the module file as a sequence of bytes.
	// If the OS requires ASCII, UTF-8 or some other encoding, it is up to
	// the compiler that produced the module to arrange that.
	//   library:
	//     The library to find an entry point in.
	//   name:
	//     The name of the entry point.
	// Returns:
	//   A non-null address if the entry point is found; otherwise, null.
	void *FindLibraryFunction(LibraryHandle *library, const char *name);

} // namespace os

} // namespace ovum
