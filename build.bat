@echo off
setlocal enabledelayedexpansion

rem OpenAPI-XPanel Windows packaging script - uses Nuitka(MSVC) + PySide6
rem Usage: build.bat [version]   version is optional, defaults to V0.0.0
rem Output: output\OpenAPI-XPanel.exe

set APP_VERSION=%~1
if "%APP_VERSION%"=="" set APP_VERSION=V0.0.0

echo ==========================================
echo  OpenAPI-XPanel %APP_VERSION% Build (MSVC + PySide6)
echo ==========================================

echo [1/3] Initializing MSVC x64 environment...
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if %ERRORLEVEL% neq 0 (
    echo ERROR: MSVC environment init failed
    exit /b 1
)

set SCRIPT_DIR=%~dp0
set DIST_DIR=%SCRIPT_DIR%output
if not exist "%DIST_DIR%" mkdir "%DIST_DIR%"

echo [2/3] Writing version and build time...
for /f "delims=" %%t in ('powershell -Command "Get-Date -Format 'yyyy/MM/dd HH:mm:ss'"') do set BUILD_TIME=%%t
echo Build time: %BUILD_TIME%
echo Generating protobuf files...
"%SCRIPT_DIR%wenv\Scripts\python.exe" "%SCRIPT_DIR%src\generate_proto.py"
powershell -Command "(Get-Content '%SCRIPT_DIR%example\main.py' -Encoding UTF8) -replace '^APP_VERSION = .*', 'APP_VERSION = \"%APP_VERSION%\"' | Set-Content '%SCRIPT_DIR%example\main.py' -Encoding UTF8"
powershell -Command "(Get-Content '%SCRIPT_DIR%example\main.py' -Encoding UTF8) -replace '^APP_BUILD_TIME = .*', 'APP_BUILD_TIME = \"%BUILD_TIME%\"' | Set-Content '%SCRIPT_DIR%example\main.py' -Encoding UTF8"

echo [3/3] Compiling with Nuitka (MSVC)...
"%SCRIPT_DIR%wenv\Scripts\python.exe" -m nuitka ^
    --standalone --onefile ^
    --output-dir="%DIST_DIR%" ^
    --output-filename="OpenAPI-XPanel.exe" ^
    --enable-plugin=pyside6 ^
    --include-package=example ^
    --include-package=example.core ^
    --include-package=example.gui ^
    --include-package=src ^
    --include-package=src.proto_gen ^
    --include-package=src.proto_gen.openapi ^
    --include-module=google.protobuf ^
    --include-module=google.protobuf.descriptor ^
    --include-module=google.protobuf.message ^
    --include-module=google.protobuf.json_format ^
    --include-module=google.protobuf.symbol_database ^
    --include-module=paho.mqtt.client ^
    --include-data-dir="%SCRIPT_DIR%assets"=assets ^
    --include-data-files="%SCRIPT_DIR%example\i18n\openapi_zh.qm"=example/i18n/openapi_zh.qm ^
    --windows-icon-from-ico="%SCRIPT_DIR%assets\icon.ico" ^
    --nofollow-import-to=grpc_tools ^
    --nofollow-import-to=tkinter ^
    --nofollow-import-to=unittest ^
    --nofollow-import-to=pytest ^
    --windows-console-mode=disable ^
    --assume-yes-for-downloads ^
    --windows-company-name=XGRIDS ^
    --windows-product-name=OpenAPI-XPanel ^
    --windows-file-version=%APP_VERSION:~1% ^
    --windows-product-version=%APP_VERSION:~1% ^
    "--windows-file-description=OpenAPI-XPanel" ^
    --jobs=%NUMBER_OF_PROCESSORS% ^
    "%SCRIPT_DIR%example\main.py"

set BUILD_RESULT=%ERRORLEVEL%

echo Restoring version and build time fields...
powershell -Command "(Get-Content '%SCRIPT_DIR%example\main.py' -Encoding UTF8) -replace '^APP_VERSION = .*', 'APP_VERSION = \"V0.0.0\"  # Automatically filled in by the build script' | Set-Content '%SCRIPT_DIR%example\main.py' -Encoding UTF8"
powershell -Command "(Get-Content '%SCRIPT_DIR%example\main.py' -Encoding UTF8) -replace '^APP_BUILD_TIME = .*', 'APP_BUILD_TIME = \"\"  # Automatically filled in by the build script' | Set-Content '%SCRIPT_DIR%example\main.py' -Encoding UTF8"

if %BUILD_RESULT% neq 0 (
    echo ERROR: Nuitka build failed
    exit /b 1
)

set EXE=%DIST_DIR%\OpenAPI-XPanel.exe
if exist "%EXE%" (
    for %%f in ("%EXE%") do set /a MB=%%~zf / 1048576
    echo.
    echo ==========================================
    echo  Build complete!
    echo  Output: %EXE%
    echo  Size: !MB! MB
    echo ==========================================
) else (
    echo ERROR: No exe generated
    exit /b 1
)

endlocal
