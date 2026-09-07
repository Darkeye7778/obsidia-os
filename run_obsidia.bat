@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
if defined OBSIDIA_WSL_REPO set "WSL_REPO=%OBSIDIA_WSL_REPO%"
if defined WSL_REPO goto :repo_ready
set "UNC_REMAINDER=%SCRIPT_DIR:\\wsl.localhost\=%"
if not "%UNC_REMAINDER%"=="%SCRIPT_DIR%" goto :parse_wsl_unc
for /f "delims=" %%D in ('wsl.exe wslpath -u "%SCRIPT_DIR%"') do if not defined WSL_REPO set "WSL_REPO=%%D"
goto :repo_ready

:parse_wsl_unc
for /f "tokens=1,* delims=\" %%D in ("%UNC_REMAINDER%") do set "WSL_REPO_RAW=%%E"
set "WSL_REPO=/%WSL_REPO_RAW:\=/%"

:repo_ready
if not defined WSL_REPO (
    echo Could not translate the repository path into WSL.
    echo Set OBSIDIA_WSL_REPO to the Linux repository path and try again.
    exit /b 1
)

pushd "%SCRIPT_DIR%" || exit /b 1
wsl.exe --cd "%WSL_REPO%" make
if errorlevel 1 goto :failed

if defined OBSIDIA_QEMU_WINDOWS set "QEMU_EXE=%OBSIDIA_QEMU_WINDOWS%"
if not defined QEMU_EXE for /f "delims=" %%Q in ('where qemu-system-x86_64.exe 2^>nul') do if not defined QEMU_EXE set "QEMU_EXE=%%Q"
if not defined QEMU_EXE if exist "%ProgramFiles%\qemu\qemu-system-x86_64.exe" set "QEMU_EXE=%ProgramFiles%\qemu\qemu-system-x86_64.exe"
if not defined QEMU_EXE if exist "%ProgramFiles(x86)%\qemu\qemu-system-x86_64.exe" set "QEMU_EXE=%ProgramFiles(x86)%\qemu\qemu-system-x86_64.exe"

if not defined QEMU_EXE (
    echo Windows-native QEMU was not found.
    echo Install QEMU for Windows, add it to PATH, or set OBSIDIA_QEMU_WINDOWS.
    goto :failed
)

if not defined OBSIDIA_QEMU_WINDOWS_DISPLAY set "OBSIDIA_QEMU_WINDOWS_DISPLAY=sdl,gl=off"
"%QEMU_EXE%" -machine pc -display "%OBSIDIA_QEMU_WINDOWS_DISPLAY%" -cdrom "%CD%\build\obsidia.iso" -serial stdio -drive "file=%CD%\build\obsidia_disk.img,format=raw,if=ide" -m 256
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%

:failed
popd
exit /b 1
