@echo off

ver | findstr /c:"Version 5." > nul
if %ERRORLEVEL%==0 goto :install_old

:install_new
%SystemRoot%\System32\InfDefaultInstall.exe %~dp0\pybcdc.inf
rem %SystemRoot%\System32\rundll32.exe advpack.dll,LaunchINFSectionEx %~dp0\pybcdc.inf,,,4,N
goto :eof

:install_old
%SystemRoot%\System32\rundll32.exe setupapi,InstallHinfSection DefaultInstall 132 %~dp0\pybcdc.inf
rem %SystemRoot%\System32\rundll32.exe advpack.dll,LaunchINFSectionEx %~dp0\pybcdc.inf,,,4,N
