@echo off
REM ===========================================
REM Automated Build and Cook Script for ue_stereo
REM ===========================================

setlocal enabledelayedexpansion

REM Configuration
set ENGINE_PATH=C:\Program Files\Epic Games\UE_5.7
set PROJECT_PATH=%~dp0ue_stereo.uproject
set PROJECT_NAME=ue_stereo
set OUTPUT_PATH=%~dp0Build\WindowsNoEditor
set BUILD_CONFIG=Development
set PLATFORM=Win64

REM Unreal Engine Tools
set UBT=%ENGINE_PATH%\Engine\Binaries\DotNET\UnrealBuildTool\UnrealBuildTool.exe
set UAT=%ENGINE_PATH%\Engine\Build\BatchFiles\RunUAT.bat

echo.
echo ============================================
echo Starting Build and Cook Process
echo ============================================
echo Engine Path: %ENGINE_PATH%
echo Project: %PROJECT_NAME%
echo Platform: %PLATFORM%
echo Configuration: %BUILD_CONFIG%
echo Output Path: %OUTPUT_PATH%
echo ============================================
echo.

REM Check if project file exists
if not exist "%PROJECT_PATH%" (
    echo ERROR: Project file not found at %PROJECT_PATH%
    exit /b 1
)

REM Check if engine exists
if not exist "%ENGINE_PATH%" (
    echo ERROR: Unreal Engine not found at %ENGINE_PATH%
    exit /b 1
)

echo.
echo [Step 1/3] Building Editor Binaries...
echo ============================================
"%UBT%" %PROJECT_NAME%Editor %PLATFORM% Development -Project="%PROJECT_PATH%" -Progress -NoHotReloadFromIDE
if errorlevel 1 (
    echo ERROR: Failed to build editor binaries
    exit /b 1
)
echo Editor build completed successfully!

echo.
echo [Step 2/3] Building Game Binaries...
echo ============================================
"%UBT%" %PROJECT_NAME% %PLATFORM% %BUILD_CONFIG% -Project="%PROJECT_PATH%" -Progress -NoHotReloadFromIDE
if errorlevel 1 (
    echo ERROR: Failed to build game binaries
    exit /b 1
)
echo Game build completed successfully!

echo.
echo [Step 3/3] Cooking and Packaging...
echo ============================================
call "%UAT%" BuildCookRun -project="%PROJECT_PATH%" -platform=%PLATFORM% -clientconfig=%BUILD_CONFIG% -serverconfig=%BUILD_CONFIG% -cook -stage -package -pak -archive -archivedirectory="%OUTPUT_PATH%" -build -noP4 -utf8output
if errorlevel 1 (
    echo ERROR: Failed to cook and package
    exit /b 1
)

echo.
echo ============================================
echo Build and Cook Process Completed Successfully!
echo ============================================
echo.
echo Output Location: %OUTPUT_PATH%
echo.

endlocal
pause
