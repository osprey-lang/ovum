@echo off

rem Path to Ovum
set OVUM="%OSP%\Ovum\Release\Ovum.exe"

if [%1]==[skip-build] (
	set SKIPBUILD=1
) else (
	set SKIPBUILD=0
)

if %SKIPBUILD%==0 (
	echo [!] Compiling aves.tests...
	call build.bat
)

if %ERRORLEVEL%==0 (
	if %SKIPBUILD%==0 (
		echo.
		echo [!] Running tests
	)
	%OVUM% /L "%LIB%" aves.tests.ovm
)
