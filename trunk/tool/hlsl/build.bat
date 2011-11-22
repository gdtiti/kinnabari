@echo off

if x%OS% == xWindows_NT goto :start
echo You need WindowsNT or newer to run this script.
exit

:mk_dir
	if not exist %1 mkdir %1
	goto :EOF

:build_vtx
	set NAME=vtx_%1
	hlsl_tool %_SRC_%\%NAME%.hlsl %_BIN_%\%NAME%.cod %_BIN_%\%NAME%.prm vs_3_0
	echo %NAME%>> %PROG_LIST%
goto :EOF

:build_pix
	set NAME=pix_%1
	hlsl_tool %_SRC_%\%NAME%.hlsl %_BIN_%\%NAME%.cod %_BIN_%\%NAME%.prm ps_3_0
	echo %NAME%>> %PROG_LIST%
goto :EOF

:start

setlocal ENABLEEXTENSIONS ENABLEDELAYEDEXPANSION

set _BASE_=..\..
set _SRC_=%_BASE_%\src\shader
set _BIN_=%_BASE_%\bin\hlsl

call :mk_dir %_BASE_%\src\gen
call :mk_dir %_BASE_%\bin
call :mk_dir %_BASE_%\bin\data
call :mk_dir %_BASE_%\bin\data\sys
call :mk_dir %_BIN_%

set PROG_LIST=%_BIN_%\proglist.txt
if exist %PROG_LIST% del %PROG_LIST%


::call dxpath.bat

for /f %%i in (vtx_list.txt) do (
	call :build_vtx %%i
)

for /f %%i in (pix_list.txt) do (
	call :build_pix %%i
)

mkgpu
