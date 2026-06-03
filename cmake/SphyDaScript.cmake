# daScript integration without modifying thirdparty/dascript sources.
#
# Upstream writes static libs to thirdparty/dascript/lib/, shared across every build
# tree and CMAKE_BUILD_TYPE. That mixes Debug (DAS_FUSION=0) and Release (DAS_FUSION=2)
# object code in one archive and triggers SimNode prefix assertions at runtime.
#
# Fix: build daScript in an isolated binary dir per top-level build directory, and
# redirect each library target's output into ${CMAKE_BINARY_DIR}/lib/dascript.

include_guard(GLOBAL)

function(sphy_add_dascript_subdirectory)
    set(_das_src "${CMAKE_SOURCE_DIR}/thirdparty/dascript")
    set(_das_bin "${CMAKE_BINARY_DIR}/_subbuild/dascript")
    set(_stamp "${_das_bin}/.sphy_configured_build_type")

    if(EXISTS "${_stamp}")
        file(READ "${_stamp}" _prev)
        string(STRIP "${_prev}" _prev)
        if(NOT _prev STREQUAL "${CMAKE_BUILD_TYPE}")
            message(
                STATUS
                "CMAKE_BUILD_TYPE changed (${_prev} -> ${CMAKE_BUILD_TYPE}); "
                "wiping daScript build tree ${_das_bin}")
            file(REMOVE_RECURSE "${_das_bin}")
        endif()
    endif()

    file(MAKE_DIRECTORY "${_das_bin}")
    file(WRITE "${_stamp}" "${CMAKE_BUILD_TYPE}\n")

    add_subdirectory("${_das_src}" "${_das_bin}")

    set(_out "${CMAKE_BINARY_DIR}/lib/dascript")
    file(MAKE_DIRECTORY "${_out}")

    get_directory_property(_targets DIRECTORY "${_das_bin}" BUILDSYSTEM_TARGETS)
    foreach(_lib IN LISTS _targets)
        if(NOT TARGET "${_lib}")
            continue()
        endif()
        get_target_property(_type "${_lib}" TYPE)
        if(_type STREQUAL "STATIC_LIBRARY" OR _type STREQUAL "SHARED_LIBRARY")
            set_target_properties(
                "${_lib}"
                PROPERTIES
                    ARCHIVE_OUTPUT_DIRECTORY "${_out}"
                    LIBRARY_OUTPUT_DIRECTORY "${_out}"
                    RUNTIME_OUTPUT_DIRECTORY "${_out}")
        endif()
    endforeach()

    if(EXISTS "${_das_src}/lib/liblibDaScript.a")
        message(
            WARNING
            "Legacy daScript libraries in thirdparty/dascript/lib/ are ignored. "
            "Delete that directory after switching to per-build-tree outputs.")
    endif()
endfunction()
