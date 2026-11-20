@echo off
rem ToolSets build script (MinGW GCC)
setlocal
cd /d "%~dp0"

set CC=gcc
set WINDRES=windres
set CFLAGS=-std=c99 -O2 -Wall -Wextra -D_WIN32_WINNT=0x0501
set LIBS=-lgdi32 -luser32 -lcomctl32 -lshell32 -lole32 -lwininet
set SRC_APP=app
set SRC_FW=framework
set OUT=bin
set RES=res

if not exist %OUT% mkdir %OUT%
if not exist %RES% mkdir %RES%

echo [1/4] generating icon resource ...
python scripts\make_icon.py
if errorlevel 1 (
    echo [ERROR] icon generation failed.
    exit /b 1
)
%WINDRES% -I. %RES%\app.rc -O coff -o %RES%\app_res.o
if errorlevel 1 (
    echo [ERROR] windres failed.
    exit /b 1
)

echo [2/4] building application ...
rem --- To add a tool: append its module sources to the list below ---
%CC% %CFLAGS% -mwindows -I. -I%SRC_APP% -I%SRC_FW% -o %OUT%\toolsets.exe ^
    %SRC_APP%\main.c ^
    %SRC_APP%\tool_manifest.c ^
    %SRC_APP%\updater.c ^
    %SRC_FW%\ui_common.c ^
    %SRC_FW%\ui_menu.c ^
    tools\hexascii\tool_hexascii.c ^
    tools\hexascii\hex_conv.c ^
    tools\dbgterm\tool_dbgterm.c ^
    tools\dbgterm\adb_dlg.c ^
    tools\dbgterm\filter_view.c ^
    tools\dbgterm\color_view.c ^
    tools\dbgterm\dbg_save.c ^
    tools\dbgterm\dbg_conf.c ^
    tools\gb32960\tool_gb32960.c ^
    tools\gb32960\gb32960.c ^
    tools\dbgterm\adb_core.c ^
    tools\dbgterm\dbg_source.c ^
    tools\dbgterm\dbg_frame.c ^
    tools\dbgterm\dbg_filter.c ^
    %RES%\app_res.o ^
    %LIBS%
if errorlevel 1 (
    echo [ERROR] application build failed.
    exit /b 1
)
echo       [OK] %OUT%\toolsets.exe

echo [3/4] building unit tests ...
%CC% %CFLAGS% -Itools\hexascii -o %OUT%\test_hex_conv.exe ^
    tools\hexascii\tests\test_hex_conv.c ^
    tools\hexascii\hex_conv.c
if errorlevel 1 (
    echo [ERROR] unit test build failed.
    exit /b 1
)
%OUT%\test_hex_conv.exe
if errorlevel 1 (
    echo [ERROR] unit tests failed.
    exit /b 1
)
%CC% %CFLAGS% -Itools\dbgterm -o %OUT%\test_dbg_frame.exe ^
    tools\dbgterm\tests\test_dbg_frame.c ^
    tools\dbgterm\dbg_frame.c
if errorlevel 1 (
    echo [ERROR] unit test build failed.
    exit /b 1
)
%OUT%\test_dbg_frame.exe
if errorlevel 1 (
    echo [ERROR] unit tests failed.
    exit /b 1
)
%CC% %CFLAGS% -Itools\dbgterm -o %OUT%\test_dbg_filter.exe ^
    tools\dbgterm\tests\test_dbg_filter.c ^
    tools\dbgterm\dbg_filter.c
if errorlevel 1 (
    echo [ERROR] unit test build failed.
    exit /b 1
)
%OUT%\test_dbg_filter.exe
if errorlevel 1 (
    echo [ERROR] unit tests failed.
    exit /b 1
)
%CC% %CFLAGS% -Itools\gb32960 -o %OUT%\test_gb32960.exe ^
    tools\gb32960\tests\test_gb32960.c ^
    tools\gb32960\gb32960.c
if errorlevel 1 (
    echo [ERROR] unit test build failed.
    exit /b 1
)
%OUT%\test_gb32960.exe
if errorlevel 1 (
    echo [ERROR] unit tests failed.
    exit /b 1
)

echo [4/4] all done.
endlocal
