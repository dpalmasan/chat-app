cmake_minimum_required(VERSION 3.16)
project(chat_service LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(chat_service
    src/main.cpp
    src/ChatService.cpp
)

target_include_directories(chat_service PRIVATE src)
