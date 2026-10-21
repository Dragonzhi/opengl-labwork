@echo off
setlocal
call "%~dp0build.cmd"
if errorlevel 1 goto failed
"%~dp0project\build\triangle.exe"
if errorlevel 1 goto failed
exit /b 0
:failed
echo Build or run failed. Please copy the errors above to your teacher.
pause
exit /b 1
