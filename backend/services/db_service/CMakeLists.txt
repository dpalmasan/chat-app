cmake_minimum_required(VERSION 3.16)
project(db_service LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(db_service
    src/main.cpp
    src/WorkerServer.cpp
    src/InMemoryMessageStore.cpp
    ../../libs/logger/src/Logger.cpp
)

target_include_directories(db_service PRIVATE src ../../libs/logger/include)
