@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

if not defined DevEnvDir (
    call "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat" || exit /b 1
)

if not exist build mkdir build

rem Build languages.txt
(for %%f in (lang\*.txt) do (echo @file %%~nf& type "%%f"& echo.)) > build\languages.txt

rem Compile resource
rc /nologo /i res /i build /fo build\app.res res\app.rc || exit /b 1

rem Compile and Link
set CFLAGS=/nologo /std:c++17 /O2 /W3 /DNOMINMAX /D_CRT_SECURE_NO_WARNINGS /D_UNICODE /DUNICODE /D_WIN32_WINNT=0x0A00 /DWINVER=0x0A00 /EHsc /MD /Fo:build\ /Fd:build\
set LIBS=user32.lib hid.lib setupapi.lib cfgmgr32.lib ole32.lib oleaut32.lib comctl32.lib gdi32.lib ws2_32.lib winhttp.lib onecore.lib
set LFLAGS=/link /SUBSYSTEM:WINDOWS /ENTRY:wWinMainCRTStartup /OUT:WirelessBattery.exe

cl %CFLAGS% src\*.cpp src\drivers\*.cpp build\app.res %LIBS% %LFLAGS% || exit /b 1

echo Successfully built WirelessBattery.exe
