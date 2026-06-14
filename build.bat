@echo off
chcp 65001 >nul
setlocal

echo ============================================
echo   Live2DCompanion - One-Click Build
echo ============================================
echo.

cd /d "%~dp0"

echo [1/2] Building Release...
cmake --build build --config Release
if %errorlevel% neq 0 (
    echo [ERROR] Build failed!
    pause
    exit /b 1
)

echo.
echo [2/2] Build artifacts already in bin\Release\
dir /b bin\Release\Live2DCompanion.exe 2>nul && (
    echo   bin\Release\Live2DCompanion.exe  [OK]
) || (
    echo   [WARN] exe not found in bin\Release
)

echo.
echo ============================================
echo   Build SUCCESS!
echo ============================================
echo.

set /p RUN="Run now? (y/n): "
if /i "%RUN%"=="y" (
    start "" "bin\Release\Live2DCompanion.exe"
)
pause
endlocal
