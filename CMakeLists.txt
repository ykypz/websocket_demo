cmake_minimum_required(VERSION 3.10)
project(websocket_demo)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找必要的依赖包
find_package(Threads REQUIRED)

# 设置外部库的路径（您可能需要调整这些路径）
set(WEBSOCKETPP_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/extern/websocketpp")
set(JSON_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/extern/json/include")

# 添加包含目录
include_directories(
    ${WEBSOCKETPP_INCLUDE_DIR}
    ${JSON_INCLUDE_DIR}
)

# 服务端可执行文件
add_executable(server
    server/server.cpp
)

# 客户端可执行文件
add_executable(client
    client/client.cpp
)

# 链接线程库
target_link_libraries(server Threads::Threads)
target_link_libraries(client Threads::Threads)

# 在Windows上，可能需要链接ws2_32库用于网络功能
if(WIN32)
    target_link_libraries(server ws2_32)
    target_link_libraries(client ws2_32)
endif()

# 添加测试目录
add_subdirectory(test)
