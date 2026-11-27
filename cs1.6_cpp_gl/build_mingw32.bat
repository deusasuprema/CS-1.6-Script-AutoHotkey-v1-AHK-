@echo off
setlocal

rem Build 32-bit (x86) overlay DLL and injector for CS 1.6 (hl.exe is 32-bit)
rem Requires a 32-bit MinGW/LLVM-MinGW toolchain in PATH (i686-w64-mingw32-* or 32-bit clang/g++)

if not exist build mkdir build

set "CXX="
set "WINDRES="

where i686-w64-mingw32-g++ >nul 2>&1
if %errorlevel%==0 (
    set "CXX=i686-w64-mingw32-g++"
    where i686-w64-mingw32-windres >nul 2>&1 && set "WINDRES=i686-w64-mingw32-windres"
    if "%WINDRES%"=="" where llvm-windres >nul 2>&1 && set "WINDRES=llvm-windres"
    if "%WINDRES%"=="" where windres >nul 2>&1 && set "WINDRES=windres"
) else (
    where g++ >nul 2>&1
    if %errorlevel%==0 (
        set "CXX=g++ -m32"
        where llvm-windres >nul 2>&1 && set "WINDRES=llvm-windres"
        if "%WINDRES%"=="" where windres >nul 2>&1 && set "WINDRES=windres"
        if "%WINDRES%"=="" where i686-w64-mingw32-windres >nul 2>&1 && set "WINDRES=i686-w64-mingw32-windres"
    )
)

if "%CXX%"=="" (
    echo Compilador 32-bit nao encontrado.
    echo Instale um toolchain i686 (ex.: LLVM-MinGW 32-bit / WinLibs i686 / w64devkit i686) e adicione ao PATH.
    exit /b 1
)

echo Usando compilador: %CXX%

rem Build overlay DLL
%CXX% -std=c++17 -O2 -shared -municode -DUNICODE -D_UNICODE -DNOMINMAX -DWIN32_LEAN_AND_MEAN ^
    overlay_dll\main.cpp -o build\cs16_overlay.dll ^
    -mwindows -static-libgcc -static-libstdc++ -luser32 -lgdi32 -lopengl32 -lwinmm -ladvapi32 -lpsapi -lavrt
if errorlevel 1 (
    echo Falha ao compilar overlay_dll.
    exit /b 1
)

rem Compile injector icon resource (optional)
set "INJ_RES="
if not "%WINDRES%"=="" (
    echo Usando resource compiler: %WINDRES%
    %WINDRES% --target=pe-i386 injector\resource.rc -O coff -o build\injector_res.o
    if %errorlevel%==0 set "INJ_RES=build\injector_res.o"
)

rem Build injector
if not "%INJ_RES%"=="" (
    %CXX% -std=c++17 -O2 -municode -DUNICODE -D_UNICODE -DNOMINMAX -DWIN32_LEAN_AND_MEAN ^
        injector\main.cpp %INJ_RES% -o build\CS16_Overlay_Injector.exe ^
        -mwindows -static-libgcc -static-libstdc++ -luser32 -ladvapi32
) else (
    %CXX% -std=c++17 -O2 -municode -DUNICODE -D_UNICODE -DNOMINMAX -DWIN32_LEAN_AND_MEAN ^
        injector\main.cpp -o build\CS16_Overlay_Injector.exe ^
        -mwindows -static-libgcc -static-libstdc++ -luser32 -ladvapi32
)
if errorlevel 1 (
    echo Falha ao compilar injector.
    exit /b 1
)

echo.
echo Compilado com sucesso:
echo   build\cs16_overlay.dll
echo   build\CS16_Overlay_Injector.exe
echo.
echo Copie ambos para a mesma pasta e execute o Injector (como admin se necessario) com o CS 1.6 aberto.
