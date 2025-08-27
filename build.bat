@echo off
echo 创建build目录...
if not exist build mkdir build
cd build

echo 配置CMake项目...
cmake -G "Visual Studio 17 2022" -A x64 ..
if %ERRORLEVEL% NEQ 0 (
    echo CMake配置失败
    exit /b %ERRORLEVEL%
)

echo 编译项目...
cmake --build . --config Release
if %ERRORLEVEL% NEQ 0 (
    echo 编译失败
    exit /b %ERRORLEVEL%
)

echo 编译成功！
echo 服务端: %cd%\Release\server.exe
echo 客户端: %cd%\Release\client.exe

cd ..
echo.
echo 如需运行测试:
echo 1. 先启动服务端: build\Release\server.exe
echo 2. 然后启动客户端: build\Release\client.exe
echo.
