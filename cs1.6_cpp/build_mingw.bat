@echo off
setlocal
cd /d "%~dp0"

where g++ >nul 2>nul
if errorlevel 1 (
    echo [ERRO] g++ ^(MinGW^) nao encontrado no PATH.
    exit /b 1
)

if not exist build mkdir build

rem Compile icon resource (try llvm-windres first, then windres)
set "RC_OBJ=build\resource.o"
where llvm-windres >nul 2>&1
if %errorlevel%==0 (
    llvm-windres src\resource.rc -O coff -o %RC_OBJ%
) else (
    where windres >nul 2>&1
    if %errorlevel%==0 (
        windres src\resource.rc -O coff -o %RC_OBJ%
    ) else (
        echo Aviso: windres/llvm-windres nao encontrado. Compilando sem icone.
        set "RC_OBJ="
    )
)

if defined RC_OBJ (
    g++ -std=c++17 -O2 -municode -DUNICODE -D_UNICODE -DNOMINMAX -DWIN32_LEAN_AND_MEAN ^
        src\main.cpp %RC_OBJ% -o build\CS16AHK_CPP.exe ^
        -mwindows -luser32 -lgdi32 -lwinmm -ladvapi32 -lpsapi -lavrt
) else (
    g++ -std=c++17 -O2 -municode -DUNICODE -D_UNICODE -DNOMINMAX -DWIN32_LEAN_AND_MEAN ^
        src\main.cpp -o build\CS16AHK_CPP.exe ^
        -mwindows -luser32 -lgdi32 -lwinmm -ladvapi32 -lpsapi -lavrt
)
if errorlevel 1 (
    echo [ERRO] Falha na compilacao.
    exit /b 1
)

echo [OK] Compilado: %cd%\build\CS16AHK_CPP.exe
exit /b 0
