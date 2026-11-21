@echo off
rem DbgTerm standalone tool build script (MinGW GCC)
rem Self-contained: copy this whole directory anywhere and run this script.
setlocal
cd /d "%~dp0"

set CC=gcc
set WINDRES=windres
set CFLAGS=-std=c99 -O2 -Wall -Wextra -D_WIN32_WINNT=0x0501
set LIBS=-lgdi32 -luser32 -lcomdlg32 -lcomctl32 -lshell32 -lole32
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
%CC% %CFLAGS% -mwindows -Iframework -o %OUT%\dbgterm.exe ^
    entry.c ^
    tool_dbgterm.c ^
    adb_dlg.c ^
    filter_view.c ^
    color_view.c ^
    proto_cap.c ^
    proto_view.c ^
    dbg_save.c ^
    dbg_conf.c ^
    adb_core.c ^
    dbg_source.c ^
    dbg_frame.c ^
    dbg_filter.c ^
    framework\ui_common.c framework\ui_menu.c %RES_ARG% ^
    %LIBS%
if errorlevel 1 (
    echo [ERROR] application build failed.
    exit /b 1
)
echo       [OK] %OUT%\dbgterm.exe

echo [3/3] building and running unit tests ...
%CC% %CFLAGS% -I. -o %OUT%\test_dbg_frame.exe tests\test_dbg_frame.c dbg_frame.c
if errorlevel 1 (
    echo [ERROR] test_dbg_frame build failed.
    exit /b 1
)
%OUT%\test_dbg_frame.exe
if errorlevel 1 (
    echo [ERROR] test_dbg_frame failed.
    exit /b 1
)
%CC% %CFLAGS% -I. -o %OUT%\test_dbg_filter.exe tests\test_dbg_filter.c dbg_filter.c
if errorlevel 1 (
    echo [ERROR] test_dbg_filter build failed.
    exit /b 1
)
%OUT%\test_dbg_filter.exe
if errorlevel 1 (
    echo [ERROR] test_dbg_filter failed.
    exit /b 1
)
%CC% %CFLAGS% -I. -o %OUT%\test_adb_core.exe tests\test_adb_core.c adb_core.c dbg_source.c -luser32
if errorlevel 1 (
    echo [ERROR] test_adb_core build failed.
    exit /b 1
)
%OUT%\test_adb_core.exe
if errorlevel 1 (
    echo [ERROR] test_adb_core failed.
    exit /b 1
)
%CC% %CFLAGS% -I. -o %OUT%\test_proto_cap.exe tests\test_proto_cap.c proto_cap.c -luser32
if errorlevel 1 (
    echo [ERROR] test_proto_cap build failed.
    exit /b 1
)
%OUT%\test_proto_cap.exe
if errorlevel 1 (
    echo [ERROR] test_proto_cap failed.
    exit /b 1
)

echo all done.
endlocal
