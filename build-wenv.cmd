@echo off
rem Create a Windows virtual environment and install dependencies (GUI uses PySide6)
rem Usage: build-wenv.cmd

setlocal enabledelayedexpansion

set SCRIPT_DIR=%~dp0
set VENV_DIR=%SCRIPT_DIR%wenv

echo ==========================================
echo  Create Windows virtual environment (wenv)
echo ==========================================

where python >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo ERROR: python not found, make sure it is installed and on PATH
    exit /b 1
)
for /f "delims=" %%v in ('python --version') do echo Python: %%v

if exist "%VENV_DIR%" (
    echo Virtual environment already exists: %VENV_DIR%
    set /p confirm="Delete and rebuild? [y/N] "
    if /i "!confirm!"=="y" (
        rmdir /s /q "%VENV_DIR%"
    ) else (
        echo Skip creation, only update dependencies...
        goto install
    )
)

echo Creating virtual environment...
python -m venv "%VENV_DIR%"

:install
echo Installing dependencies...
"%VENV_DIR%\Scripts\python.exe" -m pip install --upgrade pip -q
if %ERRORLEVEL% neq 0 (
    echo ERROR: failed to upgrade pip
    exit /b 1
)
"%VENV_DIR%\Scripts\pip.exe" install -r "%SCRIPT_DIR%requirements-windows.txt"
if %ERRORLEVEL% neq 0 (
    echo ERROR: failed to install dependencies
    exit /b 1
)

echo Generating Protobuf files...
"%VENV_DIR%\Scripts\python.exe" "%SCRIPT_DIR%src\generate_proto.py"
if %ERRORLEVEL% neq 0 (
    echo ERROR: protobuf code generation failed
    exit /b 1
)

echo.
echo ==========================================
echo  Done!
echo  Activate (cmd)      : wenv\Scripts\activate
echo  Activate (Git Bash) : source wenv/Scripts/activate
echo  Run: python example\main.py
echo ==========================================

endlocal
