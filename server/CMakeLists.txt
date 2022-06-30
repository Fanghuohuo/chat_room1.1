# CMake 最低版本号要求
cmake_minimum_required(VERSION 3.3.2)

# 项目信息
project (server)

set(CMAKE_CXX_FLAGS "-std=c++14 -lboost_system -pthread -lprotobuf -g -O2")

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../protoSerial
    ${CMAKE_CURRENT_SOURCE_DIR}/../
)

# 查找当前目录下的所有源文件
# 并将名称保存到 DIR_SRCS 变量
aux_source_directory(../protoSerial DIR_SRCS)
add_library(protoSerial ${DIR_SRCS})

# 添加链接库目录(要在add_executable之前)
link_directories(
    /usr/local/lib
)

# 指定生成目标
add_executable(server chat_server.cpp)

# 添加链接库（要在add_executable之后）
target_link_libraries(server
    protoSerial
)
