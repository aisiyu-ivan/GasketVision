@echo off
setlocal EnableExtensions
REM 正式环境：编译 + 启动 HMI / VisionEngine / CameraService（cwd=build/）
set "ROOT=%~dp0"
set "BUILD=%ROOT%build"
cd /d "%ROOT%"

if not exist "%BUILD%\HMI.exe" (
  echo First build: configuring CMake ...
  if not exist "%BUILD%" mkdir "%BUILD%"
  cd /d "%BUILD%"
  cmake .. -G Ninja ^
    -DCMAKE_BUILD_TYPE=Debug ^
    -DCMAKE_PREFIX_PATH=F:/QtCreater/6.10.2/mingw_64 ^
    -DCMAKE_CXX_COMPILER=F:/QtCreater/Tools/mingw1310_64/bin/g++.exe ^
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ^
    -DOpenCV_DIR=F:/opencv/mingw-install/x64/mingw/lib
  if errorlevel 1 exit /b 1
  cmake --build . -j 4
  if errorlevel 1 exit /b 1
  cd /d "%ROOT%"
)

cd /d "%BUILD%"
cmake --build . --target HMI HMI_Test CameraService VisionEngine
if errorlevel 1 exit /b 1

echo Starting HMI (VisionEngine / CameraService will be launched detached by HMI) ...
start "" "HMI.exe"
echo 正式环境已启动 HMI，将自动分离拉起 VisionEngine 与 CameraService。工作目录: %CD%
exit /b 0
