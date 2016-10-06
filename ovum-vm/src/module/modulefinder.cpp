#include "modulefinder.h"
#include "../ee/vm.h"
#include "../util/stringformatters.h"

namespace ovum
{

const pathchar_t *const ModuleFinder::EXTENSION = OVUM_PATH(".ovm");
const pathchar_t *const ModuleFinder::VERSION_SEPARATOR = OVUM_PATH("-");

ModuleFinder::ModuleFinder(VM *vm) :
	vm(vm)
{
	InitSearchDirectories();
}

void ModuleFinder::InitSearchDirectories()
{
	const PathName **searchDirs = this->searchDirs;
	*searchDirs++ = vm->GetStartupPathLib();
	*searchDirs++ = vm->GetStartupPath();
	*searchDirs++ = vm->GetModulePath();
}

int ModuleFinder::GetSearchDirectories(int resultSize, const PathName **result) const
{
	int count = min(resultSize, SEARCH_DIR_COUNT);

	CopyMemoryT(result, searchDirs, (size_t)resultSize);

	return SEARCH_DIR_COUNT;
}

bool ModuleFinder::FindModulePath(String *module, ModuleVersion *version, PathName &result) const
{
	PathName versionNumber(VERSION_NUMBER_CAPACITY);
	if (version != nullptr)
		AppendVersionString(versionNumber, version);

	PathName modulePath(MODULE_PATH_CAPACITY);

	bool found = false;
	for (int i = 0; i < SEARCH_DIR_COUNT; i++)
	{
		found = SearchDirectory(searchDirs[i], module, versionNumber, modulePath);
		if (found)
			break;
	}

	if (found)
		result.ReplaceWith(modulePath);
	return found;
}

bool ModuleFinder::SearchDirectory(const PathName *dir, String *module, const PathName &version, PathName &result) const
{
	result.ReplaceWith(*dir);
	uint32_t simpleName = result.Join(module);
	// Versioned names first:
	//    dir/$name-$version/$name.ovm
	//    dir/$name-$version.ovm
	if (version.GetLength() > 0)
	{
		result.Append(VERSION_SEPARATOR);
		// The length for dir/$name-$version
		uint32_t versionedName = result.Append(version);

		// dir/$name-version/$name.ovm
		result.Join(module);
		result.Append(EXTENSION);
		if (os::FileExists(result.GetDataPointer()))
			return true;

		// dir/$name-$version.ovm
		result.ClipTo(0, versionedName);
		result.Append(EXTENSION);
		if (os::FileExists(result.GetDataPointer()))
			return true;
	}

	// Then, unversioned names:
	//    dir/$name/$name.ovm
	//    dir/$name.ovm
	// simpleName contains the length for path/$name

	// dir/$name/$name.ovm
	result.ClipTo(0, simpleName);
	result.Join(module);
	result.Append(EXTENSION);
	if (os::FileExists(result.GetDataPointer()))
		return true;

	// dir/$name.ovm
	result.ClipTo(0, simpleName);
	result.Append(EXTENSION);
	if (os::FileExists(result.GetDataPointer()))
		return true;

	return false;
}

void ModuleFinder::AppendVersionString(PathName &dest, ModuleVersion *version) const
{
	static const int32_t BUFFER_SIZE = 16;
	ovchar_t buffer[BUFFER_SIZE];
	int32_t length;

	length = IntFormatter::ToDec(version->major, buffer, BUFFER_SIZE);
	dest.Append(length, buffer);

	dest.Append(OVUM_PATH("."));

	length = IntFormatter::ToDec(version->minor, buffer, BUFFER_SIZE);
	dest.Append(length, buffer);

	dest.Append(OVUM_PATH("."));

	length = IntFormatter::ToDec(version->patch, buffer, BUFFER_SIZE);
	dest.Append(length, buffer);
}

} // namespace ovum
