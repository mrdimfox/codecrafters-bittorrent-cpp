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
    "src/client/client.hpp"
    "src/client/client.cpp"
    "src/client/piece.cpp"
    "src/client/piece.cpp"
    "src/bencode/decoders.cpp"
    "src/bencode/decoders.hpp"
    "src/bencode/encoders.hpp"
    "src/bencode/encoders.cpp"
    "src/bencode/types.hpp"
    "src/bencode/tools.hpp"
    "src/bencode/consts.hpp"
    "src/peers/peers.hpp"
    "src/peers/deserialize.cpp"
    "src/peers/deserialize.hpp"
    "src/peers/serialize.cpp"
    "src/peers/serialize.hpp"
    "src/peers/types.hpp"
    "src/peers/utils.hpp"
    "src/misc/curl.cpp"
    "src/misc/curl.hpp"
)

# Libs
add_subdirectory(src/lib/fmt-10.1.1)

add_subdirectory(src/lib/range-v3)

add_subdirectory(src/lib/magic_enum)

set(SPDLOG_FMT_EXTERNAL ON)
add_subdirectory(src/lib/spdlog-1.12.0)

set(EXPECTED_BUILD_TESTS OFF)
add_subdirectory(src/lib/expected)

add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE src/lib/nlohmann/include)

find_package(CURL REQUIRED)

add_library(asio INTERFACE)
target_include_directories(asio INTERFACE src/lib/asio-1.28.0/include)
target_compile_definitions(asio INTERFACE ASIO_STANDALONE ASIO_NO_DEPRECATED)
target_link_options(asio INTERFACE -pthread)

# Target
target_compile_features(bittorrent PRIVATE cxx_std_20)
target_link_libraries(
    bittorrent
    PRIVATE nlohmann_json
            fmt::fmt
            CURL::libcurl
            range-v3
            magic_enum::magic_enum
            asio
            tl::expected
            spdlog::spdlog
)
target_include_directories(bittorrent PRIVATE src)

if(ENABLE_TESTS)
    message(STATUS "Tests are enabled")
    target_compile_definitions(bittorrent PRIVATE ENABLE_TESTS)
else()
    message(STATUS "Tests are disabled")
endif()
