cmake_minimum_required(VERSION 3.16)
project(message_queue_worker LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(message_queue_worker
    src/main.cpp
    src/WorkerServer.cpp
    src/InMemoryMessageQueue.cpp
    src/MessageEventSubscriber.cpp
    src/RemoteDbMessageStore.cpp
    ../../libs/logger/src/Logger.cpp
)

target_include_directories(message_queue_worker PRIVATE src ../../libs/logger/include)
