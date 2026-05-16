# Compile .sc shaders with bgfx shaderc for the host renderer (Metal on macOS, SPIR-V on Linux, etc.).

function(sphy_add_bgfx_shaders target_name)
    set(options "")
    set(oneValueArgs BGFX_DIR BGFX_BUILD_DIR OUTPUT_DIR)
    set(multiValueArgs SOURCES)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT ARG_BGFX_DIR OR NOT ARG_BGFX_BUILD_DIR OR NOT ARG_OUTPUT_DIR)
        message(FATAL_ERROR "sphy_add_bgfx_shaders: BGFX_DIR, BGFX_BUILD_DIR, and OUTPUT_DIR are required")
    endif()

    set(_shaderc "${ARG_BGFX_BUILD_DIR}/bgfx/shaderc")
    if(NOT EXISTS "${_shaderc}")
        message(FATAL_ERROR "bgfx shaderc not found at ${_shaderc} — build thirdparty/bgfx first (scripts/init-sphy-2d.sh)")
    endif()

    if(APPLE)
        set(_platform osx)
        set(_profile metal)
    elseif(WIN32)
        set(_platform windows)
        set(_profile spirv)
    else()
        set(_platform linux)
        set(_profile spirv)
    endif()

    file(MAKE_DIRECTORY "${ARG_OUTPUT_DIR}")
    set(_outputs "")
    foreach(_shader ${ARG_SOURCES})
        get_filename_component(_name "${_shader}" NAME_WE)
        set(_out "${ARG_OUTPUT_DIR}/${_name}.bin")
        string(SUBSTRING "${_name}" 0 1 _type)
        add_custom_command(
            OUTPUT "${_out}"
            COMMAND "${_shaderc}"
                -f "${_shader}"
                -o "${_out}"
                --platform "${_platform}"
                --type "${_type}"
                --profile "${_profile}"
                -i "${ARG_BGFX_DIR}/bgfx/src"
            DEPENDS "${_shader}"
            COMMENT "Compiling shader ${_name} (${_platform}/${_profile})"
            VERBATIM)
        list(APPEND _outputs "${_out}")
    endforeach()

    add_custom_target(${target_name} DEPENDS ${_outputs})
    message(STATUS "bgfx shaders: platform=${_platform} profile=${_profile} -> ${ARG_OUTPUT_DIR}")
endfunction()
