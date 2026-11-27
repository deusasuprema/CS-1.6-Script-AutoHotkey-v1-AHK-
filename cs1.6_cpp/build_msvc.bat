@echo off
setlocal
cd /d "%~dp0"

where cl >nul 2>nul
if errorlevel 1 (
    echo [ERRO] Compilador MSVC ^(cl.exe^) nao encontrado no PATH.
    echo Abra um "Developer Command Prompt for VS" e execute este script novamente.
    exit /b 1
)

if not exist build mkdir build

rc /nologo /fo build\resource.res src\resource.rc
if errorlevel 1 (
    echo Falha ao compilar recurso (rc). Tentando compilar sem icone...
    cl /nologo /std:c++17 /W4 /EHsc /DUNICODE /D_UNICODE /DNOMINMAX /DWIN32_LEAN_AND_MEAN ^
       src\main.cpp /Fe:build\CS16AHK_CPP.exe /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib winmm.lib advapi32.lib psapi.lib avrt.lib
) else (
    cl /nologo /std:c++17 /W4 /EHsc /DUNICODE /D_UNICODE /DNOMINMAX /DWIN32_LEAN_AND_MEAN ^
       src\main.cpp build\resource.res /Fe:build\CS16AHK_CPP.exe /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib winmm.lib advapi32.lib psapi.lib avrt.lib
)
if errorlevel 1 (
    echo Falha ao compilar.
    exit /b 1
)

echo Compilado: %cd%\build\CS16AHK_CPP.exe
exit /b 0
