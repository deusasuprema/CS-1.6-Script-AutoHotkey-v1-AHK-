@echo off
setlocal

set "AHK2EXE=C:\Program Files\AutoHotkey\Compiler\Ahk2Exe.exe"
set "BASE=C:\Program Files\AutoHotkey\Compiler\AutoHotkeySC.bin"
set "SCRIPT=%~dp0CS 1.6 H.ahk"
set "OUTPUT=%~dp0CS 1.6 H.exe"
set "ICON=%~dp0.extracted\.rsrc\ICON\8.ico"

if not exist "%AHK2EXE%" (
    echo Ahk2Exe nao encontrado em:
    echo %AHK2EXE%
    pause
    exit /b 1
)

if not exist "%BASE%" (
    echo AutoHotkeySC.bin nao encontrado em:
    echo %BASE%
    pause
    exit /b 1
)

"%AHK2EXE%" /in "%SCRIPT%" /out "%OUTPUT%" /base "%BASE%" /icon "%ICON%"
if errorlevel 1 (
    echo.
    echo Falha ao compilar.
    pause
    exit /b 1
)

echo.
echo Compilado: %OUTPUT%
pause
