@echo off
setlocal
set "VSDEVCMD="
if defined VSINSTALLDIR if exist "%VSINSTALLDIR%Common7\Tools\VsDevCmd.bat" set "VSDEVCMD=%VSINSTALLDIR%Common7\Tools\VsDevCmd.bat"
if not defined VSDEVCMD if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" (
    pushd "%ProgramFiles(x86)%\Microsoft Visual Studio\Installer"
    for /f "delims=" %%I in ('vswhere.exe -latest -products * -version "[17.14,18.0)" -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -find Common7\Tools\VsDevCmd.bat') do set "VSDEVCMD=%%I"
    popd
)
if not defined VSDEVCMD (
    echo Visual Studio 2022 C++ toolchain not found.>&2
    exit /b 1
)
call "%VSDEVCMD%" -arch=x64 -host_arch=x64
if errorlevel 1 exit /b %errorlevel%
rem MSVC 17.14 exposes the required C++23 feature set through /std:c++latest.
if not exist "build-artifacts\obj" mkdir "build-artifacts\obj"
set "DIALOGUE_DIAGNOSTIC_DEFINE="
set "DIALOGUE_DISCOVERY_DEFINE="
set "ZOOM_TRANSITION_DEFINE="
set "HORPLUS_FOV_STATE_DEFINE="
set "DIALOGUE_RECOVERY_ENDPOINT_DEFINE="
set "CAMERA_STATE_SNAPSHOT_DEFINE="
set "SUPPORTED_DIAGNOSTICS_DEFINE="
set "DIALOGUE_OUTPUT=STALKER2CameraTweaks.asi"
if /I "%CAMERA_TWEAKS_BUILD_PROFILE%"=="diagnostic" (
    set "SUPPORTED_DIAGNOSTICS_DEFINE=/DZOOM_TRANSITION_DIAGNOSTIC /DHORPLUS_FOV_STATE_DIAGNOSTIC /DCAMERA_STATE_SNAPSHOT_DIAGNOSTIC /DHORPLUS_CINEMATIC_WRITER_TRACE_DIAGNOSTIC /DMATCHGAMEPLAY_DIAGNOSTIC"
    if /I "%DIALOGUE_DIAGNOSTIC%"=="1" set "DIALOGUE_DIAGNOSTIC_DEFINE=/DDIALOGUE_BOUNDARY_DIAGNOSTIC"
    if /I "%DIALOGUE_DISCOVERY_DIAGNOSTIC%"=="1" set "DIALOGUE_DISCOVERY_DEFINE=/DDIALOGUE_DISCOVERY_DIAGNOSTIC"
    if /I "%ZOOM_TRANSITION_DIAGNOSTIC%"=="1" set "ZOOM_TRANSITION_DEFINE=/DZOOM_TRANSITION_DIAGNOSTIC"
    if /I "%HORPLUS_FOV_STATE_DIAGNOSTIC%"=="1" set "HORPLUS_FOV_STATE_DEFINE=/DHORPLUS_FOV_STATE_DIAGNOSTIC"
    if /I "%DIALOGUE_RECOVERY_ENDPOINT_DIAGNOSTIC%"=="1" set "DIALOGUE_RECOVERY_ENDPOINT_DEFINE=/DDIALOGUE_RECOVERY_ENDPOINT_DIAGNOSTIC"
    if /I "%CAMERA_STATE_SNAPSHOT_DIAGNOSTIC%"=="1" set "CAMERA_STATE_SNAPSHOT_DEFINE=/DCAMERA_STATE_SNAPSHOT_DIAGNOSTIC"
    set "DIALOGUE_OUTPUT=STALKER2CameraTweaksDiagnostic.asi"
)
cl /nologo /LD /std:c++latest /O1 /MT /EHsc /W4 /utf-8 /DNDEBUG %SUPPORTED_DIAGNOSTICS_DEFINE% %DIALOGUE_DIAGNOSTIC_DEFINE% %DIALOGUE_DISCOVERY_DEFINE% %ZOOM_TRANSITION_DEFINE% %HORPLUS_FOV_STATE_DEFINE% %DIALOGUE_RECOVERY_ENDPOINT_DEFINE% %CAMERA_STATE_SNAPSHOT_DEFINE% /Fobuild-artifacts\obj\ /Iexternal\safetyhook /Iexternal\spdlog\include src\plugin\runtime.cpp src\plugin\worker_lifecycle.cpp src\plugin\feature_status.cpp src\plugin\dll_entry.cpp src\config\feature_config.cpp src\config\config_repository.cpp src\config\config_template.cpp src\diagnostics\diagnostic_runtime.cpp src\hooks\signature_scanner.cpp src\hooks\instruction_validator.cpp src\hooks\hook_set.cpp src\gameplay\gameplay_state.cpp src\gameplay\gameplay_camera.cpp src\gameplay\aspect_policy.cpp src\gameplay\horplus_gameplay.cpp src\cinematics\cinematic_fov.cpp src\cinematics\cinematic_selection.cpp src\cinematics\cinematic_aspect.cpp src\cinematics\cinematic_initialization.cpp src\dialogue\dialogue_fov.cpp src\dialogue\dialogue_state.cpp src\camera\camera_state_snapshot.cpp src\camera\fov_observation.cpp src\camera\gameplay_baseline.cpp src\camera\gameplay_aspect_restoration.cpp src\camera\presentation_state.cpp src\camera\horplus.cpp src\platform\win32\memory.cpp src\platform\win32\sha256.cpp src\platform\win32\window.cpp src\platform\win32\viewport.cpp external\safetyhook\safetyhook.cpp external\safetyhook\Zydis.c /link user32.lib bcrypt.lib /OUT:%DIALOGUE_OUTPUT%
exit /b %errorlevel%
