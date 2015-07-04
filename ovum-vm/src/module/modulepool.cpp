#include "modulepool.h"
#include "module.h"
#include "../../inc/ov_string.h"

namespace ovum
{

ModulePool::ModulePool() :
	capacity(0), length(0), data(nullptr)
{
	Init(0);
}
ModulePool::ModulePool(int capacity) :
	capacity(0), length(0), data(nullptr)
{
	Init(capacity);
}
ModulePool::~ModulePool()
{
	for (int i = 0; i < length; i++)
		delete data[i];
	delete[] data;
}

void ModulePool::Init(int capacity)
{
	capacity = max(capacity, 4);

	data = new Module*[capacity];
	memset(data, 0, sizeof(Module*) * capacity);

	this->capacity = capacity;
}

Module *ModulePool::Get(String *name) const
{
	for (int i = 0; i < length; i++)
		if (String_Equals(data[i]->name, name))
			return data[i];
	return nullptr;
}
Module *ModulePool::Get(String *name, ModuleVersion *version) const
{
	for (int i = 0; i < length; i++)
	{
		Module *module = data[i];
		if (String_Equals(module->name, name) && module->version == *version)
			return module;
	}
	return nullptr;
}

int ModulePool::Add(Module *value)
{
	if (length == capacity)
		Resize();
	int index = length++;
	data[index] = value;
	return index;
}

bool ModulePool::Remove(Module *value)
{
	bool found = false;
	for (int i = 0; i < length; i++)
	{
		if (found)
			data[i - 1] = data[i];
		else
			found = data[i] == value;
	}
	if (found)
		length--;
	return found;
}

void ModulePool::Resize()
{
	int newCap = capacity * 2;

	Module **newData = new Module*[newCap];
	CopyMemoryT(newData, this->data, capacity);

	capacity = newCap;
	delete[] this->data;
	this->data = newData;
}

}
