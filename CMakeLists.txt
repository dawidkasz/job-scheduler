cmake_minimum_required(VERSION 3.22)
project(job_scheduler CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_CLANG_TIDY
    clang-tidy;
    --config-file=${CMAKE_SOURCE_DIR}/.clang-tidy
)

find_package(Boost REQUIRED COMPONENTS program_options system log log_setup)

include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.14.0
)
FetchContent_MakeAvailable(googletest)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

file(GLOB_RECURSE LIB_SOURCES src/core/*.cpp src/scheduler/*.cpp src/storage/*.cpp src/api/*.cpp)
file(GLOB_RECURSE LIB_HEADERS include/**/*.hpp)

add_library(job_scheduler_lib STATIC ${LIB_SOURCES} ${LIB_HEADERS})

target_include_directories(job_scheduler_lib PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(job_scheduler_lib PUBLIC
    Boost::program_options
    Boost::system
    Boost::log
    Boost::log_setup
    nlohmann_json::nlohmann_json
)

add_executable(job_scheduler_cli src/cli_main.cpp)
target_link_libraries(job_scheduler_cli PRIVATE job_scheduler_lib)

add_executable(job_scheduler_http src/http_main.cpp)
target_link_libraries(job_scheduler_http PRIVATE job_scheduler_lib)

enable_testing()
add_subdirectory(test)
