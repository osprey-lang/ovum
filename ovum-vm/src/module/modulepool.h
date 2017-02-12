#pragma once

#include "../vm.h"
#include "../../inc/ovum_module.h"
#include <vector>

namespace ovum
{

class ModulePool
{
public:
	static Box<ModulePool> New(size_t capacity);

	inline size_t GetLength() const
	{
		return length;
	}

	inline Module *Get(int index) const
	{
		return data[index].get();
	}
	Module *Get(String *name) const;
	Module *Get(String *name, ModuleVersion *version) const;

	inline void Set(size_t index, Box<Module> value)
	{
		data[index] = std::move(value);
	}

	size_t Add(Box<Module> value);

	Box<Module> Remove(Module *value);

private:
	OVUM_DISABLE_COPY_AND_ASSIGN(ModulePool);

	size_t capacity;
	size_t length;
	Box<Box<Module>[]> data;

	explicit ModulePool(size_t capacity);

	void Init(size_t capacity);

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
