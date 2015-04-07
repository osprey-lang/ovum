@echo off

rem Path to the compiler
set OSPC="%OSP%\Osprey\bin\Release\Osprey.exe"
rem Path to the library folder
set LIB=%OSP%\lib
rem Path to the folder containing aves.dll
set DLLDIR=%OSP%\Ovum\Release

%OSPC% /nativelib "%DLLDIR%\aves.dll" /meta meta.txt /type module /nostdlib /out "%LIB%\aves.ovm" /doc "%LIB%\aves.ovm.json" /formatjson /verbose %* aves.osp
