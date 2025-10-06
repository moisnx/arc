cmake_minimum_required(VERSION 3.16)
project(arc VERSION 0.0.1 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define a central location for external dependencies
set(DEPS_DIR ${CMAKE_SOURCE_DIR}/deps)

# Initialize Tree-sitter as enabled by default
set(TREE_SITTER_ENABLED TRUE)

# ----------------------------------------------------
# 1. Tree-sitter Core Library
# ----------------------------------------------------

# Manually list core source files
set(TS_CORE_SOURCES
    ${DEPS_DIR}/tree-sitter-core/lib/src/language.c
    ${DEPS_DIR}/tree-sitter-core/lib/src/lexer.c
    ${DEPS_DIR}/tree-sitter-core/lib/src/node.c
    ${DEPS_DIR}/tree-sitter-core/lib/src/parser.c
    ${DEPS_DIR}/tree-sitter-core/lib/src/query.c
    ${DEPS_DIR}/tree-sitter-core/lib/src/tree.c
    ${DEPS_DIR}/tree-sitter-core/lib/src/tree_cursor.c
    ${DEPS_DIR}/tree-sitter-core/lib/src/alloc.c
    ${DEPS_DIR}/tree-sitter-core/lib/src/get_changed_ranges.c
    ${DEPS_DIR}/tree-sitter-core/lib/src/stack.c
    ${DEPS_DIR}/tree-sitter-core/lib/src/subtree.c
    ${DEPS_DIR}/tree-sitter-core/lib/src/point.c
    ${DEPS_DIR}/tree-sitter-core/lib/src/wasm_store.c
)

# Check if Tree-sitter headers exist
if(NOT EXISTS ${DEPS_DIR}/tree-sitter-core/lib/include/tree_sitter/api.h)
    message(WARNING "Tree-sitter header files not found at ${DEPS_DIR}/tree-sitter-core/lib/include/tree_sitter/api.h")
    set(TREE_SITTER_ENABLED FALSE)
endif()

if(TREE_SITTER_ENABLED)
    # Verify core files exist
    set(MISSING_FILES "")
    foreach(file ${TS_CORE_SOURCES})
        if(NOT EXISTS ${file})
            list(APPEND MISSING_FILES ${file})
        endif()
    endforeach()
    
    if(MISSING_FILES)
        message(WARNING "Missing Tree-sitter core files: ${MISSING_FILES}")
        set(TREE_SITTER_ENABLED FALSE)
    else()
        message(STATUS "Tree-sitter core sources detected: ${TS_CORE_SOURCES}")
        message(STATUS "Building Tree-sitter core library.")
        
        add_library(tree-sitter-core STATIC ${TS_CORE_SOURCES})

        # Explicitly set these as C sources
        set_source_files_properties(${TS_CORE_SOURCES} PROPERTIES LANGUAGE C)

        # The public API headers are in lib/include/
        target_include_directories(tree-sitter-core PUBLIC
            ${DEPS_DIR}/tree-sitter-core/lib/include
        )
        # Explicitly set C standard for C files
        target_compile_features(tree-sitter-core PUBLIC c_std_99)
        
        # Force static runtime for Tree-sitter to match main executable
        if(MSVC)
            target_compile_options(tree-sitter-core PRIVATE 
                $<$<CONFIG:Debug>:/MTd>
                $<$<CONFIG:Release>:/MT>
            )
        endif()
        
        set(TS_LIBRARIES tree-sitter-core)
        set(TS_INCLUDES ${DEPS_DIR}/tree-sitter-core/lib/include)
        
        message(STATUS "Tree-sitter enabled with include path: ${TS_INCLUDES}")
    endif()
else()
    message(WARNING "Tree-sitter core sources or headers not found. Disabling Tree-sitter features.")
    set(TREE_SITTER_ENABLED FALSE)
    set(TS_LIBRARIES "")
    set(TS_INCLUDES "")
endif()

# ----------------------------------------------------
# 2. Language Parsers (Auto-Discovery) - FIXED FOR WINDOWS
# ----------------------------------------------------

if(TREE_SITTER_ENABLED)
    message(STATUS "=== Tree-sitter Auto-Discovery ===")
    
    # Define parsers - use CMake lists instead of colon-separated strings
    # Format: list of pairs (lang_name, parser_path)
    set(PARSER_NAMES 
        "python" "c" "cpp" "rust" "markdown" "javascript" "typescript" "tsx" "zig" "go"
    )
    set(PARSER_PATHS
        "${DEPS_DIR}/tree-sitter-python"
        "${DEPS_DIR}/tree-sitter-c"
        "${DEPS_DIR}/tree-sitter-cpp"
        "${DEPS_DIR}/tree-sitter-rust"
        "${DEPS_DIR}/tree-sitter-markdown/tree-sitter-markdown"
        "${DEPS_DIR}/tree-sitter-javascript"
        "${DEPS_DIR}/tree-sitter-typescript/typescript"
        "${DEPS_DIR}/tree-sitter-typescript/tsx"
        "${DEPS_DIR}/tree-sitter-zig"
        "${DEPS_DIR}/tree-sitter-go"
    )
    
    set(DISCOVERED_PARSERS "")
    
    # Iterate using indices
    list(LENGTH PARSER_NAMES parser_count)
    math(EXPR parser_count "${parser_count} - 1")
    
    foreach(i RANGE ${parser_count})
        list(GET PARSER_NAMES ${i} lang_name)
        list(GET PARSER_PATHS ${i} parser_dir)
        
        if(NOT EXISTS ${parser_dir})
            message(STATUS "  ✗ Skipping ${lang_name}: directory not found at ${parser_dir}")
            continue()
        endif()
        
        # Collect source files
        set(PARSER_SOURCES "")
        
        # Check for parser.c (required)
        if(EXISTS ${parser_dir}/src/parser.c)
            list(APPEND PARSER_SOURCES ${parser_dir}/src/parser.c)
        else()
            message(STATUS "  ✗ Skipping ${lang_name}: no parser.c at ${parser_dir}/src/")
            continue()
        endif()
        
        # Check for scanner files (optional)
        if(EXISTS ${parser_dir}/src/scanner.c)
            list(APPEND PARSER_SOURCES ${parser_dir}/src/scanner.c)
        endif()
        
        if(EXISTS ${parser_dir}/src/scanner.cc)
            list(APPEND PARSER_SOURCES ${parser_dir}/src/scanner.cc)
        endif()
        
        # Create library
        add_library(tree-sitter-${lang_name} STATIC ${PARSER_SOURCES})
        
        # Set as C sources
        set_source_files_properties(${PARSER_SOURCES} PROPERTIES LANGUAGE C)
        
        # Set C99 standard
        target_compile_features(tree-sitter-${lang_name} PUBLIC c_std_99)
        
        # Force static runtime for parsers to match main executable
        if(MSVC)
            target_compile_options(tree-sitter-${lang_name} PRIVATE 
                $<$<CONFIG:Debug>:/MTd>
                $<$<CONFIG:Release>:/MT>
            )
        endif()
        
        # Include directories for scanner files
        target_include_directories(tree-sitter-${lang_name} PRIVATE 
            ${parser_dir}/src
        )
        
        # Add to libraries list
        list(APPEND TS_LIBRARIES tree-sitter-${lang_name})
        list(APPEND DISCOVERED_PARSERS ${lang_name})
        
        message(STATUS "  ✓ Built parser: ${lang_name}")
    endforeach()
    
    # ----------------------------------------------------
    # 3. Generate Language Registry Header (Auto-registration)
    # ----------------------------------------------------
    
    if(DISCOVERED_PARSERS)
        set(LANG_REGISTRY_FILE "${CMAKE_BINARY_DIR}/generated/language_registry.h")
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")
        
        # Build the header file content
        set(REGISTRY_CONTENT "// Auto-generated by CMake - DO NOT EDIT MANUALLY\n")
        string(APPEND REGISTRY_CONTENT "// Generated from: ${CMAKE_CURRENT_LIST_FILE}\n\n")
        string(APPEND REGISTRY_CONTENT "#pragma once\n\n")
        string(APPEND REGISTRY_CONTENT "#ifdef TREE_SITTER_ENABLED\n\n")
        string(APPEND REGISTRY_CONTENT "#include <tree_sitter/api.h>\n")
        string(APPEND REGISTRY_CONTENT "#include <unordered_map>\n")
        string(APPEND REGISTRY_CONTENT "#include <string>\n\n")
        
        # Extern declarations for all discovered languages
        string(APPEND REGISTRY_CONTENT "// External language function declarations\n")
        string(APPEND REGISTRY_CONTENT "extern \"C\" {\n")
        foreach(lang ${DISCOVERED_PARSERS})
            string(APPEND REGISTRY_CONTENT "  const TSLanguage *tree_sitter_${lang}();\n")
        endforeach()
        string(APPEND REGISTRY_CONTENT "}\n\n")
        
        # Registration function
        string(APPEND REGISTRY_CONTENT "// Auto-register all available languages\n")
        string(APPEND REGISTRY_CONTENT "inline void registerAllLanguages(std::unordered_map<std::string, const TSLanguage* (*)()>& registry) {\n")
        foreach(lang ${DISCOVERED_PARSERS})
            string(APPEND REGISTRY_CONTENT "  registry[\"${lang}\"] = tree_sitter_${lang};\n")
        endforeach()
        string(APPEND REGISTRY_CONTENT "}\n\n")
        
        # List of available languages as a comment
        string(APPEND REGISTRY_CONTENT "// Available languages: ")
        string(JOIN DISCOVERED_PARSERS ", " LANG_LIST)
        string(APPEND REGISTRY_CONTENT "${LANG_LIST}\n\n")
        
        string(APPEND REGISTRY_CONTENT "#endif // TREE_SITTER_ENABLED\n")
        
        # Write the file
        file(WRITE ${LANG_REGISTRY_FILE} "${REGISTRY_CONTENT}")
        
        message(STATUS "Generated language registry: ${LANG_REGISTRY_FILE}")
        message(STATUS "  Registered parsers: ${DISCOVERED_PARSERS}")
    else()
        message(WARNING "No parsers discovered, skipping registry generation")
        set(TREE_SITTER_ENABLED FALSE)
    endif()
    
    message(STATUS "=== End Tree-sitter Auto-Discovery ===")
    
endif()

if(NOT TREE_SITTER_ENABLED)
    message(STATUS "Tree-sitter disabled - using fallback syntax highlighting")
    set(TS_LIBRARIES "")
    set(TS_INCLUDES "")
endif()

# ----------------------------------------------------
# 4. EFSW (Event File System Watcher) - For Live Reloading
# ----------------------------------------------------
set(EFSW_BASE_DIR ${DEPS_DIR}/efsw)
set(EFSW_SOURCES "")

# List ALL required core files from the flattened structure (src/efsw/)
list(APPEND EFSW_SOURCES 
    # Core Files (Unconditional)
    ${EFSW_BASE_DIR}/src/efsw/Debug.cpp
    ${EFSW_BASE_DIR}/src/efsw/DirWatcherGeneric.cpp
    ${EFSW_BASE_DIR}/src/efsw/DirectorySnapshot.cpp
    ${EFSW_BASE_DIR}/src/efsw/DirectorySnapshotDiff.cpp
    ${EFSW_BASE_DIR}/src/efsw/FileInfo.cpp
    ${EFSW_BASE_DIR}/src/efsw/FileSystem.cpp
    ${EFSW_BASE_DIR}/src/efsw/FileWatcher.cpp
    ${EFSW_BASE_DIR}/src/efsw/FileWatcherCWrapper.cpp
    ${EFSW_BASE_DIR}/src/efsw/FileWatcherImpl.cpp
    ${EFSW_BASE_DIR}/src/efsw/Log.cpp
    ${EFSW_BASE_DIR}/src/efsw/String.cpp
    ${EFSW_BASE_DIR}/src/efsw/System.cpp
    ${EFSW_BASE_DIR}/src/efsw/Watcher.cpp
    
    # CRITICAL FIX: Add Generic implementation for default constructors
    ${EFSW_BASE_DIR}/src/efsw/FileWatcherGeneric.cpp
    ${EFSW_BASE_DIR}/src/efsw/WatcherGeneric.cpp
)

# Conditionally add the correct platform backend
if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    list(APPEND EFSW_SOURCES 
        ${EFSW_BASE_DIR}/src/efsw/FileWatcherInotify.cpp
        ${EFSW_BASE_DIR}/src/efsw/WatcherInotify.cpp
        ${EFSW_BASE_DIR}/src/efsw/platform/posix/FileSystemImpl.cpp
        ${EFSW_BASE_DIR}/src/efsw/platform/posix/SystemImpl.cpp
    )
elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin") # macOS
    list(APPEND EFSW_SOURCES 
        ${EFSW_BASE_DIR}/src/efsw/FileWatcherFSEvents.cpp
        ${EFSW_BASE_DIR}/src/efsw/WatcherFSEvents.cpp
        ${EFSW_BASE_DIR}/src/efsw/platform/posix/FileSystemImpl.cpp
        ${EFSW_BASE_DIR}/src/efsw/platform/posix/SystemImpl.cpp
    )
elseif(WIN32)
    list(APPEND EFSW_SOURCES 
        ${EFSW_BASE_DIR}/src/efsw/FileWatcherWin32.cpp
        ${EFSW_BASE_DIR}/src/efsw/WatcherWin32.cpp
        ${EFSW_BASE_DIR}/src/efsw/platform/win/FileSystemImpl.cpp
        ${EFSW_BASE_DIR}/src/efsw/platform/win/SystemImpl.cpp
    )
else()
    list(APPEND EFSW_SOURCES 
        ${EFSW_BASE_DIR}/src/efsw/FileWatcherGeneric.cpp
        ${EFSW_BASE_DIR}/src/efsw/WatcherGeneric.cpp
    )
endif()

if(EFSW_SOURCES)
    add_library(efsw STATIC ${EFSW_SOURCES})
    
    target_include_directories(efsw PUBLIC
        ${EFSW_BASE_DIR}/include
        ${EFSW_BASE_DIR}/src
    )
    
    # Force static runtime for EFSW to match main executable
    if(MSVC)
        target_compile_options(efsw PRIVATE 
            $<$<CONFIG:Debug>:/MTd>
            $<$<CONFIG:Release>:/MT>
        )
    endif()
    
    find_package(Threads REQUIRED)
    target_link_libraries(efsw PRIVATE Threads::Threads)
    
    if(CMAKE_SYSTEM_NAME MATCHES "Linux")
        target_link_libraries(efsw PRIVATE rt)
    elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
        target_link_libraries(efsw PRIVATE CoreServices)
    endif()
    
    message(STATUS "Added EFSW from deps folder.")
    set(EFSW_LIBRARIES efsw)
else()
    message(WARNING "EFSW sources missing. Live reload feature disabled.")
    set(EFSW_LIBRARIES "")
endif()

# ----------------------------------------------------
# 5. Platform-Specific Settings
# ----------------------------------------------------

# Windows-specific optimizations
if(WIN32)
    set(VCPKG_APPLOCAL_DEPS OFF)
    set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS OFF)
    set(CMAKE_COLOR_MAKEFILE OFF)
    
    if(MINGW)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++ -static")
    endif()
    
    if(MSVC)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
endif()

# Find packages - platform-specific approach
if(WIN32)
   set(PDCURSESMOD_DIR ${DEPS_DIR}/PDCursesMod/wincon)

if(EXISTS "${PDCURSESMOD_DIR}/pdcurses.lib")
    message(STATUS "Using locally built PDCursesMod from ${PDCURSESMOD_DIR}")

    add_library(pdcursesmod STATIC IMPORTED)
    set_target_properties(pdcursesmod PROPERTIES
        IMPORTED_LOCATION "${PDCURSESMOD_DIR}/pdcurses.lib"
        INTERFACE_INCLUDE_DIRECTORIES "${DEPS_DIR}/PDCursesMod"
    )

    set(CURSES_LIBRARIES pdcursesmod)
    set(CURSES_INCLUDE_DIRS "${DEPS_DIR}/PDCursesMod")
else()
    message(FATAL_ERROR "PDCursesMod not found at ${PDCURSESMOD_DIR}. Please build it manually with nmake -f Makefile.vc HAVE_VT=Y")
endif()
    
elseif(ANDROID)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(NCURSES REQUIRED ncurses)
    set(CURSES_LIBRARIES ${NCURSES_LIBRARIES})
    set(CURSES_INCLUDE_DIRS ${NCURSES_INCLUDE_DIRS})
    
else()
    find_package(Curses REQUIRED)
    set(CURSES_LIBRARIES ${CURSES_LIBRARIES})
    set(CURSES_INCLUDE_DIRS ${CURSES_INCLUDE_DIR})
endif()

find_package(yaml-cpp CONFIG REQUIRED)

# ----------------------------------------------------
# 6. Main Executable
# ----------------------------------------------------

# Source files
set(SOURCES
    src/main.cpp
    src/core/editor.cpp
    src/core/buffer.cpp
    src/core/config_manager.cpp
    src/ui/input_handler.cpp
    src/ui/renderer.cpp
    src/ui/style_manager.cpp
    src/features/syntax_config_loader.cpp
    src/features/syntax_highlighter.cpp
)

# Create executable
add_executable(arc ${SOURCES})

# Conditionally add Tree-sitter compile definition
if(TREE_SITTER_ENABLED)
    target_compile_definitions(arc PRIVATE TREE_SITTER_ENABLED)
    message(STATUS "Compiling with Tree-sitter support enabled")
else()
    message(STATUS "Compiling without Tree-sitter support")
endif()

# Link libraries
target_link_libraries(arc PRIVATE
    yaml-cpp::yaml-cpp
    ${CURSES_LIBRARIES}
    ${TS_LIBRARIES}
    ${EFSW_LIBRARIES}
)

if(WIN32)
    # Link the Windows Multimedia library required by PDCursesMod's beep()
    target_link_libraries(arc PRIVATE winmm) 
endif()

# Include directories
target_include_directories(arc PRIVATE 
    . 
    ${CURSES_INCLUDE_DIRS}
    ${TS_INCLUDES}
)

# Add generated headers directory if Tree-sitter is enabled
if(TREE_SITTER_ENABLED)
    target_include_directories(arc PRIVATE ${CMAKE_BINARY_DIR}/generated)
endif()

# Compiler flags with optimizations
if(MSVC)
    target_compile_options(arc PRIVATE /W4 /MP)
    target_compile_options(arc PRIVATE $<$<CONFIG:Debug>:/MTd> $<$<CONFIG:Release>:/MT>)
else()
    target_compile_options(arc PRIVATE -Wall -Wextra)
    
    if(ANDROID)
        target_link_libraries(arc PRIVATE ${NCURSES_LINK_LIBRARIES})
    endif()
    
    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(arc PRIVATE -g1 -O0 -fno-omit-frame-pointer)
    else()
        target_compile_options(arc PRIVATE -O2)
    endif()
endif()

# ----------------------------------------------------
# 7. Build Summary
# ----------------------------------------------------

message(STATUS "")
message(STATUS "========================================")
message(STATUS "Arc Editor Build Configuration")
message(STATUS "========================================")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Curses library: ${CURSES_LIBRARIES}")
message(STATUS "Tree-sitter enabled: ${TREE_SITTER_ENABLED}")
if(TREE_SITTER_ENABLED)
    message(STATUS "  Tree-sitter libraries: ${TS_LIBRARIES}")
    message(STATUS "  Tree-sitter includes: ${TS_INCLUDES}")
    message(STATUS "  Discovered parsers: ${DISCOVERED_PARSERS}")
endif()
message(STATUS "EFSW enabled: ${EFSW_LIBRARIES}")
if(WIN32)
    message(STATUS "VCPKG_APPLOCAL_DEPS: ${VCPKG_APPLOCAL_DEPS}")
endif()
message(STATUS "========================================")
message(STATUS "")