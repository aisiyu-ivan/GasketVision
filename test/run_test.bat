@echo off
setlocal EnableExtensions
REM 测试环境：生成样品 + 编译 + 启动 HMI_Test / VisionEngine / CameraService（cwd=test/）
pushd "%~dp0.."
set "ROOT=%CD%"
set "BUILD=%ROOT%\build"
set "TEST=%ROOT%\test"
popd

if not exist "%BUILD%\HMI_Test.exe" (
  echo First build: configuring CMake ...
  if not exist "%BUILD%" mkdir "%BUILD%"
  pushd "%BUILD%"
  cmake .. -G Ninja ^
    -DCMAKE_BUILD_TYPE=Debug ^
    -DCMAKE_PREFIX_PATH=F:/QtCreater/6.10.2/mingw_64 ^
    -DCMAKE_CXX_COMPILER=F:/QtCreater/Tools/mingw1310_64/bin/g++.exe ^
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ^
    -DOpenCV_DIR=F:/opencv/mingw-install/x64/mingw/lib
  if errorlevel 1 exit /b 1
  cmake --build . -j 4
  if errorlevel 1 exit /b 1
  popd
)

pushd "%BUILD%"
cmake --build . --target HMI HMI_Test CameraService VisionEngine
if errorlevel 1 (
  popd
  exit /b 1
)
popd

echo Generating synthetic samples ...
cd /d "%TEST%"
if not exist "%TEST%\templates" mkdir "%TEST%\templates"
copy /Y "%ROOT%\config\vision_engine_test.json" "%TEST%\vision_engine.json" >nul
copy /Y "%ROOT%\config\templates\fiducial_L.png" "%TEST%\templates\fiducial_L.png" >nul
python scripts\generate_samples.py --total 1000
if errorlevel 1 (
  echo Failed. Is Python available?
  exit /b 1
)

echo Starting HMI_Test (Engine / Camera detached by HMI_Test) ...
start "" /D "%TEST%" "%BUILD%\HMI_Test.exe"
echo 测试环境已启动 HMI_Test，将自动分离拉起 VisionEngine 与 CameraService。样品: %TEST%\station1
exit /b 0
