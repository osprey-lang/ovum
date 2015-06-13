#ifndef VM__MODULEPOOL_H
#define VM__MODULEPOOL_H

#include "../vm.h"

namespace ovum
{

class ModulePool
{
private:
	int capacity;
	int length;
	Module **data;

public:
	ModulePool();
	ModulePool(int capacity);
	~ModulePool();

	inline int GetLength() const
	{
		return length;
	}

	inline Module *Get(int index) const
	{
		return data[index];
	}
	Module *Get(String *name) const;
	Module *Get(String *name, ModuleVersion *version) const;

	inline void Set(int index, Module *value)
	{
		data[index] = value;
	}

	int Add(Module *value);

	bool Remove(Module *value);

private:
	void Init(int capacity);

	void Resize();
};

} // namespace ovum

#endif // VM__MODULEPOOL_H