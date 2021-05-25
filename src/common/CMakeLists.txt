# Add a custom command to generate a new shader_cache_version hash when any of the following files change
# NOTE: This is an approximation of what files affect shader generation, its possible something else
# could affect the result, but much more unlikely than the following files. Keeping a list of files
# like this allows for much better caching since it doesn't force the user to recompile binary shaders every update
set(VIDEO_CORE "${CMAKE_SOURCE_DIR}/src/video_core")
if (DEFINED ENV{AZURECIREPO})
  set(BUILD_REPOSITORY $ENV{AZURECIREPO})
endif()
if (DEFINED ENV{TITLEBARFORMATIDLE})
  set(TITLE_BAR_FORMAT_IDLE $ENV{TITLEBARFORMATIDLE})
endif ()
if (DEFINED ENV{TITLEBARFORMATRUNNING})
  set(TITLE_BAR_FORMAT_RUNNING $ENV{TITLEBARFORMATRUNNING})
endif ()
if (DEFINED ENV{DISPLAYVERSION})
  set(DISPLAY_VERSION $ENV{DISPLAYVERSION})
endif ()

# Pass the path to git to the GenerateSCMRev.cmake as well
find_package(Git QUIET)

add_custom_command(OUTPUT scm_rev.cpp
    COMMAND ${CMAKE_COMMAND}
      -DSRC_DIR=${CMAKE_SOURCE_DIR}
      -DBUILD_REPOSITORY=${BUILD_REPOSITORY}
      -DTITLE_BAR_FORMAT_IDLE=${TITLE_BAR_FORMAT_IDLE}
      -DTITLE_BAR_FORMAT_RUNNING=${TITLE_BAR_FORMAT_RUNNING}
      -DBUILD_TAG=${BUILD_TAG}
      -DBUILD_ID=${DISPLAY_VERSION}
      -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
      -P ${CMAKE_SOURCE_DIR}/CMakeModules/GenerateSCMRev.cmake
    DEPENDS
      # WARNING! It was too much work to try and make a common location for this list,
      # so if you need to change it, please update CMakeModules/GenerateSCMRev.cmake as well
      # ...
      # and also check that the scm_rev files haven't changed
      "${CMAKE_CURRENT_SOURCE_DIR}/scm_rev.cpp.in"
      "${CMAKE_CURRENT_SOURCE_DIR}/scm_rev.h"
      # technically we should regenerate if the git version changed, but its not worth the effort imo
      "${CMAKE_SOURCE_DIR}/CMakeModules/GenerateSCMRev.cmake"
    VERBATIM
)

add_library(common STATIC
    algorithm.h
    alignment.h
    assert.cpp
    assert.h
    atomic_ops.h
    detached_tasks.cpp
    detached_tasks.h
    bit_cast.h
    bit_field.h
    bit_set.h
    bit_util.h
    cityhash.cpp
    cityhash.h
    common_funcs.h
    common_types.h
    concepts.h
    div_ceil.h
    dynamic_library.cpp
    dynamic_library.h
    fiber.cpp
    fiber.h
    fs/file.cpp
    fs/file.h
    fs/fs.cpp
    fs/fs.h
    fs/fs_paths.h
    fs/fs_types.h
    fs/fs_util.cpp
    fs/fs_util.h
    fs/path_util.cpp
    fs/path_util.h
    hash.h
    hex_util.cpp
    hex_util.h
    host_memory.cpp
    host_memory.h
    intrusive_red_black_tree.h
    literals.h
    logging/backend.cpp
    logging/backend.h
    logging/filter.cpp
    logging/filter.h
    logging/log.h
    logging/text_formatter.cpp
    logging/text_formatter.h
    logging/types.h
    lz4_compression.cpp
    lz4_compression.h
    math_util.h
    memory_detect.cpp
    memory_detect.h
    microprofile.cpp
    microprofile.h
    microprofileui.h
    misc.cpp
    nvidia_flags.cpp
    nvidia_flags.h
    page_table.cpp
    page_table.h
    param_package.cpp
    param_package.h
    parent_of_member.h
    point.h
    quaternion.h
    ring_buffer.h
    scm_rev.cpp
    scm_rev.h
    scope_exit.h
    settings.cpp
    settings.h
    settings_input.cpp
    settings_input.h
    spin_lock.cpp
    spin_lock.h
    stream.cpp
    stream.h
    string_util.cpp
    string_util.h
    swap.h
    telemetry.cpp
    telemetry.h
    thread.cpp
    thread.h
    thread_queue_list.h
    thread_worker.h
    threadsafe_queue.h
    time_zone.cpp
    time_zone.h
    tiny_mt.h
    tree.h
    uint128.h
    unique_function.h
    uuid.cpp
    uuid.h
    vector_math.h
    virtual_buffer.cpp
    virtual_buffer.h
    wall_clock.cpp
    wall_clock.h
    zstd_compression.cpp
    zstd_compression.h
)

if(ARCHITECTURE_x86_64)
    target_sources(common
        PRIVATE
            x64/cpu_detect.cpp
            x64/cpu_detect.h
            x64/native_clock.cpp
            x64/native_clock.h
            x64/xbyak_abi.h
            x64/xbyak_util.h
    )
endif()

if (MSVC)
  target_compile_definitions(common PRIVATE
    # The standard library doesn't provide any replacement for codecvt yet
    # so we can disable this deprecation warning for the time being.
    _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
  )
  target_compile_options(common PRIVATE
    /W4
    /WX
  )
else()
  target_compile_options(common PRIVATE
    -Werror

    $<$<CXX_COMPILER_ID:Clang>:-fsized-deallocation>
  )
endif()

create_target_directory_groups(common)

target_link_libraries(common PUBLIC ${Boost_LIBRARIES} fmt::fmt microprofile)
target_link_libraries(common PRIVATE lz4::lz4 xbyak)
if (MSVC)
  target_link_libraries(common PRIVATE zstd::zstd)
else()
  target_link_libraries(common PRIVATE zstd)
endif()
