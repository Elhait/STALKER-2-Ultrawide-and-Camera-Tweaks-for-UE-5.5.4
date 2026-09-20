@echo off
setlocal
set "DIALOGUE_DIAGNOSTIC=1"
set "DIALOGUE_OUTPUT=STALKER2CameraTweaks_DialogueDiagnostic.asi"
call build.cmd
exit /b %errorlevel%
