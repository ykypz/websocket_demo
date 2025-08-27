cmake_minimum_required(VERSION 3.10)
project(websocket_demo)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加UTF-8支持
if(MSVC)
  add_compile_options(/utf-8)
endif()

# 查找必要的依赖包
find_package(Threads REQUIRED)

# 添加包含目录
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/include/nlohmann)
include_directories(${CMAKE_SOURCE_DIR}/include/websocketpp)
include_directories(${CMAKE_SOURCE_DIR}/include/asio-asio-1-24-0/asio/include)

# 服务端可执行文件
add_executable(server
    src/server.cpp
)

# 客户端可执行文件
add_executable(client
    src/client.cpp
)

# 链接线程库
target_link_libraries(server Threads::Threads)
target_link_libraries(client Threads::Threads)

# 在Windows上，可能需要链接ws2_32库用于网络功能
if(WIN32)
    target_link_libraries(server ws2_32)
    target_link_libraries(client ws2_32)
endif()
