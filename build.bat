@echo off
chcp 65001 >nul
setlocal EnableDelayedExpansion

title Build MyToolBox

REM ============================================================
REM  0. 参数解析（提前判断是否 --pack）
REM ============================================================
set "PACK=%~1"
set "SKIP_BUILD=0"
if /i "%PACK%"=="--pack"  set "SKIP_BUILD=1"
if /i "%PACK%"=="-p"      set "SKIP_BUILD=1"

REM ============================================================
REM  1. 杀掉可能占用文件的进程
REM ============================================================
if "%SKIP_BUILD%"=="0" (
    taskkill /f /im MyToolBox.exe 2>nul
    taskkill /f /im MyToolBoxBrowser.exe 2>nul
    taskkill /f /im moc.exe        2>nul
    taskkill /f /im cmake.exe      2>nul
    taskkill /f /im cl.exe         2>nul
    taskkill /f /im link.exe       2>nul
    taskkill /f /im MSBuild.exe    2>nul
    taskkill /f /im mspdbsrv.exe   2>nul
    timeout /t 2 /nobreak >nul
)

REM ============================================================
REM  2. 环境变量
REM ============================================================
set QT_DIR=F:\Qt\6.11.2\msvc2022_64
set VCVARS=E:\Microsoft\Microsoft Visual Studio\Microsoft Visual Studio 2026\VC\Auxiliary\Build\vcvars64.bat
set "INNO_DIR=E:\Program Files\Inno Setup 6"

echo ============ 环境诊断 ============
echo QT_DIR   = %QT_DIR%
echo VCVARS   = %VCVARS%
echo INNO_DIR = %INNO_DIR%
echo SKIP_BUILD = %SKIP_BUILD%
echo.

if "%SKIP_BUILD%"=="0" (
    if not exist "%QT_DIR%" (
        echo [ERROR] QT_DIR 目录不存在
        pause & exit /b 1
    )
    if not exist "%VCVARS%" (
        echo [ERROR] VCVARS 不存在
        pause & exit /b 1
    )
)

REM ============================================================
REM  3. 清理 build 目录（仅编译时）
REM ============================================================
if "%SKIP_BUILD%"=="0" (
    if exist build rmdir /s /q build
    if exist build (
        echo [ERROR] 无法删除 build 目录
        pause & exit /b 1
    )
    mkdir build
)

REM ============================================================
REM  4. 编译（仅编译时）
REM ============================================================
if "%SKIP_BUILD%"=="0" (
    call "%~dp0build_inner.bat"
    if errorlevel 1 (
        echo [ERROR] 构建失败
        pause & exit /b 1
    )
    echo.
    echo ============ Build 成功 ============
) else (
    echo.
    echo ============ 跳过编译，直接打包 ============
)

REM ============================================================
REM  5. 询问是否打包
REM ============================================================
if /i "%PACK%"=="--pack" goto :do_pack
if /i "%PACK%"=="-p"     goto :do_pack
if /i "%PACK%"=="--build-only" goto :skip_pack

set /p PACK="是否打包为安装包? [Y/N]: "
if /i not "%PACK%"=="Y" goto :skip_pack

:do_pack
echo.
echo ============ 开始打包 ============

REM ---- 5.1 查找 ISCC.exe ----
set "ISCC="
if exist "%INNO_DIR%\ISCC.exe" set "ISCC=%INNO_DIR%\ISCC.exe"
if not defined ISCC if exist "%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
if not defined ISCC if exist "%ProgramFiles%\Inno Setup 6\ISCC.exe"       set "ISCC=%ProgramFiles%\Inno Setup 6\ISCC.exe"

if not defined ISCC (
    for /f "tokens=2,*" %%A in ('reg query "HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Inno Setup 6_is1" /v InstallLocation 2^>nul ^| findstr InstallLocation') do (
        set "ISCC=%%B\ISCC.exe"
    )
)
if not defined ISCC (
    for /f "tokens=2,*" %%A in ('reg query "HKLM\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\Inno Setup 6_is1" /v InstallLocation 2^>nul ^| findstr InstallLocation') do (
        set "ISCC=%%B\ISCC.exe"
    )
)
if not defined ISCC (
    for /f "tokens=2,*" %%A in ('reg query "HKCU\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Inno Setup 6_is1" /v InstallLocation 2^>nul ^| findstr InstallLocation') do (
        set "ISCC=%%B\ISCC.exe"
    )
)
if not defined ISCC (
    for %%D in (E D C) do (
        if not defined ISCC if exist "%%D:\Inno Setup 6\ISCC.exe" set "ISCC=%%D:\Inno Setup 6\ISCC.exe"
        if not defined ISCC if exist "%%D:\Program Files\Inno Setup 6\ISCC.exe" set "ISCC=%%D:\Program Files\Inno Setup 6\ISCC.exe"
        if not defined ISCC if exist "%%D:\Program Files (x86)\Inno Setup 6\ISCC.exe" set "ISCC=%%D:\Program Files (x86)\Inno Setup 6\ISCC.exe"
    )
)

if not defined ISCC (
    echo [ERROR] 找不到 Inno Setup 的 ISCC.exe
    echo         已尝试:
    echo           - INNO_DIR = %INNO_DIR%
    echo           - 标准 Program Files 目录
    echo           - 注册表 HKLM/HKCU
    echo           - E:\ D:\ C:\ 常见目录
    echo.
    echo         请安装 Inno Setup 6: https://jrsoftware.org/isdl.php
    echo         或修改本脚本顶部 INNO_DIR 变量指向实际安装目录
    pause & goto :skip_pack
)

REM ★ 打印 ISCC 路径（用 set 避免 () 断句问题）
echo 使用 ISCC:
set ISCC

REM ---- 5.2 版本号 ----
set "APP_VER=1.0.0"
for /f "tokens=3" %%V in ('findstr /r /c:"project(MyToolBox VERSION" "%~dp0CMakeLists.txt" 2^>nul') do (
    set "APP_VER=%%V"
)
echo 版本号: %APP_VER%

REM ---- 5.3 源目录 / 输出目录 ----
set "SRC_DIR=%~dp0build\Release"
set "OUT_DIR=%~dp0build\installer"

if not exist "%SRC_DIR%" (
    echo [ERROR] 找不到 Release 目录
    echo         请先执行：build.bat --build-only
    pause & goto :skip_pack
)
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"

REM ---- 5.4 调 ISCC ----
echo.
echo 调用 ISCC 打包...
"%ISCC%" ^
    /DMyAppVersion="%APP_VER%" ^
    /DMySourceDir="%SRC_DIR%" ^
    /DMyOutputDir="%OUT_DIR%" ^
    "%~dp0installer\MyToolBox.iss"

set "ISCC_RC=%errorlevel%"
echo.
echo ISCC 退出码: %ISCC_RC%

if not "%ISCC_RC%"=="0" (
    echo.
    echo [ERROR] 打包失败
    pause
    goto :skip_pack
)

echo.
echo ============ 打包成功 ============
echo 安装包位置: %OUT_DIR%
dir /b "%OUT_DIR%\MyToolBox_Setup_*.exe" 2>nul
echo.
start "" "%OUT_DIR%"

:skip_pack

echo.
echo ============ 全部完成 ============
pause
endlocal