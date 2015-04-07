@echo off

rem Path to the compiler
set OSPC="%OSP%\Osprey\bin\Release\Osprey.exe"
rem Path to the library folder
set LIB=%OSP%\lib

%OSPC% /libpath "%LIB%" /main aves.tests.main /out aves.tests.ovm /verbose /r *.osp
