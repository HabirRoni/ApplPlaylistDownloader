cmake_minimum_required(VERSION 3.14)
project(ApplPlaylistDownloader)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Suppress third-party warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  add_compile_options(-Wno-deprecated-declarations -Wno-format-overflow)
endif()

# Enable AddressSanitizer on this target only
# option(ENABLE_ASAN "Enable AddressSanitizer on the main executable" ON)

# 1) Fetch nlohmann/json
include(FetchContent)
FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(json)

# 2) Fetch CPR (configured for OpenSSL)
set(CPR_USE_SSL        ON CACHE BOOL "")
set(CPR_SSL_BACKEND    OpenSSL CACHE STRING "")
set(CPR_BUILD_TESTS    OFF CACHE BOOL "")
set(CPR_BUILD_TESTS_SSL OFF CACHE BOOL "")
FetchContent_Declare(
  cpr
  GIT_REPOSITORY https://github.com/libcpr/cpr.git
  GIT_TAG        1.10.5
)
FetchContent_MakeAvailable(cpr)

# 3) Find and configure Python (for embedding)
find_package(Python3 COMPONENTS Interpreter Development REQUIRED)
# Python.h and the libpython library will now be available

# 4) Define your executable (only once!)
add_executable(ApplPlaylistDownloader src/main.cpp)

# 5) Include directories (json, cpr, python)
target_include_directories(ApplPlaylistDownloader PRIVATE
  ${Python3_INCLUDE_DIRS}
  ${json_SOURCE_DIR}/include
  ${cpr_SOURCE_DIR}/include
)

# 6) Link against Python, JSON, and CPR
target_link_libraries(ApplPlaylistDownloader PRIVATE
  Python3::Python
  nlohmann_json::nlohmann_json
  cpr::cpr
)

# # 7) Turn on ASan if desired
# if(ENABLE_ASAN AND (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU"))
#   message(STATUS "Enabling AddressSanitizer on ApplPlaylistDownloader")
#   target_compile_options(ApplPlaylistDownloader PRIVATE
#     -fsanitize=address -fno-omit-frame-pointer -g
#   )
#   target_link_options(ApplPlaylistDownloader PRIVATE
#     -fsanitize=address
#   )
# endif()
