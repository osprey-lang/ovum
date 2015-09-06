#include "aves_state.h"
#include <ov_module.h>
#include <new>

namespace aves
{

#define SFS        ::StringFlags::STATIC
#define AVES       'a','v','e','s','.'
#define REFLECTION AVES,'r','e','f','l','e','c','t','i','o','n','.'

namespace type_names
{
	LitString<18> ArgumentError      = { 18, 0, SFS, AVES,'A','r','g','u','m','e','n','t','E','r','r','o','r',0 };
	LitString<22> ArgumentNullError  = { 22, 0, SFS, AVES,'A','r','g','u','m','e','n','t','N','u','l','l','E','r','r','o','r',0 };
	LitString<23> ArgumentRangeError = { 23, 0, SFS, AVES,'A','r','g','u','m','e','n','t','R','a','n','g','e','E','r','r','o','r',0 };
	LitString<10> Array              = { 10, 0, SFS, AVES,'A','r','r','a','y',0 };
	LitString<11> Buffer             = { 11, 0, SFS, AVES,'B','u','f','f','e','r',0 };
	LitString<19> BufferViewKind     = { 19, 0, SFS, AVES,'B','u','f','f','e','r','V','i','e','w','K','i','n','d',0 };
	LitString<9>  Char               = { 9,  0, SFS, AVES,'C','h','a','r',0 };
	LitString<17> ConsoleColor       = { 17, 0, SFS, AVES,'C','o','n','s','o','l','e','C','o','l','o','r',0 };
	LitString<15> ConsoleKey         = { 15, 0, SFS, AVES,'C','o','n','s','o','l','e','K','e','y',0 };
	LitString<19> ConsoleKeyCode     = { 19, 0, SFS, AVES,'C','o','n','s','o','l','e','K','e','y','C','o','d','e',0 };
	LitString<22> DuplicateKeyError  = { 22, 0, SFS, AVES,'D','u','p','l','i','c','a','t','e','K','e','y','E','r','r','o','r',0 };
	LitString<14> HashEntry          = { 14, 0, SFS, AVES,'H','a','s','h','E','n','t','r','y',0 };
	LitString<8>  Int                = { 8,  0, SFS, AVES,'I','n','t',0 };
	LitString<22> InvalidStateError  = { 22, 0, SFS, AVES,'I','n','v','a','l','i','d','S','t','a','t','e','E','r','r','o','r',0 };
	LitString<22> NotSupportedError  = { 22, 0, SFS, AVES,'N','o','t','S','u','p','p','o','r','t','e','d','E','r','r','o','r',0 };
	LitString<9>  Real               = { 9,  0, SFS, AVES,'R','e','a','l',0 };
	LitString<11> String             = { 11, 0, SFS, AVES,'S','t','r','i','n','g',0 };
	LitString<9>  UInt               = { 9,  0, SFS, AVES,'U','I','n','t',0 };
	LitString<20> UnicodeCategory    = { 20, 0, SFS, AVES,'U','n','i','c','o','d','e','C','a','t','e','g','o','r','y',0 };
	LitString<12> Version            = { 12, 0, SFS, AVES,'V','e','r','s','i','o','n',0 };

	LitString<27> AccessLevel        = { 27, 0, SFS, REFLECTION,'A','c','c','e','s','s','L','e','v','e','l',0 };
	LitString<27> Constructor        = { 27, 0, SFS, REFLECTION,'C','o','n','s','t','r','u','c','t','o','r',0 };
	LitString<21> Field              = { 21, 0, SFS, REFLECTION,'F','i','e','l','d',0 };
	LitString<30> GlobalConstant     = { 30, 0, SFS, REFLECTION,'G','l','o','b','a','l','C','o','n','s','t','a','n','t',0 };
	LitString<33> MemberSearchFlags  = { 33, 0, SFS, REFLECTION,'M','e','m','b','e','r','S','e','a','r','c','h','F','l','a','g','s',0 };
	LitString<22> Module             = { 22, 0, SFS, REFLECTION,'M','o','d','u','l','e',0 };
	LitString<28> NativeHandle       = { 28, 0, SFS, REFLECTION,'N','a','t','i','v','e','H','a','n','d','l','e',0 };
	LitString<24> Overload           = { 24, 0, SFS, REFLECTION,'O','v','e','r','l','o','a','d',0 };
	LitString<24> Property           = { 24, 0, SFS, REFLECTION,'P','r','o','p','e','r','t','y',0 };
	LitString<22> ReflMethod         = { 22, 0, SFS, REFLECTION,'M','e','t','h','o','d',0 };

	LitString<20> FileNotFoundError  = { 20, 0, SFS, 'i','o','.','F','i','l','e','N','o','t','F','o','u','n','d','E','r','r','o','r',0 };
	LitString<10> IOError            = { 10, 0, SFS, 'i','o','.','I','O','E','r','r','o','r',0 };
}

#undef REFLECTION
#undef AVES
#undef SFS

Aves::Aves()
{
	// Deliberately leave the type handles untouched.
	// There are a lot of them, and InitTypes will be
	// called right after the constructor anyway.
}

Aves::~Aves()
{
}

void Aves::InitTypes(ModuleHandle module)
{
	aves.ArgumentError      = Module_FindType(module, type_names::ArgumentError.AsString(),      true);
	aves.ArgumentNullError  = Module_FindType(module, type_names::ArgumentNullError.AsString(),  true);
	aves.ArgumentRangeError = Module_FindType(module, type_names::ArgumentRangeError.AsString(), true);
	aves.Array              = Module_FindType(module, type_names::Array.AsString(),              true);
	aves.BufferViewKind     = Module_FindType(module, type_names::BufferViewKind.AsString(),     true);
	aves.Buffer             = Module_FindType(module, type_names::Buffer.AsString(),             true);
	aves.Char               = Module_FindType(module, type_names::Char.AsString(),               true);
	aves.ConsoleColor       = Module_FindType(module, type_names::ConsoleColor.AsString(),       true);
	aves.ConsoleKey         = Module_FindType(module, type_names::ConsoleKey.AsString(),         true);
	aves.ConsoleKeyCode     = Module_FindType(module, type_names::ConsoleKeyCode.AsString(),     true);
	aves.DuplicateKeyError  = Module_FindType(module, type_names::DuplicateKeyError.AsString(),  true);
	aves.HashEntry          = Module_FindType(module, type_names::HashEntry.AsString(),          true);
	aves.Int                = Module_FindType(module, type_names::Int.AsString(),                true);
	aves.InvalidStateError  = Module_FindType(module, type_names::InvalidStateError.AsString(),  true);
	aves.NotSupportedError  = Module_FindType(module, type_names::NotSupportedError.AsString(),  true);
	aves.Real               = Module_FindType(module, type_names::Real.AsString(),               true);
	aves.String             = Module_FindType(module, type_names::String.AsString(),             true);
	aves.UInt               = Module_FindType(module, type_names::UInt.AsString(),               true);
	aves.UnicodeCategory    = Module_FindType(module, type_names::UnicodeCategory.AsString(),    true);
	aves.Version            = Module_FindType(module, type_names::Version.AsString(),            true);

	aves.reflection.AccessLevel       = Module_FindType(module, type_names::AccessLevel.AsString(),       true);
	aves.reflection.Constructor       = Module_FindType(module, type_names::Constructor.AsString(),       true);
	aves.reflection.Field             = Module_FindType(module, type_names::Field.AsString(),             true);
	aves.reflection.GlobalConstant    = Module_FindType(module, type_names::GlobalConstant.AsString(),    true);
	aves.reflection.MemberSearchFlags = Module_FindType(module, type_names::MemberSearchFlags.AsString(), true);
	aves.reflection.Method            = Module_FindType(module, type_names::ReflMethod.AsString(),        true);
	aves.reflection.Module            = Module_FindType(module, type_names::Module.AsString(),            true);
	aves.reflection.NativeHandle      = Module_FindType(module, type_names::NativeHandle.AsString(),      true);
	aves.reflection.Overload          = Module_FindType(module, type_names::Overload.AsString(),          true);
	aves.reflection.Property          = Module_FindType(module, type_names::Property.AsString(),          true);

	io.FileNotFoundError  = Module_FindType(module, type_names::FileNotFoundError.AsString(), true);
	io.IOError            = Module_FindType(module, type_names::IOError.AsString(),           true);
}

Aves *Aves::Get(ThreadHandle thread)
{
	void *state = Module_GetCurrentStaticState(thread);
	return reinterpret_cast<Aves*>(state);
}

bool Aves::Init(ModuleHandle module)
{
	Aves *aves = new(std::nothrow) Aves();
	if (aves == nullptr)
		return false;

	aves->InitTypes(module);

	Module_InitStaticState(module, aves, Aves::Deallocate);
	return true;
}

void Aves::Deallocate(void *state)
{
	Aves *aves = reinterpret_cast<Aves*>(state);
	delete aves;
}

} // namespace aves
