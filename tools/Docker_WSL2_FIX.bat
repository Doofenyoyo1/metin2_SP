@echo off
setlocal EnableExtensions
title Docker + WSL2 Setup

rem --- Elevate to Administrator if needed ---
fltmc >nul 2>&1
if errorlevel 1 (
  echo Requesting Administrator privileges...
  powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
  exit /b
)

set "MARKER=%ProgramData%\DockerWSL2_setup_phase1.done"

echo.
echo =============================================
echo        Docker / WSL2 automatic setup
echo =============================================
echo.

if exist "%MARKER%" goto PHASE2

:PHASE1
echo [1/3] Enabling Virtual Machine Platform...
dism.exe /online /enable-feature /featurename:VirtualMachinePlatform /all /norestart

echo.
echo [2/3] Enabling Windows Subsystem for Linux...
dism.exe /online /enable-feature /featurename:Microsoft-Windows-Subsystem-Linux /all /norestart

echo.
echo [3/3] Installing WSL components...
wsl.exe --install --no-distribution

echo phase1_done>"%MARKER%"

echo.
echo =============================================
echo First stage finished.
echo RESTART the PC, then run THIS SAME BAT again.
echo =============================================
echo.
choice /C YN /N /M "Restart now? [Y/N]: "
if errorlevel 2 goto NORESTART
shutdown.exe /r /t 5
exit /b

:NORESTART
echo.
echo Restart Windows manually, then run this BAT again.
pause
exit /b

:PHASE2
echo [1/3] Updating WSL...
wsl.exe --update

echo.
echo [2/3] Setting WSL 2 as default...
wsl.exe --set-default-version 2

echo.
echo [3/3] WSL status:
wsl.exe --status

del "%MARKER%" >nul 2>&1

echo.
echo =============================================
echo DONE. Windows is ready for Docker Desktop.
echo In Docker Desktop use the WSL 2 based engine.
echo =============================================
echo.
pause
endlocal
