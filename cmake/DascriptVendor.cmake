# Apply sphy-2d local patches to the daScript submodule at configure time.
# Patches live under ${CMAKE_SOURCE_DIR}/cmake/vendor/dascript/ — commit them in this
# repo only; no need to push a fork of thirdparty/dascript.
#
# After bumping the dascript submodule, if apply fails, regenerate:
#   cd thirdparty/dascript && git diff origin/master -- CMakeCommon.txt CMakeLists.txt \
#       include/daScript/das_config.h > ../../cmake/vendor/dascript/sphy-dascript.patch

function(sphy_apply_dascript_patches)
    set(_das_root "${CMAKE_SOURCE_DIR}/thirdparty/dascript")
    set(_patch "${CMAKE_SOURCE_DIR}/cmake/vendor/dascript/sphy-dascript.patch")
    if(NOT EXISTS "${_patch}")
        message(FATAL_ERROR "Missing daScript vendor patch: ${_patch}")
    endif()
    if(NOT EXISTS "${_das_root}/CMakeLists.txt")
        message(FATAL_ERROR "daScript submodule missing at ${_das_root} (git submodule update?)")
    endif()

    find_program(_git NAMES git REQUIRED)

    # Already applied?
    execute_process(
        COMMAND "${_git}" -C "${_das_root}" apply --reverse --check "${_patch}"
        RESULT_VARIABLE _rev_ok
        OUTPUT_QUIET ERROR_QUIET)
    if(_rev_ok EQUAL 0)
        message(STATUS "daScript vendor patch already applied (sphy-dascript.patch)")
        return()
    endif()

    execute_process(
        COMMAND "${_git}" -C "${_das_root}" apply --check "${_patch}"
        RESULT_VARIABLE _chk_ok
        OUTPUT_VARIABLE _chk_out
        ERROR_VARIABLE _chk_err)
    if(NOT _chk_ok EQUAL 0)
        message(
            FATAL_ERROR
            "daScript vendor patch does not apply cleanly.\n"
            "  Patch: ${_patch}\n"
            "  Submodule: ${_das_root}\n"
            "  git apply --check said:\n${_chk_out}\n${_chk_err}\n"
            "Regenerate the patch after updating the submodule (see top of DascriptVendor.cmake).")
    endif()

    execute_process(
        COMMAND "${_git}" -C "${_das_root}" apply "${_patch}"
        RESULT_VARIABLE _apply_ok
        OUTPUT_VARIABLE _apply_out
        ERROR_VARIABLE _apply_err)
    if(NOT _apply_ok EQUAL 0)
        message(FATAL_ERROR "daScript vendor patch apply failed:\n${_apply_out}\n${_apply_err}")
    endif()
    message(STATUS "Applied daScript vendor patch: sphy-dascript.patch")
endfunction()
