@echo off
REM ============================================================================
REM LTM B+ Tree - Complete Data Generation & Visualization Pipeline (Windows)
REM ============================================================================

setlocal enabledelayedexpansion
set PROJECT_ROOT=%~dp0
set BUILD_DIR=%PROJECT_ROOT%build
set DATA_DIR=%PROJECT_ROOT%data
set BIN=%BUILD_DIR%\Release\ltm_index.exe

echo.
echo ==============================================================
echo    LTM B+ Tree - Data Generation ^& Visualization
echo ==============================================================
echo.

REM Step 1: Check environment
echo [1] Checking environment...
where cmake >nul 2>&1
if errorlevel 1 (
    echo ERROR: CMake not found!
    exit /b 1
)
where g++ >nul 2>&1
if errorlevel 1 (
    where cl.exe >nul 2>&1
    if errorlevel 1 (
        echo ERROR: C++ compiler not found!
        exit /b 1
    )
)
where python >nul 2>&1
if errorlevel 1 (
    echo ERROR: Python not found!
    exit /b 1
)
echo     OK: All dependencies found
echo.

REM Step 2: Create data directory
echo [2] Creating data directories...
if not exist "%DATA_DIR%" mkdir "%DATA_DIR%"
if not exist "%BUILD_DIR%" mkdir "%BUILD_DIR%"
echo     OK: Directories ready
echo.

REM Step 3: Build C++ project
echo [3] Building C++ project...
cd /d "%BUILD_DIR%"
cmake ..
cmake --build . --config Release
if errorlevel 1 (
    echo ERROR: Build failed!
    exit /b 1
)
echo     OK: Build completed
echo.

REM Step 4: Run tests and data generation
echo [4] Running tests and generating data...
cd /d "%PROJECT_ROOT%"
"%BIN%"
if errorlevel 1 (
    echo ERROR: Data generation failed!
    exit /b 1
)
echo.
echo     OK: Data generation completed
echo     Generated files:
dir "%DATA_DIR%\*.csv" 2>nul || echo     (checking files...)
echo.

REM Step 5: Generate visualizations
echo [5] Generating visualization charts...
python "%PROJECT_ROOT%benchmark_visualizer.py"
if errorlevel 1 (
    echo ERROR: Visualization generation failed!
    exit /b 1
)
echo.
echo     OK: Visualization completed
echo     Generated charts:
dir "%DATA_DIR%\*.png" 2>nul || echo     (checking files...)
echo.

REM Step 6: Summary
echo ==============================================================
echo    ^! Pipeline completed successfully!
echo ==============================================================
echo.
echo ^[Results Location^]: %DATA_DIR%
echo.
echo Data Files:
for %%f in ("%DATA_DIR%\*.csv") do echo    - %%~nxf
echo.
echo Charts:
for %%f in ("%DATA_DIR%\*.png") do echo    - %%~nxf
echo.
echo ==============================================================
echo.
