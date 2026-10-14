@echo off
REM ---------------------------------------------------------------
REM  Console de testes Marlin - MKS TinyBee / CoreXY
REM  Abre a interface grafica de teste e calibracao.
REM
REM  Portavel: procura um Python que tenha tkinter e pyserial.
REM ---------------------------------------------------------------
setlocal

REM Entra na pasta deste .bat, independente de onde ele foi chamado
cd /d "%~dp0"

REM 1) Launcher oficial do Windows (py -3)
set "PY=py -3"
%PY% -c "import tkinter, serial" >nul 2>&1
if not errorlevel 1 goto :run

REM 2) python do PATH
set "PY=python"
%PY% -c "import tkinter, serial" >nul 2>&1
if not errorlevel 1 goto :run

REM 3) python3 do PATH (Linux/macOS via Git Bash, ou instalacoes alternativas)
set "PY=python3"
%PY% -c "import tkinter, serial" >nul 2>&1
if not errorlevel 1 goto :run

echo ============================================================
echo  Nenhum Python utilizavel foi encontrado.
echo.
echo  E preciso Python 3 com tkinter (vem no instalador oficial
echo  do Windows) e pyserial:
echo.
echo      pip install pyserial
echo.
echo  Observacao: o Python embutido no PlatformIO NAO serve,
echo  porque nao inclui o tkinter.
echo ============================================================
echo.
pause
exit /b 1

:run
title Console Marlin - MKS TinyBee
%PY% marlin_console.py

if errorlevel 1 (
    echo.
    echo ============================================================
    echo  O console terminou com erro ^(codigo %errorlevel%^).
    echo.
    echo  Causa mais comum: porta serial ocupada por outro programa.
    echo ============================================================
    echo.
    pause
)

endlocal
