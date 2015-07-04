#ifndef VM__MODULEFINDER_H
#define VM__MODULEFINDER_H

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
	VM *vm;

public:
	ModuleFinder(VM *vm);

	bool FindModulePath(String *module, ModuleVersion *version, PathName &result);

private:
	bool SearchDirectory(const PathName *dir, String *module, const PathName &version, PathName &result);

	void AppendVersionString(PathName &dest, ModuleVersion *version);

	static const uint32_t MODULE_PATH_CAPACITY = 256;
	static const uint32_t VERSION_NUMBER_CAPACITY = 32;
	// File extension for module files; currently ".ovm"
	static const pathchar_t *const EXTENSION;
	// Separator between module name and version number; currently "-"
	static const pathchar_t *const VERSION_SEPARATOR;
};

} // namespace ovum

#endif // VM__MODULEFINDER_H
