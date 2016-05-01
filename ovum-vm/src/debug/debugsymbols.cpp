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
		using namespace std;

		try
		{
			ModuleReader reader(module->GetVM());
			PathName fileName(moduleFile);
			fileName.Append(OVUM_PATH(".dbg"));
			reader.Open(fileName);

			char magicNumber[4];
			reader.Read(magicNumber, 4);

			for (int i = 0; i < 4; i++)
				if (magicNumber[i] != debug_file::magicNumber[i])
					return;

			unique_ptr<ModuleDebugData> output(new ModuleDebugData());
			module->debugData = output.get(); // So that the GC can reach it

			ReadSourceFiles(reader, output.get());
			ReadMethodSymbols(reader, module, output.get());

			output->AttachSymbols();
			output.release();
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

	void ModuleDebugData::ReadSourceFiles(ModuleReader &reader, ModuleDebugData *target)
	{
		using namespace std;

		uint32_t size = reader.ReadUInt32();
		if (size != 0)
		{
			int32_t length = reader.ReadInt32();
			target->files = new SourceFile[length];

			for (int32_t i = 0; i < length; i++)
			{
				SourceFile *file = target->files + i;
				file->fileName = reader.ReadString();
				reader.Read(file->hash, 20);
				target->fileCount++;
			}
		}
	}

	void ModuleDebugData::ReadMethodSymbols(ModuleReader &reader, Module *module, ModuleDebugData *target)
	{
		using namespace std;

		int32_t totalCount = reader.ReadInt32(); // DebugSymbols.totalOverloadsWithSymbols
		uint32_t methodsSize = reader.ReadUInt32(); // DebugSymbols.methodSymbols.size
		if (methodsSize != 0)
		{
			target->symbols = new DebugSymbols*[totalCount];

			int32_t methodsLength = reader.ReadInt32(); // DebugSymbols.methodSymbols.length
			for (int32_t i = 0; i < methodsLength; i++)
			{
				TokenId methodId = reader.ReadToken(); // MethodSymbols.methodId
				uint32_t overloadsSize = reader.ReadUInt32(); // overloads.size

				// The token must be a functiondef or methoddef; if it isn't,
				// we ignore this method's symbols.
				// If it is one of those, then it must map to an actual method.
				// Otherwise we also ignore things.
				Method *method;
				if ((methodId & IDMASK_FUNCTIONDEF) != IDMASK_FUNCTIONDEF &&
					(methodId & IDMASK_METHODDEF) != IDMASK_METHODDEF
					||
					(method = module->FindMethod(methodId)) == nullptr)
				{
					reader.Seek(overloadsSize, os::FILE_SEEK_CURRENT);
					continue;
				}
				if (overloadsSize == 0)
					continue;

				int32_t overloadsLength = reader.ReadInt32(); // overloads.length
				if (overloadsLength != method->overloadCount)
				{
					// -4 for the length
					reader.Seek(overloadsSize - 4, os::FILE_SEEK_CURRENT);
					continue;
				}

				for (int32_t k = 0; k < overloadsLength; k++)
				{
					uint32_t symbolCount = reader.ReadUInt32(); // OverloadSymbols.count
					if (symbolCount == 0)
						continue;

					unique_ptr<SourceLocation[]> symbols(new SourceLocation[symbolCount]);
					SourceLocation *symp = symbols.get();
					for (uint32_t s = 0; s < symbolCount; s++, symp++)
						ReadSourceLocation(reader, target, symp);

					unique_ptr<DebugSymbols> debugSymbols(new DebugSymbols(method->overloads + k, symbolCount, symbols.get()));
					symbols.release();

					target->symbols[target->symbolCount++] = debugSymbols.release();
				}
			}
		}
	}

	void ModuleDebugData::ReadSourceLocation(ModuleReader &reader, ModuleDebugData *data, SourceLocation *location)
	{
		location->startOffset      = reader.ReadUInt32();
		location->endOffset        = reader.ReadUInt32();

		int32_t fileIndex          = reader.ReadInt32();
		location->file             = data->files + fileIndex;
		location->lineNumber       = reader.ReadInt32();
		location->column           = reader.ReadInt32();

		location->sourceStartIndex = reader.ReadInt32();
		location->sourceEndIndex   = reader.ReadInt32();
	}
} // namespace debug

} // namespace ovum
