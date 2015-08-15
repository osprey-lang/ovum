@echo off

rem Path to the compiler
set OSPC="%OSP%\Osprey\bin\Release\Osprey.exe"
rem Path to the library folder
set LIB=%OSP%\lib
rem Path to the folder containing aves.dll
set DLLDIR=%OSP%\Ovum\Release

if not exist "%LIB%\aves\" (
	echo Creating directory %LIB%\aves
	mkdir "%LIB%\aves"
)

%OSPC% /nativelib "%DLLDIR%\aves.dll" /meta meta.txt /type module /nostdlib /out "%LIB%\aves\aves.ovm" /doc "%LIB%\aves\aves.ovm.json" /formatjson /verbose %* aves.osp
