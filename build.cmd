@echo off
setlocal
set "ROOT=%~dp0"
set "PATH=%ROOT%tools\mingw64\bin;%ROOT%tools\cmake\bin;%PATH%"
if not exist "%ROOT%tools\mingw64\bin\g++.exe" goto missing
if not exist "%ROOT%tools\cmake\bin\cmake.exe" goto missing
if not exist "%ROOT%project\build\CMakeCache.txt" goto configure
cmake --build "%ROOT%project\build" --parallel
if errorlevel 1 goto failed
exit /b 0
:configure
cmake --fresh -S "%ROOT%project" -B "%ROOT%project\build" -G "MinGW Makefiles" "-DCMAKE_C_COMPILER=%ROOT%tools/mingw64/bin/gcc.exe" "-DCMAKE_CXX_COMPILER=%ROOT%tools/mingw64/bin/g++.exe" "-DCMAKE_MAKE_PROGRAM=%ROOT%tools/mingw64/bin/mingw32-make.exe" -DCMAKE_BUILD_TYPE=Debug
if errorlevel 1 goto failed
cmake --build "%ROOT%project\build" --parallel
if errorlevel 1 goto failed
exit /b 0
:failed
echo Build failed. If you moved this folder, delete project\build and run again.
exit /b 1
:missing
echo Tool files missing. Extract the complete OpenGL-Lab folder first.
exit /b 1
