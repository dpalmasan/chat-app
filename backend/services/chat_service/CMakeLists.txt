cmake_minimum_required(VERSION 3.16)
project(chat_service LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(chat_service
    src/main.cpp
    src/ChatService.cpp
    src/RemoteMessageQueuePublisher.cpp
    src/RemoteMessageStore.cpp
    src/WebSocketServer.cpp
    ../../libs/logger/src/Logger.cpp
)

target_include_directories(chat_service PRIVATE src ../../libs/logger/include)
target_link_libraries(chat_service PRIVATE crypto)
