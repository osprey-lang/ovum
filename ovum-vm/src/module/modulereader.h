#pragma once

#include "modulefile.h"
#include "../../inc/ov_module.h" // for ModuleVersion
#include "../vm.h"
#include "../ee/vm.h"
#include "../util/pathname.h"
#include "../object/member.h"
#include "../object/method.h"
#include <vector>

namespace ovum
{

class ModuleFile
{
public:
	ModuleFile();
	~ModuleFile();

	void Open(const pathchar_t *fileName);
	void Open(const PathName &fileName);

	inline const PathName &GetFileName() const
	{
		return fileName;
	}

	inline const void *GetData() const
	{
		return data;
	}

	template<class T>
	inline const T *Read(uint32_t address) const
	{
		return reinterpret_cast<const T*>(
			reinterpret_cast<const char*>(data) + address
		);
	}

	template<class T>
	inline void Read(uint32_t address, T *output) const
	{
		*output = *reinterpret_cast<const T*>(
			reinterpret_cast<const char*>(data) + address
		);
	}

	template<class T>
	inline const T *Deref(module_file::Rva<T> rva) const
	{
		return reinterpret_cast<const T*>(
			reinterpret_cast<const char*>(data) + rva.address
		);
	}

	template<class T>
	inline void Deref(module_file::Rva<T> rva, T *output) const
	{
		*output = *reinterpret_cast<const T*>(
			reinterpret_cast<const char*>(data) + rva.address
		);
	}

	template<class T>
	inline const T *Deref(module_file::Rva<T[]> rva) const
	{
		return reinterpret_cast<const T*>(
			reinterpret_cast<const char*>(data) + rva.address
		);
	}

private:
	// Memory-mapped file contents
	const void *data;

	os::MemoryMappedFile file;
	PathName fileName;

	void HandleFileOpenError(os::FileStatus err);
};

class ModuleReader
{
public:
	ModuleReader(VM *owner);
	~ModuleReader();

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

	Box<Module> ReadModule();

private:
	struct UnresolvedConstant;

	ModuleFile file;

	// The VM instance that the reader reads module data for.
	VM *vm;

	// Vector of fields with unresolved constant value types.
	std::vector<UnresolvedConstant> unresolvedConstants;

	String *ReadString(module_file::Rva<module_file::WideString> rva);

	String *ResolveString(Module *module, Token token);

	void ReadNativeLibrary(Module *module, module_file::Rva<module_file::WideString> libraryNameRva);

	Method *GetMainMethod(Module *module, Token token);

	void ReadStringTable(Module *module, const module_file::StringTableHeader *header);

	void ReadReferences(Module *module, const module_file::RefTableHeader *header);

	void ReadModuleRefs(Module *module, const module_file::RefTableHeader *header);

	void ReadTypeRefs(Module *module, const module_file::RefTableHeader *header);

	void ReadFieldRefs(Module *module, const module_file::RefTableHeader *header);

	void ReadMethodRefs(Module *module, const module_file::RefTableHeader *header);

	void ReadFunctionRefs(Module *module, const module_file::RefTableHeader *header);

	void ReadDefinitions(Module *module, const module_file::ModuleHeader *header);

	void ReadTypeDefs(Module *module, const module_file::ModuleHeader *header);

	Box<Type> ReadSingleTypeDef(Module *module, const module_file::ModuleHeader *header, const module_file::TypeDef *def);

	Type *GetBaseType(Module *module, Token baseTypeToken);

	Type *GetSharedType(Module *module, Token sharedTypeToken);

	Method *FindInstanceConstructor(Type *type);

	void RunTypeIniter(Module *module, Type *type, module_file::Rva<module_file::ByteString> initerRva);

	void ReadFieldDefs(Module *module, Type *type, uint32_t fieldsBase, int32_t count, Token firstField);

	void ReadFieldConstantValue(Module *module, Field *field, const module_file::ConstantValue *value, bool allowUnresolvedType);

	void AddUnresolvedConstant(Module *module, Field *field, const module_file::ConstantValue *value);

	void ReadMethodDefs(Module *module, Type *type, uint32_t methodsBase, int32_t count, Token firstMethod);

	Method *FindBaseMethod(Method *method, Type *declType);

	void ReadPropertyDefs(Module *module, Type *type, int32_t count, module_file::Rva<module_file::PropertyDef[]> properties);

	Method *ReadPropertyAccessor(Module *module, Type *type, Token token, MemberFlags &flags);

	void ReadOperatorDefs(Module *module, Type *type, int32_t count, module_file::Rva<module_file::OperatorDef[]> operators);

	void ResolveRemainingConstants(Module *module);

	void ReadFunctionDefs(Module *module, const module_file::ModuleHeader *header);

	void ReadConstantDefs(Module *module, const module_file::ModuleHeader *header);

	bool ReadConstantValue(Module *module, const module_file::ConstantValue *value, Value &result);

	Box<Method> ReadSingleMethodDef(Module *module, const module_file::MethodDef *def);

	void ReadParameters(Module *module, MethodOverload *overload, int32_t count, module_file::Rva<module_file::Parameter[]> rva);

	void ReadMethodBody(Module *module, MethodOverload *overload, const module_file::OverloadDef *def);

	void ReadNativeMethodBody(Module *module, MethodOverload *overload, const module_file::NativeMethodHeader *header);

	void ReadShortMethodBody(Module *module, MethodOverload *overload, const module_file::MethodBody *header);

	void ReadLongMethodBody(Module *module, MethodOverload *overload, const module_file::MethodHeader *header);

	void ReadTryBlocks(Module *module, MethodOverload *overload, int32_t count, module_file::Rva<module_file::TryBlock[]> rva);

	void ReadCatchClauses(Module *module, TryBlock *tryBlock, const module_file::CatchClauses &catchClauses);

	void ReadBytecodeBody(Module *module, MethodOverload *overload, const module_file::MethodBody *body);

	static MemberFlags GetMemberFlags(module_file::FieldFlags flags);
	static MemberFlags GetMemberFlags(module_file::MethodFlags flags);

	static OverloadFlags GetOverloadFlags(module_file::OverloadFlags flags);

	ModuleVersion ReadVersion(const module_file::ModuleVersion &version);

	void VerifyHeader(const module_file::ModuleHeader *header);

	void VerifyAnnotations(module_file::Rva<module_file::Annotations> rva);

	OVUM_NOINLINE void ModuleLoadError(const char *message);

	struct UnresolvedConstant
	{
		Field *field;
		const module_file::ConstantValue *value;
	};

	static const int MaxShortStringLength = 128;
};

} // namespace ovum
