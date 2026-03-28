@echo off

cd /d %~dp0

echo ===================================
echo FlatBuffers Schema Builder
echo ===================================

python build_flatbuffers.py

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo Build Failed
    pause
    exit /b 1
)

echo.
echo Build Success
pause