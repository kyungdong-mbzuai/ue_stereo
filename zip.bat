@echo off
setlocal

REM 1. Generate date and time for the filename
for /f %%i in ('powershell -NoProfile -Command "Get-Date -Format 'yyyyMMdd_HHmmss'"') do set "timestamp=%%i"

REM 2. Identify the .uproject file
set "uprojectfile="
for %%f in (*.uproject) do set "uprojectfile=%%f"

if not defined uprojectfile (
    echo Error: No .uproject file found in the current directory.
    pause
    exit /b 1
)

for %%f in ("%uprojectfile%") do set "projectname=%%~nf"
set "outputfilename=Archives\%projectname%_%timestamp%.zip"
set "temp_dir=%CD%\_temp_zip"

echo [1/3] Creating temporary structure and filtering folders...

REM 3. Use Robocopy to clone the structure while excluding specific paths
REM We use absolute paths in /XD to ensure precise filtering of Plugins\UROSBridge subfolders.

set "CNT_EXCLUDE_LIST="%CD%\Content\MetaHumans""
set "PLG_EXCLUDE_LIST="%CD%\Plugins\UROSBridge\Binaries" "%CD%\Plugins\UROSBridge\Intermediate" "%CD%\Plugins\LiveLinkViconDataStream\Intermediate""

robocopy "Source" "%temp_dir%\Source" /E /NFL /NDL /NJH /NJS
robocopy "Content" "%temp_dir%\Content" /E /XD %CNT_EXCLUDE_LIST% /NFL /NDL /NJH /NJS
robocopy "Config" "%temp_dir%\Config" /E /NFL /NDL /NJH /NJS
robocopy "Plugins" "%temp_dir%\Plugins" /E /XD %PLG_EXCLUDE_LIST% /NFL /NDL /NJH /NJS

copy "%uprojectfile%" "%temp_dir%\" >nul
copy "*.bat" "%temp_dir%\" >nul

echo [2/3] Generating archive (maintaining hierarchy): %outputfilename%

REM 4. Use PowerShell to compress the temporary folder content in one go
powershell -NoProfile -Command "Compress-Archive -Path '%temp_dir%\*' -DestinationPath '%outputfilename%' -Force"

echo [3/3] Cleaning up temporary files...

REM 5. Delete the temporary directory
if exist "%temp_dir%" rd /s /q "%temp_dir%"

if %errorlevel% equ 0 (
    echo.
    echo Success: %outputfilename% created successfully.
) else (
    echo.
    echo Failure: An error occurred during the compression process.
)

pause