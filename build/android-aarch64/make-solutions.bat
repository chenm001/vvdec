@echo off
::
:: run this batch file to create a Andeoid NDK Makefile for this project.
:: See the cmake documentation for other generator targets
::

SETLOCAL

:: Android 4.4 and above
SET ANDROID_ABI=arm64-v8a
SET ANDROID_NATIVE_API_LEVEL=19
SET ANDROID_PLATFORM=19

SET CMAKE_MAKE_PROGRAM="%ANDROID_NDK%/prebuilt/windows-x86_64/bin/make.exe"

cmake -DCMAKE_TOOLCHAIN_FILE=%ANDROID_NDK%/build/cmake/android.toolchain.cmake -DANDROID_ABI=%ANDROID_ABI% -DANDROID_NATIVE_API_LEVEL=%ANDROID_NATIVE_API_LEVEL% -DANDROID_PLATFORM=%ANDROID_PLATFORM% -DCMAKE_MAKE_PROGRAM=%CMAKE_MAKE_PROGRAM% -G "Unix Makefiles" ..\..\
