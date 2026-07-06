@echo off
setlocal

echo ========================================
echo  DirectComposition App Build Script
echo ========================================

:: Check for Visual Studio
where cmake >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: cmake not found in PATH
    echo Please install CMake or add it to PATH
    pause
    exit /b 1
)

:: Create build directory
if not exist build mkdir build

echo.
echo Configuring CMake...
cd build
cmake .. -A x64
if %errorlevel% neq 0 (
    echo ERROR: CMake configuration failed
    pause
    exit /b 1
)

echo.
echo Building project...
cmake --build . --config Release
if %errorlevel% neq 0 (
    echo ERROR: Build failed
    pause
    exit /b 1
)

echo.
echo ========================================
echo  Build successful!
echo  Output: build\Release\DirectCompositionApp.exe
echo ========================================

pause
