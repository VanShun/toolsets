@echo off
rem GB32960 standalone tool build script (MinGW GCC)
rem Self-contained: copy this whole directory anywhere and run this script.
setlocal
cd /d "%~dp0"

set CC=gcc
set WINDRES=windres
set CFLAGS=-std=c99 -O2 -Wall -Wextra -D_WIN32_WINNT=0x0501
set LIBS=-lgdi32 -luser32 -lcomdlg32 -lcomctl32
set OUT=bin
set FW_SRC=..\..\framework
set FW_ROOT=..\..

if not exist %OUT% mkdir %OUT%

echo [1/3] syncing framework sources (vendor-on-build) ...
if not exist framework mkdir framework
if exist "%FW_SRC%\*.c" copy /y "%FW_SRC%\*.c" framework\ >nul
if exist "%FW_SRC%\*.h" copy /y "%FW_SRC%\*.h" framework\ >nul
if exist "%FW_ROOT%\version.h" copy /y "%FW_ROOT%\version.h" framework\ >nul
if not exist "framework\ui_common.h" (
    echo [ERROR] framework sources not found: neither %FW_SRC% nor bundled framework\
    exit /b 1
)

echo [2/3] building standalone application ...
rem optional icon: packages may ship res\app.rc + res\app.ico (resource ID = 1)
set RES_ARG=
if exist res\app.rc (
    %WINDRES% res\app.rc -O coff -o res\app_res.o
    if errorlevel 1 echo [WARN] windres failed, exe will have no custom icon.
)
if exist res\app_res.o set RES_ARG=res\app_res.o
%CC% %CFLAGS% -mwindows -Iframework -o %OUT%\gb32960.exe ^
    entry.c ^
    tool_gb32960.c ^
    gb32960.c ^
    framework\ui_common.c %RES_ARG% ^
    %LIBS%
if errorlevel 1 (
    echo [ERROR] application build failed.
    exit /b 1
)
echo       [OK] %OUT%\gb32960.exe

echo [3/3] building and running unit tests ...
%CC% %CFLAGS% -I. -o %OUT%\test_gb32960.exe tests\test_gb32960.c gb32960.c
if errorlevel 1 (
    echo [ERROR] unit test build failed.
    exit /b 1
)
%OUT%\test_gb32960.exe
if errorlevel 1 (
    echo [ERROR] unit tests failed.
    exit /b 1
)

echo all done.
endlocal
