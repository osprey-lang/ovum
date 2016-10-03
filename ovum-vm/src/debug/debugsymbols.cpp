#include "debugsymbols.h"
#include "../gc/gc.h"
#include "../object/method.h"
#include "../module/module.h"
#include "../module/modulereader.h"
#include "../util/pathname.h"

// For convenience only
namespace mf = ovum::module_file;
namespace df = ovum::debug_file;

namespace ovum
{

namespace debug_file
{
	static const module_file::MagicNumber ExpectedMagicNumber = { 'O', 'V', 'D', 'S' };
}

namespace debug
{
	OverloadSymbols::OverloadSymbols(MethodSymbols *parent, MethodOverload *overload, int32_t symbolCount, std::unique_ptr<DebugSymbol[]> symbols) :
		parent(parent),
		overload(overload),
		symbolCount(symbolCount),
		symbols(std::move(symbols))
	{ }

	DebugSymbol *OverloadSymbols::FindSymbol(uint32_t offset) const
	{
		DebugSymbol *symbols = this->symbols.get();

		int32_t imin = 0, imax = symbolCount - 1;
		while (imax >= imin)
		{
			int32_t i = imin + (imax - imin) / 2;
			DebugSymbol *loc = symbols + i;
			if (offset < loc->startOffset)
				imax = i - 1;
			else if (offset >= loc->endOffset)
				imin = i + 1;
			else
				return loc;
		}

		return nullptr;
	}

	MethodSymbols::MethodSymbols(Method *method) :
		method(method),
		overloadCount(0),
		overloads()
	{ }

	void MethodSymbols::SetOverloads(int32_t count, std::unique_ptr<std::unique_ptr<OverloadSymbols>[]> overloads)
	{
		this->overloadCount = count;
		this->overloads = std::move(overloads);
	}

	ModuleDebugData::ModuleDebugData() :
		fileCount(0),
		files(),
		methodSymbolCount(0),
		methodSymbols()
	{ }

	void ModuleDebugData::TryLoad(const PathName &moduleFile, Module *module)
	{
		PathName fileName(moduleFile);
		fileName.Append(OVUM_PATH(".dbg"));

		try
		{
			DebugSymbolsReader reader(module->GetVM());
			reader.Open(fileName);

			reader.ReadDebugSymbols(module);

			throw ModuleLoadException(fileName, "Not implemented");
		}
		catch (ModuleLoadException&)
		{
			// Ignore error; reset and return
			module->debugData = nullptr;
		}
		catch (ModuleIOException&)
		{
			// Ignore error; reset and return
			module->debugData = nullptr;
		}
	}

	DebugSymbolsReader::DebugSymbolsReader(VM *vm) :
		file(),
		vm(vm)
	{ }

	DebugSymbolsReader::~DebugSymbolsReader()
	{ }

	void DebugSymbolsReader::Open(const pathchar_t *fileName)
	{
		file.Open(fileName);
	}

	void DebugSymbolsReader::Open(const PathName &fileName)
	{
		Open(fileName.GetDataPointer());
	}

	void DebugSymbolsReader::ReadDebugSymbols(Module *module)
	{
		const df::DebugSymbolsHeader *header = file.Read<df::DebugSymbolsHeader>(0);
		VerifyHeader(header);

		std::unique_ptr<ModuleDebugData> output(new ModuleDebugData());
		// Assign the unfinished data to the module already, so that the GC
		// can reach it if it has to.
		module->debugData = output.get();

		ReadSourceFiles(output.get(), file.Deref(header->sourceFiles));
		ReadMethodSymbols(module, output.get(), header);

		// Success! Now that we know we've successfully read all the symbols,
		// we can attach them to their respective overloads.
		AttachSymbols(output.get());

		// And now let's make sure we don't delete our progress.
		output.release();
	}

	void DebugSymbolsReader::ReadSourceFiles(ModuleDebugData *data, const df::SourceFileList *list)
	{
		int32_t count = list->fileCount;

		// Give the debug data the file list immediately, so the GC can find
		// the file name strings if it has to.
		data->fileCount = count;
		data->files.reset(new SourceFile[count]);
		memset(data->files.get(), 0, sizeof(SourceFile) * count);

		const mf::Rva<df::SourceFile> *defRvas = list->files.Get();
		for (int32_t i = 0; i < count; i++)
		{
			mf::Rva<df::SourceFile> defRva = defRvas[i];
			const df::SourceFile *def = file.Deref(defRva);

			SourceFile *file = data->files.get() + i;

			file->fileName = ReadString(&def->fileName);
			CopyMemoryT(file->hash, def->hash, df::Sha1HashSize);
		}
	}

	void DebugSymbolsReader::ReadMethodSymbols(Module *module, ModuleDebugData *data, const df::DebugSymbolsHeader *header)
	{
		int32_t count = header->methodSymbolCount;

		std::unique_ptr<std::unique_ptr<MethodSymbols>[]> methodSymbols(new std::unique_ptr<MethodSymbols>[count]);

		const mf::Rva<df::MethodSymbols> *defRvas = header->methodSymbols.Get();
		for (int32_t i = 0; i < count; i++)
		{
			mf::Rva<df::MethodSymbols> defRva = defRvas[i];
			const df::MethodSymbols *def = file.Deref(defRva);

			methodSymbols[i] = ReadSingleMethodSymbols(data, module, def);
		}

		data->methodSymbolCount = count;
		data->methodSymbols = std::move(methodSymbols);
	}

	std::unique_ptr<MethodSymbols> DebugSymbolsReader::ReadSingleMethodSymbols(
		ModuleDebugData *data,
		Module *module,
		const df::MethodSymbols *symbols
	)
	{
		Method *method = module->FindMethod(symbols->memberToken);
		if (method == nullptr)
			ModuleLoadError("Unresolved method token in debug symbols file.");
		if (method->declModule != module)
			ModuleLoadError("Method belongs to the wrong module.");

		std::unique_ptr<MethodSymbols> methodSymbols(new MethodSymbols(method));

		int32_t count = symbols->overloadCount;
		std::unique_ptr<std::unique_ptr<OverloadSymbols>[]> overloads(new std::unique_ptr<OverloadSymbols>[count]);

		const mf::Rva<df::OverloadSymbols> *defRvas = symbols->overloads.Get();
		for (int32_t i = 0; i < count; i++)
		{
			mf::Rva<df::OverloadSymbols> defRva = defRvas[i];
			// If the overload is abstract or native, or just doesn't have any
			// debug symbols, the RVA will be zero. In that case we can just
			// skip it, since std::unique_ptr<>'s default constructor sets its
			// pointer to null.
			if (defRva.IsNull())
				continue;

			const df::OverloadSymbols *def = file.Deref(defRva);

			MethodOverload *overload = method->overloads + i;

			overloads[i] = ReadSingleOverloadSymbols(
				data,
				methodSymbols.get(),
				overload,
				def
			);
		}

		methodSymbols->SetOverloads(count, std::move(overloads));

		return std::move(methodSymbols);
	}

	std::unique_ptr<OverloadSymbols> DebugSymbolsReader::ReadSingleOverloadSymbols(
		ModuleDebugData *data,
		MethodSymbols *parent,
		MethodOverload *overload,
		const debug_file::OverloadSymbols *symbols
	)
	{
		int32_t count = symbols->symbolCount;
		std::unique_ptr<DebugSymbol[]> debugSymbols(new DebugSymbol[count]);

		const df::DebugSymbol *defs = symbols->symbols.Get();
		for (int32_t i = 0; i < count; i++)
		{
			const df::DebugSymbol *def = defs + i;

			DebugSymbol *debugSymbol = debugSymbols.get() + i;

			debugSymbol->startOffset = def->startOffset;
			debugSymbol->endOffset = def->endOffset;

			debugSymbol->file = data->GetSourceFile(def->sourceFile);
			if (debugSymbol->file == nullptr)
				ModuleLoadError("Invalid source file index.");

			debugSymbol->startLocation.lineNumber = def->startLocation.lineNumber;
			debugSymbol->startLocation.column = def->startLocation.column;
			debugSymbol->endLocation.lineNumber = def->endLocation.lineNumber;
			debugSymbol->endLocation.column = def->endLocation.column;
		}

		std::unique_ptr<OverloadSymbols> overloadSymbols(new OverloadSymbols(
			parent,
			overload,
			count,
			std::move(debugSymbols)
		));
		return std::move(overloadSymbols);
	}

	String *DebugSymbolsReader::ReadString(const mf::WideString *str)
	{
		String *string = GetGC()->ConstructModuleString(
			nullptr,
			str->length,
			str->chars.Get()
		);
		return string;
	}

	void DebugSymbolsReader::VerifyHeader(const df::DebugSymbolsHeader *header)
	{
		if (header->magic.number != df::ExpectedMagicNumber.number)
			ModuleLoadError("Invalid magic number in debug symbols file.");
	}

	void DebugSymbolsReader::AttachSymbols(ModuleDebugData *data)
	{
		int32_t methodSymbolCount = data->methodSymbolCount;
		for (int32_t m = 0; m < methodSymbolCount; m++)
		{
			MethodSymbols *method = data->methodSymbols[m].get();
			int32_t overloadCount = method->overloadCount;

			for (int32_t i = 0; i < overloadCount; i++)
			{
				OverloadSymbols *overload = method->overloads[i].get();
				overload->overload->debugSymbols = overload;
			}
		}
	}

	OVUM_NOINLINE void DebugSymbolsReader::ModuleLoadError(const char *message)
	{
		throw ModuleLoadException(file.GetFileName(), message);
	}

} // namespace debug

} // namespace ovum
