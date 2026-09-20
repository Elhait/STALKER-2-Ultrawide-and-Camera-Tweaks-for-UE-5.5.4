@echo off
setlocal
set "HORPLUS_FOV_STATE_DIAGNOSTIC=1"
set "ZOOM_TRANSITION_DIAGNOSTIC=1"
set "CAMERA_STATE_SNAPSHOT_DIAGNOSTIC=1"
set "DIALOGUE_OUTPUT=STALKER2CameraTweaks_HorPlusFovStateWideboyDiagnostic.asi"
call build.cmd
exit /b %errorlevel%
