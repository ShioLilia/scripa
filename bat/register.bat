@echo off
rem Register the compiled TSF COM DLL for development testing
rem Usage: run as Administrator after building the DLL into build\ or out\ folder

set DLL_NAME=ScripaTSF.dll
set BUILD_DIR=build

if not exist "%BUILD_DIR%\%DLL_NAME%" (
	echo DLL not found in %BUILD_DIR%: %DLL_NAME%
	echo Please copy the built DLL to %BUILD_DIR% or update the path in this script.
	pause
	exit /b 1
)

echo Registering %DLL_NAME% from %BUILD_DIR%...
regsvr32 /s "%cd%\%BUILD_DIR%\%DLL_NAME%"
if errorlevel 1 (
	echo regsvr32 failed. Try running this script as Administrator.
	pause
	exit /b 1
)
echo Registration complete.
pause
