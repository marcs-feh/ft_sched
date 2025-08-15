@echo off

setlocal

set cc=clang++
set cflags=-std=c++14 -fno-strict-aliasing -fwrapv -O0
set wflags=-Wall -Wextra -Werror=return-type -D_CRT_SECURE_NO_WARNINGS

rem Code Generation
rem %cc% %cflags% %wflags% generate.cpp base.cpp -o generate.exe
rem if %ERRORLEVEL% NEQ 0 GOTO ERROR

rem generate.exe
rem if %ERRORLEVEL% NEQ 0 GOTO ERROR

rem Compile project
%cc% %cflags% %wflags% main.cpp base.cpp -o ft_sched.exe
if %ERRORLEVEL% NEQ 0 GOTO ERROR

ft_sched.exe
if %ERRORLEVEL% NEQ 0 GOTO ERROR

:SUCCESS
exit /b 0

:ERROR
exit /b 1
