@echo off
rem build.cmd -- builds sorp.exe with MSVC (the "Build SORP" task runs this).
rem
rem cl.exe and rc.exe are on the PATH only in a Developer Command Prompt.
rem When they are not, this finds Visual Studio (or its Build Tools) with
rem vswhere, which the Visual Studio installer always puts in the same
rem place, and runs its vcvars64.bat first.  So it works from VS Code
rem however VS Code was started, from a plain cmd, or from Git Bash
rem (./build.cmd).
rem
rem Outputs: sorp.exe and SDL3.dll beside it (a DLL is found beside the
rem executable at run time, not through /LIBPATH); the objects, the icon's
rem sorp.res and the debug database in build\msvc\ (build\ itself is
rem CMake's tree).
setlocal
cd /d "%~dp0"

where cl.exe >nul 2>nul
if errorlevel 1 call :vcvars
if errorlevel 1 exit /b 1

set "OUT=build\msvc"
if not exist %OUT% mkdir %OUT%
copy /y SDL3\lib\x64\SDL3.dll . >nul || exit /b 1

rem the icon, linked into the executable; rc.exe ships with the Windows SDK
rc.exe /nologo /fo %OUT%\sorp.res src\sorp.rc || exit /b 1

rem TZ=UTC0: the build stamp cl writes through __DATE__ and __TIME__ (F1's
rem "About", and the log's first line) is then in UTC, the same as the
rem Linux and handheld builds, made in a Docker container on UTC.  Without
rem it MSVC stamps the PC's local time.
set TZ=UTC0

rem /SUBSYSTEM:WINDOWS: no console comes with the game (the options
rem overlay, F1, opens one on demand).  <SDL3/SDL_main.h> is header-only in
rem SDL3 and supplies the WinMain.
cl.exe /nologo /Zi /W3 /D_CRT_SECURE_NO_WARNINGS ^
    /I src /I SDL3\include ^
    /Fo%OUT%\ /Fd%OUT%\ /Fesorp.exe ^
    src\sorp.c src\offroad_gfx.c src\og_data.c src\og_course.c ^
    src\og_palette.c src\og_sound.c src\opl3.c ^
    src\tp_rom.c src\tp_data.c src\tp_sound.c ^
    /link /SUBSYSTEM:WINDOWS /LIBPATH:SDL3\lib\x64 SDL3.lib user32.lib ^
    %OUT%\sorp.res
exit /b %errorlevel%

:vcvars
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
    echo build.cmd: cl.exe is not on the PATH and vswhere.exe was not found:
    echo install Visual Studio or its Build Tools with the C++ workload.
    exit /b 1
)
set "VSDIR="
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR (
    echo build.cmd: no Visual Studio with the C++ x64 tools was found.
    exit /b 1
)
rem vcvars64.bat looks for vswhere on the PATH and says so when it isn't
set "PATH=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer;%PATH%"
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul
where cl.exe >nul 2>nul || (
    echo build.cmd: vcvars64.bat did not put cl.exe on the PATH.
    exit /b 1
)
exit /b 0
