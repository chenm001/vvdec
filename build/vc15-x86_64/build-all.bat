@echo off
if "%VS150COMNTOOLS%" == "" (
  msg "%username%" "Visual Studio 15 not detected"
  exit 1
)
if not exist vvdec.sln (
  call make-solutions.bat
)
if exist vvdec.sln (
  call "%VS150COMNTOOLS%\..\..\VC\vcvarsall.bat"
  MSBuild /property:Configuration="Release" vvdec.sln
  MSBuild /property:Configuration="Debug" vvdec.sln
  MSBuild /property:Configuration="RelWithDebInfo" vvdec.sln
)
