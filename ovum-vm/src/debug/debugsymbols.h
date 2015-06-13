#ifndef VM__DEBUG_SYMBOLS_INTERNAL_H
#define VM__DEBUG_SYMBOLS_INTERNAL_H

#include "../vm.h"

namespace ovum
{

namespace debug
{
	class DebugSymbols
	{
	public:
		DebugSymbols(MethodOverload *overload, int32_t symbolCount, SourceLocation *symbols);
		~DebugSymbols();

		MethodOverload *overload;
		int32_t symbolCount;
		SourceLocation *symbols;

		SourceLocation *FindSymbol(uint32_t offset);
	};

	struct SourceFile
	{
		String *fileName;
		uint8_t hash[20]; // SHA-1 hash
	};

	struct SourceLocation
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
} // namespace ovum::debug

} // namespace ovum

#endif // VM__DEBUG_SYMBOLS_INTERNAL_H