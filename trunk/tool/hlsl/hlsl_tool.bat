@echo off

set PROJ_NAME=hlsl_tool


if x%OS% == xWindows_NT goto :nt_start
echo You need WindowsNT or newer to run this script.
exit

:nt_start

setlocal ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION

call dxpath.bat

set VC_PATH=%ProgramFiles%\Microsoft Visual Studio 8\VC

for %%i in ("%VC_PATH%") do (set VC_PATH=%%~fsi)
set VC_BIN=%VC_PATH%\bin

for %%i in ("%DX_PATH%") do (set DX_PATH=%%~fsi)
set DX_LIB=%DX_PATH%\Lib\x86
set EXT_LIB=%DX_LIB%\d3d9.lib %DX_LIB%\d3dx9.lib

set VC=%VC_BIN%\cl.exe
set VC_LINK=%VC_BIN%\link.exe
set MAKE=%VC_BIN%\nmake.exe /nologo

for /f "tokens=*" %%i in (vc_opt.txt) do (set VC_OPT=%%i)

::echo %VC_OPT%

if x%1 == xclean (
	%MAKE% clean
) else (
	call %VC_PATH%\vcvarsall.bat
	%MAKE% CC=%VC% LNK=%VC_LINK% CC_OPT="%VC_OPT%" OPT_FILE=vc_opt.txt
)

