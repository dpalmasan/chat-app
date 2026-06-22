cmake_minimum_required(VERSION 3.16)
project(db_service LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PkgConfig REQUIRED)
pkg_check_modules(MONGOC REQUIRED libmongoc-1.0)
pkg_check_modules(BSON REQUIRED libbson-1.0)

add_executable(db_service
    src/main.cpp
    src/WorkerServer.cpp
    src/InMemoryMessageStore.cpp
    src/MongoMessageStore.cpp
    ../../libs/logger/src/Logger.cpp
)

target_include_directories(
    db_service
    PRIVATE
    src
    ../../libs/logger/include
    ${MONGOC_INCLUDE_DIRS}
    ${BSON_INCLUDE_DIRS}
)

target_compile_options(db_service PRIVATE ${MONGOC_CFLAGS_OTHER} ${BSON_CFLAGS_OTHER})
target_link_libraries(db_service PRIVATE ${MONGOC_LIBRARIES} ${BSON_LIBRARIES})
