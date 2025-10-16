cmake_minimum_required(VERSION 3.16)
project(arc VERSION 0.0.1 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define a central location for external dependencies
set(DEPS_DIR ${CMAKE_SOURCE_DIR}/deps)

# Initialize Tree-sitter as enabled by default
set(TREE_SITTER_ENABLED TRUE)

set(DEV_DEBUG_ENABLED TRUE)

# Debug Delta
# add_compile_definitions(DEBUG_DELTA_UNDO)

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
# 2. Language Parsers (Manifest-Based Auto-Discovery)
# ----------------------------------------------------

if(TREE_SITTER_ENABLED)
    message(STATUS "=== Tree-sitter Manifest-Based Discovery ===")

    set(GRAMMAR_MANIFEST "${DEPS_DIR}/grammars/manifest.yaml")

    if(NOT EXISTS ${GRAMMAR_MANIFEST})
        message(WARNING "Grammar manifest not found at ${GRAMMAR_MANIFEST}")
        message(WARNING "Parsers will not be built. Create manifest.yaml or use legacy discovery.")
        set(TREE_SITTER_ENABLED FALSE)
    else()
        message(STATUS "Reading grammar manifest: ${GRAMMAR_MANIFEST}")

        # Read and parse the YAML manifest
        file(READ ${GRAMMAR_MANIFEST} MANIFEST_CONTENT)

        set(DISCOVERED_PARSERS "")
        set(PARSER_PATHS "")

        # Parse YAML manually (CMake doesn't have built-in YAML parser)
        # We look for lines like: "  path: tree-sitter-python"
        string(REGEX MATCHALL "[a-z_]+:\n[ ]+path: [^\n]+" PARSER_ENTRIES "${MANIFEST_CONTENT}")

        foreach(entry ${PARSER_ENTRIES})
            # Extract parser name (e.g., "python:")
            string(REGEX MATCH "^([a-z_]+):" _ ${entry})
            set(parser_name ${CMAKE_MATCH_1})

            # Extract path (e.g., "path: tree-sitter-python")
            string(REGEX MATCH "path: ([^\n]+)" _ ${entry})
            set(parser_path ${CMAKE_MATCH_1})

            # Check if enabled (look for "enabled: true" in the same block)
            # We'll do a simple check: if the block doesn't contain "enabled: false", assume true
            string(FIND "${entry}" "enabled: false" is_disabled)

            if(is_disabled EQUAL -1) # Not found, so it's enabled
                set(full_path "${DEPS_DIR}/grammars/${parser_path}")

                if(NOT EXISTS ${full_path})
                    message(STATUS "  ⚠️  Skipping ${parser_name}: directory not found at ${full_path}")
                    continue()
                endif()

                # Check for required parser.c
                if(NOT EXISTS "${full_path}/src/parser.c")
                    message(STATUS "  ⚠️  Skipping ${parser_name}: no parser.c at ${full_path}/src/")
                    continue()
                endif()

                # Collect source files
                set(PARSER_SOURCES "${full_path}/src/parser.c")

                # Auto-detect scanner files
                if(EXISTS "${full_path}/src/scanner.c")
                    list(APPEND PARSER_SOURCES "${full_path}/src/scanner.c")
                endif()

                if(EXISTS "${full_path}/src/scanner.cc")
                    list(APPEND PARSER_SOURCES "${full_path}/src/scanner.cc")
                endif()

                # Create library
                add_library(tree-sitter-${parser_name} STATIC ${PARSER_SOURCES})

                # Set as C sources
                set_source_files_properties(${PARSER_SOURCES} PROPERTIES LANGUAGE C)

                # Set C99 standard
                target_compile_features(tree-sitter-${parser_name} PUBLIC c_std_99)

                # Force static runtime for parsers
                if(MSVC)
                    target_compile_options(tree-sitter-${parser_name} PRIVATE
                        $<$<CONFIG:Debug>:/MTd>
                        $<$<CONFIG:Release>:/MT>
                    )
                endif()

                # Include directories
                target_include_directories(tree-sitter-${parser_name} PRIVATE
                    ${full_path}/src
                )

                # Add to libraries list
                list(APPEND TS_LIBRARIES tree-sitter-${parser_name})
                list(APPEND DISCOVERED_PARSERS ${parser_name})

                message(STATUS "  ✓ Built parser: ${parser_name} (from ${parser_path})")
            else()
                message(STATUS "  ⊘ Skipped ${parser_name}: disabled in manifest")
            endif()
        endforeach()

        if(NOT DISCOVERED_PARSERS)
            message(WARNING "No parsers were discovered from manifest!")
            set(TREE_SITTER_ENABLED FALSE)
        else()
            message(STATUS "")
            message(STATUS "Discovered ${CMAKE_MATCH_COUNT} parsers from manifest:")
            message(STATUS "  ${DISCOVERED_PARSERS}")
        endif()
    endif()

    # ----------------------------------------------------
    # 3. Generate Language Registry Header (Auto-registration)
    # ----------------------------------------------------

    if(DISCOVERED_PARSERS)
        set(LANG_REGISTRY_FILE "${CMAKE_BINARY_DIR}/generated/language_registry.h")
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")

        # Build the header file content
        set(REGISTRY_CONTENT "// Auto-generated by CMake from manifest.yaml\n")
        string(APPEND REGISTRY_CONTENT "// DO NOT EDIT MANUALLY\n\n")
        string(APPEND REGISTRY_CONTENT "#pragma once\n\n")
        string(APPEND REGISTRY_CONTENT "#ifdef TREE_SITTER_ENABLED\n\n")
        string(APPEND REGISTRY_CONTENT "#include <tree_sitter/api.h>\n")
        string(APPEND REGISTRY_CONTENT "#include <unordered_map>\n")
        string(APPEND REGISTRY_CONTENT "#include <string>\n\n")

        # Extern declarations
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

        # List of available languages
        string(APPEND REGISTRY_CONTENT "// Available languages: ")
        string(JOIN DISCOVERED_PARSERS ", " LANG_LIST)
        string(APPEND REGISTRY_CONTENT "${LANG_LIST}\n\n")

        string(APPEND REGISTRY_CONTENT "#endif // TREE_SITTER_ENABLED\n")

        # Write the file
        file(WRITE ${LANG_REGISTRY_FILE} "${REGISTRY_CONTENT}")

        message(STATUS "Generated language registry: ${LANG_REGISTRY_FILE}")
        message(STATUS "  Registered parsers: ${DISCOVERED_PARSERS}")
    endif()

    message(STATUS "=== End Tree-sitter Discovery ===\n")

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

# ----------------------------------------------------
# 6. Find Curses - WITH WIDE CHARACTER SUPPORT
# ----------------------------------------------------

if(WIN32)
    # Windows: Use PDCursesMod (already has UTF-8 support via VT mode)
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
        message(STATUS "PDCursesMod supports UTF-8 via VT mode")
    else()
        message(FATAL_ERROR "PDCursesMod not found at ${PDCURSESMOD_DIR}. Please build it manually with nmake -f Makefile.vc HAVE_VT=Y")
    endif()

elseif(ANDROID)
    # Android: Try to find ncursesw first, fall back to ncurses
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(NCURSESW ncursesw)

    if(NCURSESW_FOUND)
        message(STATUS "Found ncursesw for Android")
        set(CURSES_LIBRARIES ${NCURSESW_LIBRARIES})
        set(CURSES_INCLUDE_DIRS ${NCURSESW_INCLUDE_DIRS})
        set(CURSES_LINK_LIBRARIES ${NCURSESW_LINK_LIBRARIES})
    else()
        message(STATUS "ncursesw not found, falling back to ncurses")
        pkg_check_modules(NCURSES REQUIRED ncurses)
        set(CURSES_LIBRARIES ${NCURSES_LIBRARIES})
        set(CURSES_INCLUDE_DIRS ${NCURSES_INCLUDE_DIRS})
        set(CURSES_LINK_LIBRARIES ${NCURSES_LINK_LIBRARIES})
    endif()

else()
    # Linux/macOS: Prefer ncursesw over ncurses
    find_package(PkgConfig)

    if(PkgConfig_FOUND)
        # Try pkg-config first for ncursesw
        pkg_check_modules(NCURSESW ncursesw)

        if(NCURSESW_FOUND)
            message(STATUS "Found ncursesw via pkg-config")
            set(CURSES_LIBRARIES ${NCURSESW_LIBRARIES})
            set(CURSES_INCLUDE_DIRS ${NCURSESW_INCLUDE_DIRS})
            set(CURSES_LINK_FLAGS ${NCURSESW_LDFLAGS})
        endif()
    endif()

    # If pkg-config didn't work, try FindCurses with wide character preference
    if(NOT NCURSESW_FOUND)
        set(CURSES_NEED_WIDE TRUE)
        find_package(Curses REQUIRED)

        # Verify we got wide character support
        if(CURSES_HAVE_NCURSESW_NCURSES_H OR CURSES_HAVE_NCURSESW_H)
            message(STATUS "Found ncursesw via FindCurses")
        elseif(CURSES_HAVE_NCURSES_NCURSES_H OR CURSES_HAVE_NCURSES_H)
            message(WARNING "Only found ncurses (not ncursesw). Unicode icons may not render correctly.")
            message(WARNING "Install ncursesw: apt-get install libncursesw5-dev (Debian/Ubuntu)")
        else()
            message(STATUS "Found curses library: ${CURSES_LIBRARIES}")
        endif()

        set(CURSES_LIBRARIES ${CURSES_LIBRARIES})
        set(CURSES_INCLUDE_DIRS ${CURSES_INCLUDE_DIR})
    endif()
endif()

find_package(yaml-cpp CONFIG REQUIRED)

# ----------------------------------------------------
# 7. Main Executable
# ----------------------------------------------------

# Source files
set(SOURCES
    src/main.cpp
    src/core/application.cpp
    src/core/editor_loop.cpp
    src/core/args_parser.cpp
    src/core/clipboard.cpp
    src/core/editor.cpp
    src/core/buffer.cpp
    src/core/config_manager.cpp
    src/core/file_browser.cpp
    src/core/signals/signal_handler.cpp
    src/ui/input_handler.cpp
    src/ui/style_manager.cpp
    src/ui/icon_provider.cpp
    src/ui/browser_renderer.cpp
    src/features/syntax_config_loader.cpp
    src/features/syntax_highlighter.cpp
    src/benchmark/benchmark.cpp
    src/modes/browser_mode.cpp
    src/modes/editor_mode.cpp
)

# Create executable
add_executable(arc ${SOURCES})

if(TREE_SITTER_ENABLED AND DISCOVERED_PARSERS)
    message(STATUS "=== Embedding Tree-sitter Queries ===")

    set(QUERY_REGISTRY_HEADER "${CMAKE_BINARY_DIR}/generated/embedded_queries.h")
    set(QUERY_REGISTRY_SOURCE "${CMAKE_BINARY_DIR}/generated/embedded_queries.cpp")

    # Ensure generated directory exists
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")

    # Start building the header file
    file(WRITE ${QUERY_REGISTRY_HEADER} "// Auto-generated by CMake - DO NOT EDIT\n")
    file(APPEND ${QUERY_REGISTRY_HEADER} "// Generated from: ${CMAKE_CURRENT_LIST_FILE}\n\n")
    file(APPEND ${QUERY_REGISTRY_HEADER} "#pragma once\n\n")
    file(APPEND ${QUERY_REGISTRY_HEADER} "#ifdef TREE_SITTER_ENABLED\n\n")
    file(APPEND ${QUERY_REGISTRY_HEADER} "#include <string>\n")
    file(APPEND ${QUERY_REGISTRY_HEADER} "#include <unordered_map>\n\n")
    file(APPEND ${QUERY_REGISTRY_HEADER} "namespace embedded_queries {\n\n")

    # Start building the source file
    file(WRITE ${QUERY_REGISTRY_SOURCE} "// Auto-generated by CMake - DO NOT EDIT\n")
    file(APPEND ${QUERY_REGISTRY_SOURCE} "#include \"embedded_queries.h\"\n\n")
    file(APPEND ${QUERY_REGISTRY_SOURCE} "namespace embedded_queries {\n\n")

    set(ALL_EMBEDDED_QUERIES "")
    set(TOTAL_QUERY_SIZE 0)

    foreach(lang ${DISCOVERED_PARSERS})
        set(QUERY_DIR "${CMAKE_SOURCE_DIR}/runtime/queries/${lang}")

        if(EXISTS ${QUERY_DIR})
            # Find all .scm files for this language
            file(GLOB QUERY_FILES "${QUERY_DIR}/*.scm")

            if(QUERY_FILES)
                message(STATUS "  📦 Embedding queries for ${lang}:")

                foreach(query_file ${QUERY_FILES})
                    get_filename_component(query_name ${query_file} NAME_WE)

                    # Read the query file content
                    file(READ ${query_file} QUERY_CONTENT)
                    string(LENGTH "${QUERY_CONTENT}" QUERY_SIZE)
                    math(EXPR TOTAL_QUERY_SIZE "${TOTAL_QUERY_SIZE} + ${QUERY_SIZE}")

                    # Create a unique delimiter for raw string literal
                    # Use a hash to ensure uniqueness
                    string(MD5 QUERY_HASH "${QUERY_CONTENT}")
                    string(SUBSTRING ${QUERY_HASH} 0 8 DELIMITER)

                    # Generate unique identifier
                    set(VAR_NAME "${lang}_${query_name}_scm")

                    # Write declaration to header
                    file(APPEND ${QUERY_REGISTRY_HEADER}
                        "extern const char* ${VAR_NAME};\n")

                    # Write definition to source using raw string literal
                    # R"DELIMITER(content)DELIMITER" syntax avoids most escaping issues
                    file(APPEND ${QUERY_REGISTRY_SOURCE}
                        "const char* ${VAR_NAME} = R\"${DELIMITER}(${QUERY_CONTENT})${DELIMITER}\";\n\n")

                    list(APPEND ALL_EMBEDDED_QUERIES "${lang}:${query_name}")

                    message(STATUS "      - ${query_name}.scm (${QUERY_SIZE} bytes)")
                endforeach()
            endif()
        endif()
    endforeach()

    # CRITICAL: Also embed queries from special directories like ecma, _javascript, etc.
    # These are dependencies for languages like JavaScript/TypeScript
    set(SPECIAL_QUERY_DIRS "ecma" "_javascript" "_jsx" "_typescript")
    foreach(special_dir ${SPECIAL_QUERY_DIRS})
        set(QUERY_DIR "${CMAKE_SOURCE_DIR}/runtime/queries/${special_dir}")

        if(EXISTS ${QUERY_DIR})
            message(STATUS "  📦 Embedding queries for ${special_dir} (dependency):")

            file(GLOB QUERY_FILES "${QUERY_DIR}/*.scm")

            foreach(query_file ${QUERY_FILES})
                get_filename_component(query_name ${query_file} NAME_WE)

                file(READ ${query_file} QUERY_CONTENT)
                string(LENGTH "${QUERY_CONTENT}" QUERY_SIZE)
                math(EXPR TOTAL_QUERY_SIZE "${TOTAL_QUERY_SIZE} + ${QUERY_SIZE}")

                string(MD5 QUERY_HASH "${QUERY_CONTENT}")
                string(SUBSTRING ${QUERY_HASH} 0 8 DELIMITER)

                set(VAR_NAME "${special_dir}_${query_name}_scm")

                file(APPEND ${QUERY_REGISTRY_HEADER}
                    "extern const char* ${VAR_NAME};\n")

                file(APPEND ${QUERY_REGISTRY_SOURCE}
                    "const char* ${VAR_NAME} = R\"${DELIMITER}(${QUERY_CONTENT})${DELIMITER}\";\n\n")

                list(APPEND ALL_EMBEDDED_QUERIES "${special_dir}:${query_name}")

                message(STATUS "      - ${query_name}.scm (${QUERY_SIZE} bytes)")
            endforeach()
        endif()
    endforeach()

    # Generate lookup functions
    file(APPEND ${QUERY_REGISTRY_HEADER} "\n// Query lookup functions\n")
    file(APPEND ${QUERY_REGISTRY_HEADER}
        "const char* getEmbeddedQuery(const std::string& lang, const std::string& query_type);\n")
    file(APPEND ${QUERY_REGISTRY_HEADER}
        "bool hasEmbeddedQuery(const std::string& lang, const std::string& query_type);\n")
    file(APPEND ${QUERY_REGISTRY_HEADER} "\n} // namespace embedded_queries\n\n")
    file(APPEND ${QUERY_REGISTRY_HEADER} "#endif // TREE_SITTER_ENABLED\n")

    # Generate lookup function implementation
    file(APPEND ${QUERY_REGISTRY_SOURCE}
        "// Query lookup function\n")
    file(APPEND ${QUERY_REGISTRY_SOURCE}
        "const char* getEmbeddedQuery(const std::string& lang, const std::string& query_type) {\n")
    file(APPEND ${QUERY_REGISTRY_SOURCE}
        "    static const std::unordered_map<std::string, const char*> registry = {\n")

    foreach(entry ${ALL_EMBEDDED_QUERIES})
        string(REPLACE ":" ";" entry_parts ${entry})
        list(GET entry_parts 0 lang)
        list(GET entry_parts 1 query_type)
        file(APPEND ${QUERY_REGISTRY_SOURCE}
            "        {\"${lang}:${query_type}\", ${lang}_${query_type}_scm},\n")
    endforeach()

    file(APPEND ${QUERY_REGISTRY_SOURCE} "    };\n\n")
    file(APPEND ${QUERY_REGISTRY_SOURCE} "    auto key = lang + \":\" + query_type;\n")
    file(APPEND ${QUERY_REGISTRY_SOURCE} "    auto it = registry.find(key);\n")
    file(APPEND ${QUERY_REGISTRY_SOURCE} "    return (it != registry.end()) ? it->second : nullptr;\n")
    file(APPEND ${QUERY_REGISTRY_SOURCE} "}\n\n")

    file(APPEND ${QUERY_REGISTRY_SOURCE}
        "bool hasEmbeddedQuery(const std::string& lang, const std::string& query_type) {\n")
    file(APPEND ${QUERY_REGISTRY_SOURCE}
        "    return getEmbeddedQuery(lang, query_type) != nullptr;\n")
    file(APPEND ${QUERY_REGISTRY_SOURCE} "}\n\n")

    file(APPEND ${QUERY_REGISTRY_SOURCE} "} // namespace embedded_queries\n")

    # NOW add the generated source to the existing arc target
    target_sources(arc PRIVATE ${QUERY_REGISTRY_SOURCE})

    # Make sure the generated directory is in the include path
    target_include_directories(arc PRIVATE ${CMAKE_BINARY_DIR}/generated)

    # Calculate total size
    math(EXPR TOTAL_QUERY_KB "${TOTAL_QUERY_SIZE} / 1024")
    message(STATUS "=== Embedding languages.yaml Configuration ===")

    set(LANGUAGES_YAML_PATH "${CMAKE_SOURCE_DIR}/runtime/languages.yaml")
    set(EMBEDDED_CONFIG_HEADER "${CMAKE_BINARY_DIR}/generated/embedded_config.h")
    set(EMBEDDED_CONFIG_SOURCE "${CMAKE_BINARY_DIR}/generated/embedded_config.cpp")

    if(EXISTS ${LANGUAGES_YAML_PATH})
        # Read the languages.yaml file
        file(READ ${LANGUAGES_YAML_PATH} LANGUAGES_YAML_CONTENT)
        string(LENGTH "${LANGUAGES_YAML_CONTENT}" YAML_SIZE)

        # Create unique delimiter for raw string literal
        string(MD5 YAML_HASH "${LANGUAGES_YAML_CONTENT}")
        string(SUBSTRING ${YAML_HASH} 0 8 YAML_DELIMITER)

        # Write header file
        file(WRITE ${EMBEDDED_CONFIG_HEADER} "// Auto-generated by CMake - DO NOT EDIT\n")
        file(APPEND ${EMBEDDED_CONFIG_HEADER} "#pragma once\n\n")
        file(APPEND ${EMBEDDED_CONFIG_HEADER} "#ifdef TREE_SITTER_ENABLED\n\n")
        file(APPEND ${EMBEDDED_CONFIG_HEADER} "namespace embedded_config {\n\n")
        file(APPEND ${EMBEDDED_CONFIG_HEADER} "extern const char* languages_yaml;\n")
        file(APPEND ${EMBEDDED_CONFIG_HEADER} "bool hasEmbeddedConfig();\n")
        file(APPEND ${EMBEDDED_CONFIG_HEADER} "\n} // namespace embedded_config\n\n")
        file(APPEND ${EMBEDDED_CONFIG_HEADER} "#endif // TREE_SITTER_ENABLED\n")

        # Write source file
        file(WRITE ${EMBEDDED_CONFIG_SOURCE} "// Auto-generated by CMake - DO NOT EDIT\n")
        file(APPEND ${EMBEDDED_CONFIG_SOURCE} "#include \"embedded_config.h\"\n\n")
        file(APPEND ${EMBEDDED_CONFIG_SOURCE} "namespace embedded_config {\n\n")
        file(APPEND ${EMBEDDED_CONFIG_SOURCE}
            "const char* languages_yaml = R\"${YAML_DELIMITER}(${LANGUAGES_YAML_CONTENT})${YAML_DELIMITER}\";\n\n")
        file(APPEND ${EMBEDDED_CONFIG_SOURCE}
            "bool hasEmbeddedConfig() {\n    return true;\n}\n\n")
        file(APPEND ${EMBEDDED_CONFIG_SOURCE} "} // namespace embedded_config\n")

        # Add to target sources
        target_sources(arc PRIVATE ${EMBEDDED_CONFIG_SOURCE})

        math(EXPR YAML_KB "${YAML_SIZE} / 1024")
        message(STATUS "✅ Embedded languages.yaml (${YAML_KB} KB)")
        message(STATUS "   Header: ${EMBEDDED_CONFIG_HEADER}")
        message(STATUS "   Source: ${EMBEDDED_CONFIG_SOURCE}")
    else()
        message(WARNING "languages.yaml not found at ${LANGUAGES_YAML_PATH}")

        # Create empty fallback
        file(WRITE ${EMBEDDED_CONFIG_HEADER} "#pragma once\n")
        file(APPEND ${EMBEDDED_CONFIG_HEADER} "namespace embedded_config {\n")
        file(APPEND ${EMBEDDED_CONFIG_HEADER} "inline bool hasEmbeddedConfig() { return false; }\n")
        file(APPEND ${EMBEDDED_CONFIG_HEADER} "}\n")
    endif()

    message(STATUS "=== End Configuration Embedding ===\n")

    message(STATUS "")
    message(STATUS "✅ Generated embedded query registry:")
    message(STATUS "   Header: ${QUERY_REGISTRY_HEADER}")
    message(STATUS "   Source: ${QUERY_REGISTRY_SOURCE}")
    message(STATUS "   Total embedded queries: ${ALL_EMBEDDED_QUERIES}")
    message(STATUS "   Total size: ~${TOTAL_QUERY_KB} KB")
    message(STATUS "=== End Embedding ===")
    message(STATUS "")
endif()

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

if(ANDROID AND CURSES_LINK_LIBRARIES)
    target_link_libraries(arc PRIVATE ${CURSES_LINK_LIBRARIES})
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

    if(CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_options(arc PRIVATE -g1 -O0 -fno-omit-frame-pointer)
    else()
        target_compile_options(arc PRIVATE -O2)
    endif()
endif()

# ----------------------------------------------------
# 8. Build Summary
# ----------------------------------------------------

message(STATUS "")
message(STATUS "========================================")
message(STATUS "Arc Editor Build Configuration")
message(STATUS "========================================")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Platform: ${CMAKE_SYSTEM_NAME}")
message(STATUS "Curses library: ${CURSES_LIBRARIES}")
if(NCURSESW_FOUND OR CURSES_HAVE_NCURSESW_H OR CURSES_HAVE_NCURSESW_NCURSES_H)
    message(STATUS "Wide character support: YES (ncursesw)")
else()
    message(STATUS "Wide character support: UNKNOWN (verify with ldd)")
endif()
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
