#include "debugsymbols.h"
#include "../object/method.h"
#include "../module/module.h"
#include "../module/modulereader.h"
#include "../util/pathname.h"

namespace ovum
{

namespace debug_file
{
	const char magicNumber[] = { 'O', 'V', 'D', 'S' };
}

namespace debug
{
	DebugSymbols::DebugSymbols(MethodOverload *overload, int32_t symbolCount, SourceLocation *symbols)
		: overload(overload), symbolCount(symbolCount), symbols(symbols)
	{ }

	DebugSymbols::~DebugSymbols()
	{
		if (symbols != nullptr)
		{
			delete[] symbols;
			symbolCount = 0;
			symbols = nullptr;
		}
	}

	SourceLocation *DebugSymbols::FindSymbol(uint32_t offset)
	{
		if (symbols == nullptr)
			return nullptr;

		int32_t imin = 0, imax = symbolCount - 1;
		while (imax >= imin)
		{
			int32_t i = imin + (imax - imin) / 2;
			SourceLocation *loc = symbols + i;
			if (offset < loc->startOffset)
				imax = i - 1;
			else if (offset >= loc->endOffset)
				imin = i + 1;
			else
				return loc;
		}

		return nullptr;
	}


	ModuleDebugData::ModuleDebugData()
		: fileCount(0), files(nullptr), symbolCount(0), symbols(nullptr)
	{ }

	ModuleDebugData::~ModuleDebugData()
	{
		if (files != nullptr)
			delete files;

		for (int32_t i = 0; i != symbolCount; i++)
			delete symbols[i];
		delete[] symbols;
	}

	void ModuleDebugData::TryLoad(const PathName &moduleFile, Module *module)
	{
		PathName fileName(moduleFile);
		fileName.Append(OVUM_PATH(".dbg"));

		try
		{
			ModuleReader reader(module->GetVM());
			reader.Open(fileName);

			throw ModuleLoadException(fileName, "Not implemented");
		}
		catch (ModuleIOException&)
		{
			// Ignore error; reset and return
			module->debugData = nullptr;
		}
	}

	void ModuleDebugData::AttachSymbols()
	{
		for (int32_t i = 0; i < symbolCount; i++)
		{
			DebugSymbols *d = symbols[i];
			d->overload->debugSymbols = d;
		}
	}
} // namespace debug

} // namespace ovum
