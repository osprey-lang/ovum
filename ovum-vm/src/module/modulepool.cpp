#include "modulepool.h"
#include "module.h"
#include "../../inc/ovum_string.h"

namespace ovum
{

Box<ModulePool> ModulePool::New(int capacity)
{
	Box<ModulePool> pool(new ModulePool(capacity));
	return std::move(pool);
}

ModulePool::ModulePool(int capacity) :
	capacity(0),
	length(0),
	data(nullptr)
{
	Init(capacity);
}

void ModulePool::Init(int capacity)
{
	capacity = max(capacity, 4);

	data = Box<Box<Module>[]>(new Box<Module>[capacity]);

	this->capacity = capacity;
}

Module *ModulePool::Get(String *name) const
{
	for (int i = 0; i < length; i++)
		if (String_Equals(data[i]->name, name))
			return data[i].get();
	return nullptr;
}
Module *ModulePool::Get(String *name, ModuleVersion *version) const
{
	for (int i = 0; i < length; i++)
	{
		Module *module = data[i].get();
		if (String_Equals(module->name, name) && module->version == *version)
			return module;
	}
	return nullptr;
}

int ModulePool::Add(Box<Module> value)
{
	if (length == capacity)
		Resize();

	int index = length++;
	data[index] = std::move(value);
	return index;
}

Box<Module> ModulePool::Remove(Module *value)
{
	Box<Module> foundModule;
	for (int i = 0; i < length; i++)
	{
		if (foundModule)
			data[i - 1] = std::move(data[i]);
		else if (data[i].get() == value)
			foundModule = std::move(data[i]);
	}
	if (foundModule)
		length--;
	return std::move(foundModule);
}

void ModulePool::Resize()
{
	int newCap = capacity * 2;

	Box<Box<Module>[]> newData = Box<Box<Module>[]>(new Box<Module>[newCap]);
	for (int i = 0; i < length; i++)
		newData[i] = std::move(data[i]);

	capacity = newCap;
	this->data = std::move(newData);
}

PartiallyOpenedModulesList::PartiallyOpenedModulesList() :
	modules()
{ }

void PartiallyOpenedModulesList::Add(Module *module)
{
	modules.push_back(module);
}

void PartiallyOpenedModulesList::Remove(Module *module)
{
	for (auto i = modules.begin(); i != modules.end(); ++i)
	{
		if (*i == module)
		{
			modules.erase(i);
			break;
		}
	}
}

bool PartiallyOpenedModulesList::Contains(String *name, ModuleVersion *version) const
{
	for (auto module : modules)
	{
		if (String_Equals(module->GetName(), name) && module->GetVersion() == *version)
			return true;
	}

	return false;
}

} // namespace ovum
