cmake_minimum_required(VERSION 3.5)
project(Craftmine_Server)

set (SOURCE_PATH ${CMAKE_CURRENT_LIST_DIR}/Classes)
set (CMAKE_EXE_LINKER_FLAGS /MACHINE:X64)
set (CMAKE_GENERATOR "Visual Studio 14 2015 Win64" CACHE INTERNAL "" FORCE)

set (CMAKE_MODULE_LINKER_FLAGS /MACHINE:X64)
set (CMAKE_SHARED_LINKER_FLAGS /MACHINE:X64)
set (CMAKE_STATIC_LINKER_FLAGS /MACHINE:X64)

set (SOURCES
    ${SOURCE_PATH}/main.cpp
)

if (WIN32)
    set (LIBRARIES enet pdcurses winmm ws2_32)
    set (INCLUDE_DIRS ${SOURCE_PATH} C:/Users/include)
    set (LIBRARY_DIRS C:/Users/lib)
elseif (APPLE)
    set (LIBRARIES enet ncurses)
    set (INCLUDE_DIRS ${SOURCE_PATH} /usr/local/include)
    set (LIBRARY_DIRS /usr/local/lib)
endif()

set (CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_CURRENT_LIST_DIR})
set (CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_CURRENT_LIST_DIR})

set (CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Zi")
set (CMAKE_SHARED_LINKER_FLAGS_RELEASE "${CMAKE_SHARED_LINKER_FLAGS_RELEASE} /DEBUG /OPT:REF /OPT:ICF")

include_directories(${INCLUDE_DIRS})
link_directories(${LIBRARY_DIRS})
add_executable(Craftmine_Server ${SOURCES})

set_target_properties(Craftmine_Server PROPERTIES
    LINKER_LANGUAGE CXX
)

if (APPLE)
    target_compile_options(Craftmine_Server PUBLIC -arch x86_64 -std=gnu++14 -F Build -MMD -MT dependencies -O0 -g)
endif()

target_link_libraries(Craftmine_Server ${LIBRARIES})