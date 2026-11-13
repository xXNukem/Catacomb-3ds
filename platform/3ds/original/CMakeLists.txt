cmake_minimum_required(VERSION 3.20)
project(Catacomb3DSOriginalEngine LANGUAGES C CXX)

if(NOT NINTENDO_3DS)
  message(FATAL_ERROR "Use the devkitPro 3DS toolchain: -DCMAKE_TOOLCHAIN_FILE=$ENV{DEVKITPRO}/cmake/3DS.cmake")
endif()

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")
get_filename_component(ROOT "${ROOT}" ABSOLUTE)

# These are ReflectionHLE's actual original source lists; do not reproduce,
# rewrite, or fork the renderer, actors, game loop or physics.
include("${ROOT}/src/CMakeLists.txt")
include("${ROOT}/src/id91_11/cat_all/cat3d/CMakeLists.txt")
include("${ROOT}/src/id91_11/cat_all/catabyss/CMakeLists.txt")
list(REMOVE_ITEM be_src
  "src/actual_main.c"
  "src/backend/misc/be_misc_lpt_linux.c"
  "src/backend/misc/be_misc_lpt_win32.c")
list(APPEND be_src
  "src/backend/filesystem/be_filesystem_dir_unix.c"
  "src/backend/misc/be_misc_lpt_null.c")
list(REMOVE_DUPLICATES be_src)

# The SDL3 menu keeps the original four-game Catacomb 3DS selector.
# On launch it shuts down SDL and hands over to ReflectionHLE's normal entry.
set(ENGINE_SOURCES
  "${CMAKE_CURRENT_LIST_DIR}/refkeen_original_main.c"
  "${CMAKE_CURRENT_LIST_DIR}/cat3ds_sdl_launcher.cpp"
  "${CMAKE_CURRENT_LIST_DIR}/cat3ds_dualscreen.c"
  "${ROOT}/platform/3ds/source/cat3ds_game_files.cpp"
  "${ROOT}/platform/3ds/source/cat3ds_resource_probe.cpp"
  "${ROOT}/platform/3ds/source/cat3ds_abyss_compat.cpp")
# ReflectionHLE's upstream unified build compiles each original engine in
# its own C++ namespace. Each shared DOS file must be compiled independently
# for Cat3D and Abyss, while the backend is compiled just once.
set(CAT3DS_INCLUDED_DIRS
  "${CMAKE_CURRENT_LIST_DIR}"
  "${ROOT}/platform/3ds/include"
  "${ROOT}/platform/3ds/source"
  "${CMAKE_CURRENT_BINARY_DIR}/generated"
  "${ROOT}/src" "${ROOT}/src/crc32" "${ROOT}/src/depklite"
  "${ROOT}/src/opl" "${ROOT}/src/unlzexe"
  "${ROOT}/src/id91_11" "${ROOT}/src/id91_11/cat_all"
  "${ROOT}/src/id91_11/cat_all/cat3d"
  "${ROOT}/src/id91_11/cat_all/catabyss"
  "${ROOT}/src/id91_11/cat_all/external_restored"
  "${ROOT}/rsrc")
set(CAT3DS_COMMON_DEFS
    __3DS__=1 REFKEEN_UNIFIED_BUILD=1
    REFKEEN_HAS_VER_CAT3D=1 REFKEEN_HAS_VER_CATADVENTURES=1
    REFKEEN_HAS_VER_CATABYSS=1 REFKEEN_HAS_VER_CATACOMB_ALL=1
    REFKEEN_CONFIG_THREADS=1 REFKEEN_CONFIG_ENABLE_CMDLINE=1)

foreach(engine IN ITEMS cat3d catabyss)
  set(engine_sources "")
  foreach(rel IN LISTS ${engine}_src)
    if(NOT EXISTS "${ROOT}/${rel}")
      message(FATAL_ERROR "Missing ReflectionHLE source: ${ROOT}/${rel}")
    endif()
    list(APPEND engine_sources "${ROOT}/${rel}")
  endforeach()
  list(REMOVE_DUPLICATES engine_sources)
  set_source_files_properties(${engine_sources} PROPERTIES LANGUAGE CXX)
  add_library(cat3ds_engine_${engine} OBJECT ${engine_sources})
  target_include_directories(cat3ds_engine_${engine} PRIVATE ${CAT3DS_INCLUDED_DIRS})
  target_compile_definitions(cat3ds_engine_${engine} PRIVATE ${CAT3DS_COMMON_DEFS})
  target_compile_options(cat3ds_engine_${engine} PRIVATE -O2 -ffunction-sections -fdata-sections -fno-exceptions)
endforeach()
target_compile_definitions(cat3ds_engine_cat3d PRIVATE REFKEEN_VER_CAT3D=1 REFKEEN_VER_CATACOMB_ALL=1)
target_compile_definitions(cat3ds_engine_catabyss PRIVATE
  REFKEEN_VER_CATADVENTURES=1 REFKEEN_VER_CATABYSS=1 REFKEEN_VER_CATACOMB_ALL=1)
foreach(rel IN LISTS be_src)
  if(NOT EXISTS "${ROOT}/${rel}")
    message(FATAL_ERROR "Missing ReflectionHLE backend file: ${ROOT}/${rel}")
  endif()
  list(APPEND ENGINE_SOURCES "${ROOT}/${rel}")
  set_source_files_properties("${ROOT}/${rel}" PROPERTIES LANGUAGE CXX)
endforeach()
# All ReflectionHLE backend calls are C++-linked in unified mode.
set_source_files_properties(
  "${CMAKE_CURRENT_LIST_DIR}/refkeen_original_main.c"
  "${CMAKE_CURRENT_LIST_DIR}/cat3ds_dualscreen.c"
  PROPERTIES LANGUAGE CXX)
list(APPEND ENGINE_SOURCES
  $<TARGET_OBJECTS:cat3ds_engine_cat3d>
  $<TARGET_OBJECTS:cat3ds_engine_catabyss>)

find_package(SDL3 CONFIG REQUIRED COMPONENTS SDL3)
# Object-library sources compile independently; they need SDL3 headers too.
target_link_libraries(cat3ds_engine_cat3d PRIVATE SDL3::SDL3)
target_link_libraries(cat3ds_engine_catabyss PRIVATE SDL3::SDL3)

# ReflectionHLE's version header expects a generated Git-ID header.
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/generated/be_git_id.h"
     "/* Catacomb 3DS - original engine build */\n")

add_executable(cat3ds-original ${ENGINE_SOURCES})
if(EXISTS "$ENV{DEVKITPRO}/libctru/include/3ds.h")
  target_include_directories(cat3ds-original PRIVATE "$ENV{DEVKITPRO}/libctru/include")
endif()
target_include_directories(cat3ds-original PRIVATE ${CAT3DS_INCLUDED_DIRS})
target_compile_definitions(cat3ds-original PRIVATE ${CAT3DS_COMMON_DEFS})
target_compile_options(cat3ds-original PRIVATE -O2 -ffunction-sections -fdata-sections -fno-exceptions)
target_link_libraries(cat3ds-original PRIVATE SDL3::SDL3 m ctru)

# The 3DS toolchain provides these functions (also used by SDL3 examples).
ctr_generate_smdh(cat3ds-original.smdh
  NAME "Catacomb 3DS Original"
  DESCRIPTION "Original ReflectionHLE game engine"
  AUTHOR "xXNukem"
  ICON "${CMAKE_CURRENT_LIST_DIR}/icon.png")
ctr_create_3dsx(cat3ds-original SMDH cat3ds-original.smdh)
