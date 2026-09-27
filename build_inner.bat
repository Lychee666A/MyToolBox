@echo off
REM ============================================================
REM  这个 bat 在 MSVC 环境里执行构建，由 build.bat 通过 call 调用
REM ============================================================

call "E:\Microsoft\Microsoft Visual Studio\Microsoft Visual Studio 2026\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1

set PATH=F:\Qt\6.11.2\msvc2022_64\bin;%PATH%

cd /d "%~dp0build"
if errorlevel 1 exit /b 1

cmake -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="F:\Qt\6.11.2\msvc2022_64" ..
if errorlevel 1 exit /b 1

cmake --build . --config Release --parallel
if errorlevel 1 exit /b 1

exit /b 0