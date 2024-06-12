cmake_minimum_required(VERSION 3.28)
project(SIXW_NEXUS CXX)

# set(CMAKE_EXPERIMENTAL_CXX_MODULE_DYNDEP 1)
set(CMAKE_CXX_COMPILER /usr/bin/clang++)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Python3 COMPONENTS Interpreter REQUIRED)

find_path(BSONCXX_INCLUDE_DIR bsoncxx/json.hpp PATHS /usr/local/include/bsoncxx/v_noabi)
find_path(MONGOCXX_INCLUDE_DIR mongocxx/client.hpp PATHS /usr/local/include/mongocxx/v_noabi)

find_library(LIBMONGOCXX mongocxx PATHS /usr/local/lib64)
find_library(LIBBSONCXX bsoncxx PATHS /usr/local/lib64)
link_directories(/usr/local/lib64)

# set(Seastar_SANITIZE OFF CACHE STRING "Enable sanitizers for Seastar" FORCE)
if(CMAKE_BUILD_TYPE MATCHES Debug)
    set(SEASTAR_SANITIZE "address,undefined,leak" CACHE STRING "Enable sanitizers for Seastar" FORCE)
endif()

add_subdirectory(seastar)

file(GLOB_RECURSE INCLUDE_HEADERS INCLUDE/*.h)

add_executable(NEXUS main.cc)
target_link_libraries(NEXUS PUBLIC Seastar::seastar ${LIBMONGOCXX} ${LIBBSONCXX})

target_include_directories(NEXUS PUBLIC
    INCLUDE
    ${INCLUDE_HEADERS}
    ${BSONCXX_INCLUDE_DIR}
    ${MONGOCXX_INCLUDE_DIR}
)

target_include_directories(seastar PUBLIC
    ${BSONCXX_INCLUDE_DIR}
    ${MONGOCXX_INCLUDE_DIR}
)
