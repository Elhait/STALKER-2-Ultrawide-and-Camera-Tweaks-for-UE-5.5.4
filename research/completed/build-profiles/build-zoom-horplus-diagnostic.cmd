@echo off
setlocal
set "ZOOM_TRANSITION_DIAGNOSTIC=1"
set "HORPLUS_FOV_STATE_DIAGNOSTIC=1"
set "DIALOGUE_OUTPUT=STALKER2CameraTweaks_ZoomHorPlusDiagnostic.asi"
call build.cmd
exit /b %errorlevel%
