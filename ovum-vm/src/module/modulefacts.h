#pragma once

#include "../vm.h"
#include "modulefile.h"

namespace ovum
{

namespace module_file
{

	// The magic number that we expect to find in every single module file.
	extern const MagicNumber ExpectedMagicNumber;

	// The minimum supported file format version
	static const uint32_t MinFileFormatVersion = 0x00000100u;

	// The maximum supported file format version
	static const uint32_t MaxFileFormatVersion = 0x00000100u;

	// The name of the native module initialization routine, which is run when
	// the module has just finished loading.
	extern const char *NativeModuleIniterName;

	// The function pointer type of the native module initialization routine.
	typedef void (OVUM_CDECL *NativeModuleMain)(ModuleHandle module);

} // namespace module_file

} // namespace ovum