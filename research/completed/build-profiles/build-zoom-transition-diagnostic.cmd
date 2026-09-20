@echo off
setlocal
set "ZOOM_TRANSITION_DIAGNOSTIC=1"
set "DIALOGUE_OUTPUT=STALKER2CameraTweaks_ZoomTransitionDiagnostic.asi"
call build.cmd
exit /b %errorlevel%
