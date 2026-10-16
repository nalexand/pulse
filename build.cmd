@echo off
setlocal
pushd "%~dp0"
set "PULSE_VSROOT="
for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "PULSE_VSROOT=%%i"
if not defined PULSE_VSROOT (
    echo Visual Studio C++ Build Tools and the Windows SDK are required.
    exit /b 1
)
call "%PULSE_VSROOT%\VC\Auxiliary\Build\vcvars64.bat"
if not "%errorlevel%"=="0" exit /b 1
if not exist "vendor\JUCE\CMakeLists.txt" (
    git clone --depth 1 --branch 8.0.16-security https://github.com/juce-framework/JUCE.git vendor/JUCE
    if not "%errorlevel%"=="0" exit /b 1
)
set "CMAKE_BUILD_PARALLEL_LEVEL=3"
rem Configure JUCE's helper first so compiler probes use Release on this machine.
cmake -S vendor/JUCE -B build-native/vendor/JUCE/tools -G Ninja -DJUCE_BUILD_HELPER_TOOLS=ON -DCMAKE_BUILD_TYPE=Custom -DCMAKE_TRY_COMPILE_CONFIGURATION=Release
if not "%errorlevel%"=="0" exit /b 1
cmake -S . -B build-native -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TRY_COMPILE_CONFIGURATION=Release
if not "%errorlevel%"=="0" exit /b 1
cmake --build build-native -j 3
if not "%errorlevel%"=="0" exit /b 1
ctest --test-dir build-native --output-on-failure
if not "%errorlevel%"=="0" exit /b 1
echo Build verified. Use run.cmd to open Pulse Studio.
popd

