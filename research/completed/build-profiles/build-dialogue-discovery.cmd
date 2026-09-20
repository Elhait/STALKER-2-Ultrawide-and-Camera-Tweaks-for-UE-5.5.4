@echo off
setlocal
set "DIALOGUE_DISCOVERY_DIAGNOSTIC=1"
set "DIALOGUE_OUTPUT=STALKER2CameraTweaks_DialogueDiscovery.asi"
call build.cmd
exit /b %errorlevel%
