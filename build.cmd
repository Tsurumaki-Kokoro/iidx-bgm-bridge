@echo off
setlocal
cd /d "%~dp0"
if defined VCVARS64 goto setup
for /f "usebackq tokens=*" %%I in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%I"
if not defined VSROOT exit /b 1
set "VCVARS64=%VSROOT%\VC\Auxiliary\Build\vcvars64.bat"
:setup
call "%VCVARS64%"
if errorlevel 1 exit /b 1
if not exist build mkdir build
cl /nologo /TC /W3 /O2 /MT /c vendor\minhook\src\buffer.c vendor\minhook\src\hook.c vendor\minhook\src\trampoline.c vendor\minhook\src\hde\hde64.c /Fo:build\
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /utf-8 /W4 /WX /O2 /MT /EHsc /Iinclude /LD src\plugin.cpp build\buffer.obj build\hook.obj build\trampoline.obj build\hde64.obj /Fo:build\plugin.obj /Fe:build\bgm_bridge.dll /link bcrypt.lib /INCREMENTAL:NO
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /utf-8 /W4 /WX /O2 /MT /EHsc /Iinclude tests\test_core.cpp build\buffer.obj build\hook.obj build\trampoline.obj build\hde64.obj /Fo:build\test_core.obj /Fe:build\bgm-tests.exe /link /INCREMENTAL:NO
if errorlevel 1 exit /b 1
build\bgm-tests.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /utf-8 /W4 /WX /O2 /MT /EHsc tools\audio_codec.cpp /Fo:build\audio_codec.obj /Fe:build\audio_codec.exe /link msacm32.lib /INCREMENTAL:NO
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /utf-8 /W4 /WX /O2 /MT /EHsc tools\loudness_meter.cpp /Fo:build\loudness_meter.obj /Fe:build\loudness_meter.exe /link /INCREMENTAL:NO
exit /b %errorlevel%
