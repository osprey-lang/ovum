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
	OverloadSymbols::OverloadSymbols(
		MethodSymbols *parent,
		MethodOverload *overload,
		size_t symbolCount,
		Box<DebugSymbol[]> symbols
	) :
		parent(parent),
		overload(overload),
		symbolCount(symbolCount),
		symbols(std::move(symbols))
	{ }

	DebugSymbol *OverloadSymbols::FindSymbol(uint32_t offset) const
	{
		DebugSymbol *symbols = this->symbols.get();

		// We have to use a signed type here. Otherwise, if offset is before the
		// first debug symbol, i - 1 will overflow when i = 0.
		ssize_t imin = 0, imax = (ssize_t)symbolCount - 1;
		while (imax >= imin)
		{
			ssize_t i = imin + (imax - imin) / 2;
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

	void MethodSymbols::SetOverloads(size_t count, Box<Box<OverloadSymbols>[]> overloads)
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

		// Assign the unfinished data to the module already, so that the GC
		// can reach it if it has to. If an error occurs, TryLoad() will
		// reset this value.
		module->debugData = Box<ModuleDebugData>(new ModuleDebugData());
		ModuleDebugData *output = module->debugData.get();

		ReadSourceFiles(output, file.Deref(header->sourceFiles));
		ReadMethodSymbols(module, output, header);

		// Success! Now that we know we've successfully read all the symbols,
		// we can attach them to their respective overloads.
		AttachSymbols(output);
	}

	void DebugSymbolsReader::ReadSourceFiles(ModuleDebugData *data, const df::SourceFileList *list)
	{
		size_t count = (size_t)list->fileCount;

		// Give the debug data the file list immediately, so the GC can find
		// the file name strings if it has to.
		data->fileCount = count;
		data->files.reset(new SourceFile[count]);
		memset(data->files.get(), 0, sizeof(SourceFile) * count);

		const mf::Rva<df::SourceFile> *defRvas = list->files.Get();
		for (size_t i = 0; i < count; i++)
		{
			mf::Rva<df::SourceFile> defRva = defRvas[i];
			const df::SourceFile *def = file.Deref(defRva);

			SourceFile *file = data->files.get() + i;

			file->fileName = ReadString(&def->fileName);
			CopyMemoryT(file->hash, def->hash, df::Sha1HashSize);
		}
	}

	void DebugSymbolsReader::ReadMethodSymbols(
		Module *module,
		ModuleDebugData *data,
		const df::DebugSymbolsHeader *header
	)
	{
		size_t count = (size_t)header->methodSymbolCount;

		Box<Box<MethodSymbols>[]> methodSymbols(new Box<MethodSymbols>[count]);

		const mf::Rva<df::MethodSymbols> *defRvas = header->methodSymbols.Get();
		for (size_t i = 0; i < count; i++)
		{
			mf::Rva<df::MethodSymbols> defRva = defRvas[i];
			const df::MethodSymbols *def = file.Deref(defRva);

			methodSymbols[i] = ReadSingleMethodSymbols(data, module, def);
		}

		data->methodSymbolCount = count;
		data->methodSymbols = std::move(methodSymbols);
	}

	Box<MethodSymbols> DebugSymbolsReader::ReadSingleMethodSymbols(
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

		Box<MethodSymbols> methodSymbols(new MethodSymbols(method));

		size_t count = (size_t)symbols->overloadCount;
		Box<Box<OverloadSymbols>[]> overloads(new Box<OverloadSymbols>[count]);

		const mf::Rva<df::OverloadSymbols> *defRvas = symbols->overloads.Get();
		for (size_t i = 0; i < count; i++)
		{
			mf::Rva<df::OverloadSymbols> defRva = defRvas[i];
			// If the overload is abstract or native, or just doesn't have any
			// debug symbols, the RVA will be zero. In that case we can just
			// skip it, since Box<>'s default constructor sets its pointer to
			// null.
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

	Box<OverloadSymbols> DebugSymbolsReader::ReadSingleOverloadSymbols(
		ModuleDebugData *data,
		MethodSymbols *parent,
		MethodOverload *overload,
		const debug_file::OverloadSymbols *symbols
	)
	{
		size_t count = (size_t)symbols->symbolCount;
		Box<DebugSymbol[]> debugSymbols(new DebugSymbol[count]);

		const df::DebugSymbol *defs = symbols->symbols.Get();
		for (size_t i = 0; i < count; i++)
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

		Box<OverloadSymbols> overloadSymbols(new OverloadSymbols(
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
		size_t methodSymbolCount = data->methodSymbolCount;
		for (size_t m = 0; m < methodSymbolCount; m++)
		{
			MethodSymbols *method = data->methodSymbols[m].get();
			size_t overloadCount = method->overloadCount;

			for (size_t i = 0; i < overloadCount; i++)
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
