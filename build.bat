@echo off
REM UMVC3 Modding - Complete Build Script with Progress
REM Builds both the DLL (for injection) and the Overlay App (EXE)

setlocal enabledelayedexpansion

echo.
echo ========================================
echo UMVC3 Overlay Mod - Build System
echo ========================================
echo.

REM Try to find Visual Studio 2022
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    echo [OK] Visual Studio 2022 Community Edition
) else if exist "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1
    echo [OK] Visual Studio 2022 Professional Edition
) else (
    echo [ERROR] Visual Studio 2022 not found!
    exit /b 1
)

REM Create build directories
if not exist "build" mkdir build
if not exist "build\obj" mkdir build\obj
if not exist "build\obj\dll" mkdir build\obj\dll
if not exist "build\obj\exe" mkdir build\obj\exe
if not exist "build\bin" mkdir build\bin

REM Include paths
set INCLUDE_PATHS=/I "include" /I "Resources"

echo [OK] Build directories ready
echo.

echo [BUILD] Directories ready
echo.

REM Define include paths
set INCLUDE_PATHS=/I"include" /I"utils" /I"."

REM ============================================================
REM PART 1: BUILD DLL
REM ============================================================
echo ========================================
echo BUILDING DLL (UMVC3Overlay.dll)
echo ========================================
echo.

set DLL_SOURCES=src\dllmain.cpp src\character.cpp utils\addr.cpp
set /a DLL_COUNT=0
for %%F in (%DLL_SOURCES%) do set /a DLL_COUNT+=1

set COMPILER_FLAGS=/W4 /WX- /EHsc /O2 /std:c++17 /fp:precise /Zc:inline /permissive- /D "WIN32" /D "_WINDOWS" /D "_USRDLL" /D "UMVC3OVERLAY_EXPORTS" /c

set /a CURRENT=0
for %%F in (%DLL_SOURCES%) do (
    set /a CURRENT+=1
    set FILENAME=%%~nF
    set /a PERCENT=!CURRENT! * 100 / !DLL_COUNT!
    
    echo [!PERCENT!%%] Compiling !FILENAME!
    cl.exe %COMPILER_FLAGS% %INCLUDE_PATHS% /Fo"build\obj\dll\\" %%F >nul 2>&1
    if errorlevel 1 (
        echo [ERROR] Failed to compile %%F
        cl.exe %COMPILER_FLAGS% %INCLUDE_PATHS% /Fo"build\obj\dll\\" %%F
        exit /b 1
    )
)

echo [100%%] DLL compilation complete
echo.
echo [  ] Linking DLL...
link.exe /OUT:"build\bin\UMVC3Overlay.dll" /SUBSYSTEM:WINDOWS /DLL /MACHINE:X64 /LIBPATH:"." d3d9.lib kernel32.lib user32.lib gdi32.lib build\obj\dll\dllmain.obj build\obj\dll\character.obj build\obj\dll\addr.obj >nul 2>&1
if errorlevel 1 (
    echo [ERROR] DLL Linking failed
    exit /b 1
)

echo [OK] UMVC3Overlay.dll created (262 KB)
echo.

REM ============================================================
REM PART 2: BUILD HITBOX VIEWER APP EXE
REM ============================================================
echo ========================================
echo BUILDING HITBOX VIEWER APP (UMVC3OverlayApp.exe)
echo ========================================
echo.

set EXE_SOURCES=src\overlay_app.cpp
set COMPILER_FLAGS=/W4 /WX- /EHsc /O2 /std:c++17 /D "WIN32" /D "_WINDOWS" /c

echo [50%%] Compiling overlay_app.cpp
cl.exe %COMPILER_FLAGS% %INCLUDE_PATHS% /Fo"build\obj\exe\\" %EXE_SOURCES% >nul 2>&1
if errorlevel 1 (
    echo [ERROR] Failed to compile overlay_app.cpp
    cl.exe %COMPILER_FLAGS% %INCLUDE_PATHS% /Fo"build\obj\exe\\" %EXE_SOURCES%
    exit /b 1
)

echo [100%%] EXE compilation complete
echo.
echo [  ] Linking EXE...
link.exe /OUT:"build\bin\UMVC3OverlayApp.exe" /SUBSYSTEM:WINDOWS /MACHINE:X64 kernel32.lib user32.lib gdi32.lib gdiplus.lib build\obj\exe\overlay_app.obj >nul 2>&1
if errorlevel 1 (
    echo [ERROR] EXE Linking failed
    exit /b 1
)

echo [OK] UMVC3OverlayApp.exe created
echo.
echo.

REM ============================================================
REM SUMMARY
REM ============================================================
echo ========================================
echo BUILD COMPLETE
echo ========================================
echo.
echo Output:
echo   DLL:  build\bin\UMVC3Overlay.dll (262 KB)
echo   EXE:  build\bin\UMVC3OverlayApp.exe (141 KB)
echo   by N3
echo.
echo Usage:
echo   1. Run the .\build\bin\UMVC3OverlayApp.exe
echo   2. Open the UMVC3 game
echo   3. Go to training mode and enjoy the hitbox viewer.
echo. 

