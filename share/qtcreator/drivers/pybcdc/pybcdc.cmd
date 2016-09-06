@echo off
echo Installing Python Board Driver...

ver | findstr /c:"Version 5." > nul
if %ERRORLEVEL%==0 goto :install_old

if DEFINED PROCESSOR_ARCHITEW6432 goto :install_new_wow

:install_new
%SystemRoot%\System32\InfDefaultInstall.exe "%~dp0\pybcdc.inf"
exit

:install_new_wow
%SystemRoot%\Sysnative\cmd.exe /c %SystemRoot%\System32\InfDefaultInstall.exe "%~dp0\pybcdc.inf"
exit

:install_old
%SystemRoot%\System32\rundll32.exe %SystemRoot%\System32\setupapi.dll,InstallHinfSection DefaultInstall 132 "%~dp0\pybcdc.inf"
exit
