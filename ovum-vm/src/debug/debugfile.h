#pragma once

#include "../vm.h"
#include "../module/modulefile.h"

namespace ovum
{

namespace debug_file
{

	static const size_t Sha1HashSize = 20;

	struct DebugSymbolsHeader;
	struct SourceFileList;
	struct SourceFile;
	struct MethodSymbols;
	struct OverloadSymbols;
	struct DebugSymbol;
	struct SourceLocation;

	struct DebugSymbolsHeader
	{
		module_file::MagicNumber magic;
		uint32_t metadata;

		module_file::Rva<SourceFileList> sourceFiles;

		int32_t methodSymbolCount;
		module_file::InlineArray<module_file::Rva<MethodSymbols>> methodSymbols;
	};

	struct MethodSymbols
	{
		Token memberToken;

		uint32_t metadata;

		int32_t overloadCount;
		module_file::InlineArray<module_file::Rva<OverloadSymbols>> overloads;
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

		int32_t sourceFile;

		SourceLocation startLocation;
		SourceLocation endLocation;
	};

	struct OverloadSymbols
	{
		uint32_t metadata;

		int32_t symbolCount;
		module_file::InlineArray<DebugSymbol> symbols;
	};

	struct SourceFileList
	{
		int32_t fileCount;
		module_file::InlineArray<module_file::Rva<SourceFile>> files;
	};

	struct SourceFile
	{
		uint8_t hash[Sha1HashSize];
		module_file::WideString fileName;
	};

} // namespace debug_file

} // namespace ovum
