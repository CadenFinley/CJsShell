file(
    GLOB_RECURSE CJSH_ALL_CPP
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)

file(
    GLOB_RECURSE CJSH_ALL_HEADERS
    CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h"
)

set(CJSH_MAIN_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/src/cjsh.cpp")
set(CJSH_CORE_SOURCES ${CJSH_ALL_CPP})
list(REMOVE_ITEM CJSH_CORE_SOURCES "${CJSH_MAIN_SOURCE}")

cjsh_collect_include_dirs(ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src" OUT_VAR CJSH_INCLUDE_DIRS)

add_library(cjsh_core STATIC)
target_sources(cjsh_core PRIVATE ${CJSH_CORE_SOURCES} ${CJSH_ALL_HEADERS})
target_include_directories(cjsh_core PUBLIC ${CJSH_INCLUDE_DIRS})
target_link_libraries(cjsh_core PUBLIC cjsh_build_settings cjsh_isocline)
set_target_properties(
    cjsh_core
    PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9)
    target_link_libraries(cjsh_core PUBLIC stdc++fs)
endif()

add_executable(cjsh "${CJSH_MAIN_SOURCE}")
target_link_libraries(cjsh PRIVATE cjsh_core)
target_include_directories(cjsh PRIVATE ${CJSH_INCLUDE_DIRS})
set_target_properties(cjsh PROPERTIES OUTPUT_NAME "cjsh")

if(CJSH_STRIP_BINARY)
    cjsh_enable_binary_stripping(cjsh)
endif()

install(TARGETS cjsh RUNTIME DESTINATION bin)
