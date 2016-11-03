#pragma once

#include "../vm.h"
#include "../../inc/ovum_module.h"
#include <vector>

namespace ovum
{

class ModulePool
{
public:
	static Box<ModulePool> New(int capacity);

	inline int GetLength() const
	{
		return length;
	}

	inline Module *Get(int index) const
	{
		return data[index].get();
	}
	Module *Get(String *name) const;
	Module *Get(String *name, ModuleVersion *version) const;

	inline void Set(int index, Box<Module> value)
	{
		data[index] = std::move(value);
	}

	int Add(Box<Module> value);

	Box<Module> Remove(Module *value);

private:
	OVUM_DISABLE_COPY_AND_ASSIGN(ModulePool);

	int capacity;
	int length;
	Box<Box<Module>[]> data;

	ModulePool(int capacity);

	void Init(int capacity);

	void Resize();
};

// Contains a list of modules that are partially opened; that is, when a Module
// object has been constructed, but not all members have been read. This class
// exists so that circular references can be detected.
class PartiallyOpenedModulesList
{
public:
	PartiallyOpenedModulesList();

	void Add(Module *module);

	void Remove(Module *module);

	bool Contains(String *name, ModuleVersion *version) const;

private:
	OVUM_DISABLE_COPY_AND_ASSIGN(PartiallyOpenedModulesList);

	std::vector<Module*> modules;
};

} // namespace ovum
