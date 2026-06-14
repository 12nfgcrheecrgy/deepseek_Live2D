@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" >/dev/null 2>&1
cd /d "Z:\deepseek_live2D"
rmdir /s /q build\msvc 2>nul
cmake -S . -B build\msvc -G Ninja -DCMAKE_BUILD_TYPE=Release
if %errorlevel% neq 0 exit /b 1
cmake --build build\msvc --config Release
