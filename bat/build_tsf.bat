@echo off
echo Building Scripa TSF Input Method DLL...
echo.

REM Set up Visual Studio environment
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

REM Navigate to build directory
cd /d "%~dp0..\build"

REM Compile all TSF source files
echo Compiling TSF sources...
cl /c /std:c++17 /utf-8 /EHsc /W3 /Zi ^
   /D_UNICODE /DUNICODE ^
   /I"..\src" ^
   ..\src\tsf\dllmain.cpp ^
   ..\src\tsf\TextService.cpp ^
   ..\src\tsf\ScripaTSF.cpp ^
   ..\src\tsf\ScripaTSFCOM.cpp ^
   ..\src\tsf\CandidateWindow.cpp

if errorlevel 1 (
    echo Compilation failed!
    pause
    exit /b 1
)

REM Link into DLL
echo Linking DLL...
link /DLL /DEF:..\src\tsf\ScripaTSF.def ^
     /OUT:ScripaTSF.dll ^
     /PDB:ScripaTSF.pdb ^
     dllmain.obj TextService.obj ScripaTSF.obj ScripaTSFCOM.obj CandidateWindow.obj ^
     ole32.lib oleaut32.lib uuid.lib user32.lib advapi32.lib gdiplus.lib

if errorlevel 1 (
    echo Linking failed!
    pause
    exit /b 1
)

echo.
echo Build successful! ScripaTSF.dll created in build directory.
echo.
echo Next steps:
echo 1. Run bat\register.bat as Administrator to register the input method
echo 2. Go to Windows Settings ^> Time ^& Language ^> Language to add it
echo.
pause
