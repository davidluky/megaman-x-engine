cmake_minimum_required(VERSION 3.20)

project(megaman-x-engine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest) # BUILD_TESTING defaults to ON.

set(MMX_PUBLIC_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

find_package(raylib REQUIRED)
find_package(nlohmann_json REQUIRED)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra)
endif()

# These four translation units include large generated .inc tables.  The
# tables are deliberately outside the public source snapshot, so the default
# core compiles every other source TU but does not pretend to contain the
# source-backed visual/cutscene tables.
set(MMX_GENERATED_DEPENDENT_SOURCES
    "${MMX_PUBLIC_SOURCE_ROOT}/src/gameplay/cp_capsule_cutscene.cpp"
    "${MMX_PUBLIC_SOURCE_ROOT}/src/gameplay/cp_source_obj_foreground.cpp"
    "${MMX_PUBLIC_SOURCE_ROOT}/src/gameplay/sting_chameleon_capsule_cutscene.cpp"
    "${MMX_PUBLIC_SOURCE_ROOT}/src/gameplay/storm_eagle_capsule_cutscene.cpp"
)

set(MMX_GENERATED_REQUIRED_INCLUDES
    "${MMX_PUBLIC_SOURCE_ROOT}/src/generated/cp_capsule/cp_capsule_dialog.inc"
    "${MMX_PUBLIC_SOURCE_ROOT}/src/generated/cp_capsule/cp_capsule_source_planes.inc"
    "${MMX_PUBLIC_SOURCE_ROOT}/src/generated/cp_capsule/cp_capsule_obj_composites.inc"
    "${MMX_PUBLIC_SOURCE_ROOT}/src/generated/cp_source_obj_foreground/cp_f1091_source_obj_foreground.inc"
    "${MMX_PUBLIC_SOURCE_ROOT}/src/generated/sting_chameleon_capsule/sting_chameleon_capsule_timeline.inc"
    "${MMX_PUBLIC_SOURCE_ROOT}/src/generated/sting_chameleon_capsule/sting_chameleon_capsule_hardware_visuals.inc"
    "${MMX_PUBLIC_SOURCE_ROOT}/src/generated/sting_chameleon_capsule/sting_chameleon_capsule_arena_backdrop.inc"
    "${MMX_PUBLIC_SOURCE_ROOT}/src/generated/storm_eagle_capsule/storm_eagle_capsule_timeline.inc"
    "${MMX_PUBLIC_SOURCE_ROOT}/src/generated/storm_eagle_capsule/storm_eagle_capsule_visuals.inc"
    "${MMX_PUBLIC_SOURCE_ROOT}/src/generated/storm_eagle_capsule/storm_eagle_capsule_hardware_visuals.inc"
)

file(GLOB_RECURSE MMX_ALL_SOURCE_TUS CONFIGURE_DEPENDS
    "${MMX_PUBLIC_SOURCE_ROOT}/src/*.cpp")
set(MMX_CORE_SOURCE_TUS ${MMX_ALL_SOURCE_TUS})
list(REMOVE_ITEM MMX_CORE_SOURCE_TUS
    "${MMX_PUBLIC_SOURCE_ROOT}/src/main.cpp")
list(REMOVE_ITEM MMX_CORE_SOURCE_TUS ${MMX_GENERATED_DEPENDENT_SOURCES})

add_library(mmx-engine-core STATIC ${MMX_CORE_SOURCE_TUS})
target_include_directories(mmx-engine-core PUBLIC
    "${MMX_PUBLIC_SOURCE_ROOT}/src")
target_link_libraries(mmx-engine-core PUBLIC
    raylib
    nlohmann_json::nlohmann_json)

set_target_properties(mmx-engine-core PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF)

option(MMX_PUBLIC_BUILD_ENGINE
    "Build the gameplay executable; requires private generated .inc tables"
    OFF)

if(MMX_PUBLIC_BUILD_ENGINE)
    set(MMX_MISSING_GENERATED_INCLUDES)
    foreach(required_include IN LISTS MMX_GENERATED_REQUIRED_INCLUDES)
        if(NOT EXISTS "${required_include}")
            list(APPEND MMX_MISSING_GENERATED_INCLUDES "${required_include}")
        endif()
    endforeach()

    if(MMX_MISSING_GENERATED_INCLUDES)
        string(REPLACE ";" "\n  " MMX_MISSING_GENERATED_TEXT
            "${MMX_MISSING_GENERATED_INCLUDES}")
        message(FATAL_ERROR
            "MMX_PUBLIC_BUILD_ENGINE=ON requires generated visual/cutscene "
            "tables that are not part of the public snapshot:\n  "
            "${MMX_MISSING_GENERATED_TEXT}")
    endif()

    # Only the previously omitted TUs are added here; the core is shared with
    # contract tests and already contains every other src/*.cpp TU.
    target_sources(mmx-engine-core PRIVATE ${MMX_GENERATED_DEPENDENT_SOURCES})
    add_executable(megaman-x
        "${MMX_PUBLIC_SOURCE_ROOT}/src/main.cpp")
    target_link_libraries(megaman-x PRIVATE mmx-engine-core)
    target_include_directories(megaman-x PRIVATE
        "${MMX_PUBLIC_SOURCE_ROOT}/src")
    set_target_properties(megaman-x PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF)
    message(STATUS
        "Full executable enabled; runtime still requires separately supplied "
        "content/ and knowledge_base/.")
else()
    message(STATUS
        "Full executable disabled: generated .inc tables are excluded from "
        "the public snapshot. Building mmx-engine-core and public contracts.")
endif()

if(BUILD_TESTING)
    include("${MMX_PUBLIC_SOURCE_ROOT}/cmake/PublicTests.cmake")
endif()
