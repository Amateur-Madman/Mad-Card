@echo off
setlocal

REM --- Disable NuGet vulnerability auditing for this VS session only ---
set NuGetAudit=false

REM --- Prevent NU1901-NU1904 from failing restore if warnings-as-errors is enabled ---
set WarningsNotAsErrors=NU1901;NU1902;NU1903;NU1904

REM --- Optional if NU1701 is also blocking builds ---
set NoWarn=NU1701

REM --- Find the first .sln in this folder ---
set SLN_FILE=
for %%f in ("%~dp0*.sln") do (
    set SLN_FILE=%%f
    goto :found
)

echo No .sln file found in this folder.
pause
exit /b

:found
echo Launching Visual Studio with %SLN_FILE%

start "" "%ProgramFiles%\Microsoft Visual Studio\2022\Community\Common7\IDE\devenv.exe" "%SLN_FILE%"