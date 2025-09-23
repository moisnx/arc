cmake_minimum_required(VERSION 3.16)
project(arc VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows-specific optimizations for faster builds
if(WIN32)
    # MAJOR: Disable slow vcpkg applocal script (saves 3-5 seconds)
    set(VCPKG_APPLOCAL_DEPS OFF)
    
    # Reduce CMake overhead
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS OFF)
    set(CMAKE_COLOR_MAKEFILE OFF)
    
    # Static linking to avoid DLL dependencies
    if(MINGW)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++ -static")
    endif()
    
    # Force static linking for MSVC too
    if(MSVC)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
endif()

# Find packages - try to prefer static libraries
if(WIN32)
    # Try to find static version first
    set(CMAKE_FIND_LIBRARY_SUFFIXES .lib .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
    
    find_package(unofficial-pdcurses CONFIG REQUIRED)
    
    # Force static linking by using the static target if available
    if(TARGET unofficial::pdcurses::pdcurses_static)
        set(CURSES_LIBRARIES unofficial::pdcurses::pdcurses_static)
    else()
        set(CURSES_LIBRARIES unofficial::pdcurses::pdcurses)
    endif()
else()
    find_package(Curses REQUIRED)
    set(CURSES_LIBRARIES ${CURSES_LIBRARIES})
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/core/editor.cpp
    src/core/gap_buffer.cpp
    src/ui/colors.cpp
    src/features/syntax_highlighter.cpp
)

# Create executable
add_executable(arc ${SOURCES})

# Link libraries
target_link_libraries(arc PRIVATE ${CURSES_LIBRARIES})

# Include directories
target_include_directories(arc PRIVATE .)

# Compiler flags with optimizations
if(MSVC)
    target_compile_options(arc PRIVATE /W4 /MP)  # Parallel compilation
    # Static runtime
    target_compile_options(arc PRIVATE $<$<CONFIG:Debug>:/MTd> $<$<CONFIG:Release>:/MT>)
else()
    target_compile_options(arc PRIVATE -Wall -Wextra)
    
    # Reduce debug info in debug builds for faster compilation
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(arc PRIVATE -g1 -O0 -fno-omit-frame-pointer)
    else()
        target_compile_options(arc PRIVATE -O2)
    endif()
endif()

# Print build info
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Curses library: ${CURSES_LIBRARIES}")
if(WIN32)
    message(STATUS "VCPKG_APPLOCAL_DEPS: ${VCPKG_APPLOCAL_DEPS}")
endif()