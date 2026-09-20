@echo off
setlocal
set "CAMERA_TWEAKS_BUILD_PROFILE=diagnostic"
call "%~dp0build.cmd"
exit /b %errorlevel%
