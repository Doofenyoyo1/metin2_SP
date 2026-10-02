@echo off
rem Metin2 SinglePlayer - dolaczenie do swiata znajomego (COOP): kod zaproszenia -> coop.cfg
powershell.exe -NoProfile -ExecutionPolicy Bypass -STA -File "%~dp0Dolacz.ps1"
if errorlevel 1 pause
