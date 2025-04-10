:: Copyright (c) 2023 Google LLC.
::
:: Licensed under the Apache License, Version 2.0 (the "License");
:: you may not use this file except in compliance with the License.
:: You may obtain a copy of the License at
::
::     http://www.apache.org/licenses/LICENSE-2.0
::
:: Unless required by applicable law or agreed to in writing, software
:: distributed under the License is distributed on an "AS IS" BASIS,
:: WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
:: See the License for the specific language governing permissions and
:: limitations under the License.
::
:: Windows Build Script.

@echo on

set BUILD_ROOT=%cd%
set SRC=%cd%\github\effcee
set BUILD_TYPE=%1
set VS_VERSION=%2

:: Force usage of python 3.12
set PATH=C:\python312;%PATH%
set PATH=C:\cmake-3.31.2\bin;%PATH%

cmake --version

:: #########################################
:: set up msvc build env
:: #########################################
if %VS_VERSION% == 2019 (
  call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x64
  echo "Using VS 2019..."
) else if %VS_VERSION% == 2022 (
  call "C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat" x64
  echo "Using VS 2022..."
)

cd %SRC%
python utils/git-sync-deps

mkdir build
cd build

:: #########################################
:: Start building.
:: #########################################
echo "Starting build... %DATE% %TIME%"
if "%KOKORO_GITHUB_COMMIT%." == "." (
  set BUILD_SHA=%KOKORO_GITHUB_PULL_REQUEST_COMMIT%
) else (
  set BUILD_SHA=%KOKORO_GITHUB_COMMIT%
)

set CMAKE_FLAGS=-DCMAKE_INSTALL_PREFIX=%KOKORO_ARTIFACTS_DIR%\install -GNinja -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DRE2_BUILD_TESTING=OFF -DCMAKE_C_COMPILER=cl.exe -DCMAKE_CXX_COMPILER=cl.exe

cmake %CMAKE_FLAGS% ..

if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%

echo "Build everything... %DATE% %TIME%"
ninja
if %ERRORLEVEL% NEQ 0 exit /b %ERRORLEVEL%
echo "Build Completed %DATE% %TIME%"

:: This lets us use !ERRORLEVEL! inside an IF ... () and get the actual error at that point.
setlocal ENABLEDELAYEDEXPANSION

:: ################################################
:: Run the tests
:: ################################################
echo "Running Tests... %DATE% %TIME%"
ctest -C %BUILD_TYPE% --output-on-failure --timeout 300
if !ERRORLEVEL! NEQ 0 exit /b !ERRORLEVEL!
echo "Tests Completed %DATE% %TIME%"

:: ################################################
:: Install and package.
:: ################################################
ninja install

:: Clean up some directories.
rm -rf %SRC%\build
rm -rf %SRC%\external

exit /b 0
