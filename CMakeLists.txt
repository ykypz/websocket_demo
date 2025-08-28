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

# 添加第三方库的包含目录
include_directories(${CMAKE_SOURCE_DIR}/third_party)
include_directories(${CMAKE_SOURCE_DIR}/third_party/nlohmann)
include_directories(${CMAKE_SOURCE_DIR}/third_party/websocketpp)
include_directories(${CMAKE_SOURCE_DIR}/third_party/asio/asio/include)

# 添加包含目录
include_directories(${CMAKE_SOURCE_DIR}/include/common)
include_directories(${CMAKE_SOURCE_DIR}/include/client)
include_directories(${CMAKE_SOURCE_DIR}/include/server)

# 公共源文件
set(COMMON_SOURCES
    src/common/command_types.cpp
    src/common/time_utils.cpp
)

# 客户端源文件
set(CLIENT_SOURCES
    src/client/device_client.cpp
    src/client/client_main.cpp
    ${COMMON_SOURCES}
)

# 服务端源文件
set(SERVER_SOURCES
    src/server/device_server.cpp
    src/server/server_main.cpp
    ${COMMON_SOURCES}
)

# 客户端可执行文件
add_executable(client ${CLIENT_SOURCES})

# 服务端可执行文件
add_executable(server ${SERVER_SOURCES})

# 链接线程库
target_link_libraries(client Threads::Threads)
target_link_libraries(server Threads::Threads)

# 在Windows上，可能需要链接ws2_32库用于网络功能
if(WIN32)
    target_link_libraries(client ws2_32)
    target_link_libraries(server ws2_32)
endif()
