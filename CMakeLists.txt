cmake_minimum_required(VERSION 3.13)

# Enable compile commands for IntelliSense support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)

project(bittorrent-starter-cpp)

# Executable
add_executable(
    bittorrent
    src/main.cpp
    src/tests.cpp
    "src/torrent.cpp"
    "src/torrent.hpp"
    "src/bencode/decoders.cpp"
    "src/bencode/decoders.hpp"
    "src/bencode/encoders.hpp"
    "src/bencode/encoders.cpp"
    "src/bencode/types.hpp"
    "src/bencode/tools.hpp"
    "src/bencode/consts.hpp"
)

# Libs
add_subdirectory(src/lib/fmt-10.1.1)

add_subdirectory(src/lib/range-v3)

add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE src/lib/nlohmann/include)

find_package(CURL REQUIRED)

# Target
target_compile_features(bittorrent PRIVATE cxx_std_20)
target_link_libraries(
    bittorrent
    PRIVATE nlohmann_json
            fmt::fmt
            CURL::libcurl
            range-v3
)
target_include_directories(bittorrent PRIVATE src)

if(ENABLE_TESTS)
    message(STATUS "Tests are enabled")
    target_compile_definitions(bittorrent PRIVATE ENABLE_TESTS)
else()
    message(STATUS "Tests are disabled")
endif()
