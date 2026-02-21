cmake_minimum_required(VERSION 3.15)
project(pgnx LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# --- Find Node.js addon API includes ---
execute_process(
    COMMAND node -p "require('node-addon-api').include.replace(/\"/g, '')"
    OUTPUT_VARIABLE NAPI_INCLUDE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE NAPI_RESULT
)
if(NOT NAPI_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to find node-addon-api. Run 'npm install' first.")
endif()

execute_process(
    COMMAND node -e "require('path');console.log(require('child_process').execSync('node -e \"console.log(require.resolve(\\\"node-api-headers/include/node_api.h\\\").replace(/node_api\\.h$/,\\\"\\\"))\"',{encoding:'utf8'}).trim())"
    OUTPUT_VARIABLE NODE_API_HEADERS_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

# --- Find PostgreSQL and libpqxx ---
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBPQXX IMPORTED_TARGET libpqxx)
    pkg_check_modules(LIBPQ IMPORTED_TARGET libpq)
endif()

# Allow manual path specification for Windows or custom installations
set(PGNX_INCLUDE_DIRS "" CACHE STRING "Additional include directories (semicolon-separated)")
set(PGNX_LIB_DIRS "" CACHE STRING "Additional library directories (semicolon-separated)")

# --- Define the native addon target ---
add_library(pgnx SHARED
    src/addon.cpp
    src/connection.cpp
    src/connection_pool.cpp
    src/listener.cpp
)

target_include_directories(pgnx PRIVATE
    ${NAPI_INCLUDE_DIR}
    ${NODE_API_HEADERS_DIR}
    ${PGNX_INCLUDE_DIRS}
)

target_compile_definitions(pgnx PRIVATE NAPI_VERSION=8)

# --- Link libraries ---
if(LIBPQXX_FOUND)
    target_link_libraries(pgnx PRIVATE PkgConfig::LIBPQXX)
elseif(LIBPQ_FOUND)
    target_link_libraries(pgnx PRIVATE pqxx PkgConfig::LIBPQ)
else()
    target_link_libraries(pgnx PRIVATE pqxx pq)
endif()

target_link_libraries(pgnx PRIVATE ssl crypto z)

# --- Platform-specific settings ---
if(WIN32)
    target_link_libraries(pgnx PRIVATE ws2_32 secur32 crypt32 wldap32)
    target_compile_definitions(pgnx PRIVATE WIN32 _WIN32)
    if(PGNX_LIB_DIRS)
        target_link_directories(pgnx PRIVATE ${PGNX_LIB_DIRS})
    endif()
elseif(UNIX AND NOT APPLE)
    target_link_libraries(pgnx PRIVATE pthread)
elseif(APPLE)
    target_link_libraries(pgnx PRIVATE
        "-framework CoreFoundation"
        "-framework Security"
    )
endif()

# --- Output as .node file ---
set_target_properties(pgnx PROPERTIES
    PREFIX ""
    SUFFIX ".node"
    OUTPUT_NAME "pgnx"
)

# --- Install to build/Release for node-gyp-build compatibility ---
install(TARGETS pgnx DESTINATION "${CMAKE_SOURCE_DIR}/build/Release")
