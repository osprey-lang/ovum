#pragma once

#include "../vm.h"
#include "../util/pathname.h"
#include "../../inc/ov_module.h"

namespace ovum
{

// The ModuleFinder class is responsible for locating module files. When starting
// the VM, only the startup module is given an explicit path. All other modules
// must be located in the file system. All dependencies have a required version,
// and a module can be loaded from a versioned or an unversioned path.
//
// There are several directories within which we look for modules, in order:
//  * The 'lib' folder within the startup folder (that is, the folder that the
//    startup module lives in).
//  * The startup folder (VM::startupPath).
//  * The module library folder (VM::modulePath), which is specified as an argument
//    to VM_Start.
//
// Within each folder, we examine the following paths, in order:
//    $dir/$name-$version/$name.ovm
//    $dir/$name-$version.ovm
//    $dir/$name/$name.ovm
//    $dir/$name.ovm
// where
//    $dir = the directory name
//    $name = the full name of the module (e.g. "osprey.compiler")
//    $version = the required version, in the format "major.minor.build.revision",
//      e.g. "8.4.7.2"
//
// As soon as a file is found in one of the possible paths, we stop looking and
// use that path.
class ModuleFinder
{
private:
	static const int SEARCH_DIR_COUNT = 3;

	VM *vm;
	const PathName *searchDirs[SEARCH_DIR_COUNT];

public:
	ModuleFinder(VM *vm);

	// Gets the total number of directories that will be searched for modules.
	// The module finder tries several paths within each directory; see the
	// class documentation.
	inline int GetSearchDirectoryCount() const
	{
		return SEARCH_DIR_COUNT;
	}

	// Gets the directories that will be searched for modules. The module finder
	// tries several paths within each directory; see the class documentation.
	//
	// The paths are returned in the order they are searched.
	//
	// Parameters:
	//   resultSize:
	//     The size of the result buffer. At most this number of values are written
	//     into the buffer.
	//   result:
	//     The result buffer, which receives one PathName for each search directory.
	//     These are managed by the VM; the caller should not deallocate them. This
	//     parameter cannot be null.
	// Returns:
	//   The total number of search directories. If the buffer is too small, the
	//   return value will be larger than resultSize.
	int GetSearchDirectories(int resultSize, const PathName **result) const;

	bool FindModulePath(String *module, ModuleVersion *version, PathName &result) const;

private:
	void InitSearchDirectories();

	bool SearchDirectory(const PathName *dir, String *module, const PathName &version, PathName &result) const;

	void AppendVersionString(PathName &dest, ModuleVersion *version) const;

	static const uint32_t MODULE_PATH_CAPACITY = 256;
	static const uint32_t VERSION_NUMBER_CAPACITY = 32;
	// File extension for module files; currently ".ovm"
	static const pathchar_t *const EXTENSION;
	// Separator between module name and version number; currently "-"
	static const pathchar_t *const VERSION_SEPARATOR;
};

} // namespace ovum
