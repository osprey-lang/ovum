#pragma once

#ifndef VM__DEBUG_SYMBOLS_INTERNAL_H
#define VM__DEBUG_SYMBOLS_INTERNAL_H

#include "ov_vm.internal.h"

class ModuleReader;

namespace debug
{
	typedef struct SourceFile_S SourceFile;
	typedef struct SourceLocation_S SourceLocation;

	class DebugSymbols
	{
	public:
		DebugSymbols(Method::Overload *overload, int32_t symbolCount, SourceLocation *symbols);
		~DebugSymbols();

		Method::Overload *overload;
		int32_t symbolCount;
		SourceLocation *symbols;

		SourceLocation *FindSymbol(uint32_t offset);
	};

	struct SourceFile_S
	{
		String *fileName;
		uint8_t hash[20]; // SHA-1 hash
	};

	struct SourceLocation_S
	{
		SourceFile *file;

		uint32_t startOffset;
		uint32_t endOffset;

		int32_t lineNumber;
		int32_t column;
		int32_t sourceStartIndex;
		int32_t sourceEndIndex;
	};

	class ModuleDebugData
	{
	private:
		int32_t fileCount;
		SourceFile *files;
		int32_t symbolCount;
		DebugSymbols **symbols;

	public:
		ModuleDebugData();
		~ModuleDebugData();

		static void TryLoad(const PathName &moduleFile, Module *module);

	private:
		static void ReadSourceFiles(ModuleReader &reader, ModuleDebugData *target);
		static void ReadMethodSymbols(ModuleReader &reader, Module *module, ModuleDebugData *target);
		static void ReadSourceLocation(ModuleReader &reader, ModuleDebugData *data, SourceLocation *location);

		void AttachSymbols();

		friend class GC;
	};
}

#endif // VM__DEBUG_SYMBOLS_INTERNAL_H