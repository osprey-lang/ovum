#pragma once

#include "debugfile.h"
#include "../vm.h"
#include "../module/modulereader.h"
#include "../util/pathname.h"

namespace ovum
{

namespace debug
{
	struct SourceFile
	{
		String *fileName;
		uint8_t hash[20]; // SHA-1 hash
	};

	struct SourceLocation
	{
		int32_t lineNumber;
		int32_t column;
	};

	struct DebugSymbol
	{
		uint32_t startOffset;
		uint32_t endOffset;

		SourceFile *file;

		SourceLocation startLocation;
		SourceLocation endLocation;
	};

	class OverloadSymbols
	{
	public:
		inline MethodOverload *GetOverload() const
		{
			return overload;
		}

		inline MethodSymbols *GetParent() const
		{
			return parent;
		}

		inline int32_t GetSymbolCount() const
		{
			return symbolCount;
		}

		inline DebugSymbol &GetSymbol(int32_t index) const
		{
			return symbols[index];
		}

		DebugSymbol *FindSymbol(uint32_t offset) const;

	private:
		OVUM_DISABLE_COPY_AND_ASSIGN(OverloadSymbols);

		OverloadSymbols(MethodSymbols *parent, MethodOverload *overload, int32_t symbolCount, Box<DebugSymbol[]> symbols);

		MethodSymbols *parent;
		MethodOverload *overload;
		int32_t symbolCount;
		Box<DebugSymbol[]> symbols;

		friend class DebugSymbolsReader;
	};

	class MethodSymbols
	{
	public:
		inline Method *GetMethod() const
		{
			return method;
		}

		OverloadSymbols *GetOverload(int32_t index) const;

	private:
		OVUM_DISABLE_COPY_AND_ASSIGN(MethodSymbols);

		MethodSymbols(Method *method);

		Method *method;

		int32_t overloadCount;
		Box<Box<OverloadSymbols>[]> overloads;

		void SetOverloads(int32_t count, Box<Box<OverloadSymbols>[]> overloads);

		friend class DebugSymbolsReader;
	};

	class ModuleDebugData
	{
	public:
		static void TryLoad(const PathName &moduleFile, Module *module);

		inline SourceFile *GetSourceFile(int32_t index) const
		{
			if (index < 0 || index >= fileCount)
				return nullptr;
			return files.get() + index;
		}

	private:
		OVUM_DISABLE_COPY_AND_ASSIGN(ModuleDebugData);

		ModuleDebugData();

		int32_t fileCount;
		Box<SourceFile[]> files;

		int32_t methodSymbolCount;
		Box<Box<MethodSymbols>[]> methodSymbols;
		
		friend class GC;
		friend class DebugSymbolsReader;
	};

	class DebugSymbolsReader
	{
	public:
		DebugSymbolsReader(VM *vm);
		~DebugSymbolsReader();

		void Open(const pathchar_t *fileName);
		void Open(const PathName &fileName);

		inline const PathName &GetFileName() const
		{
			return file.GetFileName();
		}

		inline VM *GetVM() const
		{
			return vm;
		}

		inline GC *GetGC() const
		{
			return vm->GetGC();
		}

		void ReadDebugSymbols(Module *module);

	private:
		ModuleFile file;

		VM *vm;

		void ReadSourceFiles(ModuleDebugData *data, const debug_file::SourceFileList *list);

		void ReadMethodSymbols(Module *module, ModuleDebugData *data, const debug_file::DebugSymbolsHeader *header);

		Box<MethodSymbols> ReadSingleMethodSymbols(
			ModuleDebugData *data,
			Module *module,
			const debug_file::MethodSymbols *symbols
		);

		Box<OverloadSymbols> ReadSingleOverloadSymbols(
			ModuleDebugData *data,
			MethodSymbols *parent,
			MethodOverload *overload,
			const debug_file::OverloadSymbols *symbols
		);

		String *ReadString(const module_file::WideString *str);

		void VerifyHeader(const debug_file::DebugSymbolsHeader *header);

		void AttachSymbols(ModuleDebugData *data);

		OVUM_NOINLINE void ModuleLoadError(const char *message);
	};
} // namespace ovum::debug

} // namespace ovum
