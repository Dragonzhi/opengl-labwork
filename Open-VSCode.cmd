@echo off
setlocal
set "PATH=%~dp0tools\mingw64\bin;%~dp0tools\cmake\bin;%PATH%"
where code >nul 2>&1
if errorlevel 1 goto manual
call code "%~dp0project"
exit /b %errorlevel%
:manual
echo Open VS Code manually, then use File - Open Folder to open:
echo %~dp0project
pause
